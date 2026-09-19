// Copyright 2025 Cognitas Trading Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COMMON_SHARED_MEMORY_BROADCAST_RING_BUFFER_H_
#define COMMON_SHARED_MEMORY_BROADCAST_RING_BUFFER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/shared_memory/memory_map.h"

namespace cognitas::trading {

constexpr size_t kMaxReaders = 16;
constexpr size_t kRingBufferCacheLineSize = 64;

/**
 * @brief Represents a slot for a consumer to register its progress.
 * Aligned to prevent false sharing between concurrent consumers.
 */
struct alignas(kRingBufferCacheLineSize) ReaderSlot {
  std::atomic<bool> active{false};
  std::atomic<uint64_t> cursor{0};
  std::atomic<int64_t> heartbeat_ts{0};
  std::atomic<pid_t> pid{0};
  std::atomic<uint32_t> generation_id{0};
  char padding[35];  // Explicit padding clarity
};

/**
 * @brief Header structure for the Broadcast Ring Buffer.
 *
 * This structure resides in a dedicated metadata shared memory segment.
 * It contains atomic cursors and a consumer roster for backpressure.
 * Aligned to 64 bytes to prevent false sharing.
 */
struct alignas(kRingBufferCacheLineSize) BroadcastHeader {
  /**
   * @brief The cursor where the producer is currently reserving space.
   * Consumers should not read past this point.
   */
  alignas(kRingBufferCacheLineSize) std::atomic<uint64_t> producer_cursor{0};

  /**
   * @brief The cursor up to which data has been fully written and committed.
   * This is the safe read limit for consumers.
   */
  alignas(kRingBufferCacheLineSize) std::atomic<uint64_t> commit_cursor{0};

  /**
   * @brief The total capacity of the data buffer in bytes.
   */
  alignas(kRingBufferCacheLineSize) uint64_t buffer_capacity{0};

  /**
   * @brief Consumer state roster for SPMC backpressure.
   */
  alignas(kRingBufferCacheLineSize) ReaderSlot readers[kMaxReaders];
};

struct FrameHeader;

/**
 * @brief Helper class for consumers to track their read position.
 *
 * Each consumer maintains its own local cursor to track what it has already
 * processed.
 */
class BroadcastReader {
 public:
  /**
   * @brief Constructs a new BroadcastReader.
   * @param start_cursor The initial cursor position (default 0).
   */
  explicit BroadcastReader(uint64_t start_cursor = 0)
      : cursor_(start_cursor), slot_index_(-1), generation_id_(0) {}

  /**
   * @brief Gets the current read cursor position.
   * @return The cursor value.
   */
  uint64_t cursor() const { return cursor_; }

  /**
   * @brief Gets the reader's generation ID.
   * @return The generation ID.
   */
  uint32_t generation_id() const { return generation_id_; }

  /**
   * @brief Sets the reader's generation ID.
   * @param generation_id The generation ID.
   */
  void set_generation_id(uint32_t generation_id) {
    generation_id_ = generation_id;
  }

  /**
   * @brief Advances the read cursor by a specified number of raw bytes.
   *
   * WARNING: This is a low-level API. If you are reading frames via the `Poll`
   * or `PollBatch` API, you should strongly prefer `Advance(const
   * FrameHeader&)` which automatically handles 8-byte payload alignment
   * requirements. Manually calculating frame sizes using `length` can lead to
   * unaligned reads and memory corruption if the payload size is not a multiple
   * of 8.
   *
   * @param bytes The number of bytes to advance.
   */
  void advance(uint64_t bytes) { cursor_ += bytes; }

  /**
   * @brief Advances the read cursor past the provided frame.
   *
   * This is the recommended and safe way to advance the reader cursor after
   * processing a frame obtained from `Poll()` or `PollBatch()`. It
   * automatically calculates the correct 8-byte padded alignment required by
   * the Ring Buffer.
   *
   * @param header The frame header to advance past.
   */
  void Advance(const FrameHeader& header);

  /**
   * @brief Sets the read cursor to a specific position.
   * @param pos The new cursor position.
   */
  void set_cursor(uint64_t pos) { cursor_ = pos; }

  /**
   * @brief Gets the reader's registered slot index.
   * @return The slot index, or -1 if not registered.
   */
  int slot_index() const { return slot_index_; }

  /**
   * @brief Sets the reader's registered slot index.
   * @param slot The slot index.
   */
  void set_slot_index(int slot) { slot_index_ = slot; }

 private:
  uint64_t cursor_;
  int slot_index_;
  uint32_t generation_id_;
};

/**
 * @brief Header for each data frame within the ring buffer.
 */
struct FrameHeader {
  /**
   * @brief Sequence lock version for preventing TOCTOU vulnerabilities.
   */
  std::atomic<uint32_t> version;

  /**
   * @brief The length of the payload in bytes.
   */
  uint32_t length;

  /**
   * @brief The type identifier of the payload message.
   */
  uint32_t type;

  /**
   * @brief The timestamp when the frame was written (Unix nanos).
   */
  int64_t timestamp;

  /**
   * @brief Calculates the payload size padded to 8-byte alignment.
   * @return The aligned payload size in bytes.
   */
  uint32_t AlignedPayloadSize() const { return (length + 7) & ~7; }

  /**
   * @brief Calculates the total size of the frame (header + aligned payload).
   * @return The total frame size in bytes.
   */
  uint32_t TotalFrameSize() const {
    return sizeof(FrameHeader) + AlignedPayloadSize();
  }
};

inline void BroadcastReader::Advance(const FrameHeader& header) {
  cursor_ += header.TotalFrameSize();
}

/**
 * @brief A safe, self-contained message copy from the ring buffer.
 */
struct SafeMessage {
  /**
   * @brief The message type identifier.
   */
  uint32_t type;

  /**
   * @brief The timestamp when the frame was written (Unix nanos).
   */
  int64_t timestamp;

  /**
   * @brief A copy of the message payload.
   */
  std::vector<uint8_t> payload;
};

/**
 * @brief A lock-free, single-producer, multi-consumer broadcast ring buffer.
 *
 * This implementation uses two shared memory segments:
 * 1. Metadata Segment ("<name>_meta"): Stores the BroadcastHeader.
 * 2. Data Segment ("<name>_data"): Stores the actual messages.
 *
 * The Data Segment uses Virtual Memory Mirroring ("Magic Ring Buffer") to
 * provide a contiguous virtual address space, eliminating the need for padding
 * and simplifying wrap-around logic.
 *
 * It uses a SeqLock mechanism to ensure data integrity for consumers without
 * blocking the producer.
 */
class BroadcastRingBuffer {
 public:
  /**
   * @brief Creates a new Ring Buffer (Producer Mode).
   *
   * Creates the underlying shared memory mappings and initializes the header.
   *
   * @param name The base name of the shared memory object.
   * @param capacity_bytes The size of the data buffer. Must be page-aligned.
   * @param evict_timeout_ns The timeout in nanoseconds for evicting slow
   * readers (default 500ms).
   * @return A StatusOr containing a unique_ptr to the created
   * BroadcastRingBuffer.
   */
  static absl::StatusOr<std::unique_ptr<BroadcastRingBuffer>> Create(
      const std::string& name, size_t capacity_bytes,
      int64_t evict_timeout_ns = 500'000'000);

  /**
   * @brief Attaches to an existing Ring Buffer.
   *
   * By default, attaches in Consumer Mode (read-only).
   * Set writable=true to attach as a Producer (for IPC/recovery).
   *
   * @param name The base name of the shared memory object.
   * @param writable If true, maps with write permissions.
   * @return A StatusOr containing a unique_ptr to the attached
   * BroadcastRingBuffer.
   */
  static absl::StatusOr<std::unique_ptr<BroadcastRingBuffer>> Attach(
      const std::string& name, bool writable = false);

  /**
   * @brief Unlinks the shared memory objects associated with the given name.
   *
   * @param name The base name of the shared memory object.
   * @return absl::Status Ok on success, or an error status.
   */
  static absl::Status Unlink(const std::string& name);

  ~BroadcastRingBuffer() = default;

  // --- PRODUCER API ---

  /**
   * @brief Reserves space in the ring buffer for a new message.
   *
   * This updates the sequence counter and the producer cursor.
   * The returned pointer is where the producer should write the payload.
   *
   * @param payload_size The size of the message payload in bytes.
   * @param type_id The message type ID.
   * @return A StatusOr containing a pointer to the reserved memory buffer.
   */
  absl::StatusOr<uint8_t*> Reserve(uint32_t payload_size, uint32_t type_id);

  /**
   * @brief Commits the previously reserved message.
   *
   * Updates the sequence counter and the commit cursor, making the message
   * visible and stable for consumers.
   */
  void Commit();

  // --- CONSUMER API ---

  /**
   * @brief Registers a new reader in the next available slot.
   * @param reader The reader state to register.
   * @return absl::Status Ok on success, or ResourceExhausted if max readers
   * reached.
   */
  absl::Status RegisterReader(BroadcastReader& reader);

  /**
   * @brief Unregisters a reader, freeing its slot.
   * @param reader The reader state to unregister.
   */
  void UnregisterReader(BroadcastReader& reader);

  /**
   * @brief Polls for the next available message (Zero-Copy).
   *
   * @param reader The consumer's reader state.
   * @param header_out Reference to store the parsed FrameHeader.
   * @return A StatusOr containing a span pointing directly to the payload in
   * shared memory.
   */
  absl::StatusOr<absl::Span<const uint8_t>> PollZeroCopy(
      BroadcastReader& reader, FrameHeader& header_out);

  /**
   * @brief Polls for the next available message and safely copies it.
   *
   * This uses the Sequence Lock mechanism to ensure the data is not
   * overwritten by the producer while it is being copied.
   *
   * @param reader The consumer's reader state.
   * @param header_out Reference to store the parsed FrameHeader.
   * @return A StatusOr containing a SafeMessage with the copied payload.
   */
  absl::Status PollCopy(BroadcastReader& reader, FrameHeader& header_out,
                        SafeMessage& msg_out);

  /**
   * @brief Polls for a batch of messages and copies them into a pre-allocated
   * vector.
   *
   * This reuses the capacity of the `messages_out` vector to avoid allocations.
   * Automatically advances the reader's cursor by the total size of the
   * messages read.
   *
   * @param reader The consumer's reader state.
   * @param max_messages The maximum number of messages to read.
   * @param messages_out Vector to populate with messages.
   * @return A StatusOr containing the number of messages successfully read.
   */
  absl::StatusOr<size_t> PollCopyBatch(BroadcastReader& reader,
                                       size_t max_messages,
                                       std::vector<SafeMessage>& messages_out);

  /**
   * @brief Acknowledges reading of a frame and updates the reader's cursor in
   * shared memory.
   *
   * This MUST be called after processing the payload returned by PollZeroCopy
   * to relieve backpressure on the producer.
   *
   * @param reader The consumer's reader state.
   * @param total_frame_length The total size of the frame (header + aligned
   * payload).
   */
  void Advance(BroadcastReader& reader, size_t total_frame_length);

  /**
   * @brief Gets the latest commit cursor from the shared header.
   * @return The current commit cursor value.
   */
  uint64_t GetLatestCursor() const;

  /**
   * @brief Gets the total capacity of the ring buffer.
   * @return The capacity in bytes.
   */
  uint64_t capacity() const { return capacity_; }

 private:
  BroadcastRingBuffer(MemoryMap metadata_map, MemoryMap data_map,
                      bool is_producer, int64_t evict_timeout_ns);

  MemoryMap metadata_map_;
  MemoryMap data_map_;
  BroadcastHeader* header_;
  uint8_t* data_buffer_;
  uint64_t capacity_;
  bool is_producer_;
  int64_t evict_timeout_ns_;

  struct Reservation {
    uint64_t claimed_cursor;
    uint64_t start_offset;
    uint32_t total_frame_size;
  };
  Reservation current_reservation_;
  uint64_t cached_min_cursor_;
};

}  // namespace cognitas::trading

#endif  // COMMON_SHARED_MEMORY_BROADCAST_RING_BUFFER_H_

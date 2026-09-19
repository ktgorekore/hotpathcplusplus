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

#include "common/shared_memory/broadcast_ring_buffer.h"

#include <immintrin.h>  // For _mm_pause()
#include <unistd.h>     // For getpid()

#include <cstring>
#include <new>
#include <thread>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"

namespace cognitas::trading {

namespace {
constexpr size_t kHeaderSize = sizeof(BroadcastHeader);
constexpr size_t kFrameHeaderSize = sizeof(FrameHeader);
}  // namespace

absl::StatusOr<std::unique_ptr<BroadcastRingBuffer>>
BroadcastRingBuffer::Create(const std::string& name, size_t capacity_bytes,
                            int64_t evict_timeout_ns) {
  // Create Metadata segment
  auto meta_or = MemoryMap::Create(absl::StrCat(name, "_meta"), kHeaderSize);
  if (!meta_or.ok()) return meta_or.status();

  // Create Data segment with Magic Mirror
  auto data_or = MemoryMap::Create(absl::StrCat(name, "_data"), capacity_bytes,
                                   /*use_magic_mirror=*/true);
  if (!data_or.ok()) {
    MemoryMap::Unlink(absl::StrCat(name, "_meta")).IgnoreError();
    return data_or.status();
  }

  auto* ptr = new BroadcastRingBuffer(std::move(*meta_or), std::move(*data_or),
                                      /*is_producer=*/true, evict_timeout_ns);

  // Initialize Header
  new (ptr->header_) BroadcastHeader();
  ptr->header_->buffer_capacity = capacity_bytes;
  ptr->header_->producer_cursor.store(0, std::memory_order_relaxed);
  ptr->header_->commit_cursor.store(0, std::memory_order_relaxed);

  for (size_t i = 0; i < kMaxReaders; ++i) {
    ptr->header_->readers[i].active.store(false, std::memory_order_relaxed);
    ptr->header_->readers[i].cursor.store(0, std::memory_order_relaxed);
    ptr->header_->readers[i].heartbeat_ts.store(0, std::memory_order_relaxed);
    ptr->header_->readers[i].pid.store(0, std::memory_order_relaxed);
    ptr->header_->readers[i].generation_id.store(0, std::memory_order_relaxed);
  }

  return std::unique_ptr<BroadcastRingBuffer>(ptr);
}

absl::StatusOr<std::unique_ptr<BroadcastRingBuffer>>
BroadcastRingBuffer::Attach(const std::string& name, bool writable) {
  // Attach Metadata
  // Metadata MUST always be writable for SPMC so consumers can register and
  // update their cursors.
  auto meta_or = MemoryMap::Open(absl::StrCat(name, "_meta"), kHeaderSize,
                                 /*read_only=*/false);
  if (!meta_or.ok()) return meta_or.status();

  auto* temp_header = static_cast<BroadcastHeader*>(meta_or->addr());
  size_t capacity = temp_header->buffer_capacity;

  // Sanity check to prevent allocating ridiculous amounts of RAM if the header
  // is corrupted
  if (capacity > 1024ULL * 1024ULL * 1024ULL) {  // Max 1GB ring buffer
    return absl::DataLossError(absl::StrCat(
        "Corrupted ring buffer capacity in ", name, ": ", capacity, " bytes"));
  }

  // Attach Data with Magic Mirror
  auto data_or = MemoryMap::Open(absl::StrCat(name, "_data"), capacity,
                                 !writable, /*use_magic_mirror=*/true);
  if (!data_or.ok()) return data_or.status();

  return std::unique_ptr<BroadcastRingBuffer>(
      new BroadcastRingBuffer(std::move(*meta_or), std::move(*data_or),
                              writable, /*evict_timeout_ns=*/0));
}

absl::Status BroadcastRingBuffer::Unlink(const std::string& name) {
  auto s1 = MemoryMap::Unlink(absl::StrCat(name, "_meta"));
  auto s2 = MemoryMap::Unlink(absl::StrCat(name, "_data"));
  if (!s1.ok()) return s1;
  return s2;
}

BroadcastRingBuffer::BroadcastRingBuffer(MemoryMap metadata_map,
                                         MemoryMap data_map, bool is_producer,
                                         int64_t evict_timeout_ns)
    : metadata_map_(std::move(metadata_map)),
      data_map_(std::move(data_map)),
      is_producer_(is_producer),
      evict_timeout_ns_(evict_timeout_ns),
      cached_min_cursor_(0) {
  header_ = static_cast<BroadcastHeader*>(metadata_map_.addr());
  data_buffer_ = static_cast<uint8_t*>(data_map_.addr());
  capacity_ = data_map_.size();
}

absl::StatusOr<uint8_t*> BroadcastRingBuffer::Reserve(uint32_t payload_size,
                                                      uint32_t type_id) {
  if (!is_producer_) {
    return absl::PermissionDeniedError("Cannot write to read-only buffer");
  }

  uint32_t aligned_payload_size = (payload_size + 7) & ~7;
  uint64_t required_space = kFrameHeaderSize + aligned_payload_size;

  if (required_space > capacity_) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Payload size %u exceeds buffer capacity %lu",
                        payload_size, capacity_));
  }

  uint64_t current_cursor =
      header_->producer_cursor.load(std::memory_order_relaxed);
  uint64_t target_cursor = current_cursor + required_space;

  // SPMC Backpressure Check
  if (target_cursor - cached_min_cursor_ > capacity_) {
    int64_t now = absl::ToUnixNanos(absl::Now());
    uint64_t last_tsc = __rdtsc();

    while (true) {
      uint64_t min_c = target_cursor;

      // Rate-limit the syscall to approximately every 100,000 CPU cycles
      uint64_t current_tsc = __rdtsc();
      if (current_tsc - last_tsc > 100000ULL) {
        now = absl::ToUnixNanos(absl::Now());
        last_tsc = current_tsc;
      }

      for (size_t i = 0; i < kMaxReaders; ++i) {
        if (header_->readers[i].active.load(std::memory_order_acquire)) {
          // SAFE RECLAMATION: Evict dead/hung nodes to prevent global deadlock
          if (now - header_->readers[i].heartbeat_ts.load(
                        std::memory_order_relaxed) >
              evict_timeout_ns_) {
            header_->readers[i].active.store(false, std::memory_order_release);
            continue;
          }
          uint64_t rc =
              header_->readers[i].cursor.load(std::memory_order_acquire);
          if (rc < min_c) min_c = rc;
        }
      }

      cached_min_cursor_ = min_c;
      if (target_cursor - cached_min_cursor_ <= capacity_) break;

      _mm_pause();  // Yield to readers
    }
  }

  uint64_t offset = current_cursor % capacity_;

  current_reservation_.claimed_cursor = target_cursor;
  current_reservation_.start_offset = offset;
  current_reservation_.total_frame_size = static_cast<uint32_t>(required_space);

  auto* header_dest = reinterpret_cast<FrameHeader*>(data_buffer_ + offset);
  uint32_t current_version =
      header_dest->version.load(std::memory_order_relaxed);
  header_dest->version.store(current_version | 1, std::memory_order_release);

  header_dest->length = payload_size;
  header_dest->type = type_id;
  header_dest->timestamp = absl::ToUnixNanos(absl::Now());

  header_->producer_cursor.store(target_cursor, std::memory_order_relaxed);

  return data_buffer_ + offset + kFrameHeaderSize;
}

void BroadcastRingBuffer::Commit() {
  auto* header_dest = reinterpret_cast<FrameHeader*>(
      data_buffer_ + current_reservation_.start_offset);
  uint32_t current_version =
      header_dest->version.load(std::memory_order_relaxed);
  header_dest->version.store(current_version + 1, std::memory_order_release);

  std::atomic_thread_fence(std::memory_order_release);

  header_->commit_cursor.store(current_reservation_.claimed_cursor,
                               std::memory_order_release);
}

absl::Status BroadcastRingBuffer::RegisterReader(BroadcastReader& reader) {
  if (reader.slot_index() != -1) {
    return absl::AlreadyExistsError("Reader is already registered");
  }

  // Find an empty slot
  for (size_t i = 0; i < kMaxReaders; ++i) {
    bool expected = false;
    if (header_->readers[i].active.compare_exchange_strong(
            expected, true, std::memory_order_acquire)) {
      // Slot claimed
      uint32_t gen_id = header_->readers[i].generation_id.fetch_add(
                            1, std::memory_order_relaxed) +
                        1;
      header_->readers[i].pid.store(getpid(), std::memory_order_relaxed);
      header_->readers[i].cursor.store(reader.cursor(),
                                       std::memory_order_release);
      header_->readers[i].heartbeat_ts.store(absl::ToUnixNanos(absl::Now()),
                                             std::memory_order_relaxed);
      reader.set_slot_index(static_cast<int>(i));
      reader.set_generation_id(gen_id);
      return absl::OkStatus();
    }
  }

  return absl::ResourceExhaustedError("Maximum number of readers reached");
}

void BroadcastRingBuffer::UnregisterReader(BroadcastReader& reader) {
  int slot = reader.slot_index();
  if (slot >= 0 && slot < static_cast<int>(kMaxReaders)) {
    header_->readers[slot].active.store(false, std::memory_order_release);
    reader.set_slot_index(-1);
  }
}

absl::StatusOr<absl::Span<const uint8_t>> BroadcastRingBuffer::PollZeroCopy(
    BroadcastReader& reader, FrameHeader& header_out) {
  uint64_t current = reader.cursor();
  uint64_t commit = header_->commit_cursor.load(std::memory_order_acquire);

  if (current >= commit) return absl::NotFoundError("No data");

  // Protection check: Did we freeze for 5 minutes and get evicted?
  // MUST check producer_cursor because the producer claims memory before
  // commit! If producer_cursor - current > capacity_, the producer is actively
  // overwriting this slot!
  uint64_t prod = header_->producer_cursor.load(std::memory_order_acquire);
  if (prod - current > capacity_) {
    return absl::ResourceExhaustedError("Consumer was evicted and lapped");
  }

  uint64_t offset = current % capacity_;
  const auto* frame =
      reinterpret_cast<const FrameHeader*>(data_buffer_ + offset);

  if (frame->length > capacity_) {
    return absl::DataLossError("Corrupt frame length detected");
  }

  header_out.type = frame->type;
  header_out.timestamp = frame->timestamp;
  header_out.length = frame->length;

  // Return Zero-Copy view. Magic mirror makes the contiguous layout safe!
  return absl::MakeSpan(data_buffer_ + offset + kFrameHeaderSize,
                        frame->length);
}

absl::Status BroadcastRingBuffer::PollCopy(BroadcastReader& reader,
                                           FrameHeader& header_out,
                                           SafeMessage& msg_out) {
  uint64_t current = reader.cursor();
  uint64_t commit = header_->commit_cursor.load(std::memory_order_acquire);

  if (current >= commit) return absl::NotFoundError("No data");

  uint64_t offset = current % capacity_;
  const auto* frame =
      reinterpret_cast<const FrameHeader*>(data_buffer_ + offset);
  int retry_count = 0;
  constexpr int kMaxRetries = 1000;

  while (true) {
    uint64_t prod = header_->producer_cursor.load(std::memory_order_acquire);
    if (prod - current > capacity_) {
      return absl::ResourceExhaustedError("Consumer was evicted and lapped");
    }

    uint32_t version_before = frame->version.load(std::memory_order_acquire);
    if ((version_before & 1) != 0) {
      // Producer is actively writing this frame.
      // We shouldn't normally see this if current < commit, unless the producer
      // just lapped us and is rewriting the very slot we are trying to read.
      _mm_pause();
      retry_count++;
      if (retry_count > kMaxRetries) {
        // If we spin too long, check if we got lapped.
        continue;
      }
      continue;
    }

    uint32_t length = frame->length;
    if (length > capacity_) {
      return absl::DataLossError("Corrupt frame length detected");
    }

    header_out.type = frame->type;
    header_out.timestamp = frame->timestamp;
    header_out.length = length;
    msg_out.type = frame->type;
    msg_out.timestamp = frame->timestamp;

    // Safely copy the payload memory
    const uint8_t* payload_src = data_buffer_ + offset + kFrameHeaderSize;
    msg_out.payload.assign(payload_src, payload_src + length);

    // Memory fence to ensure reads complete before checking version
    std::atomic_thread_fence(std::memory_order_acquire);

    uint32_t version_after = frame->version.load(std::memory_order_acquire);
    if (version_before == version_after) {
      // Data was successfully copied without being concurrently modified
      break;
    }

    // Data changed during copy. Loop will re-evaluate eviction check.
    retry_count++;
  }

  return absl::OkStatus();
}

absl::StatusOr<size_t> BroadcastRingBuffer::PollCopyBatch(
    BroadcastReader& reader, size_t max_messages,
    std::vector<SafeMessage>& messages_out) {
  uint64_t current = reader.cursor();
  uint64_t commit = header_->commit_cursor.load(std::memory_order_acquire);

  if (current >= commit) return absl::NotFoundError("No data");

  size_t messages_read = 0;
  uint64_t total_bytes_advanced = 0;
  constexpr int kMaxRetries = 1000;

  while (messages_read < max_messages && current < commit) {
    uint64_t offset = current % capacity_;
    const auto* frame =
        reinterpret_cast<const FrameHeader*>(data_buffer_ + offset);

    int retry_count = 0;
    bool success = false;
    uint32_t length = 0;
    uint32_t total_frame_size = 0;

    while (!success) {
      uint64_t prod = header_->producer_cursor.load(std::memory_order_acquire);
      if (prod - current > capacity_) {
        // If we're midway through a batch and get evicted, returning what we
        // have is safer. We'll let the next call fail with ResourceExhausted if
        // we return early here.
        if (messages_read > 0) goto batch_done;
        return absl::ResourceExhaustedError("Consumer was evicted and lapped");
      }

      uint32_t version_before = frame->version.load(std::memory_order_acquire);
      if ((version_before & 1) != 0) {
        _mm_pause();
        retry_count++;
        if (retry_count > kMaxRetries) {
          break;  // break retry loop, will exit batch loop below
        }
        continue;
      }

      length = frame->length;
      if (length > capacity_) {
        if (messages_read > 0) goto batch_done;
        return absl::DataLossError("Corrupt frame length detected");
      }

      if (messages_out.size() <= messages_read) {
        messages_out.emplace_back();
      }
      auto& msg = messages_out[messages_read];

      msg.type = frame->type;
      msg.timestamp = frame->timestamp;

      const uint8_t* payload_src = data_buffer_ + offset + kFrameHeaderSize;
      msg.payload.assign(payload_src, payload_src + length);

      std::atomic_thread_fence(std::memory_order_acquire);

      uint32_t version_after = frame->version.load(std::memory_order_acquire);
      if (version_before == version_after) {
        success = true;
        total_frame_size = frame->TotalFrameSize();
      } else {
        retry_count++;
      }
    }

    if (!success) {
      break;
    }

    current += total_frame_size;
    total_bytes_advanced += total_frame_size;
    messages_read++;
  }

batch_done:
  if (messages_read > 0) {
    Advance(reader, total_bytes_advanced);
    return messages_read;
  }

  // We didn't manage to read any messages, likely because we were interrupted
  // constantly or `current >= commit` was true on the first try (already
  // checked above though).
  return absl::NotFoundError("No data");
}

void BroadcastRingBuffer::Advance(BroadcastReader& reader,
                                  size_t total_frame_length) {
  uint64_t new_cursor = reader.cursor() + total_frame_length;
  reader.set_cursor(new_cursor);

  int slot = reader.slot_index();
  if (slot >= 0) {
    auto& my_slot = header_->readers[slot];
    uint32_t expected_gen = reader.generation_id();

    // Verify the reader's local epoch matches the slot's epoch via CAS
    // before updating cursors, to prevent the Eviction ABA race condition.
    if (my_slot.generation_id.compare_exchange_strong(
            expected_gen, expected_gen, std::memory_order_acquire)) {
      my_slot.cursor.store(new_cursor, std::memory_order_release);
      my_slot.heartbeat_ts.store(absl::ToUnixNanos(absl::Now()),
                                 std::memory_order_relaxed);
    } else {
      // The reader was evicted and potentially replaced by a new reader.
      // Unbind it to prevent further corruption attempts.
      reader.set_slot_index(-1);
    }
  }
}

uint64_t BroadcastRingBuffer::GetLatestCursor() const {
  return header_->commit_cursor.load(std::memory_order_acquire);
}

}  // namespace cognitas::trading
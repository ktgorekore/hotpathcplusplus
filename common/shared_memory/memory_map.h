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

#ifndef COMMON_SHARED_MEMORY_MEMORY_MAP_H_
#define COMMON_SHARED_MEMORY_MEMORY_MAP_H_

#include <cstddef>
#include <string>

#include "absl/status/statusor.h"

namespace cognitas::trading {

/**
 * @brief A wrapper around POSIX shared memory (shm_open, mmap).
 *
 * This class manages the lifetime of the mapping within the process.
 * It does NOT automatically unlink the shared memory object on destruction,
 * ensuring persistence across process restarts/crashes.
 */
class MemoryMap {
 public:
  /**
   * @brief Creates a NEW shared memory segment.
   *
   * Fails if the segment already exists (uses O_CREAT | O_EXCL).
   *
   * @param name The name of the shared memory object (e.g., "/my_shm").
   * @param size The size of the shared memory segment in bytes.
   * @param use_magic_mirror If true, maps the segment twice in adjacent virtual
   * address ranges to allow transparent wrap-around for ring buffers.
   * @return A StatusOr containing the MemoryMap instance on success, or an
   * error status.
   */
  static absl::StatusOr<MemoryMap> Create(const std::string& name, size_t size,
                                          bool use_magic_mirror = false);

  /**
   * @brief Opens an EXISTING shared memory segment.
   *
   * Fails if the segment does not exist.
   *
   * @param name The name of the shared memory object.
   * @param size The size of the shared memory segment in bytes.
   * @param readonly If true, maps the memory as read-only.
   * @param use_magic_mirror If true, maps the segment twice.
   * @return A StatusOr containing the MemoryMap instance on success, or an
   * error status.
   */
  static absl::StatusOr<MemoryMap> Open(const std::string& name, size_t size,
                                        bool readonly = false,
                                        bool use_magic_mirror = false);

  /**
   * @brief Opens an existing shared memory segment or creates it if it doesn't
   * exist.
   *
   * @param name The name of the shared memory object.
   * @param size The size of the shared memory segment in bytes.
   * @param use_magic_mirror If true, maps the segment twice.
   * @return A StatusOr containing the MemoryMap instance on success, or an
   * error status.
   */
  static absl::StatusOr<MemoryMap> CreateOrOpen(const std::string& name,
                                                size_t size,
                                                bool use_magic_mirror = false);

  /**
   * @brief Unlinks the shared memory object from the filesystem namespace.
   *
   * Existing mappings remain valid, but no new opens can occur.
   * Should be called by the "owner" (Gateway) when cleaning up.
   *
   * @param name The name of the shared memory object to unlink.
   * @return absl::Status Ok on success, or an error status.
   */
  static absl::Status Unlink(const std::string& name);

  /**
   * @brief Destructor.
   *
   * Unmaps the shared memory from the process address space.
   * Does NOT unlink the shared memory object.
   */
  ~MemoryMap();

  /**
   * @brief Move constructor.
   */
  MemoryMap(MemoryMap&& other) noexcept;

  /**
   * @brief Move assignment operator.
   */
  MemoryMap& operator=(MemoryMap&& other) noexcept;

  // Delete copy constructor and assignment operator to prevent multiple owners
  MemoryMap(const MemoryMap&) = delete;
  MemoryMap& operator=(const MemoryMap&) = delete;

  /**
   * @brief Gets the base address of the mapped memory.
   * @return A void pointer to the start of the memory mapping.
   */
  void* addr() const { return addr_; }

  /**
   * @brief Gets the size of the mapped memory (excluding the mirror if
   * present).
   * @return The size in bytes.
   */
  size_t size() const { return size_; }

  /**
   * @brief Checks if this mapping uses a magic mirror.
   * @return True if mirrored.
   */
  bool is_mirrored() const { return is_mirrored_; }

  /**
   * @brief Gets the name of the shared memory object.
   * @return The name string.
   */
  const std::string& name() const { return name_; }

 private:
  MemoryMap(std::string name, int fd, void* addr, size_t size,
            bool is_mirrored);

  std::string name_;
  int fd_;
  void* addr_;
  size_t size_;
  bool is_mirrored_;
};

}  // namespace cognitas::trading

#endif  // COMMON_SHARED_MEMORY_MEMORY_MAP_H_
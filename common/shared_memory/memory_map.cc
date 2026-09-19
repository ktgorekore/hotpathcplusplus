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

#include "common/shared_memory/memory_map.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace cognitas::trading {

namespace {
absl::Status ErrnoToStatus(const std::string& msg) {
  const std::string error_msg = absl::StrCat(msg, ": ", std::strerror(errno));
  switch (errno) {
    case ENOENT:
      return absl::NotFoundError(error_msg);
    case EEXIST:
      return absl::AlreadyExistsError(error_msg);
    case EACCES:
    case EPERM:
      return absl::PermissionDeniedError(error_msg);
    case ENOMEM:
      return absl::ResourceExhaustedError(error_msg);
    case EINVAL:
      return absl::InvalidArgumentError(error_msg);
    default:
      return absl::InternalError(error_msg);
  }
}
}  // namespace

MemoryMap::MemoryMap(std::string name, int fd, void* addr, size_t size,
                     bool is_mirrored)
    : name_(std::move(name)),
      fd_(fd),
      addr_(addr),
      size_(size),
      is_mirrored_(is_mirrored) {}

MemoryMap::~MemoryMap() {
  if (addr_ != MAP_FAILED && addr_ != nullptr) {
    munmap(addr_, is_mirrored_ ? 2 * size_ : size_);
  }
  if (fd_ != -1) {
    close(fd_);
  }
}

MemoryMap::MemoryMap(MemoryMap&& other) noexcept
    : name_(std::move(other.name_)),
      fd_(other.fd_),
      addr_(other.addr_),
      size_(other.size_),
      is_mirrored_(other.is_mirrored_) {
  other.fd_ = -1;
  other.addr_ = nullptr;
  other.size_ = 0;
  other.is_mirrored_ = false;
}

MemoryMap& MemoryMap::operator=(MemoryMap&& other) noexcept {
  if (this != &other) {
    // Clean up current
    if (addr_ != MAP_FAILED && addr_ != nullptr) {
      munmap(addr_, is_mirrored_ ? 2 * size_ : size_);
    }
    if (fd_ != -1) {
      close(fd_);
    }

    // Move
    name_ = std::move(other.name_);
    fd_ = other.fd_;
    addr_ = other.addr_;
    size_ = other.size_;
    is_mirrored_ = other.is_mirrored_;

    // Reset other
    other.fd_ = -1;
    other.addr_ = nullptr;
    other.size_ = 0;
    other.is_mirrored_ = false;
  }
  return *this;
}

namespace {
absl::StatusOr<void*> DoMap(int fd, size_t size, int prot,
                            bool use_magic_mirror) {
  if (!use_magic_mirror) {
    void* addr = mmap(nullptr, size, prot, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
      return ErrnoToStatus("mmap failed");
    }
    return addr;
  }

  // Magic Mirror logic:
  // 1. Find a contiguous virtual address space of size 2 * size.
  // We do this by mapping 2 * size bytes with PROT_NONE and MAP_PRIVATE |
  // MAP_ANONYMOUS.
  size_t total_size = 2 * size;
  void* base_addr =
      mmap(nullptr, total_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base_addr == MAP_FAILED) {
    return ErrnoToStatus("mmap for address space reservation failed");
  }

  // 2. Map the first half.
  void* first_half = mmap(base_addr, size, prot, MAP_SHARED | MAP_FIXED, fd, 0);
  if (first_half == MAP_FAILED) {
    munmap(base_addr, total_size);
    return ErrnoToStatus("mmap for first half of magic mirror failed");
  }

  // 3. Map the second half to the same file offset 0.
  void* second_half = mmap(static_cast<uint8_t*>(base_addr) + size, size, prot,
                           MAP_SHARED | MAP_FIXED, fd, 0);
  if (second_half == MAP_FAILED) {
    munmap(base_addr, total_size);
    return ErrnoToStatus("mmap for second half of magic mirror failed");
  }

  // Verification removed: Writing to shared memory to verify the mirror
  // can corrupt live data if this process is attaching to an existing buffer.
  // We rely on the OS (mmap) return codes.

  return base_addr;
}
}  // namespace

absl::StatusOr<MemoryMap> MemoryMap::Create(const std::string& name,
                                            size_t size,
                                            bool use_magic_mirror) {
  // Size must be page-aligned for magic mirror.
  const long page_size = sysconf(_SC_PAGESIZE);
  if (use_magic_mirror && (size % page_size != 0)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Size for magic mirror must be page-aligned (", page_size,
                     " bytes). Requested: ", size));
  }

  // O_CREAT | O_EXCL ensures we create it new. O_RDWR for resizing.
  int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
  if (fd == -1) {
    return ErrnoToStatus("shm_open failed in Create");
  }

  // Truncate to size
  if (ftruncate(fd, size) == -1) {
    close(fd);
    shm_unlink(name.c_str());  // Clean up if we failed to size it
    return ErrnoToStatus("ftruncate failed");
  }

  auto map_res = DoMap(fd, size, PROT_READ | PROT_WRITE, use_magic_mirror);
  if (!map_res.ok()) {
    close(fd);
    shm_unlink(name.c_str());
    return map_res.status();
  }

  return MemoryMap(name, fd, *map_res, size, use_magic_mirror);
}

absl::StatusOr<MemoryMap> MemoryMap::Open(const std::string& name, size_t size,
                                          bool readonly,
                                          bool use_magic_mirror) {
  // Size must be page-aligned for magic mirror.
  const long page_size = sysconf(_SC_PAGESIZE);
  if (use_magic_mirror && (size % page_size != 0)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Size for magic mirror must be page-aligned (", page_size,
                     " bytes). Requested: ", size));
  }

  int flags = readonly ? O_RDONLY : O_RDWR;
  int fd = shm_open(name.c_str(), flags, 0666);
  if (fd == -1) {
    return ErrnoToStatus("shm_open failed in Open");
  }

  int prot = readonly ? PROT_READ : (PROT_READ | PROT_WRITE);
  auto map_res = DoMap(fd, size, prot, use_magic_mirror);
  if (!map_res.ok()) {
    close(fd);
    return map_res.status();
  }

  return MemoryMap(name, fd, *map_res, size, use_magic_mirror);
}

absl::StatusOr<MemoryMap> MemoryMap::CreateOrOpen(const std::string& name,
                                                  size_t size,
                                                  bool use_magic_mirror) {
  auto create_res = Create(name, size, use_magic_mirror);
  if (create_res.ok()) {
    return create_res;
  }
  // If it failed because it exists, try Open
  if (absl::IsAlreadyExists(create_res.status())) {
    return Open(name, size, false, use_magic_mirror);
  }
  return create_res.status();
}

absl::Status MemoryMap::Unlink(const std::string& name) {
  if (shm_unlink(name.c_str()) == -1) {
    return ErrnoToStatus("shm_unlink failed");
  }
  return absl::OkStatus();
}

}  // namespace cognitas::trading

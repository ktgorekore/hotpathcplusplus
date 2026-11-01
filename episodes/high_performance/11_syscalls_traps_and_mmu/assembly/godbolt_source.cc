// Copyright 2026 Hot Path C++ Authors
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

#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>

// 1. Ring 3 vDSO clock_gettime (0 syscalls! Reads [vvar] Seqlock + rdtscp)
std::uint64_t ReadClockMonotonicVdso() noexcept {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
         static_cast<std::uint64_t>(ts.tv_nsec);
}

// 2. Forced Ring 0 Syscall Trap (SYS_clock_gettime / SYS_gettid)
pid_t ReadTidRawSyscall() noexcept {
  return static_cast<pid_t>(syscall(SYS_gettid));
}

// 3. Cached Thread-Local Storage TID (Single FS-segment mov in Ring 3!)
pid_t ReadTidCachedTls() noexcept {
  thread_local const pid_t cached_tid = static_cast<pid_t>(syscall(SYS_gettid));
  return cached_tid;
}

// 4. First-Touch Demand Paging Write Loop (Fires 512 #PF Traps if Unwired!)
void Touch512Pages(std::uint8_t* arena_2mib, std::uint8_t tag) noexcept {
  for (std::size_t offset = 0; offset < (2ULL << 20); offset += 4096ULL) {
    arena_2mib[offset] = tag;
  }
}

// 5. Pre-Faulted & HugePage-Advised Order Book Arena Setup
void ConfigureHugePageOrderBook(void* arena, std::size_t bytes) noexcept {
  madvise(arena, bytes, MADV_HUGEPAGE);
  mlock(arena, bytes);
}

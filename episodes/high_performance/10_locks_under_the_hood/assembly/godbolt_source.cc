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

#include <immintrin.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>

// 1. Naive 3-Line TAS Spinlock -> Emits locked `xchg` on EVERY failed spin!
void LockNaiveTAS(std::atomic_flag& flag) noexcept {
  while (flag.test_and_set(std::memory_order_acquire)) {
  }
}

// 2. Hardware-Sympathetic TTAS Spinlock -> Spins in Shared (S) + x86 PAUSE!
void LockTTASPause(std::atomic<bool>& locked) noexcept {
  for (;;) {
    if (!locked.exchange(true, std::memory_order_acquire)) return;
    while (locked.load(std::memory_order_relaxed)) {
      _mm_pause();  // Throttles ROB & prevents 150-cycle Machine Clears!
    }
  }
}

// 3. 3-State Linux futex Fast/Slow Path (Inside std::mutex / pthread_mutex)
void LockRawFutex(std::atomic<uint32_t>& state) noexcept {
  uint32_t expected = 0;
  // Stage 1: Ring 3 Userspace Fast Path (lock cmpxchg 0 -> 1, 0 Syscalls!)
  if (state.compare_exchange_strong(expected, 1, std::memory_order_acquire,
                                    std::memory_order_relaxed)) {
    return;
  }
  // Stage 2: Contended Slow Path -> Leave State 2 Sticky Note & Sleep in Ring 0
  if (expected != 2) {
    expected = state.exchange(2, std::memory_order_acquire);
  }
  while (expected != 0) {
    syscall(SYS_futex, reinterpret_cast<uint32_t*>(&state), FUTEX_WAIT_PRIVATE,
            2, nullptr, nullptr, 0);
    expected = state.exchange(2, std::memory_order_acquire);
  }
}

void UnlockRawFutex(std::atomic<uint32_t>& state) noexcept {
  // Fast Path: If old state was 1 (no sleepers), skip sys_futex completely!
  if (state.exchange(0, std::memory_order_release) == 2) {
    syscall(SYS_futex, reinterpret_cast<uint32_t*>(&state), FUTEX_WAKE_PRIVATE,
            1, nullptr, nullptr, 0);
  }
}

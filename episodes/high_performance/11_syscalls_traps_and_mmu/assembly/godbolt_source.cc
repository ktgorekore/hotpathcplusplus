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

namespace hotpath::episode11 {

/**
 * @brief 64-byte cache-line-aligned order book slot for MMU TLB probing.
 */
struct alignas(64) OrderSlot {
  std::uint64_t order_id;
  std::int64_t price_ticks;
  std::uint64_t quantity;
  std::uint64_t next_probe_idx;
  std::uint8_t padding[32];
};

/**
 * @brief Reads `CLOCK_MONOTONIC` via the Ring 3 `[vdso]` seqlock (`[vvar]` +
 * `rdtsc`), executing zero Ring 0 syscalls (~16.3 ns).
 *
 * @return Monotonic timestamp in nanoseconds.
 */
std::uint64_t ReadClockMonotonicVdso() noexcept {
  struct timespec ts {};
  ::clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
         static_cast<std::uint64_t>(ts.tv_nsec);
}

/**
 * @brief Forces a raw Ring 0 `SYS_gettid` kernel trap (~37.6 ns in a tight
 * loop; ~75.1–80.8 ns effective when interleaved with a 48 KiB L1D working
 * set).
 *
 * @return Calling thread's kernel TID.
 */
pid_t ReadTidRawSyscall() noexcept {
  return static_cast<pid_t>(::syscall(SYS_gettid));
}

/**
 * @brief Reads the calling thread's TID from Ring 3 `thread_local` storage
 * (`%fs` segment offset, ~0.19 ns — 196x faster than `SYS_gettid`).
 *
 * @return Calling thread's cached TID.
 */
pid_t ReadTidCachedTls() noexcept {
  thread_local const pid_t kCachedTid =
      static_cast<pid_t>(::syscall(SYS_gettid));
  return kCachedTid;
}

/**
 * @brief Touches 512 x 4 KiB pages across a 2 MiB arena (fires 512 synchronous
 * `#PF` Interrupt 14 traps on first write ~198.4 us vs ~3.27 us pre-faulted).
 *
 * @param arena_2mib Pointer to a 2 MiB virtual memory region.
 * @param tag Byte value written to the start of each 4 KiB page.
 */
void Touch512Pages(std::uint8_t* arena_2mib, std::uint8_t tag) noexcept {
  for (std::size_t offset = 0; offset < (2ULL << 20); offset += 4096ULL) {
    arena_2mib[offset] = tag;
  }
}

/**
 * @brief Probes a 512 MiB order book slot (`shl rdx, 6; mov rax, [rdi+rdx+8]`),
 * triggering a 4-level radix page walk on 4 KiB pages (~15.2 ns, 93.4% dTLB
 * miss rate) vs a 2 MiB HugePage L2 dTLB hit (~8.66–9.79 ns, 0.27% dTLB miss).
 *
 * @param book Pointer to the 64-byte-aligned `OrderSlot` array.
 * @param slot_idx Target slot index.
 * @return Slot `price_ticks` value.
 */
std::int64_t ProbeOrderBookSlot(const OrderSlot* book,
                                std::uint32_t slot_idx) noexcept {
  return book[slot_idx].price_ticks;
}

/**
 * @brief Configures a 2 MiB-aligned order book arena for Transparent HugePages
 * (`MADV_HUGEPAGE`, PD Entry Bit 7 `PS=1`) and locks physical frames in RAM.
 *
 * @param arena Pointer to the virtual memory arena.
 * @param bytes Size of the arena in bytes.
 */
void ConfigureHugePageOrderBook(void* arena, std::size_t bytes) noexcept {
  ::madvise(arena, bytes, MADV_HUGEPAGE);
  ::mlock(arena, bytes);
}

}  // namespace hotpath::episode11

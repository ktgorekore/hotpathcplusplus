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

/**
 * @file syscalls_traps_and_mmu_test.cc
 * @brief Unit tests for Episode 11 Ring 3 vDSO & TLS primitives, Demand Paging
 * `#PF` minor fault tracking, `PrefaultedArenaPool`, and `OrderBookArena`
 * (`4 KiB` vs `2 MiB` HugePages).
 */

#include "common/syscalls_traps_and_mmu/syscalls_traps_and_mmu.h"

#include <sys/mman.h>

#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace hotpath::syscalls_traps_and_mmu {
namespace {

TEST(SyscallsTrapsAndMmuTest, VdsoAndRawSyscallClocksAreMonotonic) {
  const std::uint64_t t1 = VdsoNowNs();
  const std::uint64_t t2 = RawSyscallNowNs();
  const std::uint64_t t3 = VdsoNowNs();

  EXPECT_GT(t1, 0ULL);
  EXPECT_GE(t2, t1);
  EXPECT_GE(t3, t2);
}

TEST(SyscallsTrapsAndMmuTest, CachedThreadIdMatchesRawSyscallAcrossThreads) {
  EXPECT_EQ(CachedThreadId(), RawSyscallGettid());

  std::vector<std::thread> workers;
  for (int i = 0; i < 8; ++i) {
    workers.emplace_back([]() {
      const pid_t sys_tid = RawSyscallGettid();
      const pid_t tls_tid1 = CachedThreadId();
      const pid_t tls_tid2 = CachedThreadId();
      EXPECT_GT(sys_tid, 0);
      EXPECT_EQ(tls_tid1, sys_tid);
      EXPECT_EQ(tls_tid2, sys_tid);
    });
  }
  for (auto& w : workers) {
    w.join();
  }
}

TEST(SyscallsTrapsAndMmuTest,
     LazyMmapTriggers512MinorFaultsWhilePrefaultedPoolTriggersZero) {
  // 1. Lazy mmap(2 MiB) must trigger >= 512 minor page faults on first write.
  void* raw = mmap(nullptr, kHugePageSize2MiB, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(raw, MAP_FAILED);
  madvise(raw, kHugePageSize2MiB, MADV_NOHUGEPAGE);

  const long flt_before_lazy = ReadMinorFaults();
  TouchPages4KiB(static_cast<std::uint8_t*>(raw), kPagesPer2MiB, 0x42);
  const long flt_after_lazy = ReadMinorFaults();
  munmap(raw, kHugePageSize2MiB);

  EXPECT_GE(flt_after_lazy - flt_before_lazy, static_cast<long>(kPagesPer2MiB));

  // 2. Pre-faulted pool (MAP_POPULATE + mlock) must trigger 0 runtime faults.
  PrefaultedArenaPool pool(kHugePageSize2MiB);
  ASSERT_NE(pool.Acquire(), nullptr);

  const long flt_before_pool = ReadMinorFaults();
  for (int iter = 0; iter < 10; ++iter) {
    TouchPages4KiB(pool.Acquire(), kPagesPer2MiB,
                   static_cast<std::uint8_t>(iter));
  }
  const long flt_after_pool = ReadMinorFaults();

  EXPECT_EQ(flt_after_pool - flt_before_pool, 0L);
}

TEST(SyscallsTrapsAndMmuTest,
     OrderBookArenaProducesIdenticalChecksumsFor4KiBAnd2MiBHugePages) {
  constexpr std::size_t kTestArenaBytes = 4ULL * 1024ULL * 1024ULL;  // 4 MiB
  constexpr std::size_t kLookups = 16'384;

  OrderBookArena book_4k(kTestArenaBytes, /*use_hugepages=*/false, kLookups);
  OrderBookArena book_2m(kTestArenaBytes, /*use_hugepages=*/true, kLookups);

  ASSERT_EQ(book_4k.num_slots(), book_2m.num_slots());
  ASSERT_EQ(book_4k.random_indices().size(), kLookups);

  std::uint64_t acc_4k = 0;
  std::uint64_t acc_2m = 0;
  for (std::size_t i = 0; i < kLookups; ++i) {
    const std::size_t idx_4k = book_4k.random_indices()[i];
    const std::size_t idx_2m = book_2m.random_indices()[i];
    EXPECT_EQ(idx_4k, idx_2m);

    const OrderLevelSlot& s4 = book_4k.slots()[idx_4k];
    const OrderLevelSlot& s2 = book_2m.slots()[idx_2m];
    EXPECT_EQ(s4.price_ticks ^ s4.quantity_lots, s4.checksum);
    EXPECT_EQ(s2.price_ticks ^ s2.quantity_lots, s2.checksum);

    acc_4k += s4.price_ticks + s4.quantity_lots;
    acc_2m += s2.price_ticks + s2.quantity_lots;
  }

  EXPECT_EQ(acc_4k, acc_2m);
  EXPECT_GT(acc_4k, 0ULL);
}

}  // namespace
}  // namespace hotpath::syscalls_traps_and_mmu

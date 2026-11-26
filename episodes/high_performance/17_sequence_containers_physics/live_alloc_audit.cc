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
 * @file live_alloc_audit.cc
 * @brief Dedicated global `operator new` allocation auditor for Episode 17
 *        Stories 1, 2, and 3 inside `RealtimeGatewayService`.
 *
 * Kept strictly separate from timed benchmark binaries (`benchmark`,
 * `challenge`) so timed runs use stock glibc allocation without
 * instrumentation overhead.
 */

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <new>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "boost/circular_buffer.hpp"
#include "common/sequence_containers/sequence_containers.h"
#include "folly/FBVector.h"

ABSL_FLAG(std::string, story, "all", "Story filter: all, 1b, 2, 2a, 2b, 3");

namespace {

thread_local bool g_track_allocs = false;
thread_local std::uint64_t g_alloc_count = 0;
thread_local std::uint64_t g_alloc_bytes = 0;

void* TrackedMalloc(std::size_t size) {
  if (g_track_allocs) {
    ++g_alloc_count;
    g_alloc_bytes += size;
  }
  void* ptr = std::malloc(size == 0 ? 1U : size);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

void* TrackedAlignedMalloc(std::size_t size, std::align_val_t al) {
  if (g_track_allocs) {
    ++g_alloc_count;
    g_alloc_bytes += size;
  }
  const std::size_t align = static_cast<std::size_t>(al);
  const std::size_t padded = ((size + align - 1U) / align) * align;
  void* ptr = std::aligned_alloc(align, padded == 0 ? align : padded);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

class AllocScope {
 public:
  AllocScope() {
    g_alloc_count = 0;
    g_alloc_bytes = 0;
    g_track_allocs = true;
  }
  ~AllocScope() { g_track_allocs = false; }
  [[nodiscard]] std::uint64_t Count() const noexcept { return g_alloc_count; }
  [[nodiscard]] std::uint64_t Bytes() const noexcept { return g_alloc_bytes; }
};

}  // namespace

void* operator new(std::size_t size) { return TrackedMalloc(size); }
void* operator new[](std::size_t size) { return TrackedMalloc(size); }
void* operator new(std::size_t size, std::align_val_t al) {
  return TrackedAlignedMalloc(size, al);
}
void* operator new[](std::size_t size, std::align_val_t al) {
  return TrackedAlignedMalloc(size, al);
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::align_val_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::align_val_t) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept {
  std::free(ptr);
}
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept {
  std::free(ptr);
}

namespace hotpath::episode17 {
namespace {

void AuditStory1BDualQueue() {
  const auto wl = BuildDualQueueEventWorkload(16'384, 200'000);
  LOG(INFO) << "=== STORY 1B: 16,384 Pinned 256B Connections (Dual LRU + "
               "Timeout Queues, 200K Events) ===";
  {
    std::uint64_t allocs = 0, bytes = 0, cs = 0;
    {
      AllocScope scope;
      cs = RunDualQueueIntrusive(wl);
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    LOG(INFO) << absl::StrFormat(
        "  %-40s | allocs=%8llu (1 slab, 0 nodes) | bytes=%6.2f MiB | "
        "cs=%016llx",
        "boost::intrusive::list (dual hooks)",
        static_cast<unsigned long long>(allocs),
        static_cast<double>(bytes) / (1024.0 * 1024.0),
        static_cast<unsigned long long>(cs));
  }
  {
    std::uint64_t allocs = 0, bytes = 0, cs = 0;
    {
      AllocScope scope;
      cs = RunDualQueueStdListSplice(wl);
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    LOG(INFO) << absl::StrFormat(
        "  %-40s | allocs=%8llu (+32,768 nodes)  | bytes=%6.2f MiB | "
        "cs=%016llx",
        "std::list<ActiveConnection*> (splice)",
        static_cast<unsigned long long>(allocs),
        static_cast<double>(bytes) / (1024.0 * 1024.0),
        static_cast<unsigned long long>(cs));
  }
  {
    std::uint64_t allocs = 0, bytes = 0, cs = 0;
    {
      AllocScope scope;
      cs = RunDualQueueVectorIndexedRotate(wl);
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    LOG(INFO) << absl::StrFormat(
        "  %-40s | allocs=%8llu (1 slab + 4 vecs) | bytes=%6.2f MiB | "
        "cs=%016llx",
        "std::vector<u32> (O(1) idx + memmove)",
        static_cast<unsigned long long>(allocs),
        static_cast<double>(bytes) / (1024.0 * 1024.0),
        static_cast<unsigned long long>(cs));
  }
}

void AuditStory2DequeVsRing() {
  constexpr std::size_t kQueues = 10'000;
  LOG(INFO) << absl::StrFormat(
      "=== STORY 2A: %zu Idle Per-Connection Outbound Queues ===", kQueues);

  auto log_empty = [](const char* label, std::uint64_t allocs,
                      std::uint64_t bytes, std::size_t elem_size) {
    LOG(INFO) << absl::StrFormat(
        "  %-45s | allocs=%8llu | bytes=%6.2f MiB | sizeof=%zuB", label,
        static_cast<unsigned long long>(allocs),
        static_cast<double>(bytes) / (1024.0 * 1024.0), elem_size);
  };

  {
    std::uint64_t allocs = 0, bytes = 0;
    {
      AllocScope scope;
      std::vector<std::deque<OutboundMessage>> queues(kQueues);
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    log_empty("std::deque<OutboundMessage> (10K empty)", allocs, bytes,
              sizeof(std::deque<OutboundMessage>));
  }
  {
    std::uint64_t allocs = 0, bytes = 0;
    {
      using SpaceOptRing =
          boost::circular_buffer_space_optimized<OutboundMessage>;
      AllocScope scope;
      std::vector<SpaceOptRing> queues(
          kQueues, SpaceOptRing(SpaceOptRing::capacity_type(1'024, 0)));
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    log_empty("boost::circular_buffer_space_optimized (10K)", allocs, bytes,
              sizeof(boost::circular_buffer_space_optimized<OutboundMessage>));
  }
  {
    std::uint64_t allocs = 0, bytes = 0;
    {
      AllocScope scope;
      std::vector<CompactRingDeque<OutboundMessage>> queues(kQueues);
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    log_empty("CompactRingDeque<OutboundMessage> (10K empty)", allocs, bytes,
              sizeof(CompactRingDeque<OutboundMessage>));
  }

  LOG(INFO) << "=== STORY 2B: Steady-State FIFO Stream (8,000,000 push+pop @ "
               "depth 1,024) ===";
  {
    std::deque<OutboundMessage> q;
    for (std::size_t i = 0; i < 1'024; ++i) {
      q.push_back(MakeOutboundMessage(i));
    }
    std::uint64_t allocs = 0, bytes = 0, cs = 0x17F1F0ULL;
    {
      AllocScope scope;
      for (std::size_t i = 0; i < 8'000'000; ++i) {
        cs = MixOrderHash(cs, DigestMessage(q.front()));
        q.pop_front();
        q.push_back(MakeOutboundMessage(1'024 + i));
      }
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    LOG(INFO) << absl::StrFormat(
        "  %-45s | steady_allocs=%8llu | bytes=%7.2f MiB | cs=%016llx",
        "std::deque<OutboundMessage> (512B chunks)",
        static_cast<unsigned long long>(allocs),
        static_cast<double>(bytes) / (1024.0 * 1024.0),
        static_cast<unsigned long long>(cs));
  }
  {
    boost::circular_buffer<OutboundMessage> q(1'024);
    for (std::size_t i = 0; i < 1'024; ++i) {
      q.push_back(MakeOutboundMessage(i));
    }
    std::uint64_t allocs = 0, bytes = 0, cs = 0x17F1F0ULL;
    {
      AllocScope scope;
      for (std::size_t i = 0; i < 8'000'000; ++i) {
        cs = MixOrderHash(cs, DigestMessage(q.front()));
        q.pop_front();
        q.push_back(MakeOutboundMessage(1'024 + i));
      }
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    LOG(INFO) << absl::StrFormat(
        "  %-45s | steady_allocs=%8llu | bytes=%7.2f MiB | cs=%016llx",
        "boost::circular_buffer<OutboundMessage>",
        static_cast<unsigned long long>(allocs),
        static_cast<double>(bytes) / (1024.0 * 1024.0),
        static_cast<unsigned long long>(cs));
  }
  {
    CompactRingDeque<OutboundMessage> q(1'024);
    for (std::size_t i = 0; i < 1'024; ++i) {
      q.push_back(MakeOutboundMessage(i));
    }
    std::uint64_t allocs = 0, bytes = 0, cs = 0x17F1F0ULL;
    {
      AllocScope scope;
      for (std::size_t i = 0; i < 8'000'000; ++i) {
        cs = MixOrderHash(cs, DigestMessage(q.front()));
        q.pop_front();
        q.push_back(MakeOutboundMessage(1'024 + i));
      }
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    LOG(INFO) << absl::StrFormat(
        "  %-45s | steady_allocs=%8llu | bytes=%7.2f MiB | cs=%016llx",
        "CompactRingDeque<OutboundMessage>",
        static_cast<unsigned long long>(allocs),
        static_cast<double>(bytes) / (1024.0 * 1024.0),
        static_cast<unsigned long long>(cs));
  }
}

void AuditStory3SessionGrowth() {
  constexpr std::size_t kSessions = 150'000;
  LOG(INFO) << absl::StrFormat(
      "=== STORY 3: Gateway Reconnect Wave — %zu Slot-Indexed ClientSessions "
      "===",
      kSessions);

  auto measure = [&](const char* label, auto fn) {
    std::uint64_t allocs = 0, bytes = 0;
    SessionGrowthMetrics m{};
    {
      AllocScope scope;
      m = fn();
      allocs = scope.Count();
      bytes = scope.Bytes();
    }
    LOG(INFO) << absl::StrFormat(
        "  %-45s | allocs=%9llu | bytes=%7.2f MiB | cap=%6zu | cs=%016llx",
        label, static_cast<unsigned long long>(allocs),
        static_cast<double>(bytes) / (1024.0 * 1024.0), m.final_capacity,
        static_cast<unsigned long long>(m.checksum));
  };

  measure("std::vector + std::queue (unreserved)", [&] {
    return RunSessionTableGrowth<std::vector<SessionRuleOfZeroWithStdQueue>>(
        kSessions);
  });
  measure("std::vector + std::queue (.reserve(150K))", [&] {
    return RunSessionTableGrowth<std::vector<SessionRuleOfZeroWithStdQueue>,
                                 true>(kSessions);
  });
  measure("std::vector + Custom Move (missing noexcept)", [&] {
    return RunSessionTableGrowth<std::vector<SessionMissingNoexceptMove>>(
        kSessions);
  });
  measure("std::vector + CompactRingDeque (2.0x)", [&] {
    return RunSessionTableGrowth<std::vector<SessionNoexceptRingQueue>>(
        kSessions);
  });
  measure("std::vector<unique_ptr<Session>> (2.0x)",
          [&] { return RunSessionPointerVectorGrowth(kSessions); });
  measure("std::vector + CompactRingDeque (.reserve)", [&] {
    return RunSessionTableGrowth<std::vector<SessionNoexceptRingQueue>, true>(
        kSessions);
  });
  measure("folly::fbvector + CompactRingDeque (1.5x)", [&] {
    return RunSessionTableGrowth<folly::fbvector<SessionNoexceptRingQueue>>(
        kSessions);
  });
}

}  // namespace
}  // namespace hotpath::episode17

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  const std::string story = absl::GetFlag(FLAGS_story);
  if (story == "all" || story == "1b") {
    hotpath::episode17::AuditStory1BDualQueue();
  }
  if (story == "all" || story == "2" || story == "2a" || story == "2b") {
    hotpath::episode17::AuditStory2DequeVsRing();
  }
  if (story == "all" || story == "3") {
    hotpath::episode17::AuditStory3SessionGrowth();
  }
  return 0;
}

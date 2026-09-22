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

#include "common/concurrency/ebr.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace cognitas::trading {

namespace {

constexpr uint64_t kEpochInactive = 0;

struct ThreadControlBlock {
  std::atomic<uint64_t> local_epoch{kEpochInactive};
  std::atomic<bool> active{true};
  ThreadControlBlock* next = nullptr;

  struct Batch {
    uint64_t epoch;
    std::vector<EbrNode*> nodes;
  };
  std::vector<Batch> retirement_batches;
};

std::atomic<uint64_t> global_epoch_{1};
std::atomic<ThreadControlBlock*> head_{nullptr};
std::mutex list_mutex_;

thread_local ThreadControlBlock* tls_tcb = nullptr;
thread_local int critical_depth = 0;

}  // namespace

void EpochBasedReclamation::RegisterThread() {
  if (tls_tcb != nullptr) return;

  auto* tcb = new ThreadControlBlock();

  std::lock_guard<std::mutex> lock(list_mutex_);
  tcb->next = head_.load(std::memory_order_relaxed);
  head_.store(tcb, std::memory_order_release);

  tls_tcb = tcb;
}

void EpochBasedReclamation::UnregisterThread() {
  if (tls_tcb == nullptr) return;

  tls_tcb->active.store(false, std::memory_order_release);
  tls_tcb->local_epoch.store(kEpochInactive, std::memory_order_release);
  tls_tcb = nullptr;
}

void EpochBasedReclamation::EnterCritical() noexcept {
  if (tls_tcb == nullptr) {
    RegisterThread();
  }

  if (critical_depth++ == 0) {
    uint64_t epoch = global_epoch_.load(std::memory_order_relaxed);
    tls_tcb->local_epoch.store(epoch, std::memory_order_release);
  }
}

void EpochBasedReclamation::ExitCritical() noexcept {
  if (tls_tcb == nullptr) return;

  if (--critical_depth == 0) {
    tls_tcb->local_epoch.store(kEpochInactive, std::memory_order_release);
  }
}

bool EpochBasedReclamation::IsInCritical() noexcept {
  return critical_depth > 0;
}

void EpochBasedReclamation::Retire(EbrNode* node) {
  if (node == nullptr) return;

  if (tls_tcb == nullptr) {
    RegisterThread();
  }

  uint64_t current_epoch = global_epoch_.load(std::memory_order_relaxed);

  if (tls_tcb->retirement_batches.empty() ||
      tls_tcb->retirement_batches.back().epoch != current_epoch) {
    tls_tcb->retirement_batches.push_back({current_epoch, {}});
  }
  tls_tcb->retirement_batches.back().nodes.push_back(node);

  static constexpr size_t kBatchThreshold = 64;
  if (tls_tcb->retirement_batches.back().nodes.size() >= kBatchThreshold) {
    Gc();
  }
}

void EpochBasedReclamation::Gc() {
  uint64_t min_epoch = global_epoch_.load(std::memory_order_acquire);

  ThreadControlBlock* curr = head_.load(std::memory_order_acquire);
  while (curr) {
    if (curr->active.load(std::memory_order_relaxed)) {
      uint64_t local = curr->local_epoch.load(std::memory_order_acquire);
      if (local != kEpochInactive && local < min_epoch) {
        min_epoch = local;
      }
    }
    curr = curr->next;
  }

  uint64_t current_global = global_epoch_.load(std::memory_order_relaxed);
  if (min_epoch == current_global) {
    global_epoch_.compare_exchange_strong(current_global, current_global + 1,
                                          std::memory_order_release);
  }

  if (tls_tcb) {
    auto& batches = tls_tcb->retirement_batches;
    auto it = batches.begin();
    while (it != batches.end()) {
      if (it->epoch < min_epoch) {
        for (auto* node : it->nodes) {
          delete node;
        }
        it = batches.erase(it);
      } else {
        ++it;
      }
    }
  }
}

}  // namespace cognitas::trading

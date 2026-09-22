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

#ifndef COMMON_CONCURRENCY_EBR_H_
#define COMMON_CONCURRENCY_EBR_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cognitas::trading {

/**
 * @struct EbrNode
 * @brief Base class for any dynamically allocated node managed by EBR.
 */
struct EbrNode {
  EbrNode* next_retired = nullptr;
  virtual ~EbrNode() = default;
};

/**
 * @class EpochBasedReclamation
 * @brief High-performance, zero-dependency Epoch-Based Reclamation (EBR)
 * engine.
 *
 * Coordinates thread-local read epochs and deferred memory reclamation,
 * enabling wait-free readers without atomic reference counting overhead.
 */
class EpochBasedReclamation {
 public:
  /**
   * @brief Registers the calling thread for EBR participation.
   * Allocates or binds a Thread Control Block (TCB) in the global list.
   */
  static void RegisterThread();

  /**
   * @brief Unregisters the calling thread upon exit to prevent stalling
   * reclamation.
   */
  static void UnregisterThread();

  /**
   * @brief Enters an EBR read-side critical section.
   */
  static void EnterCritical() noexcept;

  /**
   * @brief Exits an EBR read-side critical section.
   */
  static void ExitCritical() noexcept;

  /**
   * @brief Schedules an unlinked node for deferred deletion.
   * @param node The retired node to reclaim once older epochs quiesce.
   */
  static void Retire(EbrNode* node);

  /**
   * @brief Attempts to advance the global epoch and reclaim obsolete memory
   * batches.
   */
  static void Gc();

  /**
   * @brief Queries whether the calling thread is currently inside a critical
   * section.
   * @return true if currently within EnterCritical/ExitCritical scope.
   */
  static bool IsInCritical() noexcept;
};

/**
 * @class EbrGuard
 * @brief RAII scoped guard for entering and exiting an EBR critical section.
 */
class EbrGuard {
 public:
  EbrGuard() noexcept { EpochBasedReclamation::EnterCritical(); }
  ~EbrGuard() noexcept { EpochBasedReclamation::ExitCritical(); }

  EbrGuard(const EbrGuard&) = delete;
  EbrGuard& operator=(const EbrGuard&) = delete;
};

}  // namespace cognitas::trading

namespace hotpath {
using cognitas::trading::EbrGuard;
using cognitas::trading::EbrNode;
using cognitas::trading::EpochBasedReclamation;
}  // namespace hotpath

#endif  // COMMON_CONCURRENCY_EBR_H_

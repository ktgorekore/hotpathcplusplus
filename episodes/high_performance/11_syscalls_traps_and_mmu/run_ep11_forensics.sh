#!/usr/bin/env bash
# Copyright 2026 Hot Path C++ Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# ==============================================================================
# Hot Path C++ — Episode 11 Hardware Telemetry & perf stat Runner
# File: episodes/11_syscalls_traps_and_mmu/run_ep11_forensics.sh
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

echo "======================================================================"
echo " 1. Building Episode 11 Forensic Suite via Bazel (-c opt)..."
echo "======================================================================"
cd "${REPO_ROOT}"
bazel build -c opt //episodes/high_performance/11_syscalls_traps_and_mmu:challenge \
  //episodes/high_performance/11_syscalls_traps_and_mmu:benchmark

CHALLENGE_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/11_syscalls_traps_and_mmu/challenge"
BENCHMARK_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/11_syscalls_traps_and_mmu/benchmark"

echo ""
echo "======================================================================"
echo " 2. Running Live Forensic Telemetry Suite (challenge)..."
echo "======================================================================"
"${CHALLENGE_BIN}"

if command -v perf >/dev/null 2>&1; then
  echo ""
  echo "======================================================================"
  echo " 3. Capturing Hardware PMU Counters (dTLB-load-misses & page-faults)"
  echo "======================================================================"
  perf stat -e cycles,instructions,dTLB-loads,dTLB-load-misses,page-faults,minor-faults \
    "${CHALLENGE_BIN}" || true

  echo ""
  echo "======================================================================"
  echo " 4. Isolated PMU Comparison: 512 MiB 4 KiB Pages vs. 2 MiB HugePages"
  echo "======================================================================"
  perf stat -e cycles,instructions,dTLB-loads,dTLB-load-misses,page-faults,minor-faults \
    "${CHALLENGE_BIN}" 4k_512m || true
  perf stat -e cycles,instructions,dTLB-loads,dTLB-load-misses,page-faults,minor-faults \
    "${CHALLENGE_BIN}" 2m_512m || true
fi

echo ""
echo "======================================================================"
echo " 5. Running Full Google Benchmark Suite (benchmark.cc)..."
echo "======================================================================"
"${BENCHMARK_BIN}" --benchmark_min_time=0.25s

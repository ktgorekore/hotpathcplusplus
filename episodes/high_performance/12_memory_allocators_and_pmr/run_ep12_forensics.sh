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
# Hot Path C++ — Episode 12 Memory Allocators, Arenas & std::pmr Forensics
# File: episodes/high_performance/12_memory_allocators_and_pmr/run_ep12_forensics.sh
# ==============================================================================
set -euo pipefail

EP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${EP_DIR}/../../.." && pwd)"
OUT_DIR="/tmp/ep12_telemetry"
mkdir -p "${OUT_DIR}" "${EP_DIR}/assembly"

echo "========================================================================"
echo "EPISODE 12: MEMORY ALLOCATORS, ARENAS & std::pmr FORENSIC SUITE"
echo "Host CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | xargs)"
echo "Kernel:   $(uname -srm)"
echo "GCC:      $(g++ --version | head -n 1)"
echo "Clang:    $(clang++ --version | head -n 1)"
echo "Glibc:    $(ldd --version | head -n 1)"
echo "========================================================================"

cd "${REPO_ROOT}"

echo ""
echo ">>> [1/6] Running GoogleTest Verification Suite..."
bazel test -c opt //common/memory_allocators:memory_allocators_test \
  --test_output=errors

echo ""
echo ">>> [2/6] Building Episode 12 Optimized Targets (-c opt)..."
bazel build -c opt \
  //episodes/high_performance/12_memory_allocators_and_pmr:live_heap_aging_repro \
  //episodes/high_performance/12_memory_allocators_and_pmr:challenge \
  //episodes/high_performance/12_memory_allocators_and_pmr:benchmark

REPRO_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/12_memory_allocators_and_pmr/live_heap_aging_repro"
CHALLENGE_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/12_memory_allocators_and_pmr/challenge"
BENCHMARK_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/12_memory_allocators_and_pmr/benchmark"

echo ""
echo ">>> [3/6] Running Cold-Open Live Reproducer (glibc ptmalloc2 vs Arena/PMR)..."
"${REPRO_BIN}" | tee "${OUT_DIR}/cold_open_glibc.txt"

JEMALLOC_SO="/lib/x86_64-linux-gnu/libjemalloc.so.2"
if [[ -f "${JEMALLOC_SO}" ]]; then
  echo ""
  echo ">>> [3b/6] Comparing Cold-Open & Cross-Thread under jemalloc (LD_PRELOAD)..."
  LD_PRELOAD="${JEMALLOC_SO}" "${REPRO_BIN}" | tee "${OUT_DIR}/cold_open_jemalloc.txt"
fi

echo ""
echo ">>> [4/6] Running Forensic Challenge + Two-Pass Unmultiplexed perf stat..."
"${CHALLENGE_BIN}" all | tee "${OUT_DIR}/challenge_all_glibc.txt"

if [[ -f "${JEMALLOC_SO}" ]]; then
  echo ""
  echo "--- jemalloc Cross-Thread & Burst Verification ---"
  LD_PRELOAD="${JEMALLOC_SO}" "${CHALLENGE_BIN}" --mode=b1_tcache7 | tee "${OUT_DIR}/jemalloc_b1_7.txt"
  LD_PRELOAD="${JEMALLOC_SO}" "${CHALLENGE_BIN}" --mode=b1_tcache1024 | tee "${OUT_DIR}/jemalloc_b1_1024.txt"
  LD_PRELOAD="${JEMALLOC_SO}" "${CHALLENGE_BIN}" --mode=b1_xthread_std | tee "${OUT_DIR}/jemalloc_xthread.txt"
  LD_PRELOAD="${JEMALLOC_SO}" "${CHALLENGE_BIN}" --mode=b2_aged_scan | tee "${OUT_DIR}/jemalloc_aged_scan.txt"
fi

if command -v perf >/dev/null 2>&1; then
  for MODE in b2_fresh_scan b2_aged_scan b2_slab_scan b2_pmr_scan b2_arena_scan b3_std_parse b3_broken_pmr b3_pmr_stack; do
    echo ""
    echo "--- perf stat [Pass 1: L1D & Instructions] mode=${MODE} ---"
    perf stat -e cycles,instructions,L1-dcache-loads,L1-dcache-load-misses,cache-misses \
      "${CHALLENGE_BIN}" "--mode=${MODE}" 2>&1 | tee "${OUT_DIR}/perf_${MODE}_p1.txt" || true
    echo "--- perf stat [Pass 2: dTLB & Page Faults] mode=${MODE} ---"
    perf stat -e cycles,dTLB-loads,dTLB-load-misses,cache-references,cache-misses,page-faults \
      "${CHALLENGE_BIN}" "--mode=${MODE}" 2>&1 | tee "${OUT_DIR}/perf_${MODE}_p2.txt" || true
  done
fi

echo ""
echo ">>> [5/6] Generating Annotated Intel-Syntax Assembly (assembly/godbolt_comparison.s)..."
g++ -std=c++20 -O3 -march=native -S -masm=intel -fno-asynchronous-unwind-tables \
  -fno-dwarf2-cfi-asm "${EP_DIR}/assembly/godbolt_source.cc" \
  -o "${OUT_DIR}/godbolt_gcc.s"

{
  echo "# Copyright 2026 Hot Path C++ Authors"
  echo "#"
  echo "# Licensed under the Apache License, Version 2.0 (the \"License\");"
  echo "# you may not use this file except in compliance with the License."
  echo "# You may obtain a copy of the License at"
  echo "#"
  echo "#     http://www.apache.org/licenses/LICENSE-2.0"
  echo "#"
  echo "# Unless required by applicable law or agreed to in writing, software"
  echo "# distributed under the License is distributed on an \"AS IS\" BASIS,"
  echo "# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied."
  echo "# See the License for the specific language governing permissions and"
  echo "# limitations under the License."
  echo ""
  echo "# Hot Path C++ Episode 12: Annotated Intel-Syntax x86-64 Disassembly"
  echo "# Generated with: g++ -std=c++20 -O3 -march=native -S -masm=intel"
  echo ""
  cat "${OUT_DIR}/godbolt_gcc.s"
} > "${EP_DIR}/assembly/godbolt_comparison.s"

echo ""
echo ">>> [6/6] Running Full Google Benchmark Suite..."
"${BENCHMARK_BIN}" --benchmark_min_time=0.25s | tee "${OUT_DIR}/google_benchmark.txt"

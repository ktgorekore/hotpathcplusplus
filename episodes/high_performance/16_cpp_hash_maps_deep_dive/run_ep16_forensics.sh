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
# Hot Path C++ — Episode 16: Production C++ Hash Maps Forensic Telemetry Runner
# File: episodes/high_performance/16_cpp_hash_maps_deep_dive/run_ep16_forensics.sh
# ==============================================================================
set -euo pipefail

EP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${EP_DIR}/../../.." && pwd)"
OUT_DIR="/tmp/ep16_telemetry"
mkdir -p "${OUT_DIR}" "${EP_DIR}/assembly"

echo "========================================================================"
echo "EPISODE 16: PRODUCTION C++ HASH MAPS FORENSIC TELEMETRY RUNNER"
echo "Host CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | xargs)"
echo "Kernel:   $(uname -srm)"
echo "GCC:      $(g++ --version | head -n 1)"
echo "Clang:    $(clang++ --version | head -n 1)"
echo "Bazel:    $(bazel --version)"
echo "========================================================================"

cd "${REPO_ROOT}"

echo ""
echo ">>> [1/7] Running Unit Tests & Building Optimized Targets (-c opt)..."
bazel test -c opt \
  //common/cpp_hash_maps:cpp_hash_maps_test \
  --test_output=errors

bazel build -c opt \
  //episodes/high_performance/16_cpp_hash_maps_deep_dive:benchmark \
  //episodes/high_performance/16_cpp_hash_maps_deep_dive:challenge \
  //episodes/high_performance/16_cpp_hash_maps_deep_dive:live_alloc_audit \
  //episodes/high_performance/16_cpp_hash_maps_deep_dive:live_uaf_migration_repro

BENCH="${REPO_ROOT}/bazel-bin/episodes/high_performance/16_cpp_hash_maps_deep_dive/benchmark"
CHALLENGE="${REPO_ROOT}/bazel-bin/episodes/high_performance/16_cpp_hash_maps_deep_dive/challenge"
ALLOC_AUDIT="${REPO_ROOT}/bazel-bin/episodes/high_performance/16_cpp_hash_maps_deep_dive/live_alloc_audit"
UAF_REPRO="${REPO_ROOT}/bazel-bin/episodes/high_performance/16_cpp_hash_maps_deep_dive/live_uaf_migration_repro"

echo ""
echo ">>> [2/7] Running Allocation Audit (Stories 1 & 2) & Pointer-Stability Reproducer (Story 5)..."
"${ALLOC_AUDIT}" | tee "${OUT_DIR}/01_alloc_audit.txt"
"${UAF_REPRO}" | tee "${OUT_DIR}/02_pointer_stability.txt"

echo ""
echo ">>> [2B/7] Verifying Story 5 Live Migration UAF Under AddressSanitizer..."
bazel build --copt=-fsanitize=address --linkopt=-fsanitize=address --copt=-g \
  //episodes/high_performance/16_cpp_hash_maps_deep_dive:live_uaf_migration_repro
ASAN_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/16_cpp_hash_maps_deep_dive/live_uaf_migration_repro"
set +e
"${ASAN_BIN}" --trigger-uaf > "${OUT_DIR}/02b_asan_uaf_repro.txt" 2>&1
ASAN_EXIT=$?
set -e
echo "ASan migration UAF reproducer exited with code ${ASAN_EXIT} (expected non-zero)."
grep -E "ERROR: AddressSanitizer|SUMMARY:|TriggerFlatMapMigrationUaf" "${OUT_DIR}/02b_asan_uaf_repro.txt" | head -n 10 || true

# Rebuild clean -c opt binaries after ASan build so bazel-bin holds optimized artifacts.
bazel build -c opt \
  //episodes/high_performance/16_cpp_hash_maps_deep_dive:benchmark \
  //episodes/high_performance/16_cpp_hash_maps_deep_dive:challenge

echo ""
echo ">>> [3/7] Running Isolated Fresh-Process VmRSS Audits (Story 3: 1M Post-Double & 800K Pre-Double)..."
{
  echo "=== Story 3A: Isolated VmRSS @ 1,000,000 Entries (Post-Doubling ~48% Flat Load) ==="
  for m in \
    rss_32_std rss_32_absl rss_32_f14_val rss_32_f14_fast rss_32_boost \
    rss_128_std rss_128_absl rss_128_f14_val rss_128_f14_fast rss_128_boost \
    rss_128_absl_node rss_128_f14_node rss_128_boost_node rss_128_absl_uptr \
    rss_256_std rss_256_absl rss_256_f14_val rss_256_f14_fast rss_256_boost \
    rss_256_absl_node rss_256_f14_node rss_256_boost_node rss_256_absl_uptr; do
    "${CHALLENGE}" --mode="${m}" --count=1000000
  done
  echo ""
  echo "=== Story 3B: Isolated VmRSS @ 800,000 Entries (Pre-Doubling ~76% Flat Load) ==="
  for m in \
    rss_32_std rss_32_absl rss_32_f14_val rss_32_f14_fast rss_32_boost \
    rss_128_std rss_128_absl rss_128_f14_val rss_128_f14_fast rss_128_boost \
    rss_128_absl_uptr \
    rss_256_std rss_256_absl rss_256_f14_val rss_256_f14_fast rss_256_boost \
    rss_256_absl_uptr; do
    "${CHALLENGE}" --mode="${m}" --count=800000
  done
} | tee "${OUT_DIR}/03_isolated_rss.txt"

echo ""
echo ">>> [4/7] Running Full Forensic Suites (Stories 1, 2, 3, 4, 5)..."
"${CHALLENGE}" --mode=story1 --iters=10 | tee "${OUT_DIR}/04_story1_routes.txt"
"${CHALLENGE}" --mode=story2 --iters=250000 | tee "${OUT_DIR}/05_story2_small_maps.txt"
"${CHALLENGE}" --mode=story3_5 --iters=10 | tee "${OUT_DIR}/06_story3_5_large_and_node.txt"
"${CHALLENGE}" --mode=story4 --iters=4000000 | tee "${OUT_DIR}/07_story4_churn.txt"

echo ""
echo ">>> [5/7] Running Official Google Benchmark Suite..."
"${BENCH}" --benchmark_min_time=0.25s | tee "${OUT_DIR}/08_google_benchmark.txt"

echo ""
echo ">>> [6/7] Running 3-Pass Unmultiplexed Linux perf stat PMU Profiling..."
PMU_CORE="cycles,instructions,branches,branch-misses"
PMU_L1_TLB="L1-dcache-loads,L1-dcache-load-misses,dTLB-loads,dTLB-load-misses"
PMU_LLC="cache-references,cache-misses,page-faults"

run_perf_3pass() {
  local tag="$1"
  shift
  echo "=== perf stat [${tag}] ==="
  echo "--- Pass 1 (Core & Branches) ---"
  perf stat -e "${PMU_CORE}" "$@" > /dev/null
  echo "--- Pass 2 (L1D & dTLB) ---"
  perf stat -e "${PMU_L1_TLB}" "$@" > /dev/null
  echo "--- Pass 3 (LLC & Page Faults) ---"
  perf stat -e "${PMU_LLC}" "$@" > /dev/null
}

{
  # Story 1 Zipfian Route Lookups (20M queries)
  run_perf_3pass "S1 Zipf std::unordered_map Default (Alloc)" "${CHALLENGE}" --mode=s1_std_def --iters=20
  run_perf_3pass "S1 Zipf std::unordered_map C++20 Transparent" "${CHALLENGE}" --mode=s1_std_tr --iters=20
  run_perf_3pass "S1 Zipf std::pmr::unordered_map Monotonic" "${CHALLENGE}" --mode=s1_pmr --iters=20
  run_perf_3pass "S1 Zipf absl::flat_hash_map" "${CHALLENGE}" --mode=s1_absl --iters=20
  run_perf_3pass "S1 Zipf folly::F14FastMap" "${CHALLENGE}" --mode=s1_f14_fast --iters=20
  run_perf_3pass "S1 Zipf boost::unordered_flat_map" "${CHALLENGE}" --mode=s1_boost --iters=20

  # Story 1 100% Negative Misses (20M queries)
  run_perf_3pass "S1 100%Miss std::unordered_map Transparent" "${CHALLENGE}" --mode=s1_std_tr_miss --iters=20
  run_perf_3pass "S1 100%Miss absl::flat_hash_map" "${CHALLENGE}" --mode=s1_absl_miss --iters=20
  run_perf_3pass "S1 100%Miss folly::F14FastMap" "${CHALLENGE}" --mode=s1_f14_fast_miss --iters=20
  run_perf_3pass "S1 100%Miss boost::unordered_flat_map" "${CHALLENGE}" --mode=s1_boost_miss --iters=20

  # Story 3 256B Full-Table Iteration (20 sweeps = 20M entries)
  run_perf_3pass "S3 Iter256B std::unordered_map" "${CHALLENGE}" --mode=s3_iter256_std --iters=20
  run_perf_3pass "S3 Iter256B absl::flat_hash_map" "${CHALLENGE}" --mode=s3_iter256_absl --iters=20
  run_perf_3pass "S3 Iter256B folly::F14FastMap (Dense Vector)" "${CHALLENGE}" --mode=s3_iter256_f14_fast --iters=20
  run_perf_3pass "S3 Iter256B boost::unordered_flat_map" "${CHALLENGE}" --mode=s3_iter256_boost --iters=20

  # Story 4 4M Steady-State Eviction Churn Cycles (Band A: 780K & Band B: 640K F14Vector high-fill)
  run_perf_3pass "S4 Churn4M (780K) std::unordered_map" "${CHALLENGE}" --mode=s4_churn_std --iters=4000000 --count=780000
  run_perf_3pass "S4 Churn4M (780K) absl::flat_hash_map (Tombstones)" "${CHALLENGE}" --mode=s4_churn_absl --iters=4000000 --count=780000
  run_perf_3pass "S4 Churn4M (780K) folly::F14ValueMap (RefCnt Ofw)" "${CHALLENGE}" --mode=s4_churn_f14_val --iters=4000000 --count=780000
  run_perf_3pass "S4 Churn4M (780K) folly::F14FastMap (Vector 50%)" "${CHALLENGE}" --mode=s4_churn_f14_fast --iters=4000000 --count=780000
  run_perf_3pass "S4 Churn4M (640K) folly::F14FastMap (Vector 81%)" "${CHALLENGE}" --mode=s4_churn_f14_fast --iters=4000000 --count=640000
  run_perf_3pass "S4 Churn4M (780K) boost::unordered_flat_map (Ofw)" "${CHALLENGE}" --mode=s4_churn_boost --iters=4000000 --count=780000
} 2>&1 | tee "${OUT_DIR}/09_pmu_perf_stat.txt"

echo ""
echo ">>> [7/7] Generating Annotated Intel-Syntax Disassembly (assembly/godbolt_comparison.s)..."
clang++ -std=c++20 -O3 -march=znver4 -S -masm=intel -fno-asynchronous-unwind-tables \
  -fno-dwarf2-cfi-asm "${EP_DIR}/assembly/godbolt_source.cc" -o "${OUT_DIR}/godbolt_clang.s"

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
  echo "# =============================================================================="
  echo "# EPISODE 16: PRODUCTION C++ HASH MAPS — INTEL-SYNTAX DISASSEMBLY (ZEN 4)"
  echo "# =============================================================================="
  echo ""
  echo "# --- 1. Self-Contained Godbolt Comparison (Clang 18 -O3 -march=znver4) ---"
  cat "${OUT_DIR}/godbolt_clang.s"
  echo ""
  echo "# --- 2. Live Compiled std::unordered_map U64 Lookup (GCC 13 -O3 -march=native) ---"
  llvm-objdump -d --disassemble-symbols=Ep16DisasmStdFindU64 -M intel "${CHALLENGE}" || true
  echo ""
  echo "# --- 3. Live Compiled absl::flat_hash_map U64 Lookup (16-way SwissTable) ---"
  llvm-objdump -d --disassemble-symbols=Ep16DisasmAbslFlatFindU64 -M intel "${CHALLENGE}" || true
  echo ""
  echo "# --- 4. Live Compiled folly::F14ValueMap U64 Lookup (14-way F14 Chunk) ---"
  llvm-objdump -d --disassemble-symbols=Ep16DisasmFollyF14ValueFindU64 -M intel "${CHALLENGE}" || true
  echo ""
  echo "# --- 5. Live Compiled folly::F14FastMap U64 Lookup (14-way F14VectorMap) ---"
  llvm-objdump -d --disassemble-symbols=Ep16DisasmFollyF14FastFindU64 -M intel "${CHALLENGE}" || true
  echo ""
  echo "# --- 6. Live Compiled boost::unordered_flat_map U64 Lookup (15-way group15 + ofw) ---"
  llvm-objdump -d --disassemble-symbols=Ep16DisasmBoostFlatFindU64 -M intel "${CHALLENGE}" || true
  echo ""
  echo "# --- 7. Live Compiled Story 1 Default vs Transparent Route Lookups ---"
  llvm-objdump -d --disassemble-symbols=Ep16StdDefaultFindRoute -M intel "${CHALLENGE}" || true
  llvm-objdump -d --disassemble-symbols=Ep16StdTransparentFindRoute -M intel "${CHALLENGE}" || true
} > "${EP_DIR}/assembly/godbolt_comparison.s"

echo ""
echo ">>> Episode 16 forensic telemetry complete! Outputs in ${OUT_DIR}/ and ${EP_DIR}/assembly/godbolt_comparison.s."

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
# Hot Path C++ — Episode 15: Hash Maps Across Languages Forensic Runner
# ==============================================================================
set -euo pipefail

EP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${EP_DIR}/../.." && pwd)"
BUILD_DIR="/tmp/ep15_build"
OUT_DIR="/tmp/ep15_telemetry"
mkdir -p "${BUILD_DIR}" "${OUT_DIR}" "${EP_DIR}/assembly"

echo "========================================================================"
echo "EPISODE 15: HASH MAPS ACROSS LANGUAGES FORENSIC TELEMETRY RUNNER"
echo "Host CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | xargs)"
echo "Kernel:   $(uname -srm)"
echo "GCC:      $(g++ --version | head -n 1)"
echo "Clang:    $(clang++ --version | head -n 1)"
echo "Rustc:    $(rustc --version)"
echo "Go Local: $(GOTOOLCHAIN=local go version)"
echo "Go 1.24:  $(GOTOOLCHAIN=go1.24.0 go version 2>/dev/null || echo 'unavailable')"
echo "Java:     $(java -version 2>&1 | head -n 1)"
echo "Python:   $(python3 --version)"
echo "========================================================================"

cd "${REPO_ROOT}"

echo ""
echo ">>> [1/7] Building C++20 & Java 21 Optimized Targets (-c opt) & Running Unit Tests..."
bazel test -c opt \
  //common/hash_maps:hash_maps_test \
  //episodes/high_performance/15_hash_maps_across_languages:hotpath_java_test \
  --test_output=errors

bazel build -c opt \
  //episodes/high_performance/15_hash_maps_across_languages:benchmark \
  //episodes/high_performance/15_hash_maps_across_languages:challenge \
  //episodes/high_performance/15_hash_maps_across_languages:live_hashdos_repro \
  //episodes/high_performance/15_hash_maps_across_languages:java_benchmarks \
  //episodes/high_performance/15_hash_maps_across_languages:java_challenge \
  //episodes/high_performance/15_hash_maps_across_languages:java_challenge_zgc

CPP_BENCH="${REPO_ROOT}/bazel-bin/episodes/high_performance/15_hash_maps_across_languages/benchmark"
CPP_CHALLENGE="${REPO_ROOT}/bazel-bin/episodes/high_performance/15_hash_maps_across_languages/challenge"
CPP_DOS="${REPO_ROOT}/bazel-bin/episodes/high_performance/15_hash_maps_across_languages/live_hashdos_repro"
JAVA_JMH="${REPO_ROOT}/bazel-bin/episodes/high_performance/15_hash_maps_across_languages/java_benchmarks"
JAVA_G1="${REPO_ROOT}/bazel-bin/episodes/high_performance/15_hash_maps_across_languages/java_challenge"
JAVA_ZGC="${REPO_ROOT}/bazel-bin/episodes/high_performance/15_hash_maps_across_languages/java_challenge_zgc"

echo ""
echo ">>> [2/7] Building Rust 1.91 & Go (1.22.2 + 1.24.0) Binaries..."
rustc --edition=2021 -C opt-level=3 -C target-cpu=native -C debuginfo=1 \
  "${EP_DIR}/rust_hash_map_bench.rs" -o "${BUILD_DIR}/bench_rust"
GOTOOLCHAIN=local go build -o "${BUILD_DIR}/bench_go122" "${EP_DIR}/go_hash_map_bench.go"
HAS_GO124=0
if GOTOOLCHAIN=go1.24.0 go build -o "${BUILD_DIR}/bench_go124" "${EP_DIR}/go_hash_map_bench.go" 2>/dev/null; then
  HAS_GO124=1
fi

echo ""
echo ">>> [3/7] Verifying Cross-Language Checksum Parity (--mode=test)..."
"${BUILD_DIR}/bench_rust" --mode=test
"${BUILD_DIR}/bench_go122" --mode=test
if [[ "${HAS_GO124}" -eq 1 ]]; then
  "${BUILD_DIR}/bench_go124" --mode=test
fi
python3 "${EP_DIR}/python_hash_map_bench.py" --mode=test

echo ""
echo ">>> [4/7] Running Full Polyglot Forensic Suites (Battles 1, 2, 3)..."
"${CPP_CHALLENGE}" --mode=all | tee "${OUT_DIR}/01_cpp20_forensic.txt"
{
  echo "=== C++20 Battle 2A Isolated VmRSS (Fresh Process Per Map) ==="
  "${CPP_CHALLENGE}" --mode=b2_rss_std_packed
  "${CPP_CHALLENGE}" --mode=b2_rss_absl_packed
  "${CPP_CHALLENGE}" --mode=b2_rss_std_session
  "${CPP_CHALLENGE}" --mode=b2_rss_absl_session
} | tee "${OUT_DIR}/01_cpp20_isolated_rss.txt"
"${CPP_DOS}" | tee "${OUT_DIR}/01_cpp20_live_hashdos.txt"
"${BUILD_DIR}/bench_rust" --mode=all | tee "${OUT_DIR}/02_rust191_forensic.txt"
"${BUILD_DIR}/bench_go122" --mode=all | tee "${OUT_DIR}/03_go122_forensic.txt"
{
  echo "=== Go 1.22.2 & 1.24.0 Isolated Footprint ==="
  "${BUILD_DIR}/bench_go122" --mode=b2_rss_packed
  "${BUILD_DIR}/bench_go122" --mode=b2_rss_session
  if [[ "${HAS_GO124}" -eq 1 ]]; then
    "${BUILD_DIR}/bench_go124" --mode=b2_rss_packed
    "${BUILD_DIR}/bench_go124" --mode=b2_rss_session
  fi
} | tee "${OUT_DIR}/03_go_isolated_rss.txt"
if [[ "${HAS_GO124}" -eq 1 ]]; then
  "${BUILD_DIR}/bench_go124" --mode=all | tee "${OUT_DIR}/03_go124_forensic.txt"
fi
"${JAVA_G1}" --mode=all | tee "${OUT_DIR}/04_java21_g1gc_forensic.txt"
"${JAVA_ZGC}" --mode=b2_tail | tee "${OUT_DIR}/04_java21_zgc_tail.txt"
python3 "${EP_DIR}/python_hash_map_bench.py" --mode=all | tee "${OUT_DIR}/05_python312_forensic.txt"
{
  echo "=== Python 3.12 Isolated VmRSS ==="
  python3 "${EP_DIR}/python_hash_map_bench.py" --mode=b2_rss_packed
  python3 "${EP_DIR}/python_hash_map_bench.py" --mode=b2_rss_session
} | tee "${OUT_DIR}/05_python312_isolated_rss.txt"

echo ""
echo ">>> [5/7] Running Google Benchmark (C++20) & OpenJDK JMH (Java 21)..."
"${CPP_BENCH}" --benchmark_min_time=0.25s | tee "${OUT_DIR}/06_cpp_google_benchmark.txt"
"${JAVA_JMH}" --no-color | tee "${OUT_DIR}/06_java_jmh_benchmark.txt"

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
  run_perf_3pass "C++20 std::unordered_map L1 1K" "${CPP_CHALLENGE}" --mode=b1_l1_std --iters=250
  run_perf_3pass "C++20 absl::flat_hash_map L1 1K" "${CPP_CHALLENGE}" --mode=b1_l1_absl --iters=250
  run_perf_3pass "Rust 1.91 SipHash L1 1K" "${BUILD_DIR}/bench_rust" --mode=b1_l1_sip --iters=250
  run_perf_3pass "Rust 1.91 FxHash L1 1K" "${BUILD_DIR}/bench_rust" --mode=b1_l1_fx --iters=250

  run_perf_3pass "C++20 std::unordered_map Aged 2M 80/20" "${CPP_CHALLENGE}" --mode=b1_aged_std --iters=40
  run_perf_3pass "C++20 absl::flat_hash_map Aged 2M 80/20" "${CPP_CHALLENGE}" --mode=b1_aged_absl --iters=40
  run_perf_3pass "Rust 1.91 SipHash Aged 2M 80/20" "${BUILD_DIR}/bench_rust" --mode=b1_aged_sip --iters=40
  run_perf_3pass "Rust 1.91 FxHash Aged 2M 80/20" "${BUILD_DIR}/bench_rust" --mode=b1_aged_fx --iters=40
  run_perf_3pass "Go 1.22.2 map Aged 2M 80/20" "${BUILD_DIR}/bench_go122" --mode=b1_aged_packed --iters=40
  if [[ "${HAS_GO124}" -eq 1 ]]; then
    run_perf_3pass "Go 1.24.0 SwissTable map Aged 2M 80/20" "${BUILD_DIR}/bench_go124" --mode=b1_aged_packed --iters=40
  fi
  run_perf_3pass "Java 21 HashMap<Long,Long> Aged 2M 80/20" "${JAVA_G1}" --mode=b1_aged_jdk --iters=40
  run_perf_3pass "Java 21 fastutil Long2Long Aged 2M 80/20" "${JAVA_G1}" --mode=b1_aged_fastutil --iters=40
  run_perf_3pass "Python 3.12 dict Aged 2M 80/20" python3 "${EP_DIR}/python_hash_map_bench.py" --mode=b1_aged_packed --iters=6

  run_perf_3pass "C++20 std::unordered_map Iter 2M Full" "${CPP_CHALLENGE}" --mode=b1_iter_full_std --iters=40
  run_perf_3pass "C++20 absl::flat_hash_map Iter 2M Full" "${CPP_CHALLENGE}" --mode=b1_iter_full_absl --iters=40
  run_perf_3pass "Rust 1.91 SipHash Iter 2M Full" "${BUILD_DIR}/bench_rust" --mode=b1_iter_full_sip --iters=40
  run_perf_3pass "Rust 1.91 FxHash Iter 2M Full" "${BUILD_DIR}/bench_rust" --mode=b1_iter_full_fx --iters=40
  run_perf_3pass "Go 1.22.2 map Iter 2M Full" "${BUILD_DIR}/bench_go122" --mode=b1_iter_full_packed --iters=40
  if [[ "${HAS_GO124}" -eq 1 ]]; then
    run_perf_3pass "Go 1.24.0 SwissTable map Iter 2M Full" "${BUILD_DIR}/bench_go124" --mode=b1_iter_full_packed --iters=40
  fi
  run_perf_3pass "Java 21 HashMap<Long,Long> Iter 2M Full" "${JAVA_G1}" --mode=b1_iter_full_jdk --iters=40
  run_perf_3pass "Java 21 fastutil Long2Long Iter 2M Full" "${JAVA_G1}" --mode=b1_iter_full_fastutil --iters=40
  run_perf_3pass "Python 3.12 dict Iter 2M Full" python3 "${EP_DIR}/python_hash_map_bench.py" --mode=b1_iter_full_packed --iters=10

  run_perf_3pass "C++20 std::unordered_map Sparse 200K" "${CPP_CHALLENGE}" --mode=b1_iter_sparse_std --iters=200
  run_perf_3pass "C++20 absl::flat_hash_map Sparse 200K" "${CPP_CHALLENGE}" --mode=b1_iter_sparse_absl --iters=200
  run_perf_3pass "Rust 1.91 SipHash Sparse 200K" "${BUILD_DIR}/bench_rust" --mode=b1_iter_sparse_sip --iters=200
  run_perf_3pass "Rust 1.91 FxHash Sparse 200K" "${BUILD_DIR}/bench_rust" --mode=b1_iter_sparse_fx --iters=200
  run_perf_3pass "Go 1.22.2 map Sparse 200K" "${BUILD_DIR}/bench_go122" --mode=b1_iter_sparse_packed --iters=200
  if [[ "${HAS_GO124}" -eq 1 ]]; then
    run_perf_3pass "Go 1.24.0 SwissTable map Sparse 200K" "${BUILD_DIR}/bench_go124" --mode=b1_iter_sparse_packed --iters=200
  fi
  run_perf_3pass "Java 21 HashMap<Long,Long> Sparse 200K" "${JAVA_G1}" --mode=b1_iter_sparse_jdk --iters=200
  run_perf_3pass "Java 21 fastutil Long2Long Sparse 200K" "${JAVA_G1}" --mode=b1_iter_sparse_fastutil --iters=200
  run_perf_3pass "Python 3.12 dict Sparse 200K" python3 "${EP_DIR}/python_hash_map_bench.py" --mode=b1_iter_sparse_packed --iters=40
} 2>&1 | tee "${OUT_DIR}/07_pmu_perf_stat.txt"

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
  echo "# EPISODE 15: HASH MAPS ACROSS LANGUAGES — INTEL-SYNTAX DISASSEMBLY"
  echo "# =============================================================================="
  echo ""
  echo "# --- 1. Godbolt Self-Contained Comparison (Clang 18 -O3 -march=znver4) ---"
  cat "${OUT_DIR}/godbolt_clang.s"
  echo ""
  echo "# --- 2. C++20 std::unordered_map Lookup Kernel (GCC 13 -O3 -march=native) ---"
  llvm-objdump -d --disassemble-symbols=ep15_std_unordered_find_packed -M intel "${CPP_CHALLENGE}" || true
  echo ""
  echo "# --- 3. C++20 absl::flat_hash_map Lookup Kernel (GCC 13 -O3 -march=native) ---"
  llvm-objdump -d --disassemble-symbols=ep15_absl_flat_find_packed -M intel "${CPP_CHALLENGE}" || true
  echo ""
  echo "# --- 4. Rust 1.91 SipHash-1-3 vs FxHash Lookup Kernels ---"
  llvm-objdump -d --disassemble-symbols=rust_siphash_find_packed -M intel "${BUILD_DIR}/bench_rust" || true
  llvm-objdump -d --disassemble-symbols=rust_fxhash_find_packed -M intel "${BUILD_DIR}/bench_rust" || true
  echo ""
  echo "# --- 5. Go 1.22.2 mapaccess2_fast64 (Classic 8-Slot Bucket) ---"
  GOTOOLCHAIN=local go tool objdump -s 'runtime\.mapaccess2_fast64' "${BUILD_DIR}/bench_go122" | head -n 90 || true
  if [[ "${HAS_GO124}" -eq 1 ]]; then
    echo ""
    echo "# --- 6. Go 1.24.0 SwissTable Map Lookup (runtime.mapaccess2_fast64) ---"
    GOTOOLCHAIN=go1.24.0 go tool objdump -s 'runtime\.mapaccess2_fast64' "${BUILD_DIR}/bench_go124" | head -n 125 || true
  fi
} > "${EP_DIR}/assembly/godbolt_comparison.s"

echo ""
echo ">>> Episode 15 forensic telemetry complete! Outputs in ${OUT_DIR}/ and ${EP_DIR}/assembly/godbolt_comparison.s."

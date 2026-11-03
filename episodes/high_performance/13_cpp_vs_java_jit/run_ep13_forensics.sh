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
# Hot Path C++ — Episode 13 Hardware Telemetry, JMH & perf stat Runner
# File: episodes/high_performance/13_cpp_vs_java_jit/run_ep13_forensics.sh
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OUT_DIR="${SCRIPT_DIR}/telemetry_out"
mkdir -p "${OUT_DIR}"

echo "======================================================================"
echo " 0. System, CPU, Compiler & Hermetic JDK 21 Environment"
echo "======================================================================"
{
  echo "=== Date ==="
  date -u
  echo "=== CPU Info ==="
  lscpu | grep -E "Model name|Socket|Thread|NUMA|CPU\(s\)|L1d|L1i|L2|L3" || true
  echo "=== C++ Compiler ==="
  g++ --version | head -n 2
  echo "=== Bazel Version ==="
  bazel --version
} | tee "${OUT_DIR}/00_sysinfo.txt"

echo ""
echo "======================================================================"
echo " 1. Building C++20 (Separate-TU & LTO) and Java 21 (JMH & Forensic)..."
echo "======================================================================"
cd "${REPO_ROOT}"
bazel build -c opt \
  //episodes/high_performance/13_cpp_vs_java_jit:benchmark \
  //episodes/high_performance/13_cpp_vs_java_jit:benchmark_lto \
  //episodes/high_performance/13_cpp_vs_java_jit:challenge \
  //episodes/high_performance/13_cpp_vs_java_jit:java_benchmarks \
  //episodes/high_performance/13_cpp_vs_java_jit:java_challenge \
  //episodes/high_performance/13_cpp_vs_java_jit:java_challenge_zgc \
  //episodes/high_performance/13_cpp_vs_java_jit:java_challenge_zgc_tight

BENCH_NO_LTO="${REPO_ROOT}/bazel-bin/episodes/high_performance/13_cpp_vs_java_jit/benchmark"
BENCH_LTO="${REPO_ROOT}/bazel-bin/episodes/high_performance/13_cpp_vs_java_jit/benchmark_lto"
CHALLENGE_CPP="${REPO_ROOT}/bazel-bin/episodes/high_performance/13_cpp_vs_java_jit/challenge"
JAVA_JMH_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/13_cpp_vs_java_jit/java_benchmarks"
JAVA_G1_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/13_cpp_vs_java_jit/java_challenge"
JAVA_ZGC_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/13_cpp_vs_java_jit/java_challenge_zgc"
JAVA_ZGC_TIGHT_BIN="${REPO_ROOT}/bazel-bin/episodes/high_performance/13_cpp_vs_java_jit/java_challenge_zgc_tight"

echo ""
echo "======================================================================"
echo " 2. BATTLE 1: Allocation (malloc vs TLAB) & Polymorphic Dispatch"
echo "======================================================================"
echo "--- [C++20 Separate-TU Build (-O3 -march=native, NO LTO)] ---"
"${BENCH_NO_LTO}" --benchmark_filter="BM_Battle1.*" --benchmark_min_time=0.25s \
  | tee "${OUT_DIR}/01_battle1_cpp_no_lto.txt"

echo ""
echo "--- [C++20 Whole-Program LTO Build (-O3 -march=native -flto)] ---"
"${BENCH_LTO}" --benchmark_filter="BM_Battle1.*" --benchmark_min_time=0.25s \
  | tee "${OUT_DIR}/01_battle1_cpp_lto.txt"

echo ""
echo "--- [Java 21 HotSpot C2 Tiered JIT (Forensic Harness)] ---"
"${JAVA_G1_BIN}" --mode=battle1 | tee "${OUT_DIR}/01_battle1_java_c2.txt"

echo ""
echo "--- [Java 21 OpenJDK JMH Benchmark Suite (Battle 1 & Battle 2)] ---"
"${JAVA_JMH_BIN}" | tee "${OUT_DIR}/01_java_jmh_suite.txt"

echo ""
echo "======================================================================"
echo " 3. BATTLE 2: Cache-Line Physics & Hardware PMU (perf stat)"
echo "======================================================================"
echo "--- [C++20 Battle 2 Benchmarks (4,000,000 Orders)] ---"
"${BENCH_LTO}" --benchmark_filter="BM_Battle2.*" --benchmark_min_time=0.25s \
  | tee "${OUT_DIR}/02_battle2_cpp.txt"

echo ""
echo "--- [Java 21 Battle 2 Benchmarks (4,000,000 Orders)] ---"
"${JAVA_G1_BIN}" --mode=battle2_fresh | tee "${OUT_DIR}/02_battle2_java_fresh.txt"
"${JAVA_G1_BIN}" --mode=battle2_churned | tee "${OUT_DIR}/02_battle2_java_churned.txt"
"${JAVA_G1_BIN}" --mode=battle2_soa | tee "${OUT_DIR}/02_battle2_java_soa.txt"

if command -v perf >/dev/null 2>&1; then
  PMU_L1="cycles,instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-loads"
  PMU_L3="cycles,instructions,cache-references,cache-misses,dTLB-load-misses"

  echo ""
  echo "--- [perf stat: C++20 Contiguous std::vector<Order> (32B AoS)] ---"
  perf stat -e "${PMU_L1}" \
    "${BENCH_LTO}" --benchmark_filter="BM_Battle2_StdVector_ContiguousAoS" \
    --benchmark_min_time=1.0s 2>&1 | tee "${OUT_DIR}/02_perf_cpp_vector_l1.txt"
  perf stat -e "${PMU_L3}" \
    "${BENCH_LTO}" --benchmark_filter="BM_Battle2_StdVector_ContiguousAoS" \
    --benchmark_min_time=1.0s 2>&1 | tee "${OUT_DIR}/02_perf_cpp_vector_l3.txt"

  echo ""
  echo "--- [perf stat: C++20 Flat SoA (OrderBookSoA, AVX2/AVX-512)] ---"
  perf stat -e "${PMU_L1}" \
    "${BENCH_LTO}" --benchmark_filter="BM_Battle2_SIMD_StructureOfArrays" \
    --benchmark_min_time=1.0s 2>&1 | tee "${OUT_DIR}/02_perf_cpp_soa_l1.txt"
  perf stat -e "${PMU_L3}" \
    "${BENCH_LTO}" --benchmark_filter="BM_Battle2_SIMD_StructureOfArrays" \
    --benchmark_min_time=1.0s 2>&1 | tee "${OUT_DIR}/02_perf_cpp_soa_l3.txt"

  echo ""
  echo "--- [perf stat: C++20 Pointer Array (Churned Heap Order*)] ---"
  perf stat -e "${PMU_L1}" \
    "${BENCH_LTO}" --benchmark_filter="BM_Battle2_PointerArray_ChurnedHeap" \
    --benchmark_min_time=1.0s 2>&1 | tee "${OUT_DIR}/02_perf_cpp_ptr_l1.txt"
  perf stat -e "${PMU_L3}" \
    "${BENCH_LTO}" --benchmark_filter="BM_Battle2_PointerArray_ChurnedHeap" \
    --benchmark_min_time=1.0s 2>&1 | tee "${OUT_DIR}/02_perf_cpp_ptr_l3.txt"

  echo ""
  echo "--- [perf stat: Java 21 ArrayList<OrderRecord> (Fresh Sequential TLAB)] ---"
  perf stat -e "${PMU_L1}" \
    "${JAVA_G1_BIN}" --mode=battle2_fresh 2>&1 | tee "${OUT_DIR}/02_perf_java_fresh_l1.txt"
  perf stat -e "${PMU_L3}" \
    "${JAVA_G1_BIN}" --mode=battle2_fresh 2>&1 | tee "${OUT_DIR}/02_perf_java_fresh_l3.txt"

  echo ""
  echo "--- [perf stat: Java 21 ArrayList<OrderRecord> (Post-Churn Fragmented Heap)] ---"
  perf stat -e "${PMU_L1}" \
    "${JAVA_G1_BIN}" --mode=battle2_churned 2>&1 | tee "${OUT_DIR}/02_perf_java_churned_l1.txt"
  perf stat -e "${PMU_L3}" \
    "${JAVA_G1_BIN}" --mode=battle2_churned 2>&1 | tee "${OUT_DIR}/02_perf_java_churned_l3.txt"

  echo ""
  echo "--- [perf stat: Java 21 Flat Primitive Arrays (long[] SoA)] ---"
  perf stat -e "${PMU_L1}" \
    "${JAVA_G1_BIN}" --mode=battle2_soa 2>&1 | tee "${OUT_DIR}/02_perf_java_soa_l1.txt"
  perf stat -e "${PMU_L3}" \
    "${JAVA_G1_BIN}" --mode=battle2_soa 2>&1 | tee "${OUT_DIR}/02_perf_java_soa_l3.txt"
fi

echo ""
echo "======================================================================"
echo " 4. BATTLE 3: Tail Latency (p50/p99/p99.99/Max) & C2 Uncommon Trap"
echo "======================================================================"
echo "--- [C++20 Pre-Allocated Slab Pool + Rare Event Dispatch] ---"
"${CHALLENGE_CPP}" battle3 | tee "${OUT_DIR}/03_battle3_cpp_slab.txt"

echo ""
echo "--- [Java 21 Default G1GC (-Xms4g -Xmx4g -XX:+UseG1GC)] ---"
"${JAVA_G1_BIN}" --mode=battle3 | tee "${OUT_DIR}/03_battle3_java_g1gc.txt"

echo ""
echo "--- [Java 21 Generational ZGC (-Xms4g -Xmx4g -XX:+UseZGC -XX:+ZGenerational)] ---"
"${JAVA_ZGC_BIN}" --mode=battle3 | tee "${OUT_DIR}/03_battle3_java_zgc_4g.txt"

echo ""
echo "--- [Java 21 Generational ZGC Under Tight Heap (-Xms1536m -Xmx1536m)] ---"
"${JAVA_ZGC_TIGHT_BIN}" --mode=battle3 | tee "${OUT_DIR}/03_battle3_java_zgc_tight.txt"

echo ""
echo "======================================================================"
echo " 5. Disassembly Extraction (C++ Separate-TU vs LTO)"
echo "======================================================================"
objdump -d -M intel -C "${BENCH_NO_LTO}" \
  | grep -A 35 "BM_Battle1_Cpp_CrossTuVirtual_NoAlloc(benchmark::State&)" \
  > "${OUT_DIR}/04_asm_cpp_no_lto.s" || true

objdump -d -M intel -C "${BENCH_LTO}" \
  | grep -A 35 "BM_Battle1_Cpp_CrossTuVirtual_NoAlloc(benchmark::State&)" \
  > "${OUT_DIR}/04_asm_cpp_lto.s" || true

echo "All Episode 13 forensic telemetry saved under: ${OUT_DIR}/"

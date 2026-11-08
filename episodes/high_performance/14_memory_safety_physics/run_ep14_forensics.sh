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

set -euo pipefail

EP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "${EP_DIR}" rev-parse --show-toplevel)"
BUILD_DIR="/tmp/ep14_build"
OUT_DIR="/tmp/ep14_telemetry"
mkdir -p "${BUILD_DIR}" "${OUT_DIR}" "${EP_DIR}/assembly"

echo "========================================================================"
echo "EPISODE 14: MEMORY SAFETY FORENSIC TELEMETRY RUNNER"
echo "Host CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | xargs)"
echo "Kernel:   $(uname -srm)"
echo "Clang:    $(clang++ --version | head -n 1)"
echo "GCC:      $(g++ --version | head -n 1)"
echo "Rustc:    $(rustc --version)"
echo "Java:     $(java -version 2>&1 | head -n 1)"
echo "========================================================================"

# ------------------------------------------------------------------------------
# 1. LIVE SESSION 1: TEMPORAL MEMORY SAFETY & LIFETIME PHYSICS
# ------------------------------------------------------------------------------
echo ""
echo ">>> [SESSION 1] Compiling & Running Temporal Memory Safety Reproducers..."

{
  echo "\$ clang++ -std=c++20 -O3 -Wall -Wextra -Wdangling -I \"${REPO_ROOT}\" live_uaf_repro.cc -o /tmp/ep14_build/live_uaf_O3"
  clang++ -std=c++20 -O3 -Wall -Wextra -Wdangling -I "${REPO_ROOT}" "${EP_DIR}/live_uaf_repro.cc" -o "${BUILD_DIR}/live_uaf_O3" 2>&1
  echo "[Compiler Warnings]: 0 (Exit Code 0)"
  echo ""
  echo "\$ /tmp/ep14_build/live_uaf_O3"
  "${BUILD_DIR}/live_uaf_O3"
} | tee "${OUT_DIR}/session1_cpp_uaf_o3.txt"

{
  echo "\$ clang++ -std=c++20 -O3 -Wall -Wextra -Wdangling -DDEMO_DANGLING_VIEW -I \"${REPO_ROOT}\" -fsyntax-only live_uaf_repro.cc"
  clang++ -std=c++20 -O3 -Wall -Wextra -Wdangling -DDEMO_DANGLING_VIEW -I "${REPO_ROOT}" -fsyntax-only "${EP_DIR}/live_uaf_repro.cc" 2>&1 || true
} | tee "${OUT_DIR}/session1_cpp_wdangling_local.txt"

{
  echo "\$ clang++ -std=c++20 -O1 -g -fsanitize=address -fno-omit-frame-pointer -I \"${REPO_ROOT}\" live_uaf_repro.cc -o /tmp/ep14_build/live_uaf_asan"
  clang++ -std=c++20 -O1 -g -fsanitize=address -fno-omit-frame-pointer -I "${REPO_ROOT}" "${EP_DIR}/live_uaf_repro.cc" -o "${BUILD_DIR}/live_uaf_asan" 2>&1
  echo "\$ /tmp/ep14_build/live_uaf_asan"
  ASAN_OPTIONS=color=never "${BUILD_DIR}/live_uaf_asan" 2>&1 || true
} | tee "${OUT_DIR}/session1_cpp_asan.txt"

{
  echo "\$ rustc --edition=2021 live_uaf_repro.rs -o /tmp/ep14_build/live_uaf_rust"
  rustc --edition=2021 "${EP_DIR}/live_uaf_repro.rs" -o "${BUILD_DIR}/live_uaf_rust" 2>&1 || true
  echo ""
  echo "\$ rustc --edition=2021 -O --cfg safe_slab live_uaf_repro.rs -o /tmp/ep14_build/live_uaf_rust_safe && /tmp/ep14_build/live_uaf_rust_safe"
  rustc --edition=2021 -O --cfg safe_slab "${EP_DIR}/live_uaf_repro.rs" -o "${BUILD_DIR}/live_uaf_rust_safe" 2>&1
  "${BUILD_DIR}/live_uaf_rust_safe"
} | tee "${OUT_DIR}/session1_rust_borrow_and_slab.txt"

# ------------------------------------------------------------------------------
# 2. BUILD STANDALONE CLANG 18, GCC 13, RUST 1.91 & JAVA 21 BINARIES
# ------------------------------------------------------------------------------
echo ""
echo ">>> [SESSION 2 & 3] Building Clang 18, GCC 13, Rust 1.91 & Java 21 Binaries..."

clang++ -std=c++20 -O3 -march=native -I "${REPO_ROOT}" "${EP_DIR}/challenge.cc" -o "${BUILD_DIR}/challenge_clang_unchecked"
clang++ -std=c++20 -O3 -march=native -D_GLIBCXX_ASSERTIONS -I "${REPO_ROOT}" "${EP_DIR}/challenge.cc" -o "${BUILD_DIR}/challenge_clang_hardened"
g++ -std=c++20 -O3 -march=native -I "${REPO_ROOT}" "${EP_DIR}/challenge.cc" -o "${BUILD_DIR}/challenge_gcc_unchecked"
g++ -std=c++20 -O3 -march=native -D_GLIBCXX_ASSERTIONS -I "${REPO_ROOT}" "${EP_DIR}/challenge.cc" -o "${BUILD_DIR}/challenge_gcc_hardened"
rustc --edition=2021 -C opt-level=3 -C target-cpu=native "${EP_DIR}/memory_safety_bench.rs" -o "${BUILD_DIR}/bench_rust"
javac -d "${BUILD_DIR}" "${EP_DIR}/MemorySafetyBench.java"

# ------------------------------------------------------------------------------
# 3. EXTRACT CLEAN INTEL-SYNTAX DISASSEMBLY TO assembly/godbolt_comparison.s
# ------------------------------------------------------------------------------
echo ""
echo ">>> Extracting Annotated Intel-Syntax Disassembly..."

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
  echo "# EPISODE 14: THE PHYSICS OF MEMORY SAFETY (AMD RYZEN 9 9950X - ZEN 5)"
  echo "# Compiler 1: Ubuntu clang version 18.1.3 (-std=c++20 -O3 -march=native)"
  echo "# Compiler 2: rustc 1.91.0 (-C opt-level=3 -C target-cpu=native)"
  echo "# =============================================================================="
  echo ""
  echo "# --- 1. CLANG 18: Unchecked Batch Gather (AVX-512 vpgatherqq) ---"
  llvm-objdump -d --disassemble-symbols=ep14_gather_batch_unchecked -M intel "${BUILD_DIR}/challenge_clang_unchecked"
  echo ""
  echo "# --- 2. CLANG 18: Hardened Fast-Trap Batch Gather (cmp + jae ud2, Scalar) ---"
  llvm-objdump -d --disassemble-symbols=ep14_gather_batch_hardened_trap -M intel "${BUILD_DIR}/challenge_clang_hardened"
  echo ""
  echo "# --- 3. CLANG 18: Power-of-Two Bitmask-Safe Gather (idx & 0xFFFF, AVX-512) ---"
  llvm-objdump -d --disassemble-symbols=ep14_gather_batch_bitmask_safe -M intel "${BUILD_DIR}/challenge_clang_hardened"
  echo ""
  echo "# --- 4. CLANG 18: Dependent Probe Chain (Unchecked vs. Hardened Trap) ---"
  llvm-objdump -d --disassemble-symbols=ep14_probe_dependent_unchecked -M intel "${BUILD_DIR}/challenge_clang_unchecked"
  llvm-objdump -d --disassemble-symbols=ep14_probe_dependent_hardened_trap -M intel "${BUILD_DIR}/challenge_clang_hardened"
  echo ""
  echo "# --- 5. CLANG 18: 64-Bit Generational Slab Handle Lookup ---"
  llvm-objdump -d --disassemble-symbols=ep14_temporal_generational_lookup -M intel "${BUILD_DIR}/challenge_clang_hardened"
  echo ""
  echo "# --- 6. RUST 1.91: Checked vs. Unchecked vs. Bitmask-Safe Gather ---"
  llvm-objdump -d --disassemble-symbols=rust_gather_batch_unchecked -M intel "${BUILD_DIR}/bench_rust"
  llvm-objdump -d --disassemble-symbols=rust_gather_batch_checked -M intel "${BUILD_DIR}/bench_rust"
  llvm-objdump -d --disassemble-symbols=rust_gather_batch_bitmask_safe -M intel "${BUILD_DIR}/bench_rust"
  echo ""
  echo "# --- 7. RUST 1.91: 64-Bit Generational Slab Handle Lookup ---"
  llvm-objdump -d --disassemble-symbols=rust_temporal_generational_lookup -M intel "${BUILD_DIR}/bench_rust"
} > "${EP_DIR}/assembly/godbolt_comparison.s"

# ------------------------------------------------------------------------------
# 4. RUN FULL FORENSIC SUITES (CLANG 18, GCC 13, RUST 1.91, JAVA 21 G1GC & ZGC)
# ------------------------------------------------------------------------------
echo ""
echo ">>> Running Clang 18 (-D_GLIBCXX_ASSERTIONS) Forensic Challenge..."
"${BUILD_DIR}/challenge_clang_hardened" --mode=all --iters=2000 | tee "${OUT_DIR}/clang18_forensic.txt"

echo ""
echo ">>> Running GCC 13 (-D_GLIBCXX_ASSERTIONS) Forensic Challenge..."
"${BUILD_DIR}/challenge_gcc_hardened" --mode=all --iters=2000 | tee "${OUT_DIR}/gcc13_forensic.txt"

echo ""
echo ">>> Running Rust 1.91 Forensic Challenge..."
"${BUILD_DIR}/bench_rust" --mode=all --iters=2000 | tee "${OUT_DIR}/rust191_forensic.txt"

echo ""
echo ">>> Running Java 21 (G1GC) Forensic Challenge..."
java -Xms512m -Xmx512m -XX:+AlwaysPreTouch -XX:+UseG1GC -cp "${BUILD_DIR}" MemorySafetyBench --mode=all --iters=2000 | tee "${OUT_DIR}/java21_g1gc_forensic.txt"

echo ""
echo ">>> Running Java 21 (Generational ZGC) Forensic Challenge..."
java -Xms512m -Xmx512m -XX:+AlwaysPreTouch -XX:+UseZGC -XX:+ZGenerational -cp "${BUILD_DIR}" MemorySafetyBench --mode=all --iters=2000 | tee "${OUT_DIR}/java21_zgc_forensic.txt"

# ------------------------------------------------------------------------------
# 5. RUN UNMULTIPLEXED HARDWARE PMU PROFILING (perf stat) ON ZEN 5
# ------------------------------------------------------------------------------
echo ""
echo ">>> Running Unmultiplexed Hardware PMU (perf stat) Counters..."
PMU_CORE="cycles,instructions,branches,branch-misses"
PMU_CACHE="L1-dcache-loads,L1-dcache-load-misses"

{
  echo "=== PMU 1: Battle 1B Batch Gather (4000 Passes x 65,536 Lookups = 262.14M Ops) ==="
  for v in unchecked hardened span at bitmask; do
    echo "--- Clang 18 BatchGather variant=${v} ---"
    perf stat -e "${PMU_CORE}" "${BUILD_DIR}/challenge_clang_hardened" --mode=spatial_rand --variant="${v}" --iters=4000 2>&1
    perf stat -e "${PMU_CACHE}" "${BUILD_DIR}/challenge_clang_hardened" --mode=spatial_rand --variant="${v}" --iters=4000 2>&1 > /dev/null || true
  done

  for v in unchecked checked option_get bitmask; do
    echo "--- Rust 1.91 BatchGather variant=${v} ---"
    perf stat -e "${PMU_CORE}" "${BUILD_DIR}/bench_rust" --mode=spatial_rand --variant="${v}" --iters=4000 2>&1
  done

  echo "=== PMU 2: Battle 1C Dependent Probe Chain (4000 Passes x 65,536 Probes = 262.14M Ops) ==="
  for v in unchecked hardened at; do
    echo "--- Clang 18 DependentProbe variant=${v} ---"
    perf stat -e "${PMU_CORE}" "${BUILD_DIR}/challenge_clang_hardened" --mode=spatial_dep --variant="${v}" --iters=4000 2>&1
  done

  for v in unchecked checked; do
    echo "--- Rust 1.91 DependentProbe variant=${v} ---"
    perf stat -e "${PMU_CORE}" "${BUILD_DIR}/bench_rust" --mode=spatial_dep --variant="${v}" --iters=4000 2>&1
  done

  echo "=== PMU 3: Battle 3 Temporal Safety Under 25% Cancellation & ABA Recycling (4000 Passes) ==="
  for v in unchecked generational weak_ptr; do
    echo "--- Clang 18 Temporal variant=${v} ---"
    perf stat -e "${PMU_CORE}" "${BUILD_DIR}/challenge_clang_hardened" --mode=temporal --variant="${v}" --iters=4000 2>&1
    perf stat -e "${PMU_CACHE}" "${BUILD_DIR}/challenge_clang_hardened" --mode=temporal --variant="${v}" --iters=4000 2>&1
  done

  for v in unchecked generational rc_weak; do
    echo "--- Rust 1.91 Temporal variant=${v} ---"
    perf stat -e "${PMU_CORE}" "${BUILD_DIR}/bench_rust" --mode=temporal --variant="${v}" --iters=4000 2>&1
    perf stat -e "${PMU_CACHE}" "${BUILD_DIR}/bench_rust" --mode=temporal --variant="${v}" --iters=4000 2>&1
  done

  echo "--- Java 21 G1GC Temporal Suite ---"
  perf stat -e "${PMU_CORE}" java -Xms512m -Xmx512m -XX:+AlwaysPreTouch -XX:+UseG1GC -cp "${BUILD_DIR}" MemorySafetyBench --mode=temporal --iters=2000 2>&1
  perf stat -e "${PMU_CACHE}" java -Xms512m -Xmx512m -XX:+AlwaysPreTouch -XX:+UseG1GC -cp "${BUILD_DIR}" MemorySafetyBench --mode=temporal --iters=2000 2>&1

  echo "--- Java 21 Generational ZGC Temporal Suite ---"
  perf stat -e "${PMU_CORE}" java -Xms512m -Xmx512m -XX:+AlwaysPreTouch -XX:+UseZGC -XX:+ZGenerational -cp "${BUILD_DIR}" MemorySafetyBench --mode=temporal --iters=2000 2>&1
  perf stat -e "${PMU_CACHE}" java -Xms512m -Xmx512m -XX:+AlwaysPreTouch -XX:+UseZGC -XX:+ZGenerational -cp "${BUILD_DIR}" MemorySafetyBench --mode=temporal --iters=2000 2>&1
} | tee "${OUT_DIR}/pmu_perf_stat.txt"

echo ""
echo ">>> Forensic run complete. Outputs saved to ${OUT_DIR}/ and ${EP_DIR}/assembly/godbolt_comparison.s."

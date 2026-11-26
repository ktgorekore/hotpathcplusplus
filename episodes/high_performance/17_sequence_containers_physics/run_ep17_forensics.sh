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
# Hot Path Engineering (@HotPathEng) — Episode 17 Forensic Telemetry Runner
# ==============================================================================
set -euo pipefail

EP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${EP_DIR}/../../.." && pwd)"
OUT_DIR="/tmp/ep17_telemetry"
mkdir -p "${OUT_DIR}" "${EP_DIR}/assembly"

cd "${REPO_ROOT}"

echo ">>> [1/7] Checking Google C++ Formatting & Running Unit Tests (GCC & Clang)..."
clang-format --dry-run --Werror -style=Google \
  "${REPO_ROOT}/common/sequence_containers/sequence_containers.h" \
  "${REPO_ROOT}/common/sequence_containers/sequence_containers_test.cc" \
  "${EP_DIR}/benchmark.cc" \
  "${EP_DIR}/challenge.cc" \
  "${EP_DIR}/live_alloc_audit.cc" \
  "${EP_DIR}/godbolt_snippets.cc"

bazel test -c opt //common/sequence_containers:sequence_containers_test --test_output=errors
CC=clang CXX=clang++ bazel test -c opt //common/sequence_containers:sequence_containers_test --test_output=errors

echo ">>> [2/7] Regenerating Dual-Compiler Intel-Syntax Disassembly (assembly/godbolt_comparison.s)..."
ASM_OUT="${EP_DIR}/assembly/godbolt_comparison.s"
cat <<'EOF' > "${ASM_OUT}"
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
# EPISODE 17: SEQUENCE CONTAINERS ON REAL HARDWARE — INTEL-SYNTAX DISASSEMBLY
# Live Compiler Explorer Permalink: https://godbolt.org/z/9e95Kc4hh
# Source: episodes/high_performance/17_sequence_containers_physics/godbolt_snippets.cc
#   1. ScanRuleVector (+64B stride) vs. ScanRuleList (mov rax, [rax])
#   2. IndexCompactRing (6 branchless insns) vs. IndexStdDeque (2-level chunk map)
#   3. PushSessionWithStdQueue (copy fallback) vs. PushSessionWithRingQueue (noexcept move)
# ==============================================================================

# --- 1. GCC 13.3.0 (-std=c++20 -O3 -march=native -masm=intel) ---
EOF
g++ -std=c++20 -O3 -march=native -S -masm=intel \
  "${EP_DIR}/godbolt_snippets.cc" -o - >> "${ASM_OUT}"
cat <<'EOF' >> "${ASM_OUT}"

# --- 2. Clang 18.1.3 (-std=c++20 -O3 -march=native -masm=intel) ---
EOF
clang++ -std=c++20 -O3 -march=native -S -masm=intel \
  "${EP_DIR}/godbolt_snippets.cc" -o - >> "${ASM_OUT}"

echo ">>> [3/7] Building & Running GCC Optimized Suites..."
PKG="//episodes/high_performance/17_sequence_containers_physics"
BIN_DIR="${REPO_ROOT}/bazel-bin/episodes/high_performance/17_sequence_containers_physics"
bazel build -c opt \
  "${PKG}:live_alloc_audit" \
  "${PKG}:challenge" \
  "${PKG}:benchmark" \
  "${PKG}:java_challenge"

"${BIN_DIR}/live_alloc_audit" 2>&1 | tee "${OUT_DIR}/01_alloc_audit.txt"
"${BIN_DIR}/challenge" --mode=story2a_rss 2>&1 | tee "${OUT_DIR}/02_gcc_story2a_rss.txt"
"${BIN_DIR}/challenge" --mode=story1a --iters=5 2>&1 | tee "${OUT_DIR}/03_gcc_story1a.txt"
"${BIN_DIR}/challenge" --mode=story1b --iters=5 2>&1 | tee "${OUT_DIR}/04_gcc_story1b.txt"
"${BIN_DIR}/challenge" --mode=story2b --iters=5 2>&1 | tee "${OUT_DIR}/05_gcc_story2b.txt"
"${BIN_DIR}/challenge" --mode=story3 --iters=5 2>&1 | tee "${OUT_DIR}/06_gcc_story3.txt"
"${BIN_DIR}/benchmark" --benchmark_min_time=0.2s 2>&1 | tee "${OUT_DIR}/07_gcc_gbench.txt"

echo ">>> [4/7] Running 3-Pass Unmultiplexed Linux perf stat PMU Profiling..."
CHALLENGE_GCC="${BIN_DIR}/challenge"
PMU_CORE="cycles,instructions,branches,branch-misses"
PMU_L1_TLB="L1-dcache-loads,L1-dcache-load-misses,dTLB-loads,dTLB-load-misses"
PMU_LLC="cache-references,cache-misses,page-faults"

run_perf_3pass() {
  local tag="$1"
  shift
  echo "=== perf stat [${tag}] ==="
  perf stat -e "${PMU_CORE}" -- "$@" > /dev/null
  perf stat -e "${PMU_L1_TLB}" -- "$@" > /dev/null
  perf stat -e "${PMU_LLC}" -- "$@" > /dev/null
}

{
  run_perf_3pass "Story 1A Aged Scan (N=16384): std::vector" "${CHALLENGE_GCC}" --mode=perf_s1_vec --n=16384 --iters=2
  run_perf_3pass "Story 1A Aged Scan (N=16384): std::list" "${CHALLENGE_GCC}" --mode=perf_s1_list --n=16384 --iters=2
  run_perf_3pass "Story 1B Dual Queue (16K Conns, 200K Ev): boost::intrusive::list" "${CHALLENGE_GCC}" --mode=perf_s1b_intrusive --iters=5
  run_perf_3pass "Story 1B Dual Queue (16K Conns, 200K Ev): std::vector<u32> O(1) idx+memmove" "${CHALLENGE_GCC}" --mode=perf_s1b_vecidx --iters=1
  run_perf_3pass "Story 1B Dual Queue (16K Conns, 200K Ev): std::vector<Conn*> find+rotate" "${CHALLENGE_GCC}" --mode=perf_s1b_vecptr --iters=1
  run_perf_3pass "Story 2B 1,024-Conn Mux FIFO (8M Msgs): std::deque" "${CHALLENGE_GCC}" --mode=perf_s2_deque --iters=2
  run_perf_3pass "Story 2B 1,024-Conn Mux FIFO (8M Msgs): CompactRingDeque" "${CHALLENGE_GCC}" --mode=perf_s2_ring --iters=2
  run_perf_3pass "Story 3 150K Sessions: std::vector + std::queue" "${CHALLENGE_GCC}" --mode=perf_s3_stdqueue --iters=2
  run_perf_3pass "Story 3 150K Sessions: std::vector + CompactRingDeque" "${CHALLENGE_GCC}" --mode=perf_s3_ring --iters=2
} 2>&1 | tee "${OUT_DIR}/08_pmu_perf_stat.txt"

echo ">>> [5/7] Building & Running Clang Optimized Suites..."
CC=clang CXX=clang++ bazel build -c opt "${PKG}:challenge"
"${BIN_DIR}/challenge" --mode=story1a --iters=5 2>&1 | tee "${OUT_DIR}/09_clang_story1a.txt"
"${BIN_DIR}/challenge" --mode=story1b --iters=5 2>&1 | tee "${OUT_DIR}/10_clang_story1b.txt"
"${BIN_DIR}/challenge" --mode=story2b --iters=5 2>&1 | tee "${OUT_DIR}/11_clang_story2b.txt"
"${BIN_DIR}/challenge" --mode=story3 --iters=5 2>&1 | tee "${OUT_DIR}/12_clang_story3.txt"

bazel build -c opt "${PKG}:benchmark" "${PKG}:challenge" "${PKG}:live_alloc_audit" "${PKG}:java_challenge"

echo ">>> [6/7] Running Rust 1.91 (-C opt-level=3 -C target-cpu=native) Parity Suite..."
rustc -C opt-level=3 -C target-cpu=native "${EP_DIR}/rust_sequence_bench.rs" -o "${OUT_DIR}/ep17_rust"
"${OUT_DIR}/ep17_rust" 2>&1 | tee "${OUT_DIR}/13_rust_opt3.txt"
rm -f "${OUT_DIR}/ep17_rust"

echo ">>> [7/7] Running OpenJDK 21 LTS Parity Suite..."
"${BIN_DIR}/java_challenge" 2>&1 | tee "${OUT_DIR}/14_java21.txt"

echo ">>> ALL EPISODE 17 FORENSIC TELEMETRY COMPLETE! Logs in ${OUT_DIR}/"

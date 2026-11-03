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

# ==============================================================================
# Episode 13: Is C++ Actually Faster Than Java?
# Annotated x86-64 Disassembly Comparison (AMD Ryzen 9 9950X Zen 5)
# Compiler (C++): GCC 13.3.0 (-std=c++20 -O3 -march=native [-flto])
# Runtime  (JVM): OpenJDK 21.0.12+8-LTS HotSpot Tier 4 C2 Compiler
# ==============================================================================

# ------------------------------------------------------------------------------
# 1. BATTLE 1A: C++20 Naive OOP (`malloc` + `free` + Cross-TU `vtable` Call)
#    Measured on Zen 5: 16.8 ns/op (No LTO) | 15.2 ns/op (-flto)
# ------------------------------------------------------------------------------
# Every iteration allocates a 32B `Event` via `operator new(32)` (`malloc`),
# dispatches `ProcessHeapAlloc()` through `vtable[2]` (`call QWORD PTR [rax+0x10]`),
# which allocates a 16B `OrderResult` via `operator new(16)`, and then calls
# `operator delete` twice (`~unique_ptr<OrderResult>` and `~unique_ptr<Event>`).
BM_Battle1_Cpp_NaiveOop_HeapAndVirtual_HotLoop:
    mov     esi, eax                        # tag (0 = LimitOrderEvent, 1 = CancelOrderEvent)
    mov     rdi, r13                        # sret slot for std::unique_ptr<Event>
    call    MakeEventByType                 # Cross-TU call -> operator new(32)
    mov     rsi, QWORD PTR [rsp+0x40]       # rsi = Event*
    mov     rax, QWORD PTR [rsi]            # Load vptr from Event header
    lea     rdi, [rsp+0x38]                 # sret slot for std::unique_ptr<OrderResult>
    call    QWORD PTR [rax+0x10]            # Indirect vtable call: ProcessHeapAlloc() -> operator new(16)
    mov     rdi, QWORD PTR [rsp+0x38]       # rdi = OrderResult*
    mov     rax, QWORD PTR [rdi]            # res->notional
    sub     rax, QWORD PTR [rdi+0x8]        # res->notional - res->fee_ticks
    mov     esi, 0x10                       # sizeof(OrderResult) = 16
    call    operator delete(void*, ulong)   # Free OrderResult on glibc heap
    mov     rdi, QWORD PTR [rsp+0x40]       # rdi = Event*
    mov     rax, QWORD PTR [rdi]            # Load vptr for virtual destructor ~Event()
    call    QWORD PTR [rax+0x8]             # Virtual ~Event() -> operator delete(void*, 32)

# ------------------------------------------------------------------------------
# 2. BATTLE 1B: C++20 Pre-Allocated Cross-TU `vtable` Loop (`ProcessNoAlloc`)
#    Measured on Zen 5: 1.52 ns/op (Separate-TU) | 1.32 ns/op (-flto)
# ------------------------------------------------------------------------------
# Even with `-flto`, C++ AOT compilation sees a ring buffer populated with
# multiple derived classes (`LimitOrderEvent` and `CancelOrderEvent`) and
# cannot speculatively inline across the `std::unique_ptr<Event>` array without
# Profile-Guided Optimization (`-fprofile-use`). It must emit an indirect call:
BM_Battle1_Cpp_CrossTuVirtual_NoAlloc_HotLoop:
.L_cpp_vtable_loop:
    mov     rax, r12                        # Loop counter i
    inc     r12                             # ++i
    and     eax, 0xfff                      # idx = i & 4095
    mov     rdi, QWORD PTR [r13+rax*8+0x0]  # rdi = ring[idx].get()
    mov     rax, QWORD PTR [rdi]            # Load vptr from object header
    call    QWORD PTR [rax+0x18]            # Indirect vtable call: ProcessNoAlloc()
    cmp     r12, rbx                        # Compare against state.iterations()
    jne     .L_cpp_vtable_loop

# ------------------------------------------------------------------------------
# 3. BATTLE 1C: Java 21 HotSpot C2 Tier 4 Monomorphic Devirtualization +
#    Escape Analysis & Scalar Replacement (`OrderEvent.runBattle1Step`)
#    Measured on Zen 5: 0.73 ns/op (Forensic) | 0.89 ns/op (JMH)
#    Fresh Event + Result Alloc in loop: 0.53 ns/op (Forensic) | 0.67 ns/op (JMH)
# ------------------------------------------------------------------------------
# C2 observes at Tier 3 that 100% of runtime receivers are `LimitOrderEvent`.
# 1. Speculative Class Guard: Compares the 32-bit Compressed Klass pointer at
#    `[rsi+0x8]` against `LimitOrderEvent`. On mismatch, jumps to an Uncommon Trap.
# 2. Inlining: `LimitOrderEvent.processHeapAlloc()` is inlined directly.
# 3. Escape Analysis & Scalar Replacement: `new OrderResult(gross, gross >> 10)`
#    does not escape `runBattle1Step` (`NoEscape`). C2 deletes the TLAB bump
#    allocation completely and keeps `notional` and `feeTicks` in `rax`/`rdx`!
HotSpot_C2_OrderEvent_runBattle1Step_Monomorphic:
    mov     r10d, DWORD PTR [rsi+0x8]       # Load 32-bit Compressed Klass Word
    cmp     r10d, 0x2000c140                # Speculative guard: is receiver LimitOrderEvent?
    jne     .L_c2_uncommon_trap_class_check # Rare path -> Deoptimize to Tier 0 interpreter!
    mov     rax, QWORD PTR [rsi+0x10]       # Load LimitOrderEvent.price
    imul    rax, QWORD PTR [rsi+0x18]       # gross = price * qty (NoEscape: no `new OrderResult`!)
    mov     rdx, rax
    sar     rdx, 0xa                        # feeTicks = gross >> 10
    sub     rax, rdx                        # return notional - feeTicks in rax

# ------------------------------------------------------------------------------
# 4. BATTLE 1D: C++20 Idiomatic Flat Value Struct (`FlatEvent::ProcessInline`)
#    Measured on Zen 5: 0.56 ns/op (Bimodal) | 0.57 ns/op (99.9% Monomorphic)
# ------------------------------------------------------------------------------
# No heap allocation, no `vptr`, no speculative trap risk.
BM_Battle1_Cpp_IdiomaticFlatValue_HotLoop:
    mov     rax, QWORD PTR [rbx+rdx*1]      # flat.price
    imul    rax, QWORD PTR [rbx+rdx*1+0x8]  # gross = price * qty
    mov     rcx, rax
    sar     rcx, 0xa                        # gross >> 10
    sub     rax, rcx                        # gross - (gross >> 10)

# ------------------------------------------------------------------------------
# 5. BATTLE 2: Cache-Line Physics — Pointer Chasing vs. AVX-512 Flat SoA
#    (4,000,000 Orders)
# ------------------------------------------------------------------------------
# A. Java Churned `ArrayList<OrderRecord>` (32.21 ms / scan, 49.4% LLC miss rate,
#    204.6M dTLB load misses across 40 scans) & C++ Churned `Order*` (26.6 ms):
#    Every element dereferences a heap pointer jumping across random 4 KiB pages.
#
# B. C++20 `OrderBookSoA` (1.56 ms / scan) & Java 21 `long[]` SoA (1.55 ms / scan):
#    Contiguous 64-byte cache lines stream through L1D/L2/L3 hardware prefetchers
#    and execute 512-bit AVX-512 SIMD instructions (8 orders per instruction):
BM_Battle2_SIMD_StructureOfArrays_AVX512_InnerLoop:
.L_avx512_soa_loop:
    vmovdqu64 zmm6, ZMMWORD PTR [r12+rax*1]     # Load 8 x int64_t prices (64B cache line)
    vpmullq   zmm0, zmm6, ZMMWORD PTR [rbx+rax*1] # Multiply 8 x (price * qty) in parallel
    vpandq    zmm0, zmm0, ZMMWORD PTR [r13+rax*1] # Apply 8 x active_mask (-1 or 0)
    add       rax, 0x40                         # Advance 64 bytes (8 orders)
    vpaddq    zmm1, zmm1, zmm0                  # Accumulate 8 x 64-bit notional lanes
    cmp       rax, 0x1e84800                    # 4,000,000 * 8 bytes = 32,000,000 bytes
    jne       .L_avx512_soa_loop

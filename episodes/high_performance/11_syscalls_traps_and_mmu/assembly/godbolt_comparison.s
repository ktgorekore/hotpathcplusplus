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

// Compiler: x86-64 GCC 13.3 (-std=c++20 -O3 -march=znver5 -masm=intel)

// 1. Ring 3 vDSO clock_gettime (16.3 ns — 0 Ring 0 syscalls! Reads [vvar] Seqlock + rdtsc)
hotpath::episode11::ReadClockMonotonicVdso():
  endbr64
  sub     rsp, 40
  mov     edi, 1                      ; CLOCK_MONOTONIC (1)
  vpxor   xmm0, xmm0, xmm0
  mov     rsi, rsp                    ; &ts on Ring 3 stack
  mov     rax, QWORD PTR fs:40
  mov     QWORD PTR 24[rsp], rax
  xor     eax, eax
  vmovdqa XMMWORD PTR [rsp], xmm0
  call    clock_gettime@PLT           ; Dispatches to __vdso_clock_gettime in [vdso] (Ring 3!)
  imul    rax, QWORD PTR [rsp], 1000000000
  add     rax, QWORD PTR 8[rsp]
  mov     rdx, QWORD PTR 24[rsp]
  sub     rdx, QWORD PTR fs:40
  jne     .L5
  add     rsp, 40
  ret
.L5:
  call    __stack_chk_fail@PLT

// 2. Forced Ring 0 Syscall Trap (37.6 ns tight loop / 65–81 ns effective with 48 KiB L1D working set)
hotpath::episode11::ReadTidRawSyscall():
  endbr64
  sub     rsp, 8
  mov     edi, 186                    ; SYS_gettid (186 on x86-64 Linux)
  xor     eax, eax
  call    syscall@PLT                 ; Executes `syscall` -> IA32_LSTAR -> swapgs -> Ring 0!
  add     rsp, 8
  ret

// 3. Cached Thread-Local Storage TID (0.19 ns — 196x faster than SYS_gettid!)
hotpath::episode11::ReadTidCachedTls():
  endbr64
  cmp     BYTE PTR fs:guard variable for hotpath::episode11::ReadTidCachedTls()::kCachedTid@tpoff, 0
  je      .L9                         ; Cold path: called once per thread birth
  mov     eax, DWORD PTR fs:hotpath::episode11::ReadTidCachedTls()::kCachedTid@tpoff ; Hot path: 1 Ring 3 FS-load!
  ret
.L9:
  sub     rsp, 8
  mov     edi, 186                    ; SYS_gettid (186) — executed only ONCE per thread
  xor     eax, eax
  call    syscall@PLT
  mov     BYTE PTR fs:guard variable for hotpath::episode11::ReadTidCachedTls()::kCachedTid@tpoff, 1
  mov     DWORD PTR fs:hotpath::episode11::ReadTidCachedTls()::kCachedTid@tpoff, eax
  add     rsp, 8
  ret

// 4. First-Touch Demand Paging Write Loop (198.4 us on unwired mmap [512 #PFs] vs 3.27 us pre-faulted!)
hotpath::episode11::Touch512Pages(unsigned char*, unsigned char):
  endbr64
  lea     rax, 2097152[rdi]           ; End of 2 MiB arena (512 x 4,096 B pages)
.L15:
  mov     BYTE PTR [rdi], sil         ; If PTE Present==0 or R/W==0 -> Hardware #PF Trap (IDT[14])!
  mov     BYTE PTR 4096[rdi], sil     ; Unrolled 2x 4 KiB page stores per loop iteration
  add     rdi, 8192
  cmp     rdi, rax
  jne     .L15
  ret

// 5. 512 MiB Order Book Slot Probe (15.2 ns [4 KiB, 93.4% dTLB miss] vs 8.66–9.79 ns [2 MiB HugePage, 0.27% dTLB miss])
hotpath::episode11::ProbeOrderBookSlot(hotpath::episode11::OrderSlot const*, unsigned int):
  endbr64
  mov     esi, esi                    ; Zero-extend 32-bit slot_idx to 64-bit
  sal     rsi, 6                      ; Multiply slot_idx by 64 bytes (sizeof(OrderSlot))
  mov     rax, QWORD PTR 8[rsi+rdi]   ; Load price_ticks (+8B) -> MMU translates virtual address via TLB / CR3!
  ret

// 6. Pre-Faulted & HugePage-Advised Order Book Arena Setup
hotpath::episode11::ConfigureHugePageOrderBook(void*, unsigned long):
  endbr64
  push    rbp
  push    rbx
  mov     edx, 14                     ; MADV_HUGEPAGE (14) -> Sets PD Entry Bit 7 (PS = 1, 2 MiB)
  mov     rbx, rdi
  sub     rsp, 8
  mov     rbp, rsi
  call    madvise@PLT
  add     rsp, 8
  mov     rsi, rbp
  mov     rdi, rbx
  pop     rbx
  pop     rbp
  jmp     mlock@PLT                   ; Locks pages in physical RAM (prevents swap / page eviction)

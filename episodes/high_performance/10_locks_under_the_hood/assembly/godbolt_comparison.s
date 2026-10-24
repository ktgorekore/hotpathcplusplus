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

// Compiler: x86-64 GCC 14.2 (-std=c++20 -O3 -masm=intel)

// 1. Naive 3-Line TAS Spinlock -> Emits locked `xchg` on EVERY failed spin!
LockNaiveTAS(std::atomic_flag&):
.L2:
  mov     eax, 1
  xchg    BYTE PTR [rdi], al          ; Implicit hardware LOCK prefix! Demands Modified (M) state!
  test    al, al
  jne     .L2                         ; Loops back to locked xchg on every failed spin
  ret

// 2. Hardware-Sympathetic TTAS Spinlock -> Spins in Shared (S) + x86 PAUSE!
LockTTASPause(std::atomic<bool>&):
.L8:
  mov     eax, 1
  xchg    BYTE PTR [rdi], al          ; Try once to acquire Modified (M) ownership
  test    al, al
  je      .L11                        ; Acquired! Return immediately
.L6:
  rep nop                             ; _mm_pause() (F3 90) -> Throttles ROB & yields ports to SMT!
  movzx   eax, BYTE PTR [rdi]         ; Plain MOV read in MESI Shared (S) state (0 RFO traffic!)
  test    al, al
  jne     .L6                         ; Keep spinning quietly in Shared (S) until lock == 0
  jmp     .L8                         ; Lock looks free! Retry atomic xchg
.L11:
  ret

// 3. 3-State Linux futex Fast/Slow Path (Inside std::mutex / pthread_mutex_lock)
LockRawFutex(std::atomic<unsigned int>&):
  xor     eax, eax                    ; Expected = 0 (Unlocked)
  mov     edx, 1                      ; Desired  = 1 (Locked, zero waiters)
  lock cmpxchg DWORD PTR [rdi], edx   ; Stage 1: Atomic 0 -> 1 in Ring 3 Userspace!
  jne     .L22                        ; Only jump to slow path if already locked!
  ret                                 ; Uncontended fast path finishes in ~13 ns (0 syscalls!)
.L22:
  push    rbx
  mov     rbx, rdi
  cmp     eax, 2
  je      .L15
  mov     eax, 2                      ; Stage 2: Leave State 2 Sticky Note ("Waiters Sleeping")
  xchg    DWORD PTR [rdi], eax
  test    eax, eax
  je      .L12
.L15:
  sub     rsp, 8
  xor     r9d, r9d
  xor     r8d, r8d
  mov     ecx, 2                      ; Expected val = 2 ("The Lost-Wakeup Shield")
  push    0
  mov     edx, 128                    ; FUTEX_WAIT_PRIVATE (128)
  mov     rsi, rbx                    ; uaddr = &state
  mov     edi, 202                    ; SYS_futex (202)
  xor     eax, eax
  call    syscall@PLT                 ; Trap into Ring 0 Linux Kernel wait queue
  mov     eax, 2
  xchg    DWORD PTR [rbx], eax
  pop     rdx
  pop     rcx
  test    eax, eax
  jne     .L15
.L12:
  pop     rbx
  ret

UnlockRawFutex(std::atomic<unsigned int>&):
  xor     eax, eax
  xchg    DWORD PTR [rdi], eax        ; Atomically swap state -> 0 (Unlocked)
  cmp     eax, 2                      ; Were any threads sleeping in the kernel (State 2)?
  je      .L29                        ; Only call SYS_futex if old state == 2!
  ret                                 ; Uncontended unlock returns in Ring 3 (0 syscalls!)
.L29:
  sub     rsp, 16
  mov     rsi, rdi                    ; uaddr = &state
  xor     r9d, r9d
  xor     r8d, r8d
  push    0
  mov     ecx, 1                      ; Wake 1 waiting thread
  mov     edx, 129                    ; FUTEX_WAKE_PRIVATE (129)
  xor     eax, eax
  mov     edi, 202                    ; SYS_futex (202)
  call    syscall@PLT
  add     rsp, 24
  ret

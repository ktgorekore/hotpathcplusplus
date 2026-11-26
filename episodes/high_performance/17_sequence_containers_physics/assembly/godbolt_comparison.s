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
	.file	"godbolt_snippets.cc"
	.intel_syntax noprefix
	.text
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"vector::_M_realloc_insert"
	.text
	.p2align 4
	.globl	_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_
	.type	_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_, @function
_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_:
.LFB2644:
	.cfi_startproc
	endbr64
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	mov	rbp, rsp
	.cfi_def_cfa_register 6
	push	r15
	push	r14
	push	r13
	push	r12
	push	rbx
	.cfi_offset 15, -24
	.cfi_offset 14, -32
	.cfi_offset 13, -40
	.cfi_offset 12, -48
	.cfi_offset 3, -56
	mov	r15, rdi
	mov	rbx, rsi
	and	rsp, -32
	sub	rsp, 32
	mov	r13, QWORD PTR 8[rdi]
	cmp	r13, QWORD PTR 16[rdi]
	je	.L2
	mov	rax, QWORD PTR [rsi]
	lea	rcx, 24[r13]
	mov	QWORD PTR 8[r13], rcx
	mov	rdx, QWORD PTR 8[rsi]
	mov	QWORD PTR 0[r13], rax
	lea	rax, 24[rsi]
	cmp	rdx, rax
	je	.L71
	mov	QWORD PTR 8[r13], rdx
	mov	rdx, QWORD PTR 24[rsi]
	mov	QWORD PTR 24[r13], rdx
.L67:
	mov	rdx, QWORD PTR 16[rbx]
.L10:
	mov	QWORD PTR 8[rbx], rax
	mov	rax, QWORD PTR 40[rbx]
	mov	QWORD PTR 16[r13], rdx
	mov	QWORD PTR 16[rbx], 0
	mov	BYTE PTR 24[rbx], 0
	mov	QWORD PTR 40[rbx], 0
	mov	QWORD PTR 40[r13], rax
	mov	rax, QWORD PTR 48[rbx]
	mov	QWORD PTR 48[rbx], 0
	mov	QWORD PTR 48[r13], rax
	mov	rax, QWORD PTR 56[rbx]
	mov	QWORD PTR 56[rbx], 0
	mov	QWORD PTR 56[r13], rax
	mov	rax, QWORD PTR 64[rbx]
	mov	QWORD PTR 64[rbx], 0
	mov	QWORD PTR 64[r13], rax
	mov	rax, QWORD PTR 72[rbx]
	mov	QWORD PTR 72[rbx], 0
	mov	QWORD PTR 72[r13], rax
	mov	rax, QWORD PTR 80[rbx]
	mov	QWORD PTR 80[rbx], 0
	mov	QWORD PTR 80[r13], rax
	mov	rax, QWORD PTR 88[rbx]
	mov	QWORD PTR 88[rbx], 0
	mov	QWORD PTR 88[r13], rax
	add	QWORD PTR 8[r15], 96
	lea	rsp, -40[rbp]
	pop	rbx
	pop	r12
	pop	r13
	pop	r14
	pop	r15
	pop	rbp
	.cfi_remember_state
	.cfi_def_cfa 7, 8
	ret
	.p2align 4
	.p2align 3
.L2:
	.cfi_restore_state
	mov	rcx, QWORD PTR [rdi]
	mov	r12, r13
	movabs	rdx, -6148914691236517205
	sub	r12, rcx
	mov	rax, r12
	sar	rax, 5
	imul	rax, rdx
	movabs	rdx, 96076792050570581
	cmp	rax, rdx
	je	.L72
	cmp	r13, rcx
	je	.L73
	lea	rdx, [rax+rax]
	cmp	rdx, rax
	jb	.L42
	test	rdx, rdx
	jne	.L74
	mov	edi, 96
	xor	r14d, r14d
	xor	edx, edx
.L17:
	mov	rsi, QWORD PTR [rbx]
	mov	r8, QWORD PTR 8[rbx]
	lea	rax, [rdx+r12]
	mov	r12, QWORD PTR 16[rbx]
	lea	r9, 24[rax]
	mov	QWORD PTR 8[rax], r9
	mov	QWORD PTR [rax], rsi
	lea	rsi, 24[rbx]
	cmp	r8, rsi
	je	.L75
	mov	QWORD PTR 8[rax], r8
	mov	r8, QWORD PTR 24[rbx]
	mov	QWORD PTR 24[rax], r8
.L25:
	vmovdqu	xmm3, XMMWORD PTR 72[rbx]
	vmovdqu	ymm2, YMMWORD PTR 40[rbx]
	mov	QWORD PTR 8[rbx], rsi
	mov	rsi, QWORD PTR 88[rbx]
	vpxor	xmm0, xmm0, xmm0
	mov	QWORD PTR 16[rax], r12
	mov	QWORD PTR 16[rbx], 0
	mov	BYTE PTR 24[rbx], 0
	vmovdqu	YMMWORD PTR 40[rbx], ymm0
	mov	QWORD PTR 72[rbx], 0
	mov	QWORD PTR 80[rbx], 0
	mov	QWORD PTR 88[rbx], 0
	mov	QWORD PTR 88[rax], rsi
	vmovdqu	YMMWORD PTR 40[rax], ymm2
	vmovdqu	XMMWORD PTR 72[rax], xmm3
	cmp	r13, rcx
	je	.L64
	lea	rbx, 24[rcx]
	mov	QWORD PTR 16[rsp], rdx
	mov	QWORD PTR 8[rsp], rcx
	mov	rax, rbx
	mov	rbx, rdx
	mov	r12, rax
	vzeroupper
	jmp	.L39
	.p2align 4
	.p2align 3
.L27:
	mov	QWORD PTR 8[rbx], rcx
	mov	rcx, QWORD PTR [r12]
	mov	QWORD PTR 24[rbx], rcx
.L68:
	mov	rcx, QWORD PTR -8[r12]
.L34:
	mov	QWORD PTR 16[rbx], rcx
	mov	rcx, QWORD PTR 16[r12]
	mov	QWORD PTR -16[r12], r12
	mov	QWORD PTR -8[r12], 0
	mov	BYTE PTR [r12], 0
	mov	QWORD PTR 16[r12], 0
	mov	rdi, QWORD PTR -16[r12]
	mov	QWORD PTR 40[rbx], rcx
	mov	rcx, QWORD PTR 24[r12]
	mov	QWORD PTR 24[r12], 0
	mov	QWORD PTR 48[rbx], rcx
	mov	rcx, QWORD PTR 32[r12]
	mov	QWORD PTR 32[r12], 0
	mov	QWORD PTR 56[rbx], rcx
	mov	rcx, QWORD PTR 40[r12]
	mov	QWORD PTR 40[r12], 0
	mov	QWORD PTR 64[rbx], rcx
	mov	rcx, QWORD PTR 48[r12]
	mov	QWORD PTR 48[r12], 0
	mov	QWORD PTR 72[rbx], rcx
	mov	rcx, QWORD PTR 56[r12]
	mov	QWORD PTR 56[r12], 0
	mov	QWORD PTR 80[rbx], rcx
	mov	rcx, QWORD PTR 64[r12]
	mov	QWORD PTR 64[r12], 0
	mov	QWORD PTR 88[rbx], rcx
	cmp	rdi, r12
	je	.L36
	mov	rax, QWORD PTR [r12]
	lea	rsi, 1[rax]
	call	_ZdlPvm@PLT
.L36:
	lea	rax, 72[r12]
	add	rbx, 96
	lea	rcx, 96[r12]
	cmp	r13, rax
	je	.L63
	mov	r12, rcx
.L39:
	mov	rcx, QWORD PTR -24[r12]
	lea	rsi, 24[rbx]
	mov	QWORD PTR 8[rbx], rsi
	mov	QWORD PTR [rbx], rcx
	mov	rcx, QWORD PTR -16[r12]
	cmp	rcx, r12
	jne	.L27
	mov	rcx, QWORD PTR -8[r12]
	lea	rdi, 1[rcx]
	cmp	edi, 8
	jnb	.L28
	test	dil, 4
	jne	.L76
	test	edi, edi
	je	.L34
	movzx	ecx, BYTE PTR [r12]
	mov	BYTE PTR [rsi], cl
	test	dil, 2
	je	.L68
	mov	edi, edi
	movzx	ecx, WORD PTR -2[r12+rdi]
	mov	WORD PTR -2[rsi+rdi], cx
	mov	rcx, QWORD PTR -8[r12]
	jmp	.L34
	.p2align 4
	.p2align 3
.L73:
	add	rax, 1
	jc	.L42
	movabs	rdx, 96076792050570581
	cmp	rax, rdx
	cmova	rax, rdx
	lea	r14, [rax+rax*2]
	sal	r14, 5
.L16:
	mov	QWORD PTR 16[rsp], rcx
	mov	rdi, r14
	call	_Znwm@PLT
	mov	rcx, QWORD PTR 16[rsp]
	mov	rdx, rax
	add	r14, rax
	lea	rdi, 96[rax]
	jmp	.L17
	.p2align 4
	.p2align 3
.L63:
	mov	rdx, QWORD PTR 16[rsp]
	mov	rcx, QWORD PTR 8[rsp]
	lea	rdi, 96[rbx]
.L26:
	vmovq	xmm1, rdx
	vpinsrq	xmm0, xmm1, rdi, 1
	test	rcx, rcx
	je	.L40
	mov	rsi, QWORD PTR 16[r15]
	mov	rdi, rcx
	vmovdqa	XMMWORD PTR 16[rsp], xmm0
	sub	rsi, rcx
	call	_ZdlPvm@PLT
	vmovdqa	xmm0, XMMWORD PTR 16[rsp]
.L40:
	vmovdqu	XMMWORD PTR [r15], xmm0
	mov	QWORD PTR 16[r15], r14
	lea	rsp, -40[rbp]
	pop	rbx
	pop	r12
	pop	r13
	pop	r14
	pop	r15
	pop	rbp
	.cfi_remember_state
	.cfi_def_cfa 7, 8
	ret
	.p2align 4
	.p2align 3
.L28:
	.cfi_restore_state
	mov	rcx, QWORD PTR [r12]
	mov	r9, r12
	mov	QWORD PTR [rsi], rcx
	mov	ecx, edi
	mov	r8, QWORD PTR -8[r12+rcx]
	mov	QWORD PTR -8[rsi+rcx], r8
	lea	r8, 32[rbx]
	and	r8, -8
	sub	rsi, r8
	add	edi, esi
	sub	r9, rsi
	and	edi, -8
	cmp	edi, 8
	jb	.L68
	and	edi, -8
	xor	ecx, ecx
.L32:
	mov	esi, ecx
	add	ecx, 8
	mov	r10, QWORD PTR [r9+rsi]
	mov	QWORD PTR [r8+rsi], r10
	cmp	ecx, edi
	jb	.L32
	jmp	.L68
	.p2align 4
	.p2align 3
.L71:
	mov	rdx, QWORD PTR 16[rsi]
	lea	rsi, 1[rdx]
	cmp	esi, 8
	jnb	.L4
	test	sil, 4
	jne	.L77
	test	esi, esi
	je	.L10
	movzx	edx, BYTE PTR 24[rbx]
	mov	BYTE PTR 24[r13], dl
	test	sil, 2
	je	.L67
	mov	esi, esi
	movzx	edx, WORD PTR -2[rax+rsi]
	mov	WORD PTR -2[rcx+rsi], dx
	mov	rdx, QWORD PTR 16[rbx]
	jmp	.L10
	.p2align 4
	.p2align 3
.L42:
	movabs	r14, 9223372036854775776
	jmp	.L16
	.p2align 4
	.p2align 3
.L4:
	mov	rdx, QWORD PTR 24[rbx]
	mov	r9, rax
	mov	QWORD PTR 24[r13], rdx
	mov	edx, esi
	mov	rdi, QWORD PTR -8[rax+rdx]
	mov	QWORD PTR -8[rcx+rdx], rdi
	lea	rdi, 32[r13]
	and	rdi, -8
	sub	rcx, rdi
	add	esi, ecx
	sub	r9, rcx
	and	esi, -8
	cmp	esi, 8
	jb	.L67
	and	esi, -8
	xor	edx, edx
.L8:
	mov	ecx, edx
	add	edx, 8
	mov	r8, QWORD PTR [r9+rcx]
	mov	QWORD PTR [rdi+rcx], r8
	cmp	edx, esi
	jb	.L8
	jmp	.L67
	.p2align 4
	.p2align 3
.L75:
	lea	r8, 1[r12]
	cmp	r8d, 8
	jnb	.L19
	test	r8b, 4
	jne	.L78
	test	r8d, r8d
	je	.L25
	movzx	r10d, BYTE PTR 24[rbx]
	mov	BYTE PTR 24[rax], r10b
	test	r8b, 2
	je	.L25
	mov	r8d, r8d
	movzx	r10d, WORD PTR -2[rsi+r8]
	mov	WORD PTR -2[r9+r8], r10w
	jmp	.L25
	.p2align 4
	.p2align 3
.L19:
	mov	r10, QWORD PTR 24[rbx]
	mov	QWORD PTR 24[rax], r10
	mov	r10d, r8d
	mov	r11, QWORD PTR -8[rsi+r10]
	mov	QWORD PTR -8[r9+r10], r11
	lea	r10, 32[rax]
	mov	r11, rsi
	and	r10, -8
	sub	r9, r10
	add	r8d, r9d
	sub	r11, r9
	and	r8d, -8
	cmp	r8d, 8
	jb	.L25
	and	r8d, -8
	mov	QWORD PTR 16[rsp], rbx
	xor	r9d, r9d
	mov	rbx, rdi
	vmovq	xmm4, rdx
	mov	rdi, rsi
	mov	rsi, rax
	mov	eax, r8d
.L23:
	mov	r8d, r9d
	add	r9d, 8
	mov	rdx, QWORD PTR [r11+r8]
	mov	QWORD PTR [r10+r8], rdx
	cmp	r9d, eax
	jb	.L23
	mov	rax, rsi
	vmovq	rdx, xmm4
	mov	rsi, rdi
	mov	rdi, rbx
	mov	rbx, QWORD PTR 16[rsp]
	jmp	.L25
	.p2align 4
	.p2align 3
.L64:
	vzeroupper
	jmp	.L26
.L76:
	mov	ecx, DWORD PTR [r12]
	mov	edi, edi
	mov	DWORD PTR [rsi], ecx
	mov	ecx, DWORD PTR -4[r12+rdi]
	mov	DWORD PTR -4[rsi+rdi], ecx
	mov	rcx, QWORD PTR -8[r12]
	jmp	.L34
.L77:
	mov	edx, DWORD PTR 24[rbx]
	mov	esi, esi
	mov	DWORD PTR 24[r13], edx
	mov	edx, DWORD PTR -4[rax+rsi]
	mov	DWORD PTR -4[rcx+rsi], edx
	mov	rdx, QWORD PTR 16[rbx]
	jmp	.L10
.L78:
	mov	r10d, DWORD PTR 24[rbx]
	mov	r8d, r8d
	mov	DWORD PTR 24[rax], r10d
	mov	r10d, DWORD PTR -4[rsi+r8]
	mov	DWORD PTR -4[r9+r8], r10d
	jmp	.L25
.L74:
	movabs	rax, 96076792050570581
	cmp	rdx, rax
	cmovbe	rax, rdx
	imul	r14, rax, 96
	jmp	.L16
.L72:
	lea	rdi, .LC0[rip]
	call	_ZSt20__throw_length_errorPKc@PLT
	.cfi_endproc
.LFE2644:
	.size	_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_, .-_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_
	.p2align 4
	.globl	_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm
	.type	_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm, @function
_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm:
.LFB2645:
	.cfi_startproc
	endbr64
	mov	rdx, QWORD PTR 16[rdi]
	mov	rcx, QWORD PTR 40[rdi]
	mov	rax, rdx
	sub	rax, QWORD PTR 24[rdi]
	sar	rax, 3
	add	rax, rsi
	js	.L80
	cmp	rax, 63
	jle	.L84
	mov	rdx, rax
	sar	rdx, 6
.L83:
	mov	rsi, rdx
	mov	rdx, QWORD PTR [rcx+rdx*8]
	sal	rsi, 6
	sub	rax, rsi
	lea	rax, [rdx+rax*8]
	mov	rax, QWORD PTR [rax]
	ret
	.p2align 4
	.p2align 3
.L84:
	lea	rax, [rdx+rsi*8]
	mov	rax, QWORD PTR [rax]
	ret
	.p2align 4
	.p2align 3
.L80:
	mov	rdx, rax
	not	rdx
	shr	rdx, 6
	not	rdx
	jmp	.L83
	.cfi_endproc
.LFE2645:
	.size	_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm, .-_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm
	.p2align 4
	.globl	_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm
	.type	_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm, @function
_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm:
.LFB2646:
	.cfi_startproc
	endbr64
	mov	rax, QWORD PTR 8[rdi]
	add	rsi, QWORD PTR 16[rdi]
	dec	rax
	and	rsi, rax
	mov	rax, QWORD PTR [rdi]
	mov	rax, QWORD PTR [rax+rsi*8]
	ret
	.cfi_endproc
.LFE2646:
	.size	_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm, .-_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm
	.p2align 4
	.globl	_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE
	.type	_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE, @function
_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE:
.LFB2647:
	.cfi_startproc
	endbr64
	mov	rax, QWORD PTR [rdi]
	mov	rsi, QWORD PTR 8[rdi]
	xor	ecx, ecx
	cmp	rsi, rax
	je	.L86
	.p2align 4
	.p2align 3
.L88:
	mov	edx, DWORD PTR 16[rax]
	add	rax, 64
	sub	edx, DWORD PTR -44[rax]
	imul	rdx, QWORD PTR -56[rax]
	add	rcx, rdx
	cmp	rsi, rax
	jne	.L88
.L86:
	mov	rax, rcx
	ret
	.cfi_endproc
.LFE2647:
	.size	_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE, .-_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE
	.p2align 4
	.globl	_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE
	.type	_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE, @function
_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE:
.LFB2652:
	.cfi_startproc
	endbr64
	mov	rax, QWORD PTR [rdi]
	xor	ecx, ecx
	cmp	rdi, rax
	je	.L91
	.p2align 4
	.p2align 3
.L93:
	mov	edx, DWORD PTR 80[rax]
	sub	edx, DWORD PTR 84[rax]
	imul	rdx, QWORD PTR 72[rax]
	mov	rax, QWORD PTR [rax]
	add	rcx, rdx
	cmp	rdi, rax
	jne	.L93
.L91:
	mov	rax, rcx
	ret
	.cfi_endproc
.LFE2652:
	.size	_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE, .-_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE
	.section	.text._ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv,"axG",@progbits,_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv,comdat
	.align 2
	.p2align 4
	.weak	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv
	.type	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv, @function
_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv:
.LFB2984:
	.cfi_startproc
	endbr64
	mov	rax, QWORD PTR [rdi]
	lea	rdx, 16[rdi]
	cmp	rax, rdx
	je	.L98
	mov	rsi, QWORD PTR 16[rdi]
	mov	rdi, rax
	inc	rsi
	jmp	_ZdlPvm@PLT
	.p2align 4
	.p2align 3
.L98:
	ret
	.cfi_endproc
.LFE2984:
	.size	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv, .-_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv
	.section	.text._ZNSt6vectorISt4byteSaIS0_EED2Ev,"axG",@progbits,_ZNSt6vectorISt4byteSaIS0_EED5Ev,comdat
	.align 2
	.p2align 4
	.weak	_ZNSt6vectorISt4byteSaIS0_EED2Ev
	.type	_ZNSt6vectorISt4byteSaIS0_EED2Ev, @function
_ZNSt6vectorISt4byteSaIS0_EED2Ev:
.LFB3368:
	.cfi_startproc
	endbr64
	mov	rax, QWORD PTR [rdi]
	test	rax, rax
	je	.L101
	mov	rsi, QWORD PTR 16[rdi]
	mov	rdi, rax
	sub	rsi, rax
	jmp	_ZdlPvm@PLT
	.p2align 4
	.p2align 3
.L101:
	ret
	.cfi_endproc
.LFE3368:
	.size	_ZNSt6vectorISt4byteSaIS0_EED2Ev, .-_ZNSt6vectorISt4byteSaIS0_EED2Ev
	.weak	_ZNSt6vectorISt4byteSaIS0_EED1Ev
	.set	_ZNSt6vectorISt4byteSaIS0_EED1Ev,_ZNSt6vectorISt4byteSaIS0_EED2Ev
	.section	.text._ZN7hotpath9episode177godbolt19SessionWithStdQueueD2Ev,"axG",@progbits,_ZN7hotpath9episode177godbolt19SessionWithStdQueueD5Ev,comdat
	.align 2
	.p2align 4
	.weak	_ZN7hotpath9episode177godbolt19SessionWithStdQueueD2Ev
	.type	_ZN7hotpath9episode177godbolt19SessionWithStdQueueD2Ev, @function
_ZN7hotpath9episode177godbolt19SessionWithStdQueueD2Ev:
.LFB3391:
	.cfi_startproc
	endbr64
	push	r12
	.cfi_def_cfa_offset 16
	.cfi_offset 12, -16
	push	rbp
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
	mov	rbp, rdi
	push	rbx
	.cfi_def_cfa_offset 32
	.cfi_offset 3, -32
	mov	rdi, QWORD PTR 64[rdi]
	test	rdi, rdi
	je	.L103
	mov	rax, QWORD PTR 136[rbp]
	mov	rbx, QWORD PTR 104[rbp]
	lea	r12, 8[rax]
	cmp	rbx, r12
	jnb	.L104
	.p2align 4
	.p2align 3
.L105:
	mov	rdi, QWORD PTR [rbx]
	mov	esi, 512
	add	rbx, 8
	call	_ZdlPvm@PLT
	cmp	rbx, r12
	jb	.L105
	mov	rdi, QWORD PTR 64[rbp]
.L104:
	mov	rax, QWORD PTR 72[rbp]
	lea	rsi, 0[0+rax*8]
	call	_ZdlPvm@PLT
.L103:
	mov	rdi, QWORD PTR 40[rbp]
	test	rdi, rdi
	je	.L106
	mov	rsi, QWORD PTR 56[rbp]
	sub	rsi, rdi
	call	_ZdlPvm@PLT
.L106:
	mov	rdi, QWORD PTR 8[rbp]
	lea	rax, 24[rbp]
	cmp	rdi, rax
	je	.L115
	mov	rsi, QWORD PTR 24[rbp]
	pop	rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 24
	pop	rbp
	.cfi_def_cfa_offset 16
	pop	r12
	.cfi_def_cfa_offset 8
	inc	rsi
	jmp	_ZdlPvm@PLT
	.p2align 4
	.p2align 3
.L115:
	.cfi_restore_state
	pop	rbx
	.cfi_def_cfa_offset 24
	pop	rbp
	.cfi_def_cfa_offset 16
	pop	r12
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE3391:
	.size	_ZN7hotpath9episode177godbolt19SessionWithStdQueueD2Ev, .-_ZN7hotpath9episode177godbolt19SessionWithStdQueueD2Ev
	.weak	_ZN7hotpath9episode177godbolt19SessionWithStdQueueD1Ev
	.set	_ZN7hotpath9episode177godbolt19SessionWithStdQueueD1Ev,_ZN7hotpath9episode177godbolt19SessionWithStdQueueD2Ev
	.section	.text._ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,"axG",@progbits,_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,comdat
	.align 2
	.p2align 4
	.weak	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
	.type	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm, @function
_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm:
.LFB3441:
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDA3441
	endbr64
	mov	rax, rsi
	push	r14
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -16
	push	r13
	.cfi_def_cfa_offset 24
	.cfi_offset 13, -24
	push	r12
	.cfi_def_cfa_offset 32
	.cfi_offset 12, -32
	shr	rax, 6
	push	rbp
	.cfi_def_cfa_offset 40
	.cfi_offset 6, -40
	push	rbx
	.cfi_def_cfa_offset 48
	.cfi_offset 3, -48
	lea	rbx, 1[rax]
	add	rax, 3
	mov	rbp, rdi
	mov	r12, rsi
	cmp	rax, 8
	ja	.L134
	mov	eax, 8
	mov	edi, 64
.L118:
	mov	QWORD PTR 8[rbp], rax
.LEHB0:
	call	_Znwm@PLT
.LEHE0:
	mov	rdx, QWORD PTR 8[rbp]
	mov	QWORD PTR 0[rbp], rax
	sub	rdx, rbx
	shr	rdx
	lea	r14, [rax+rdx*8]
	lea	r13, [r14+rbx*8]
	cmp	r14, r13
	jnb	.L119
	mov	rbx, r14
	.p2align 4
	.p2align 3
.L120:
	mov	edi, 512
.LEHB1:
	call	_Znwm@PLT
.LEHE1:
	mov	QWORD PTR [rbx], rax
	add	rbx, 8
	cmp	rbx, r13
	jb	.L120
.L119:
	mov	rdx, QWORD PTR [r14]
	and	r12d, 63
	mov	QWORD PTR 40[rbp], r14
	lea	rax, 512[rdx]
	mov	QWORD PTR 24[rbp], rdx
	mov	QWORD PTR 16[rbp], rdx
	mov	QWORD PTR 32[rbp], rax
	lea	rax, -8[r13]
	mov	QWORD PTR 72[rbp], rax
	mov	rax, QWORD PTR -8[r13]
	mov	QWORD PTR 56[rbp], rax
	lea	rcx, 512[rax]
	lea	rax, [rax+r12*8]
	mov	QWORD PTR 64[rbp], rcx
	mov	QWORD PTR 48[rbp], rax
	pop	rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 40
	pop	rbp
	.cfi_def_cfa_offset 32
	pop	r12
	.cfi_def_cfa_offset 24
	pop	r13
	.cfi_def_cfa_offset 16
	pop	r14
	.cfi_def_cfa_offset 8
	ret
	.p2align 4
	.p2align 3
.L134:
	.cfi_restore_state
	lea	rdi, 0[0+rax*8]
	jmp	.L118
.L128:
	endbr64
	mov	rdi, rax
.L121:
	vzeroupper
	call	__cxa_begin_catch@PLT
.L122:
	cmp	r14, rbx
	jnb	.L135
	mov	rdi, QWORD PTR [r14]
	mov	esi, 512
	add	r14, 8
	call	_ZdlPvm@PLT
	jmp	.L122
.L135:
.LEHB2:
	call	__cxa_rethrow@PLT
.LEHE2:
.L129:
	endbr64
	mov	rbx, rax
.L124:
	vzeroupper
	call	__cxa_end_catch@PLT
	mov	rdi, rbx
	call	__cxa_begin_catch@PLT
	mov	rax, QWORD PTR 8[rbp]
	mov	rdi, QWORD PTR 0[rbp]
	lea	rsi, 0[0+rax*8]
	call	_ZdlPvm@PLT
	xor	eax, eax
	mov	QWORD PTR 0[rbp], rax
	mov	QWORD PTR 8[rbp], rax
.LEHB3:
	call	__cxa_rethrow@PLT
.LEHE3:
.L127:
	endbr64
	mov	rbx, rax
.L125:
	vzeroupper
	call	__cxa_end_catch@PLT
	mov	rdi, rbx
.LEHB4:
	call	_Unwind_Resume@PLT
.LEHE4:
	.cfi_endproc
.LFE3441:
	.globl	__gxx_personality_v0
	.section	.gcc_except_table._ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,"aG",@progbits,_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,comdat
	.align 4
.LLSDA3441:
	.byte	0xff
	.byte	0x9b
	.uleb128 .LLSDATT3441-.LLSDATTD3441
.LLSDATTD3441:
	.byte	0x1
	.uleb128 .LLSDACSE3441-.LLSDACSB3441
.LLSDACSB3441:
	.uleb128 .LEHB0-.LFB3441
	.uleb128 .LEHE0-.LEHB0
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB1-.LFB3441
	.uleb128 .LEHE1-.LEHB1
	.uleb128 .L128-.LFB3441
	.uleb128 0x1
	.uleb128 .LEHB2-.LFB3441
	.uleb128 .LEHE2-.LEHB2
	.uleb128 .L129-.LFB3441
	.uleb128 0x3
	.uleb128 .LEHB3-.LFB3441
	.uleb128 .LEHE3-.LEHB3
	.uleb128 .L127-.LFB3441
	.uleb128 0
	.uleb128 .LEHB4-.LFB3441
	.uleb128 .LEHE4-.LEHB4
	.uleb128 0
	.uleb128 0
.LLSDACSE3441:
	.byte	0x1
	.byte	0
	.byte	0
	.byte	0x7d
	.align 4
	.long	0

.LLSDATT3441:
	.section	.text._ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,"axG",@progbits,_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,comdat
	.size	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm, .-_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
	.section	.text._ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_,"axG",@progbits,_ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_,comdat
	.p2align 4
	.weak	_ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_
	.type	_ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_, @function
_ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_:
.LFB3671:
	.cfi_startproc
	endbr64
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	sub	rdx, rsi
	mov	rbp, rsp
	.cfi_def_cfa_register 6
	push	r15
	push	r14
	push	r13
	push	r12
	push	rbx
	.cfi_offset 15, -24
	.cfi_offset 14, -32
	.cfi_offset 13, -40
	.cfi_offset 12, -48
	.cfi_offset 3, -56
	mov	r13, rdx
	mov	r12, rcx
	and	rsp, -32
	sub	rsp, 32
	mov	rax, QWORD PTR 16[rcx]
	mov	QWORD PTR 24[rsp], rdi
	mov	rdi, QWORD PTR [rcx]
	sar	r13, 3
	test	rdx, rdx
	jle	.L137
	mov	r14, rsi
	jmp	.L147
	.p2align 4
	.p2align 3
.L153:
	cmp	rdx, 63
	jg	.L144
	add	rdi, rbx
	mov	rax, QWORD PTR 16[r12]
	mov	QWORD PTR [r12], rdi
	test	r15, r15
	je	.L148
.L147:
	sub	rax, rdi
	mov	rsi, rax
	sar	rsi, 3
	mov	rdx, rsi
	cmp	rsi, r13
	jl	.L138
	lea	rbx, 0[0+r13*8]
	xor	r15d, r15d
	cmp	rbx, 8
	je	.L140
.L139:
	mov	rdx, rbx
	mov	rsi, r14
	vzeroupper
	call	memmove@PLT
	mov	rdi, QWORD PTR [r12]
	mov	rdx, r13
	mov	r13, r15
.L142:
	mov	rsi, QWORD PTR 8[r12]
	mov	rax, rdi
	add	r14, rbx
	sub	rax, rsi
	sar	rax, 3
	add	rdx, rax
	jns	.L153
	mov	rcx, rdx
	not	rcx
	shr	rcx, 6
	not	rcx
.L146:
	mov	rax, QWORD PTR 24[r12]
	lea	r8, [rax+rcx*8]
	mov	rsi, QWORD PTR [r8]
	sal	rcx, 6
	sub	rdx, rcx
	lea	rax, 512[rsi]
	lea	rdi, [rsi+rdx*8]
	vmovq	xmm2, rax
	vmovq	xmm3, rdi
	vpinsrq	xmm0, xmm2, r8, 1
	vpinsrq	xmm1, xmm3, rsi, 1
	vinserti128	ymm0, ymm1, xmm0, 0x1
	vmovdqu	YMMWORD PTR [r12], ymm0
	test	r15, r15
	jne	.L147
.L148:
	vmovq	xmm4, rax
	vpinsrq	xmm1, xmm4, QWORD PTR 24[r12], 1
	mov	rax, QWORD PTR 24[rsp]
	vmovq	xmm5, rdi
	vpinsrq	xmm0, xmm5, rsi, 1
	vinserti128	ymm0, ymm0, xmm1, 0x1
	vmovdqu	YMMWORD PTR [rax], ymm0
	vzeroupper
	lea	rsp, -40[rbp]
	pop	rbx
	pop	r12
	pop	r13
	pop	r14
	pop	r15
	pop	rbp
	.cfi_remember_state
	.cfi_def_cfa 7, 8
	ret
	.p2align 4
	.p2align 3
.L144:
	.cfi_restore_state
	mov	rcx, rdx
	sar	rcx, 6
	jmp	.L146
	.p2align 4
	.p2align 3
.L138:
	mov	r15, r13
	mov	rbx, rax
	sub	r15, rsi
	cmp	rax, 8
	jle	.L141
	mov	r13, rsi
	jmp	.L139
	.p2align 4
	.p2align 3
.L141:
	mov	r13, r15
	cmp	rax, 8
	jne	.L142
	mov	r13d, 1
	.p2align 4
	.p2align 3
.L140:
	mov	rax, QWORD PTR [r14]
	mov	rdx, r13
	mov	ebx, 8
	mov	r13, r15
	mov	QWORD PTR [rdi], rax
	jmp	.L142
	.p2align 4
	.p2align 3
.L137:
	mov	rsi, QWORD PTR 8[rcx]
	jmp	.L148
	.cfi_endproc
.LFE3671:
	.size	_ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_, .-_ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_
	.section	.rodata._ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_.str1.1,"aMS",@progbits,1
.LC1:
	.string	"basic_string::_M_create"
	.section	.text._ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_,"axG",@progbits,_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_,comdat
	.p2align 4
	.weak	_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_
	.type	_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_, @function
_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_:
.LFB3450:
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDA3450
	endbr64
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	mov	rbp, rsp
	.cfi_def_cfa_register 6
	push	r15
	push	r14
	push	r13
	push	r12
	push	rbx
	and	rsp, -32
	sub	rsp, 160
	.cfi_offset 15, -24
	.cfi_offset 14, -32
	.cfi_offset 13, -40
	.cfi_offset 12, -48
	.cfi_offset 3, -56
	mov	QWORD PTR 32[rsp], rsi
	mov	QWORD PTR 24[rsp], rdx
	mov	rax, QWORD PTR fs:40
	mov	QWORD PTR 152[rsp], rax
	xor	eax, eax
	cmp	rdi, rsi
	je	.L186
	mov	rbx, rdi
	mov	r12, rdx
	.p2align 4
	.p2align 3
.L176:
	mov	rax, QWORD PTR [rbx]
	lea	rdi, 24[r12]
	mov	QWORD PTR 8[r12], rdi
	mov	r14, QWORD PTR 8[rbx]
	mov	QWORD PTR [r12], rax
	mov	r13, QWORD PTR 16[rbx]
	cmp	r13, 15
	ja	.L201
	cmp	r13, 1
	jne	.L162
	movzx	eax, BYTE PTR [r14]
	mov	BYTE PTR 24[r12], al
.L163:
	mov	QWORD PTR 16[r12], r13
	mov	BYTE PTR [rdi+r13], 0
	mov	r13, QWORD PTR 48[rbx]
	sub	r13, QWORD PTR 40[rbx]
	mov	QWORD PTR 40[r12], 0
	mov	QWORD PTR 48[r12], 0
	mov	QWORD PTR 56[r12], 0
	je	.L187
	js	.L202
	mov	rdi, r13
.LEHB5:
	call	_Znwm@PLT
.LEHE5:
	mov	rcx, rax
.L165:
	add	r13, rcx
	vpbroadcastq	xmm0, rcx
	vmovdqu	XMMWORD PTR 40[r12], xmm0
	mov	QWORD PTR 56[r12], r13
	mov	rsi, QWORD PTR 40[rbx]
	mov	r13, QWORD PTR 48[rbx]
	sub	r13, rsi
	cmp	r13, 1
	jle	.L168
	mov	rdi, rcx
	mov	rdx, r13
	call	memmove@PLT
	mov	rcx, rax
.L169:
	mov	rax, QWORD PTR 136[rbx]
	add	rcx, r13
	vpxor	xmm0, xmm0, xmm0
	lea	rdi, 64[r12]
	mov	QWORD PTR 48[r12], rcx
	mov	rsi, rax
	sub	rsi, QWORD PTR 104[rbx]
	sar	rsi, 3
	cmp	rax, 1
	mov	rax, QWORD PTR 112[rbx]
	adc	rsi, -1
	sub	rax, QWORD PTR 120[rbx]
	sal	rsi, 6
	sar	rax, 3
	add	rsi, rax
	mov	rax, QWORD PTR 96[rbx]
	sub	rax, QWORD PTR 80[rbx]
	vmovdqu64	ZMMWORD PTR 80[r12], zmm0
	mov	QWORD PTR 64[r12], 0
	mov	QWORD PTR 72[r12], 0
	sar	rax, 3
	add	rsi, rax
	vzeroupper
.LEHB6:
	call	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
.LEHE6:
	mov	rax, QWORD PTR 112[rbx]
	vmovdqu	ymm0, YMMWORD PTR 80[r12]
	mov	r13, QWORD PTR 136[rbx]
	mov	r14, QWORD PTR 104[rbx]
	mov	rsi, QWORD PTR 80[rbx]
	mov	rdx, QWORD PTR 96[rbx]
	mov	QWORD PTR 48[rsp], rax
	mov	rax, QWORD PTR 120[rbx]
	vmovdqa	YMMWORD PTR 64[rsp], ymm0
	mov	QWORD PTR 40[rsp], rax
	cmp	r13, r14
	je	.L172
	lea	rax, 96[rsp]
	lea	r15, 64[rsp]
	mov	rcx, r15
	mov	rdi, rax
	mov	QWORD PTR 56[rsp], rax
	jmp	.L198
	.p2align 4
	.p2align 3
.L203:
	mov	rsi, QWORD PTR [r14]
	mov	rdi, QWORD PTR 56[rsp]
	mov	rcx, r15
	lea	rdx, 512[rsi]
.L198:
	vzeroupper
	call	_ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_
	vmovdqa	ymm0, YMMWORD PTR 96[rsp]
	add	r14, 8
	vmovdqa	YMMWORD PTR 64[rsp], ymm0
	cmp	r13, r14
	jne	.L203
	mov	rdx, QWORD PTR 48[rsp]
	mov	rsi, QWORD PTR 40[rsp]
	mov	rdi, QWORD PTR 56[rsp]
	mov	rcx, r15
	vzeroupper
	call	_ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_
.L175:
	add	rbx, 144
	add	r12, 144
	cmp	QWORD PTR 32[rsp], rbx
	jne	.L176
.L154:
	mov	rax, QWORD PTR 152[rsp]
	sub	rax, QWORD PTR fs:40
	jne	.L200
	lea	rsp, -40[rbp]
	mov	rax, r12
	pop	rbx
	pop	r12
	pop	r13
	pop	r14
	pop	r15
	pop	rbp
	.cfi_remember_state
	.cfi_def_cfa 7, 8
	ret
	.p2align 4
	.p2align 3
.L187:
	.cfi_restore_state
	xor	ecx, ecx
	jmp	.L165
	.p2align 4
	.p2align 3
.L162:
	test	r13, r13
	je	.L163
	mov	rdx, r13
	mov	rsi, r14
	call	memcpy@PLT
	mov	rdi, QWORD PTR 8[r12]
	jmp	.L163
	.p2align 4
	.p2align 3
.L201:
	test	r13, r13
	js	.L204
	mov	rdi, r13
	inc	rdi
	js	.L205
.LEHB7:
	call	_Znwm@PLT
.LEHE7:
	mov	rdi, rax
	mov	rdx, r13
	mov	rsi, r14
	mov	QWORD PTR 8[r12], rax
	mov	QWORD PTR 24[r12], r13
	call	memcpy@PLT
	mov	rdi, QWORD PTR 8[r12]
	jmp	.L163
	.p2align 4
	.p2align 3
.L172:
	mov	rdx, QWORD PTR 48[rsp]
	lea	rdi, 96[rsp]
	lea	rcx, 64[rsp]
	vzeroupper
	call	_ZSt14__copy_move_a1ILb0EPmmEN9__gnu_cxx11__enable_ifIXsrSt23__is_random_access_iterIT0_NSt15iterator_traitsIS4_E17iterator_categoryEE7__valueESt15_Deque_iteratorIT1_RSA_PSA_EE6__typeES4_S4_SD_
	jmp	.L175
	.p2align 4
	.p2align 3
.L168:
	jne	.L169
	movzx	eax, BYTE PTR [rsi]
	mov	BYTE PTR [rcx], al
	jmp	.L169
.L202:
	mov	rax, QWORD PTR 152[rsp]
	sub	rax, QWORD PTR fs:40
	jne	.L200
.LEHB8:
	call	_ZSt17__throw_bad_allocv@PLT
.LEHE8:
.L205:
	mov	rax, QWORD PTR 152[rsp]
	sub	rax, QWORD PTR fs:40
	jne	.L200
.LEHB9:
	call	_ZSt17__throw_bad_allocv@PLT
.LEHE9:
.L186:
	mov	r12, rdx
	jmp	.L154
.L183:
	vzeroupper
	call	__cxa_end_catch@PLT
	mov	rax, QWORD PTR 152[rsp]
	sub	rax, QWORD PTR fs:40
	je	.L184
.L200:
	call	__stack_chk_fail@PLT
.L204:
	mov	rax, QWORD PTR 152[rsp]
	sub	rax, QWORD PTR fs:40
	jne	.L200
	lea	rdi, .LC1[rip]
.LEHB10:
	call	_ZSt20__throw_length_errorPKc@PLT
.LEHE10:
.L190:
	endbr64
	mov	rbx, rax
	jmp	.L178
.L191:
	endbr64
	mov	rbx, rax
	jmp	.L177
.L188:
	endbr64
	mov	rdi, rax
	jmp	.L179
.L177:
	lea	rdi, 40[r12]
	vzeroupper
	call	_ZNSt6vectorISt4byteSaIS0_EED1Ev
.L178:
	lea	rdi, 8[r12]
	vzeroupper
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv
	mov	rdi, rbx
.L179:
	vzeroupper
	call	__cxa_begin_catch@PLT
.L180:
	cmp	QWORD PTR 24[rsp], r12
	je	.L206
	mov	rbx, QWORD PTR 24[rsp]
	mov	rdi, rbx
	call	_ZN7hotpath9episode177godbolt19SessionWithStdQueueD1Ev
	mov	rax, rbx
	add	rax, 144
	mov	QWORD PTR 24[rsp], rax
	jmp	.L180
.L206:
	mov	rax, QWORD PTR 152[rsp]
	sub	rax, QWORD PTR fs:40
	jne	.L200
.LEHB11:
	call	__cxa_rethrow@PLT
.LEHE11:
.L189:
	endbr64
	mov	rbx, rax
	jmp	.L183
.L184:
	mov	rdi, rbx
.LEHB12:
	call	_Unwind_Resume@PLT
.LEHE12:
	.cfi_endproc
.LFE3450:
	.section	.gcc_except_table._ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_,"aG",@progbits,_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_,comdat
	.align 4
.LLSDA3450:
	.byte	0xff
	.byte	0x9b
	.uleb128 .LLSDATT3450-.LLSDATTD3450
.LLSDATTD3450:
	.byte	0x1
	.uleb128 .LLSDACSE3450-.LLSDACSB3450
.LLSDACSB3450:
	.uleb128 .LEHB5-.LFB3450
	.uleb128 .LEHE5-.LEHB5
	.uleb128 .L190-.LFB3450
	.uleb128 0x3
	.uleb128 .LEHB6-.LFB3450
	.uleb128 .LEHE6-.LEHB6
	.uleb128 .L191-.LFB3450
	.uleb128 0x3
	.uleb128 .LEHB7-.LFB3450
	.uleb128 .LEHE7-.LEHB7
	.uleb128 .L188-.LFB3450
	.uleb128 0x1
	.uleb128 .LEHB8-.LFB3450
	.uleb128 .LEHE8-.LEHB8
	.uleb128 .L190-.LFB3450
	.uleb128 0x3
	.uleb128 .LEHB9-.LFB3450
	.uleb128 .LEHE9-.LEHB9
	.uleb128 .L188-.LFB3450
	.uleb128 0x1
	.uleb128 .LEHB10-.LFB3450
	.uleb128 .LEHE10-.LEHB10
	.uleb128 .L188-.LFB3450
	.uleb128 0x1
	.uleb128 .LEHB11-.LFB3450
	.uleb128 .LEHE11-.LEHB11
	.uleb128 .L189-.LFB3450
	.uleb128 0
	.uleb128 .LEHB12-.LFB3450
	.uleb128 .LEHE12-.LEHB12
	.uleb128 0
	.uleb128 0
.LLSDACSE3450:
	.byte	0x1
	.byte	0
	.byte	0
	.byte	0x7d
	.align 4
	.long	0

.LLSDATT3450:
	.section	.text._ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_,"axG",@progbits,_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_,comdat
	.size	_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_, .-_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_
	.section	.text.unlikely,"ax",@progbits
.LCOLDB2:
	.text
.LHOTB2:
	.p2align 4
	.globl	_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_
	.type	_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_, @function
_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_:
.LFB2643:
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDA2643
	endbr64
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	mov	rbp, rsp
	.cfi_def_cfa_register 6
	push	r15
	push	r14
	push	r13
	push	r12
	push	rbx
	.cfi_offset 15, -24
	.cfi_offset 14, -32
	.cfi_offset 13, -40
	.cfi_offset 12, -48
	.cfi_offset 3, -56
	mov	r13, rdi
	mov	rbx, rsi
	and	rsp, -64
	sub	rsp, 64
	mov	r12, QWORD PTR 8[rdi]
	cmp	r12, QWORD PTR 16[rdi]
	je	.L208
	mov	rax, QWORD PTR [rsi]
	lea	rcx, 24[r12]
	mov	QWORD PTR 8[r12], rcx
	mov	rdx, QWORD PTR 8[rsi]
	mov	QWORD PTR [r12], rax
	lea	rax, 24[rsi]
	cmp	rdx, rax
	je	.L294
	mov	QWORD PTR 8[r12], rdx
	mov	rdx, QWORD PTR 24[rsi]
	mov	QWORD PTR 24[r12], rdx
.L293:
	mov	rdx, QWORD PTR 16[rbx]
.L216:
	vmovdqu	xmm4, XMMWORD PTR 40[rbx]
	vpxor	xmm0, xmm0, xmm0
	mov	QWORD PTR 8[rbx], rax
	mov	QWORD PTR 16[rbx], 0
	mov	rax, QWORD PTR 56[rbx]
	mov	QWORD PTR 16[r12], rdx
	mov	BYTE PTR 24[rbx], 0
	mov	QWORD PTR 56[rbx], 0
	mov	QWORD PTR 48[rbx], 0
	mov	QWORD PTR 40[rbx], 0
	vmovdqu64	ZMMWORD PTR 80[r12], zmm0
	lea	rdi, 64[r12]
	xor	esi, esi
	mov	QWORD PTR 64[r12], 0
	mov	QWORD PTR 72[r12], 0
	vmovdqu	XMMWORD PTR 40[r12], xmm4
	mov	QWORD PTR 56[r12], rax
	vzeroupper
.LEHB13:
	call	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
.LEHE13:
	cmp	QWORD PTR 64[rbx], 0
	je	.L217
	vpxor	xmm1, xmm1, xmm1
	vmovdqu64	zmm0, ZMMWORD PTR 80[r12]
	mov	rdx, QWORD PTR 64[r12]
	mov	rax, QWORD PTR 72[r12]
	mov	QWORD PTR 64[r12], 0
	mov	QWORD PTR 72[r12], 0
	vmovdqu64	ZMMWORD PTR 80[r12], zmm1
	vmovdqu64	zmm5, ZMMWORD PTR 64[rbx]
	vmovdqu	xmm6, XMMWORD PTR 128[rbx]
	mov	QWORD PTR 64[rbx], rdx
	mov	QWORD PTR 72[rbx], rax
	vmovdqu	XMMWORD PTR 128[r12], xmm6
	vmovdqu64	ZMMWORD PTR 80[rbx], zmm0
	vmovdqu64	ZMMWORD PTR 64[r12], zmm5
	vzeroupper
.L217:
	add	QWORD PTR 8[r13], 144
	lea	rsp, -40[rbp]
	pop	rbx
	pop	r12
	pop	r13
	pop	r14
	pop	r15
	pop	rbp
	.cfi_remember_state
	.cfi_def_cfa 7, 8
	ret
	.p2align 4
	.p2align 3
.L208:
	.cfi_restore_state
	mov	rax, QWORD PTR [rdi]
	mov	r15, r12
	movabs	rdx, -8198552921648689607
	sub	r15, rax
	mov	QWORD PTR 56[rsp], rax
	mov	rax, r15
	sar	rax, 4
	imul	rax, rdx
	movabs	rdx, 64051194700380387
	cmp	rax, rdx
	je	.L295
	mov	rcx, QWORD PTR 56[rsp]
	cmp	r12, rcx
	je	.L296
	lea	rcx, [rax+rax]
	mov	QWORD PTR 40[rsp], rcx
	cmp	rcx, rax
	jb	.L257
	test	rcx, rcx
	jne	.L297
	mov	QWORD PTR 48[rsp], 0
.L225:
	mov	rax, QWORD PTR 48[rsp]
	mov	rdx, QWORD PTR 8[rbx]
	mov	rcx, QWORD PTR 16[rbx]
	add	r15, rax
	mov	rax, QWORD PTR [rbx]
	lea	rdi, 24[r15]
	mov	QWORD PTR 8[r15], rdi
	mov	QWORD PTR [r15], rax
	lea	rax, 24[rbx]
	cmp	rdx, rax
	je	.L298
	mov	QWORD PTR 8[r15], rdx
	mov	rdx, QWORD PTR 24[rbx]
	mov	QWORD PTR 24[r15], rdx
.L233:
	vmovdqu	xmm7, XMMWORD PTR 40[rbx]
	vpxor	xmm0, xmm0, xmm0
	mov	QWORD PTR 8[rbx], rax
	mov	rax, QWORD PTR 56[rbx]
	lea	rdi, 64[r15]
	xor	esi, esi
	mov	QWORD PTR 16[r15], rcx
	mov	QWORD PTR 16[rbx], 0
	vmovdqu64	ZMMWORD PTR 80[r15], zmm0
	mov	BYTE PTR 24[rbx], 0
	mov	QWORD PTR 56[rbx], 0
	mov	QWORD PTR 48[rbx], 0
	mov	QWORD PTR 40[rbx], 0
	mov	QWORD PTR 64[r15], 0
	mov	QWORD PTR 72[r15], 0
	mov	QWORD PTR 56[r15], rax
	vmovdqu	XMMWORD PTR 40[r15], xmm7
	vzeroupper
.LEHB14:
	call	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
.LEHE14:
	cmp	QWORD PTR 64[rbx], 0
	je	.L234
	vmovdqu64	zmm3, ZMMWORD PTR 64[rbx]
	vmovdqu64	zmm0, ZMMWORD PTR 80[r15]
	mov	rdx, QWORD PTR 64[r15]
	mov	rax, QWORD PTR 72[r15]
	vmovdqu	xmm7, XMMWORD PTR 128[rbx]
	mov	QWORD PTR 64[rbx], rdx
	mov	QWORD PTR 72[rbx], rax
	vmovdqu	XMMWORD PTR 128[r15], xmm7
	vmovdqu64	ZMMWORD PTR 64[r15], zmm3
	vmovdqu64	ZMMWORD PTR 80[rbx], zmm0
	vzeroupper
.L234:
	mov	rdx, QWORD PTR 48[rsp]
	mov	rdi, QWORD PTR 56[rsp]
	mov	rsi, r12
.LEHB15:
	call	_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_
.LEHE15:
	lea	rbx, 144[rax]
	mov	rsi, r12
	mov	rdi, r12
	mov	rdx, rbx
.LEHB16:
	call	_ZSt16__do_uninit_copyIPKN7hotpath9episode177godbolt19SessionWithStdQueueEPS3_ET0_T_S8_S7_
.LEHE16:
	vmovq	xmm3, QWORD PTR 48[rsp]
	vpinsrq	xmm2, xmm3, rax, 1
	mov	rax, QWORD PTR 56[rsp]
	vmovdqa	XMMWORD PTR 16[rsp], xmm2
	mov	rbx, rax
	cmp	r12, rax
	je	.L251
	.p2align 4
	.p2align 3
.L242:
	mov	rdi, QWORD PTR 64[rbx]
	test	rdi, rdi
	je	.L245
	mov	rax, QWORD PTR 136[rbx]
	mov	r14, QWORD PTR 104[rbx]
	lea	r15, 8[rax]
	cmp	r14, r15
	jnb	.L246
	.p2align 4
	.p2align 3
.L247:
	mov	rdi, QWORD PTR [r14]
	mov	esi, 512
	add	r14, 8
	call	_ZdlPvm@PLT
	cmp	r14, r15
	jb	.L247
	mov	rdi, QWORD PTR 64[rbx]
.L246:
	mov	rax, QWORD PTR 72[rbx]
	lea	rsi, 0[0+rax*8]
	call	_ZdlPvm@PLT
.L245:
	mov	rdi, QWORD PTR 40[rbx]
	test	rdi, rdi
	je	.L248
	mov	rsi, QWORD PTR 56[rbx]
	sub	rsi, rdi
	call	_ZdlPvm@PLT
.L248:
	mov	rdi, QWORD PTR 8[rbx]
	lea	rax, 24[rbx]
	cmp	rdi, rax
	je	.L249
	mov	rax, QWORD PTR 24[rbx]
	add	rbx, 144
	lea	rsi, 1[rax]
	call	_ZdlPvm@PLT
	cmp	r12, rbx
	jne	.L242
.L251:
	cmp	QWORD PTR 56[rsp], 0
	je	.L244
	mov	rsi, QWORD PTR 16[r13]
	mov	rdi, QWORD PTR 56[rsp]
	sub	rsi, rdi
	call	_ZdlPvm@PLT
.L244:
	vmovdqa	xmm2, XMMWORD PTR 16[rsp]
	mov	rax, QWORD PTR 48[rsp]
	mov	rcx, QWORD PTR 40[rsp]
	add	rax, rcx
	vmovdqu	XMMWORD PTR 0[r13], xmm2
	mov	QWORD PTR 16[r13], rax
	lea	rsp, -40[rbp]
	pop	rbx
	pop	r12
	pop	r13
	pop	r14
	pop	r15
	pop	rbp
	.cfi_remember_state
	.cfi_def_cfa 7, 8
	ret
.L296:
	.cfi_restore_state
	add	rax, 1
	jc	.L257
	movabs	rdx, 64051194700380387
	cmp	rax, rdx
	cmova	rax, rdx
	lea	rax, [rax+rax*8]
	sal	rax, 4
	mov	rdi, rax
	mov	QWORD PTR 40[rsp], rax
.L224:
.LEHB17:
	call	_Znwm@PLT
	mov	QWORD PTR 48[rsp], rax
	jmp	.L225
	.p2align 4
	.p2align 3
.L249:
	add	rbx, 144
	cmp	r12, rbx
	jne	.L242
	jmp	.L251
.L294:
	mov	rdx, QWORD PTR 16[rsi]
	lea	rsi, 1[rdx]
	cmp	esi, 8
	jnb	.L210
	test	sil, 4
	jne	.L299
	test	esi, esi
	je	.L216
	movzx	edx, BYTE PTR 24[rbx]
	mov	BYTE PTR 24[r12], dl
	test	sil, 2
	je	.L293
	mov	esi, esi
	movzx	edx, WORD PTR -2[rax+rsi]
	mov	WORD PTR -2[rcx+rsi], dx
	mov	rdx, QWORD PTR 16[rbx]
	jmp	.L216
	.p2align 4
	.p2align 3
.L257:
	movabs	rax, 9223372036854775728
	mov	rdi, rax
	mov	QWORD PTR 40[rsp], rax
	jmp	.L224
.L210:
	mov	rdx, QWORD PTR 24[rbx]
	mov	r9, rax
	mov	QWORD PTR 24[r12], rdx
	mov	edx, esi
	mov	rdi, QWORD PTR -8[rax+rdx]
	mov	QWORD PTR -8[rcx+rdx], rdi
	lea	rdi, 32[r12]
	and	rdi, -8
	sub	rcx, rdi
	add	esi, ecx
	sub	r9, rcx
	and	esi, -8
	cmp	esi, 8
	jb	.L293
	and	esi, -8
	xor	edx, edx
.L214:
	mov	ecx, edx
	add	edx, 8
	mov	r8, QWORD PTR [r9+rcx]
	mov	QWORD PTR [rdi+rcx], r8
	cmp	edx, esi
	jb	.L214
	jmp	.L293
.L298:
	lea	r10, 1[rcx]
	cmp	r10d, 8
	jnb	.L227
	test	r10b, 4
	jne	.L300
	test	r10d, r10d
	je	.L233
	movzx	edx, BYTE PTR 24[rbx]
	mov	BYTE PTR 24[r15], dl
	test	r10b, 2
	je	.L233
	mov	r10d, r10d
	movzx	esi, WORD PTR -2[rax+r10]
	mov	WORD PTR -2[rdi+r10], si
	jmp	.L233
	.p2align 4
	.p2align 3
.L227:
	mov	rdx, QWORD PTR 24[rbx]
	mov	esi, r10d
	lea	r8, 32[r15]
	mov	r9, rax
	and	r8, -8
	mov	QWORD PTR 24[r15], rdx
	mov	rdx, QWORD PTR -8[rax+rsi]
	mov	QWORD PTR -8[rdi+rsi], rdx
	mov	rdx, rdi
	sub	rdx, r8
	sub	r9, rdx
	add	edx, r10d
	and	edx, -8
	cmp	edx, 8
	jb	.L233
	and	edx, -8
	xor	esi, esi
.L231:
	mov	edi, esi
	add	esi, 8
	mov	r10, QWORD PTR [r9+rdi]
	mov	QWORD PTR [r8+rdi], r10
	cmp	esi, edx
	jb	.L231
	jmp	.L233
.L299:
	mov	edx, DWORD PTR 24[rbx]
	mov	esi, esi
	mov	DWORD PTR 24[r12], edx
	mov	edx, DWORD PTR -4[rax+rsi]
	mov	DWORD PTR -4[rcx+rsi], edx
	mov	rdx, QWORD PTR 16[rbx]
	jmp	.L216
.L300:
	mov	edx, DWORD PTR 24[rbx]
	mov	r10d, r10d
	mov	DWORD PTR 24[r15], edx
	mov	esi, DWORD PTR -4[rax+r10]
	mov	DWORD PTR -4[rdi+r10], esi
	jmp	.L233
.L295:
	lea	rdi, .LC0[rip]
	call	_ZSt20__throw_length_errorPKc@PLT
.LEHE17:
.L297:
	movabs	rax, 64051194700380387
	cmp	rcx, rax
	cmovbe	rax, rcx
	imul	rax, rax, 144
	mov	rdi, rax
	mov	QWORD PTR 40[rsp], rax
	jmp	.L224
.L260:
	endbr64
	mov	rbx, rax
	jmp	.L219
.L263:
	endbr64
	mov	rbx, rax
	jmp	.L236
.L262:
	endbr64
	mov	rdi, rax
	jmp	.L240
.L264:
	endbr64
	mov	rdi, rax
	jmp	.L252
	.section	.gcc_except_table,"a",@progbits
	.align 4
.LLSDA2643:
	.byte	0xff
	.byte	0x9b
	.uleb128 .LLSDATT2643-.LLSDATTD2643
.LLSDATTD2643:
	.byte	0x1
	.uleb128 .LLSDACSE2643-.LLSDACSB2643
.LLSDACSB2643:
	.uleb128 .LEHB13-.LFB2643
	.uleb128 .LEHE13-.LEHB13
	.uleb128 .L260-.LFB2643
	.uleb128 0
	.uleb128 .LEHB14-.LFB2643
	.uleb128 .LEHE14-.LEHB14
	.uleb128 .L263-.LFB2643
	.uleb128 0x3
	.uleb128 .LEHB15-.LFB2643
	.uleb128 .LEHE15-.LEHB15
	.uleb128 .L264-.LFB2643
	.uleb128 0x1
	.uleb128 .LEHB16-.LFB2643
	.uleb128 .LEHE16-.LEHB16
	.uleb128 .L262-.LFB2643
	.uleb128 0x1
	.uleb128 .LEHB17-.LFB2643
	.uleb128 .LEHE17-.LEHB17
	.uleb128 0
	.uleb128 0
.LLSDACSE2643:
	.byte	0x1
	.byte	0
	.byte	0
	.byte	0x7d
	.align 4
	.long	0

.LLSDATT2643:
	.text
	.cfi_endproc
	.section	.text.unlikely
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDAC2643
	.type	_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_.cold, @function
_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_.cold:
.LFSB2643:
.L219:
	.cfi_def_cfa 6, 16
	.cfi_offset 3, -56
	.cfi_offset 6, -16
	.cfi_offset 12, -48
	.cfi_offset 13, -40
	.cfi_offset 14, -32
	.cfi_offset 15, -24
	lea	rdi, 40[r12]
	vzeroupper
	call	_ZNSt6vectorISt4byteSaIS0_EED1Ev
	lea	rdi, 8[r12]
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv
	mov	rdi, rbx
.LEHB18:
	call	_Unwind_Resume@PLT
.LEHE18:
.L236:
	lea	rdi, 40[r15]
	vzeroupper
	call	_ZNSt6vectorISt4byteSaIS0_EED1Ev
	lea	rdi, 8[r15]
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv
	mov	rdi, rbx
	call	__cxa_begin_catch@PLT
	cmp	QWORD PTR 48[rsp], 0
	je	.L237
.L290:
	mov	rsi, QWORD PTR 40[rsp]
	mov	rdi, QWORD PTR 48[rsp]
	call	_ZdlPvm@PLT
.L254:
.LEHB19:
	call	__cxa_rethrow@PLT
.LEHE19:
.L240:
	vzeroupper
	call	__cxa_begin_catch@PLT
	mov	r12, QWORD PTR 48[rsp]
.L241:
	cmp	rbx, r12
	je	.L238
	mov	rdi, r12
	add	r12, 144
	call	_ZN7hotpath9episode177godbolt19SessionWithStdQueueD1Ev
	jmp	.L241
.L252:
	vzeroupper
	call	__cxa_begin_catch@PLT
.L237:
	mov	rdi, r15
	call	_ZN7hotpath9episode177godbolt19SessionWithStdQueueD1Ev
.L238:
	cmp	QWORD PTR 48[rsp], 0
	jne	.L290
	jmp	.L254
.L261:
	endbr64
	mov	rbx, rax
	vzeroupper
	call	__cxa_end_catch@PLT
	mov	rdi, rbx
.LEHB20:
	call	_Unwind_Resume@PLT
.LEHE20:
	.cfi_endproc
.LFE2643:
	.section	.gcc_except_table
	.align 4
.LLSDAC2643:
	.byte	0xff
	.byte	0x9b
	.uleb128 .LLSDATTC2643-.LLSDATTDC2643
.LLSDATTDC2643:
	.byte	0x1
	.uleb128 .LLSDACSEC2643-.LLSDACSBC2643
.LLSDACSBC2643:
	.uleb128 .LEHB18-.LCOLDB2
	.uleb128 .LEHE18-.LEHB18
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB19-.LCOLDB2
	.uleb128 .LEHE19-.LEHB19
	.uleb128 .L261-.LCOLDB2
	.uleb128 0
	.uleb128 .LEHB20-.LCOLDB2
	.uleb128 .LEHE20-.LEHB20
	.uleb128 0
	.uleb128 0
.LLSDACSEC2643:
	.byte	0x1
	.byte	0
	.byte	0
	.byte	0x7d
	.align 4
	.long	0

.LLSDATTC2643:
	.section	.text.unlikely
	.text
	.size	_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_, .-_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_
	.section	.text.unlikely
	.size	_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_.cold, .-_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_.cold
.LCOLDE2:
	.text
.LHOTE2:
	.hidden	DW.ref.__gxx_personality_v0
	.weak	DW.ref.__gxx_personality_v0
	.section	.data.rel.local.DW.ref.__gxx_personality_v0,"awG",@progbits,DW.ref.__gxx_personality_v0,comdat
	.align 8
	.type	DW.ref.__gxx_personality_v0, @object
	.size	DW.ref.__gxx_personality_v0, 8
DW.ref.__gxx_personality_v0:
	.quad	__gxx_personality_v0
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:

# --- 2. Clang 18.1.3 (-std=c++20 -O3 -march=native -masm=intel) ---
	.text
	.intel_syntax noprefix
	.file	"godbolt_snippets.cc"
	.globl	_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_ # -- Begin function _ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_
	.p2align	4, 0x90
	.type	_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_,@function
_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_: # @_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_
	.cfi_startproc
# %bb.0:
	push	rbx
	.cfi_def_cfa_offset 16
	.cfi_offset rbx, -16
	mov	rdx, rsi
	mov	rsi, qword ptr [rdi + 8]
	mov	rbx, rdi
	cmp	rsi, qword ptr [rdi + 16]
	je	.LBB0_2
# %bb.1:
	mov	rdi, rsi
	mov	rsi, rdx
	call	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_
	add	qword ptr [rbx + 8], 144
	pop	rbx
	.cfi_def_cfa_offset 8
	ret
.LBB0_2:
	.cfi_def_cfa_offset 16
	mov	rdi, rbx
	pop	rbx
	.cfi_def_cfa_offset 8
	jmp	_ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_ # TAILCALL
.Lfunc_end0:
	.size	_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_, .Lfunc_end0-_ZN7hotpath9episode177godbolt23PushSessionWithStdQueueERSt6vectorINS1_19SessionWithStdQueueESaIS3_EEOS3_
	.cfi_endproc
                                        # -- End function
	.globl	_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_ # -- Begin function _ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_
	.p2align	4, 0x90
	.type	_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_,@function
_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_: # @_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_
	.cfi_startproc
# %bb.0:
	push	r15
	.cfi_def_cfa_offset 16
	push	r14
	.cfi_def_cfa_offset 24
	push	r12
	.cfi_def_cfa_offset 32
	push	rbx
	.cfi_def_cfa_offset 40
	push	rax
	.cfi_def_cfa_offset 48
	.cfi_offset rbx, -40
	.cfi_offset r12, -32
	.cfi_offset r14, -24
	.cfi_offset r15, -16
	mov	r14, qword ptr [rdi + 8]
	mov	rbx, rsi
	cmp	r14, qword ptr [rdi + 16]
	je	.LBB1_5
# %bb.1:
	mov	rax, qword ptr [rbx]
	lea	r15, [rbx + 24]
	mov	qword ptr [r14], rax
	lea	rax, [r14 + 24]
	mov	qword ptr [r14 + 8], rax
	mov	rsi, qword ptr [rbx + 8]
	cmp	rsi, r15
	je	.LBB1_2
# %bb.3:
	mov	qword ptr [r14 + 8], rsi
	mov	rax, qword ptr [r15]
	mov	qword ptr [r14 + 24], rax
	jmp	.LBB1_4
.LBB1_5:
	mov	rsi, r14
	mov	rdx, rbx
	add	rsp, 8
	.cfi_def_cfa_offset 40
	pop	rbx
	.cfi_def_cfa_offset 32
	pop	r12
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	jmp	_ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_ # TAILCALL
.LBB1_2:
	.cfi_def_cfa_offset 48
	mov	rdx, qword ptr [rbx + 16]
	mov	r12, rdi
	mov	rdi, rax
	inc	rdx
	call	memcpy@PLT
	mov	rdi, r12
.LBB1_4:
	mov	rax, qword ptr [rbx + 16]
	mov	qword ptr [r14 + 16], rax
	mov	qword ptr [rbx + 8], r15
	mov	qword ptr [rbx + 16], 0
	mov	byte ptr [rbx + 24], 0
	vmovups	xmm0, xmmword ptr [rbx + 40]
	vmovups	xmmword ptr [r14 + 40], xmm0
	vxorps	xmm0, xmm0, xmm0
	mov	rax, qword ptr [rbx + 56]
	mov	qword ptr [r14 + 56], rax
	mov	rax, qword ptr [rbx + 64]
	vmovups	ymmword ptr [rbx + 40], ymm0
	mov	qword ptr [r14 + 64], rax
	mov	rax, qword ptr [rbx + 72]
	mov	qword ptr [rbx + 72], 0
	mov	qword ptr [r14 + 72], rax
	mov	rax, qword ptr [rbx + 80]
	mov	qword ptr [rbx + 80], 0
	mov	qword ptr [r14 + 80], rax
	mov	rax, qword ptr [rbx + 88]
	mov	qword ptr [rbx + 88], 0
	mov	qword ptr [r14 + 88], rax
	add	qword ptr [rdi + 8], 96
	add	rsp, 8
	.cfi_def_cfa_offset 40
	pop	rbx
	.cfi_def_cfa_offset 32
	pop	r12
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	vzeroupper
	ret
.Lfunc_end1:
	.size	_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_, .Lfunc_end1-_ZN7hotpath9episode177godbolt24PushSessionWithRingQueueERSt6vectorINS1_20SessionWithRingQueueESaIS3_EEOS3_
	.cfi_endproc
                                        # -- End function
	.globl	_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm # -- Begin function _ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm
	.p2align	4, 0x90
	.type	_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm,@function
_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm: # @_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm
	.cfi_startproc
# %bb.0:
	mov	rdx, qword ptr [rdi + 16]
	mov	rcx, qword ptr [rdi + 40]
	mov	rax, rdx
	sub	rax, qword ptr [rdi + 24]
	sar	rax, 3
	add	rax, rsi
	js	.LBB2_4
# %bb.1:
	cmp	rax, 63
	ja	.LBB2_3
# %bb.2:
	lea	rax, [rdx + 8*rsi]
	mov	rax, qword ptr [rax]
	ret
.LBB2_4:
	mov	rdx, rax
	sar	rdx, 6
	jmp	.LBB2_5
.LBB2_3:
	mov	rdx, rax
	shr	rdx, 6
.LBB2_5:
	mov	rsi, rdx
	shl	rsi, 6
	sub	rax, rsi
	shl	rax, 3
	add	rax, qword ptr [rcx + 8*rdx]
	mov	rax, qword ptr [rax]
	ret
.Lfunc_end2:
	.size	_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm, .Lfunc_end2-_ZN7hotpath9episode177godbolt13IndexStdDequeERKSt5dequeImSaImEEm
	.cfi_endproc
                                        # -- End function
	.globl	_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm # -- Begin function _ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm
	.p2align	4, 0x90
	.type	_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm,@function
_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm: # @_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm
	.cfi_startproc
# %bb.0:
	mov	rcx, qword ptr [rdi + 8]
	add	rsi, qword ptr [rdi + 16]
	mov	rax, qword ptr [rdi]
	dec	rcx
	and	rcx, rsi
	mov	rax, qword ptr [rax + 8*rcx]
	ret
.Lfunc_end3:
	.size	_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm, .Lfunc_end3-_ZN7hotpath9episode177godbolt16IndexCompactRingERKNS1_13MiniRingQueueEm
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst32,"aM",@progbits,32
	.p2align	5, 0x0                          # -- Begin function _ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE
.LCPI4_0:
	.long	0                               # 0x0
	.long	64                              # 0x40
	.long	128                             # 0x80
	.long	192                             # 0xc0
	.long	256                             # 0x100
	.long	320                             # 0x140
	.long	384                             # 0x180
	.long	448                             # 0x1c0
.LCPI4_1:
	.long	512                             # 0x200
	.long	576                             # 0x240
	.long	640                             # 0x280
	.long	704                             # 0x2c0
	.long	768                             # 0x300
	.long	832                             # 0x340
	.long	896                             # 0x380
	.long	960                             # 0x3c0
.LCPI4_2:
	.long	1024                            # 0x400
	.long	1088                            # 0x440
	.long	1152                            # 0x480
	.long	1216                            # 0x4c0
	.long	1280                            # 0x500
	.long	1344                            # 0x540
	.long	1408                            # 0x580
	.long	1472                            # 0x5c0
.LCPI4_3:
	.long	1536                            # 0x600
	.long	1600                            # 0x640
	.long	1664                            # 0x680
	.long	1728                            # 0x6c0
	.long	1792                            # 0x700
	.long	1856                            # 0x740
	.long	1920                            # 0x780
	.long	1984                            # 0x7c0
.LCPI4_4:
	.long	16                              # 0x10
	.long	80                              # 0x50
	.long	144                             # 0x90
	.long	208                             # 0xd0
	.long	272                             # 0x110
	.long	336                             # 0x150
	.long	400                             # 0x190
	.long	464                             # 0x1d0
.LCPI4_5:
	.long	528                             # 0x210
	.long	592                             # 0x250
	.long	656                             # 0x290
	.long	720                             # 0x2d0
	.long	784                             # 0x310
	.long	848                             # 0x350
	.long	912                             # 0x390
	.long	976                             # 0x3d0
.LCPI4_6:
	.long	1040                            # 0x410
	.long	1104                            # 0x450
	.long	1168                            # 0x490
	.long	1232                            # 0x4d0
	.long	1296                            # 0x510
	.long	1360                            # 0x550
	.long	1424                            # 0x590
	.long	1488                            # 0x5d0
.LCPI4_7:
	.long	1552                            # 0x610
	.long	1616                            # 0x650
	.long	1680                            # 0x690
	.long	1744                            # 0x6d0
	.long	1808                            # 0x710
	.long	1872                            # 0x750
	.long	1936                            # 0x790
	.long	2000                            # 0x7d0
.LCPI4_8:
	.long	20                              # 0x14
	.long	84                              # 0x54
	.long	148                             # 0x94
	.long	212                             # 0xd4
	.long	276                             # 0x114
	.long	340                             # 0x154
	.long	404                             # 0x194
	.long	468                             # 0x1d4
.LCPI4_9:
	.long	532                             # 0x214
	.long	596                             # 0x254
	.long	660                             # 0x294
	.long	724                             # 0x2d4
	.long	788                             # 0x314
	.long	852                             # 0x354
	.long	916                             # 0x394
	.long	980                             # 0x3d4
.LCPI4_10:
	.long	1044                            # 0x414
	.long	1108                            # 0x454
	.long	1172                            # 0x494
	.long	1236                            # 0x4d4
	.long	1300                            # 0x514
	.long	1364                            # 0x554
	.long	1428                            # 0x594
	.long	1492                            # 0x5d4
.LCPI4_11:
	.long	1556                            # 0x614
	.long	1620                            # 0x654
	.long	1684                            # 0x694
	.long	1748                            # 0x6d4
	.long	1812                            # 0x714
	.long	1876                            # 0x754
	.long	1940                            # 0x794
	.long	2004                            # 0x7d4
	.text
	.globl	_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE
	.p2align	4, 0x90
	.type	_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE,@function
_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE: # @_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE
	.cfi_startproc
# %bb.0:
	mov	rdx, qword ptr [rdi]
	mov	rcx, qword ptr [rdi + 8]
	cmp	rdx, rcx
	je	.LBB4_1
# %bb.3:
	mov	rax, rcx
	sub	rax, rdx
	add	rax, -64
	cmp	rax, 2048
	jae	.LBB4_5
# %bb.4:
	xor	eax, eax
	jmp	.LBB4_8
.LBB4_1:
	xor	eax, eax
	ret
.LBB4_5:
	vmovdqa	ymm1, ymmword ptr [rip + .LCPI4_0] # ymm1 = [0,64,128,192,256,320,384,448]
	vmovdqa	ymm2, ymmword ptr [rip + .LCPI4_1] # ymm2 = [512,576,640,704,768,832,896,960]
	vmovdqa	ymm3, ymmword ptr [rip + .LCPI4_2] # ymm3 = [1024,1088,1152,1216,1280,1344,1408,1472]
	vmovdqa	ymm4, ymmword ptr [rip + .LCPI4_3] # ymm4 = [1536,1600,1664,1728,1792,1856,1920,1984]
	vmovdqa	ymm5, ymmword ptr [rip + .LCPI4_4] # ymm5 = [16,80,144,208,272,336,400,464]
	vmovdqa	ymm6, ymmword ptr [rip + .LCPI4_5] # ymm6 = [528,592,656,720,784,848,912,976]
	vmovdqa	ymm7, ymmword ptr [rip + .LCPI4_6] # ymm7 = [1040,1104,1168,1232,1296,1360,1424,1488]
	vmovdqa	ymm8, ymmword ptr [rip + .LCPI4_7] # ymm8 = [1552,1616,1680,1744,1808,1872,1936,2000]
	vmovdqa	ymm9, ymmword ptr [rip + .LCPI4_8] # ymm9 = [20,84,148,212,276,340,404,468]
	vmovdqa	ymm10, ymmword ptr [rip + .LCPI4_9] # ymm10 = [532,596,660,724,788,852,916,980]
	vmovdqa	ymm11, ymmword ptr [rip + .LCPI4_10] # ymm11 = [1044,1108,1172,1236,1300,1364,1428,1492]
	vmovdqa	ymm12, ymmword ptr [rip + .LCPI4_11] # ymm12 = [1556,1620,1684,1748,1812,1876,1940,2004]
	shr	rax, 6
	mov	edi, 32
	vpxor	xmm0, xmm0, xmm0
	vpxor	xmm13, xmm13, xmm13
	vpxor	xmm14, xmm14, xmm14
	vpxor	xmm15, xmm15, xmm15
	inc	rax
	mov	esi, eax
	and	esi, 31
	cmovne	rdi, rsi
	sub	rax, rdi
	mov	rsi, rax
	shl	rsi, 6
	add	rsi, rdx
	.p2align	4, 0x90
.LBB4_6:                                # =>This Inner Loop Header: Depth=1
	vpxord	xmm16, xmm16, xmm16
	kxnorw	k1, k0, k0
	vpxord	xmm17, xmm17, xmm17
	vpxord	xmm18, xmm18, xmm18
	vpxord	xmm19, xmm19, xmm19
	vpxord	xmm20, xmm20, xmm20
	vpxord	xmm21, xmm21, xmm21
	vpxord	xmm22, xmm22, xmm22
	vpxord	xmm23, xmm23, xmm23
	vpxord	xmm24, xmm24, xmm24
	vpgatherdq	zmm16 {k1}, zmmword ptr [rdx + ymm1 + 8]
	kxnorw	k1, k0, k0
	vpgatherdq	zmm17 {k1}, zmmword ptr [rdx + ymm2 + 8]
	kxnorw	k1, k0, k0
	vpgatherdq	zmm18 {k1}, zmmword ptr [rdx + ymm3 + 8]
	kxnorw	k1, k0, k0
	vpgatherdq	zmm19 {k1}, zmmword ptr [rdx + ymm4 + 8]
	kxnorw	k1, k0, k0
	vpgatherdd	ymm20 {k1}, ymmword ptr [rdx + ymm5]
	kxnorw	k1, k0, k0
	vpgatherdd	ymm21 {k1}, ymmword ptr [rdx + ymm6]
	kxnorw	k1, k0, k0
	vpgatherdd	ymm22 {k1}, ymmword ptr [rdx + ymm7]
	kxnorw	k1, k0, k0
	vpgatherdd	ymm23 {k1}, ymmword ptr [rdx + ymm8]
	kxnorw	k1, k0, k0
	vpgatherdd	ymm24 {k1}, ymmword ptr [rdx + ymm9]
	kxnorw	k1, k0, k0
	vpsubd	ymm20, ymm20, ymm24
	vpxord	xmm24, xmm24, xmm24
	vpgatherdd	ymm24 {k1}, ymmword ptr [rdx + ymm10]
	kxnorw	k1, k0, k0
	vpmovzxdq	zmm20, ymm20            # zmm20 = ymm20[0],zero,ymm20[1],zero,ymm20[2],zero,ymm20[3],zero,ymm20[4],zero,ymm20[5],zero,ymm20[6],zero,ymm20[7],zero
	vpmullq	zmm16, zmm16, zmm20
	vpsubd	ymm21, ymm21, ymm24
	vpxord	xmm24, xmm24, xmm24
	vpgatherdd	ymm24 {k1}, ymmword ptr [rdx + ymm11]
	kxnorw	k1, k0, k0
	vpmovzxdq	zmm21, ymm21            # zmm21 = ymm21[0],zero,ymm21[1],zero,ymm21[2],zero,ymm21[3],zero,ymm21[4],zero,ymm21[5],zero,ymm21[6],zero,ymm21[7],zero
	vpaddq	zmm0, zmm16, zmm0
	vpmullq	zmm16, zmm17, zmm21
	vpsubd	ymm22, ymm22, ymm24
	vpxord	xmm24, xmm24, xmm24
	vpgatherdd	ymm24 {k1}, ymmword ptr [rdx + ymm12]
	vpmovzxdq	zmm20, ymm22            # zmm20 = ymm22[0],zero,ymm22[1],zero,ymm22[2],zero,ymm22[3],zero,ymm22[4],zero,ymm22[5],zero,ymm22[6],zero,ymm22[7],zero
	add	rdx, 2048
	add	rax, -32
	vpaddq	zmm13, zmm16, zmm13
	vpmullq	zmm16, zmm18, zmm20
	vpsubd	ymm23, ymm23, ymm24
	vpmovzxdq	zmm17, ymm23            # zmm17 = ymm23[0],zero,ymm23[1],zero,ymm23[2],zero,ymm23[3],zero,ymm23[4],zero,ymm23[5],zero,ymm23[6],zero,ymm23[7],zero
	vpaddq	zmm14, zmm16, zmm14
	vpmullq	zmm16, zmm19, zmm17
	vpaddq	zmm15, zmm16, zmm15
	jne	.LBB4_6
# %bb.7:
	vpaddq	zmm0, zmm13, zmm0
	mov	rdx, rsi
	vpaddq	zmm0, zmm14, zmm0
	vpaddq	zmm0, zmm15, zmm0
	vextracti64x4	ymm1, zmm0, 1
	vpaddq	zmm0, zmm0, zmm1
	vextracti128	xmm1, ymm0, 1
	vpaddq	xmm0, xmm0, xmm1
	vpshufd	xmm1, xmm0, 238                 # xmm1 = xmm0[2,3,2,3]
	vpaddq	xmm0, xmm0, xmm1
	vmovq	rax, xmm0
	.p2align	4, 0x90
.LBB4_8:                                # =>This Inner Loop Header: Depth=1
	mov	esi, dword ptr [rdx + 16]
	sub	esi, dword ptr [rdx + 20]
	imul	rsi, qword ptr [rdx + 8]
	add	rdx, 64
	add	rax, rsi
	cmp	rdx, rcx
	jne	.LBB4_8
# %bb.2:
	vzeroupper
	ret
.Lfunc_end4:
	.size	_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE, .Lfunc_end4-_ZN7hotpath9episode177godbolt14ScanRuleVectorERKSt6vectorINS1_6Rule64ESaIS3_EE
	.cfi_endproc
                                        # -- End function
	.globl	_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE # -- Begin function _ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE
	.p2align	4, 0x90
	.type	_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE,@function
_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE: # @_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE
	.cfi_startproc
# %bb.0:
	mov	rcx, qword ptr [rdi]
	xor	eax, eax
	cmp	rcx, rdi
	je	.LBB5_3
	.p2align	4, 0x90
.LBB5_1:                                # =>This Inner Loop Header: Depth=1
	mov	edx, dword ptr [rcx + 80]
	sub	edx, dword ptr [rcx + 84]
	imul	rdx, qword ptr [rcx + 72]
	mov	rcx, qword ptr [rcx]
	add	rax, rdx
	cmp	rcx, rdi
	jne	.LBB5_1
.LBB5_3:
	ret
.Lfunc_end5:
	.size	_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE, .Lfunc_end5-_ZN7hotpath9episode177godbolt12ScanRuleListERKNSt7__cxx114listINS1_6Rule64ESaIS4_EEE
	.cfi_endproc
                                        # -- End function
	.section	.text._ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_,"axG",@progbits,_ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_,comdat
	.weak	_ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_ # -- Begin function _ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_
	.p2align	4, 0x90
	.type	_ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_,@function
_ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_: # @_ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_
.Lfunc_begin0:
	.cfi_startproc
	.cfi_personality 155, DW.ref.__gxx_personality_v0
	.cfi_lsda 27, .Lexception0
# %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	push	r15
	.cfi_def_cfa_offset 24
	push	r14
	.cfi_def_cfa_offset 32
	push	r13
	.cfi_def_cfa_offset 40
	push	r12
	.cfi_def_cfa_offset 48
	push	rbx
	.cfi_def_cfa_offset 56
	sub	rsp, 24
	.cfi_def_cfa_offset 80
	.cfi_offset rbx, -56
	.cfi_offset r12, -48
	.cfi_offset r13, -40
	.cfi_offset r14, -32
	.cfi_offset r15, -24
	.cfi_offset rbp, -16
	mov	r14, qword ptr [rdi + 8]
	mov	r8, qword ptr [rdi]
	movabs	rcx, 9223372036854775728
	mov	rax, r14
	sub	rax, r8
	cmp	rax, rcx
	je	.LBB6_52
# %bb.1:
	sar	rax, 4
	movabs	rcx, -8198552921648689607
	mov	rbp, rdx
	mov	r12, rsi
	movabs	rsi, 64051194700380387
	mov	r15, r12
	mov	qword ptr [rsp + 16], rdi       # 8-byte Spill
	mov	qword ptr [rsp + 8], r8         # 8-byte Spill
	imul	rax, rcx
	cmp	rax, 1
	mov	rdx, rax
	adc	rdx, 0
	lea	rbx, [rdx + rax]
	cmp	rbx, rsi
	cmovae	rbx, rsi
	add	rdx, rax
	cmovb	rbx, rsi
	sub	r15, r8
	sar	r15, 4
	imul	r15, rcx
	test	rbx, rbx
	je	.LBB6_2
# %bb.3:
	mov	rax, rbx
	shl	rax, 4
	lea	rdi, [rax + 8*rax]
	call	_Znwm@PLT
	jmp	.LBB6_4
.LBB6_2:
	xor	eax, eax
.LBB6_4:
	lea	r13, [r15 + 8*r15]
	shl	r13, 4
	add	r13, rax
.Ltmp0:
	mov	rdi, r13
	mov	rsi, rbp
	mov	r15, rax
	mov	qword ptr [rsp], rax            # 8-byte Spill
	call	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_
.Ltmp1:
# %bb.5:
	mov	rbp, r15
	cmp	qword ptr [rsp + 8], r12        # 8-byte Folded Reload
	je	.LBB6_9
# %bb.6:
	mov	rbp, qword ptr [rsp]            # 8-byte Reload
	mov	r15, qword ptr [rsp + 8]        # 8-byte Reload
	.p2align	4, 0x90
.LBB6_7:                                # =>This Inner Loop Header: Depth=1
.Ltmp3:
	mov	rdi, rbp
	mov	rsi, r15
	call	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_
.Ltmp4:
# %bb.8:                                #   in Loop: Header=BB6_7 Depth=1
	add	r15, 144
	add	rbp, 144
	cmp	r15, r12
	jne	.LBB6_7
.LBB6_9:
	add	rbp, 144
	cmp	r14, r12
	je	.LBB6_10
# %bb.17:
	mov	r13, rbp
	.p2align	4, 0x90
.LBB6_18:                               # =>This Inner Loop Header: Depth=1
.Ltmp14:
	mov	rdi, r13
	mov	rsi, r12
	call	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_
.Ltmp15:
# %bb.19:                               #   in Loop: Header=BB6_18 Depth=1
	add	r12, 144
	add	r13, 144
	cmp	r12, r14
	jne	.LBB6_18
# %bb.20:
	cmp	qword ptr [rsp + 8], r14        # 8-byte Folded Reload
	jne	.LBB6_21
	jmp	.LBB6_33
.LBB6_10:
	mov	r13, rbp
	cmp	qword ptr [rsp + 8], r14        # 8-byte Folded Reload
	je	.LBB6_33
.LBB6_21:
	mov	r15, qword ptr [rsp + 8]        # 8-byte Reload
	jmp	.LBB6_22
	.p2align	4, 0x90
.LBB6_32:                               #   in Loop: Header=BB6_22 Depth=1
	add	r15, 144
	cmp	r15, r14
	je	.LBB6_33
.LBB6_22:                               # =>This Loop Header: Depth=1
                                        #     Child Loop BB6_25 Depth 2
	mov	rdi, qword ptr [r15 + 64]
	test	rdi, rdi
	je	.LBB6_28
# %bb.23:                               #   in Loop: Header=BB6_22 Depth=1
	mov	rbp, qword ptr [r15 + 136]
	mov	r12, qword ptr [r15 + 104]
	lea	rax, [rbp + 8]
	cmp	r12, rax
	jae	.LBB6_27
# %bb.24:                               #   in Loop: Header=BB6_22 Depth=1
	add	r12, -8
	.p2align	4, 0x90
.LBB6_25:                               #   Parent Loop BB6_22 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	mov	rdi, qword ptr [r12 + 8]
	add	r12, 8
	call	_ZdlPv@PLT
	cmp	r12, rbp
	jb	.LBB6_25
# %bb.26:                               #   in Loop: Header=BB6_22 Depth=1
	mov	rdi, qword ptr [r15 + 64]
.LBB6_27:                               #   in Loop: Header=BB6_22 Depth=1
	call	_ZdlPv@PLT
.LBB6_28:                               #   in Loop: Header=BB6_22 Depth=1
	mov	rdi, qword ptr [r15 + 40]
	test	rdi, rdi
	je	.LBB6_30
# %bb.29:                               #   in Loop: Header=BB6_22 Depth=1
	call	_ZdlPv@PLT
.LBB6_30:                               #   in Loop: Header=BB6_22 Depth=1
	mov	rdi, qword ptr [r15 + 8]
	lea	rax, [r15 + 24]
	cmp	rdi, rax
	je	.LBB6_32
# %bb.31:                               #   in Loop: Header=BB6_22 Depth=1
	call	_ZdlPv@PLT
	jmp	.LBB6_32
.LBB6_33:
	mov	rdi, qword ptr [rsp + 8]        # 8-byte Reload
	test	rdi, rdi
	je	.LBB6_35
# %bb.34:
	call	_ZdlPv@PLT
.LBB6_35:
	mov	rcx, qword ptr [rsp]            # 8-byte Reload
	mov	rdx, qword ptr [rsp + 16]       # 8-byte Reload
	lea	rax, [rbx + 8*rbx]
	shl	rax, 4
	add	rax, rcx
	mov	qword ptr [rdx], rcx
	mov	qword ptr [rdx + 8], r13
	mov	qword ptr [rdx + 16], rax
	add	rsp, 24
	.cfi_def_cfa_offset 56
	pop	rbx
	.cfi_def_cfa_offset 48
	pop	r12
	.cfi_def_cfa_offset 40
	pop	r13
	.cfi_def_cfa_offset 32
	pop	r14
	.cfi_def_cfa_offset 24
	pop	r15
	.cfi_def_cfa_offset 16
	pop	rbp
	.cfi_def_cfa_offset 8
	ret
.LBB6_52:
	.cfi_def_cfa_offset 80
	lea	rdi, [rip + .L.str]
	call	_ZSt20__throw_length_errorPKc@PLT
.LBB6_43:
.Ltmp2:
	mov	rdi, rax
	call	__cxa_begin_catch@PLT
	mov	rbp, r15
	test	r15, r15
	jne	.LBB6_40
	jmp	.LBB6_44
.LBB6_36:
.Ltmp16:
	mov	rdi, rax
	call	__cxa_begin_catch@PLT
.Ltmp17:
	mov	rdi, rbp
	mov	rsi, r13
	call	_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_
.Ltmp18:
# %bb.37:
.Ltmp19:
	call	__cxa_rethrow@PLT
.Ltmp20:
# %bb.42:
.LBB6_38:
.Ltmp21:
	mov	rbx, rax
.Ltmp22:
	call	__cxa_end_catch@PLT
.Ltmp23:
# %bb.39:
	mov	rdi, rbx
	call	__cxa_begin_catch@PLT
.LBB6_40:
.Ltmp25:
	mov	rdi, qword ptr [rsp]            # 8-byte Reload
	mov	rsi, rbp
	call	_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_
.Ltmp26:
	jmp	.LBB6_45
.LBB6_41:
.Ltmp24:
	mov	rdi, rax
	call	__clang_call_terminate
.LBB6_11:
.Ltmp5:
	mov	rdi, rax
	call	__cxa_begin_catch@PLT
.Ltmp6:
	mov	rdi, qword ptr [rsp]            # 8-byte Reload
	mov	rsi, rbp
	call	_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_
.Ltmp7:
# %bb.12:
.Ltmp8:
	call	__cxa_rethrow@PLT
.Ltmp9:
# %bb.16:
.LBB6_13:
.Ltmp10:
	mov	rbx, rax
.Ltmp11:
	call	__cxa_end_catch@PLT
.Ltmp12:
# %bb.14:
	mov	rdi, rbx
	call	__cxa_begin_catch@PLT
.LBB6_44:
	mov	rdi, r13
	call	_ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_
.LBB6_45:
	cmp	qword ptr [rsp], 0              # 8-byte Folded Reload
	je	.LBB6_47
# %bb.46:
	mov	rdi, qword ptr [rsp]            # 8-byte Reload
	call	_ZdlPv@PLT
.LBB6_47:
.Ltmp27:
	call	__cxa_rethrow@PLT
.Ltmp28:
# %bb.51:
.LBB6_15:
.Ltmp13:
	mov	rdi, rax
	call	__clang_call_terminate
.LBB6_48:
.Ltmp29:
	mov	rbx, rax
.Ltmp30:
	call	__cxa_end_catch@PLT
.Ltmp31:
# %bb.49:
	mov	rdi, rbx
	call	_Unwind_Resume@PLT
.LBB6_50:
.Ltmp32:
	mov	rdi, rax
	call	__clang_call_terminate
.Lfunc_end6:
	.size	_ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_, .Lfunc_end6-_ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_
	.cfi_endproc
	.section	.gcc_except_table._ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_,"aG",@progbits,_ZNSt6vectorIN7hotpath9episode177godbolt19SessionWithStdQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_,comdat
	.p2align	2, 0x0
GCC_except_table6:
.Lexception0:
	.byte	255                             # @LPStart Encoding = omit
	.byte	155                             # @TType Encoding = indirect pcrel sdata4
	.uleb128 .Lttbase0-.Lttbaseref0
.Lttbaseref0:
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end0-.Lcst_begin0
.Lcst_begin0:
	.uleb128 .Lfunc_begin0-.Lfunc_begin0    # >> Call Site 1 <<
	.uleb128 .Ltmp0-.Lfunc_begin0           #   Call between .Lfunc_begin0 and .Ltmp0
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp0-.Lfunc_begin0           # >> Call Site 2 <<
	.uleb128 .Ltmp1-.Ltmp0                  #   Call between .Ltmp0 and .Ltmp1
	.uleb128 .Ltmp2-.Lfunc_begin0           #     jumps to .Ltmp2
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp3-.Lfunc_begin0           # >> Call Site 3 <<
	.uleb128 .Ltmp4-.Ltmp3                  #   Call between .Ltmp3 and .Ltmp4
	.uleb128 .Ltmp5-.Lfunc_begin0           #     jumps to .Ltmp5
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp14-.Lfunc_begin0          # >> Call Site 4 <<
	.uleb128 .Ltmp15-.Ltmp14                #   Call between .Ltmp14 and .Ltmp15
	.uleb128 .Ltmp16-.Lfunc_begin0          #     jumps to .Ltmp16
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp15-.Lfunc_begin0          # >> Call Site 5 <<
	.uleb128 .Ltmp17-.Ltmp15                #   Call between .Ltmp15 and .Ltmp17
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp17-.Lfunc_begin0          # >> Call Site 6 <<
	.uleb128 .Ltmp20-.Ltmp17                #   Call between .Ltmp17 and .Ltmp20
	.uleb128 .Ltmp21-.Lfunc_begin0          #     jumps to .Ltmp21
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp22-.Lfunc_begin0          # >> Call Site 7 <<
	.uleb128 .Ltmp23-.Ltmp22                #   Call between .Ltmp22 and .Ltmp23
	.uleb128 .Ltmp24-.Lfunc_begin0          #     jumps to .Ltmp24
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp23-.Lfunc_begin0          # >> Call Site 8 <<
	.uleb128 .Ltmp25-.Ltmp23                #   Call between .Ltmp23 and .Ltmp25
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp25-.Lfunc_begin0          # >> Call Site 9 <<
	.uleb128 .Ltmp26-.Ltmp25                #   Call between .Ltmp25 and .Ltmp26
	.uleb128 .Ltmp29-.Lfunc_begin0          #     jumps to .Ltmp29
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp26-.Lfunc_begin0          # >> Call Site 10 <<
	.uleb128 .Ltmp6-.Ltmp26                 #   Call between .Ltmp26 and .Ltmp6
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp6-.Lfunc_begin0           # >> Call Site 11 <<
	.uleb128 .Ltmp9-.Ltmp6                  #   Call between .Ltmp6 and .Ltmp9
	.uleb128 .Ltmp10-.Lfunc_begin0          #     jumps to .Ltmp10
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp11-.Lfunc_begin0          # >> Call Site 12 <<
	.uleb128 .Ltmp12-.Ltmp11                #   Call between .Ltmp11 and .Ltmp12
	.uleb128 .Ltmp13-.Lfunc_begin0          #     jumps to .Ltmp13
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp12-.Lfunc_begin0          # >> Call Site 13 <<
	.uleb128 .Ltmp27-.Ltmp12                #   Call between .Ltmp12 and .Ltmp27
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp27-.Lfunc_begin0          # >> Call Site 14 <<
	.uleb128 .Ltmp28-.Ltmp27                #   Call between .Ltmp27 and .Ltmp28
	.uleb128 .Ltmp29-.Lfunc_begin0          #     jumps to .Ltmp29
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp30-.Lfunc_begin0          # >> Call Site 15 <<
	.uleb128 .Ltmp31-.Ltmp30                #   Call between .Ltmp30 and .Ltmp31
	.uleb128 .Ltmp32-.Lfunc_begin0          #     jumps to .Ltmp32
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp31-.Lfunc_begin0          # >> Call Site 16 <<
	.uleb128 .Lfunc_end6-.Ltmp31            #   Call between .Ltmp31 and .Lfunc_end6
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
.Lcst_end0:
	.byte	1                               # >> Action Record 1 <<
                                        #   Catch TypeInfo 1
	.byte	0                               #   No further actions
	.p2align	2, 0x0
                                        # >> Catch TypeInfos <<
	.long	0                               # TypeInfo 1
.Lttbase0:
	.p2align	2, 0x0
                                        # -- End function
	.section	.text._ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_,"axG",@progbits,_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_,comdat
	.weak	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_ # -- Begin function _ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_
	.p2align	4, 0x90
	.type	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_,@function
_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_: # @_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_
.Lfunc_begin1:
	.cfi_startproc
	.cfi_personality 155, DW.ref.__gxx_personality_v0
	.cfi_lsda 27, .Lexception1
# %bb.0:
	push	r15
	.cfi_def_cfa_offset 16
	push	r14
	.cfi_def_cfa_offset 24
	push	r12
	.cfi_def_cfa_offset 32
	push	rbx
	.cfi_def_cfa_offset 40
	sub	rsp, 24
	.cfi_def_cfa_offset 64
	.cfi_offset rbx, -40
	.cfi_offset r12, -32
	.cfi_offset r14, -24
	.cfi_offset r15, -16
	mov	rax, qword ptr [rsi]
	lea	r15, [rdi + 24]
	mov	r14, rsi
	lea	r12, [r14 + 24]
	mov	rbx, rdi
	mov	qword ptr [rdi], rax
	mov	qword ptr [rdi + 8], r15
	mov	rsi, qword ptr [rsi + 8]
	cmp	rsi, r12
	je	.LBB7_1
# %bb.2:
	mov	qword ptr [rbx + 8], rsi
	mov	rax, qword ptr [r12]
	mov	qword ptr [rbx + 24], rax
	jmp	.LBB7_3
.LBB7_1:
	mov	rdx, qword ptr [r14 + 16]
	mov	rdi, r15
	inc	rdx
	call	memcpy@PLT
.LBB7_3:
	mov	rax, qword ptr [r14 + 16]
	mov	qword ptr [rbx + 16], rax
	mov	qword ptr [r14 + 8], r12
	mov	qword ptr [r14 + 16], 0
	mov	byte ptr [r14 + 24], 0
	lea	r12, [rbx + 64]
	vmovups	xmm0, xmmword ptr [r14 + 40]
	vmovups	xmmword ptr [rbx + 40], xmm0
	vxorps	xmm0, xmm0, xmm0
	mov	rax, qword ptr [r14 + 56]
	mov	qword ptr [rbx + 56], rax
	vmovups	xmmword ptr [r14 + 40], xmm0
	vxorps	xmm0, xmm0, xmm0
	mov	qword ptr [r14 + 56], 0
	vmovups	zmmword ptr [rbx + 80], zmm0
	vmovups	zmmword ptr [rbx + 64], zmm0
.Ltmp33:
	mov	rdi, r12
	xor	esi, esi
	vzeroupper
	call	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
.Ltmp34:
# %bb.4:
	lea	rax, [r14 + 64]
	cmp	qword ptr [rax], 0
	je	.LBB7_6
# %bb.5:
	vmovups	xmm0, xmmword ptr [r12]
	vxorps	xmm1, xmm1, xmm1
	vmovaps	xmmword ptr [rsp], xmm0
	vmovups	zmm0, zmmword ptr [rbx + 80]
	vmovups	zmmword ptr [r12 + 16], zmm1
	vmovups	zmmword ptr [r12], zmm1
	vmovups	zmm1, zmmword ptr [rax]
	vmovups	zmm2, zmmword ptr [rax + 16]
	vmovups	zmmword ptr [r12 + 16], zmm2
	vmovups	zmmword ptr [r12], zmm1
	vmovaps	xmm1, xmmword ptr [rsp]
	vmovups	xmmword ptr [rax], xmm1
	vmovups	zmmword ptr [r14 + 80], zmm0
.LBB7_6:
	add	rsp, 24
	.cfi_def_cfa_offset 40
	pop	rbx
	.cfi_def_cfa_offset 32
	pop	r12
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	vzeroupper
	ret
.LBB7_7:
	.cfi_def_cfa_offset 64
.Ltmp35:
	mov	rdi, qword ptr [rbx + 40]
	mov	r14, rax
	test	rdi, rdi
	jne	.LBB7_8
# %bb.9:
	mov	rdi, qword ptr [rbx + 8]
	cmp	rdi, r15
	jne	.LBB7_10
.LBB7_11:
	mov	rdi, r14
	call	_Unwind_Resume@PLT
.LBB7_8:
	call	_ZdlPv@PLT
	mov	rdi, qword ptr [rbx + 8]
	cmp	rdi, r15
	je	.LBB7_11
.LBB7_10:
	call	_ZdlPv@PLT
	mov	rdi, r14
	call	_Unwind_Resume@PLT
.Lfunc_end7:
	.size	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_, .Lfunc_end7-_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_
	.cfi_endproc
	.section	.gcc_except_table._ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_,"aG",@progbits,_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2EOS2_,comdat
	.p2align	2, 0x0
GCC_except_table7:
.Lexception1:
	.byte	255                             # @LPStart Encoding = omit
	.byte	255                             # @TType Encoding = omit
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end1-.Lcst_begin1
.Lcst_begin1:
	.uleb128 .Lfunc_begin1-.Lfunc_begin1    # >> Call Site 1 <<
	.uleb128 .Ltmp33-.Lfunc_begin1          #   Call between .Lfunc_begin1 and .Ltmp33
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp33-.Lfunc_begin1          # >> Call Site 2 <<
	.uleb128 .Ltmp34-.Ltmp33                #   Call between .Ltmp33 and .Ltmp34
	.uleb128 .Ltmp35-.Lfunc_begin1          #     jumps to .Ltmp35
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp34-.Lfunc_begin1          # >> Call Site 3 <<
	.uleb128 .Lfunc_end7-.Ltmp34            #   Call between .Ltmp34 and .Lfunc_end7
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
.Lcst_end1:
	.p2align	2, 0x0
                                        # -- End function
	.section	.text.__clang_call_terminate,"axG",@progbits,__clang_call_terminate,comdat
	.hidden	__clang_call_terminate          # -- Begin function __clang_call_terminate
	.weak	__clang_call_terminate
	.p2align	4, 0x90
	.type	__clang_call_terminate,@function
__clang_call_terminate:                 # @__clang_call_terminate
	.cfi_startproc
# %bb.0:
	push	rax
	.cfi_def_cfa_offset 16
	call	__cxa_begin_catch@PLT
	call	_ZSt9terminatev@PLT
.Lfunc_end8:
	.size	__clang_call_terminate, .Lfunc_end8-__clang_call_terminate
	.cfi_endproc
                                        # -- End function
	.section	.text._ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,"axG",@progbits,_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,comdat
	.weak	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm # -- Begin function _ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
	.p2align	4, 0x90
	.type	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,@function
_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm: # @_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
.Lfunc_begin2:
	.cfi_startproc
	.cfi_personality 155, DW.ref.__gxx_personality_v0
	.cfi_lsda 27, .Lexception2
# %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	push	r15
	.cfi_def_cfa_offset 24
	push	r14
	.cfi_def_cfa_offset 32
	push	r13
	.cfi_def_cfa_offset 40
	push	r12
	.cfi_def_cfa_offset 48
	push	rbx
	.cfi_def_cfa_offset 56
	push	rax
	.cfi_def_cfa_offset 64
	.cfi_offset rbx, -56
	.cfi_offset r12, -48
	.cfi_offset r13, -40
	.cfi_offset r14, -32
	.cfi_offset r15, -24
	.cfi_offset rbp, -16
	mov	r12, rsi
	shr	r12, 6
	mov	r13d, 5
	mov	rbx, rdi
	mov	r14, rsi
	cmp	r12, 6
	lea	r15, [r12 + 1]
	cmovae	r13, r12
	lea	rax, [r13 + 3]
	mov	qword ptr [rdi + 8], rax
	lea	rdi, [8*r13 + 24]
	call	_Znwm@PLT
	neg	r15
	mov	qword ptr [rbx], rax
	lea	rcx, [r13 + r15 + 3]
	shr	rcx
	lea	r15, [rax + 8*rcx]
	lea	rbp, [r15 + 8*r12 + 8]
	mov	r13, r15
	.p2align	4, 0x90
.LBB9_1:                                # =>This Inner Loop Header: Depth=1
.Ltmp36:
	mov	edi, 512
	call	_Znwm@PLT
.Ltmp37:
# %bb.2:                                #   in Loop: Header=BB9_1 Depth=1
	mov	qword ptr [r13], rax
	add	r13, 8
	cmp	r13, rbp
	jb	.LBB9_1
# %bb.3:
	mov	qword ptr [rbx + 40], r15
	lea	rdx, [r15 + 8*r12]
	and	r14d, 63
	mov	rax, qword ptr [r15]
	lea	rcx, [rax + 512]
	mov	qword ptr [rbx + 24], rax
	mov	qword ptr [rbx + 32], rcx
	mov	qword ptr [rbx + 72], rdx
	mov	rcx, qword ptr [r15 + 8*r12]
	mov	qword ptr [rbx + 56], rcx
	lea	rdx, [rcx + 512]
	lea	rcx, [rcx + 8*r14]
	mov	qword ptr [rbx + 64], rdx
	mov	qword ptr [rbx + 16], rax
	mov	qword ptr [rbx + 48], rcx
	add	rsp, 8
	.cfi_def_cfa_offset 56
	pop	rbx
	.cfi_def_cfa_offset 48
	pop	r12
	.cfi_def_cfa_offset 40
	pop	r13
	.cfi_def_cfa_offset 32
	pop	r14
	.cfi_def_cfa_offset 24
	pop	r15
	.cfi_def_cfa_offset 16
	pop	rbp
	.cfi_def_cfa_offset 8
	ret
.LBB9_4:
	.cfi_def_cfa_offset 64
.Ltmp38:
	mov	rdi, rax
	call	__cxa_begin_catch@PLT
	cmp	r13, r15
	jbe	.LBB9_6
	.p2align	4, 0x90
.LBB9_5:                                # =>This Inner Loop Header: Depth=1
	mov	rdi, qword ptr [r15]
	call	_ZdlPv@PLT
	add	r15, 8
	cmp	r15, r13
	jb	.LBB9_5
.LBB9_6:
.Ltmp39:
	call	__cxa_rethrow@PLT
.Ltmp40:
# %bb.7:
.LBB9_8:
.Ltmp41:
	mov	r14, rax
.Ltmp42:
	call	__cxa_end_catch@PLT
.Ltmp43:
# %bb.9:
	mov	rdi, r14
	call	__cxa_begin_catch@PLT
	mov	rdi, qword ptr [rbx]
	call	_ZdlPv@PLT
	vxorps	xmm0, xmm0, xmm0
	vmovups	xmmword ptr [rbx], xmm0
.Ltmp45:
	call	__cxa_rethrow@PLT
.Ltmp46:
# %bb.13:
.LBB9_10:
.Ltmp47:
	mov	rbx, rax
.Ltmp48:
	call	__cxa_end_catch@PLT
.Ltmp49:
# %bb.11:
	mov	rdi, rbx
	call	_Unwind_Resume@PLT
.LBB9_12:
.Ltmp50:
	mov	rdi, rax
	call	__clang_call_terminate
.LBB9_14:
.Ltmp44:
	mov	rdi, rax
	call	__clang_call_terminate
.Lfunc_end9:
	.size	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm, .Lfunc_end9-_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
	.cfi_endproc
	.section	.gcc_except_table._ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,"aG",@progbits,_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm,comdat
	.p2align	2, 0x0
GCC_except_table9:
.Lexception2:
	.byte	255                             # @LPStart Encoding = omit
	.byte	155                             # @TType Encoding = indirect pcrel sdata4
	.uleb128 .Lttbase1-.Lttbaseref1
.Lttbaseref1:
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end2-.Lcst_begin2
.Lcst_begin2:
	.uleb128 .Lfunc_begin2-.Lfunc_begin2    # >> Call Site 1 <<
	.uleb128 .Ltmp36-.Lfunc_begin2          #   Call between .Lfunc_begin2 and .Ltmp36
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp36-.Lfunc_begin2          # >> Call Site 2 <<
	.uleb128 .Ltmp37-.Ltmp36                #   Call between .Ltmp36 and .Ltmp37
	.uleb128 .Ltmp38-.Lfunc_begin2          #     jumps to .Ltmp38
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp37-.Lfunc_begin2          # >> Call Site 3 <<
	.uleb128 .Ltmp39-.Ltmp37                #   Call between .Ltmp37 and .Ltmp39
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp39-.Lfunc_begin2          # >> Call Site 4 <<
	.uleb128 .Ltmp40-.Ltmp39                #   Call between .Ltmp39 and .Ltmp40
	.uleb128 .Ltmp41-.Lfunc_begin2          #     jumps to .Ltmp41
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp42-.Lfunc_begin2          # >> Call Site 5 <<
	.uleb128 .Ltmp43-.Ltmp42                #   Call between .Ltmp42 and .Ltmp43
	.uleb128 .Ltmp44-.Lfunc_begin2          #     jumps to .Ltmp44
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp43-.Lfunc_begin2          # >> Call Site 6 <<
	.uleb128 .Ltmp45-.Ltmp43                #   Call between .Ltmp43 and .Ltmp45
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp45-.Lfunc_begin2          # >> Call Site 7 <<
	.uleb128 .Ltmp46-.Ltmp45                #   Call between .Ltmp45 and .Ltmp46
	.uleb128 .Ltmp47-.Lfunc_begin2          #     jumps to .Ltmp47
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp48-.Lfunc_begin2          # >> Call Site 8 <<
	.uleb128 .Ltmp49-.Ltmp48                #   Call between .Ltmp48 and .Ltmp49
	.uleb128 .Ltmp50-.Lfunc_begin2          #     jumps to .Ltmp50
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp49-.Lfunc_begin2          # >> Call Site 9 <<
	.uleb128 .Lfunc_end9-.Ltmp49            #   Call between .Ltmp49 and .Lfunc_end9
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
.Lcst_end2:
	.byte	1                               # >> Action Record 1 <<
                                        #   Catch TypeInfo 1
	.byte	0                               #   No further actions
	.p2align	2, 0x0
                                        # >> Catch TypeInfos <<
	.long	0                               # TypeInfo 1
.Lttbase1:
	.p2align	2, 0x0
                                        # -- End function
	.section	.text._ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_,"axG",@progbits,_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_,comdat
	.weak	_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_ # -- Begin function _ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_
	.p2align	4, 0x90
	.type	_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_,@function
_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_: # @_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_
	.cfi_startproc
# %bb.0:
	cmp	rdi, rsi
	je	.LBB10_14
# %bb.1:
	push	r15
	.cfi_def_cfa_offset 16
	push	r14
	.cfi_def_cfa_offset 24
	push	r12
	.cfi_def_cfa_offset 32
	push	rbx
	.cfi_def_cfa_offset 40
	push	rax
	.cfi_def_cfa_offset 48
	.cfi_offset rbx, -40
	.cfi_offset r12, -32
	.cfi_offset r14, -24
	.cfi_offset r15, -16
	mov	rbx, rsi
	mov	r14, rdi
	jmp	.LBB10_2
	.p2align	4, 0x90
.LBB10_12:                              #   in Loop: Header=BB10_2 Depth=1
	add	r14, 144
	cmp	r14, rbx
	je	.LBB10_13
.LBB10_2:                               # =>This Loop Header: Depth=1
                                        #     Child Loop BB10_5 Depth 2
	mov	rdi, qword ptr [r14 + 64]
	test	rdi, rdi
	je	.LBB10_8
# %bb.3:                                #   in Loop: Header=BB10_2 Depth=1
	mov	r12, qword ptr [r14 + 136]
	mov	r15, qword ptr [r14 + 104]
	lea	rax, [r12 + 8]
	cmp	r15, rax
	jae	.LBB10_7
# %bb.4:                                #   in Loop: Header=BB10_2 Depth=1
	add	r15, -8
	.p2align	4, 0x90
.LBB10_5:                               #   Parent Loop BB10_2 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	mov	rdi, qword ptr [r15 + 8]
	add	r15, 8
	call	_ZdlPv@PLT
	cmp	r15, r12
	jb	.LBB10_5
# %bb.6:                                #   in Loop: Header=BB10_2 Depth=1
	mov	rdi, qword ptr [r14 + 64]
.LBB10_7:                               #   in Loop: Header=BB10_2 Depth=1
	call	_ZdlPv@PLT
.LBB10_8:                               #   in Loop: Header=BB10_2 Depth=1
	mov	rdi, qword ptr [r14 + 40]
	test	rdi, rdi
	je	.LBB10_10
# %bb.9:                                #   in Loop: Header=BB10_2 Depth=1
	call	_ZdlPv@PLT
.LBB10_10:                              #   in Loop: Header=BB10_2 Depth=1
	mov	rdi, qword ptr [r14 + 8]
	lea	rax, [r14 + 24]
	cmp	rdi, rax
	je	.LBB10_12
# %bb.11:                               #   in Loop: Header=BB10_2 Depth=1
	call	_ZdlPv@PLT
	jmp	.LBB10_12
.LBB10_13:
	add	rsp, 8
	.cfi_def_cfa_offset 40
	pop	rbx
	.cfi_def_cfa_offset 32
	pop	r12
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	.cfi_restore rbx
	.cfi_restore r12
	.cfi_restore r14
	.cfi_restore r15
.LBB10_14:
	ret
.Lfunc_end10:
	.size	_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_, .Lfunc_end10-_ZSt8_DestroyIPN7hotpath9episode177godbolt19SessionWithStdQueueEEvT_S5_
	.cfi_endproc
                                        # -- End function
	.section	.text._ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_,"axG",@progbits,_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_,comdat
	.weak	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_ # -- Begin function _ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_
	.p2align	4, 0x90
	.type	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_,@function
_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_: # @_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_
.Lfunc_begin3:
	.cfi_startproc
	.cfi_personality 155, DW.ref.__gxx_personality_v0
	.cfi_lsda 27, .Lexception3
# %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	push	r15
	.cfi_def_cfa_offset 24
	push	r14
	.cfi_def_cfa_offset 32
	push	r13
	.cfi_def_cfa_offset 40
	push	r12
	.cfi_def_cfa_offset 48
	push	rbx
	.cfi_def_cfa_offset 56
	push	rax
	.cfi_def_cfa_offset 64
	.cfi_offset rbx, -56
	.cfi_offset r12, -48
	.cfi_offset r13, -40
	.cfi_offset r14, -32
	.cfi_offset r15, -24
	.cfi_offset rbp, -16
	mov	rax, qword ptr [rsi]
	lea	rbp, [rdi + 24]
	mov	r14, rsi
	mov	rbx, rdi
	mov	r13, rbp
	mov	qword ptr [rdi], rax
	mov	qword ptr [rdi + 8], rbp
	mov	r12, qword ptr [rsi + 8]
	mov	r15, qword ptr [rsi + 16]
	cmp	r15, 16
	jb	.LBB11_4
# %bb.1:
	test	r15, r15
	js	.LBB11_27
# %bb.2:
	mov	rdi, r15
	inc	rdi
	js	.LBB11_28
# %bb.3:
	call	_Znwm@PLT
	mov	r13, rax
	mov	qword ptr [rbx + 8], rax
	mov	qword ptr [rbx + 24], r15
.LBB11_4:
	test	r15, r15
	je	.LBB11_8
# %bb.5:
	cmp	r15, 1
	jne	.LBB11_7
# %bb.6:
	movzx	eax, byte ptr [r12]
	mov	byte ptr [r13], al
	jmp	.LBB11_8
.LBB11_7:
	mov	rdi, r13
	mov	rsi, r12
	mov	rdx, r15
	call	memcpy@PLT
.LBB11_8:
	mov	qword ptr [rbx + 16], r15
	mov	byte ptr [r13 + r15], 0
	vxorps	xmm0, xmm0, xmm0
	mov	r12, qword ptr [r14 + 48]
	sub	r12, qword ptr [r14 + 40]
	vmovups	xmmword ptr [rbx + 40], xmm0
	mov	qword ptr [rbx + 56], 0
	je	.LBB11_9
# %bb.10:
	js	.LBB11_11
# %bb.13:
.Ltmp51:
	mov	rdi, r12
	call	_Znwm@PLT
.Ltmp52:
# %bb.14:
	mov	r15, rax
	jmp	.LBB11_15
.LBB11_9:
	xor	r15d, r15d
.LBB11_15:
	add	r12, r15
	mov	qword ptr [rbx + 40], r15
	mov	qword ptr [rbx + 48], r15
	mov	qword ptr [rbx + 56], r12
	mov	rsi, qword ptr [r14 + 40]
	mov	r12, qword ptr [r14 + 48]
	sub	r12, rsi
	cmp	r12, 2
	jl	.LBB11_17
# %bb.16:
	mov	rdi, r15
	mov	rdx, r12
	call	memmove@PLT
.LBB11_19:
	lea	r13, [rbx + 40]
	add	r15, r12
	lea	rdi, [rbx + 64]
	add	r14, 64
	mov	qword ptr [rbx + 48], r15
.Ltmp56:
	mov	rsi, r14
	call	_ZNSt5dequeImSaImEEC2ERKS1_
.Ltmp57:
# %bb.20:
	add	rsp, 8
	.cfi_def_cfa_offset 56
	pop	rbx
	.cfi_def_cfa_offset 48
	pop	r12
	.cfi_def_cfa_offset 40
	pop	r13
	.cfi_def_cfa_offset 32
	pop	r14
	.cfi_def_cfa_offset 24
	pop	r15
	.cfi_def_cfa_offset 16
	pop	rbp
	.cfi_def_cfa_offset 8
	ret
.LBB11_17:
	.cfi_def_cfa_offset 64
	cmp	r12, 1
	jne	.LBB11_19
# %bb.18:
	movzx	eax, byte ptr [rsi]
	mov	byte ptr [r15], al
	jmp	.LBB11_19
.LBB11_11:
.Ltmp53:
	call	_ZSt17__throw_bad_allocv@PLT
.Ltmp54:
# %bb.12:
.LBB11_28:
	call	_ZSt17__throw_bad_allocv@PLT
.LBB11_27:
	lea	rdi, [rip + .L.str.1]
	call	_ZSt20__throw_length_errorPKc@PLT
.LBB11_22:
.Ltmp58:
	mov	rdi, qword ptr [r13]
	mov	r14, rax
	test	rdi, rdi
	jne	.LBB11_23
# %bb.24:
	mov	rdi, qword ptr [rbx + 8]
	cmp	rdi, rbp
	jne	.LBB11_25
.LBB11_26:
	mov	rdi, r14
	call	_Unwind_Resume@PLT
.LBB11_23:
	call	_ZdlPv@PLT
	mov	rdi, qword ptr [rbx + 8]
	cmp	rdi, rbp
	je	.LBB11_26
	jmp	.LBB11_25
.LBB11_21:
.Ltmp55:
	mov	r14, rax
	mov	rdi, qword ptr [rbx + 8]
	cmp	rdi, rbp
	je	.LBB11_26
.LBB11_25:
	call	_ZdlPv@PLT
	mov	rdi, r14
	call	_Unwind_Resume@PLT
.Lfunc_end11:
	.size	_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_, .Lfunc_end11-_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_
	.cfi_endproc
	.section	.gcc_except_table._ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_,"aG",@progbits,_ZN7hotpath9episode177godbolt19SessionWithStdQueueC2ERKS2_,comdat
	.p2align	2, 0x0
GCC_except_table11:
.Lexception3:
	.byte	255                             # @LPStart Encoding = omit
	.byte	255                             # @TType Encoding = omit
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end3-.Lcst_begin3
.Lcst_begin3:
	.uleb128 .Lfunc_begin3-.Lfunc_begin3    # >> Call Site 1 <<
	.uleb128 .Ltmp51-.Lfunc_begin3          #   Call between .Lfunc_begin3 and .Ltmp51
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp51-.Lfunc_begin3          # >> Call Site 2 <<
	.uleb128 .Ltmp52-.Ltmp51                #   Call between .Ltmp51 and .Ltmp52
	.uleb128 .Ltmp55-.Lfunc_begin3          #     jumps to .Ltmp55
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp52-.Lfunc_begin3          # >> Call Site 3 <<
	.uleb128 .Ltmp56-.Ltmp52                #   Call between .Ltmp52 and .Ltmp56
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp56-.Lfunc_begin3          # >> Call Site 4 <<
	.uleb128 .Ltmp57-.Ltmp56                #   Call between .Ltmp56 and .Ltmp57
	.uleb128 .Ltmp58-.Lfunc_begin3          #     jumps to .Ltmp58
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp53-.Lfunc_begin3          # >> Call Site 5 <<
	.uleb128 .Ltmp54-.Ltmp53                #   Call between .Ltmp53 and .Ltmp54
	.uleb128 .Ltmp55-.Lfunc_begin3          #     jumps to .Ltmp55
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp54-.Lfunc_begin3          # >> Call Site 6 <<
	.uleb128 .Lfunc_end11-.Ltmp54           #   Call between .Ltmp54 and .Lfunc_end11
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
.Lcst_end3:
	.p2align	2, 0x0
                                        # -- End function
	.section	.text._ZNSt5dequeImSaImEEC2ERKS1_,"axG",@progbits,_ZNSt5dequeImSaImEEC2ERKS1_,comdat
	.weak	_ZNSt5dequeImSaImEEC2ERKS1_     # -- Begin function _ZNSt5dequeImSaImEEC2ERKS1_
	.p2align	4, 0x90
	.type	_ZNSt5dequeImSaImEEC2ERKS1_,@function
_ZNSt5dequeImSaImEEC2ERKS1_:            # @_ZNSt5dequeImSaImEEC2ERKS1_
.Lfunc_begin4:
	.cfi_startproc
	.cfi_personality 155, DW.ref.__gxx_personality_v0
	.cfi_lsda 27, .Lexception4
# %bb.0:
	push	r14
	.cfi_def_cfa_offset 16
	push	rbx
	.cfi_def_cfa_offset 24
	sub	rsp, 136
	.cfi_def_cfa_offset 160
	.cfi_offset rbx, -24
	.cfi_offset r14, -16
	mov	rax, qword ptr [rsi + 72]
	mov	r14, rsi
	vxorps	xmm0, xmm0, xmm0
	mov	rbx, rdi
	mov	rcx, rax
	sub	rcx, qword ptr [rsi + 40]
	mov	rsi, qword ptr [rsi + 32]
	shr	rcx, 3
	cmp	rax, 1
	mov	rax, qword ptr [r14 + 48]
	adc	rcx, -1
	sub	rax, qword ptr [r14 + 56]
	sub	rsi, qword ptr [r14 + 16]
	vmovups	zmmword ptr [rdi + 16], zmm0
	vmovups	zmmword ptr [rdi], zmm0
	shl	rcx, 6
	sar	rax, 3
	sar	rsi, 3
	add	rsi, rax
	add	rsi, rcx
	vzeroupper
	call	_ZNSt11_Deque_baseImSaImEE17_M_initialize_mapEm
	vmovups	ymm0, ymmword ptr [r14 + 16]
	vmovups	ymmword ptr [rsp + 72], ymm0
	vmovups	ymm0, ymmword ptr [r14 + 48]
	vmovups	ymmword ptr [rsp + 40], ymm0
	vmovups	ymm0, ymmword ptr [rbx + 16]
	vmovups	ymmword ptr [rsp + 8], ymm0
.Ltmp59:
	lea	rdi, [rsp + 104]
	lea	rsi, [rsp + 72]
	lea	rdx, [rsp + 40]
	lea	rcx, [rsp + 8]
	vzeroupper
	call	_ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_
.Ltmp60:
# %bb.1:
	add	rsp, 136
	.cfi_def_cfa_offset 24
	pop	rbx
	.cfi_def_cfa_offset 16
	pop	r14
	.cfi_def_cfa_offset 8
	ret
.LBB12_2:
	.cfi_def_cfa_offset 160
.Ltmp61:
	mov	rdi, rbx
	mov	r14, rax
	call	_ZNSt11_Deque_baseImSaImEED2Ev
	mov	rdi, r14
	call	_Unwind_Resume@PLT
.Lfunc_end12:
	.size	_ZNSt5dequeImSaImEEC2ERKS1_, .Lfunc_end12-_ZNSt5dequeImSaImEEC2ERKS1_
	.cfi_endproc
	.section	.gcc_except_table._ZNSt5dequeImSaImEEC2ERKS1_,"aG",@progbits,_ZNSt5dequeImSaImEEC2ERKS1_,comdat
	.p2align	2, 0x0
GCC_except_table12:
.Lexception4:
	.byte	255                             # @LPStart Encoding = omit
	.byte	255                             # @TType Encoding = omit
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end4-.Lcst_begin4
.Lcst_begin4:
	.uleb128 .Lfunc_begin4-.Lfunc_begin4    # >> Call Site 1 <<
	.uleb128 .Ltmp59-.Lfunc_begin4          #   Call between .Lfunc_begin4 and .Ltmp59
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp59-.Lfunc_begin4          # >> Call Site 2 <<
	.uleb128 .Ltmp60-.Ltmp59                #   Call between .Ltmp59 and .Ltmp60
	.uleb128 .Ltmp61-.Lfunc_begin4          #     jumps to .Ltmp61
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp60-.Lfunc_begin4          # >> Call Site 3 <<
	.uleb128 .Lfunc_end12-.Ltmp60           #   Call between .Ltmp60 and .Lfunc_end12
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
.Lcst_end4:
	.p2align	2, 0x0
                                        # -- End function
	.section	.text._ZNSt11_Deque_baseImSaImEED2Ev,"axG",@progbits,_ZNSt11_Deque_baseImSaImEED2Ev,comdat
	.weak	_ZNSt11_Deque_baseImSaImEED2Ev  # -- Begin function _ZNSt11_Deque_baseImSaImEED2Ev
	.p2align	4, 0x90
	.type	_ZNSt11_Deque_baseImSaImEED2Ev,@function
_ZNSt11_Deque_baseImSaImEED2Ev:         # @_ZNSt11_Deque_baseImSaImEED2Ev
	.cfi_startproc
# %bb.0:
	mov	rax, rdi
	mov	rdi, qword ptr [rdi]
	test	rdi, rdi
	je	.LBB13_6
# %bb.1:
	push	r15
	.cfi_def_cfa_offset 16
	push	r14
	.cfi_def_cfa_offset 24
	push	rbx
	.cfi_def_cfa_offset 32
	.cfi_offset rbx, -32
	.cfi_offset r14, -24
	.cfi_offset r15, -16
	mov	r14, qword ptr [rax + 72]
	mov	rbx, qword ptr [rax + 40]
	lea	rcx, [r14 + 8]
	cmp	rbx, rcx
	jae	.LBB13_5
# %bb.2:
	add	rbx, -8
	mov	r15, rax
	.p2align	4, 0x90
.LBB13_3:                               # =>This Inner Loop Header: Depth=1
	mov	rdi, qword ptr [rbx + 8]
	add	rbx, 8
	call	_ZdlPv@PLT
	cmp	rbx, r14
	jb	.LBB13_3
# %bb.4:
	mov	rdi, qword ptr [r15]
.LBB13_5:
	pop	rbx
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	jmp	_ZdlPv@PLT                      # TAILCALL
.LBB13_6:
	.cfi_restore rbx
	.cfi_restore r14
	.cfi_restore r15
	ret
.Lfunc_end13:
	.size	_ZNSt11_Deque_baseImSaImEED2Ev, .Lfunc_end13-_ZNSt11_Deque_baseImSaImEED2Ev
	.cfi_endproc
                                        # -- End function
	.section	.text._ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_,"axG",@progbits,_ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_,comdat
	.weak	_ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_ # -- Begin function _ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_
	.p2align	4, 0x90
	.type	_ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_,@function
_ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_: # @_ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_
	.cfi_startproc
# %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	push	r15
	.cfi_def_cfa_offset 24
	push	r14
	.cfi_def_cfa_offset 32
	push	r13
	.cfi_def_cfa_offset 40
	push	r12
	.cfi_def_cfa_offset 48
	push	rbx
	.cfi_def_cfa_offset 56
	sub	rsp, 40
	.cfi_def_cfa_offset 96
	.cfi_offset rbx, -56
	.cfi_offset r12, -48
	.cfi_offset r13, -40
	.cfi_offset r14, -32
	.cfi_offset r15, -24
	.cfi_offset rbp, -16
	mov	r13, qword ptr [rsi]
	mov	rax, qword ptr [rsi + 24]
	mov	qword ptr [rsp + 32], rdi       # 8-byte Spill
	cmp	rax, qword ptr [rdx + 24]
	je	.LBB14_40
# %bb.1:
	mov	r15, qword ptr [rsi + 16]
	mov	qword ptr [rsp + 24], rdx       # 8-byte Spill
	mov	rax, qword ptr [rcx + 8]
	mov	r14, qword ptr [rcx]
	mov	rdx, qword ptr [rcx + 16]
	mov	rbx, qword ptr [rcx + 24]
	mov	rbp, rsi
	mov	qword ptr [rsp + 16], rcx       # 8-byte Spill
	sub	r15, r13
	mov	qword ptr [rsp], rax            # 8-byte Spill
	sar	r15, 3
	test	r15, r15
	jg	.LBB14_2
.LBB14_13:
	mov	rcx, qword ptr [rsp + 16]       # 8-byte Reload
	mov	rax, qword ptr [rsp]            # 8-byte Reload
	mov	qword ptr [rcx], r14
	mov	qword ptr [rcx + 8], rax
	mov	qword ptr [rcx + 16], rdx
	mov	qword ptr [rcx + 24], rbx
	mov	rcx, qword ptr [rbp + 24]
	jmp	.LBB14_14
	.p2align	4, 0x90
.LBB14_39:                              #   in Loop: Header=BB14_14 Depth=1
	mov	rcx, qword ptr [rsp + 16]       # 8-byte Reload
	mov	rax, qword ptr [rsp]            # 8-byte Reload
	mov	qword ptr [rcx], r14
	mov	qword ptr [rcx + 8], rax
	mov	qword ptr [rcx + 16], rdx
	mov	qword ptr [rcx + 24], rbx
	mov	rcx, qword ptr [rsp + 8]        # 8-byte Reload
.LBB14_14:                              # =>This Loop Header: Depth=1
                                        #     Child Loop BB14_28 Depth 2
	mov	rax, qword ptr [rsp + 24]       # 8-byte Reload
	add	rcx, 8
	cmp	rcx, qword ptr [rax + 24]
	je	.LBB14_15
# %bb.27:                               #   in Loop: Header=BB14_14 Depth=1
	mov	r13, qword ptr [rcx]
	mov	r15d, 64
	mov	qword ptr [rsp + 8], rcx        # 8-byte Spill
	jmp	.LBB14_28
	.p2align	4, 0x90
.LBB14_36:                              #   in Loop: Header=BB14_28 Depth=2
	mov	rcx, rax
	sar	rcx, 6
.LBB14_37:                              #   in Loop: Header=BB14_28 Depth=2
	mov	rsi, qword ptr [rbx + 8*rcx]
	lea	rbx, [rbx + 8*rcx]
	shl	rcx, 6
	sub	rax, rcx
	lea	r14, [rsi + 8*rax]
	lea	rdx, [rsi + 512]
	mov	qword ptr [rsp], rsi            # 8-byte Spill
	lea	r13, [r13 + 8*r12]
	sub	r15, r12
	jle	.LBB14_39
.LBB14_28:                              #   Parent Loop BB14_14 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	mov	r12, rdx
	sub	r12, r14
	mov	rbp, rdx
	sar	r12, 3
	cmp	r12, r15
	cmovge	r12, r15
	cmp	r12, 2
	jl	.LBB14_30
# %bb.29:                               #   in Loop: Header=BB14_28 Depth=2
	lea	rdx, [8*r12]
	mov	rdi, r14
	mov	rsi, r13
	call	memmove@PLT
.LBB14_32:                              #   in Loop: Header=BB14_28 Depth=2
	mov	rax, r14
	sub	rax, qword ptr [rsp]            # 8-byte Folded Reload
	sar	rax, 3
	add	rax, r12
	js	.LBB14_36
# %bb.33:                               #   in Loop: Header=BB14_28 Depth=2
	cmp	rax, 63
	ja	.LBB14_35
# %bb.34:                               #   in Loop: Header=BB14_28 Depth=2
	lea	r14, [r14 + 8*r12]
	mov	rdx, rbp
	lea	r13, [r13 + 8*r12]
	sub	r15, r12
	jg	.LBB14_28
	jmp	.LBB14_39
	.p2align	4, 0x90
.LBB14_35:                              #   in Loop: Header=BB14_28 Depth=2
	mov	rcx, rax
	shr	rcx, 6
	jmp	.LBB14_37
.LBB14_30:                              #   in Loop: Header=BB14_28 Depth=2
	cmp	r12, 1
	jne	.LBB14_32
# %bb.31:                               #   in Loop: Header=BB14_28 Depth=2
	mov	rax, qword ptr [r13]
	mov	qword ptr [r14], rax
	jmp	.LBB14_32
	.p2align	4, 0x90
.LBB14_10:                              #   in Loop: Header=BB14_2 Depth=1
	mov	rcx, rax
	sar	rcx, 6
.LBB14_11:                              #   in Loop: Header=BB14_2 Depth=1
	mov	rsi, qword ptr [rbx + 8*rcx]
	lea	rbx, [rbx + 8*rcx]
	shl	rcx, 6
	sub	rax, rcx
	lea	r14, [rsi + 8*rax]
	lea	rdx, [rsi + 512]
	mov	qword ptr [rsp], rsi            # 8-byte Spill
	lea	r13, [r13 + 8*r12]
	sub	r15, r12
	jle	.LBB14_13
.LBB14_2:                               # =>This Inner Loop Header: Depth=1
	mov	r12, rdx
	sub	r12, r14
	mov	qword ptr [rsp + 8], rdx        # 8-byte Spill
	sar	r12, 3
	cmp	r12, r15
	cmovge	r12, r15
	cmp	r12, 2
	jl	.LBB14_4
# %bb.3:                                #   in Loop: Header=BB14_2 Depth=1
	lea	rdx, [8*r12]
	mov	rdi, r14
	mov	rsi, r13
	call	memmove@PLT
.LBB14_6:                               #   in Loop: Header=BB14_2 Depth=1
	mov	rax, r14
	sub	rax, qword ptr [rsp]            # 8-byte Folded Reload
	sar	rax, 3
	add	rax, r12
	js	.LBB14_10
# %bb.7:                                #   in Loop: Header=BB14_2 Depth=1
	cmp	rax, 63
	ja	.LBB14_9
# %bb.8:                                #   in Loop: Header=BB14_2 Depth=1
	mov	rdx, qword ptr [rsp + 8]        # 8-byte Reload
	lea	r14, [r14 + 8*r12]
	lea	r13, [r13 + 8*r12]
	sub	r15, r12
	jg	.LBB14_2
	jmp	.LBB14_13
	.p2align	4, 0x90
.LBB14_9:                               #   in Loop: Header=BB14_2 Depth=1
	mov	rcx, rax
	shr	rcx, 6
	jmp	.LBB14_11
.LBB14_4:                               #   in Loop: Header=BB14_2 Depth=1
	cmp	r12, 1
	jne	.LBB14_6
# %bb.5:                                #   in Loop: Header=BB14_2 Depth=1
	mov	rax, qword ptr [r13]
	mov	qword ptr [r14], rax
	jmp	.LBB14_6
.LBB14_15:
	mov	r13, qword ptr [rax]
	mov	r15, qword ptr [rax + 8]
	sub	r13, r15
	sar	r13, 3
	test	r13, r13
	jg	.LBB14_16
	jmp	.LBB14_52
	.p2align	4, 0x90
.LBB14_24:                              #   in Loop: Header=BB14_16 Depth=1
	mov	rcx, rax
	sar	rcx, 6
.LBB14_25:                              #   in Loop: Header=BB14_16 Depth=1
	mov	rsi, qword ptr [rbx + 8*rcx]
	lea	rbx, [rbx + 8*rcx]
	shl	rcx, 6
	sub	rax, rcx
	lea	r14, [rsi + 8*rax]
	lea	rdx, [rsi + 512]
	mov	qword ptr [rsp], rsi            # 8-byte Spill
	lea	r15, [r15 + 8*r12]
	sub	r13, r12
	jle	.LBB14_52
.LBB14_16:                              # =>This Inner Loop Header: Depth=1
	mov	r12, rdx
	sub	r12, r14
	mov	qword ptr [rsp + 8], rdx        # 8-byte Spill
	sar	r12, 3
	cmp	r12, r13
	cmovge	r12, r13
	cmp	r12, 2
	jl	.LBB14_18
# %bb.17:                               #   in Loop: Header=BB14_16 Depth=1
	lea	rdx, [8*r12]
	mov	rdi, r14
	mov	rsi, r15
	call	memmove@PLT
.LBB14_20:                              #   in Loop: Header=BB14_16 Depth=1
	mov	rax, r14
	sub	rax, qword ptr [rsp]            # 8-byte Folded Reload
	sar	rax, 3
	add	rax, r12
	js	.LBB14_24
# %bb.21:                               #   in Loop: Header=BB14_16 Depth=1
	cmp	rax, 63
	ja	.LBB14_23
# %bb.22:                               #   in Loop: Header=BB14_16 Depth=1
	mov	rdx, qword ptr [rsp + 8]        # 8-byte Reload
	lea	r14, [r14 + 8*r12]
	lea	r15, [r15 + 8*r12]
	sub	r13, r12
	jg	.LBB14_16
	jmp	.LBB14_52
	.p2align	4, 0x90
.LBB14_23:                              #   in Loop: Header=BB14_16 Depth=1
	mov	rcx, rax
	shr	rcx, 6
	jmp	.LBB14_25
.LBB14_18:                              #   in Loop: Header=BB14_16 Depth=1
	cmp	r12, 1
	jne	.LBB14_20
# %bb.19:                               #   in Loop: Header=BB14_16 Depth=1
	mov	rax, qword ptr [r15]
	mov	qword ptr [r14], rax
	jmp	.LBB14_20
.LBB14_40:
	mov	r15, qword ptr [rdx]
	mov	rax, qword ptr [rcx + 8]
	mov	r14, qword ptr [rcx]
	mov	rdx, qword ptr [rcx + 16]
	mov	rbx, qword ptr [rcx + 24]
	sub	r15, r13
	mov	qword ptr [rsp], rax            # 8-byte Spill
	sar	r15, 3
	test	r15, r15
	jg	.LBB14_41
.LBB14_52:
	mov	rax, qword ptr [rsp + 32]       # 8-byte Reload
	mov	rcx, qword ptr [rsp]            # 8-byte Reload
	mov	qword ptr [rax], r14
	mov	qword ptr [rax + 8], rcx
	mov	qword ptr [rax + 16], rdx
	mov	qword ptr [rax + 24], rbx
	add	rsp, 40
	.cfi_def_cfa_offset 56
	pop	rbx
	.cfi_def_cfa_offset 48
	pop	r12
	.cfi_def_cfa_offset 40
	pop	r13
	.cfi_def_cfa_offset 32
	pop	r14
	.cfi_def_cfa_offset 24
	pop	r15
	.cfi_def_cfa_offset 16
	pop	rbp
	.cfi_def_cfa_offset 8
	ret
	.p2align	4, 0x90
.LBB14_49:                              #   in Loop: Header=BB14_41 Depth=1
	.cfi_def_cfa_offset 96
	mov	rcx, rax
	sar	rcx, 6
.LBB14_50:                              #   in Loop: Header=BB14_41 Depth=1
	mov	rsi, qword ptr [rbx + 8*rcx]
	lea	rbx, [rbx + 8*rcx]
	shl	rcx, 6
	sub	rax, rcx
	lea	r14, [rsi + 8*rax]
	lea	rdx, [rsi + 512]
	mov	qword ptr [rsp], rsi            # 8-byte Spill
	lea	r13, [r13 + 8*r12]
	sub	r15, r12
	jle	.LBB14_52
.LBB14_41:                              # =>This Inner Loop Header: Depth=1
	mov	r12, rdx
	sub	r12, r14
	mov	rbp, rdx
	sar	r12, 3
	cmp	r12, r15
	cmovge	r12, r15
	cmp	r12, 2
	jl	.LBB14_43
# %bb.42:                               #   in Loop: Header=BB14_41 Depth=1
	lea	rdx, [8*r12]
	mov	rdi, r14
	mov	rsi, r13
	call	memmove@PLT
.LBB14_45:                              #   in Loop: Header=BB14_41 Depth=1
	mov	rax, r14
	sub	rax, qword ptr [rsp]            # 8-byte Folded Reload
	sar	rax, 3
	add	rax, r12
	js	.LBB14_49
# %bb.46:                               #   in Loop: Header=BB14_41 Depth=1
	cmp	rax, 63
	ja	.LBB14_48
# %bb.47:                               #   in Loop: Header=BB14_41 Depth=1
	lea	r14, [r14 + 8*r12]
	mov	rdx, rbp
	lea	r13, [r13 + 8*r12]
	sub	r15, r12
	jg	.LBB14_41
	jmp	.LBB14_52
	.p2align	4, 0x90
.LBB14_48:                              #   in Loop: Header=BB14_41 Depth=1
	mov	rcx, rax
	shr	rcx, 6
	jmp	.LBB14_50
.LBB14_43:                              #   in Loop: Header=BB14_41 Depth=1
	cmp	r12, 1
	jne	.LBB14_45
# %bb.44:                               #   in Loop: Header=BB14_41 Depth=1
	mov	rax, qword ptr [r13]
	mov	qword ptr [r14], rax
	jmp	.LBB14_45
.Lfunc_end14:
	.size	_ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_, .Lfunc_end14-_ZSt15__copy_move_ditILb0EmRKmPS0_St15_Deque_iteratorImRmPmEET3_S3_IT0_T1_T2_ESB_S7_
	.cfi_endproc
                                        # -- End function
	.section	.text._ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_,"axG",@progbits,_ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_,comdat
	.weak	_ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_ # -- Begin function _ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_
	.p2align	4, 0x90
	.type	_ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_,@function
_ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_: # @_ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_
	.cfi_startproc
# %bb.0:
	push	r15
	.cfi_def_cfa_offset 16
	push	r14
	.cfi_def_cfa_offset 24
	push	rbx
	.cfi_def_cfa_offset 32
	.cfi_offset rbx, -32
	.cfi_offset r14, -24
	.cfi_offset r15, -16
	mov	rbx, rdi
	mov	rdi, qword ptr [rdi + 64]
	test	rdi, rdi
	je	.LBB15_6
# %bb.1:
	mov	r15, qword ptr [rbx + 136]
	mov	r14, qword ptr [rbx + 104]
	lea	rax, [r15 + 8]
	cmp	r14, rax
	jae	.LBB15_5
# %bb.2:
	add	r14, -8
	.p2align	4, 0x90
.LBB15_3:                               # =>This Inner Loop Header: Depth=1
	mov	rdi, qword ptr [r14 + 8]
	add	r14, 8
	call	_ZdlPv@PLT
	cmp	r14, r15
	jb	.LBB15_3
# %bb.4:
	mov	rdi, qword ptr [rbx + 64]
.LBB15_5:
	call	_ZdlPv@PLT
.LBB15_6:
	mov	rdi, qword ptr [rbx + 40]
	test	rdi, rdi
	je	.LBB15_8
# %bb.7:
	call	_ZdlPv@PLT
.LBB15_8:
	mov	rdi, qword ptr [rbx + 8]
	add	rbx, 24
	cmp	rdi, rbx
	je	.LBB15_9
# %bb.10:
	pop	rbx
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	jmp	_ZdlPv@PLT                      # TAILCALL
.LBB15_9:
	.cfi_def_cfa_offset 32
	pop	rbx
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	ret
.Lfunc_end15:
	.size	_ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_, .Lfunc_end15-_ZSt10destroy_atIN7hotpath9episode177godbolt19SessionWithStdQueueEEvPT_
	.cfi_endproc
                                        # -- End function
	.section	.text._ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_,"axG",@progbits,_ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_,comdat
	.weak	_ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_ # -- Begin function _ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_
	.p2align	4, 0x90
	.type	_ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_,@function
_ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_: # @_ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_
	.cfi_startproc
# %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	push	r15
	.cfi_def_cfa_offset 24
	push	r14
	.cfi_def_cfa_offset 32
	push	r13
	.cfi_def_cfa_offset 40
	push	r12
	.cfi_def_cfa_offset 48
	push	rbx
	.cfi_def_cfa_offset 56
	sub	rsp, 40
	.cfi_def_cfa_offset 96
	.cfi_offset rbx, -56
	.cfi_offset r12, -48
	.cfi_offset r13, -40
	.cfi_offset r14, -32
	.cfi_offset r15, -24
	.cfi_offset rbp, -16
	mov	rbx, qword ptr [rdi + 8]
	mov	r12, qword ptr [rdi]
	movabs	rcx, 9223372036854775776
	mov	rax, rbx
	sub	rax, r12
	cmp	rax, rcx
	je	.LBB16_22
# %bb.1:
	sar	rax, 5
	movabs	rcx, -6148914691236517205
	mov	r13, rdx
	mov	r15, rsi
	mov	qword ptr [rsp + 32], rdi       # 8-byte Spill
	movabs	rsi, 96076792050570581
	mov	r14, r15
	imul	rax, rcx
	cmp	rax, 1
	mov	rdx, rax
	adc	rdx, 0
	lea	rdi, [rdx + rax]
	cmp	rdi, rsi
	cmovae	rdi, rsi
	add	rdx, rax
	cmovb	rdi, rsi
	sub	r14, r12
	sar	r14, 5
	mov	qword ptr [rsp + 24], rdi       # 8-byte Spill
	imul	r14, rcx
	test	rdi, rdi
	je	.LBB16_2
# %bb.3:
	mov	rax, rdi
	shl	rax, 5
	lea	rdi, [rax + 2*rax]
	call	_Znwm@PLT
	mov	rbp, rax
	jmp	.LBB16_4
.LBB16_2:
	xor	ebp, ebp
.LBB16_4:
	lea	r14, [r14 + 2*r14]
	mov	rax, qword ptr [r13]
	lea	rcx, [r13 + 24]
	mov	qword ptr [rsp + 16], r12       # 8-byte Spill
	shl	r14, 5
	lea	rdi, [rbp + r14 + 24]
	mov	qword ptr [rbp + r14], rax
	mov	qword ptr [rbp + r14 + 8], rdi
	mov	rsi, qword ptr [r13 + 8]
	cmp	rsi, rcx
	je	.LBB16_5
# %bb.6:
	lea	rax, [rbp + r14 + 8]
	mov	qword ptr [rax], rsi
	mov	rax, qword ptr [r13 + 24]
	mov	qword ptr [rdi], rax
	mov	r12, qword ptr [r13 + 16]
	jmp	.LBB16_7
.LBB16_5:
	mov	r12, qword ptr [r13 + 16]
	mov	qword ptr [rsp + 8], rcx        # 8-byte Spill
	lea	rdx, [r12 + 1]
	call	memcpy@PLT
	mov	rcx, qword ptr [rsp + 8]        # 8-byte Reload
.LBB16_7:
	mov	qword ptr [rbp + r14 + 16], r12
	mov	qword ptr [r13 + 8], rcx
	mov	qword ptr [r13 + 16], 0
	mov	byte ptr [r13 + 24], 0
	vxorps	xmm3, xmm3, xmm3
	mov	qword ptr [rsp + 8], rbp        # 8-byte Spill
	vbroadcastsd	ymm0, qword ptr [r13 + 56]
	vblendps	ymm0, ymm0, ymmword ptr [r13 + 40], 207 # ymm0 = mem[0,1,2,3],ymm0[4,5],mem[6,7]
	vmovups	xmmword ptr [r13 + 40], xmm3
	mov	qword ptr [r13 + 56], 0
	vbroadcastsd	ymm1, qword ptr [r13 + 64]
	mov	qword ptr [r13 + 64], 0
	vblendps	ymm0, ymm0, ymm1, 192           # ymm0 = ymm0[0,1,2,3,4,5],ymm1[6,7]
	vmovups	ymmword ptr [rbp + r14 + 40], ymm0
	vmovups	xmm0, xmmword ptr [r13 + 72]
	vmovups	xmmword ptr [r13 + 72], xmm3
	vmovups	xmmword ptr [rbp + r14 + 72], xmm0
	mov	rax, qword ptr [r13 + 88]
	mov	qword ptr [r13 + 88], 0
	mov	r13, rbp
	mov	qword ptr [rbp + r14 + 88], rax
	cmp	qword ptr [rsp + 16], r15       # 8-byte Folded Reload
	je	.LBB16_13
# %bb.8:
	mov	r13, qword ptr [rsp + 8]        # 8-byte Reload
	mov	r14, qword ptr [rsp + 16]       # 8-byte Reload
	jmp	.LBB16_9
	.p2align	4, 0x90
.LBB16_11:                              #   in Loop: Header=BB16_9 Depth=1
	mov	qword ptr [r13 + 8], rsi
	mov	rax, qword ptr [r14 + 24]
	mov	qword ptr [r13 + 24], rax
	mov	r12, qword ptr [r14 + 16]
.LBB16_12:                              #   in Loop: Header=BB16_9 Depth=1
	mov	qword ptr [r13 + 16], r12
	mov	qword ptr [r14 + 8], rbp
	mov	qword ptr [r14 + 16], 0
	mov	byte ptr [r14 + 24], 0
	vmovups	xmm0, xmmword ptr [r14 + 40]
	vmovups	xmmword ptr [r14 + 40], xmm3
	vbroadcastsd	ymm1, qword ptr [r14 + 56]
	vbroadcastsd	ymm2, qword ptr [r14 + 64]
	vmovups	xmmword ptr [r14 + 56], xmm3
	vblendps	ymm0, ymm0, ymm1, 48            # ymm0 = ymm0[0,1,2,3],ymm1[4,5],ymm0[6,7]
	vblendps	ymm0, ymm0, ymm2, 192           # ymm0 = ymm0[0,1,2,3,4,5],ymm2[6,7]
	vmovups	ymmword ptr [r13 + 40], ymm0
	vmovups	xmm0, xmmword ptr [r14 + 72]
	vmovups	xmmword ptr [r14 + 72], xmm3
	vmovups	xmmword ptr [r13 + 72], xmm0
	mov	rax, qword ptr [r14 + 88]
	mov	qword ptr [r14 + 88], 0
	add	r14, 96
	mov	qword ptr [r13 + 88], rax
	add	r13, 96
	cmp	r14, r15
	je	.LBB16_13
.LBB16_9:                               # =>This Inner Loop Header: Depth=1
	mov	rax, qword ptr [r14]
	lea	rdi, [r13 + 24]
	lea	rbp, [r14 + 24]
	mov	qword ptr [r13], rax
	mov	qword ptr [r13 + 8], rdi
	mov	rsi, qword ptr [r14 + 8]
	cmp	rbp, rsi
	jne	.LBB16_11
# %bb.10:                               #   in Loop: Header=BB16_9 Depth=1
	mov	r12, qword ptr [r14 + 16]
	lea	rdx, [r12 + 1]
	vzeroupper
	call	memcpy@PLT
	vxorps	xmm3, xmm3, xmm3
	jmp	.LBB16_12
.LBB16_13:
	add	r13, 96
	cmp	rbx, r15
	je	.LBB16_19
# %bb.14:
	vxorps	xmm3, xmm3, xmm3
	jmp	.LBB16_15
	.p2align	4, 0x90
.LBB16_17:                              #   in Loop: Header=BB16_15 Depth=1
	mov	qword ptr [r13 + 8], rsi
	mov	rax, qword ptr [r15 + 24]
	mov	qword ptr [r13 + 24], rax
	mov	r12, qword ptr [r15 + 16]
.LBB16_18:                              #   in Loop: Header=BB16_15 Depth=1
	mov	qword ptr [r13 + 16], r12
	mov	qword ptr [r15 + 8], r14
	mov	qword ptr [r15 + 16], 0
	mov	byte ptr [r15 + 24], 0
	vmovups	xmm0, xmmword ptr [r15 + 40]
	vmovups	xmmword ptr [r15 + 40], xmm3
	vbroadcastsd	ymm1, qword ptr [r15 + 56]
	vbroadcastsd	ymm2, qword ptr [r15 + 64]
	vmovups	xmmword ptr [r15 + 56], xmm3
	vblendps	ymm0, ymm0, ymm1, 48            # ymm0 = ymm0[0,1,2,3],ymm1[4,5],ymm0[6,7]
	vblendps	ymm0, ymm0, ymm2, 192           # ymm0 = ymm0[0,1,2,3,4,5],ymm2[6,7]
	vmovups	ymmword ptr [r13 + 40], ymm0
	vmovups	xmm0, xmmword ptr [r15 + 72]
	vmovups	xmmword ptr [r15 + 72], xmm3
	vmovups	xmmword ptr [r13 + 72], xmm0
	mov	rax, qword ptr [r15 + 88]
	mov	qword ptr [r15 + 88], 0
	add	r15, 96
	mov	qword ptr [r13 + 88], rax
	add	r13, 96
	cmp	r15, rbx
	je	.LBB16_19
.LBB16_15:                              # =>This Inner Loop Header: Depth=1
	mov	rax, qword ptr [r15]
	lea	rdi, [r13 + 24]
	lea	r14, [r15 + 24]
	mov	qword ptr [r13], rax
	mov	qword ptr [r13 + 8], rdi
	mov	rsi, qword ptr [r15 + 8]
	cmp	r14, rsi
	jne	.LBB16_17
# %bb.16:                               #   in Loop: Header=BB16_15 Depth=1
	mov	r12, qword ptr [r15 + 16]
	lea	rdx, [r12 + 1]
	vzeroupper
	call	memcpy@PLT
	vxorps	xmm3, xmm3, xmm3
	jmp	.LBB16_18
.LBB16_19:
	mov	rdi, qword ptr [rsp + 16]       # 8-byte Reload
	test	rdi, rdi
	je	.LBB16_21
# %bb.20:
	vzeroupper
	call	_ZdlPv@PLT
.LBB16_21:
	mov	rax, qword ptr [rsp + 24]       # 8-byte Reload
	mov	rdx, qword ptr [rsp + 8]        # 8-byte Reload
	mov	rcx, qword ptr [rsp + 32]       # 8-byte Reload
	lea	rax, [rax + 2*rax]
	mov	qword ptr [rcx], rdx
	mov	qword ptr [rcx + 8], r13
	shl	rax, 5
	add	rax, rdx
	mov	qword ptr [rcx + 16], rax
	add	rsp, 40
	.cfi_def_cfa_offset 56
	pop	rbx
	.cfi_def_cfa_offset 48
	pop	r12
	.cfi_def_cfa_offset 40
	pop	r13
	.cfi_def_cfa_offset 32
	pop	r14
	.cfi_def_cfa_offset 24
	pop	r15
	.cfi_def_cfa_offset 16
	pop	rbp
	.cfi_def_cfa_offset 8
	vzeroupper
	ret
.LBB16_22:
	.cfi_def_cfa_offset 96
	lea	rdi, [rip + .L.str]
	call	_ZSt20__throw_length_errorPKc@PLT
.Lfunc_end16:
	.size	_ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_, .Lfunc_end16-_ZNSt6vectorIN7hotpath9episode177godbolt20SessionWithRingQueueESaIS3_EE17_M_realloc_insertIJS3_EEEvN9__gnu_cxx17__normal_iteratorIPS3_S5_EEDpOT_
	.cfi_endproc
                                        # -- End function
	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"vector::_M_realloc_insert"
	.size	.L.str, 26

	.type	.L.str.1,@object                # @.str.1
.L.str.1:
	.asciz	"basic_string::_M_create"
	.size	.L.str.1, 24

	.section	".linker-options","e",@llvm_linker_options
	.hidden	DW.ref.__gxx_personality_v0
	.weak	DW.ref.__gxx_personality_v0
	.section	.data.DW.ref.__gxx_personality_v0,"awG",@progbits,DW.ref.__gxx_personality_v0,comdat
	.p2align	3, 0x0
	.type	DW.ref.__gxx_personality_v0,@object
	.size	DW.ref.__gxx_personality_v0, 8
DW.ref.__gxx_personality_v0:
	.quad	__gxx_personality_v0
	.ident	"Ubuntu clang version 18.1.3 (1ubuntu1)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym __gxx_personality_v0
	.addrsig_sym _Unwind_Resume

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

# Hot Path C++ Episode 12: Annotated Intel-Syntax x86-64 Disassembly
# Generated with: g++ -std=c++20 -O3 -march=native -S -masm=intel

	.file	"godbolt_source.cc"
	.intel_syntax noprefix
	.text
	.section	.text._ZNSt3pmr25monotonic_buffer_resource11do_allocateEmm,"axG",@progbits,_ZNSt3pmr25monotonic_buffer_resource11do_allocateEmm,comdat
	.align 2
	.p2align 4
	.weak	_ZNSt3pmr25monotonic_buffer_resource11do_allocateEmm
	.type	_ZNSt3pmr25monotonic_buffer_resource11do_allocateEmm, @function
_ZNSt3pmr25monotonic_buffer_resource11do_allocateEmm:
.LFB2099:
	endbr64
	push	rbp
.LCFI0:
	push	rbx
.LCFI1:
	mov	eax, 1
	mov	rbx, rdi
	sub	rsp, 8
.LCFI2:
	mov	rcx, QWORD PTR 16[rdi]
	test	rsi, rsi
	cmovne	rax, rsi
	mov	rbp, rax
	cmp	rcx, rax
	jb	.L4
	mov	rsi, QWORD PTR 8[rdi]
	mov	rdi, rdx
	mov	r8, rcx
	neg	rdi
	sub	r8, rbp
	lea	rax, -1[rsi+rdx]
	and	rax, rdi
	mov	rdi, rax
	sub	rdi, rsi
	cmp	r8, rdi
	jb	.L4
	add	rcx, rsi
	mov	QWORD PTR 8[rbx], rax
	sub	rcx, rax
	mov	QWORD PTR 16[rbx], rcx
	test	rax, rax
	je	.L4
.L3:
	lea	rdx, [rax+rbp]
	sub	rcx, rbp
	mov	QWORD PTR 8[rbx], rdx
	mov	QWORD PTR 16[rbx], rcx
	add	rsp, 8
.LCFI3:
	pop	rbx
.LCFI4:
	pop	rbp
.LCFI5:
	ret
	.p2align 4
	.p2align 3
.L4:
.LCFI6:
	mov	rsi, rbp
	mov	rdi, rbx
	call	_ZNSt3pmr25monotonic_buffer_resource13_M_new_bufferEmm@PLT
	mov	rax, QWORD PTR 8[rbx]
	mov	rcx, QWORD PTR 16[rbx]
	jmp	.L3
.LFE2099:
	.size	_ZNSt3pmr25monotonic_buffer_resource11do_allocateEmm, .-_ZNSt3pmr25monotonic_buffer_resource11do_allocateEmm
	.text
	.p2align 4
	.globl	_ZN7hotpath12godbolt_ep1211AllocStdNewEml
	.type	_ZN7hotpath12godbolt_ep1211AllocStdNewEml, @function
_ZN7hotpath12godbolt_ep1211AllocStdNewEml:
.LFB2105:
	endbr64
	push	rbp
.LCFI7:
	push	rbx
.LCFI8:
	mov	rbp, rdi
	mov	edi, 32
	sub	rsp, 8
.LCFI9:
	mov	rbx, rsi
	call	_Znwm@PLT
	mov	rdx, QWORD PTR .LC0[rip]
	mov	QWORD PTR [rax], rbp
	mov	QWORD PTR 8[rax], rbx
	mov	QWORD PTR 24[rax], 0
	mov	QWORD PTR 16[rax], rdx
	add	rsp, 8
.LCFI10:
	pop	rbx
.LCFI11:
	pop	rbp
.LCFI12:
	ret
.LFE2105:
	.size	_ZN7hotpath12godbolt_ep1211AllocStdNewEml, .-_ZN7hotpath12godbolt_ep1211AllocStdNewEml
	.p2align 4
	.globl	_ZN7hotpath12godbolt_ep1214AllocBumpArenaERNS0_13BumpArenaViewEml
	.type	_ZN7hotpath12godbolt_ep1214AllocBumpArenaERNS0_13BumpArenaViewEml, @function
_ZN7hotpath12godbolt_ep1214AllocBumpArenaERNS0_13BumpArenaViewEml:
.LFB2106:
	endbr64
	mov	rax, QWORD PTR [rdi]
	lea	rcx, 32[rax]
	cmp	QWORD PTR 8[rdi], rcx
	jb	.L15
	mov	QWORD PTR [rdi], rcx
	test	rax, rax
	je	.L12
	mov	QWORD PTR 8[rax], rdx
	mov	rdx, QWORD PTR .LC0[rip]
	mov	QWORD PTR [rax], rsi
	mov	QWORD PTR 24[rax], 0
	mov	QWORD PTR 16[rax], rdx
	ret
	.p2align 4
	.p2align 3
.L15:
	xor	eax, eax
.L12:
	ret
.LFE2106:
	.size	_ZN7hotpath12godbolt_ep1214AllocBumpArenaERNS0_13BumpArenaViewEml, .-_ZN7hotpath12godbolt_ep1214AllocBumpArenaERNS0_13BumpArenaViewEml
	.p2align 4
	.globl	_ZN7hotpath12godbolt_ep1213AllocSlabPoolERNS0_12SlabPoolViewEml
	.type	_ZN7hotpath12godbolt_ep1213AllocSlabPoolERNS0_12SlabPoolViewEml, @function
_ZN7hotpath12godbolt_ep1213AllocSlabPoolERNS0_12SlabPoolViewEml:
.LFB2107:
	endbr64
	mov	rax, QWORD PTR [rdi]
	test	rax, rax
	je	.L19
	mov	rcx, QWORD PTR [rax]
	mov	QWORD PTR [rdi], rcx
	mov	QWORD PTR 8[rax], rdx
	mov	rdx, QWORD PTR .LC0[rip]
	mov	QWORD PTR [rax], rsi
	mov	QWORD PTR 24[rax], 0
	mov	QWORD PTR 16[rax], rdx
.L19:
	ret
.LFE2107:
	.size	_ZN7hotpath12godbolt_ep1213AllocSlabPoolERNS0_12SlabPoolViewEml, .-_ZN7hotpath12godbolt_ep1213AllocSlabPoolERNS0_12SlabPoolViewEml
	.p2align 4
	.globl	_ZN7hotpath12godbolt_ep1216AllocPmrResourceEPNSt3pmr15memory_resourceE
	.type	_ZN7hotpath12godbolt_ep1216AllocPmrResourceEPNSt3pmr15memory_resourceE, @function
_ZN7hotpath12godbolt_ep1216AllocPmrResourceEPNSt3pmr15memory_resourceE:
.LFB2108:
	endbr64
	push	rbx
.LCFI13:
	mov	rax, QWORD PTR [rdi]
	lea	rdx, _ZNSt3pmr25monotonic_buffer_resource11do_allocateEmm[rip]
	mov	rbx, rdi
	mov	rax, QWORD PTR 16[rax]
	cmp	rax, rdx
	jne	.L25
	mov	rdx, QWORD PTR 16[rdi]
	cmp	rdx, 31
	jbe	.L28
	mov	rcx, QWORD PTR 8[rdi]
	lea	rdi, -32[rdx]
	lea	rax, 7[rcx]
	and	rax, -8
	mov	rsi, rax
	sub	rsi, rcx
	cmp	rdi, rsi
	jb	.L28
	add	rdx, rcx
	mov	QWORD PTR 8[rbx], rax
	sub	rdx, rax
	mov	QWORD PTR 16[rbx], rdx
	test	rax, rax
	je	.L28
.L27:
	lea	rcx, 32[rax]
	sub	rdx, 32
	mov	QWORD PTR 8[rbx], rcx
	mov	QWORD PTR 16[rbx], rdx
	pop	rbx
.LCFI14:
	ret
	.p2align 4
	.p2align 3
.L28:
.LCFI15:
	mov	edx, 8
	mov	esi, 32
	mov	rdi, rbx
	call	_ZNSt3pmr25monotonic_buffer_resource13_M_new_bufferEmm@PLT
	mov	rax, QWORD PTR 8[rbx]
	mov	rdx, QWORD PTR 16[rbx]
	jmp	.L27
	.p2align 4
	.p2align 3
.L25:
	pop	rbx
.LCFI16:
	mov	edx, 8
	mov	esi, 32
	jmp	rax
.LFE2108:
	.size	_ZN7hotpath12godbolt_ep1216AllocPmrResourceEPNSt3pmr15memory_resourceE, .-_ZN7hotpath12godbolt_ep1216AllocPmrResourceEPNSt3pmr15memory_resourceE
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC0:
	.long	100
	.long	1
	.section	.eh_frame,"a",@progbits
.Lframe1:
	.long	.LECIE1-.LSCIE1
.LSCIE1:
	.long	0
	.byte	0x3
	.string	"zR"
	.uleb128 0x1
	.sleb128 -8
	.uleb128 0x10
	.uleb128 0x1
	.byte	0x1b
	.byte	0xc
	.uleb128 0x7
	.uleb128 0x8
	.byte	0x90
	.uleb128 0x1
	.align 8
.LECIE1:
.LSFDE1:
	.long	.LEFDE1-.LASFDE1
.LASFDE1:
	.long	.LASFDE1-.Lframe1
	.long	.LFB2099-.
	.long	.LFE2099-.LFB2099
	.uleb128 0
	.byte	0x4
	.long	.LCFI0-.LFB2099
	.byte	0xe
	.uleb128 0x10
	.byte	0x86
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI1-.LCFI0
	.byte	0xe
	.uleb128 0x18
	.byte	0x83
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI2-.LCFI1
	.byte	0xe
	.uleb128 0x20
	.byte	0x4
	.long	.LCFI3-.LCFI2
	.byte	0xa
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI4-.LCFI3
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI5-.LCFI4
	.byte	0xe
	.uleb128 0x8
	.byte	0x4
	.long	.LCFI6-.LCFI5
	.byte	0xb
	.align 8
.LEFDE1:
.LSFDE3:
	.long	.LEFDE3-.LASFDE3
.LASFDE3:
	.long	.LASFDE3-.Lframe1
	.long	.LFB2105-.
	.long	.LFE2105-.LFB2105
	.uleb128 0
	.byte	0x4
	.long	.LCFI7-.LFB2105
	.byte	0xe
	.uleb128 0x10
	.byte	0x86
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI8-.LCFI7
	.byte	0xe
	.uleb128 0x18
	.byte	0x83
	.uleb128 0x3
	.byte	0x4
	.long	.LCFI9-.LCFI8
	.byte	0xe
	.uleb128 0x20
	.byte	0x4
	.long	.LCFI10-.LCFI9
	.byte	0xe
	.uleb128 0x18
	.byte	0x4
	.long	.LCFI11-.LCFI10
	.byte	0xe
	.uleb128 0x10
	.byte	0x4
	.long	.LCFI12-.LCFI11
	.byte	0xe
	.uleb128 0x8
	.align 8
.LEFDE3:
.LSFDE5:
	.long	.LEFDE5-.LASFDE5
.LASFDE5:
	.long	.LASFDE5-.Lframe1
	.long	.LFB2108-.
	.long	.LFE2108-.LFB2108
	.uleb128 0
	.byte	0x4
	.long	.LCFI13-.LFB2108
	.byte	0xe
	.uleb128 0x10
	.byte	0x83
	.uleb128 0x2
	.byte	0x4
	.long	.LCFI14-.LCFI13
	.byte	0xa
	.byte	0xe
	.uleb128 0x8
	.byte	0x4
	.long	.LCFI15-.LCFI14
	.byte	0xb
	.byte	0x4
	.long	.LCFI16-.LCFI15
	.byte	0xe
	.uleb128 0x8
	.align 8
.LEFDE5:
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

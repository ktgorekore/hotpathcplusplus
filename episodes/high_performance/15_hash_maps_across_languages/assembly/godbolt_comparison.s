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
# EPISODE 15: HASH MAPS ACROSS LANGUAGES — INTEL-SYNTAX DISASSEMBLY
# ==============================================================================

# --- 1. Godbolt Self-Contained Comparison (Clang 18 -O3 -march=znver4) ---
	.text
	.intel_syntax noprefix
	.file	"godbolt_source.cc"
	.globl	_ZN7hotpath12godbolt_ep1519FindStdUnorderedMapERKSt13unordered_mapImmSt4hashImESt8equal_toImESaISt4pairIKmmEEEm # -- Begin function _ZN7hotpath12godbolt_ep1519FindStdUnorderedMapERKSt13unordered_mapImmSt4hashImESt8equal_toImESaISt4pairIKmmEEEm
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1519FindStdUnorderedMapERKSt13unordered_mapImmSt4hashImESt8equal_toImESaISt4pairIKmmEEEm,@function
_ZN7hotpath12godbolt_ep1519FindStdUnorderedMapERKSt13unordered_mapImmSt4hashImESt8equal_toImESaISt4pairIKmmEEEm: # @_ZN7hotpath12godbolt_ep1519FindStdUnorderedMapERKSt13unordered_mapImmSt4hashImESt8equal_toImESaISt4pairIKmmEEEm
	.cfi_startproc
# %bb.0:
	cmp	qword ptr [rdi + 24], 0
	je	.LBB0_1
# %bb.5:
	mov	r10, qword ptr [rdi + 8]
	mov	r9, qword ptr [rdi]
	mov	rax, rsi
	xor	edx, edx
	xor	ecx, ecx
	div	r10
	mov	rax, qword ptr [r9 + 8*rdx]
	test	rax, rax
	je	.LBB0_11
# %bb.6:
	mov	rdi, qword ptr [rax]
	cmp	qword ptr [rdi + 8], rsi
	je	.LBB0_4
# %bb.7:
	mov	r8, rdx
	xor	ecx, ecx
	.p2align	4, 0x90
.LBB0_9:                                # =>This Inner Loop Header: Depth=1
	mov	rdi, qword ptr [rdi]
	test	rdi, rdi
	je	.LBB0_11
# %bb.10:                               #   in Loop: Header=BB0_9 Depth=1
	mov	r9, qword ptr [rdi + 8]
	xor	edx, edx
	mov	rax, r9
	div	r10
	cmp	rdx, r8
	jne	.LBB0_11
# %bb.8:                                #   in Loop: Header=BB0_9 Depth=1
	cmp	r9, rsi
	jne	.LBB0_9
	jmp	.LBB0_4
.LBB0_1:
	add	rdi, 16
	xor	ecx, ecx
	.p2align	4, 0x90
.LBB0_2:                                # =>This Inner Loop Header: Depth=1
	mov	rdi, qword ptr [rdi]
	test	rdi, rdi
	je	.LBB0_11
# %bb.3:                                #   in Loop: Header=BB0_2 Depth=1
	cmp	qword ptr [rdi + 8], rsi
	jne	.LBB0_2
.LBB0_4:
	mov	rax, qword ptr [rdi + 16]
	ret
.LBB0_11:
	mov	rax, rcx
	ret
.Lfunc_end0:
	.size	_ZN7hotpath12godbolt_ep1519FindStdUnorderedMapERKSt13unordered_mapImmSt4hashImESt8equal_toImESaISt4pairIKmmEEEm, .Lfunc_end0-_ZN7hotpath12godbolt_ep1519FindStdUnorderedMapERKSt13unordered_mapImmSt4hashImESt8equal_toImESaISt4pairIKmmEEEm
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst16,"aM",@progbits,16
	.p2align	4, 0x0                          # -- Begin function _ZN7hotpath12godbolt_ep1517ProbeSwissGroup16EPKaa
.LCPI1_0:
	.zero	16,128
	.text
	.globl	_ZN7hotpath12godbolt_ep1517ProbeSwissGroup16EPKaa
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1517ProbeSwissGroup16EPKaa,@function
_ZN7hotpath12godbolt_ep1517ProbeSwissGroup16EPKaa: # @_ZN7hotpath12godbolt_ep1517ProbeSwissGroup16EPKaa
	.cfi_startproc
# %bb.0:
	vmovdqu	xmm0, xmmword ptr [rdi]
	vpbroadcastb	xmm1, esi
	xor	ecx, ecx
	vpcmpeqb	k1, xmm0, xmmword ptr [rip + .LCPI1_0]
	vpcmpeqb	k0, xmm0, xmm1
	kortestw	k1, k1
	kmovw	eax, k0
	setne	cl
	shl	rcx, 32
	or	rax, rcx
	ret
.Lfunc_end1:
	.size	_ZN7hotpath12godbolt_ep1517ProbeSwissGroup16EPKaa, .Lfunc_end1-_ZN7hotpath12godbolt_ep1517ProbeSwissGroup16EPKaa
	.cfi_endproc
                                        # -- End function
	.globl	_ZN7hotpath12godbolt_ep1517NextCandidateSlotERj # -- Begin function _ZN7hotpath12godbolt_ep1517NextCandidateSlotERj
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1517NextCandidateSlotERj,@function
_ZN7hotpath12godbolt_ep1517NextCandidateSlotERj: # @_ZN7hotpath12godbolt_ep1517NextCandidateSlotERj
# %bb.0:
	mov	ecx, dword ptr [rdi]
	tzcnt	eax, ecx
	blsr	ecx, ecx
	mov	dword ptr [rdi], ecx
	ret
.Lfunc_end2:
	.size	_ZN7hotpath12godbolt_ep1517NextCandidateSlotERj, .Lfunc_end2-_ZN7hotpath12godbolt_ep1517NextCandidateSlotERj
                                        # -- End function
	.globl	_ZN7hotpath12godbolt_ep159HashFxU64Em # -- Begin function _ZN7hotpath12godbolt_ep159HashFxU64Em
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep159HashFxU64Em,@function
_ZN7hotpath12godbolt_ep159HashFxU64Em:  # @_ZN7hotpath12godbolt_ep159HashFxU64Em
# %bb.0:
	movabs	rax, 5871781006564002453
	imul	rax, rdi
	ret
.Lfunc_end3:
	.size	_ZN7hotpath12godbolt_ep159HashFxU64Em, .Lfunc_end3-_ZN7hotpath12godbolt_ep159HashFxU64Em
                                        # -- End function
	.globl	_ZN7hotpath12godbolt_ep1511SipRoundMixEmmmm # -- Begin function _ZN7hotpath12godbolt_ep1511SipRoundMixEmmmm
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1511SipRoundMixEmmmm,@function
_ZN7hotpath12godbolt_ep1511SipRoundMixEmmmm: # @_ZN7hotpath12godbolt_ep1511SipRoundMixEmmmm
# %bb.0:
	add	rdx, rcx
	rorx	rcx, rcx, 48
	add	rdi, rsi
	rorx	rax, rsi, 51
	xor	rcx, rdx
	xor	rax, rdi
	rorx	rcx, rcx, 43
	rorx	rsi, rax, 47
	add	rdx, rax
	xor	rsi, rcx
	rorx	rax, rdx, 32
	xor	rax, rsi
	xor	rax, rdx
	ret
.Lfunc_end4:
	.size	_ZN7hotpath12godbolt_ep1511SipRoundMixEmmmm, .Lfunc_end4-_ZN7hotpath12godbolt_ep1511SipRoundMixEmmmm
                                        # -- End function
	.section	".linker-options","e",@llvm_linker_options
	.ident	"Ubuntu clang version 18.1.3 (1ubuntu1)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym __gxx_personality_v0

# --- 2. C++20 std::unordered_map Lookup Kernel (GCC 13 -O3 -march=native) ---

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/15_hash_maps_across_languages/challenge:	file format elf64-x86-64

Disassembly of section .text:

0000000000005f80 <ep15_std_unordered_find_packed>:
    5f80: f3 0f 1e fa                  	endbr64
    5f84: 48 85 d2                     	test	rdx, rdx
    5f87: 0f 84 df 00 00 00            	je	0x606c <ep15_std_unordered_find_packed+0xec>
    5f8d: 55                           	push	rbp
    5f8e: 49 89 fa                     	mov	r10, rdi
    5f91: 45 31 db                     	xor	r11d, r11d
    5f94: 48 89 e5                     	mov	rbp, rsp
    5f97: 41 57                        	push	r15
    5f99: 41 56                        	push	r14
    5f9b: 41 55                        	push	r13
    5f9d: 41 54                        	push	r12
    5f9f: 53                           	push	rbx
    5fa0: 4c 8b 7f 18                  	mov	r15, qword ptr [rdi + 0x18]
    5fa4: 4c 8d 2c d6                  	lea	r13, [rsi + 8*rdx]
    5fa8: 45 31 e4                     	xor	r12d, r12d
    5fab: 0f 1f 44 00 00               	nop	dword ptr [rax + rax]
    5fb0: 48 8b 0e                     	mov	rcx, qword ptr [rsi]
    5fb3: 4d 85 ff                     	test	r15, r15
    5fb6: 75 58                        	jne	0x6010 <ep15_std_unordered_find_packed+0x90>
    5fb8: 49 8b 52 10                  	mov	rdx, qword ptr [r10 + 0x10]
    5fbc: 48 85 d2                     	test	rdx, rdx
    5fbf: 75 17                        	jne	0x5fd8 <ep15_std_unordered_find_packed+0x58>
    5fc1: eb 22                        	jmp	0x5fe5 <ep15_std_unordered_find_packed+0x65>
    5fc3: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    5fce: 66 90                        	nop
    5fd0: 48 8b 12                     	mov	rdx, qword ptr [rdx]
    5fd3: 48 85 d2                     	test	rdx, rdx
    5fd6: 74 0d                        	je	0x5fe5 <ep15_std_unordered_find_packed+0x65>
    5fd8: 48 3b 4a 08                  	cmp	rcx, qword ptr [rdx + 0x8]
    5fdc: 75 f2                        	jne	0x5fd0 <ep15_std_unordered_find_packed+0x50>
    5fde: 4c 03 62 10                  	add	r12, qword ptr [rdx + 0x10]
    5fe2: 49 ff c3                     	inc	r11
    5fe5: 48 83 c6 08                  	add	rsi, 0x8
    5fe9: 49 39 f5                     	cmp	r13, rsi
    5fec: 75 c2                        	jne	0x5fb0 <ep15_std_unordered_find_packed+0x30>
    5fee: 48 b8 15 7c 4a 7f b9 79 37 9e	movabs	rax, -0x61c8864680b583eb
    5ff8: 5b                           	pop	rbx
    5ff9: 49 0f af c3                  	imul	rax, r11
    5ffd: 4c 31 e0                     	xor	rax, r12
    6000: 41 5c                        	pop	r12
    6002: 41 5d                        	pop	r13
    6004: 41 5e                        	pop	r14
    6006: 41 5f                        	pop	r15
    6008: 5d                           	pop	rbp
    6009: c3                           	ret
    600a: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
    6010: 4d 8b 4a 08                  	mov	r9, qword ptr [r10 + 0x8]
    6014: 48 89 c8                     	mov	rax, rcx
    6017: 31 d2                        	xor	edx, edx
    6019: 49 f7 f1                     	div	r9
    601c: 49 8b 02                     	mov	rax, qword ptr [r10]
    601f: 48 8b 1c d0                  	mov	rbx, qword ptr [rax + 8*rdx]
    6023: 49 89 d6                     	mov	r14, rdx
    6026: 48 85 db                     	test	rbx, rbx
    6029: 74 ba                        	je	0x5fe5 <ep15_std_unordered_find_packed+0x65>
    602b: 48 8b 13                     	mov	rdx, qword ptr [rbx]
    602e: 48 8b 7a 08                  	mov	rdi, qword ptr [rdx + 0x8]
    6032: 48 39 f9                     	cmp	rcx, rdi
    6035: 74 24                        	je	0x605b <ep15_std_unordered_find_packed+0xdb>
    6037: 4c 8b 02                     	mov	r8, qword ptr [rdx]
    603a: 4d 85 c0                     	test	r8, r8
    603d: 74 a6                        	je	0x5fe5 <ep15_std_unordered_find_packed+0x65>
    603f: 49 8b 78 08                  	mov	rdi, qword ptr [r8 + 0x8]
    6043: 48 89 d3                     	mov	rbx, rdx
    6046: 31 d2                        	xor	edx, edx
    6048: 48 89 f8                     	mov	rax, rdi
    604b: 49 f7 f1                     	div	r9
    604e: 49 39 d6                     	cmp	r14, rdx
    6051: 75 92                        	jne	0x5fe5 <ep15_std_unordered_find_packed+0x65>
    6053: 4c 89 c2                     	mov	rdx, r8
    6056: 48 39 f9                     	cmp	rcx, rdi
    6059: 75 dc                        	jne	0x6037 <ep15_std_unordered_find_packed+0xb7>
    605b: 48 8b 13                     	mov	rdx, qword ptr [rbx]
    605e: 48 85 d2                     	test	rdx, rdx
    6061: 0f 85 77 ff ff ff            	jne	0x5fde <ep15_std_unordered_find_packed+0x5e>
    6067: e9 79 ff ff ff               	jmp	0x5fe5 <ep15_std_unordered_find_packed+0x65>
    606c: 31 c0                        	xor	eax, eax
    606e: c3                           	ret
    606f: 90                           	nop

# --- 3. C++20 absl::flat_hash_map Lookup Kernel (GCC 13 -O3 -march=native) ---

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/15_hash_maps_across_languages/challenge:	file format elf64-x86-64

Disassembly of section .text:

0000000000006070 <ep15_absl_flat_find_packed>:
    6070: f3 0f 1e fa                  	endbr64
    6074: 55                           	push	rbp
    6075: 48 89 e5                     	mov	rbp, rsp
    6078: 41 57                        	push	r15
    607a: 41 56                        	push	r14
    607c: 41 55                        	push	r13
    607e: 41 54                        	push	r12
    6080: 53                           	push	rbx
    6081: 48 89 75 c8                  	mov	qword ptr [rbp - 0x38], rsi
    6085: 48 85 d2                     	test	rdx, rdx
    6088: 0f 84 92 01 00 00            	je	0x6220 <ep15_absl_flat_find_packed+0x1b0>
    608e: 48 89 f9                     	mov	rcx, rdi
    6091: 44 0f b6 1f                  	movzx	r11d, byte ptr [rdi]
    6095: 49 89 d2                     	mov	r10, rdx
    6098: bb 01 00 00 00               	mov	ebx, 0x1
    609d: 48 8b 31                     	mov	rsi, qword ptr [rcx]
    60a0: 45 31 c9                     	xor	r9d, r9d
    60a3: 31 ff                        	xor	edi, edi
    60a5: 4d 89 cf                     	mov	r15, r9
    60a8: c4 e2 a1 f7 db               	shlx	rbx, rbx, r11
    60ad: 48 8d 43 0f                  	lea	rax, [rbx + 0xf]
    60b1: 41 0f b6 db                  	movzx	ebx, r11b
    60b5: 48 89 f2                     	mov	rdx, rsi
    60b8: 48 89 45 c0                  	mov	qword ptr [rbp - 0x40], rax
    60bc: 49 89 de                     	mov	r14, rbx
    60bf: 31 c0                        	xor	eax, eax
    60c1: 48 c1 ee 11                  	shr	rsi, 0x11
    60c5: 0f b6 d6                     	movzx	edx, dh
    60c8: 48 89 75 b0                  	mov	qword ptr [rbp - 0x50], rsi
    60cc: 48 89 55 b8                  	mov	qword ptr [rbp - 0x48], rdx
    60d0: e9 a3 00 00 00               	jmp	0x6178 <ep15_absl_flat_find_packed+0x108>
    60d5: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    60e0: 48 8b 55 c8                  	mov	rdx, qword ptr [rbp - 0x38]
    60e4: 4a 8b 34 fa                  	mov	rsi, qword ptr [rdx + 8*r15]
    60e8: 4c 8b 45 b8                  	mov	r8, qword ptr [rbp - 0x48]
    60ec: 48 ba f5 8c 1e de e0 f9 d5 79	movabs	rdx, 0x79d5f9e0de1e8cf5
    60f6: 4c 8b 69 10                  	mov	r13, qword ptr [rcx + 0x10]
    60fa: 31 db                        	xor	ebx, ebx
    60fc: 49 31 f0                     	xor	r8, rsi
    60ff: c4 42 bb f6 c8               	mulx	r9, r8, r8
    6104: 4c 89 45 a0                  	mov	qword ptr [rbp - 0x60], r8
    6108: 4c 8b 45 a0                  	mov	r8, qword ptr [rbp - 0x60]
    610c: 4c 89 4d a8                  	mov	qword ptr [rbp - 0x58], r9
    6110: 4c 89 fa                     	mov	rdx, r15
    6113: 4d 31 c8                     	xor	r8, r9
    6116: c4 42 88 f5 c8               	bzhi	r9, r8, r14
    611b: 49 c1 e8 39                  	shr	r8, 0x39
    611f: 62 d2 7d 08 7a e8            	vpbroadcastb	xmm5, r8d
    6125: c4 81 7a 6f 1c 0c            	vmovdqu	xmm3, xmmword ptr [r12 + r9]
    612b: 4d 89 c8                     	mov	r8, r9
    612e: 49 c1 e0 04                  	shl	r8, 0x4
    6132: 43 0f 18 4c 05 00            	prefetcht0	byte ptr [r13 + r8]
    6138: c5 d1 74 e3                  	vpcmpeqb	xmm4, xmm5, xmm3
    613c: c5 79 d7 fc                  	vpmovmskb	r15d, xmm4
    6140: 45 85 ff                     	test	r15d, r15d
    6143: 0f 84 b7 00 00 00            	je	0x6200 <ep15_absl_flat_find_packed+0x190>
    6149: f3 45 0f bc c7               	tzcnt	r8d, r15d
    614e: 4d 01 c8                     	add	r8, r9
    6151: c4 42 88 f5 c0               	bzhi	r8, r8, r14
    6156: 49 c1 e0 04                  	shl	r8, 0x4
    615a: 4d 01 e8                     	add	r8, r13
    615d: 49 3b 30                     	cmp	rsi, qword ptr [r8]
    6160: 0f 85 8a 00 00 00            	jne	0x61f0 <ep15_absl_flat_find_packed+0x180>
    6166: 49 89 d7                     	mov	r15, rdx
    6169: 49 03 78 08                  	add	rdi, qword ptr [r8 + 0x8]
    616d: 48 ff c0                     	inc	rax
    6170: 49 ff c7                     	inc	r15
    6173: 4d 39 fa                     	cmp	r10, r15
    6176: 74 59                        	je	0x61d1 <ep15_absl_flat_find_packed+0x161>
    6178: 41 80 fb 01                  	cmp	r11b, 0x1
    617c: 76 32                        	jbe	0x61b0 <ep15_absl_flat_find_packed+0x140>
    617e: 4c 8b 61 08                  	mov	r12, qword ptr [rcx + 0x8]
    6182: 48 8b 5d c0                  	mov	rbx, qword ptr [rbp - 0x40]
    6186: 49 8d 34 1c                  	lea	rsi, [r12 + rbx]
    618a: 41 0f 18 1c 24               	prefetcht2	byte ptr [r12]
    618f: 48 39 f1                     	cmp	rcx, rsi
    6192: 0f 82 48 ff ff ff            	jb	0x60e0 <ep15_absl_flat_find_packed+0x70>
    6198: 48 8b 5d c8                  	mov	rbx, qword ptr [rbp - 0x38]
    619c: 4a 8b 34 fb                  	mov	rsi, qword ptr [rbx + 8*r15]
    61a0: e9 43 ff ff ff               	jmp	0x60e8 <ep15_absl_flat_find_packed+0x78>
    61a5: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    61b0: 48 83 7d b0 00               	cmp	qword ptr [rbp - 0x50], 0x0
    61b5: 74 b9                        	je	0x6170 <ep15_absl_flat_find_packed+0x100>
    61b7: 48 8b 5d c8                  	mov	rbx, qword ptr [rbp - 0x38]
    61bb: 4c 8d 41 08                  	lea	r8, [rcx + 0x8]
    61bf: 4a 8b 14 fb                  	mov	rdx, qword ptr [rbx + 8*r15]
    61c3: 48 39 51 08                  	cmp	qword ptr [rcx + 0x8], rdx
    61c7: 74 a0                        	je	0x6169 <ep15_absl_flat_find_packed+0xf9>
    61c9: 49 ff c7                     	inc	r15
    61cc: 4d 39 fa                     	cmp	r10, r15
    61cf: 75 a7                        	jne	0x6178 <ep15_absl_flat_find_packed+0x108>
    61d1: 48 ba 15 7c 4a 7f b9 79 37 9e	movabs	rdx, -0x61c8864680b583eb
    61db: 48 0f af c2                  	imul	rax, rdx
    61df: 5b                           	pop	rbx
    61e0: 48 31 f8                     	xor	rax, rdi
    61e3: 41 5c                        	pop	r12
    61e5: 41 5d                        	pop	r13
    61e7: 41 5e                        	pop	r14
    61e9: 41 5f                        	pop	r15
    61eb: 5d                           	pop	rbp
    61ec: c3                           	ret
    61ed: 0f 1f 00                     	nop	dword ptr [rax]
    61f0: c4 c2 00 f3 cf               	blsr	r15d, r15d
    61f5: 0f 85 4e ff ff ff            	jne	0x6149 <ep15_absl_flat_find_packed+0xd9>
    61fb: 0f 1f 44 00 00               	nop	dword ptr [rax + rax]
    6200: c4 e2 61 08 db               	vpsignb	xmm3, xmm3, xmm3
    6205: c5 79 d7 c3                  	vpmovmskb	r8d, xmm3
    6209: 45 85 c0                     	test	r8d, r8d
    620c: 74 1f                        	je	0x622d <ep15_absl_flat_find_packed+0x1bd>
    620e: 49 89 d7                     	mov	r15, rdx
    6211: e9 5a ff ff ff               	jmp	0x6170 <ep15_absl_flat_find_packed+0x100>
    6216: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
    6220: 5b                           	pop	rbx
    6221: 31 c0                        	xor	eax, eax
    6223: 41 5c                        	pop	r12
    6225: 41 5d                        	pop	r13
    6227: 41 5e                        	pop	r14
    6229: 41 5f                        	pop	r15
    622b: 5d                           	pop	rbp
    622c: c3                           	ret
    622d: 48 83 c3 10                  	add	rbx, 0x10
    6231: 49 01 d9                     	add	r9, rbx
    6234: c4 42 88 f5 c9               	bzhi	r9, r9, r14
    6239: e9 e7 fe ff ff               	jmp	0x6125 <ep15_absl_flat_find_packed+0xb5>
    623e: 66 90                        	nop

# --- 4. Rust 1.91 SipHash-1-3 vs FxHash Lookup Kernels ---

/tmp/ep15_build/bench_rust:	file format elf64-x86-64

Disassembly of section .text:

000000000001f460 <rust_siphash_find_packed>:
   1f460: 55                           	push	rbp
   1f461: 41 57                        	push	r15
   1f463: 41 56                        	push	r14
   1f465: 41 55                        	push	r13
   1f467: 41 54                        	push	r12
   1f469: 53                           	push	rbx
   1f46a: 48 85 d2                     	test	rdx, rdx
   1f46d: 0f 84 7c 02 00 00            	je	0x1f6ef <rust_siphash_find_packed+0x28f>
   1f473: 48 83 7f 18 00               	cmp	qword ptr [rdi + 0x18], 0x0
   1f478: 0f 84 71 02 00 00            	je	0x1f6ef <rust_siphash_find_packed+0x28f>
   1f47e: 48 8d 04 d6                  	lea	rax, [rsi + 8*rdx]
   1f482: 48 8b 57 28                  	mov	rdx, qword ptr [rdi + 0x28]
   1f486: 49 ba 6d 6f 64 6e 61 72 6f 64	movabs	r10, 0x646f72616e646f6d
   1f490: 49 b9 75 65 73 70 65 6d 6f 73	movabs	r9, 0x736f6d6570736575
   1f49a: 4c 8b 47 08                  	mov	r8, qword ptr [rdi + 0x8]
   1f49e: 49 bd 73 65 74 79 62 64 65 74	movabs	r13, 0x7465646279746573
   1f4a8: c5 f9 76 c0                  	vpcmpeqd	xmm0, xmm0, xmm0
   1f4ac: 31 db                        	xor	ebx, ebx
   1f4ae: 48 89 44 24 f0               	mov	qword ptr [rsp - 0x10], rax
   1f4b3: 48 8b 07                     	mov	rax, qword ptr [rdi]
   1f4b6: 49 31 d2                     	xor	r10, rdx
   1f4b9: 49 31 d5                     	xor	r13, rdx
   1f4bc: 48 8d 48 f0                  	lea	rcx, [rax - 0x10]
   1f4c0: 48 89 44 24 f8               	mov	qword ptr [rsp - 0x8], rax
   1f4c5: 48 83 c0 f8                  	add	rax, -0x8
   1f4c9: 48 89 44 24 d8               	mov	qword ptr [rsp - 0x28], rax
   1f4ce: 48 8b 47 20                  	mov	rax, qword ptr [rdi + 0x20]
   1f4d2: 48 bf 61 72 65 6e 65 67 79 6c	movabs	rdi, 0x6c7967656e657261
   1f4dc: 48 89 4c 24 e0               	mov	qword ptr [rsp - 0x20], rcx
   1f4e1: 49 31 c1                     	xor	r9, rax
   1f4e4: 48 31 c7                     	xor	rdi, rax
   1f4e7: 4d 01 d1                     	add	r9, r10
   1f4ea: c4 c3 fb f0 c1 20            	rorx	rax, r9, 0x20
   1f4f0: 48 89 44 24 e8               	mov	qword ptr [rsp - 0x18], rax
   1f4f5: c4 c3 fb f0 c2 33            	rorx	rax, r10, 0x33
   1f4fb: 4c 31 c8                     	xor	rax, r9
   1f4fe: 4c 8d 3c 07                  	lea	r15, [rdi + rax]
   1f502: c4 63 fb f0 e0 2f            	rorx	r12, rax, 0x2f
   1f508: 31 c0                        	xor	eax, eax
   1f50a: e9 8f 00 00 00               	jmp	0x1f59e <rust_siphash_find_packed+0x13e>
   1f50f: 90                           	nop
   1f510: 48 8b 4c 24 f8               	mov	rcx, qword ptr [rsp - 0x8]
   1f515: 4d 21 c2                     	and	r10, r8
   1f518: c4 a1 7a 6f 14 11            	vmovdqu	xmm2, xmmword ptr [rcx + r10]
   1f51e: 62 f1 6d 08 74 c1            	vpcmpeqb	k0, xmm2, xmm1
   1f524: c5 f8 98 c0                  	kortestw	k0, k0
   1f528: 74 36                        	je	0x1f560 <rust_siphash_find_packed+0x100>
   1f52a: c5 fb 93 c8                  	kmovd	ecx, k0
   1f52e: f3 44 0f bc d9               	tzcnt	r11d, ecx
   1f533: 4c 8b 74 24 e0               	mov	r14, qword ptr [rsp - 0x20]
   1f538: 4d 01 d3                     	add	r11, r10
   1f53b: 4d 21 c3                     	and	r11, r8
   1f53e: 4c 89 da                     	mov	rdx, r11
   1f541: 48 c1 e2 04                  	shl	rdx, 0x4
   1f545: 49 29 d6                     	sub	r14, rdx
   1f548: 49 3b 2e                     	cmp	rbp, qword ptr [r14]
   1f54b: 74 33                        	je	0x1f580 <rust_siphash_find_packed+0x120>
   1f54d: 8d 51 ff                     	lea	edx, [rcx - 0x1]
   1f550: 66 21 ca                     	and	dx, cx
   1f553: 89 d1                        	mov	ecx, edx
   1f555: 75 d7                        	jne	0x1f52e <rust_siphash_find_packed+0xce>
   1f557: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   1f560: 62 f1 6d 08 74 c0            	vpcmpeqb	k0, xmm2, xmm0
   1f566: c5 f8 98 c0                  	kortestw	k0, k0
   1f56a: 75 27                        	jne	0x1f593 <rust_siphash_find_packed+0x133>
   1f56c: 4f 8d 54 0a 10               	lea	r10, [r10 + r9 + 0x10]
   1f571: 49 83 c1 10                  	add	r9, 0x10
   1f575: eb 99                        	jmp	0x1f510 <rust_siphash_find_packed+0xb0>
   1f577: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   1f580: 48 8b 4c 24 d8               	mov	rcx, qword ptr [rsp - 0x28]
   1f585: 49 f7 db                     	neg	r11
   1f588: 48 ff c0                     	inc	rax
   1f58b: 49 c1 e3 04                  	shl	r11, 0x4
   1f58f: 4a 03 1c 19                  	add	rbx, qword ptr [rcx + r11]
   1f593: 48 3b 74 24 f0               	cmp	rsi, qword ptr [rsp - 0x10]
   1f598: 0f 84 57 01 00 00            	je	0x1f6f5 <rust_siphash_find_packed+0x295>
   1f59e: 48 8b 2e                     	mov	rbp, qword ptr [rsi]
   1f5a1: 48 8b 54 24 e8               	mov	rdx, qword ptr [rsp - 0x18]
   1f5a6: 48 83 c6 08                  	add	rsi, 0x8
   1f5aa: 48 89 e9                     	mov	rcx, rbp
   1f5ad: 4c 31 e9                     	xor	rcx, r13
   1f5b0: 4c 8d 0c 39                  	lea	r9, [rcx + rdi]
   1f5b4: c4 63 fb f0 d1 30            	rorx	r10, rcx, 0x30
   1f5ba: 4c 01 f9                     	add	rcx, r15
   1f5bd: 4d 31 ca                     	xor	r10, r9
   1f5c0: c4 63 fb f0 c9 20            	rorx	r9, rcx, 0x20
   1f5c6: 4c 31 e1                     	xor	rcx, r12
   1f5c9: 4d 8d 1c 12                  	lea	r11, [r10 + rdx]
   1f5cd: c4 43 fb f0 d2 2b            	rorx	r10, r10, 0x2b
   1f5d3: 48 ba 00 00 00 00 00 00 00 08	movabs	rdx, 0x800000000000000
   1f5dd: 4d 31 da                     	xor	r10, r11
   1f5e0: 49 31 eb                     	xor	r11, rbp
   1f5e3: 49 31 d2                     	xor	r10, rdx
   1f5e6: 49 01 cb                     	add	r11, rcx
   1f5e9: c4 e3 fb f0 c9 33            	rorx	rcx, rcx, 0x33
   1f5ef: 4d 01 d1                     	add	r9, r10
   1f5f2: c4 43 fb f0 d2 30            	rorx	r10, r10, 0x30
   1f5f8: 4c 31 d9                     	xor	rcx, r11
   1f5fb: c4 43 fb f0 db 20            	rorx	r11, r11, 0x20
   1f601: 4d 31 ca                     	xor	r10, r9
   1f604: 49 01 c9                     	add	r9, rcx
   1f607: c4 e3 fb f0 c9 2f            	rorx	rcx, rcx, 0x2f
   1f60d: 4d 01 d3                     	add	r11, r10
   1f610: c4 43 fb f0 d2 2b            	rorx	r10, r10, 0x2b
   1f616: 4c 31 c9                     	xor	rcx, r9
   1f619: c4 43 fb f0 c9 20            	rorx	r9, r9, 0x20
   1f61f: 4d 31 da                     	xor	r10, r11
   1f622: 49 31 d3                     	xor	r11, rdx
   1f625: 49 81 f1 ff 00 00 00         	xor	r9, 0xff
   1f62c: 49 01 cb                     	add	r11, rcx
   1f62f: c4 e3 fb f0 c9 33            	rorx	rcx, rcx, 0x33
   1f635: 4d 01 d1                     	add	r9, r10
   1f638: c4 43 fb f0 d2 30            	rorx	r10, r10, 0x30
   1f63e: 4c 31 d9                     	xor	rcx, r11
   1f641: 4d 31 ca                     	xor	r10, r9
   1f644: c4 43 fb f0 db 20            	rorx	r11, r11, 0x20
   1f64a: 49 01 c9                     	add	r9, rcx
   1f64d: c4 e3 fb f0 c9 2f            	rorx	rcx, rcx, 0x2f
   1f653: 4d 01 d3                     	add	r11, r10
   1f656: c4 43 fb f0 d2 2b            	rorx	r10, r10, 0x2b
   1f65c: 4c 31 c9                     	xor	rcx, r9
   1f65f: 4d 31 da                     	xor	r10, r11
   1f662: c4 43 fb f0 c9 20            	rorx	r9, r9, 0x20
   1f668: 49 01 cb                     	add	r11, rcx
   1f66b: c4 e3 fb f0 c9 33            	rorx	rcx, rcx, 0x33
   1f671: 4d 01 d1                     	add	r9, r10
   1f674: c4 43 fb f0 d2 30            	rorx	r10, r10, 0x30
   1f67a: 4c 31 d9                     	xor	rcx, r11
   1f67d: 4d 31 ca                     	xor	r10, r9
   1f680: c4 43 fb f0 db 20            	rorx	r11, r11, 0x20
   1f686: 49 01 c9                     	add	r9, rcx
   1f689: c4 e3 fb f0 c9 2f            	rorx	rcx, rcx, 0x2f
   1f68f: 4d 01 d3                     	add	r11, r10
   1f692: c4 43 fb f0 d2 2b            	rorx	r10, r10, 0x2b
   1f698: 4c 31 c9                     	xor	rcx, r9
   1f69b: 4d 31 da                     	xor	r10, r11
   1f69e: c4 43 fb f0 c9 20            	rorx	r9, r9, 0x20
   1f6a4: 49 01 cb                     	add	r11, rcx
   1f6a7: c4 e3 fb f0 c9 33            	rorx	rcx, rcx, 0x33
   1f6ad: 4d 01 d1                     	add	r9, r10
   1f6b0: c4 43 fb f0 d2 30            	rorx	r10, r10, 0x30
   1f6b6: 4c 31 d9                     	xor	rcx, r11
   1f6b9: 4d 31 ca                     	xor	r10, r9
   1f6bc: 49 01 c9                     	add	r9, rcx
   1f6bf: c4 43 fb f0 da 2b            	rorx	r11, r10, 0x2b
   1f6c5: c4 e3 fb f0 c9 2f            	rorx	rcx, rcx, 0x2f
   1f6cb: 49 31 cb                     	xor	r11, rcx
   1f6ce: c4 43 fb f0 d1 20            	rorx	r10, r9, 0x20
   1f6d4: 4d 31 da                     	xor	r10, r11
   1f6d7: 4d 31 ca                     	xor	r10, r9
   1f6da: 45 31 c9                     	xor	r9d, r9d
   1f6dd: 4c 89 d1                     	mov	rcx, r10
   1f6e0: 48 c1 e9 39                  	shr	rcx, 0x39
   1f6e4: 62 f2 7d 08 7a c9            	vpbroadcastb	xmm1, ecx
   1f6ea: e9 21 fe ff ff               	jmp	0x1f510 <rust_siphash_find_packed+0xb0>
   1f6ef: 31 c0                        	xor	eax, eax
   1f6f1: 31 db                        	xor	ebx, ebx
   1f6f3: eb 0e                        	jmp	0x1f703 <rust_siphash_find_packed+0x2a3>
   1f6f5: 48 b9 15 7c 4a 7f b9 79 37 9e	movabs	rcx, -0x61c8864680b583eb
   1f6ff: 48 0f af c1                  	imul	rax, rcx
   1f703: 48 31 d8                     	xor	rax, rbx
   1f706: 5b                           	pop	rbx
   1f707: 41 5c                        	pop	r12
   1f709: 41 5d                        	pop	r13
   1f70b: 41 5e                        	pop	r14
   1f70d: 41 5f                        	pop	r15
   1f70f: 5d                           	pop	rbp
   1f710: c3                           	ret
   1f711: cc                           	int3
   1f712: cc                           	int3
   1f713: cc                           	int3
   1f714: cc                           	int3
   1f715: cc                           	int3
   1f716: cc                           	int3
   1f717: cc                           	int3
   1f718: cc                           	int3
   1f719: cc                           	int3
   1f71a: cc                           	int3
   1f71b: cc                           	int3
   1f71c: cc                           	int3
   1f71d: cc                           	int3
   1f71e: cc                           	int3
   1f71f: cc                           	int3

/tmp/ep15_build/bench_rust:	file format elf64-x86-64

Disassembly of section .text:

000000000001f1b0 <rust_fxhash_find_packed>:
   1f1b0: 48 85 d2                     	test	rdx, rdx
   1f1b3: 0f 84 e2 00 00 00            	je	0x1f29b <rust_fxhash_find_packed+0xeb>
   1f1b9: 48 83 7f 18 00               	cmp	qword ptr [rdi + 0x18], 0x0
   1f1be: 0f 84 d7 00 00 00            	je	0x1f29b <rust_fxhash_find_packed+0xeb>
   1f1c4: 55                           	push	rbp
   1f1c5: 41 57                        	push	r15
   1f1c7: 41 56                        	push	r14
   1f1c9: 41 55                        	push	r13
   1f1cb: 41 54                        	push	r12
   1f1cd: 53                           	push	rbx
   1f1ce: 4c 8b 07                     	mov	r8, qword ptr [rdi]
   1f1d1: 48 8b 7f 08                  	mov	rdi, qword ptr [rdi + 0x8]
   1f1d5: b1 3d                        	mov	cl, 0x3d
   1f1d7: 49 8d 40 f8                  	lea	rax, [r8 - 0x8]
   1f1db: 4d 8d 48 f0                  	lea	r9, [r8 - 0x10]
   1f1df: 48 89 44 24 f0               	mov	qword ptr [rsp - 0x10], rax
   1f1e4: 48 b8 ff ff ff ff ff ff ff 1f	movabs	rax, 0x1fffffffffffffff
   1f1ee: 48 01 d0                     	add	rax, rdx
   1f1f1: c4 e2 f0 f5 d2               	bzhi	rdx, rdx, rcx
   1f1f6: 48 89 44 24 f8               	mov	qword ptr [rsp - 0x8], rax
   1f1fb: 48 83 fa 01                  	cmp	rdx, 0x1
   1f1ff: 0f 85 9e 00 00 00            	jne	0x1f2a3 <rust_fxhash_find_packed+0xf3>
   1f205: 31 c9                        	xor	ecx, ecx
   1f207: 31 d2                        	xor	edx, edx
   1f209: f6 44 24 f8 01               	test	byte ptr [rsp - 0x8], 0x1
   1f20e: 0f 85 23 02 00 00            	jne	0x1f437 <rust_fxhash_find_packed+0x287>
   1f214: 48 8b 36                     	mov	rsi, qword ptr [rsi]
   1f217: 49 ba 95 0a 22 27 b7 c1 7c 51	movabs	r10, 0x517cc1b727220a95
   1f221: c5 f1 76 c9                  	vpcmpeqd	xmm1, xmm1, xmm1
   1f225: 45 31 db                     	xor	r11d, r11d
   1f228: 4c 0f af d6                  	imul	r10, rsi
   1f22c: 4c 89 d0                     	mov	rax, r10
   1f22f: 48 c1 e8 39                  	shr	rax, 0x39
   1f233: 62 f2 7d 08 7a c0            	vpbroadcastb	xmm0, eax
   1f239: 49 21 fa                     	and	r10, rdi
   1f23c: c4 81 7a 6f 14 10            	vmovdqu	xmm2, xmmword ptr [r8 + r10]
   1f242: 62 f1 6d 08 74 c0            	vpcmpeqb	k0, xmm2, xmm0
   1f248: c5 f8 98 c0                  	kortestw	k0, k0
   1f24c: 74 32                        	je	0x1f280 <rust_fxhash_find_packed+0xd0>
   1f24e: c5 fb 93 d8                  	kmovd	ebx, k0
   1f252: f3 44 0f bc f3               	tzcnt	r14d, ebx
   1f257: 4d 89 cf                     	mov	r15, r9
   1f25a: 4d 01 d6                     	add	r14, r10
   1f25d: 49 21 fe                     	and	r14, rdi
   1f260: 4c 89 f0                     	mov	rax, r14
   1f263: 48 c1 e0 04                  	shl	rax, 0x4
   1f267: 49 29 c1                     	sub	r9, rax
   1f26a: 49 3b 31                     	cmp	rsi, qword ptr [r9]
   1f26d: 0f 84 b1 01 00 00            	je	0x1f424 <rust_fxhash_find_packed+0x274>
   1f273: 8d 43 ff                     	lea	eax, [rbx - 0x1]
   1f276: 4d 89 f9                     	mov	r9, r15
   1f279: 66 21 d8                     	and	ax, bx
   1f27c: 89 c3                        	mov	ebx, eax
   1f27e: 75 d2                        	jne	0x1f252 <rust_fxhash_find_packed+0xa2>
   1f280: 62 f1 6d 08 74 c1            	vpcmpeqb	k0, xmm2, xmm1
   1f286: c5 f8 98 c0                  	kortestw	k0, k0
   1f28a: 0f 85 a7 01 00 00            	jne	0x1f437 <rust_fxhash_find_packed+0x287>
   1f290: 4f 8d 54 1a 10               	lea	r10, [r10 + r11 + 0x10]
   1f295: 49 83 c3 10                  	add	r11, 0x10
   1f299: eb 9e                        	jmp	0x1f239 <rust_fxhash_find_packed+0x89>
   1f29b: 31 c0                        	xor	eax, eax
   1f29d: 31 c9                        	xor	ecx, ecx
   1f29f: 48 31 c8                     	xor	rax, rcx
   1f2a2: c3                           	ret
   1f2a3: c4 e2 f0 f5 d8               	bzhi	rbx, rax, rcx
   1f2a8: c5 f9 76 c0                  	vpcmpeqd	xmm0, xmm0, xmm0
   1f2ac: 31 c9                        	xor	ecx, ecx
   1f2ae: 31 d2                        	xor	edx, edx
   1f2b0: 45 31 f6                     	xor	r14d, r14d
   1f2b3: 4d 89 cb                     	mov	r11, r9
   1f2b6: 48 ff c3                     	inc	rbx
   1f2b9: 48 83 e3 fe                  	and	rbx, -0x2
   1f2bd: e9 91 00 00 00               	jmp	0x1f353 <rust_fxhash_find_packed+0x1a3>
   1f2c2: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00    	nop	word ptr cs:[rax + rax]
   1f2d0: 49 21 fc                     	and	r12, rdi
   1f2d3: c4 81 7a 6f 14 20            	vmovdqu	xmm2, xmmword ptr [r8 + r12]
   1f2d9: 62 f1 6d 08 74 c1            	vpcmpeqb	k0, xmm2, xmm1
   1f2df: c5 f8 98 c0                  	kortestw	k0, k0
   1f2e3: 74 2b                        	je	0x1f310 <rust_fxhash_find_packed+0x160>
   1f2e5: c5 fb 93 e8                  	kmovd	ebp, k0
   1f2e9: f3 44 0f bc d5               	tzcnt	r10d, ebp
   1f2ee: 4d 01 e2                     	add	r10, r12
   1f2f1: 49 21 fa                     	and	r10, rdi
   1f2f4: 4c 89 d0                     	mov	rax, r10
   1f2f7: 48 c1 e0 04                  	shl	rax, 0x4
   1f2fb: 49 29 c1                     	sub	r9, rax
   1f2fe: 4d 3b 39                     	cmp	r15, qword ptr [r9]
   1f301: 74 2d                        	je	0x1f330 <rust_fxhash_find_packed+0x180>
   1f303: 8d 45 ff                     	lea	eax, [rbp - 0x1]
   1f306: 4d 89 d9                     	mov	r9, r11
   1f309: 66 21 e8                     	and	ax, bp
   1f30c: 89 c5                        	mov	ebp, eax
   1f30e: 75 d9                        	jne	0x1f2e9 <rust_fxhash_find_packed+0x139>
   1f310: 62 f1 6d 08 74 c0            	vpcmpeqb	k0, xmm2, xmm0
   1f316: c5 f8 98 c0                  	kortestw	k0, k0
   1f31a: 75 2a                        	jne	0x1f346 <rust_fxhash_find_packed+0x196>
   1f31c: 4f 8d 64 2c 10               	lea	r12, [r12 + r13 + 0x10]
   1f321: 49 83 c5 10                  	add	r13, 0x10
   1f325: eb a9                        	jmp	0x1f2d0 <rust_fxhash_find_packed+0x120>
   1f327: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   1f330: 48 8b 44 24 f0               	mov	rax, qword ptr [rsp - 0x10]
   1f335: 49 f7 da                     	neg	r10
   1f338: 48 ff c2                     	inc	rdx
   1f33b: 4d 89 d9                     	mov	r9, r11
   1f33e: 49 c1 e2 04                  	shl	r10, 0x4
   1f342: 4a 03 0c 10                  	add	rcx, qword ptr [rax + r10]
   1f346: 49 83 c6 02                  	add	r14, 0x2
   1f34a: 49 39 de                     	cmp	r14, rbx
   1f34d: 0f 84 b6 fe ff ff            	je	0x1f209 <rust_fxhash_find_packed+0x59>
   1f353: 4c 8b 3e                     	mov	r15, qword ptr [rsi]
   1f356: 48 b8 95 0a 22 27 b7 c1 7c 51	movabs	rax, 0x517cc1b727220a95
   1f360: 45 31 ed                     	xor	r13d, r13d
   1f363: 4d 89 fc                     	mov	r12, r15
   1f366: 4c 0f af e0                  	imul	r12, rax
   1f36a: 4d 89 e2                     	mov	r10, r12
   1f36d: 49 c1 ea 39                  	shr	r10, 0x39
   1f371: 62 d2 7d 08 7a ca            	vpbroadcastb	xmm1, r10d
   1f377: 49 21 fc                     	and	r12, rdi
   1f37a: c4 81 7a 6f 14 20            	vmovdqu	xmm2, xmmword ptr [r8 + r12]
   1f380: 62 f1 6d 08 74 c1            	vpcmpeqb	k0, xmm2, xmm1
   1f386: c5 f8 98 c0                  	kortestw	k0, k0
   1f38a: 74 34                        	je	0x1f3c0 <rust_fxhash_find_packed+0x210>
   1f38c: c5 fb 93 e8                  	kmovd	ebp, k0
   1f390: f3 44 0f bc d5               	tzcnt	r10d, ebp
   1f395: 4d 01 e2                     	add	r10, r12
   1f398: 49 21 fa                     	and	r10, rdi
   1f39b: 4c 89 d0                     	mov	rax, r10
   1f39e: 48 c1 e0 04                  	shl	rax, 0x4
   1f3a2: 49 29 c1                     	sub	r9, rax
   1f3a5: 4d 3b 39                     	cmp	r15, qword ptr [r9]
   1f3a8: 74 36                        	je	0x1f3e0 <rust_fxhash_find_packed+0x230>
   1f3aa: 8d 45 ff                     	lea	eax, [rbp - 0x1]
   1f3ad: 4d 89 d9                     	mov	r9, r11
   1f3b0: 66 21 e8                     	and	ax, bp
   1f3b3: 89 c5                        	mov	ebp, eax
   1f3b5: 75 d9                        	jne	0x1f390 <rust_fxhash_find_packed+0x1e0>
   1f3b7: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   1f3c0: 62 f1 6d 08 74 c0            	vpcmpeqb	k0, xmm2, xmm0
   1f3c6: c5 f8 98 c0                  	kortestw	k0, k0
   1f3ca: 75 2a                        	jne	0x1f3f6 <rust_fxhash_find_packed+0x246>
   1f3cc: 4f 8d 64 2c 10               	lea	r12, [r12 + r13 + 0x10]
   1f3d1: 49 83 c5 10                  	add	r13, 0x10
   1f3d5: eb a0                        	jmp	0x1f377 <rust_fxhash_find_packed+0x1c7>
   1f3d7: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   1f3e0: 48 8b 44 24 f0               	mov	rax, qword ptr [rsp - 0x10]
   1f3e5: 49 f7 da                     	neg	r10
   1f3e8: 48 ff c2                     	inc	rdx
   1f3eb: 4d 89 d9                     	mov	r9, r11
   1f3ee: 49 c1 e2 04                  	shl	r10, 0x4
   1f3f2: 4a 03 0c 10                  	add	rcx, qword ptr [rax + r10]
   1f3f6: 4c 8b 7e 08                  	mov	r15, qword ptr [rsi + 0x8]
   1f3fa: 48 b8 95 0a 22 27 b7 c1 7c 51	movabs	rax, 0x517cc1b727220a95
   1f404: 48 83 c6 10                  	add	rsi, 0x10
   1f408: 45 31 ed                     	xor	r13d, r13d
   1f40b: 4d 89 fc                     	mov	r12, r15
   1f40e: 4c 0f af e0                  	imul	r12, rax
   1f412: 4c 89 e0                     	mov	rax, r12
   1f415: 48 c1 e8 39                  	shr	rax, 0x39
   1f419: 62 f2 7d 08 7a c8            	vpbroadcastb	xmm1, eax
   1f41f: e9 ac fe ff ff               	jmp	0x1f2d0 <rust_fxhash_find_packed+0x120>
   1f424: 48 8b 44 24 f0               	mov	rax, qword ptr [rsp - 0x10]
   1f429: 49 f7 de                     	neg	r14
   1f42c: 48 ff c2                     	inc	rdx
   1f42f: 49 c1 e6 04                  	shl	r14, 0x4
   1f433: 4a 03 0c 30                  	add	rcx, qword ptr [rax + r14]
   1f437: 48 b8 15 7c 4a 7f b9 79 37 9e	movabs	rax, -0x61c8864680b583eb
   1f441: 48 0f af c2                  	imul	rax, rdx
   1f445: 5b                           	pop	rbx
   1f446: 41 5c                        	pop	r12
   1f448: 41 5d                        	pop	r13
   1f44a: 41 5e                        	pop	r14
   1f44c: 41 5f                        	pop	r15
   1f44e: 5d                           	pop	rbp
   1f44f: 48 31 c8                     	xor	rax, rcx
   1f452: c3                           	ret
   1f453: cc                           	int3
   1f454: cc                           	int3
   1f455: cc                           	int3
   1f456: cc                           	int3
   1f457: cc                           	int3
   1f458: cc                           	int3
   1f459: cc                           	int3
   1f45a: cc                           	int3
   1f45b: cc                           	int3
   1f45c: cc                           	int3
   1f45d: cc                           	int3
   1f45e: cc                           	int3
   1f45f: cc                           	int3

# --- 5. Go 1.22.2 mapaccess2_fast64 (Classic 8-Slot Bucket) ---
TEXT runtime.mapaccess2_fast64(SB) /usr/lib/go-1.22/src/runtime/map_fast64.go
  map_fast64.go:53	0x411000		493b6610		CMPQ SP, 0x10(R14)			
  map_fast64.go:53	0x411004		0f8662010000		JBE 0x41116c				
  map_fast64.go:53	0x41100a		55			PUSHQ BP				
  map_fast64.go:53	0x41100b		4889e5			MOVQ SP, BP				
  map_fast64.go:53	0x41100e		4883ec10		SUBQ $0x10, SP				
  map_fast64.go:53	0x411012		48894c2430		MOVQ CX, 0x30(SP)			
  map_fast64.go:53	0x411017		660f1f840000000000	NOPW 0(AX)(AX*1)			
  map_fast64.go:58	0x411020		4885db			TESTQ BX, BX				
  map_fast64.go:58	0x411023		0f84c7000000		JE 0x4110f0				
  map_fast64.go:58	0x411029		48833b00		CMPQ 0(BX), $0x0			
  map_fast64.go:58	0x41102d		0f84bd000000		JE 0x4110f0				
  map_fast64.go:58	0x411033		4889442420		MOVQ AX, 0x20(SP)			
  map_fast64.go:58	0x411038		48895c2428		MOVQ BX, 0x28(SP)			
  map_fast64.go:61	0x41103d		0fb64b08		MOVZX 0x8(BX), CX			
  map_fast64.go:61	0x411041		f6c104			TESTL $0x4, CL				
  map_fast64.go:61	0x411044		741b			JE 0x411061				
  map_fast64.go:62	0x411046		488d057b2a0b00		LEAQ 0xb2a7b(IP), AX			
  map_fast64.go:62	0x41104d		bb21000000		MOVL $0x21, BX				
  map_fast64.go:62	0x411052		e8093b0200		CALL runtime.fatal(SB)			
  map_fast64.go:69	0x411057		488b442420		MOVQ 0x20(SP), AX			
  map_fast64.go:65	0x41105c		488b5c2428		MOVQ 0x28(SP), BX			
  map_fast64.go:65	0x411061		807b0900		CMPB 0x9(BX), $0x0			
  map_fast64.go:65	0x411065		7509			JNE 0x411070				
  map_fast64.go:67	0x411067		488b4b10		MOVQ 0x10(BX), CX			
  map_fast64.go:67	0x41106b		e99e000000		JMP 0x41110e				
  map_fast64.go:69	0x411070		488b5048		MOVQ 0x48(AX), DX			
  map_fast64.go:69	0x411074		488b0a			MOVQ 0(DX), CX				
  map_fast64.go:69	0x411077		8b730c			MOVL 0xc(BX), SI			
  map_fast64.go:69	0x41107a		488d442430		LEAQ 0x30(SP), AX			
  map_fast64.go:69	0x41107f		4889f3			MOVQ SI, BX				
  map_fast64.go:69	0x411082		ffd1			CALL CX					
  map_fast64.go:70	0x411084		488b4c2428		MOVQ 0x28(SP), CX			
  map_fast64.go:70	0x411089		0fb67109		MOVZX 0x9(CX), SI			
  map_fast64.go:71	0x41108d		488b7c2420		MOVQ 0x20(SP), DI			
  map_fast64.go:71	0x411092		440fb74752		MOVZX 0x52(DI), R8			
  map_fast64.go:72	0x411097		4c8b4918		MOVQ 0x18(CX), R9			
  map_fast64.go:58	0x41109b		4889ca			MOVQ CX, DX				
  map.go:187		0x41109e		89f1			MOVL SI, CX				
  map.go:187		0x4110a0		41ba01000000		MOVL $0x1, R10				
  map.go:187		0x4110a6		49d3e2			SHLQ CL, R10				
  map.go:192		0x4110a9		498d4aff		LEAQ -0x1(R10), CX			
  map_fast64.go:71	0x4110ad		4889ce			MOVQ CX, SI				
  map_fast64.go:71	0x4110b0		4821c1			ANDQ AX, CX				
  map_fast64.go:71	0x4110b3		490fafc8		IMULQ R8, CX				
  stubs.go:16		0x4110b7		48034a10		ADDQ 0x10(DX), CX			
  stubs.go:16		0x4110bb		0f1f440000		NOPL 0(AX)(AX*1)			
  map_fast64.go:72	0x4110c0		4d85c9			TESTQ R9, R9				
  map_fast64.go:72	0x4110c3		7426			JE 0x4110eb				
  map.go:1123		0x4110c5		0fb65208		MOVZX 0x8(DX), DX			
  map.go:1123		0x4110c9		f6c208			TESTL $0x8, DL				
  map_fast64.go:73	0x4110cc		7503			JNE 0x4110d1				
  map_fast64.go:75	0x4110ce		48d1ee			SHRQ $0x1, SI				
  map_fast64.go:77	0x4110d1		4821c6			ANDQ AX, SI				
  map_fast64.go:77	0x4110d4		490faff0		IMULQ R8, SI				
  map.go:205		0x4110d8		410fb61431		MOVZX 0(R9)(SI*1), DX			
  map.go:206		0x4110dd		83c2fe			ADDL $-0x2, DX				
  stubs.go:16		0x4110e0		4c01ce			ADDQ R9, SI				
  map.go:206		0x4110e3		80fa03			CMPL DL, $0x3				
  map_fast64.go:78	0x4110e6		7203			JB 0x4110eb				
  map_fast64.go:83	0x4110e8		4889f1			MOVQ SI, CX				
  map.go:210		0x4110eb		4889f8			MOVQ DI, AX				
  map.go:210		0x4110ee		eb1e			JMP 0x41110e				
  map_fast64.go:59	0x4110f0		488d05e95d1b00		LEAQ runtime.zeroVal(SB), AX		
  map_fast64.go:59	0x4110f7		31db			XORL BX, BX				
  map_fast64.go:59	0x4110f9		4883c410		ADDQ $0x10, SP				
  map_fast64.go:59	0x4110fd		5d			POPQ BP					
  map_fast64.go:59	0x4110fe		c3			RET					
  map.go:210		0x4110ff		0fb75052		MOVZX 0x52(AX), DX			
  stubs.go:16		0x411103		488d1411		LEAQ 0(CX)(DX*1), DX			
  stubs.go:16		0x411107		488d52f8		LEAQ -0x8(DX), DX			
  map.go:210		0x41110b		488b0a			MOVQ 0(DX), CX				
  map_fast64.go:83	0x41110e		4885c9			TESTQ CX, CX				
  map_fast64.go:83	0x411111		7409			JE 0x41111c				
  map.go:218		0x411113		90			NOPL					
  stubs.go:16		0x411114		488d5108		LEAQ 0x8(CX), DX			
  stubs.go:16		0x411118		31db			XORL BX, BX				
  map_fast64.go:84	0x41111a		eb16			JMP 0x411132				
  map_fast64.go:90	0x41111c		488d05bd5d1b00		LEAQ runtime.zeroVal(SB), AX		
  map_fast64.go:90	0x411123		31db			XORL BX, BX				
  map_fast64.go:90	0x411125		4883c410		ADDQ $0x10, SP				
  map_fast64.go:90	0x411129		5d			POPQ BP					
  map_fast64.go:90	0x41112a		c3			RET					
  map_fast64.go:84	0x41112b		48ffc3			INCQ BX					
  stubs.go:16		0x41112e		4883c208		ADDQ $0x8, DX				
  map_fast64.go:84	0x411132		4883fb08		CMPQ BX, $0x8				
  map_fast64.go:84	0x411136		73c7			JAE 0x4110ff				
  map_fast64.go:85	0x411138		488b32			MOVQ 0(DX), SI				
  map_fast64.go:85	0x41113b		0f1f440000		NOPL 0(AX)(AX*1)			
  map_fast64.go:85	0x411140		4839742430		CMPQ 0x30(SP), SI			

# --- 6. Go 1.24.0 SwissTable Map Lookup (runtime.mapaccess2_fast64) ---
TEXT runtime.mapaccess2_fast64(SB) /home/kelvingorekore/go/pkg/mod/golang.org/toolchain@v0.0.1-go1.24.0.linux-amd64/src/internal/runtime/maps/runtime_fast64_swiss.go
  runtime_fast64_swiss.go:86	0x406a40		493b6610		CMPQ SP, 0x10(R14)			
  runtime_fast64_swiss.go:86	0x406a44		0f86d9010000		JBE 0x406c23				
  runtime_fast64_swiss.go:86	0x406a4a		55			PUSHQ BP				
  runtime_fast64_swiss.go:86	0x406a4b		4889e5			MOVQ SP, BP				
  runtime_fast64_swiss.go:86	0x406a4e		4883ec18		SUBQ $0x18, SP				
  runtime_fast64_swiss.go:93	0x406a52		4885db			TESTQ BX, BX				
  runtime_fast64_swiss.go:93	0x406a55		0f84c6000000		JE 0x406b21				
  map.go:389			0x406a5b		48833b00		CMPQ 0(BX), $0x0			
  map.go:389			0x406a5f		90			NOPL					
  runtime_fast64_swiss.go:93	0x406a60		0f84bb000000		JE 0x406b21				
  runtime_fast64_swiss.go:97	0x406a66		807b2200		CMPB 0x22(BX), $0x0			
  runtime_fast64_swiss.go:97	0x406a6a		0f8595000000		JNE 0x406b05				
  runtime_fast64_swiss.go:102	0x406a70		48837b1800		CMPQ 0x18(BX), $0x0			
  runtime_fast64_swiss.go:102	0x406a75		7524			JNE 0x406a9b				
  runtime_fast64_swiss.go:104	0x406a77		488b5310		MOVQ 0x10(BX), DX			
  runtime_fast64_swiss.go:106	0x406a7b		488b32			MOVQ 0(DX), SI				
  group.go:210			0x406a7e		66480f6ec6		MOVQ SI, X0				
  group.go:210			0x406a83		660fd7f0		PMOVMSKB X0, SI				
  group.go:210			0x406a87		f7d6			NOTL SI					
  runtime_fast64_swiss.go:107	0x406a89		90			NOPL					
  runtime_fast64_swiss.go:108	0x406a8a		488b7858		MOVQ 0x58(AX), DI			
  group.go:210			0x406a8e		400fb6f6		MOVZX SI, SI				
  group.go:281			0x406a92		4883c208		ADDQ $0x8, DX				
  runtime_fast64_swiss.go:109	0x406a96		e95a010000		JMP 0x406bf5				
  runtime_fast64_swiss.go:93	0x406a9b		4889442428		MOVQ AX, 0x28(SP)			
  runtime_fast64_swiss.go:93	0x406aa0		48895c2430		MOVQ BX, 0x30(SP)			
  runtime_fast64_swiss.go:93	0x406aa5		48894c2438		MOVQ CX, 0x38(SP)			
  runtime_fast64_swiss.go:120	0x406aaa		48894c2410		MOVQ CX, 0x10(SP)			
  runtime_fast64_swiss.go:121	0x406aaf		488b5048		MOVQ 0x48(AX), DX			
  runtime_fast64_swiss.go:121	0x406ab3		488b0a			MOVQ 0(DX), CX				
  runtime_fast64_swiss.go:121	0x406ab6		488b5b08		MOVQ 0x8(BX), BX			
  runtime_fast64_swiss.go:121	0x406aba		488d442410		LEAQ 0x10(SP), AX			
  runtime_fast64_swiss.go:121	0x406abf		90			NOPL					
  runtime_fast64_swiss.go:121	0x406ac0		ffd1			CALL CX					
  map.go:330			0x406ac2		488b4c2430		MOVQ 0x30(SP), CX			
  map.go:330			0x406ac7		4883791801		CMPQ 0x18(CX), $0x1			
  map.go:330			0x406acc		7504			JNE 0x406ad2				
  map.go:330			0x406ace		31d2			XORL DX, DX				
  runtime_fast64_swiss.go:124	0x406ad0		eb18			JMP 0x406aea				
  map.go:333			0x406ad2		0fb65121		MOVZX 0x21(CX), DX			
  runtime_fast64_swiss.go:93	0x406ad6		4889cb			MOVQ CX, BX				
  map.go:333			0x406ad9		89d1			MOVL DX, CX				
  map.go:333			0x406adb		4889c6			MOVQ AX, SI				
  map.go:333			0x406ade		48d3e8			SHRQ CL, AX				
  map.go:337			0x406ae1		4889d9			MOVQ BX, CX				
  runtime_fast64_swiss.go:124	0x406ae4		4889c2			MOVQ AX, DX				
  map.go:184			0x406ae7		4889f0			MOVQ SI, AX				
  map.go:337			0x406aea		488b4910		MOVQ 0x10(CX), CX			
  map.go:337			0x406aee		488b0cd1		MOVQ 0(CX)(DX*8), CX			
  runtime_fast64_swiss.go:128	0x406af2		488b5118		MOVQ 0x18(CX), DX			
  map.go:184			0x406af6		4889c6			MOVQ AX, SI				
  map.go:184			0x406af9		48c1e807		SHRQ $0x7, AX				
  runtime_fast64_swiss.go:125	0x406afd		90			NOPL					
  table.go:1145			0x406afe		4821d0			ANDQ DX, AX				
  table.go:1145			0x406b01		31db			XORL BX, BX				
  runtime_fast64_swiss.go:129	0x406b03		eb3c			JMP 0x406b41				
  runtime_fast64_swiss.go:98	0x406b05		488d0541800d00		LEAQ 0xd8041(IP), AX			
  runtime_fast64_swiss.go:98	0x406b0c		bb21000000		MOVL $0x21, BX				
  runtime_fast64_swiss.go:98	0x406b11		e84a380600		CALL internal/runtime/maps.fatal(SB)	
  runtime_fast64_swiss.go:99	0x406b16		31c0			XORL AX, AX				
  runtime_fast64_swiss.go:99	0x406b18		31db			XORL BX, BX				
  runtime_fast64_swiss.go:99	0x406b1a		4883c418		ADDQ $0x18, SP				
  runtime_fast64_swiss.go:99	0x406b1e		5d			POPQ BP					
  runtime_fast64_swiss.go:99	0x406b1f		90			NOPL					
  runtime_fast64_swiss.go:99	0x406b20		c3			RET					
  runtime_fast64_swiss.go:94	0x406b21		488d0518741b00		LEAQ runtime.zeroVal(SB), AX		
  runtime_fast64_swiss.go:94	0x406b28		31db			XORL BX, BX				
  runtime_fast64_swiss.go:94	0x406b2a		4883c418		ADDQ $0x18, SP				
  runtime_fast64_swiss.go:94	0x406b2e		5d			POPQ BP					
  runtime_fast64_swiss.go:94	0x406b2f		c3			RET					
  table.go:1152			0x406b30		488d0403		LEAQ 0(BX)(AX*1), AX			
  table.go:1152			0x406b34		488d4001		LEAQ 0x1(AX), AX			
  table.go:1151			0x406b38		48ffc3			INCQ BX					
  table.go:1152			0x406b3b		4821d0			ANDQ DX, AX				
  map.go:191			0x406b3e		4c89d6			MOVQ R10, SI				
  group.go:319			0x406b41		488b7c2428		MOVQ 0x28(SP), DI			
  group.go:319			0x406b46		4c8b4750		MOVQ 0x50(DI), R8			
  group.go:319			0x406b4a		4c0fafc0		IMULQ AX, R8				
  group.go:322			0x406b4e		4c034110		ADDQ 0x10(CX), R8			
  runtime_fast64_swiss.go:132	0x406b52		4d8b08			MOVQ 0(R8), R9				
  map.go:191			0x406b55		4989f2			MOVQ SI, R10				
  map.go:191			0x406b58		83e67f			ANDL $0x7f, SI				
  group.go:148			0x406b5b		66480f6ec6		MOVQ SI, X0				
  group.go:148			0x406b60		660f60c0		PUNPCKLBW X0, X0			
  group.go:148			0x406b64		f20f70c000		PSHUFLW $0x0, X0, X0			
  group.go:148			0x406b69		66490f6ec9		MOVQ R9, X1				
  group.go:148			0x406b6e		660f74c1		PCMPEQB X1, X0				
  group.go:148			0x406b72		660fd7f0		PMOVMSKB X0, SI				
  runtime_fast64_swiss.go:130	0x406b76		90			NOPL					
  group.go:148			0x406b77		400fb6f6		MOVZX SI, SI				
  runtime_fast64_swiss.go:134	0x406b7b		4c8b4c2438		MOVQ 0x38(SP), R9			
  runtime_fast64_swiss.go:134	0x406b80		eb2d			JMP 0x406baf				
  group.go:170			0x406b82		48be8080808080808080	MOVQ $0x8080808080808080, SI		
  group.go:170			0x406b8c		66480f6ec6		MOVQ SI, X0				
  group.go:170			0x406b91		660f74c1		PCMPEQB X1, X0				
  group.go:170			0x406b95		66440fd7c0		PMOVMSKB X0, R8				
  group.go:170			0x406b9a		450fb6c0		MOVZX R8, R8				
  runtime_fast64_swiss.go:145	0x406b9e		90			NOPL					
  runtime_fast64_swiss.go:145	0x406b9f		90			NOPL					
  runtime_fast64_swiss.go:146	0x406ba0		4d85c0			TESTQ R8, R8				
  runtime_fast64_swiss.go:146	0x406ba3		748b			JE 0x406b30				
  runtime_fast64_swiss.go:146	0x406ba5		eb38			JMP 0x406bdf				
  group.go:64			0x406ba7		4c8d5eff		LEAQ -0x1(SI), R11			
  runtime_fast64_swiss.go:142	0x406bab		90			NOPL					
  group.go:64			0x406bac		4c21de			ANDQ R11, SI				
  runtime_fast64_swiss.go:134	0x406baf		4885f6			TESTQ SI, SI				
  runtime_fast64_swiss.go:134	0x406bb2		74ce			JE 0x406b82				
  group.go:279			0x406bb4		4c8b5f58		MOVQ 0x58(DI), R11			
  runtime_fast64_swiss.go:135	0x406bb8		90			NOPL					
  runtime_fast64_swiss.go:137	0x406bb9		90			NOPL					
  group.go:50			0x406bba		4c0fbce6		BSFQ SI, R12				
  group.go:279			0x406bbe		4d0fafdc		IMULQ R12, R11				
  runtime_fast64_swiss.go:138	0x406bc2		4f8b640308		MOVQ 0x8(R11)(R8*1), R12		
  runtime_fast64_swiss.go:138	0x406bc7		4d39cc			CMPQ R12, R9				
  runtime_fast64_swiss.go:138	0x406bca		75db			JNE 0x406ba7				
  runtime_fast64_swiss.go:139	0x406bcc		4b8d0403		LEAQ 0(R11)(R8*1), AX			
  runtime_fast64_swiss.go:139	0x406bd0		488d4010		LEAQ 0x10(AX), AX			
  runtime_fast64_swiss.go:140	0x406bd4		bb01000000		MOVL $0x1, BX				
  runtime_fast64_swiss.go:140	0x406bd9		4883c418		ADDQ $0x18, SP				
  runtime_fast64_swiss.go:140	0x406bdd		5d			POPQ BP					
  runtime_fast64_swiss.go:140	0x406bde		c3			RET					
  runtime_fast64_swiss.go:149	0x406bdf		488d055a731b00		LEAQ runtime.zeroVal(SB), AX		
  runtime_fast64_swiss.go:149	0x406be6		31db			XORL BX, BX				
  runtime_fast64_swiss.go:149	0x406be8		4883c418		ADDQ $0x18, SP				

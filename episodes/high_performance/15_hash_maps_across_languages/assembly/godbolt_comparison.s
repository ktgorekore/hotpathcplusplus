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

0000000000006420 <Ep15StdUnorderedFindPacked>:
    6420: f3 0f 1e fa                  	endbr64
    6424: 48 85 d2                     	test	rdx, rdx
    6427: 0f 84 df 00 00 00            	je	0x650c <Ep15StdUnorderedFindPacked+0xec>
    642d: 55                           	push	rbp
    642e: 49 89 fa                     	mov	r10, rdi
    6431: 45 31 db                     	xor	r11d, r11d
    6434: 48 89 e5                     	mov	rbp, rsp
    6437: 41 57                        	push	r15
    6439: 41 56                        	push	r14
    643b: 41 55                        	push	r13
    643d: 41 54                        	push	r12
    643f: 53                           	push	rbx
    6440: 4c 8b 7f 18                  	mov	r15, qword ptr [rdi + 0x18]
    6444: 4c 8d 2c d6                  	lea	r13, [rsi + 8*rdx]
    6448: 45 31 e4                     	xor	r12d, r12d
    644b: 0f 1f 44 00 00               	nop	dword ptr [rax + rax]
    6450: 48 8b 0e                     	mov	rcx, qword ptr [rsi]
    6453: 4d 85 ff                     	test	r15, r15
    6456: 75 58                        	jne	0x64b0 <Ep15StdUnorderedFindPacked+0x90>
    6458: 49 8b 52 10                  	mov	rdx, qword ptr [r10 + 0x10]
    645c: 48 85 d2                     	test	rdx, rdx
    645f: 75 17                        	jne	0x6478 <Ep15StdUnorderedFindPacked+0x58>
    6461: eb 22                        	jmp	0x6485 <Ep15StdUnorderedFindPacked+0x65>
    6463: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    646e: 66 90                        	nop
    6470: 48 8b 12                     	mov	rdx, qword ptr [rdx]
    6473: 48 85 d2                     	test	rdx, rdx
    6476: 74 0d                        	je	0x6485 <Ep15StdUnorderedFindPacked+0x65>
    6478: 48 3b 4a 08                  	cmp	rcx, qword ptr [rdx + 0x8]
    647c: 75 f2                        	jne	0x6470 <Ep15StdUnorderedFindPacked+0x50>
    647e: 4c 03 62 10                  	add	r12, qword ptr [rdx + 0x10]
    6482: 49 ff c3                     	inc	r11
    6485: 48 83 c6 08                  	add	rsi, 0x8
    6489: 49 39 f5                     	cmp	r13, rsi
    648c: 75 c2                        	jne	0x6450 <Ep15StdUnorderedFindPacked+0x30>
    648e: 48 b8 15 7c 4a 7f b9 79 37 9e	movabs	rax, -0x61c8864680b583eb
    6498: 5b                           	pop	rbx
    6499: 49 0f af c3                  	imul	rax, r11
    649d: 4c 31 e0                     	xor	rax, r12
    64a0: 41 5c                        	pop	r12
    64a2: 41 5d                        	pop	r13
    64a4: 41 5e                        	pop	r14
    64a6: 41 5f                        	pop	r15
    64a8: 5d                           	pop	rbp
    64a9: c3                           	ret
    64aa: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
    64b0: 4d 8b 4a 08                  	mov	r9, qword ptr [r10 + 0x8]
    64b4: 48 89 c8                     	mov	rax, rcx
    64b7: 31 d2                        	xor	edx, edx
    64b9: 49 f7 f1                     	div	r9
    64bc: 49 8b 02                     	mov	rax, qword ptr [r10]
    64bf: 48 8b 1c d0                  	mov	rbx, qword ptr [rax + 8*rdx]
    64c3: 49 89 d6                     	mov	r14, rdx
    64c6: 48 85 db                     	test	rbx, rbx
    64c9: 74 ba                        	je	0x6485 <Ep15StdUnorderedFindPacked+0x65>
    64cb: 48 8b 13                     	mov	rdx, qword ptr [rbx]
    64ce: 48 8b 7a 08                  	mov	rdi, qword ptr [rdx + 0x8]
    64d2: 48 39 f9                     	cmp	rcx, rdi
    64d5: 74 24                        	je	0x64fb <Ep15StdUnorderedFindPacked+0xdb>
    64d7: 4c 8b 02                     	mov	r8, qword ptr [rdx]
    64da: 4d 85 c0                     	test	r8, r8
    64dd: 74 a6                        	je	0x6485 <Ep15StdUnorderedFindPacked+0x65>
    64df: 49 8b 78 08                  	mov	rdi, qword ptr [r8 + 0x8]
    64e3: 48 89 d3                     	mov	rbx, rdx
    64e6: 31 d2                        	xor	edx, edx
    64e8: 48 89 f8                     	mov	rax, rdi
    64eb: 49 f7 f1                     	div	r9
    64ee: 49 39 d6                     	cmp	r14, rdx
    64f1: 75 92                        	jne	0x6485 <Ep15StdUnorderedFindPacked+0x65>
    64f3: 4c 89 c2                     	mov	rdx, r8
    64f6: 48 39 f9                     	cmp	rcx, rdi
    64f9: 75 dc                        	jne	0x64d7 <Ep15StdUnorderedFindPacked+0xb7>
    64fb: 48 8b 13                     	mov	rdx, qword ptr [rbx]
    64fe: 48 85 d2                     	test	rdx, rdx
    6501: 0f 85 77 ff ff ff            	jne	0x647e <Ep15StdUnorderedFindPacked+0x5e>
    6507: e9 79 ff ff ff               	jmp	0x6485 <Ep15StdUnorderedFindPacked+0x65>
    650c: 31 c0                        	xor	eax, eax
    650e: c3                           	ret
    650f: 90                           	nop

# --- 3. C++20 absl::flat_hash_map Lookup Kernel (GCC 13 -O3 -march=native) ---

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/15_hash_maps_across_languages/challenge:	file format elf64-x86-64

Disassembly of section .text:

0000000000006510 <Ep15AbslFlatFindPacked>:
    6510: f3 0f 1e fa                  	endbr64
    6514: 55                           	push	rbp
    6515: 48 89 e5                     	mov	rbp, rsp
    6518: 41 57                        	push	r15
    651a: 41 56                        	push	r14
    651c: 41 55                        	push	r13
    651e: 41 54                        	push	r12
    6520: 53                           	push	rbx
    6521: 48 89 75 c8                  	mov	qword ptr [rbp - 0x38], rsi
    6525: 48 85 d2                     	test	rdx, rdx
    6528: 0f 84 92 01 00 00            	je	0x66c0 <Ep15AbslFlatFindPacked+0x1b0>
    652e: 48 89 f9                     	mov	rcx, rdi
    6531: 44 0f b6 1f                  	movzx	r11d, byte ptr [rdi]
    6535: 49 89 d2                     	mov	r10, rdx
    6538: bb 01 00 00 00               	mov	ebx, 0x1
    653d: 48 8b 31                     	mov	rsi, qword ptr [rcx]
    6540: 45 31 c9                     	xor	r9d, r9d
    6543: 31 ff                        	xor	edi, edi
    6545: 4d 89 cf                     	mov	r15, r9
    6548: c4 e2 a1 f7 db               	shlx	rbx, rbx, r11
    654d: 48 8d 43 0f                  	lea	rax, [rbx + 0xf]
    6551: 41 0f b6 db                  	movzx	ebx, r11b
    6555: 48 89 f2                     	mov	rdx, rsi
    6558: 48 89 45 c0                  	mov	qword ptr [rbp - 0x40], rax
    655c: 49 89 de                     	mov	r14, rbx
    655f: 31 c0                        	xor	eax, eax
    6561: 48 c1 ee 11                  	shr	rsi, 0x11
    6565: 0f b6 d6                     	movzx	edx, dh
    6568: 48 89 75 b0                  	mov	qword ptr [rbp - 0x50], rsi
    656c: 48 89 55 b8                  	mov	qword ptr [rbp - 0x48], rdx
    6570: e9 a3 00 00 00               	jmp	0x6618 <Ep15AbslFlatFindPacked+0x108>
    6575: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    6580: 48 8b 55 c8                  	mov	rdx, qword ptr [rbp - 0x38]
    6584: 4a 8b 34 fa                  	mov	rsi, qword ptr [rdx + 8*r15]
    6588: 4c 8b 45 b8                  	mov	r8, qword ptr [rbp - 0x48]
    658c: 48 ba f5 8c 1e de e0 f9 d5 79	movabs	rdx, 0x79d5f9e0de1e8cf5
    6596: 4c 8b 69 10                  	mov	r13, qword ptr [rcx + 0x10]
    659a: 31 db                        	xor	ebx, ebx
    659c: 49 31 f0                     	xor	r8, rsi
    659f: c4 42 bb f6 c8               	mulx	r9, r8, r8
    65a4: 4c 89 45 a0                  	mov	qword ptr [rbp - 0x60], r8
    65a8: 4c 8b 45 a0                  	mov	r8, qword ptr [rbp - 0x60]
    65ac: 4c 89 4d a8                  	mov	qword ptr [rbp - 0x58], r9
    65b0: 4c 89 fa                     	mov	rdx, r15
    65b3: 4d 31 c8                     	xor	r8, r9
    65b6: c4 42 88 f5 c8               	bzhi	r9, r8, r14
    65bb: 49 c1 e8 39                  	shr	r8, 0x39
    65bf: 62 d2 7d 08 7a e8            	vpbroadcastb	xmm5, r8d
    65c5: c4 81 7a 6f 1c 0c            	vmovdqu	xmm3, xmmword ptr [r12 + r9]
    65cb: 4d 89 c8                     	mov	r8, r9
    65ce: 49 c1 e0 04                  	shl	r8, 0x4
    65d2: 43 0f 18 4c 05 00            	prefetcht0	byte ptr [r13 + r8]
    65d8: c5 d1 74 e3                  	vpcmpeqb	xmm4, xmm5, xmm3
    65dc: c5 79 d7 fc                  	vpmovmskb	r15d, xmm4
    65e0: 45 85 ff                     	test	r15d, r15d
    65e3: 0f 84 b7 00 00 00            	je	0x66a0 <Ep15AbslFlatFindPacked+0x190>
    65e9: f3 45 0f bc c7               	tzcnt	r8d, r15d
    65ee: 4d 01 c8                     	add	r8, r9
    65f1: c4 42 88 f5 c0               	bzhi	r8, r8, r14
    65f6: 49 c1 e0 04                  	shl	r8, 0x4
    65fa: 4d 01 e8                     	add	r8, r13
    65fd: 49 3b 30                     	cmp	rsi, qword ptr [r8]
    6600: 0f 85 8a 00 00 00            	jne	0x6690 <Ep15AbslFlatFindPacked+0x180>
    6606: 49 89 d7                     	mov	r15, rdx
    6609: 49 03 78 08                  	add	rdi, qword ptr [r8 + 0x8]
    660d: 48 ff c0                     	inc	rax
    6610: 49 ff c7                     	inc	r15
    6613: 4d 39 fa                     	cmp	r10, r15
    6616: 74 59                        	je	0x6671 <Ep15AbslFlatFindPacked+0x161>
    6618: 41 80 fb 01                  	cmp	r11b, 0x1
    661c: 76 32                        	jbe	0x6650 <Ep15AbslFlatFindPacked+0x140>
    661e: 4c 8b 61 08                  	mov	r12, qword ptr [rcx + 0x8]
    6622: 48 8b 5d c0                  	mov	rbx, qword ptr [rbp - 0x40]
    6626: 49 8d 34 1c                  	lea	rsi, [r12 + rbx]
    662a: 41 0f 18 1c 24               	prefetcht2	byte ptr [r12]
    662f: 48 39 f1                     	cmp	rcx, rsi
    6632: 0f 82 48 ff ff ff            	jb	0x6580 <Ep15AbslFlatFindPacked+0x70>
    6638: 48 8b 5d c8                  	mov	rbx, qword ptr [rbp - 0x38]
    663c: 4a 8b 34 fb                  	mov	rsi, qword ptr [rbx + 8*r15]
    6640: e9 43 ff ff ff               	jmp	0x6588 <Ep15AbslFlatFindPacked+0x78>
    6645: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    6650: 48 83 7d b0 00               	cmp	qword ptr [rbp - 0x50], 0x0
    6655: 74 b9                        	je	0x6610 <Ep15AbslFlatFindPacked+0x100>
    6657: 48 8b 5d c8                  	mov	rbx, qword ptr [rbp - 0x38]
    665b: 4c 8d 41 08                  	lea	r8, [rcx + 0x8]
    665f: 4a 8b 14 fb                  	mov	rdx, qword ptr [rbx + 8*r15]
    6663: 48 39 51 08                  	cmp	qword ptr [rcx + 0x8], rdx
    6667: 74 a0                        	je	0x6609 <Ep15AbslFlatFindPacked+0xf9>
    6669: 49 ff c7                     	inc	r15
    666c: 4d 39 fa                     	cmp	r10, r15
    666f: 75 a7                        	jne	0x6618 <Ep15AbslFlatFindPacked+0x108>
    6671: 48 ba 15 7c 4a 7f b9 79 37 9e	movabs	rdx, -0x61c8864680b583eb
    667b: 48 0f af c2                  	imul	rax, rdx
    667f: 5b                           	pop	rbx
    6680: 48 31 f8                     	xor	rax, rdi
    6683: 41 5c                        	pop	r12
    6685: 41 5d                        	pop	r13
    6687: 41 5e                        	pop	r14
    6689: 41 5f                        	pop	r15
    668b: 5d                           	pop	rbp
    668c: c3                           	ret
    668d: 0f 1f 00                     	nop	dword ptr [rax]
    6690: c4 c2 00 f3 cf               	blsr	r15d, r15d
    6695: 0f 85 4e ff ff ff            	jne	0x65e9 <Ep15AbslFlatFindPacked+0xd9>
    669b: 0f 1f 44 00 00               	nop	dword ptr [rax + rax]
    66a0: c4 e2 61 08 db               	vpsignb	xmm3, xmm3, xmm3
    66a5: c5 79 d7 c3                  	vpmovmskb	r8d, xmm3
    66a9: 45 85 c0                     	test	r8d, r8d
    66ac: 74 1f                        	je	0x66cd <Ep15AbslFlatFindPacked+0x1bd>
    66ae: 49 89 d7                     	mov	r15, rdx
    66b1: e9 5a ff ff ff               	jmp	0x6610 <Ep15AbslFlatFindPacked+0x100>
    66b6: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
    66c0: 5b                           	pop	rbx
    66c1: 31 c0                        	xor	eax, eax
    66c3: 41 5c                        	pop	r12
    66c5: 41 5d                        	pop	r13
    66c7: 41 5e                        	pop	r14
    66c9: 41 5f                        	pop	r15
    66cb: 5d                           	pop	rbp
    66cc: c3                           	ret
    66cd: 48 83 c3 10                  	add	rbx, 0x10
    66d1: 49 01 d9                     	add	r9, rbx
    66d4: c4 42 88 f5 c9               	bzhi	r9, r9, r14
    66d9: e9 e7 fe ff ff               	jmp	0x65c5 <Ep15AbslFlatFindPacked+0xb5>
    66de: 66 90                        	nop

# --- 4. Rust 1.91 SipHash-1-3 vs FxHash Lookup Kernels ---

/tmp/ep15_build/bench_rust:	file format elf64-x86-64

Disassembly of section .text:

0000000000020a40 <rust_siphash_find_packed>:
   20a40: 55                           	push	rbp
   20a41: 41 57                        	push	r15
   20a43: 41 56                        	push	r14
   20a45: 41 55                        	push	r13
   20a47: 41 54                        	push	r12
   20a49: 53                           	push	rbx
   20a4a: 48 85 d2                     	test	rdx, rdx
   20a4d: 0f 84 7c 02 00 00            	je	0x20ccf <rust_siphash_find_packed+0x28f>
   20a53: 48 83 7f 18 00               	cmp	qword ptr [rdi + 0x18], 0x0
   20a58: 0f 84 71 02 00 00            	je	0x20ccf <rust_siphash_find_packed+0x28f>
   20a5e: 48 8d 04 d6                  	lea	rax, [rsi + 8*rdx]
   20a62: 48 8b 57 28                  	mov	rdx, qword ptr [rdi + 0x28]
   20a66: 49 ba 6d 6f 64 6e 61 72 6f 64	movabs	r10, 0x646f72616e646f6d
   20a70: 49 b9 75 65 73 70 65 6d 6f 73	movabs	r9, 0x736f6d6570736575
   20a7a: 4c 8b 47 08                  	mov	r8, qword ptr [rdi + 0x8]
   20a7e: 49 bd 73 65 74 79 62 64 65 74	movabs	r13, 0x7465646279746573
   20a88: c5 f9 76 c0                  	vpcmpeqd	xmm0, xmm0, xmm0
   20a8c: 31 db                        	xor	ebx, ebx
   20a8e: 48 89 44 24 f0               	mov	qword ptr [rsp - 0x10], rax
   20a93: 48 8b 07                     	mov	rax, qword ptr [rdi]
   20a96: 49 31 d2                     	xor	r10, rdx
   20a99: 49 31 d5                     	xor	r13, rdx
   20a9c: 48 8d 48 f0                  	lea	rcx, [rax - 0x10]
   20aa0: 48 89 44 24 f8               	mov	qword ptr [rsp - 0x8], rax
   20aa5: 48 83 c0 f8                  	add	rax, -0x8
   20aa9: 48 89 44 24 d8               	mov	qword ptr [rsp - 0x28], rax
   20aae: 48 8b 47 20                  	mov	rax, qword ptr [rdi + 0x20]
   20ab2: 48 bf 61 72 65 6e 65 67 79 6c	movabs	rdi, 0x6c7967656e657261
   20abc: 48 89 4c 24 e0               	mov	qword ptr [rsp - 0x20], rcx
   20ac1: 49 31 c1                     	xor	r9, rax
   20ac4: 48 31 c7                     	xor	rdi, rax
   20ac7: 4d 01 d1                     	add	r9, r10
   20aca: c4 c3 fb f0 c1 20            	rorx	rax, r9, 0x20
   20ad0: 48 89 44 24 e8               	mov	qword ptr [rsp - 0x18], rax
   20ad5: c4 c3 fb f0 c2 33            	rorx	rax, r10, 0x33
   20adb: 4c 31 c8                     	xor	rax, r9
   20ade: 4c 8d 3c 07                  	lea	r15, [rdi + rax]
   20ae2: c4 63 fb f0 e0 2f            	rorx	r12, rax, 0x2f
   20ae8: 31 c0                        	xor	eax, eax
   20aea: e9 8f 00 00 00               	jmp	0x20b7e <rust_siphash_find_packed+0x13e>
   20aef: 90                           	nop
   20af0: 48 8b 4c 24 f8               	mov	rcx, qword ptr [rsp - 0x8]
   20af5: 4d 21 c2                     	and	r10, r8
   20af8: c4 a1 7a 6f 14 11            	vmovdqu	xmm2, xmmword ptr [rcx + r10]
   20afe: 62 f1 6d 08 74 c1            	vpcmpeqb	k0, xmm2, xmm1
   20b04: c5 f8 98 c0                  	kortestw	k0, k0
   20b08: 74 36                        	je	0x20b40 <rust_siphash_find_packed+0x100>
   20b0a: c5 fb 93 c8                  	kmovd	ecx, k0
   20b0e: f3 44 0f bc d9               	tzcnt	r11d, ecx
   20b13: 4c 8b 74 24 e0               	mov	r14, qword ptr [rsp - 0x20]
   20b18: 4d 01 d3                     	add	r11, r10
   20b1b: 4d 21 c3                     	and	r11, r8
   20b1e: 4c 89 da                     	mov	rdx, r11
   20b21: 48 c1 e2 04                  	shl	rdx, 0x4
   20b25: 49 29 d6                     	sub	r14, rdx
   20b28: 49 3b 2e                     	cmp	rbp, qword ptr [r14]
   20b2b: 74 33                        	je	0x20b60 <rust_siphash_find_packed+0x120>
   20b2d: 8d 51 ff                     	lea	edx, [rcx - 0x1]
   20b30: 66 21 ca                     	and	dx, cx
   20b33: 89 d1                        	mov	ecx, edx
   20b35: 75 d7                        	jne	0x20b0e <rust_siphash_find_packed+0xce>
   20b37: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   20b40: 62 f1 6d 08 74 c0            	vpcmpeqb	k0, xmm2, xmm0
   20b46: c5 f8 98 c0                  	kortestw	k0, k0
   20b4a: 75 27                        	jne	0x20b73 <rust_siphash_find_packed+0x133>
   20b4c: 4f 8d 54 0a 10               	lea	r10, [r10 + r9 + 0x10]
   20b51: 49 83 c1 10                  	add	r9, 0x10
   20b55: eb 99                        	jmp	0x20af0 <rust_siphash_find_packed+0xb0>
   20b57: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   20b60: 48 8b 4c 24 d8               	mov	rcx, qword ptr [rsp - 0x28]
   20b65: 49 f7 db                     	neg	r11
   20b68: 48 ff c0                     	inc	rax
   20b6b: 49 c1 e3 04                  	shl	r11, 0x4
   20b6f: 4a 03 1c 19                  	add	rbx, qword ptr [rcx + r11]
   20b73: 48 3b 74 24 f0               	cmp	rsi, qword ptr [rsp - 0x10]
   20b78: 0f 84 57 01 00 00            	je	0x20cd5 <rust_siphash_find_packed+0x295>
   20b7e: 48 8b 2e                     	mov	rbp, qword ptr [rsi]
   20b81: 48 8b 54 24 e8               	mov	rdx, qword ptr [rsp - 0x18]
   20b86: 48 83 c6 08                  	add	rsi, 0x8
   20b8a: 48 89 e9                     	mov	rcx, rbp
   20b8d: 4c 31 e9                     	xor	rcx, r13
   20b90: 4c 8d 0c 39                  	lea	r9, [rcx + rdi]
   20b94: c4 63 fb f0 d1 30            	rorx	r10, rcx, 0x30
   20b9a: 4c 01 f9                     	add	rcx, r15
   20b9d: 4d 31 ca                     	xor	r10, r9
   20ba0: c4 63 fb f0 c9 20            	rorx	r9, rcx, 0x20
   20ba6: 4c 31 e1                     	xor	rcx, r12
   20ba9: 4d 8d 1c 12                  	lea	r11, [r10 + rdx]
   20bad: c4 43 fb f0 d2 2b            	rorx	r10, r10, 0x2b
   20bb3: 48 ba 00 00 00 00 00 00 00 08	movabs	rdx, 0x800000000000000
   20bbd: 4d 31 da                     	xor	r10, r11
   20bc0: 49 31 eb                     	xor	r11, rbp
   20bc3: 49 31 d2                     	xor	r10, rdx
   20bc6: 49 01 cb                     	add	r11, rcx
   20bc9: c4 e3 fb f0 c9 33            	rorx	rcx, rcx, 0x33
   20bcf: 4d 01 d1                     	add	r9, r10
   20bd2: c4 43 fb f0 d2 30            	rorx	r10, r10, 0x30
   20bd8: 4c 31 d9                     	xor	rcx, r11
   20bdb: c4 43 fb f0 db 20            	rorx	r11, r11, 0x20
   20be1: 4d 31 ca                     	xor	r10, r9
   20be4: 49 01 c9                     	add	r9, rcx
   20be7: c4 e3 fb f0 c9 2f            	rorx	rcx, rcx, 0x2f
   20bed: 4d 01 d3                     	add	r11, r10
   20bf0: c4 43 fb f0 d2 2b            	rorx	r10, r10, 0x2b
   20bf6: 4c 31 c9                     	xor	rcx, r9
   20bf9: c4 43 fb f0 c9 20            	rorx	r9, r9, 0x20
   20bff: 4d 31 da                     	xor	r10, r11
   20c02: 49 31 d3                     	xor	r11, rdx
   20c05: 49 81 f1 ff 00 00 00         	xor	r9, 0xff
   20c0c: 49 01 cb                     	add	r11, rcx
   20c0f: c4 e3 fb f0 c9 33            	rorx	rcx, rcx, 0x33
   20c15: 4d 01 d1                     	add	r9, r10
   20c18: c4 43 fb f0 d2 30            	rorx	r10, r10, 0x30
   20c1e: 4c 31 d9                     	xor	rcx, r11
   20c21: 4d 31 ca                     	xor	r10, r9
   20c24: c4 43 fb f0 db 20            	rorx	r11, r11, 0x20
   20c2a: 49 01 c9                     	add	r9, rcx
   20c2d: c4 e3 fb f0 c9 2f            	rorx	rcx, rcx, 0x2f
   20c33: 4d 01 d3                     	add	r11, r10
   20c36: c4 43 fb f0 d2 2b            	rorx	r10, r10, 0x2b
   20c3c: 4c 31 c9                     	xor	rcx, r9
   20c3f: 4d 31 da                     	xor	r10, r11
   20c42: c4 43 fb f0 c9 20            	rorx	r9, r9, 0x20
   20c48: 49 01 cb                     	add	r11, rcx
   20c4b: c4 e3 fb f0 c9 33            	rorx	rcx, rcx, 0x33
   20c51: 4d 01 d1                     	add	r9, r10
   20c54: c4 43 fb f0 d2 30            	rorx	r10, r10, 0x30
   20c5a: 4c 31 d9                     	xor	rcx, r11
   20c5d: 4d 31 ca                     	xor	r10, r9
   20c60: c4 43 fb f0 db 20            	rorx	r11, r11, 0x20
   20c66: 49 01 c9                     	add	r9, rcx
   20c69: c4 e3 fb f0 c9 2f            	rorx	rcx, rcx, 0x2f
   20c6f: 4d 01 d3                     	add	r11, r10
   20c72: c4 43 fb f0 d2 2b            	rorx	r10, r10, 0x2b
   20c78: 4c 31 c9                     	xor	rcx, r9
   20c7b: 4d 31 da                     	xor	r10, r11
   20c7e: c4 43 fb f0 c9 20            	rorx	r9, r9, 0x20
   20c84: 49 01 cb                     	add	r11, rcx
   20c87: c4 e3 fb f0 c9 33            	rorx	rcx, rcx, 0x33
   20c8d: 4d 01 d1                     	add	r9, r10
   20c90: c4 43 fb f0 d2 30            	rorx	r10, r10, 0x30
   20c96: 4c 31 d9                     	xor	rcx, r11
   20c99: 4d 31 ca                     	xor	r10, r9
   20c9c: 49 01 c9                     	add	r9, rcx
   20c9f: c4 43 fb f0 da 2b            	rorx	r11, r10, 0x2b
   20ca5: c4 e3 fb f0 c9 2f            	rorx	rcx, rcx, 0x2f
   20cab: 49 31 cb                     	xor	r11, rcx
   20cae: c4 43 fb f0 d1 20            	rorx	r10, r9, 0x20
   20cb4: 4d 31 da                     	xor	r10, r11
   20cb7: 4d 31 ca                     	xor	r10, r9
   20cba: 45 31 c9                     	xor	r9d, r9d
   20cbd: 4c 89 d1                     	mov	rcx, r10
   20cc0: 48 c1 e9 39                  	shr	rcx, 0x39
   20cc4: 62 f2 7d 08 7a c9            	vpbroadcastb	xmm1, ecx
   20cca: e9 21 fe ff ff               	jmp	0x20af0 <rust_siphash_find_packed+0xb0>
   20ccf: 31 c0                        	xor	eax, eax
   20cd1: 31 db                        	xor	ebx, ebx
   20cd3: eb 0e                        	jmp	0x20ce3 <rust_siphash_find_packed+0x2a3>
   20cd5: 48 b9 15 7c 4a 7f b9 79 37 9e	movabs	rcx, -0x61c8864680b583eb
   20cdf: 48 0f af c1                  	imul	rax, rcx
   20ce3: 48 31 d8                     	xor	rax, rbx
   20ce6: 5b                           	pop	rbx
   20ce7: 41 5c                        	pop	r12
   20ce9: 41 5d                        	pop	r13
   20ceb: 41 5e                        	pop	r14
   20ced: 41 5f                        	pop	r15
   20cef: 5d                           	pop	rbp
   20cf0: c3                           	ret
   20cf1: cc                           	int3
   20cf2: cc                           	int3
   20cf3: cc                           	int3
   20cf4: cc                           	int3
   20cf5: cc                           	int3
   20cf6: cc                           	int3
   20cf7: cc                           	int3
   20cf8: cc                           	int3
   20cf9: cc                           	int3
   20cfa: cc                           	int3
   20cfb: cc                           	int3
   20cfc: cc                           	int3
   20cfd: cc                           	int3
   20cfe: cc                           	int3
   20cff: cc                           	int3

/tmp/ep15_build/bench_rust:	file format elf64-x86-64

Disassembly of section .text:

0000000000020550 <rust_fxhash_find_packed>:
   20550: 48 85 d2                     	test	rdx, rdx
   20553: 0f 84 e2 00 00 00            	je	0x2063b <rust_fxhash_find_packed+0xeb>
   20559: 48 83 7f 18 00               	cmp	qword ptr [rdi + 0x18], 0x0
   2055e: 0f 84 d7 00 00 00            	je	0x2063b <rust_fxhash_find_packed+0xeb>
   20564: 55                           	push	rbp
   20565: 41 57                        	push	r15
   20567: 41 56                        	push	r14
   20569: 41 55                        	push	r13
   2056b: 41 54                        	push	r12
   2056d: 53                           	push	rbx
   2056e: 4c 8b 07                     	mov	r8, qword ptr [rdi]
   20571: 48 8b 7f 08                  	mov	rdi, qword ptr [rdi + 0x8]
   20575: b1 3d                        	mov	cl, 0x3d
   20577: 49 8d 40 f8                  	lea	rax, [r8 - 0x8]
   2057b: 4d 8d 48 f0                  	lea	r9, [r8 - 0x10]
   2057f: 48 89 44 24 f0               	mov	qword ptr [rsp - 0x10], rax
   20584: 48 b8 ff ff ff ff ff ff ff 1f	movabs	rax, 0x1fffffffffffffff
   2058e: 48 01 d0                     	add	rax, rdx
   20591: c4 e2 f0 f5 d2               	bzhi	rdx, rdx, rcx
   20596: 48 89 44 24 f8               	mov	qword ptr [rsp - 0x8], rax
   2059b: 48 83 fa 01                  	cmp	rdx, 0x1
   2059f: 0f 85 9e 00 00 00            	jne	0x20643 <rust_fxhash_find_packed+0xf3>
   205a5: 31 c9                        	xor	ecx, ecx
   205a7: 31 d2                        	xor	edx, edx
   205a9: f6 44 24 f8 01               	test	byte ptr [rsp - 0x8], 0x1
   205ae: 0f 85 23 02 00 00            	jne	0x207d7 <rust_fxhash_find_packed+0x287>
   205b4: 48 8b 36                     	mov	rsi, qword ptr [rsi]
   205b7: 49 ba 95 0a 22 27 b7 c1 7c 51	movabs	r10, 0x517cc1b727220a95
   205c1: c5 f1 76 c9                  	vpcmpeqd	xmm1, xmm1, xmm1
   205c5: 45 31 db                     	xor	r11d, r11d
   205c8: 4c 0f af d6                  	imul	r10, rsi
   205cc: 4c 89 d0                     	mov	rax, r10
   205cf: 48 c1 e8 39                  	shr	rax, 0x39
   205d3: 62 f2 7d 08 7a c0            	vpbroadcastb	xmm0, eax
   205d9: 49 21 fa                     	and	r10, rdi
   205dc: c4 81 7a 6f 14 10            	vmovdqu	xmm2, xmmword ptr [r8 + r10]
   205e2: 62 f1 6d 08 74 c0            	vpcmpeqb	k0, xmm2, xmm0
   205e8: c5 f8 98 c0                  	kortestw	k0, k0
   205ec: 74 32                        	je	0x20620 <rust_fxhash_find_packed+0xd0>
   205ee: c5 fb 93 d8                  	kmovd	ebx, k0
   205f2: f3 44 0f bc f3               	tzcnt	r14d, ebx
   205f7: 4d 89 cf                     	mov	r15, r9
   205fa: 4d 01 d6                     	add	r14, r10
   205fd: 49 21 fe                     	and	r14, rdi
   20600: 4c 89 f0                     	mov	rax, r14
   20603: 48 c1 e0 04                  	shl	rax, 0x4
   20607: 49 29 c1                     	sub	r9, rax
   2060a: 49 3b 31                     	cmp	rsi, qword ptr [r9]
   2060d: 0f 84 b1 01 00 00            	je	0x207c4 <rust_fxhash_find_packed+0x274>
   20613: 8d 43 ff                     	lea	eax, [rbx - 0x1]
   20616: 4d 89 f9                     	mov	r9, r15
   20619: 66 21 d8                     	and	ax, bx
   2061c: 89 c3                        	mov	ebx, eax
   2061e: 75 d2                        	jne	0x205f2 <rust_fxhash_find_packed+0xa2>
   20620: 62 f1 6d 08 74 c1            	vpcmpeqb	k0, xmm2, xmm1
   20626: c5 f8 98 c0                  	kortestw	k0, k0
   2062a: 0f 85 a7 01 00 00            	jne	0x207d7 <rust_fxhash_find_packed+0x287>
   20630: 4f 8d 54 1a 10               	lea	r10, [r10 + r11 + 0x10]
   20635: 49 83 c3 10                  	add	r11, 0x10
   20639: eb 9e                        	jmp	0x205d9 <rust_fxhash_find_packed+0x89>
   2063b: 31 c0                        	xor	eax, eax
   2063d: 31 c9                        	xor	ecx, ecx
   2063f: 48 31 c8                     	xor	rax, rcx
   20642: c3                           	ret
   20643: c4 e2 f0 f5 d8               	bzhi	rbx, rax, rcx
   20648: c5 f9 76 c0                  	vpcmpeqd	xmm0, xmm0, xmm0
   2064c: 31 c9                        	xor	ecx, ecx
   2064e: 31 d2                        	xor	edx, edx
   20650: 45 31 f6                     	xor	r14d, r14d
   20653: 4d 89 cb                     	mov	r11, r9
   20656: 48 ff c3                     	inc	rbx
   20659: 48 83 e3 fe                  	and	rbx, -0x2
   2065d: e9 91 00 00 00               	jmp	0x206f3 <rust_fxhash_find_packed+0x1a3>
   20662: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00    	nop	word ptr cs:[rax + rax]
   20670: 49 21 fc                     	and	r12, rdi
   20673: c4 81 7a 6f 14 20            	vmovdqu	xmm2, xmmword ptr [r8 + r12]
   20679: 62 f1 6d 08 74 c1            	vpcmpeqb	k0, xmm2, xmm1
   2067f: c5 f8 98 c0                  	kortestw	k0, k0
   20683: 74 2b                        	je	0x206b0 <rust_fxhash_find_packed+0x160>
   20685: c5 fb 93 e8                  	kmovd	ebp, k0
   20689: f3 44 0f bc d5               	tzcnt	r10d, ebp
   2068e: 4d 01 e2                     	add	r10, r12
   20691: 49 21 fa                     	and	r10, rdi
   20694: 4c 89 d0                     	mov	rax, r10
   20697: 48 c1 e0 04                  	shl	rax, 0x4
   2069b: 49 29 c1                     	sub	r9, rax
   2069e: 4d 3b 39                     	cmp	r15, qword ptr [r9]
   206a1: 74 2d                        	je	0x206d0 <rust_fxhash_find_packed+0x180>
   206a3: 8d 45 ff                     	lea	eax, [rbp - 0x1]
   206a6: 4d 89 d9                     	mov	r9, r11
   206a9: 66 21 e8                     	and	ax, bp
   206ac: 89 c5                        	mov	ebp, eax
   206ae: 75 d9                        	jne	0x20689 <rust_fxhash_find_packed+0x139>
   206b0: 62 f1 6d 08 74 c0            	vpcmpeqb	k0, xmm2, xmm0
   206b6: c5 f8 98 c0                  	kortestw	k0, k0
   206ba: 75 2a                        	jne	0x206e6 <rust_fxhash_find_packed+0x196>
   206bc: 4f 8d 64 2c 10               	lea	r12, [r12 + r13 + 0x10]
   206c1: 49 83 c5 10                  	add	r13, 0x10
   206c5: eb a9                        	jmp	0x20670 <rust_fxhash_find_packed+0x120>
   206c7: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   206d0: 48 8b 44 24 f0               	mov	rax, qword ptr [rsp - 0x10]
   206d5: 49 f7 da                     	neg	r10
   206d8: 48 ff c2                     	inc	rdx
   206db: 4d 89 d9                     	mov	r9, r11
   206de: 49 c1 e2 04                  	shl	r10, 0x4
   206e2: 4a 03 0c 10                  	add	rcx, qword ptr [rax + r10]
   206e6: 49 83 c6 02                  	add	r14, 0x2
   206ea: 49 39 de                     	cmp	r14, rbx
   206ed: 0f 84 b6 fe ff ff            	je	0x205a9 <rust_fxhash_find_packed+0x59>
   206f3: 4c 8b 3e                     	mov	r15, qword ptr [rsi]
   206f6: 48 b8 95 0a 22 27 b7 c1 7c 51	movabs	rax, 0x517cc1b727220a95
   20700: 45 31 ed                     	xor	r13d, r13d
   20703: 4d 89 fc                     	mov	r12, r15
   20706: 4c 0f af e0                  	imul	r12, rax
   2070a: 4d 89 e2                     	mov	r10, r12
   2070d: 49 c1 ea 39                  	shr	r10, 0x39
   20711: 62 d2 7d 08 7a ca            	vpbroadcastb	xmm1, r10d
   20717: 49 21 fc                     	and	r12, rdi
   2071a: c4 81 7a 6f 14 20            	vmovdqu	xmm2, xmmword ptr [r8 + r12]
   20720: 62 f1 6d 08 74 c1            	vpcmpeqb	k0, xmm2, xmm1
   20726: c5 f8 98 c0                  	kortestw	k0, k0
   2072a: 74 34                        	je	0x20760 <rust_fxhash_find_packed+0x210>
   2072c: c5 fb 93 e8                  	kmovd	ebp, k0
   20730: f3 44 0f bc d5               	tzcnt	r10d, ebp
   20735: 4d 01 e2                     	add	r10, r12
   20738: 49 21 fa                     	and	r10, rdi
   2073b: 4c 89 d0                     	mov	rax, r10
   2073e: 48 c1 e0 04                  	shl	rax, 0x4
   20742: 49 29 c1                     	sub	r9, rax
   20745: 4d 3b 39                     	cmp	r15, qword ptr [r9]
   20748: 74 36                        	je	0x20780 <rust_fxhash_find_packed+0x230>
   2074a: 8d 45 ff                     	lea	eax, [rbp - 0x1]
   2074d: 4d 89 d9                     	mov	r9, r11
   20750: 66 21 e8                     	and	ax, bp
   20753: 89 c5                        	mov	ebp, eax
   20755: 75 d9                        	jne	0x20730 <rust_fxhash_find_packed+0x1e0>
   20757: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   20760: 62 f1 6d 08 74 c0            	vpcmpeqb	k0, xmm2, xmm0
   20766: c5 f8 98 c0                  	kortestw	k0, k0
   2076a: 75 2a                        	jne	0x20796 <rust_fxhash_find_packed+0x246>
   2076c: 4f 8d 64 2c 10               	lea	r12, [r12 + r13 + 0x10]
   20771: 49 83 c5 10                  	add	r13, 0x10
   20775: eb a0                        	jmp	0x20717 <rust_fxhash_find_packed+0x1c7>
   20777: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   20780: 48 8b 44 24 f0               	mov	rax, qword ptr [rsp - 0x10]
   20785: 49 f7 da                     	neg	r10
   20788: 48 ff c2                     	inc	rdx
   2078b: 4d 89 d9                     	mov	r9, r11
   2078e: 49 c1 e2 04                  	shl	r10, 0x4
   20792: 4a 03 0c 10                  	add	rcx, qword ptr [rax + r10]
   20796: 4c 8b 7e 08                  	mov	r15, qword ptr [rsi + 0x8]
   2079a: 48 b8 95 0a 22 27 b7 c1 7c 51	movabs	rax, 0x517cc1b727220a95
   207a4: 48 83 c6 10                  	add	rsi, 0x10
   207a8: 45 31 ed                     	xor	r13d, r13d
   207ab: 4d 89 fc                     	mov	r12, r15
   207ae: 4c 0f af e0                  	imul	r12, rax
   207b2: 4c 89 e0                     	mov	rax, r12
   207b5: 48 c1 e8 39                  	shr	rax, 0x39
   207b9: 62 f2 7d 08 7a c8            	vpbroadcastb	xmm1, eax
   207bf: e9 ac fe ff ff               	jmp	0x20670 <rust_fxhash_find_packed+0x120>
   207c4: 48 8b 44 24 f0               	mov	rax, qword ptr [rsp - 0x10]
   207c9: 49 f7 de                     	neg	r14
   207cc: 48 ff c2                     	inc	rdx
   207cf: 49 c1 e6 04                  	shl	r14, 0x4
   207d3: 4a 03 0c 30                  	add	rcx, qword ptr [rax + r14]
   207d7: 48 b8 15 7c 4a 7f b9 79 37 9e	movabs	rax, -0x61c8864680b583eb
   207e1: 48 0f af c2                  	imul	rax, rdx
   207e5: 5b                           	pop	rbx
   207e6: 41 5c                        	pop	r12
   207e8: 41 5d                        	pop	r13
   207ea: 41 5e                        	pop	r14
   207ec: 41 5f                        	pop	r15
   207ee: 5d                           	pop	rbp
   207ef: 48 31 c8                     	xor	rax, rcx
   207f2: c3                           	ret
   207f3: cc                           	int3
   207f4: cc                           	int3
   207f5: cc                           	int3
   207f6: cc                           	int3
   207f7: cc                           	int3
   207f8: cc                           	int3
   207f9: cc                           	int3
   207fa: cc                           	int3
   207fb: cc                           	int3
   207fc: cc                           	int3
   207fd: cc                           	int3
   207fe: cc                           	int3
   207ff: cc                           	int3

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
  map_fast64.go:62	0x411046		488d05a33a0b00		LEAQ 0xb3aa3(IP), AX			
  map_fast64.go:62	0x41104d		bb21000000		MOVL $0x21, BX				
  map_fast64.go:62	0x411052		e8e93d0200		CALL runtime.fatal(SB)			
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
  map_fast64.go:59	0x4110f0		488d05e96d1b00		LEAQ runtime.zeroVal(SB), AX		
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
  map_fast64.go:90	0x41111c		488d05bd6d1b00		LEAQ runtime.zeroVal(SB), AX		
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
  runtime_fast64_swiss.go:86	0x407060		493b6610		CMPQ SP, 0x10(R14)			
  runtime_fast64_swiss.go:86	0x407064		0f86d9010000		JBE 0x407243				
  runtime_fast64_swiss.go:86	0x40706a		55			PUSHQ BP				
  runtime_fast64_swiss.go:86	0x40706b		4889e5			MOVQ SP, BP				
  runtime_fast64_swiss.go:86	0x40706e		4883ec18		SUBQ $0x18, SP				
  runtime_fast64_swiss.go:93	0x407072		4885db			TESTQ BX, BX				
  runtime_fast64_swiss.go:93	0x407075		0f84c6000000		JE 0x407141				
  map.go:389			0x40707b		48833b00		CMPQ 0(BX), $0x0			
  map.go:389			0x40707f		90			NOPL					
  runtime_fast64_swiss.go:93	0x407080		0f84bb000000		JE 0x407141				
  runtime_fast64_swiss.go:97	0x407086		807b2200		CMPB 0x22(BX), $0x0			
  runtime_fast64_swiss.go:97	0x40708a		0f8595000000		JNE 0x407125				
  runtime_fast64_swiss.go:102	0x407090		48837b1800		CMPQ 0x18(BX), $0x0			
  runtime_fast64_swiss.go:102	0x407095		7524			JNE 0x4070bb				
  runtime_fast64_swiss.go:104	0x407097		488b5310		MOVQ 0x10(BX), DX			
  runtime_fast64_swiss.go:106	0x40709b		488b32			MOVQ 0(DX), SI				
  group.go:210			0x40709e		66480f6ec6		MOVQ SI, X0				
  group.go:210			0x4070a3		660fd7f0		PMOVMSKB X0, SI				
  group.go:210			0x4070a7		f7d6			NOTL SI					
  runtime_fast64_swiss.go:107	0x4070a9		90			NOPL					
  runtime_fast64_swiss.go:108	0x4070aa		488b7858		MOVQ 0x58(AX), DI			
  group.go:210			0x4070ae		400fb6f6		MOVZX SI, SI				
  group.go:281			0x4070b2		4883c208		ADDQ $0x8, DX				
  runtime_fast64_swiss.go:109	0x4070b6		e95a010000		JMP 0x407215				
  runtime_fast64_swiss.go:93	0x4070bb		4889442428		MOVQ AX, 0x28(SP)			
  runtime_fast64_swiss.go:93	0x4070c0		48895c2430		MOVQ BX, 0x30(SP)			
  runtime_fast64_swiss.go:93	0x4070c5		48894c2438		MOVQ CX, 0x38(SP)			
  runtime_fast64_swiss.go:120	0x4070ca		48894c2410		MOVQ CX, 0x10(SP)			
  runtime_fast64_swiss.go:121	0x4070cf		488b5048		MOVQ 0x48(AX), DX			
  runtime_fast64_swiss.go:121	0x4070d3		488b0a			MOVQ 0(DX), CX				
  runtime_fast64_swiss.go:121	0x4070d6		488b5b08		MOVQ 0x8(BX), BX			
  runtime_fast64_swiss.go:121	0x4070da		488d442410		LEAQ 0x10(SP), AX			
  runtime_fast64_swiss.go:121	0x4070df		90			NOPL					
  runtime_fast64_swiss.go:121	0x4070e0		ffd1			CALL CX					
  map.go:330			0x4070e2		488b4c2430		MOVQ 0x30(SP), CX			
  map.go:330			0x4070e7		4883791801		CMPQ 0x18(CX), $0x1			
  map.go:330			0x4070ec		7504			JNE 0x4070f2				
  map.go:330			0x4070ee		31d2			XORL DX, DX				
  runtime_fast64_swiss.go:124	0x4070f0		eb18			JMP 0x40710a				
  map.go:333			0x4070f2		0fb65121		MOVZX 0x21(CX), DX			
  runtime_fast64_swiss.go:93	0x4070f6		4889cb			MOVQ CX, BX				
  map.go:333			0x4070f9		89d1			MOVL DX, CX				
  map.go:333			0x4070fb		4889c6			MOVQ AX, SI				
  map.go:333			0x4070fe		48d3e8			SHRQ CL, AX				
  map.go:337			0x407101		4889d9			MOVQ BX, CX				
  runtime_fast64_swiss.go:124	0x407104		4889c2			MOVQ AX, DX				
  map.go:184			0x407107		4889f0			MOVQ SI, AX				
  map.go:337			0x40710a		488b4910		MOVQ 0x10(CX), CX			
  map.go:337			0x40710e		488b0cd1		MOVQ 0(CX)(DX*8), CX			
  runtime_fast64_swiss.go:128	0x407112		488b5118		MOVQ 0x18(CX), DX			
  map.go:184			0x407116		4889c6			MOVQ AX, SI				
  map.go:184			0x407119		48c1e807		SHRQ $0x7, AX				
  runtime_fast64_swiss.go:125	0x40711d		90			NOPL					
  table.go:1145			0x40711e		4821d0			ANDQ DX, AX				
  table.go:1145			0x407121		31db			XORL BX, BX				
  runtime_fast64_swiss.go:129	0x407123		eb3c			JMP 0x407161				
  runtime_fast64_swiss.go:98	0x407125		488d05498a0d00		LEAQ 0xd8a49(IP), AX			
  runtime_fast64_swiss.go:98	0x40712c		bb21000000		MOVL $0x21, BX				
  runtime_fast64_swiss.go:98	0x407131		e80a390600		CALL internal/runtime/maps.fatal(SB)	
  runtime_fast64_swiss.go:99	0x407136		31c0			XORL AX, AX				
  runtime_fast64_swiss.go:99	0x407138		31db			XORL BX, BX				
  runtime_fast64_swiss.go:99	0x40713a		4883c418		ADDQ $0x18, SP				
  runtime_fast64_swiss.go:99	0x40713e		5d			POPQ BP					
  runtime_fast64_swiss.go:99	0x40713f		90			NOPL					
  runtime_fast64_swiss.go:99	0x407140		c3			RET					
  runtime_fast64_swiss.go:94	0x407141		488d05f88d1b00		LEAQ runtime.zeroVal(SB), AX		
  runtime_fast64_swiss.go:94	0x407148		31db			XORL BX, BX				
  runtime_fast64_swiss.go:94	0x40714a		4883c418		ADDQ $0x18, SP				
  runtime_fast64_swiss.go:94	0x40714e		5d			POPQ BP					
  runtime_fast64_swiss.go:94	0x40714f		c3			RET					
  table.go:1152			0x407150		488d0403		LEAQ 0(BX)(AX*1), AX			
  table.go:1152			0x407154		488d4001		LEAQ 0x1(AX), AX			
  table.go:1151			0x407158		48ffc3			INCQ BX					
  table.go:1152			0x40715b		4821d0			ANDQ DX, AX				
  map.go:191			0x40715e		4c89d6			MOVQ R10, SI				
  group.go:319			0x407161		488b7c2428		MOVQ 0x28(SP), DI			
  group.go:319			0x407166		4c8b4750		MOVQ 0x50(DI), R8			
  group.go:319			0x40716a		4c0fafc0		IMULQ AX, R8				
  group.go:322			0x40716e		4c034110		ADDQ 0x10(CX), R8			
  runtime_fast64_swiss.go:132	0x407172		4d8b08			MOVQ 0(R8), R9				
  map.go:191			0x407175		4989f2			MOVQ SI, R10				
  map.go:191			0x407178		83e67f			ANDL $0x7f, SI				
  group.go:148			0x40717b		66480f6ec6		MOVQ SI, X0				
  group.go:148			0x407180		660f60c0		PUNPCKLBW X0, X0			
  group.go:148			0x407184		f20f70c000		PSHUFLW $0x0, X0, X0			
  group.go:148			0x407189		66490f6ec9		MOVQ R9, X1				
  group.go:148			0x40718e		660f74c1		PCMPEQB X1, X0				
  group.go:148			0x407192		660fd7f0		PMOVMSKB X0, SI				
  runtime_fast64_swiss.go:130	0x407196		90			NOPL					
  group.go:148			0x407197		400fb6f6		MOVZX SI, SI				
  runtime_fast64_swiss.go:134	0x40719b		4c8b4c2438		MOVQ 0x38(SP), R9			
  runtime_fast64_swiss.go:134	0x4071a0		eb2d			JMP 0x4071cf				
  group.go:170			0x4071a2		48be8080808080808080	MOVQ $0x8080808080808080, SI		
  group.go:170			0x4071ac		66480f6ec6		MOVQ SI, X0				
  group.go:170			0x4071b1		660f74c1		PCMPEQB X1, X0				
  group.go:170			0x4071b5		66440fd7c0		PMOVMSKB X0, R8				
  group.go:170			0x4071ba		450fb6c0		MOVZX R8, R8				
  runtime_fast64_swiss.go:145	0x4071be		90			NOPL					
  runtime_fast64_swiss.go:145	0x4071bf		90			NOPL					
  runtime_fast64_swiss.go:146	0x4071c0		4d85c0			TESTQ R8, R8				
  runtime_fast64_swiss.go:146	0x4071c3		748b			JE 0x407150				
  runtime_fast64_swiss.go:146	0x4071c5		eb38			JMP 0x4071ff				
  group.go:64			0x4071c7		4c8d5eff		LEAQ -0x1(SI), R11			
  runtime_fast64_swiss.go:142	0x4071cb		90			NOPL					
  group.go:64			0x4071cc		4c21de			ANDQ R11, SI				
  runtime_fast64_swiss.go:134	0x4071cf		4885f6			TESTQ SI, SI				
  runtime_fast64_swiss.go:134	0x4071d2		74ce			JE 0x4071a2				
  group.go:279			0x4071d4		4c8b5f58		MOVQ 0x58(DI), R11			
  runtime_fast64_swiss.go:135	0x4071d8		90			NOPL					
  runtime_fast64_swiss.go:137	0x4071d9		90			NOPL					
  group.go:50			0x4071da		4c0fbce6		BSFQ SI, R12				
  group.go:279			0x4071de		4d0fafdc		IMULQ R12, R11				
  runtime_fast64_swiss.go:138	0x4071e2		4f8b640308		MOVQ 0x8(R11)(R8*1), R12		
  runtime_fast64_swiss.go:138	0x4071e7		4d39cc			CMPQ R12, R9				
  runtime_fast64_swiss.go:138	0x4071ea		75db			JNE 0x4071c7				
  runtime_fast64_swiss.go:139	0x4071ec		4b8d0403		LEAQ 0(R11)(R8*1), AX			
  runtime_fast64_swiss.go:139	0x4071f0		488d4010		LEAQ 0x10(AX), AX			
  runtime_fast64_swiss.go:140	0x4071f4		bb01000000		MOVL $0x1, BX				
  runtime_fast64_swiss.go:140	0x4071f9		4883c418		ADDQ $0x18, SP				
  runtime_fast64_swiss.go:140	0x4071fd		5d			POPQ BP					
  runtime_fast64_swiss.go:140	0x4071fe		c3			RET					
  runtime_fast64_swiss.go:149	0x4071ff		488d053a8d1b00		LEAQ runtime.zeroVal(SB), AX		
  runtime_fast64_swiss.go:149	0x407206		31db			XORL BX, BX				
  runtime_fast64_swiss.go:149	0x407208		4883c418		ADDQ $0x18, SP				

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
# EPISODE 16: PRODUCTION C++ HASH MAPS — INTEL-SYNTAX DISASSEMBLY (ZEN 4)
# ==============================================================================

# --- 1. Self-Contained Godbolt Comparison (Clang 18 -O3 -march=znver4) ---
	.text
	.intel_syntax noprefix
	.file	"godbolt_source.cc"
	.globl	_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E # -- Begin function _ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E,@function
_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E: # @_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E
.Lfunc_begin0:
	.cfi_startproc
	.cfi_personality 155, DW.ref.__gxx_personality_v0
	.cfi_lsda 27, .Lexception0
# %bb.0:
	push	r15
	.cfi_def_cfa_offset 16
	push	r14
	.cfi_def_cfa_offset 24
	push	r13
	.cfi_def_cfa_offset 32
	push	r12
	.cfi_def_cfa_offset 40
	push	rbx
	.cfi_def_cfa_offset 48
	sub	rsp, 32
	.cfi_def_cfa_offset 80
	.cfi_offset rbx, -48
	.cfi_offset r12, -40
	.cfi_offset r13, -32
	.cfi_offset r14, -24
	.cfi_offset r15, -16
	lea	r13, [rsp + 16]
	mov	r15, rdx
	mov	r14, rsi
	mov	rbx, rdi
	mov	qword ptr [rsp], r13
	test	rsi, rsi
	je	.LBB0_2
# %bb.1:
	test	r15, r15
	je	.LBB0_20
.LBB0_2:
	mov	r12, r13
	cmp	r14, 16
	jb	.LBB0_6
# %bb.3:
	test	r14, r14
	js	.LBB0_21
# %bb.4:
	mov	rdi, r14
	inc	rdi
	js	.LBB0_22
# %bb.5:
	call	_Znwm@PLT
	mov	r12, rax
	mov	qword ptr [rsp], rax
	mov	qword ptr [rsp + 16], r14
.LBB0_6:
	test	r14, r14
	je	.LBB0_10
# %bb.7:
	cmp	r14, 1
	jne	.LBB0_9
# %bb.8:
	movzx	eax, byte ptr [r15]
	mov	byte ptr [r12], al
	jmp	.LBB0_10
.LBB0_9:
	mov	rdi, r12
	mov	rsi, r15
	mov	rdx, r14
	call	memcpy@PLT
.LBB0_10:
	mov	qword ptr [rsp + 8], r14
	mov	byte ptr [r12 + r14], 0
.Ltmp0:
	mov	rsi, rsp
	mov	rdi, rbx
	call	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_
.Ltmp1:
# %bb.11:
	mov	rdi, qword ptr [rsp]
	mov	rbx, rax
	cmp	rdi, r13
	je	.LBB0_13
# %bb.12:
	call	_ZdlPv@PLT
.LBB0_13:
	test	rbx, rbx
	je	.LBB0_14
# %bb.15:
	mov	rax, qword ptr [rbx + 40]
	jmp	.LBB0_16
.LBB0_14:
	xor	eax, eax
.LBB0_16:
	add	rsp, 32
	.cfi_def_cfa_offset 48
	pop	rbx
	.cfi_def_cfa_offset 40
	pop	r12
	.cfi_def_cfa_offset 32
	pop	r13
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	ret
.LBB0_22:
	.cfi_def_cfa_offset 80
	call	_ZSt17__throw_bad_allocv@PLT
.LBB0_20:
	lea	rdi, [rip + .L.str]
	call	_ZSt19__throw_logic_errorPKc@PLT
.LBB0_21:
	lea	rdi, [rip + .L.str.1]
	call	_ZSt20__throw_length_errorPKc@PLT
.LBB0_17:
.Ltmp2:
	mov	rdi, qword ptr [rsp]
	mov	rbx, rax
	cmp	rdi, r13
	je	.LBB0_19
# %bb.18:
	call	_ZdlPv@PLT
.LBB0_19:
	mov	rdi, rbx
	call	_Unwind_Resume@PLT
.Lfunc_end0:
	.size	_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E, .Lfunc_end0-_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E
	.cfi_endproc
	.section	.gcc_except_table,"a",@progbits
	.p2align	2, 0x0
GCC_except_table0:
.Lexception0:
	.byte	255                             # @LPStart Encoding = omit
	.byte	255                             # @TType Encoding = omit
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
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp1-.Lfunc_begin0           # >> Call Site 3 <<
	.uleb128 .Lfunc_end0-.Ltmp1             #   Call between .Ltmp1 and .Lfunc_end0
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
.Lcst_end0:
	.p2align	2, 0x0
                                        # -- End function
	.text
	.globl	_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_21TransparentStringHashESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E # -- Begin function _ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_21TransparentStringHashESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_21TransparentStringHashESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E,@function
_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_21TransparentStringHashESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E: # @_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_21TransparentStringHashESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E
	.cfi_startproc
# %bb.0:
	sub	rsp, 24
	.cfi_def_cfa_offset 32
	mov	qword ptr [rsp + 8], rsi
	lea	rsi, [rsp + 8]
	mov	qword ptr [rsp + 16], rdx
	call	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_
	test	rax, rax
	je	.LBB1_1
# %bb.2:
	mov	rax, qword ptr [rax + 40]
	add	rsp, 24
	.cfi_def_cfa_offset 8
	ret
.LBB1_1:
	.cfi_def_cfa_offset 32
	xor	eax, eax
	add	rsp, 24
	.cfi_def_cfa_offset 8
	ret
.Lfunc_end1:
	.size	_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_21TransparentStringHashESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E, .Lfunc_end1-_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_21TransparentStringHashESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst16,"aM",@progbits,16
	.p2align	4, 0x0                          # -- Begin function _ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa
.LCPI2_0:
	.zero	16,128
	.text
	.globl	_ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa,@function
_ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa: # @_ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa
	.cfi_startproc
# %bb.0:
	vmovdqu	xmm0, xmmword ptr [rdi]
	vpbroadcastb	xmm1, esi
	xor	ecx, ecx
	vpcmpeqb	k1, xmm0, xmmword ptr [rip + .LCPI2_0]
	vpcmpeqb	k0, xmm0, xmm1
	kortestw	k1, k1
	kmovw	eax, k0
	setne	cl
	shl	rcx, 32
	or	rax, rcx
	ret
.Lfunc_end2:
	.size	_ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa, .Lfunc_end2-_ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa
	.cfi_endproc
                                        # -- End function
	.section	.text.__clang_call_terminate,"axG",@progbits,__clang_call_terminate,comdat
	.hidden	__clang_call_terminate          # -- Begin function __clang_call_terminate
	.weak	__clang_call_terminate
	.p2align	4, 0x90
	.type	__clang_call_terminate,@function
__clang_call_terminate:                 # @__clang_call_terminate
# %bb.0:
	push	rax
	call	__cxa_begin_catch@PLT
	call	_ZSt9terminatev@PLT
.Lfunc_end3:
	.size	__clang_call_terminate, .Lfunc_end3-__clang_call_terminate
                                        # -- End function
	.text
	.globl	_ZN7hotpath12godbolt_ep1613ProbeFollyF14EPKNS0_14F14ChunkHeaderEh # -- Begin function _ZN7hotpath12godbolt_ep1613ProbeFollyF14EPKNS0_14F14ChunkHeaderEh
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1613ProbeFollyF14EPKNS0_14F14ChunkHeaderEh,@function
_ZN7hotpath12godbolt_ep1613ProbeFollyF14EPKNS0_14F14ChunkHeaderEh: # @_ZN7hotpath12godbolt_ep1613ProbeFollyF14EPKNS0_14F14ChunkHeaderEh
	.cfi_startproc
# %bb.0:
	vmovdqa	xmm0, xmmword ptr [rdi]
	vpbroadcastb	xmm1, esi
	xor	edx, edx
	vpcmpeqb	k0, xmm0, xmm1
	vpextrb	ecx, xmm0, 15
	kmovd	eax, k0
	and	eax, 16383
	test	ecx, ecx
	sete	dl
	shl	rdx, 32
	or	rax, rdx
	ret
.Lfunc_end4:
	.size	_ZN7hotpath12godbolt_ep1613ProbeFollyF14EPKNS0_14F14ChunkHeaderEh, .Lfunc_end4-_ZN7hotpath12godbolt_ep1613ProbeFollyF14EPKNS0_14F14ChunkHeaderEh
	.cfi_endproc
                                        # -- End function
	.globl	_ZN7hotpath12godbolt_ep1617ProbeBoostGroup15EPKNS0_12BoostGroup15Ehm # -- Begin function _ZN7hotpath12godbolt_ep1617ProbeBoostGroup15EPKNS0_12BoostGroup15Ehm
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1617ProbeBoostGroup15EPKNS0_12BoostGroup15Ehm,@function
_ZN7hotpath12godbolt_ep1617ProbeBoostGroup15EPKNS0_12BoostGroup15Ehm: # @_ZN7hotpath12godbolt_ep1617ProbeBoostGroup15EPKNS0_12BoostGroup15Ehm
	.cfi_startproc
# %bb.0:
	vmovdqa	xmm0, xmmword ptr [rdi]
	vpbroadcastb	xmm1, esi
	and	edx, 7
	xor	esi, esi
	vpcmpeqb	k0, xmm0, xmm1
	vpextrb	ecx, xmm0, 15
	kmovd	eax, k0
	and	eax, 32767
	bt	ecx, edx
	setae	sil
	shl	rsi, 32
	or	rax, rsi
	ret
.Lfunc_end5:
	.size	_ZN7hotpath12godbolt_ep1617ProbeBoostGroup15EPKNS0_12BoostGroup15Ehm, .Lfunc_end5-_ZN7hotpath12godbolt_ep1617ProbeBoostGroup15EPKNS0_12BoostGroup15Ehm
	.cfi_endproc
                                        # -- End function
	.section	.text._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,"axG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,comdat
	.weak	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_ # -- Begin function _ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_
	.p2align	4, 0x90
	.type	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,@function
_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_: # @_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_
.Lfunc_begin1:
	.cfi_startproc
	.cfi_personality 155, DW.ref.__gxx_personality_v0
	.cfi_lsda 27, .Lexception1
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
	cmp	qword ptr [rdi + 24], 20
	mov	r12, rsi
	mov	rbx, rdi
	jbe	.LBB6_10
# %bb.1:
	mov	rdi, qword ptr [r12]
	mov	rsi, qword ptr [r12 + 8]
.Ltmp3:
	mov	edx, 3339675911
	call	_ZSt11_Hash_bytesPKvmm@PLT
.Ltmp4:
# %bb.2:
	mov	rbp, qword ptr [rbx + 8]
	mov	rcx, qword ptr [rbx]
	mov	r14, rax
	xor	edx, edx
	xor	r13d, r13d
	div	rbp
	mov	rax, qword ptr [rcx + 8*rdx]
	test	rax, rax
	je	.LBB6_28
# %bb.3:
	mov	rbx, qword ptr [rax]
	mov	r8, qword ptr [r12 + 8]
	mov	r15, rdx
	mov	rcx, qword ptr [rbx + 48]
	test	r8, r8
	je	.LBB6_20
# %bb.4:
	mov	rax, qword ptr [r12]
	xor	r13d, r13d
	mov	qword ptr [rsp], rax            # 8-byte Spill
	.p2align	4, 0x90
.LBB6_5:                                # =>This Inner Loop Header: Depth=1
	cmp	rcx, r14
	jne	.LBB6_8
# %bb.6:                                #   in Loop: Header=BB6_5 Depth=1
	cmp	r8, qword ptr [rbx + 16]
	jne	.LBB6_8
# %bb.7:                                #   in Loop: Header=BB6_5 Depth=1
	mov	rsi, qword ptr [rbx + 8]
	mov	rdi, qword ptr [rsp]            # 8-byte Reload
	mov	rdx, r8
	mov	r12, r8
	call	bcmp@PLT
	mov	r8, r12
	test	eax, eax
	je	.LBB6_27
.LBB6_8:                                #   in Loop: Header=BB6_5 Depth=1
	mov	rbx, qword ptr [rbx]
	test	rbx, rbx
	je	.LBB6_28
# %bb.9:                                #   in Loop: Header=BB6_5 Depth=1
	mov	rcx, qword ptr [rbx + 48]
	xor	edx, edx
	mov	rax, rcx
	div	rbp
	cmp	rdx, r15
	je	.LBB6_5
	jmp	.LBB6_28
.LBB6_10:
	mov	r13, qword ptr [rbx + 16]
	test	r13, r13
	je	.LBB6_18
# %bb.11:
	mov	r14, qword ptr [r12 + 8]
	test	r14, r14
	je	.LBB6_17
# %bb.12:
	mov	r15, qword ptr [r12]
	jmp	.LBB6_14
	.p2align	4, 0x90
.LBB6_13:                               #   in Loop: Header=BB6_14 Depth=1
	mov	r13, qword ptr [r13]
	test	r13, r13
	je	.LBB6_18
.LBB6_14:                               # =>This Inner Loop Header: Depth=1
	cmp	r14, qword ptr [r13 + 16]
	jne	.LBB6_13
# %bb.15:                               #   in Loop: Header=BB6_14 Depth=1
	mov	rsi, qword ptr [r13 + 8]
	mov	rdi, r15
	mov	rdx, r14
	call	bcmp@PLT
	test	eax, eax
	jne	.LBB6_13
	jmp	.LBB6_28
	.p2align	4, 0x90
.LBB6_16:                               #   in Loop: Header=BB6_17 Depth=1
	mov	r13, qword ptr [r13]
	test	r13, r13
	je	.LBB6_18
.LBB6_17:                               # =>This Inner Loop Header: Depth=1
	cmp	qword ptr [r13 + 16], 0
	jne	.LBB6_16
	jmp	.LBB6_28
.LBB6_18:
	xor	r13d, r13d
.LBB6_28:
	mov	rax, r13
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
.LBB6_20:
	.cfi_def_cfa_offset 64
	xor	r13d, r13d
	.p2align	4, 0x90
.LBB6_21:                               # =>This Inner Loop Header: Depth=1
	cmp	rcx, r14
	jne	.LBB6_23
# %bb.22:                               #   in Loop: Header=BB6_21 Depth=1
	cmp	qword ptr [rbx + 16], 0
	je	.LBB6_27
.LBB6_23:                               #   in Loop: Header=BB6_21 Depth=1
	mov	rbx, qword ptr [rbx]
	test	rbx, rbx
	je	.LBB6_28
# %bb.24:                               #   in Loop: Header=BB6_21 Depth=1
	mov	rcx, qword ptr [rbx + 48]
	xor	edx, edx
	mov	rax, rcx
	div	rbp
	cmp	rdx, r15
	je	.LBB6_21
	jmp	.LBB6_28
.LBB6_27:
	mov	r13, rbx
	jmp	.LBB6_28
.LBB6_29:
.Ltmp5:
	mov	rdi, rax
	call	__clang_call_terminate
.Lfunc_end6:
	.size	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_, .Lfunc_end6-_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_
	.cfi_endproc
	.section	.gcc_except_table._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,"aG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,comdat
	.p2align	2, 0x0
GCC_except_table6:
.Lexception1:
	.byte	255                             # @LPStart Encoding = omit
	.byte	155                             # @TType Encoding = indirect pcrel sdata4
	.uleb128 .Lttbase0-.Lttbaseref0
.Lttbaseref0:
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end1-.Lcst_begin1
.Lcst_begin1:
	.uleb128 .Ltmp3-.Lfunc_begin1           # >> Call Site 1 <<
	.uleb128 .Ltmp4-.Ltmp3                  #   Call between .Ltmp3 and .Ltmp4
	.uleb128 .Ltmp5-.Lfunc_begin1           #     jumps to .Ltmp5
	.byte	1                               #   On action: 1
.Lcst_end1:
	.byte	1                               # >> Action Record 1 <<
                                        #   Catch TypeInfo 1
	.byte	0                               #   No further actions
	.p2align	2, 0x0
                                        # >> Catch TypeInfos <<
	.long	0                               # TypeInfo 1
.Lttbase0:
	.p2align	2, 0x0
                                        # -- End function
	.section	.text._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,"axG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,comdat
	.weak	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_ # -- Begin function _ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_
	.p2align	4, 0x90
	.type	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,@function
_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_: # @_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_
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
	mov	r13, rsi
	mov	r14, rdi
	mov	rsi, qword ptr [rsi]
	mov	rdi, qword ptr [r13 + 8]
.Ltmp6:
	mov	edx, 3339675911
	call	_ZSt11_Hash_bytesPKvmm@PLT
.Ltmp7:
# %bb.1:
	mov	rbp, qword ptr [r14 + 8]
	mov	rcx, qword ptr [r14]
	mov	rbx, rax
	xor	eax, eax
	mov	qword ptr [rsp], rax            # 8-byte Spill
	mov	rax, rbx
	xor	edx, edx
	div	rbp
	mov	rax, qword ptr [rcx + 8*rdx]
	test	rax, rax
	je	.LBB7_16
# %bb.2:
	mov	r14, qword ptr [rax]
	mov	r12, qword ptr [r13]
	mov	r15, rdx
	mov	rcx, qword ptr [r14 + 48]
	test	r12, r12
	je	.LBB7_9
# %bb.3:
	mov	r13, qword ptr [r13 + 8]
	xor	eax, eax
	mov	qword ptr [rsp], rax            # 8-byte Spill
	.p2align	4, 0x90
.LBB7_4:                                # =>This Inner Loop Header: Depth=1
	cmp	rcx, rbx
	jne	.LBB7_7
# %bb.5:                                #   in Loop: Header=BB7_4 Depth=1
	cmp	r12, qword ptr [r14 + 16]
	jne	.LBB7_7
# %bb.6:                                #   in Loop: Header=BB7_4 Depth=1
	mov	rsi, qword ptr [r14 + 8]
	mov	rdi, r13
	mov	rdx, r12
	call	bcmp@PLT
	test	eax, eax
	je	.LBB7_15
.LBB7_7:                                #   in Loop: Header=BB7_4 Depth=1
	mov	r14, qword ptr [r14]
	test	r14, r14
	je	.LBB7_16
# %bb.8:                                #   in Loop: Header=BB7_4 Depth=1
	mov	rcx, qword ptr [r14 + 48]
	xor	edx, edx
	mov	rax, rcx
	div	rbp
	cmp	rdx, r15
	je	.LBB7_4
	jmp	.LBB7_16
.LBB7_9:
	xor	eax, eax
	mov	qword ptr [rsp], rax            # 8-byte Spill
	.p2align	4, 0x90
.LBB7_10:                               # =>This Inner Loop Header: Depth=1
	cmp	rcx, rbx
	jne	.LBB7_12
# %bb.11:                               #   in Loop: Header=BB7_10 Depth=1
	cmp	qword ptr [r14 + 16], 0
	je	.LBB7_15
.LBB7_12:                               #   in Loop: Header=BB7_10 Depth=1
	mov	r14, qword ptr [r14]
	test	r14, r14
	je	.LBB7_16
# %bb.13:                               #   in Loop: Header=BB7_10 Depth=1
	mov	rcx, qword ptr [r14 + 48]
	xor	edx, edx
	mov	rax, rcx
	div	rbp
	cmp	rdx, r15
	je	.LBB7_10
	jmp	.LBB7_16
.LBB7_15:
	mov	qword ptr [rsp], r14            # 8-byte Spill
.LBB7_16:
	mov	rax, qword ptr [rsp]            # 8-byte Reload
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
.LBB7_17:
	.cfi_def_cfa_offset 64
.Ltmp8:
	mov	rdi, rax
	call	__clang_call_terminate
.Lfunc_end7:
	.size	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_, .Lfunc_end7-_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_
	.cfi_endproc
	.section	.gcc_except_table._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,"aG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1621TransparentStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,comdat
	.p2align	2, 0x0
GCC_except_table7:
.Lexception2:
	.byte	255                             # @LPStart Encoding = omit
	.byte	155                             # @TType Encoding = indirect pcrel sdata4
	.uleb128 .Lttbase1-.Lttbaseref1
.Lttbaseref1:
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end2-.Lcst_begin2
.Lcst_begin2:
	.uleb128 .Ltmp6-.Lfunc_begin2           # >> Call Site 1 <<
	.uleb128 .Ltmp7-.Ltmp6                  #   Call between .Ltmp6 and .Ltmp7
	.uleb128 .Ltmp8-.Lfunc_begin2           #     jumps to .Ltmp8
	.byte	1                               #   On action: 1
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
	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"basic_string: construction from null is not valid"
	.size	.L.str, 50

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

# --- 2. Live Compiled std::unordered_map U64 Lookup (GCC 13 -O3 -march=native) ---

./bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

0000000000012150 <Ep16DisasmStdFindU64>:
   12150: f3 0f 1e fa                  	endbr64
   12154: 48 8b 47 18                  	mov	rax, qword ptr [rdi + 0x18]
   12158: 48 89 f1                     	mov	rcx, rsi
   1215b: 48 85 c0                     	test	rax, rax
   1215e: 75 30                        	jne	0x12190 <Ep16DisasmStdFindU64+0x40>
   12160: 48 8b 57 10                  	mov	rdx, qword ptr [rdi + 0x10]
   12164: 48 85 d2                     	test	rdx, rdx
   12167: 75 0f                        	jne	0x12178 <Ep16DisasmStdFindU64+0x28>
   12169: c3                           	ret
   1216a: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
   12170: 48 8b 12                     	mov	rdx, qword ptr [rdx]
   12173: 48 85 d2                     	test	rdx, rdx
   12176: 74 78                        	je	0x121f0 <Ep16DisasmStdFindU64+0xa0>
   12178: 48 39 4a 08                  	cmp	qword ptr [rdx + 0x8], rcx
   1217c: 75 f2                        	jne	0x12170 <Ep16DisasmStdFindU64+0x20>
   1217e: 48 8b 42 10                  	mov	rax, qword ptr [rdx + 0x10]
   12182: c3                           	ret
   12183: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1218e: 66 90                        	nop
   12190: 4c 8b 47 08                  	mov	r8, qword ptr [rdi + 0x8]
   12194: 48 89 f0                     	mov	rax, rsi
   12197: 31 d2                        	xor	edx, edx
   12199: 49 f7 f0                     	div	r8
   1219c: 48 8b 07                     	mov	rax, qword ptr [rdi]
   1219f: 4c 8b 0c d0                  	mov	r9, qword ptr [rax + 8*rdx]
   121a3: 49 89 d2                     	mov	r10, rdx
   121a6: 4d 85 c9                     	test	r9, r9
   121a9: 74 38                        	je	0x121e3 <Ep16DisasmStdFindU64+0x93>
   121ab: 49 8b 01                     	mov	rax, qword ptr [r9]
   121ae: 48 8b 70 08                  	mov	rsi, qword ptr [rax + 0x8]
   121b2: 48 39 ce                     	cmp	rsi, rcx
   121b5: 74 24                        	je	0x121db <Ep16DisasmStdFindU64+0x8b>
   121b7: 48 8b 38                     	mov	rdi, qword ptr [rax]
   121ba: 48 85 ff                     	test	rdi, rdi
   121bd: 74 24                        	je	0x121e3 <Ep16DisasmStdFindU64+0x93>
   121bf: 48 8b 77 08                  	mov	rsi, qword ptr [rdi + 0x8]
   121c3: 49 89 c1                     	mov	r9, rax
   121c6: 31 d2                        	xor	edx, edx
   121c8: 48 89 f0                     	mov	rax, rsi
   121cb: 49 f7 f0                     	div	r8
   121ce: 49 39 d2                     	cmp	r10, rdx
   121d1: 75 10                        	jne	0x121e3 <Ep16DisasmStdFindU64+0x93>
   121d3: 48 89 f8                     	mov	rax, rdi
   121d6: 48 39 ce                     	cmp	rsi, rcx
   121d9: 75 dc                        	jne	0x121b7 <Ep16DisasmStdFindU64+0x67>
   121db: 49 8b 11                     	mov	rdx, qword ptr [r9]
   121de: 48 85 d2                     	test	rdx, rdx
   121e1: 75 9b                        	jne	0x1217e <Ep16DisasmStdFindU64+0x2e>
   121e3: 31 c0                        	xor	eax, eax
   121e5: c3                           	ret
   121e6: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
   121f0: c3                           	ret
   121f1: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nop	word ptr cs:[rax + rax]

# --- 3. Live Compiled absl::flat_hash_map U64 Lookup (16-way SwissTable) ---

./bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

0000000000011d90 <Ep16DisasmAbslFlatFindU64>:
   11d90: f3 0f 1e fa                  	endbr64
   11d94: 0f b6 07                     	movzx	eax, byte ptr [rdi]
   11d97: 49 89 fb                     	mov	r11, rdi
   11d9a: 3c 01                        	cmp	al, 0x1
   11d9c: 0f 86 8e 00 00 00            	jbe	0x11e30 <Ep16DisasmAbslFlatFindU64+0xa0>
   11da2: 48 8b 0f                     	mov	rcx, qword ptr [rdi]
   11da5: 4c 8b 57 08                  	mov	r10, qword ptr [rdi + 0x8]
   11da9: 0f b6 cd                     	movzx	ecx, ch
   11dac: 41 0f 18 1a                  	prefetcht2	byte ptr [r10]
   11db0: 48 31 f1                     	xor	rcx, rsi
   11db3: 49 89 c8                     	mov	r8, rcx
   11db6: 48 b9 f5 8c 1e de e0 f9 d5 79	movabs	rcx, 0x79d5f9e0de1e8cf5
   11dc0: 4c 89 c2                     	mov	rdx, r8
   11dc3: c4 62 bb f6 c9               	mulx	r9, r8, rcx
   11dc8: 4c 89 c9                     	mov	rcx, r9
   11dcb: 4d 8b 4b 10                  	mov	r9, qword ptr [r11 + 0x10]
   11dcf: 4c 31 c1                     	xor	rcx, r8
   11dd2: 44 0f b6 c0                  	movzx	r8d, al
   11dd6: c4 e2 f8 f5 f9               	bzhi	rdi, rcx, rax
   11ddb: 48 c1 e9 39                  	shr	rcx, 0x39
   11ddf: 62 f2 7d 08 7a c1            	vpbroadcastb	xmm0, ecx
   11de5: 31 c9                        	xor	ecx, ecx
   11de7: c4 c1 7a 6f 0c 3a            	vmovdqu	xmm1, xmmword ptr [r10 + rdi]
   11ded: 48 89 f8                     	mov	rax, rdi
   11df0: 48 c1 e0 05                  	shl	rax, 0x5
   11df4: 41 0f 18 0c 01               	prefetcht0	byte ptr [r9 + rax]
   11df9: c5 f9 74 d1                  	vpcmpeqb	xmm2, xmm0, xmm1
   11dfd: c5 f9 d7 d2                  	vpmovmskb	edx, xmm2
   11e01: 85 d2                        	test	edx, edx
   11e03: 74 5b                        	je	0x11e60 <Ep16DisasmAbslFlatFindU64+0xd0>
   11e05: f3 0f bc c2                  	tzcnt	eax, edx
   11e09: 48 01 f8                     	add	rax, rdi
   11e0c: c4 e2 b8 f5 c0               	bzhi	rax, rax, r8
   11e11: 48 c1 e0 05                  	shl	rax, 0x5
   11e15: 4c 01 c8                     	add	rax, r9
   11e18: 48 39 30                     	cmp	qword ptr [rax], rsi
   11e1b: 75 33                        	jne	0x11e50 <Ep16DisasmAbslFlatFindU64+0xc0>
   11e1d: 48 8b 48 08                  	mov	rcx, qword ptr [rax + 0x8]
   11e21: 48 89 c8                     	mov	rax, rcx
   11e24: c3                           	ret
   11e25: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   11e30: 48 8b 07                     	mov	rax, qword ptr [rdi]
   11e33: 31 c9                        	xor	ecx, ecx
   11e35: 48 c1 e8 11                  	shr	rax, 0x11
   11e39: 74 e6                        	je	0x11e21 <Ep16DisasmAbslFlatFindU64+0x91>
   11e3b: 48 8b 47 10                  	mov	rax, qword ptr [rdi + 0x10]
   11e3f: 48 39 30                     	cmp	qword ptr [rax], rsi
   11e42: 74 d9                        	je	0x11e1d <Ep16DisasmAbslFlatFindU64+0x8d>
   11e44: eb db                        	jmp	0x11e21 <Ep16DisasmAbslFlatFindU64+0x91>
   11e46: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
   11e50: c4 e2 68 f3 ca               	blsr	edx, edx
   11e55: 75 ae                        	jne	0x11e05 <Ep16DisasmAbslFlatFindU64+0x75>
   11e57: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   11e60: c4 e2 71 08 c9               	vpsignb	xmm1, xmm1, xmm1
   11e65: c5 f9 d7 c1                  	vpmovmskb	eax, xmm1
   11e69: 85 c0                        	test	eax, eax
   11e6b: 74 04                        	je	0x11e71 <Ep16DisasmAbslFlatFindU64+0xe1>
   11e6d: 31 c9                        	xor	ecx, ecx
   11e6f: eb b0                        	jmp	0x11e21 <Ep16DisasmAbslFlatFindU64+0x91>
   11e71: 48 83 c1 10                  	add	rcx, 0x10
   11e75: 48 8d 04 39                  	lea	rax, [rcx + rdi]
   11e79: c4 e2 b8 f5 f8               	bzhi	rdi, rax, r8
   11e7e: e9 64 ff ff ff               	jmp	0x11de7 <Ep16DisasmAbslFlatFindU64+0x57>
   11e83: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00       	nop	word ptr cs:[rax + rax]

# --- 4. Live Compiled folly::F14ValueMap U64 Lookup (14-way F14 Chunk) ---

./bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

0000000000011e90 <Ep16DisasmFollyF14ValueFindU64>:
   11e90: f3 0f 1e fa                  	endbr64
   11e94: 48 8b 57 08                  	mov	rdx, qword ptr [rdi + 0x8]
   11e98: 48 89 f1                     	mov	rcx, rsi
   11e9b: 48 89 d0                     	mov	rax, rdx
   11e9e: 48 c1 e8 08                  	shr	rax, 0x8
   11ea2: 48 81 fa ff 01 00 00         	cmp	rdx, 0x1ff
   11ea9: 77 35                        	ja	0x11ee0 <Ep16DisasmFollyF14ValueFindU64+0x50>
   11eab: 48 85 c0                     	test	rax, rax
   11eae: 75 10                        	jne	0x11ec0 <Ep16DisasmFollyF14ValueFindU64+0x30>
   11eb0: 31 c0                        	xor	eax, eax
   11eb2: c3                           	ret
   11eb3: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   11ebe: 66 90                        	nop
   11ec0: 48 8b 47 10                  	mov	rax, qword ptr [rdi + 0x10]
   11ec4: 48 83 e0 f0                  	and	rax, -0x10
   11ec8: 48 3b 30                     	cmp	rsi, qword ptr [rax]
   11ecb: 75 e3                        	jne	0x11eb0 <Ep16DisasmFollyF14ValueFindU64+0x20>
   11ecd: 48 8b 40 08                  	mov	rax, qword ptr [rax + 0x8]
   11ed1: c3                           	ret
   11ed2: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   11edd: 0f 1f 00                     	nop	dword ptr [rax]
   11ee0: 31 c0                        	xor	eax, eax
   11ee2: 4c 8b 17                     	mov	r10, qword ptr [rdi]
   11ee5: 41 b9 01 00 00 00            	mov	r9d, 0x1
   11eeb: f2 48 0f 38 f1 c6            	crc32	rax, rsi
   11ef1: 4c 8d 04 06                  	lea	r8, [rsi + rax]
   11ef5: be 01 00 00 00               	mov	esi, 0x1
   11efa: c4 42 e9 f7 c9               	shlx	r9, r9, rdx
   11eff: 48 c1 e8 18                  	shr	rax, 0x18
   11f03: 48 0f 44 c6                  	cmove	rax, rsi
   11f07: 0f b6 f2                     	movzx	esi, dl
   11f0a: 4c 8d 5c 00 01               	lea	r11, [rax + rax + 0x1]
   11f0f: 62 f2 7d 08 7a c0            	vpbroadcastb	xmm0, eax
   11f15: c4 c2 c8 f5 c0               	bzhi	rax, r8, rsi
   11f1a: 48 69 c0 d0 01 00 00         	imul	rax, rax, 0x1d0
   11f21: 49 8d 3c 02                  	lea	rdi, [r10 + rax]
   11f25: 41 0f 18 8c 02 10 01 00 00   	prefetcht0	byte ptr [r10 + rax + 0x110]
   11f2e: c5 f9 74 0f                  	vpcmpeqb	xmm1, xmm0, xmmword ptr [rdi]
   11f32: c5 f9 d7 d1                  	vpmovmskb	edx, xmm1
   11f36: f7 c2 ff 3f 00 00            	test	edx, 0x3fff
   11f3c: 74 22                        	je	0x11f60 <Ep16DisasmFollyF14ValueFindU64+0xd0>
   11f3e: f3 0f bc c2                  	tzcnt	eax, edx
   11f42: c4 e2 68 f3 ca               	blsr	edx, edx
   11f47: 48 c1 e0 05                  	shl	rax, 0x5
   11f4b: 48 8d 44 07 10               	lea	rax, [rdi + rax + 0x10]
   11f50: 48 3b 08                     	cmp	rcx, qword ptr [rax]
   11f53: 75 e1                        	jne	0x11f36 <Ep16DisasmFollyF14ValueFindU64+0xa6>
   11f55: 48 8b 40 08                  	mov	rax, qword ptr [rax + 0x8]
   11f59: c3                           	ret
   11f5a: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
   11f60: 80 7f 0f 00                  	cmp	byte ptr [rdi + 0xf], 0x0
   11f64: 0f 84 46 ff ff ff            	je	0x11eb0 <Ep16DisasmFollyF14ValueFindU64+0x20>
   11f6a: 4d 01 d8                     	add	r8, r11
   11f6d: 49 ff c9                     	dec	r9
   11f70: 75 a3                        	jne	0x11f15 <Ep16DisasmFollyF14ValueFindU64+0x85>
   11f72: e9 39 ff ff ff               	jmp	0x11eb0 <Ep16DisasmFollyF14ValueFindU64+0x20>
   11f77: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]

# --- 5. Live Compiled folly::F14FastMap U64 Lookup (14-way F14VectorMap) ---

./bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

0000000000011f80 <Ep16DisasmFollyF14FastFindU64>:
   11f80: f3 0f 1e fa                  	endbr64
   11f84: 48 8b 57 10                  	mov	rdx, qword ptr [rdi + 0x10]
   11f88: 48 89 d0                     	mov	rax, rdx
   11f8b: 48 c1 e8 08                  	shr	rax, 0x8
   11f8f: 0f 84 cb 00 00 00            	je	0x12060 <Ep16DisasmFollyF14FastFindU64+0xe0>
   11f95: 55                           	push	rbp
   11f96: 31 c0                        	xor	eax, eax
   11f98: b9 01 00 00 00               	mov	ecx, 0x1
   11f9d: 41 ba 01 00 00 00            	mov	r10d, 0x1
   11fa3: 48 89 e5                     	mov	rbp, rsp
   11fa6: 41 54                        	push	r12
   11fa8: 53                           	push	rbx
   11fa9: 48 8b 5f 08                  	mov	rbx, qword ptr [rdi + 0x8]
   11fad: f2 48 0f 38 f1 c6            	crc32	rax, rsi
   11fb3: 4c 8d 04 06                  	lea	r8, [rsi + rax]
   11fb7: 44 0f b6 ca                  	movzx	r9d, dl
   11fbb: c4 42 e9 f7 d2               	shlx	r10, r10, rdx
   11fc0: 48 c1 e8 18                  	shr	rax, 0x18
   11fc4: 48 0f 44 c1                  	cmove	rax, rcx
   11fc8: 4c 8d 64 00 01               	lea	r12, [rax + rax + 0x1]
   11fcd: 62 f2 7d 08 7a c0            	vpbroadcastb	xmm0, eax
   11fd3: c4 c2 b0 f5 c8               	bzhi	rcx, r8, r9
   11fd8: 48 c1 e1 06                  	shl	rcx, 0x6
   11fdc: 48 01 d9                     	add	rcx, rbx
   11fdf: c5 f9 74 09                  	vpcmpeqb	xmm1, xmm0, xmmword ptr [rcx]
   11fe3: c5 f9 d7 d1                  	vpmovmskb	edx, xmm1
   11fe7: f7 c2 ff 0f 00 00            	test	edx, 0xfff
   11fed: 74 49                        	je	0x12038 <Ep16DisasmFollyF14FastFindU64+0xb8>
   11fef: 4c 8b 1f                     	mov	r11, qword ptr [rdi]
   11ff2: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   11ffd: 0f 1f 00                     	nop	dword ptr [rax]
   12000: f3 0f bc c2                  	tzcnt	eax, edx
   12004: c4 e2 68 f3 ca               	blsr	edx, edx
   12009: 83 c0 04                     	add	eax, 0x4
   1200c: 48 98                        	cdqe
   1200e: 8b 04 81                     	mov	eax, dword ptr [rcx + 4*rax]
   12011: 48 c1 e0 05                  	shl	rax, 0x5
   12015: 4c 01 d8                     	add	rax, r11
   12018: 48 3b 30                     	cmp	rsi, qword ptr [rax]
   1201b: 75 13                        	jne	0x12030 <Ep16DisasmFollyF14FastFindU64+0xb0>
   1201d: 5b                           	pop	rbx
   1201e: 48 8b 40 08                  	mov	rax, qword ptr [rax + 0x8]
   12022: 41 5c                        	pop	r12
   12024: 5d                           	pop	rbp
   12025: c3                           	ret
   12026: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
   12030: f7 c2 ff 0f 00 00            	test	edx, 0xfff
   12036: 75 c8                        	jne	0x12000 <Ep16DisasmFollyF14FastFindU64+0x80>
   12038: 80 79 0f 00                  	cmp	byte ptr [rcx + 0xf], 0x0
   1203c: 75 12                        	jne	0x12050 <Ep16DisasmFollyF14FastFindU64+0xd0>
   1203e: 5b                           	pop	rbx
   1203f: 31 c0                        	xor	eax, eax
   12041: 41 5c                        	pop	r12
   12043: 5d                           	pop	rbp
   12044: c3                           	ret
   12045: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   12050: 4d 01 e0                     	add	r8, r12
   12053: 49 ff ca                     	dec	r10
   12056: 0f 85 77 ff ff ff            	jne	0x11fd3 <Ep16DisasmFollyF14FastFindU64+0x53>
   1205c: eb e0                        	jmp	0x1203e <Ep16DisasmFollyF14FastFindU64+0xbe>
   1205e: 66 90                        	nop
   12060: 31 c0                        	xor	eax, eax
   12062: c3                           	ret
   12063: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00       	nop	word ptr cs:[rax + rax]

# --- 6. Live Compiled boost::unordered_flat_map U64 Lookup (15-way group15 + ofw) ---

./bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

0000000000012070 <Ep16DisasmBoostFlatFindU64>:
   12070: f3 0f 1e fa                  	endbr64
   12074: 49 89 f0                     	mov	r8, rsi
   12077: 55                           	push	rbp
   12078: 48 be 15 7c 4a 7f b9 79 37 9e	movabs	rsi, -0x61c8864680b583eb
   12082: 48 8d 0d 77 08 04 00         	lea	rcx, [rip + 0x40877]    # 0x52900 <_ZZN5boost9unordered6detail3foa7group15INS2_14plain_integralEE10match_wordEmE4word>
   12089: 4c 89 c2                     	mov	rdx, r8
   1208c: 48 89 e5                     	mov	rbp, rsp
   1208f: 41 56                        	push	r14
   12091: 49 89 fe                     	mov	r14, rdi
   12094: c4 e2 cb f6 fe               	mulx	rdi, rsi, rsi
   12099: 48 89 f8                     	mov	rax, rdi
   1209c: 53                           	push	rbx
   1209d: 49 8b 3e                     	mov	rdi, qword ptr [r14]
   120a0: 48 31 f0                     	xor	rax, rsi
   120a3: 49 8b 5e 10                  	mov	rbx, qword ptr [r14 + 0x10]
   120a7: 45 31 db                     	xor	r11d, r11d
   120aa: 0f b6 f0                     	movzx	esi, al
   120ad: c4 e2 79 58 0c b1            	vpbroadcastd	xmm1, dword ptr [rcx + 4*rsi]
   120b3: c4 e2 c3 f7 f8               	shrx	rdi, rax, rdi
   120b8: 48 89 fe                     	mov	rsi, rdi
   120bb: 48 c1 e6 04                  	shl	rsi, 0x4
   120bf: 48 01 de                     	add	rsi, rbx
   120c2: c5 f1 74 06                  	vpcmpeqb	xmm0, xmm1, xmmword ptr [rsi]
   120c6: c5 f9 d7 c8                  	vpmovmskb	ecx, xmm0
   120ca: 81 e1 ff 7f 00 00            	and	ecx, 0x7fff
   120d0: 75 1e                        	jne	0x120f0 <Ep16DisasmBoostFlatFindU64+0x80>
   120d2: 48 89 c1                     	mov	rcx, rax
   120d5: 4c 8d 0d 24 0c 04 00         	lea	r9, [rip + 0x40c24]     # 0x52d00 <_ZZNK5boost9unordered6detail3foa7group15INS2_14plain_integralEE17is_not_overflowedEmE5shift>
   120dc: 83 e1 07                     	and	ecx, 0x7
   120df: 41 0f b6 0c 09               	movzx	ecx, byte ptr [r9 + rcx]
   120e4: 22 4e 0f                     	and	cl, byte ptr [rsi + 0xf]
   120e7: 75 37                        	jne	0x12120 <Ep16DisasmBoostFlatFindU64+0xb0>
   120e9: 5b                           	pop	rbx
   120ea: 31 c0                        	xor	eax, eax
   120ec: 41 5e                        	pop	r14
   120ee: 5d                           	pop	rbp
   120ef: c3                           	ret
   120f0: 4c 69 d7 e0 01 00 00         	imul	r10, rdi, 0x1e0
   120f7: 4d 03 56 18                  	add	r10, qword ptr [r14 + 0x18]
   120fb: 41 0f 18 0a                  	prefetcht0	byte ptr [r10]
   120ff: f3 44 0f bc c9               	tzcnt	r9d, ecx
   12104: 49 c1 e1 05                  	shl	r9, 0x5
   12108: 4d 01 d1                     	add	r9, r10
   1210b: 4d 39 01                     	cmp	qword ptr [r9], r8
   1210e: 75 30                        	jne	0x12140 <Ep16DisasmBoostFlatFindU64+0xd0>
   12110: 5b                           	pop	rbx
   12111: 49 8b 41 08                  	mov	rax, qword ptr [r9 + 0x8]
   12115: 41 5e                        	pop	r14
   12117: 5d                           	pop	rbp
   12118: c3                           	ret
   12119: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]
   12120: 49 8b 4e 08                  	mov	rcx, qword ptr [r14 + 0x8]
   12124: 49 ff c3                     	inc	r11
   12127: 4c 01 df                     	add	rdi, r11
   1212a: 48 21 cf                     	and	rdi, rcx
   1212d: 4c 39 d9                     	cmp	rcx, r11
   12130: 73 86                        	jae	0x120b8 <Ep16DisasmBoostFlatFindU64+0x48>
   12132: eb b5                        	jmp	0x120e9 <Ep16DisasmBoostFlatFindU64+0x79>
   12134: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1213f: 90                           	nop
   12140: c4 e2 70 f3 c9               	blsr	ecx, ecx
   12145: 75 b8                        	jne	0x120ff <Ep16DisasmBoostFlatFindU64+0x8f>
   12147: eb 89                        	jmp	0x120d2 <Ep16DisasmBoostFlatFindU64+0x62>
   12149: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]

# --- 7. Live Compiled Story 1 Default vs Transparent Route Lookups ---

./bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000000d6d0 <Ep16StdDefaultFindRoute>:
    d6d0: f3 0f 1e fa                  	endbr64
    d6d4: 55                           	push	rbp
    d6d5: 48 89 e5                     	mov	rbp, rsp
    d6d8: 41 57                        	push	r15
    d6da: 41 56                        	push	r14
    d6dc: 41 55                        	push	r13
    d6de: 41 54                        	push	r12
    d6e0: 53                           	push	rbx
    d6e1: 48 81 ec 88 00 00 00         	sub	rsp, 0x88
    d6e8: 48 89 7d 98                  	mov	qword ptr [rbp - 0x68], rdi
    d6ec: 48 89 55 88                  	mov	qword ptr [rbp - 0x78], rdx
    d6f0: 64 48 8b 04 25 28 00 00 00   	mov	rax, qword ptr fs:[0x28]
    d6f9: 48 89 45 c8                  	mov	qword ptr [rbp - 0x38], rax
    d6fd: 31 c0                        	xor	eax, eax
    d6ff: 48 85 d2                     	test	rdx, rdx
    d702: 0f 84 b3 03 00 00            	je	0xdabb <Ep16StdDefaultFindRoute+0x3eb>
    d708: 48 89 f3                     	mov	rbx, rsi
    d70b: 45 31 ed                     	xor	r13d, r13d
    d70e: 4c 8d 65 b0                  	lea	r12, [rbp - 0x50]
    d712: 48 c7 85 78 ff ff ff 00 00 00 00     	mov	qword ptr [rbp - 0x88], 0x0
    d71d: 48 c7 45 80 00 00 00 00      	mov	qword ptr [rbp - 0x80], 0x0
    d725: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    d730: 4c 8b 73 08                  	mov	r14, qword ptr [rbx + 0x8]
    d734: 4c 8b 3b                     	mov	r15, qword ptr [rbx]
    d737: 4c 89 65 a0                  	mov	qword ptr [rbp - 0x60], r12
    d73b: 4d 85 f6                     	test	r14, r14
    d73e: 75 09                        	jne	0xd749 <Ep16StdDefaultFindRoute+0x79>
    d740: 4d 85 ff                     	test	r15, r15
    d743: 0f 85 79 03 00 00            	jne	0xdac2 <Ep16StdDefaultFindRoute+0x3f2>
    d749: 49 83 ff 0f                  	cmp	r15, 0xf
    d74d: 0f 87 8d 01 00 00            	ja	0xd8e0 <Ep16StdDefaultFindRoute+0x210>
    d753: 49 83 ff 01                  	cmp	r15, 0x1
    d757: 0f 85 73 01 00 00            	jne	0xd8d0 <Ep16StdDefaultFindRoute+0x200>
    d75d: 41 0f b6 06                  	movzx	eax, byte ptr [r14]
    d761: 88 45 b0                     	mov	byte ptr [rbp - 0x50], al
    d764: 4c 89 e0                     	mov	rax, r12
    d767: 4c 89 7d a8                  	mov	qword ptr [rbp - 0x58], r15
    d76b: 42 c6 04 38 00               	mov	byte ptr [rax + r15], 0x0
    d770: 48 8b 45 98                  	mov	rax, qword ptr [rbp - 0x68]
    d774: 48 83 78 18 14               	cmp	qword ptr [rax + 0x18], 0x14
    d779: 0f 87 a1 01 00 00            	ja	0xd920 <Ep16StdDefaultFindRoute+0x250>
    d77f: 4c 8b 78 10                  	mov	r15, qword ptr [rax + 0x10]
    d783: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
    d787: 4d 85 ff                     	test	r15, r15
    d78a: 0f 84 1a 01 00 00            	je	0xd8aa <Ep16StdDefaultFindRoute+0x1da>
    d790: 4c 8b 75 a8                  	mov	r14, qword ptr [rbp - 0x58]
    d794: 4d 85 f6                     	test	r14, r14
    d797: 0f 84 eb 00 00 00            	je	0xd888 <Ep16StdDefaultFindRoute+0x1b8>
    d79d: 4c 89 e0                     	mov	rax, r12
    d7a0: 48 89 5d 90                  	mov	qword ptr [rbp - 0x70], rbx
    d7a4: 4d 89 f4                     	mov	r12, r14
    d7a7: 48 89 fb                     	mov	rbx, rdi
    d7aa: 49 89 c6                     	mov	r14, rax
    d7ad: eb 0d                        	jmp	0xd7bc <Ep16StdDefaultFindRoute+0xec>
    d7af: 90                           	nop
    d7b0: 4d 8b 3f                     	mov	r15, qword ptr [r15]
    d7b3: 4d 85 ff                     	test	r15, r15
    d7b6: 0f 84 e4 00 00 00            	je	0xd8a0 <Ep16StdDefaultFindRoute+0x1d0>
    d7bc: 4d 3b 67 10                  	cmp	r12, qword ptr [r15 + 0x10]
    d7c0: 75 ee                        	jne	0xd7b0 <Ep16StdDefaultFindRoute+0xe0>
    d7c2: 49 8b 77 08                  	mov	rsi, qword ptr [r15 + 0x8]
    d7c6: 4c 89 e2                     	mov	rdx, r12
    d7c9: 48 89 df                     	mov	rdi, rbx
    d7cc: e8 2f 8f ff ff               	call	0x6700 <_ZSt24__throw_invalid_argumentPKc@plt+0xc>
    d7d1: 85 c0                        	test	eax, eax
    d7d3: 75 db                        	jne	0xd7b0 <Ep16StdDefaultFindRoute+0xe0>
    d7d5: 48 89 df                     	mov	rdi, rbx
    d7d8: 48 8b 5d 90                  	mov	rbx, qword ptr [rbp - 0x70]
    d7dc: 4d 89 f4                     	mov	r12, r14
    d7df: 4c 39 e7                     	cmp	rdi, r12
    d7e2: 74 0d                        	je	0xd7f1 <Ep16StdDefaultFindRoute+0x121>
    d7e4: 48 8b 45 b0                  	mov	rax, qword ptr [rbp - 0x50]
    d7e8: 48 8d 70 01                  	lea	rsi, [rax + 0x1]
    d7ec: e8 1f 8f ff ff               	call	0x6710 <memcmp@plt+0xc>
    d7f1: 41 8b 57 28                  	mov	edx, dword ptr [r15 + 0x28]
    d7f5: 41 8b 4f 2c                  	mov	ecx, dword ptr [r15 + 0x2c]
    d7f9: 48 ff 85 78 ff ff ff         	inc	qword ptr [rbp - 0x88]
    d800: 41 8b 47 34                  	mov	eax, dword ptr [r15 + 0x34]
    d804: 41 03 47 30                  	add	eax, dword ptr [r15 + 0x30]
    d808: 49 33 47 38                  	xor	rax, qword ptr [r15 + 0x38]
    d80c: 48 c1 e2 20                  	shl	rdx, 0x20
    d810: 48 c1 e1 10                  	shl	rcx, 0x10
    d814: 48 31 ca                     	xor	rdx, rcx
    d817: 48 31 d0                     	xor	rax, rdx
    d81a: 48 01 45 80                  	add	qword ptr [rbp - 0x80], rax
    d81e: 49 ff c5                     	inc	r13
    d821: 48 83 c3 10                  	add	rbx, 0x10
    d825: 4c 39 6d 88                  	cmp	qword ptr [rbp - 0x78], r13
    d829: 0f 85 01 ff ff ff            	jne	0xd730 <Ep16StdDefaultFindRoute+0x60>
    d82f: 48 8b 9d 78 ff ff ff         	mov	rbx, qword ptr [rbp - 0x88]
    d836: 48 b8 15 7c 4a 7f b9 79 37 9e	movabs	rax, -0x61c8864680b583eb
    d840: 48 0f af d8                  	imul	rbx, rax
    d844: 48 89 d8                     	mov	rax, rbx
    d847: 48 8b 5d 80                  	mov	rbx, qword ptr [rbp - 0x80]
    d84b: 48 31 d8                     	xor	rax, rbx
    d84e: 48 8b 55 c8                  	mov	rdx, qword ptr [rbp - 0x38]
    d852: 64 48 2b 14 25 28 00 00 00   	sub	rdx, qword ptr fs:[0x28]
    d85b: 0f 85 97 02 00 00            	jne	0xdaf8 <Ep16StdDefaultFindRoute+0x428>
    d861: 48 81 c4 88 00 00 00         	add	rsp, 0x88
    d868: 5b                           	pop	rbx
    d869: 41 5c                        	pop	r12
    d86b: 41 5d                        	pop	r13
    d86d: 41 5e                        	pop	r14
    d86f: 41 5f                        	pop	r15
    d871: 5d                           	pop	rbp
    d872: c3                           	ret
    d873: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    d87e: 66 90                        	nop
    d880: 4d 8b 3f                     	mov	r15, qword ptr [r15]
    d883: 4d 85 ff                     	test	r15, r15
    d886: 74 22                        	je	0xd8aa <Ep16StdDefaultFindRoute+0x1da>
    d888: 49 83 7f 10 00               	cmp	qword ptr [r15 + 0x10], 0x0
    d88d: 75 f1                        	jne	0xd880 <Ep16StdDefaultFindRoute+0x1b0>
    d88f: e9 4b ff ff ff               	jmp	0xd7df <Ep16StdDefaultFindRoute+0x10f>
    d894: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    d89f: 90                           	nop
    d8a0: 48 89 df                     	mov	rdi, rbx
    d8a3: 48 8b 5d 90                  	mov	rbx, qword ptr [rbp - 0x70]
    d8a7: 4d 89 f4                     	mov	r12, r14
    d8aa: 4c 39 e7                     	cmp	rdi, r12
    d8ad: 0f 84 6b ff ff ff            	je	0xd81e <Ep16StdDefaultFindRoute+0x14e>
    d8b3: 48 8b 45 b0                  	mov	rax, qword ptr [rbp - 0x50]
    d8b7: 48 8d 70 01                  	lea	rsi, [rax + 0x1]
    d8bb: e8 50 8e ff ff               	call	0x6710 <memcmp@plt+0xc>
    d8c0: e9 59 ff ff ff               	jmp	0xd81e <Ep16StdDefaultFindRoute+0x14e>
    d8c5: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    d8d0: 4d 85 ff                     	test	r15, r15
    d8d3: 0f 84 8b fe ff ff            	je	0xd764 <Ep16StdDefaultFindRoute+0x94>
    d8d9: 4c 89 e7                     	mov	rdi, r12
    d8dc: eb 27                        	jmp	0xd905 <Ep16StdDefaultFindRoute+0x235>
    d8de: 66 90                        	nop
    d8e0: 4d 85 ff                     	test	r15, r15
    d8e3: 0f 88 f4 01 00 00            	js	0xdadd <Ep16StdDefaultFindRoute+0x40d>
    d8e9: 4c 89 ff                     	mov	rdi, r15
    d8ec: 48 ff c7                     	inc	rdi
    d8ef: 0f 88 a9 01 00 00            	js	0xda9e <Ep16StdDefaultFindRoute+0x3ce>
    d8f5: e8 86 8e ff ff               	call	0x6780 <memchr@plt+0xc>
    d8fa: 4c 89 7d b0                  	mov	qword ptr [rbp - 0x50], r15
    d8fe: 48 89 c7                     	mov	rdi, rax
    d901: 48 89 45 a0                  	mov	qword ptr [rbp - 0x60], rax
    d905: 4c 89 fa                     	mov	rdx, r15
    d908: 4c 89 f6                     	mov	rsi, r14
    d90b: e8 90 8d ff ff               	call	0x66a0 <__cxa_finalize@plt+0xc>
    d910: 48 8b 45 a0                  	mov	rax, qword ptr [rbp - 0x60]
    d914: e9 4e fe ff ff               	jmp	0xd767 <Ep16StdDefaultFindRoute+0x97>
    d919: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]
    d920: 48 8b 75 a8                  	mov	rsi, qword ptr [rbp - 0x58]
    d924: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
    d928: ba 07 69 0f c7               	mov	edx, 0xc70f6907
    d92d: e8 7e 8e ff ff               	call	0x67b0 <_ZSt17__throw_bad_allocv@plt+0xc>
    d932: 48 8b 4d 98                  	mov	rcx, qword ptr [rbp - 0x68]
    d936: 31 d2                        	xor	edx, edx
    d938: 49 89 c1                     	mov	r9, rax
    d93b: 4c 8b 41 08                  	mov	r8, qword ptr [rcx + 0x8]
    d93f: 49 f7 f0                     	div	r8
    d942: 48 8b 01                     	mov	rax, qword ptr [rcx]
    d945: 4c 8b 14 d0                  	mov	r10, qword ptr [rax + 8*rdx]
    d949: 49 89 d3                     	mov	r11, rdx
    d94c: 4d 85 d2                     	test	r10, r10
    d94f: 0f 84 5d 01 00 00            	je	0xdab2 <Ep16StdDefaultFindRoute+0x3e2>
    d955: 4d 8b 3a                     	mov	r15, qword ptr [r10]
    d958: 48 8b 45 a8                  	mov	rax, qword ptr [rbp - 0x58]
    d95c: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
    d960: 49 8b 4f 40                  	mov	rcx, qword ptr [r15 + 0x40]
    d964: 48 85 c0                     	test	rax, rax
    d967: 0f 84 1a 01 00 00            	je	0xda87 <Ep16StdDefaultFindRoute+0x3b7>
    d96d: 4c 89 ca                     	mov	rdx, r9
    d970: 4d 89 c6                     	mov	r14, r8
    d973: 4d 89 e8                     	mov	r8, r13
    d976: 49 89 c5                     	mov	r13, rax
    d979: 4c 89 e0                     	mov	rax, r12
    d97c: 49 89 d9                     	mov	r9, rbx
    d97f: 4d 89 dc                     	mov	r12, r11
    d982: 48 89 d3                     	mov	rbx, rdx
    d985: 49 89 c3                     	mov	r11, rax
    d988: eb 2d                        	jmp	0xd9b7 <Ep16StdDefaultFindRoute+0x2e7>
    d98a: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
    d990: 49 8b 37                     	mov	rsi, qword ptr [r15]
    d993: 48 85 f6                     	test	rsi, rsi
    d996: 0f 84 f4 00 00 00            	je	0xda90 <Ep16StdDefaultFindRoute+0x3c0>
    d99c: 48 8b 4e 40                  	mov	rcx, qword ptr [rsi + 0x40]
    d9a0: 31 d2                        	xor	edx, edx
    d9a2: 4d 89 fa                     	mov	r10, r15
    d9a5: 48 89 c8                     	mov	rax, rcx
    d9a8: 49 f7 f6                     	div	r14
    d9ab: 49 39 d4                     	cmp	r12, rdx
    d9ae: 0f 85 dc 00 00 00            	jne	0xda90 <Ep16StdDefaultFindRoute+0x3c0>
    d9b4: 49 89 f7                     	mov	r15, rsi
    d9b7: 48 39 cb                     	cmp	rbx, rcx
    d9ba: 75 d4                        	jne	0xd990 <Ep16StdDefaultFindRoute+0x2c0>
    d9bc: 4d 3b 6f 10                  	cmp	r13, qword ptr [r15 + 0x10]
    d9c0: 75 ce                        	jne	0xd990 <Ep16StdDefaultFindRoute+0x2c0>
    d9c2: 49 8b 77 08                  	mov	rsi, qword ptr [r15 + 0x8]
    d9c6: 4c 89 ea                     	mov	rdx, r13
    d9c9: 4c 89 9d 58 ff ff ff         	mov	qword ptr [rbp - 0xa8], r11
    d9d0: 4c 89 95 60 ff ff ff         	mov	qword ptr [rbp - 0xa0], r10
    d9d7: 4c 89 8d 68 ff ff ff         	mov	qword ptr [rbp - 0x98], r9
    d9de: 4c 89 85 70 ff ff ff         	mov	qword ptr [rbp - 0x90], r8
    d9e5: 48 89 7d 90                  	mov	qword ptr [rbp - 0x70], rdi
    d9e9: e8 12 8d ff ff               	call	0x6700 <_ZSt24__throw_invalid_argumentPKc@plt+0xc>
    d9ee: 48 8b 7d 90                  	mov	rdi, qword ptr [rbp - 0x70]
    d9f2: 4c 8b 85 70 ff ff ff         	mov	r8, qword ptr [rbp - 0x90]
    d9f9: 4c 8b 8d 68 ff ff ff         	mov	r9, qword ptr [rbp - 0x98]
    da00: 85 c0                        	test	eax, eax
    da02: 4c 8b 95 60 ff ff ff         	mov	r10, qword ptr [rbp - 0xa0]
    da09: 4c 8b 9d 58 ff ff ff         	mov	r11, qword ptr [rbp - 0xa8]
    da10: 0f 85 7a ff ff ff            	jne	0xd990 <Ep16StdDefaultFindRoute+0x2c0>
    da16: 4d 89 c5                     	mov	r13, r8
    da19: 4c 89 cb                     	mov	rbx, r9
    da1c: 4d 89 dc                     	mov	r12, r11
    da1f: 4d 8b 3a                     	mov	r15, qword ptr [r10]
    da22: 4c 39 e7                     	cmp	rdi, r12
    da25: 74 0d                        	je	0xda34 <Ep16StdDefaultFindRoute+0x364>
    da27: 48 8b 45 b0                  	mov	rax, qword ptr [rbp - 0x50]
    da2b: 48 8d 70 01                  	lea	rsi, [rax + 0x1]
    da2f: e8 dc 8c ff ff               	call	0x6710 <memcmp@plt+0xc>
    da34: 4d 85 ff                     	test	r15, r15
    da37: 0f 85 b4 fd ff ff            	jne	0xd7f1 <Ep16StdDefaultFindRoute+0x121>
    da3d: e9 dc fd ff ff               	jmp	0xd81e <Ep16StdDefaultFindRoute+0x14e>
    da42: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    da4d: 0f 1f 00                     	nop	dword ptr [rax]
    da50: 49 83 7f 10 00               	cmp	qword ptr [r15 + 0x10], 0x0
    da55: 74 c8                        	je	0xda1f <Ep16StdDefaultFindRoute+0x34f>
    da57: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
    da60: 49 8b 37                     	mov	rsi, qword ptr [r15]
    da63: 48 85 f6                     	test	rsi, rsi
    da66: 0f 84 3e fe ff ff            	je	0xd8aa <Ep16StdDefaultFindRoute+0x1da>
    da6c: 48 8b 4e 40                  	mov	rcx, qword ptr [rsi + 0x40]
    da70: 31 d2                        	xor	edx, edx
    da72: 4d 89 fa                     	mov	r10, r15
    da75: 48 89 c8                     	mov	rax, rcx
    da78: 49 f7 f0                     	div	r8
    da7b: 49 39 d3                     	cmp	r11, rdx
    da7e: 0f 85 26 fe ff ff            	jne	0xd8aa <Ep16StdDefaultFindRoute+0x1da>
    da84: 49 89 f7                     	mov	r15, rsi
    da87: 4c 39 c9                     	cmp	rcx, r9
    da8a: 75 d4                        	jne	0xda60 <Ep16StdDefaultFindRoute+0x390>
    da8c: eb c2                        	jmp	0xda50 <Ep16StdDefaultFindRoute+0x380>
    da8e: 66 90                        	nop
    da90: 4d 89 c5                     	mov	r13, r8
    da93: 4c 89 cb                     	mov	rbx, r9
    da96: 4d 89 dc                     	mov	r12, r11
    da99: e9 0c fe ff ff               	jmp	0xd8aa <Ep16StdDefaultFindRoute+0x1da>
    da9e: 48 8b 45 c8                  	mov	rax, qword ptr [rbp - 0x38]
    daa2: 64 48 2b 04 25 28 00 00 00   	sub	rax, qword ptr fs:[0x28]
    daab: 75 4b                        	jne	0xdaf8 <Ep16StdDefaultFindRoute+0x428>
    daad: e8 ee 8c ff ff               	call	0x67a0 <_ZSt28__throw_bad_array_new_lengthv@plt+0xc>
    dab2: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
    dab6: e9 ef fd ff ff               	jmp	0xd8aa <Ep16StdDefaultFindRoute+0x1da>
    dabb: 31 c0                        	xor	eax, eax
    dabd: e9 8c fd ff ff               	jmp	0xd84e <Ep16StdDefaultFindRoute+0x17e>
    dac2: 48 8b 45 c8                  	mov	rax, qword ptr [rbp - 0x38]
    dac6: 64 48 2b 04 25 28 00 00 00   	sub	rax, qword ptr fs:[0x28]
    dacf: 75 27                        	jne	0xdaf8 <Ep16StdDefaultFindRoute+0x428>
    dad1: 48 8d 3d b0 45 04 00         	lea	rdi, [rip + 0x445b0]    # 0x52088 <write+0x52088>
    dad8: e8 e3 8c ff ff               	call	0x67c0 <_ZSt11_Hash_bytesPKvmm@plt+0xc>
    dadd: 48 8b 45 c8                  	mov	rax, qword ptr [rbp - 0x38]
    dae1: 64 48 2b 04 25 28 00 00 00   	sub	rax, qword ptr fs:[0x28]
    daea: 75 0c                        	jne	0xdaf8 <Ep16StdDefaultFindRoute+0x428>
    daec: 48 8d 3d d4 37 04 00         	lea	rdi, [rip + 0x437d4]    # 0x512c7 <write+0x512c7>
    daf3: e8 18 8d ff ff               	call	0x6810 <__cxa_end_catch@plt+0xc>
    daf8: e8 23 8d ff ff               	call	0x6820 <_ZSt20__throw_length_errorPKc@plt+0xc>
    dafd: 0f 1f 00                     	nop	dword ptr [rax]

./bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000000c330 <Ep16StdTransparentFindRoute>:
    c330: f3 0f 1e fa                  	endbr64
    c334: 55                           	push	rbp
    c335: 48 89 e5                     	mov	rbp, rsp
    c338: 41 57                        	push	r15
    c33a: 41 56                        	push	r14
    c33c: 41 55                        	push	r13
    c33e: 41 54                        	push	r12
    c340: 53                           	push	rbx
    c341: 48 83 ec 38                  	sub	rsp, 0x38
    c345: 48 89 55 c8                  	mov	qword ptr [rbp - 0x38], rdx
    c349: 48 85 d2                     	test	rdx, rdx
    c34c: 0f 84 56 01 00 00            	je	0xc4a8 <Ep16StdTransparentFindRoute+0x178>
    c352: 49 89 fc                     	mov	r12, rdi
    c355: 49 89 f7                     	mov	r15, rsi
    c358: 31 db                        	xor	ebx, ebx
    c35a: 45 31 ed                     	xor	r13d, r13d
    c35d: 48 c7 45 a0 00 00 00 00      	mov	qword ptr [rbp - 0x60], 0x0
    c365: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
    c370: 49 8b 37                     	mov	rsi, qword ptr [r15]
    c373: 49 8b 7f 08                  	mov	rdi, qword ptr [r15 + 0x8]
    c377: ba 07 69 0f c7               	mov	edx, 0xc70f6907
    c37c: e8 2f a4 ff ff               	call	0x67b0 <_ZSt17__throw_bad_allocv@plt+0xc>
    c381: 4d 8b 4c 24 08               	mov	r9, qword ptr [r12 + 0x8]
    c386: 31 d2                        	xor	edx, edx
    c388: 49 89 c2                     	mov	r10, rax
    c38b: 49 f7 f1                     	div	r9
    c38e: 49 8b 04 24                  	mov	rax, qword ptr [r12]
    c392: 4c 8b 34 d0                  	mov	r14, qword ptr [rax + 8*rdx]
    c396: 49 89 d3                     	mov	r11, rdx
    c399: 4d 85 f6                     	test	r14, r14
    c39c: 0f 84 bf 00 00 00            	je	0xc461 <Ep16StdTransparentFindRoute+0x131>
    c3a2: 4d 8b 06                     	mov	r8, qword ptr [r14]
    c3a5: 4c 89 e0                     	mov	rax, r12
    c3a8: 49 89 dc                     	mov	r12, rbx
    c3ab: 4c 89 f3                     	mov	rbx, r14
    c3ae: 49 8b 48 40                  	mov	rcx, qword ptr [r8 + 0x40]
    c3b2: 4d 89 c6                     	mov	r14, r8
    c3b5: 49 89 c0                     	mov	r8, rax
    c3b8: eb 2d                        	jmp	0xc3e7 <Ep16StdTransparentFindRoute+0xb7>
    c3ba: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
    c3c0: 49 8b 36                     	mov	rsi, qword ptr [r14]
    c3c3: 48 85 f6                     	test	rsi, rsi
    c3c6: 0f 84 d4 00 00 00            	je	0xc4a0 <Ep16StdTransparentFindRoute+0x170>
    c3cc: 48 8b 4e 40                  	mov	rcx, qword ptr [rsi + 0x40]
    c3d0: 31 d2                        	xor	edx, edx
    c3d2: 4c 89 f3                     	mov	rbx, r14
    c3d5: 48 89 c8                     	mov	rax, rcx
    c3d8: 49 f7 f1                     	div	r9
    c3db: 49 39 d3                     	cmp	r11, rdx
    c3de: 0f 85 bc 00 00 00            	jne	0xc4a0 <Ep16StdTransparentFindRoute+0x170>
    c3e4: 49 89 f6                     	mov	r14, rsi
    c3e7: 49 39 ca                     	cmp	r10, rcx
    c3ea: 75 d4                        	jne	0xc3c0 <Ep16StdTransparentFindRoute+0x90>
    c3ec: 49 8b 17                     	mov	rdx, qword ptr [r15]
    c3ef: 49 3b 56 10                  	cmp	rdx, qword ptr [r14 + 0x10]
    c3f3: 75 cb                        	jne	0xc3c0 <Ep16StdTransparentFindRoute+0x90>
    c3f5: 4c 89 5d b0                  	mov	qword ptr [rbp - 0x50], r11
    c3f9: 4c 89 4d b8                  	mov	qword ptr [rbp - 0x48], r9
    c3fd: 4c 89 55 c0                  	mov	qword ptr [rbp - 0x40], r10
    c401: 48 85 d2                     	test	rdx, rdx
    c404: 74 25                        	je	0xc42b <Ep16StdTransparentFindRoute+0xfb>
    c406: 49 8b 76 08                  	mov	rsi, qword ptr [r14 + 0x8]
    c40a: 49 8b 7f 08                  	mov	rdi, qword ptr [r15 + 0x8]
    c40e: 4c 89 45 a8                  	mov	qword ptr [rbp - 0x58], r8
    c412: e8 e9 a2 ff ff               	call	0x6700 <_ZSt24__throw_invalid_argumentPKc@plt+0xc>
    c417: 4c 8b 45 a8                  	mov	r8, qword ptr [rbp - 0x58]
    c41b: 4c 8b 55 c0                  	mov	r10, qword ptr [rbp - 0x40]
    c41f: 4c 8b 4d b8                  	mov	r9, qword ptr [rbp - 0x48]
    c423: 85 c0                        	test	eax, eax
    c425: 4c 8b 5d b0                  	mov	r11, qword ptr [rbp - 0x50]
    c429: 75 95                        	jne	0xc3c0 <Ep16StdTransparentFindRoute+0x90>
    c42b: 49 89 de                     	mov	r14, rbx
    c42e: 4c 89 e3                     	mov	rbx, r12
    c431: 4d 89 c4                     	mov	r12, r8
    c434: 49 8b 16                     	mov	rdx, qword ptr [r14]
    c437: 48 85 d2                     	test	rdx, rdx
    c43a: 74 25                        	je	0xc461 <Ep16StdTransparentFindRoute+0x131>
    c43c: 8b 42 28                     	mov	eax, dword ptr [rdx + 0x28]
    c43f: 8b 4a 2c                     	mov	ecx, dword ptr [rdx + 0x2c]
    c442: 49 ff c5                     	inc	r13
    c445: 48 c1 e1 10                  	shl	rcx, 0x10
    c449: 48 c1 e0 20                  	shl	rax, 0x20
    c44d: 48 31 c8                     	xor	rax, rcx
    c450: 8b 4a 34                     	mov	ecx, dword ptr [rdx + 0x34]
    c453: 03 4a 30                     	add	ecx, dword ptr [rdx + 0x30]
    c456: 48 33 4a 38                  	xor	rcx, qword ptr [rdx + 0x38]
    c45a: 48 31 c8                     	xor	rax, rcx
    c45d: 48 01 45 a0                  	add	qword ptr [rbp - 0x60], rax
    c461: 48 ff c3                     	inc	rbx
    c464: 49 83 c7 10                  	add	r15, 0x10
    c468: 48 39 5d c8                  	cmp	qword ptr [rbp - 0x38], rbx
    c46c: 0f 85 fe fe ff ff            	jne	0xc370 <Ep16StdTransparentFindRoute+0x40>
    c472: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
    c476: 48 b8 15 7c 4a 7f b9 79 37 9e	movabs	rax, -0x61c8864680b583eb
    c480: 48 83 c4 38                  	add	rsp, 0x38
    c484: 49 0f af c5                  	imul	rax, r13
    c488: 5b                           	pop	rbx
    c489: 41 5c                        	pop	r12
    c48b: 41 5d                        	pop	r13
    c48d: 41 5e                        	pop	r14
    c48f: 41 5f                        	pop	r15
    c491: 5d                           	pop	rbp
    c492: 48 31 f8                     	xor	rax, rdi
    c495: c3                           	ret
    c496: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
    c4a0: 4c 89 e3                     	mov	rbx, r12
    c4a3: 4d 89 c4                     	mov	r12, r8
    c4a6: eb b9                        	jmp	0xc461 <Ep16StdTransparentFindRoute+0x131>
    c4a8: 48 83 c4 38                  	add	rsp, 0x38
    c4ac: 31 c0                        	xor	eax, eax
    c4ae: 5b                           	pop	rbx
    c4af: 41 5c                        	pop	r12
    c4b1: 41 5d                        	pop	r13
    c4b3: 41 5e                        	pop	r14
    c4b5: 41 5f                        	pop	r15
    c4b7: 5d                           	pop	rbp
    c4b8: c3                           	ret
    c4b9: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]

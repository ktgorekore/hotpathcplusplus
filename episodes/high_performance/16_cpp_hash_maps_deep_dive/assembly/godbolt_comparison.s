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
	.globl	_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEEPKc # -- Begin function _ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEEPKc
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEEPKc,@function
_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEEPKc: # @_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEEPKc
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
	mov	qword ptr [rsp], r13
	test	rsi, rsi
	je	.LBB0_19
# %bb.1:
	mov	rbx, rdi
	mov	rdi, rsi
	mov	r14, rsi
	call	strlen@PLT
	mov	r15, rax
	mov	r12, r13
	cmp	rax, 16
	jb	.LBB0_5
# %bb.2:
	test	r15, r15
	js	.LBB0_20
# %bb.3:
	mov	rdi, r15
	inc	rdi
	js	.LBB0_21
# %bb.4:
	call	_Znwm@PLT
	mov	r12, rax
	mov	qword ptr [rsp], rax
	mov	qword ptr [rsp + 16], r15
.LBB0_5:
	test	r15, r15
	je	.LBB0_9
# %bb.6:
	cmp	r15, 1
	jne	.LBB0_8
# %bb.7:
	movzx	eax, byte ptr [r14]
	mov	byte ptr [r12], al
	jmp	.LBB0_9
.LBB0_8:
	mov	rdi, r12
	mov	rsi, r14
	mov	rdx, r15
	call	memcpy@PLT
.LBB0_9:
	mov	qword ptr [rsp + 8], r15
	mov	byte ptr [r12 + r15], 0
.Ltmp0:
	mov	rsi, rsp
	mov	rdi, rbx
	call	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_
.Ltmp1:
# %bb.10:
	mov	rdi, qword ptr [rsp]
	mov	rbx, rax
	cmp	rdi, r13
	je	.LBB0_12
# %bb.11:
	call	_ZdlPv@PLT
.LBB0_12:
	test	rbx, rbx
	je	.LBB0_13
# %bb.14:
	mov	rax, qword ptr [rbx + 40]
	jmp	.LBB0_15
.LBB0_13:
	xor	eax, eax
.LBB0_15:
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
.LBB0_21:
	.cfi_def_cfa_offset 80
	call	_ZSt17__throw_bad_allocv@PLT
.LBB0_19:
	lea	rdi, [rip + .L.str]
	call	_ZSt19__throw_logic_errorPKc@PLT
.LBB0_20:
	lea	rdi, [rip + .L.str.1]
	call	_ZSt20__throw_length_errorPKc@PLT
.LBB0_16:
.Ltmp2:
	mov	rdi, qword ptr [rsp]
	mov	rbx, rax
	cmp	rdi, r13
	je	.LBB0_18
# %bb.17:
	call	_ZdlPv@PLT
.LBB0_18:
	mov	rdi, rbx
	call	_Unwind_Resume@PLT
.Lfunc_end0:
	.size	_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEEPKc, .Lfunc_end0-_ZN7hotpath12godbolt_ep1624LookupRoute_DefaultAllocERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmSt4hashIS7_ESt8equal_toIS7_ESaISt4pairIKS7_mEEEPKc
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
	.globl	_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_24TransparentStdStringHashESt8equal_toIvESaISt4pairIKS7_mEEEPKc # -- Begin function _ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_24TransparentStdStringHashESt8equal_toIvESaISt4pairIKS7_mEEEPKc
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_24TransparentStdStringHashESt8equal_toIvESaISt4pairIKS7_mEEEPKc,@function
_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_24TransparentStdStringHashESt8equal_toIvESaISt4pairIKS7_mEEEPKc: # @_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_24TransparentStdStringHashESt8equal_toIvESaISt4pairIKS7_mEEEPKc
	.cfi_startproc
# %bb.0:
	push	rax
	.cfi_def_cfa_offset 16
	mov	qword ptr [rsp], rsi
	mov	rsi, rsp
	call	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_
	test	rax, rax
	je	.LBB1_1
# %bb.2:
	mov	rax, qword ptr [rax + 40]
	pop	rcx
	.cfi_def_cfa_offset 8
	ret
.LBB1_1:
	.cfi_def_cfa_offset 16
	xor	eax, eax
	pop	rcx
	.cfi_def_cfa_offset 8
	ret
.Lfunc_end1:
	.size	_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_24TransparentStdStringHashESt8equal_toIvESaISt4pairIKS7_mEEEPKc, .Lfunc_end1-_ZN7hotpath12godbolt_ep1628LookupRoute_Cpp20TransparentERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_24TransparentStdStringHashESt8equal_toIvESaISt4pairIKS7_mEEEPKc
	.cfi_endproc
                                        # -- End function
	.globl	_ZN7hotpath12godbolt_ep1627LookupRoute_GccNoexceptTrapERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_32TransparentStdStringHashNoexceptESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E # -- Begin function _ZN7hotpath12godbolt_ep1627LookupRoute_GccNoexceptTrapERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_32TransparentStdStringHashNoexceptESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E
	.p2align	4, 0x90
	.type	_ZN7hotpath12godbolt_ep1627LookupRoute_GccNoexceptTrapERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_32TransparentStdStringHashNoexceptESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E,@function
_ZN7hotpath12godbolt_ep1627LookupRoute_GccNoexceptTrapERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_32TransparentStdStringHashNoexceptESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E: # @_ZN7hotpath12godbolt_ep1627LookupRoute_GccNoexceptTrapERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_32TransparentStdStringHashNoexceptESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E
.Lfunc_begin1:
	.cfi_startproc
	.cfi_personality 155, DW.ref.__gxx_personality_v0
	.cfi_lsda 27, .Lexception1
# %bb.0:
	sub	rsp, 24
	.cfi_def_cfa_offset 32
	mov	qword ptr [rsp + 8], rsi
	mov	qword ptr [rsp + 16], rdx
.Ltmp3:
	lea	rsi, [rsp + 8]
	call	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_
.Ltmp4:
# %bb.1:
	test	rax, rax
	je	.LBB2_2
# %bb.3:
	mov	rax, qword ptr [rax + 40]
	add	rsp, 24
	.cfi_def_cfa_offset 8
	ret
.LBB2_2:
	.cfi_def_cfa_offset 32
	xor	eax, eax
	add	rsp, 24
	.cfi_def_cfa_offset 8
	ret
.LBB2_5:
	.cfi_def_cfa_offset 32
.Ltmp5:
	mov	rdi, rax
	call	__clang_call_terminate
.Lfunc_end2:
	.size	_ZN7hotpath12godbolt_ep1627LookupRoute_GccNoexceptTrapERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_32TransparentStdStringHashNoexceptESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E, .Lfunc_end2-_ZN7hotpath12godbolt_ep1627LookupRoute_GccNoexceptTrapERKSt13unordered_mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmNS0_32TransparentStdStringHashNoexceptESt8equal_toIvESaISt4pairIKS7_mEEESt17basic_string_viewIcS5_E
	.cfi_endproc
	.section	.gcc_except_table,"a",@progbits
	.p2align	2, 0x0
GCC_except_table2:
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
	.section	.rodata.cst16,"aM",@progbits,16
	.p2align	4, 0x0                          # -- Begin function _ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa
.LCPI4_0:
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
	vpcmpeqb	k1, xmm0, xmmword ptr [rip + .LCPI4_0]
	vpcmpeqb	k0, xmm0, xmm1
	kortestw	k1, k1
	kmovw	eax, k0
	setne	cl
	shl	rcx, 32
	or	rax, rcx
	ret
.Lfunc_end4:
	.size	_ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa, .Lfunc_end4-_ZN7hotpath12godbolt_ep1613ProbeAbseil16EPKaa
	.cfi_endproc
                                        # -- End function
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
.Lfunc_end5:
	.size	_ZN7hotpath12godbolt_ep1613ProbeFollyF14EPKNS0_14F14ChunkHeaderEh, .Lfunc_end5-_ZN7hotpath12godbolt_ep1613ProbeFollyF14EPKNS0_14F14ChunkHeaderEh
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
.Lfunc_end6:
	.size	_ZN7hotpath12godbolt_ep1617ProbeBoostGroup15EPKNS0_12BoostGroup15Ehm, .Lfunc_end6-_ZN7hotpath12godbolt_ep1617ProbeBoostGroup15EPKNS0_12BoostGroup15Ehm
	.cfi_endproc
                                        # -- End function
	.section	.text._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,"axG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,comdat
	.weak	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_ # -- Begin function _ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_
	.p2align	4, 0x90
	.type	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,@function
_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_: # @_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_
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
	cmp	qword ptr [rdi + 24], 20
	mov	r12, rsi
	mov	rbx, rdi
	jbe	.LBB7_10
# %bb.1:
	mov	rdi, qword ptr [r12]
	mov	rsi, qword ptr [r12 + 8]
.Ltmp6:
	mov	edx, 3339675911
	call	_ZSt11_Hash_bytesPKvmm@PLT
.Ltmp7:
# %bb.2:
	mov	rbp, qword ptr [rbx + 8]
	mov	rcx, qword ptr [rbx]
	mov	r14, rax
	xor	edx, edx
	xor	r13d, r13d
	div	rbp
	mov	rax, qword ptr [rcx + 8*rdx]
	test	rax, rax
	je	.LBB7_28
# %bb.3:
	mov	rbx, qword ptr [rax]
	mov	r8, qword ptr [r12 + 8]
	mov	r15, rdx
	mov	rcx, qword ptr [rbx + 48]
	test	r8, r8
	je	.LBB7_20
# %bb.4:
	mov	rax, qword ptr [r12]
	xor	r13d, r13d
	mov	qword ptr [rsp], rax            # 8-byte Spill
	.p2align	4, 0x90
.LBB7_5:                                # =>This Inner Loop Header: Depth=1
	cmp	rcx, r14
	jne	.LBB7_8
# %bb.6:                                #   in Loop: Header=BB7_5 Depth=1
	cmp	r8, qword ptr [rbx + 16]
	jne	.LBB7_8
# %bb.7:                                #   in Loop: Header=BB7_5 Depth=1
	mov	rsi, qword ptr [rbx + 8]
	mov	rdi, qword ptr [rsp]            # 8-byte Reload
	mov	rdx, r8
	mov	r12, r8
	call	bcmp@PLT
	mov	r8, r12
	test	eax, eax
	je	.LBB7_27
.LBB7_8:                                #   in Loop: Header=BB7_5 Depth=1
	mov	rbx, qword ptr [rbx]
	test	rbx, rbx
	je	.LBB7_28
# %bb.9:                                #   in Loop: Header=BB7_5 Depth=1
	mov	rcx, qword ptr [rbx + 48]
	xor	edx, edx
	mov	rax, rcx
	div	rbp
	cmp	rdx, r15
	je	.LBB7_5
	jmp	.LBB7_28
.LBB7_10:
	mov	r13, qword ptr [rbx + 16]
	test	r13, r13
	je	.LBB7_18
# %bb.11:
	mov	r14, qword ptr [r12 + 8]
	test	r14, r14
	je	.LBB7_17
# %bb.12:
	mov	r15, qword ptr [r12]
	jmp	.LBB7_14
	.p2align	4, 0x90
.LBB7_13:                               #   in Loop: Header=BB7_14 Depth=1
	mov	r13, qword ptr [r13]
	test	r13, r13
	je	.LBB7_18
.LBB7_14:                               # =>This Inner Loop Header: Depth=1
	cmp	r14, qword ptr [r13 + 16]
	jne	.LBB7_13
# %bb.15:                               #   in Loop: Header=BB7_14 Depth=1
	mov	rsi, qword ptr [r13 + 8]
	mov	rdi, r15
	mov	rdx, r14
	call	bcmp@PLT
	test	eax, eax
	jne	.LBB7_13
	jmp	.LBB7_28
	.p2align	4, 0x90
.LBB7_16:                               #   in Loop: Header=BB7_17 Depth=1
	mov	r13, qword ptr [r13]
	test	r13, r13
	je	.LBB7_18
.LBB7_17:                               # =>This Inner Loop Header: Depth=1
	cmp	qword ptr [r13 + 16], 0
	jne	.LBB7_16
	jmp	.LBB7_28
.LBB7_18:
	xor	r13d, r13d
.LBB7_28:
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
.LBB7_20:
	.cfi_def_cfa_offset 64
	xor	r13d, r13d
	.p2align	4, 0x90
.LBB7_21:                               # =>This Inner Loop Header: Depth=1
	cmp	rcx, r14
	jne	.LBB7_23
# %bb.22:                               #   in Loop: Header=BB7_21 Depth=1
	cmp	qword ptr [rbx + 16], 0
	je	.LBB7_27
.LBB7_23:                               #   in Loop: Header=BB7_21 Depth=1
	mov	rbx, qword ptr [rbx]
	test	rbx, rbx
	je	.LBB7_28
# %bb.24:                               #   in Loop: Header=BB7_21 Depth=1
	mov	rcx, qword ptr [rbx + 48]
	xor	edx, edx
	mov	rax, rcx
	div	rbp
	cmp	rdx, r15
	je	.LBB7_21
	jmp	.LBB7_28
.LBB7_27:
	mov	r13, rbx
	jmp	.LBB7_28
.LBB7_29:
.Ltmp8:
	mov	rdi, rax
	call	__clang_call_terminate
.Lfunc_end7:
	.size	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_, .Lfunc_end7-_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_
	.cfi_endproc
	.section	.gcc_except_table._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,"aG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIS5_ESt4hashIS5_ENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE4findERS7_,comdat
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
	.section	.text._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,"axG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,comdat
	.weak	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_ # -- Begin function _ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_
	.p2align	4, 0x90
	.type	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,@function
_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_: # @_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_
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
	mov	rbx, qword ptr [rsi]
	mov	r14, rdi
	mov	r12, rsi
	mov	rdi, rbx
	call	strlen@PLT
.Ltmp9:
	mov	edx, 3339675911
	mov	rdi, rbx
	mov	rsi, rax
	call	_ZSt11_Hash_bytesPKvmm@PLT
.Ltmp10:
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
	je	.LBB8_11
# %bb.2:
	mov	r14, qword ptr [rax]
	mov	r12, qword ptr [r12]
	xor	eax, eax
	mov	r15, rdx
	mov	qword ptr [rsp], rax            # 8-byte Spill
	mov	rcx, qword ptr [r14 + 48]
	.p2align	4, 0x90
.LBB8_3:                                # =>This Inner Loop Header: Depth=1
	cmp	rcx, rbx
	jne	.LBB8_7
# %bb.4:                                #   in Loop: Header=BB8_3 Depth=1
	mov	r13, qword ptr [r14 + 16]
	mov	rdi, r12
	call	strlen@PLT
	cmp	r13, rax
	jne	.LBB8_7
# %bb.5:                                #   in Loop: Header=BB8_3 Depth=1
	test	r13, r13
	je	.LBB8_10
# %bb.6:                                #   in Loop: Header=BB8_3 Depth=1
	mov	rdi, qword ptr [r14 + 8]
	mov	rsi, r12
	mov	rdx, r13
	call	bcmp@PLT
	test	eax, eax
	je	.LBB8_10
.LBB8_7:                                #   in Loop: Header=BB8_3 Depth=1
	mov	r14, qword ptr [r14]
	test	r14, r14
	je	.LBB8_11
# %bb.8:                                #   in Loop: Header=BB8_3 Depth=1
	mov	rcx, qword ptr [r14 + 48]
	xor	edx, edx
	mov	rax, rcx
	div	rbp
	cmp	rdx, r15
	je	.LBB8_3
	jmp	.LBB8_11
.LBB8_10:
	mov	qword ptr [rsp], r14            # 8-byte Spill
.LBB8_11:
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
.LBB8_12:
	.cfi_def_cfa_offset 64
.Ltmp11:
	mov	rdi, rax
	call	__clang_call_terminate
.Lfunc_end8:
	.size	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_, .Lfunc_end8-_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_
	.cfi_endproc
	.section	.gcc_except_table._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,"aG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1624TransparentStdStringHashENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb1ELb0ELb1EEEE10_M_find_trIPKcvvEENSA_20_Node_const_iteratorIS8_Lb0ELb1EEERKT_,comdat
	.p2align	2, 0x0
GCC_except_table8:
.Lexception3:
	.byte	255                             # @LPStart Encoding = omit
	.byte	155                             # @TType Encoding = indirect pcrel sdata4
	.uleb128 .Lttbase2-.Lttbaseref2
.Lttbaseref2:
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end3-.Lcst_begin3
.Lcst_begin3:
	.uleb128 .Ltmp9-.Lfunc_begin3           # >> Call Site 1 <<
	.uleb128 .Ltmp10-.Ltmp9                 #   Call between .Ltmp9 and .Ltmp10
	.uleb128 .Ltmp11-.Lfunc_begin3          #     jumps to .Ltmp11
	.byte	1                               #   On action: 1
.Lcst_end3:
	.byte	1                               # >> Action Record 1 <<
                                        #   Catch TypeInfo 1
	.byte	0                               #   No further actions
	.p2align	2, 0x0
                                        # >> Catch TypeInfos <<
	.long	0                               # TypeInfo 1
.Lttbase2:
	.p2align	2, 0x0
                                        # -- End function
	.section	.text._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_,"axG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_,comdat
	.weak	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_ # -- Begin function _ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_
	.p2align	4, 0x90
	.type	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_,@function
_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_: # @_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_
.Lfunc_begin4:
	.cfi_startproc
	.cfi_personality 155, DW.ref.__gxx_personality_v0
	.cfi_lsda 27, .Lexception4
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
	mov	rbx, rsi
	mov	r14, rdi
	mov	rsi, qword ptr [rsi]
	mov	rdi, qword ptr [rbx + 8]
.Ltmp12:
	mov	edx, 3339675911
	call	_ZSt11_Hash_bytesPKvmm@PLT
.Ltmp13:
# %bb.1:
	xor	edx, edx
	div	qword ptr [r14 + 8]
	mov	rax, qword ptr [r14]
	xor	r15d, r15d
	mov	r13, qword ptr [rax + 8*rdx]
	test	r13, r13
	je	.LBB9_11
# %bb.2:
	mov	r12, rdx
	xor	r15d, r15d
	.p2align	4, 0x90
.LBB9_3:                                # =>This Inner Loop Header: Depth=1
	mov	r13, qword ptr [r13]
	mov	rdx, qword ptr [rbx]
	cmp	rdx, qword ptr [r13 + 16]
	jne	.LBB9_6
# %bb.4:                                #   in Loop: Header=BB9_3 Depth=1
	test	rdx, rdx
	je	.LBB9_10
# %bb.5:                                #   in Loop: Header=BB9_3 Depth=1
	mov	rdi, qword ptr [rbx + 8]
	mov	rsi, qword ptr [r13 + 8]
	call	bcmp@PLT
	test	eax, eax
	je	.LBB9_10
.LBB9_6:                                #   in Loop: Header=BB9_3 Depth=1
	mov	rax, qword ptr [r13]
	test	rax, rax
	je	.LBB9_11
# %bb.7:                                #   in Loop: Header=BB9_3 Depth=1
	mov	rbp, qword ptr [r14 + 8]
	mov	rdi, qword ptr [rax + 8]
	mov	rsi, qword ptr [rax + 16]
.Ltmp15:
	mov	edx, 3339675911
	call	_ZSt11_Hash_bytesPKvmm@PLT
.Ltmp16:
# %bb.8:                                #   in Loop: Header=BB9_3 Depth=1
	xor	edx, edx
	div	rbp
	cmp	rdx, r12
	je	.LBB9_3
	jmp	.LBB9_11
.LBB9_10:
	mov	r15, r13
.LBB9_11:
	mov	rax, r15
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
.LBB9_12:
	.cfi_def_cfa_offset 64
.Ltmp14:
	mov	rdi, rax
	call	__clang_call_terminate
.LBB9_13:
.Ltmp17:
	mov	rdi, rax
	call	__clang_call_terminate
.Lfunc_end9:
	.size	_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_, .Lfunc_end9-_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_
	.cfi_endproc
	.section	.gcc_except_table._ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_,"aG",@progbits,_ZNKSt10_HashtableINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4pairIKS5_mESaIS8_ENSt8__detail10_Select1stESt8equal_toIvEN7hotpath12godbolt_ep1632TransparentStdStringHashNoexceptENSA_18_Mod_range_hashingENSA_20_Default_ranged_hashENSA_20_Prime_rehash_policyENSA_17_Hashtable_traitsILb0ELb0ELb1EEEE10_M_find_trISt17basic_string_viewIcS3_EvvEENSA_20_Node_const_iteratorIS8_Lb0ELb0EEERKT_,comdat
	.p2align	2, 0x0
GCC_except_table9:
.Lexception4:
	.byte	255                             # @LPStart Encoding = omit
	.byte	155                             # @TType Encoding = indirect pcrel sdata4
	.uleb128 .Lttbase3-.Lttbaseref3
.Lttbaseref3:
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end4-.Lcst_begin4
.Lcst_begin4:
	.uleb128 .Ltmp12-.Lfunc_begin4          # >> Call Site 1 <<
	.uleb128 .Ltmp13-.Ltmp12                #   Call between .Ltmp12 and .Ltmp13
	.uleb128 .Ltmp14-.Lfunc_begin4          #     jumps to .Ltmp14
	.byte	1                               #   On action: 1
	.uleb128 .Ltmp15-.Lfunc_begin4          # >> Call Site 2 <<
	.uleb128 .Ltmp16-.Ltmp15                #   Call between .Ltmp15 and .Ltmp16
	.uleb128 .Ltmp17-.Lfunc_begin4          #     jumps to .Ltmp17
	.byte	1                               #   On action: 1
.Lcst_end4:
	.byte	1                               # >> Action Record 1 <<
                                        #   Catch TypeInfo 1
	.byte	0                               #   No further actions
	.p2align	2, 0x0
                                        # >> Catch TypeInfos <<
	.long	0                               # TypeInfo 1
.Lttbase3:
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

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000001f880 <Ep16DisasmStdFindU64>:
   1f880: f3 0f 1e fa                  	endbr64
   1f884: 48 8b 47 18                  	mov	rax, qword ptr [rdi + 0x18]
   1f888: 48 89 f1                     	mov	rcx, rsi
   1f88b: 48 85 c0                     	test	rax, rax
   1f88e: 75 30                        	jne	0x1f8c0 <Ep16DisasmStdFindU64+0x40>
   1f890: 48 8b 57 10                  	mov	rdx, qword ptr [rdi + 0x10]
   1f894: 48 85 d2                     	test	rdx, rdx
   1f897: 75 0f                        	jne	0x1f8a8 <Ep16DisasmStdFindU64+0x28>
   1f899: c3                           	ret
   1f89a: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
   1f8a0: 48 8b 12                     	mov	rdx, qword ptr [rdx]
   1f8a3: 48 85 d2                     	test	rdx, rdx
   1f8a6: 74 78                        	je	0x1f920 <Ep16DisasmStdFindU64+0xa0>
   1f8a8: 48 39 4a 08                  	cmp	qword ptr [rdx + 0x8], rcx
   1f8ac: 75 f2                        	jne	0x1f8a0 <Ep16DisasmStdFindU64+0x20>
   1f8ae: 48 8b 42 10                  	mov	rax, qword ptr [rdx + 0x10]
   1f8b2: c3                           	ret
   1f8b3: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1f8be: 66 90                        	nop
   1f8c0: 4c 8b 47 08                  	mov	r8, qword ptr [rdi + 0x8]
   1f8c4: 48 89 f0                     	mov	rax, rsi
   1f8c7: 31 d2                        	xor	edx, edx
   1f8c9: 49 f7 f0                     	div	r8
   1f8cc: 48 8b 07                     	mov	rax, qword ptr [rdi]
   1f8cf: 4c 8b 0c d0                  	mov	r9, qword ptr [rax + 8*rdx]
   1f8d3: 49 89 d2                     	mov	r10, rdx
   1f8d6: 4d 85 c9                     	test	r9, r9
   1f8d9: 74 38                        	je	0x1f913 <Ep16DisasmStdFindU64+0x93>
   1f8db: 49 8b 01                     	mov	rax, qword ptr [r9]
   1f8de: 48 8b 70 08                  	mov	rsi, qword ptr [rax + 0x8]
   1f8e2: 48 39 ce                     	cmp	rsi, rcx
   1f8e5: 74 24                        	je	0x1f90b <Ep16DisasmStdFindU64+0x8b>
   1f8e7: 48 8b 38                     	mov	rdi, qword ptr [rax]
   1f8ea: 48 85 ff                     	test	rdi, rdi
   1f8ed: 74 24                        	je	0x1f913 <Ep16DisasmStdFindU64+0x93>
   1f8ef: 48 8b 77 08                  	mov	rsi, qword ptr [rdi + 0x8]
   1f8f3: 49 89 c1                     	mov	r9, rax
   1f8f6: 31 d2                        	xor	edx, edx
   1f8f8: 48 89 f0                     	mov	rax, rsi
   1f8fb: 49 f7 f0                     	div	r8
   1f8fe: 49 39 d2                     	cmp	r10, rdx
   1f901: 75 10                        	jne	0x1f913 <Ep16DisasmStdFindU64+0x93>
   1f903: 48 89 f8                     	mov	rax, rdi
   1f906: 48 39 ce                     	cmp	rsi, rcx
   1f909: 75 dc                        	jne	0x1f8e7 <Ep16DisasmStdFindU64+0x67>
   1f90b: 49 8b 11                     	mov	rdx, qword ptr [r9]
   1f90e: 48 85 d2                     	test	rdx, rdx
   1f911: 75 9b                        	jne	0x1f8ae <Ep16DisasmStdFindU64+0x2e>
   1f913: 31 c0                        	xor	eax, eax
   1f915: c3                           	ret
   1f916: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
   1f920: c3                           	ret
   1f921: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nop	word ptr cs:[rax + rax]

# --- 3. Live Compiled absl::flat_hash_map U64 Lookup (16-way SwissTable) ---

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000001f4c0 <Ep16DisasmAbslFlatFindU64>:
   1f4c0: f3 0f 1e fa                  	endbr64
   1f4c4: 0f b6 07                     	movzx	eax, byte ptr [rdi]
   1f4c7: 49 89 fb                     	mov	r11, rdi
   1f4ca: 3c 01                        	cmp	al, 0x1
   1f4cc: 0f 86 8e 00 00 00            	jbe	0x1f560 <Ep16DisasmAbslFlatFindU64+0xa0>
   1f4d2: 48 8b 0f                     	mov	rcx, qword ptr [rdi]
   1f4d5: 4c 8b 57 08                  	mov	r10, qword ptr [rdi + 0x8]
   1f4d9: 0f b6 cd                     	movzx	ecx, ch
   1f4dc: 41 0f 18 1a                  	prefetcht2	byte ptr [r10]
   1f4e0: 48 31 f1                     	xor	rcx, rsi
   1f4e3: 49 89 c8                     	mov	r8, rcx
   1f4e6: 48 b9 f5 8c 1e de e0 f9 d5 79	movabs	rcx, 0x79d5f9e0de1e8cf5
   1f4f0: 4c 89 c2                     	mov	rdx, r8
   1f4f3: c4 62 bb f6 c9               	mulx	r9, r8, rcx
   1f4f8: 4c 89 c9                     	mov	rcx, r9
   1f4fb: 4d 8b 4b 10                  	mov	r9, qword ptr [r11 + 0x10]
   1f4ff: 4c 31 c1                     	xor	rcx, r8
   1f502: 44 0f b6 c0                  	movzx	r8d, al
   1f506: c4 e2 f8 f5 f9               	bzhi	rdi, rcx, rax
   1f50b: 48 c1 e9 39                  	shr	rcx, 0x39
   1f50f: 62 f2 7d 08 7a c1            	vpbroadcastb	xmm0, ecx
   1f515: 31 c9                        	xor	ecx, ecx
   1f517: c4 c1 7a 6f 0c 3a            	vmovdqu	xmm1, xmmword ptr [r10 + rdi]
   1f51d: 48 89 f8                     	mov	rax, rdi
   1f520: 48 c1 e0 05                  	shl	rax, 0x5
   1f524: 41 0f 18 0c 01               	prefetcht0	byte ptr [r9 + rax]
   1f529: c5 f9 74 d1                  	vpcmpeqb	xmm2, xmm0, xmm1
   1f52d: c5 f9 d7 d2                  	vpmovmskb	edx, xmm2
   1f531: 85 d2                        	test	edx, edx
   1f533: 74 5b                        	je	0x1f590 <Ep16DisasmAbslFlatFindU64+0xd0>
   1f535: f3 0f bc c2                  	tzcnt	eax, edx
   1f539: 48 01 f8                     	add	rax, rdi
   1f53c: c4 e2 b8 f5 c0               	bzhi	rax, rax, r8
   1f541: 48 c1 e0 05                  	shl	rax, 0x5
   1f545: 4c 01 c8                     	add	rax, r9
   1f548: 48 39 30                     	cmp	qword ptr [rax], rsi
   1f54b: 75 33                        	jne	0x1f580 <Ep16DisasmAbslFlatFindU64+0xc0>
   1f54d: 48 8b 48 08                  	mov	rcx, qword ptr [rax + 0x8]
   1f551: 48 89 c8                     	mov	rax, rcx
   1f554: c3                           	ret
   1f555: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1f560: 48 8b 07                     	mov	rax, qword ptr [rdi]
   1f563: 31 c9                        	xor	ecx, ecx
   1f565: 48 c1 e8 11                  	shr	rax, 0x11
   1f569: 74 e6                        	je	0x1f551 <Ep16DisasmAbslFlatFindU64+0x91>
   1f56b: 48 8b 47 10                  	mov	rax, qword ptr [rdi + 0x10]
   1f56f: 48 39 30                     	cmp	qword ptr [rax], rsi
   1f572: 74 d9                        	je	0x1f54d <Ep16DisasmAbslFlatFindU64+0x8d>
   1f574: eb db                        	jmp	0x1f551 <Ep16DisasmAbslFlatFindU64+0x91>
   1f576: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
   1f580: c4 e2 68 f3 ca               	blsr	edx, edx
   1f585: 75 ae                        	jne	0x1f535 <Ep16DisasmAbslFlatFindU64+0x75>
   1f587: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   1f590: c4 e2 71 08 c9               	vpsignb	xmm1, xmm1, xmm1
   1f595: c5 f9 d7 c1                  	vpmovmskb	eax, xmm1
   1f599: 85 c0                        	test	eax, eax
   1f59b: 74 04                        	je	0x1f5a1 <Ep16DisasmAbslFlatFindU64+0xe1>
   1f59d: 31 c9                        	xor	ecx, ecx
   1f59f: eb b0                        	jmp	0x1f551 <Ep16DisasmAbslFlatFindU64+0x91>
   1f5a1: 48 83 c1 10                  	add	rcx, 0x10
   1f5a5: 48 8d 04 39                  	lea	rax, [rcx + rdi]
   1f5a9: c4 e2 b8 f5 f8               	bzhi	rdi, rax, r8
   1f5ae: e9 64 ff ff ff               	jmp	0x1f517 <Ep16DisasmAbslFlatFindU64+0x57>
   1f5b3: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00       	nop	word ptr cs:[rax + rax]

# --- 4. Live Compiled folly::F14ValueMap U64 Lookup (14-way F14 Chunk) ---

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000001f5c0 <Ep16DisasmFollyF14ValueFindU64>:
   1f5c0: f3 0f 1e fa                  	endbr64
   1f5c4: 48 8b 57 08                  	mov	rdx, qword ptr [rdi + 0x8]
   1f5c8: 48 89 f1                     	mov	rcx, rsi
   1f5cb: 48 89 d0                     	mov	rax, rdx
   1f5ce: 48 c1 e8 08                  	shr	rax, 0x8
   1f5d2: 48 81 fa ff 01 00 00         	cmp	rdx, 0x1ff
   1f5d9: 77 35                        	ja	0x1f610 <Ep16DisasmFollyF14ValueFindU64+0x50>
   1f5db: 48 85 c0                     	test	rax, rax
   1f5de: 75 10                        	jne	0x1f5f0 <Ep16DisasmFollyF14ValueFindU64+0x30>
   1f5e0: 31 c0                        	xor	eax, eax
   1f5e2: c3                           	ret
   1f5e3: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1f5ee: 66 90                        	nop
   1f5f0: 48 8b 47 10                  	mov	rax, qword ptr [rdi + 0x10]
   1f5f4: 48 83 e0 f0                  	and	rax, -0x10
   1f5f8: 48 3b 30                     	cmp	rsi, qword ptr [rax]
   1f5fb: 75 e3                        	jne	0x1f5e0 <Ep16DisasmFollyF14ValueFindU64+0x20>
   1f5fd: 48 8b 40 08                  	mov	rax, qword ptr [rax + 0x8]
   1f601: c3                           	ret
   1f602: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1f60d: 0f 1f 00                     	nop	dword ptr [rax]
   1f610: 31 c0                        	xor	eax, eax
   1f612: 4c 8b 17                     	mov	r10, qword ptr [rdi]
   1f615: 41 b9 01 00 00 00            	mov	r9d, 0x1
   1f61b: f2 48 0f 38 f1 c6            	crc32	rax, rsi
   1f621: 4c 8d 04 06                  	lea	r8, [rsi + rax]
   1f625: be 01 00 00 00               	mov	esi, 0x1
   1f62a: c4 42 e9 f7 c9               	shlx	r9, r9, rdx
   1f62f: 48 c1 e8 18                  	shr	rax, 0x18
   1f633: 48 0f 44 c6                  	cmove	rax, rsi
   1f637: 0f b6 f2                     	movzx	esi, dl
   1f63a: 4c 8d 5c 00 01               	lea	r11, [rax + rax + 0x1]
   1f63f: 62 f2 7d 08 7a c0            	vpbroadcastb	xmm0, eax
   1f645: c4 c2 c8 f5 c0               	bzhi	rax, r8, rsi
   1f64a: 48 69 c0 d0 01 00 00         	imul	rax, rax, 0x1d0
   1f651: 49 8d 3c 02                  	lea	rdi, [r10 + rax]
   1f655: 41 0f 18 8c 02 10 01 00 00   	prefetcht0	byte ptr [r10 + rax + 0x110]
   1f65e: c5 f9 74 0f                  	vpcmpeqb	xmm1, xmm0, xmmword ptr [rdi]
   1f662: c5 f9 d7 d1                  	vpmovmskb	edx, xmm1
   1f666: f7 c2 ff 3f 00 00            	test	edx, 0x3fff
   1f66c: 74 22                        	je	0x1f690 <Ep16DisasmFollyF14ValueFindU64+0xd0>
   1f66e: f3 0f bc c2                  	tzcnt	eax, edx
   1f672: c4 e2 68 f3 ca               	blsr	edx, edx
   1f677: 48 c1 e0 05                  	shl	rax, 0x5
   1f67b: 48 8d 44 07 10               	lea	rax, [rdi + rax + 0x10]
   1f680: 48 3b 08                     	cmp	rcx, qword ptr [rax]
   1f683: 75 e1                        	jne	0x1f666 <Ep16DisasmFollyF14ValueFindU64+0xa6>
   1f685: 48 8b 40 08                  	mov	rax, qword ptr [rax + 0x8]
   1f689: c3                           	ret
   1f68a: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
   1f690: 80 7f 0f 00                  	cmp	byte ptr [rdi + 0xf], 0x0
   1f694: 0f 84 46 ff ff ff            	je	0x1f5e0 <Ep16DisasmFollyF14ValueFindU64+0x20>
   1f69a: 4d 01 d8                     	add	r8, r11
   1f69d: 49 ff c9                     	dec	r9
   1f6a0: 75 a3                        	jne	0x1f645 <Ep16DisasmFollyF14ValueFindU64+0x85>
   1f6a2: e9 39 ff ff ff               	jmp	0x1f5e0 <Ep16DisasmFollyF14ValueFindU64+0x20>
   1f6a7: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]

# --- 5. Live Compiled folly::F14FastMap U64 Lookup (14-way F14VectorMap) ---

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000001f6b0 <Ep16DisasmFollyF14FastFindU64>:
   1f6b0: f3 0f 1e fa                  	endbr64
   1f6b4: 48 8b 57 10                  	mov	rdx, qword ptr [rdi + 0x10]
   1f6b8: 48 89 d0                     	mov	rax, rdx
   1f6bb: 48 c1 e8 08                  	shr	rax, 0x8
   1f6bf: 0f 84 cb 00 00 00            	je	0x1f790 <Ep16DisasmFollyF14FastFindU64+0xe0>
   1f6c5: 55                           	push	rbp
   1f6c6: 31 c0                        	xor	eax, eax
   1f6c8: b9 01 00 00 00               	mov	ecx, 0x1
   1f6cd: 41 ba 01 00 00 00            	mov	r10d, 0x1
   1f6d3: 48 89 e5                     	mov	rbp, rsp
   1f6d6: 41 54                        	push	r12
   1f6d8: 53                           	push	rbx
   1f6d9: 48 8b 5f 08                  	mov	rbx, qword ptr [rdi + 0x8]
   1f6dd: f2 48 0f 38 f1 c6            	crc32	rax, rsi
   1f6e3: 4c 8d 04 06                  	lea	r8, [rsi + rax]
   1f6e7: 44 0f b6 ca                  	movzx	r9d, dl
   1f6eb: c4 42 e9 f7 d2               	shlx	r10, r10, rdx
   1f6f0: 48 c1 e8 18                  	shr	rax, 0x18
   1f6f4: 48 0f 44 c1                  	cmove	rax, rcx
   1f6f8: 4c 8d 64 00 01               	lea	r12, [rax + rax + 0x1]
   1f6fd: 62 f2 7d 08 7a c0            	vpbroadcastb	xmm0, eax
   1f703: c4 c2 b0 f5 c8               	bzhi	rcx, r8, r9
   1f708: 48 c1 e1 06                  	shl	rcx, 0x6
   1f70c: 48 01 d9                     	add	rcx, rbx
   1f70f: c5 f9 74 09                  	vpcmpeqb	xmm1, xmm0, xmmword ptr [rcx]
   1f713: c5 f9 d7 d1                  	vpmovmskb	edx, xmm1
   1f717: f7 c2 ff 0f 00 00            	test	edx, 0xfff
   1f71d: 74 49                        	je	0x1f768 <Ep16DisasmFollyF14FastFindU64+0xb8>
   1f71f: 4c 8b 1f                     	mov	r11, qword ptr [rdi]
   1f722: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1f72d: 0f 1f 00                     	nop	dword ptr [rax]
   1f730: f3 0f bc c2                  	tzcnt	eax, edx
   1f734: c4 e2 68 f3 ca               	blsr	edx, edx
   1f739: 83 c0 04                     	add	eax, 0x4
   1f73c: 48 98                        	cdqe
   1f73e: 8b 04 81                     	mov	eax, dword ptr [rcx + 4*rax]
   1f741: 48 c1 e0 05                  	shl	rax, 0x5
   1f745: 4c 01 d8                     	add	rax, r11
   1f748: 48 3b 30                     	cmp	rsi, qword ptr [rax]
   1f74b: 75 13                        	jne	0x1f760 <Ep16DisasmFollyF14FastFindU64+0xb0>
   1f74d: 5b                           	pop	rbx
   1f74e: 48 8b 40 08                  	mov	rax, qword ptr [rax + 0x8]
   1f752: 41 5c                        	pop	r12
   1f754: 5d                           	pop	rbp
   1f755: c3                           	ret
   1f756: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
   1f760: f7 c2 ff 0f 00 00            	test	edx, 0xfff
   1f766: 75 c8                        	jne	0x1f730 <Ep16DisasmFollyF14FastFindU64+0x80>
   1f768: 80 79 0f 00                  	cmp	byte ptr [rcx + 0xf], 0x0
   1f76c: 75 12                        	jne	0x1f780 <Ep16DisasmFollyF14FastFindU64+0xd0>
   1f76e: 5b                           	pop	rbx
   1f76f: 31 c0                        	xor	eax, eax
   1f771: 41 5c                        	pop	r12
   1f773: 5d                           	pop	rbp
   1f774: c3                           	ret
   1f775: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1f780: 4d 01 e0                     	add	r8, r12
   1f783: 49 ff ca                     	dec	r10
   1f786: 0f 85 77 ff ff ff            	jne	0x1f703 <Ep16DisasmFollyF14FastFindU64+0x53>
   1f78c: eb e0                        	jmp	0x1f76e <Ep16DisasmFollyF14FastFindU64+0xbe>
   1f78e: 66 90                        	nop
   1f790: 31 c0                        	xor	eax, eax
   1f792: c3                           	ret
   1f793: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00       	nop	word ptr cs:[rax + rax]

# --- 6. Live Compiled boost::unordered_flat_map U64 Lookup (15-way group15 + ofw) ---

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000001f7a0 <Ep16DisasmBoostFlatFindU64>:
   1f7a0: f3 0f 1e fa                  	endbr64
   1f7a4: 49 89 f0                     	mov	r8, rsi
   1f7a7: 55                           	push	rbp
   1f7a8: 48 be 15 7c 4a 7f b9 79 37 9e	movabs	rsi, -0x61c8864680b583eb
   1f7b2: 48 8d 0d e7 9e 09 00         	lea	rcx, [rip + 0x99ee7]    # 0xb96a0 <_ZZN5boost9unordered6detail3foa7group15INS2_14plain_integralEE10match_wordEmE4word>
   1f7b9: 4c 89 c2                     	mov	rdx, r8
   1f7bc: 48 89 e5                     	mov	rbp, rsp
   1f7bf: 41 56                        	push	r14
   1f7c1: 49 89 fe                     	mov	r14, rdi
   1f7c4: c4 e2 cb f6 fe               	mulx	rdi, rsi, rsi
   1f7c9: 48 89 f8                     	mov	rax, rdi
   1f7cc: 53                           	push	rbx
   1f7cd: 49 8b 3e                     	mov	rdi, qword ptr [r14]
   1f7d0: 48 31 f0                     	xor	rax, rsi
   1f7d3: 49 8b 5e 10                  	mov	rbx, qword ptr [r14 + 0x10]
   1f7d7: 45 31 db                     	xor	r11d, r11d
   1f7da: 0f b6 f0                     	movzx	esi, al
   1f7dd: c4 e2 79 58 0c b1            	vpbroadcastd	xmm1, dword ptr [rcx + 4*rsi]
   1f7e3: c4 e2 c3 f7 f8               	shrx	rdi, rax, rdi
   1f7e8: 48 89 fe                     	mov	rsi, rdi
   1f7eb: 48 c1 e6 04                  	shl	rsi, 0x4
   1f7ef: 48 01 de                     	add	rsi, rbx
   1f7f2: c5 f1 74 06                  	vpcmpeqb	xmm0, xmm1, xmmword ptr [rsi]
   1f7f6: c5 f9 d7 c8                  	vpmovmskb	ecx, xmm0
   1f7fa: 81 e1 ff 7f 00 00            	and	ecx, 0x7fff
   1f800: 75 1e                        	jne	0x1f820 <Ep16DisasmBoostFlatFindU64+0x80>
   1f802: 48 89 c1                     	mov	rcx, rax
   1f805: 4c 8d 0d 94 a2 09 00         	lea	r9, [rip + 0x9a294]     # 0xb9aa0 <_ZZNK5boost9unordered6detail3foa7group15INS2_14plain_integralEE17is_not_overflowedEmE5shift>
   1f80c: 83 e1 07                     	and	ecx, 0x7
   1f80f: 41 0f b6 0c 09               	movzx	ecx, byte ptr [r9 + rcx]
   1f814: 22 4e 0f                     	and	cl, byte ptr [rsi + 0xf]
   1f817: 75 37                        	jne	0x1f850 <Ep16DisasmBoostFlatFindU64+0xb0>
   1f819: 5b                           	pop	rbx
   1f81a: 31 c0                        	xor	eax, eax
   1f81c: 41 5e                        	pop	r14
   1f81e: 5d                           	pop	rbp
   1f81f: c3                           	ret
   1f820: 4c 69 d7 e0 01 00 00         	imul	r10, rdi, 0x1e0
   1f827: 4d 03 56 18                  	add	r10, qword ptr [r14 + 0x18]
   1f82b: 41 0f 18 0a                  	prefetcht0	byte ptr [r10]
   1f82f: f3 44 0f bc c9               	tzcnt	r9d, ecx
   1f834: 49 c1 e1 05                  	shl	r9, 0x5
   1f838: 4d 01 d1                     	add	r9, r10
   1f83b: 4d 39 01                     	cmp	qword ptr [r9], r8
   1f83e: 75 30                        	jne	0x1f870 <Ep16DisasmBoostFlatFindU64+0xd0>
   1f840: 5b                           	pop	rbx
   1f841: 49 8b 41 08                  	mov	rax, qword ptr [r9 + 0x8]
   1f845: 41 5e                        	pop	r14
   1f847: 5d                           	pop	rbp
   1f848: c3                           	ret
   1f849: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]
   1f850: 49 8b 4e 08                  	mov	rcx, qword ptr [r14 + 0x8]
   1f854: 49 ff c3                     	inc	r11
   1f857: 4c 01 df                     	add	rdi, r11
   1f85a: 48 21 cf                     	and	rdi, rcx
   1f85d: 4c 39 d9                     	cmp	rcx, r11
   1f860: 73 86                        	jae	0x1f7e8 <Ep16DisasmBoostFlatFindU64+0x48>
   1f862: eb b5                        	jmp	0x1f819 <Ep16DisasmBoostFlatFindU64+0x79>
   1f864: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1f86f: 90                           	nop
   1f870: c4 e2 70 f3 c9               	blsr	ecx, ecx
   1f875: 75 b8                        	jne	0x1f82f <Ep16DisasmBoostFlatFindU64+0x8f>
   1f877: eb 89                        	jmp	0x1f802 <Ep16DisasmBoostFlatFindU64+0x62>
   1f879: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]

# --- 7. Live Compiled Story 1 Default vs Transparent vs Noexcept Route Lookups ---

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000001b760 <Ep16StdDefaultFindRoute>:
   1b760: f3 0f 1e fa                  	endbr64
   1b764: 55                           	push	rbp
   1b765: 48 89 e5                     	mov	rbp, rsp
   1b768: 41 57                        	push	r15
   1b76a: 41 56                        	push	r14
   1b76c: 41 55                        	push	r13
   1b76e: 41 54                        	push	r12
   1b770: 53                           	push	rbx
   1b771: 48 81 ec 88 00 00 00         	sub	rsp, 0x88
   1b778: 48 89 7d 98                  	mov	qword ptr [rbp - 0x68], rdi
   1b77c: 48 89 55 88                  	mov	qword ptr [rbp - 0x78], rdx
   1b780: 64 48 8b 04 25 28 00 00 00   	mov	rax, qword ptr fs:[0x28]
   1b789: 48 89 45 c8                  	mov	qword ptr [rbp - 0x38], rax
   1b78d: 31 c0                        	xor	eax, eax
   1b78f: 48 85 d2                     	test	rdx, rdx
   1b792: 0f 84 b3 03 00 00            	je	0x1bb4b <Ep16StdDefaultFindRoute+0x3eb>
   1b798: 48 89 f3                     	mov	rbx, rsi
   1b79b: 45 31 ed                     	xor	r13d, r13d
   1b79e: 4c 8d 65 b0                  	lea	r12, [rbp - 0x50]
   1b7a2: 48 c7 85 78 ff ff ff 00 00 00 00     	mov	qword ptr [rbp - 0x88], 0x0
   1b7ad: 48 c7 45 80 00 00 00 00      	mov	qword ptr [rbp - 0x80], 0x0
   1b7b5: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1b7c0: 4c 8b 73 08                  	mov	r14, qword ptr [rbx + 0x8]
   1b7c4: 4c 8b 3b                     	mov	r15, qword ptr [rbx]
   1b7c7: 4c 89 65 a0                  	mov	qword ptr [rbp - 0x60], r12
   1b7cb: 4d 85 f6                     	test	r14, r14
   1b7ce: 75 09                        	jne	0x1b7d9 <Ep16StdDefaultFindRoute+0x79>
   1b7d0: 4d 85 ff                     	test	r15, r15
   1b7d3: 0f 85 79 03 00 00            	jne	0x1bb52 <Ep16StdDefaultFindRoute+0x3f2>
   1b7d9: 49 83 ff 0f                  	cmp	r15, 0xf
   1b7dd: 0f 87 8d 01 00 00            	ja	0x1b970 <Ep16StdDefaultFindRoute+0x210>
   1b7e3: 49 83 ff 01                  	cmp	r15, 0x1
   1b7e7: 0f 85 73 01 00 00            	jne	0x1b960 <Ep16StdDefaultFindRoute+0x200>
   1b7ed: 41 0f b6 06                  	movzx	eax, byte ptr [r14]
   1b7f1: 88 45 b0                     	mov	byte ptr [rbp - 0x50], al
   1b7f4: 4c 89 e0                     	mov	rax, r12
   1b7f7: 4c 89 7d a8                  	mov	qword ptr [rbp - 0x58], r15
   1b7fb: 42 c6 04 38 00               	mov	byte ptr [rax + r15], 0x0
   1b800: 48 8b 45 98                  	mov	rax, qword ptr [rbp - 0x68]
   1b804: 48 83 78 18 14               	cmp	qword ptr [rax + 0x18], 0x14
   1b809: 0f 87 a1 01 00 00            	ja	0x1b9b0 <Ep16StdDefaultFindRoute+0x250>
   1b80f: 4c 8b 78 10                  	mov	r15, qword ptr [rax + 0x10]
   1b813: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
   1b817: 4d 85 ff                     	test	r15, r15
   1b81a: 0f 84 1a 01 00 00            	je	0x1b93a <Ep16StdDefaultFindRoute+0x1da>
   1b820: 4c 8b 75 a8                  	mov	r14, qword ptr [rbp - 0x58]
   1b824: 4d 85 f6                     	test	r14, r14
   1b827: 0f 84 eb 00 00 00            	je	0x1b918 <Ep16StdDefaultFindRoute+0x1b8>
   1b82d: 4c 89 e0                     	mov	rax, r12
   1b830: 48 89 5d 90                  	mov	qword ptr [rbp - 0x70], rbx
   1b834: 4d 89 f4                     	mov	r12, r14
   1b837: 48 89 fb                     	mov	rbx, rdi
   1b83a: 49 89 c6                     	mov	r14, rax
   1b83d: eb 0d                        	jmp	0x1b84c <Ep16StdDefaultFindRoute+0xec>
   1b83f: 90                           	nop
   1b840: 4d 8b 3f                     	mov	r15, qword ptr [r15]
   1b843: 4d 85 ff                     	test	r15, r15
   1b846: 0f 84 e4 00 00 00            	je	0x1b930 <Ep16StdDefaultFindRoute+0x1d0>
   1b84c: 4d 3b 67 10                  	cmp	r12, qword ptr [r15 + 0x10]
   1b850: 75 ee                        	jne	0x1b840 <Ep16StdDefaultFindRoute+0xe0>
   1b852: 49 8b 77 08                  	mov	rsi, qword ptr [r15 + 0x8]
   1b856: 4c 89 e2                     	mov	rdx, r12
   1b859: 48 89 df                     	mov	rdi, rbx
   1b85c: e8 1f 0d ff ff               	call	0xc580 <memchr@plt+0xc>
   1b861: 85 c0                        	test	eax, eax
   1b863: 75 db                        	jne	0x1b840 <Ep16StdDefaultFindRoute+0xe0>
   1b865: 48 89 df                     	mov	rdi, rbx
   1b868: 48 8b 5d 90                  	mov	rbx, qword ptr [rbp - 0x70]
   1b86c: 4d 89 f4                     	mov	r12, r14
   1b86f: 4c 39 e7                     	cmp	rdi, r12
   1b872: 74 0d                        	je	0x1b881 <Ep16StdDefaultFindRoute+0x121>
   1b874: 48 8b 45 b0                  	mov	rax, qword ptr [rbp - 0x50]
   1b878: 48 8d 70 01                  	lea	rsi, [rax + 0x1]
   1b87c: e8 0f 0d ff ff               	call	0xc590 <memcmp@plt+0xc>
   1b881: 41 8b 57 28                  	mov	edx, dword ptr [r15 + 0x28]
   1b885: 41 8b 4f 2c                  	mov	ecx, dword ptr [r15 + 0x2c]
   1b889: 48 ff 85 78 ff ff ff         	inc	qword ptr [rbp - 0x88]
   1b890: 41 8b 47 34                  	mov	eax, dword ptr [r15 + 0x34]
   1b894: 41 03 47 30                  	add	eax, dword ptr [r15 + 0x30]
   1b898: 49 33 47 38                  	xor	rax, qword ptr [r15 + 0x38]
   1b89c: 48 c1 e2 20                  	shl	rdx, 0x20
   1b8a0: 48 c1 e1 10                  	shl	rcx, 0x10
   1b8a4: 48 31 ca                     	xor	rdx, rcx
   1b8a7: 48 31 d0                     	xor	rax, rdx
   1b8aa: 48 01 45 80                  	add	qword ptr [rbp - 0x80], rax
   1b8ae: 49 ff c5                     	inc	r13
   1b8b1: 48 83 c3 10                  	add	rbx, 0x10
   1b8b5: 4c 39 6d 88                  	cmp	qword ptr [rbp - 0x78], r13
   1b8b9: 0f 85 01 ff ff ff            	jne	0x1b7c0 <Ep16StdDefaultFindRoute+0x60>
   1b8bf: 48 8b 9d 78 ff ff ff         	mov	rbx, qword ptr [rbp - 0x88]
   1b8c6: 48 b8 15 7c 4a 7f b9 79 37 9e	movabs	rax, -0x61c8864680b583eb
   1b8d0: 48 0f af d8                  	imul	rbx, rax
   1b8d4: 48 89 d8                     	mov	rax, rbx
   1b8d7: 48 8b 5d 80                  	mov	rbx, qword ptr [rbp - 0x80]
   1b8db: 48 31 d8                     	xor	rax, rbx
   1b8de: 48 8b 55 c8                  	mov	rdx, qword ptr [rbp - 0x38]
   1b8e2: 64 48 2b 14 25 28 00 00 00   	sub	rdx, qword ptr fs:[0x28]
   1b8eb: 0f 85 97 02 00 00            	jne	0x1bb88 <Ep16StdDefaultFindRoute+0x428>
   1b8f1: 48 81 c4 88 00 00 00         	add	rsp, 0x88
   1b8f8: 5b                           	pop	rbx
   1b8f9: 41 5c                        	pop	r12
   1b8fb: 41 5d                        	pop	r13
   1b8fd: 41 5e                        	pop	r14
   1b8ff: 41 5f                        	pop	r15
   1b901: 5d                           	pop	rbp
   1b902: c3                           	ret
   1b903: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1b90e: 66 90                        	nop
   1b910: 4d 8b 3f                     	mov	r15, qword ptr [r15]
   1b913: 4d 85 ff                     	test	r15, r15
   1b916: 74 22                        	je	0x1b93a <Ep16StdDefaultFindRoute+0x1da>
   1b918: 49 83 7f 10 00               	cmp	qword ptr [r15 + 0x10], 0x0
   1b91d: 75 f1                        	jne	0x1b910 <Ep16StdDefaultFindRoute+0x1b0>
   1b91f: e9 4b ff ff ff               	jmp	0x1b86f <Ep16StdDefaultFindRoute+0x10f>
   1b924: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1b92f: 90                           	nop
   1b930: 48 89 df                     	mov	rdi, rbx
   1b933: 48 8b 5d 90                  	mov	rbx, qword ptr [rbp - 0x70]
   1b937: 4d 89 f4                     	mov	r12, r14
   1b93a: 4c 39 e7                     	cmp	rdi, r12
   1b93d: 0f 84 6b ff ff ff            	je	0x1b8ae <Ep16StdDefaultFindRoute+0x14e>
   1b943: 48 8b 45 b0                  	mov	rax, qword ptr [rbp - 0x50]
   1b947: 48 8d 70 01                  	lea	rsi, [rax + 0x1]
   1b94b: e8 40 0c ff ff               	call	0xc590 <memcmp@plt+0xc>
   1b950: e9 59 ff ff ff               	jmp	0x1b8ae <Ep16StdDefaultFindRoute+0x14e>
   1b955: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1b960: 4d 85 ff                     	test	r15, r15
   1b963: 0f 84 8b fe ff ff            	je	0x1b7f4 <Ep16StdDefaultFindRoute+0x94>
   1b969: 4c 89 e7                     	mov	rdi, r12
   1b96c: eb 27                        	jmp	0x1b995 <Ep16StdDefaultFindRoute+0x235>
   1b96e: 66 90                        	nop
   1b970: 4d 85 ff                     	test	r15, r15
   1b973: 0f 88 f4 01 00 00            	js	0x1bb6d <Ep16StdDefaultFindRoute+0x40d>
   1b979: 4c 89 ff                     	mov	rdi, r15
   1b97c: 48 ff c7                     	inc	rdi
   1b97f: 0f 88 a9 01 00 00            	js	0x1bb2e <Ep16StdDefaultFindRoute+0x3ce>
   1b985: e8 36 0c ff ff               	call	0xc5c0 <__cxa_throw@plt+0xc>
   1b98a: 4c 89 7d b0                  	mov	qword ptr [rbp - 0x50], r15
   1b98e: 48 89 c7                     	mov	rdi, rax
   1b991: 48 89 45 a0                  	mov	qword ptr [rbp - 0x60], rax
   1b995: 4c 89 fa                     	mov	rdx, r15
   1b998: 4c 89 f6                     	mov	rsi, r14
   1b99b: e8 b0 0b ff ff               	call	0xc550 <__cxa_finalize@plt+0xc>
   1b9a0: 48 8b 45 a0                  	mov	rax, qword ptr [rbp - 0x60]
   1b9a4: e9 4e fe ff ff               	jmp	0x1b7f7 <Ep16StdDefaultFindRoute+0x97>
   1b9a9: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]
   1b9b0: 48 8b 75 a8                  	mov	rsi, qword ptr [rbp - 0x58]
   1b9b4: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
   1b9b8: ba 07 69 0f c7               	mov	edx, 0xc70f6907
   1b9bd: e8 9e 0b ff ff               	call	0xc560 <memcpy@plt+0xc>
   1b9c2: 48 8b 4d 98                  	mov	rcx, qword ptr [rbp - 0x68]
   1b9c6: 31 d2                        	xor	edx, edx
   1b9c8: 49 89 c1                     	mov	r9, rax
   1b9cb: 4c 8b 41 08                  	mov	r8, qword ptr [rcx + 0x8]
   1b9cf: 49 f7 f0                     	div	r8
   1b9d2: 48 8b 01                     	mov	rax, qword ptr [rcx]
   1b9d5: 4c 8b 14 d0                  	mov	r10, qword ptr [rax + 8*rdx]
   1b9d9: 49 89 d3                     	mov	r11, rdx
   1b9dc: 4d 85 d2                     	test	r10, r10
   1b9df: 0f 84 5d 01 00 00            	je	0x1bb42 <Ep16StdDefaultFindRoute+0x3e2>
   1b9e5: 4d 8b 3a                     	mov	r15, qword ptr [r10]
   1b9e8: 48 8b 45 a8                  	mov	rax, qword ptr [rbp - 0x58]
   1b9ec: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
   1b9f0: 49 8b 4f 40                  	mov	rcx, qword ptr [r15 + 0x40]
   1b9f4: 48 85 c0                     	test	rax, rax
   1b9f7: 0f 84 1a 01 00 00            	je	0x1bb17 <Ep16StdDefaultFindRoute+0x3b7>
   1b9fd: 4c 89 ca                     	mov	rdx, r9
   1ba00: 4d 89 c6                     	mov	r14, r8
   1ba03: 4d 89 e8                     	mov	r8, r13
   1ba06: 49 89 c5                     	mov	r13, rax
   1ba09: 4c 89 e0                     	mov	rax, r12
   1ba0c: 49 89 d9                     	mov	r9, rbx
   1ba0f: 4d 89 dc                     	mov	r12, r11
   1ba12: 48 89 d3                     	mov	rbx, rdx
   1ba15: 49 89 c3                     	mov	r11, rax
   1ba18: eb 2d                        	jmp	0x1ba47 <Ep16StdDefaultFindRoute+0x2e7>
   1ba1a: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
   1ba20: 49 8b 37                     	mov	rsi, qword ptr [r15]
   1ba23: 48 85 f6                     	test	rsi, rsi
   1ba26: 0f 84 f4 00 00 00            	je	0x1bb20 <Ep16StdDefaultFindRoute+0x3c0>
   1ba2c: 48 8b 4e 40                  	mov	rcx, qword ptr [rsi + 0x40]
   1ba30: 31 d2                        	xor	edx, edx
   1ba32: 4d 89 fa                     	mov	r10, r15
   1ba35: 48 89 c8                     	mov	rax, rcx
   1ba38: 49 f7 f6                     	div	r14
   1ba3b: 49 39 d4                     	cmp	r12, rdx
   1ba3e: 0f 85 dc 00 00 00            	jne	0x1bb20 <Ep16StdDefaultFindRoute+0x3c0>
   1ba44: 49 89 f7                     	mov	r15, rsi
   1ba47: 48 39 cb                     	cmp	rbx, rcx
   1ba4a: 75 d4                        	jne	0x1ba20 <Ep16StdDefaultFindRoute+0x2c0>
   1ba4c: 4d 3b 6f 10                  	cmp	r13, qword ptr [r15 + 0x10]
   1ba50: 75 ce                        	jne	0x1ba20 <Ep16StdDefaultFindRoute+0x2c0>
   1ba52: 49 8b 77 08                  	mov	rsi, qword ptr [r15 + 0x8]
   1ba56: 4c 89 ea                     	mov	rdx, r13
   1ba59: 4c 89 9d 58 ff ff ff         	mov	qword ptr [rbp - 0xa8], r11
   1ba60: 4c 89 95 60 ff ff ff         	mov	qword ptr [rbp - 0xa0], r10
   1ba67: 4c 89 8d 68 ff ff ff         	mov	qword ptr [rbp - 0x98], r9
   1ba6e: 4c 89 85 70 ff ff ff         	mov	qword ptr [rbp - 0x90], r8
   1ba75: 48 89 7d 90                  	mov	qword ptr [rbp - 0x70], rdi
   1ba79: e8 02 0b ff ff               	call	0xc580 <memchr@plt+0xc>
   1ba7e: 48 8b 7d 90                  	mov	rdi, qword ptr [rbp - 0x70]
   1ba82: 4c 8b 85 70 ff ff ff         	mov	r8, qword ptr [rbp - 0x90]
   1ba89: 4c 8b 8d 68 ff ff ff         	mov	r9, qword ptr [rbp - 0x98]
   1ba90: 85 c0                        	test	eax, eax
   1ba92: 4c 8b 95 60 ff ff ff         	mov	r10, qword ptr [rbp - 0xa0]
   1ba99: 4c 8b 9d 58 ff ff ff         	mov	r11, qword ptr [rbp - 0xa8]
   1baa0: 0f 85 7a ff ff ff            	jne	0x1ba20 <Ep16StdDefaultFindRoute+0x2c0>
   1baa6: 4d 89 c5                     	mov	r13, r8
   1baa9: 4c 89 cb                     	mov	rbx, r9
   1baac: 4d 89 dc                     	mov	r12, r11
   1baaf: 4d 8b 3a                     	mov	r15, qword ptr [r10]
   1bab2: 4c 39 e7                     	cmp	rdi, r12
   1bab5: 74 0d                        	je	0x1bac4 <Ep16StdDefaultFindRoute+0x364>
   1bab7: 48 8b 45 b0                  	mov	rax, qword ptr [rbp - 0x50]
   1babb: 48 8d 70 01                  	lea	rsi, [rax + 0x1]
   1babf: e8 cc 0a ff ff               	call	0xc590 <memcmp@plt+0xc>
   1bac4: 4d 85 ff                     	test	r15, r15
   1bac7: 0f 85 b4 fd ff ff            	jne	0x1b881 <Ep16StdDefaultFindRoute+0x121>
   1bacd: e9 dc fd ff ff               	jmp	0x1b8ae <Ep16StdDefaultFindRoute+0x14e>
   1bad2: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1badd: 0f 1f 00                     	nop	dword ptr [rax]
   1bae0: 49 83 7f 10 00               	cmp	qword ptr [r15 + 0x10], 0x0
   1bae5: 74 c8                        	je	0x1baaf <Ep16StdDefaultFindRoute+0x34f>
   1bae7: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
   1baf0: 49 8b 37                     	mov	rsi, qword ptr [r15]
   1baf3: 48 85 f6                     	test	rsi, rsi
   1baf6: 0f 84 3e fe ff ff            	je	0x1b93a <Ep16StdDefaultFindRoute+0x1da>
   1bafc: 48 8b 4e 40                  	mov	rcx, qword ptr [rsi + 0x40]
   1bb00: 31 d2                        	xor	edx, edx
   1bb02: 4d 89 fa                     	mov	r10, r15
   1bb05: 48 89 c8                     	mov	rax, rcx
   1bb08: 49 f7 f0                     	div	r8
   1bb0b: 49 39 d3                     	cmp	r11, rdx
   1bb0e: 0f 85 26 fe ff ff            	jne	0x1b93a <Ep16StdDefaultFindRoute+0x1da>
   1bb14: 49 89 f7                     	mov	r15, rsi
   1bb17: 4c 39 c9                     	cmp	rcx, r9
   1bb1a: 75 d4                        	jne	0x1baf0 <Ep16StdDefaultFindRoute+0x390>
   1bb1c: eb c2                        	jmp	0x1bae0 <Ep16StdDefaultFindRoute+0x380>
   1bb1e: 66 90                        	nop
   1bb20: 4d 89 c5                     	mov	r13, r8
   1bb23: 4c 89 cb                     	mov	rbx, r9
   1bb26: 4d 89 dc                     	mov	r12, r11
   1bb29: e9 0c fe ff ff               	jmp	0x1b93a <Ep16StdDefaultFindRoute+0x1da>
   1bb2e: 48 8b 45 c8                  	mov	rax, qword ptr [rbp - 0x38]
   1bb32: 64 48 2b 04 25 28 00 00 00   	sub	rax, qword ptr fs:[0x28]
   1bb3b: 75 4b                        	jne	0x1bb88 <Ep16StdDefaultFindRoute+0x428>
   1bb3d: e8 9e 0a ff ff               	call	0xc5e0 <_ZSt28__throw_bad_array_new_lengthv@plt+0xc>
   1bb42: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
   1bb46: e9 ef fd ff ff               	jmp	0x1b93a <Ep16StdDefaultFindRoute+0x1da>
   1bb4b: 31 c0                        	xor	eax, eax
   1bb4d: e9 8c fd ff ff               	jmp	0x1b8de <Ep16StdDefaultFindRoute+0x17e>
   1bb52: 48 8b 45 c8                  	mov	rax, qword ptr [rbp - 0x38]
   1bb56: 64 48 2b 04 25 28 00 00 00   	sub	rax, qword ptr fs:[0x28]
   1bb5f: 75 27                        	jne	0x1bb88 <Ep16StdDefaultFindRoute+0x428>
   1bb61: 48 8d 3d 78 b6 09 00         	lea	rdi, [rip + 0x9b678]    # 0xb71e0 <write+0xb71e0>
   1bb68: e8 a3 0a ff ff               	call	0xc610 <_ZNSt6chrono3_V212steady_clock3nowEv@plt+0xc>
   1bb6d: 48 8b 45 c8                  	mov	rax, qword ptr [rbp - 0x38]
   1bb71: 64 48 2b 04 25 28 00 00 00   	sub	rax, qword ptr fs:[0x28]
   1bb7a: 75 0c                        	jne	0x1bb88 <Ep16StdDefaultFindRoute+0x428>
   1bb7c: 48 8d 3d 3d 97 09 00         	lea	rdi, [rip + 0x9973d]    # 0xb52c0 <write+0xb52c0>
   1bb83: e8 68 0a ff ff               	call	0xc5f0 <_ZSt17__throw_bad_allocv@plt+0xc>
   1bb88: e8 93 0a ff ff               	call	0xc620 <_ZSt19__throw_logic_errorPKc@plt+0xc>
   1bb8d: 0f 1f 00                     	nop	dword ptr [rax]

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000001c520 <Ep16StdTransparentFindRoute>:
   1c520: f3 0f 1e fa                  	endbr64
   1c524: 55                           	push	rbp
   1c525: 48 89 e5                     	mov	rbp, rsp
   1c528: 41 57                        	push	r15
   1c52a: 41 56                        	push	r14
   1c52c: 41 55                        	push	r13
   1c52e: 41 54                        	push	r12
   1c530: 53                           	push	rbx
   1c531: 48 83 ec 38                  	sub	rsp, 0x38
   1c535: 48 89 55 c8                  	mov	qword ptr [rbp - 0x38], rdx
   1c539: 48 85 d2                     	test	rdx, rdx
   1c53c: 0f 84 56 01 00 00            	je	0x1c698 <Ep16StdTransparentFindRoute+0x178>
   1c542: 49 89 fc                     	mov	r12, rdi
   1c545: 49 89 f7                     	mov	r15, rsi
   1c548: 31 db                        	xor	ebx, ebx
   1c54a: 45 31 ed                     	xor	r13d, r13d
   1c54d: 48 c7 45 a0 00 00 00 00      	mov	qword ptr [rbp - 0x60], 0x0
   1c555: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]
   1c560: 49 8b 37                     	mov	rsi, qword ptr [r15]
   1c563: 49 8b 7f 08                  	mov	rdi, qword ptr [r15 + 0x8]
   1c567: ba 07 69 0f c7               	mov	edx, 0xc70f6907
   1c56c: e8 ef ff fe ff               	call	0xc560 <memcpy@plt+0xc>
   1c571: 4d 8b 4c 24 08               	mov	r9, qword ptr [r12 + 0x8]
   1c576: 31 d2                        	xor	edx, edx
   1c578: 49 89 c2                     	mov	r10, rax
   1c57b: 49 f7 f1                     	div	r9
   1c57e: 49 8b 04 24                  	mov	rax, qword ptr [r12]
   1c582: 4c 8b 34 d0                  	mov	r14, qword ptr [rax + 8*rdx]
   1c586: 49 89 d3                     	mov	r11, rdx
   1c589: 4d 85 f6                     	test	r14, r14
   1c58c: 0f 84 bf 00 00 00            	je	0x1c651 <Ep16StdTransparentFindRoute+0x131>
   1c592: 4d 8b 06                     	mov	r8, qword ptr [r14]
   1c595: 4c 89 e0                     	mov	rax, r12
   1c598: 49 89 dc                     	mov	r12, rbx
   1c59b: 4c 89 f3                     	mov	rbx, r14
   1c59e: 49 8b 48 40                  	mov	rcx, qword ptr [r8 + 0x40]
   1c5a2: 4d 89 c6                     	mov	r14, r8
   1c5a5: 49 89 c0                     	mov	r8, rax
   1c5a8: eb 2d                        	jmp	0x1c5d7 <Ep16StdTransparentFindRoute+0xb7>
   1c5aa: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
   1c5b0: 49 8b 36                     	mov	rsi, qword ptr [r14]
   1c5b3: 48 85 f6                     	test	rsi, rsi
   1c5b6: 0f 84 d4 00 00 00            	je	0x1c690 <Ep16StdTransparentFindRoute+0x170>
   1c5bc: 48 8b 4e 40                  	mov	rcx, qword ptr [rsi + 0x40]
   1c5c0: 31 d2                        	xor	edx, edx
   1c5c2: 4c 89 f3                     	mov	rbx, r14
   1c5c5: 48 89 c8                     	mov	rax, rcx
   1c5c8: 49 f7 f1                     	div	r9
   1c5cb: 49 39 d3                     	cmp	r11, rdx
   1c5ce: 0f 85 bc 00 00 00            	jne	0x1c690 <Ep16StdTransparentFindRoute+0x170>
   1c5d4: 49 89 f6                     	mov	r14, rsi
   1c5d7: 49 39 ca                     	cmp	r10, rcx
   1c5da: 75 d4                        	jne	0x1c5b0 <Ep16StdTransparentFindRoute+0x90>
   1c5dc: 49 8b 17                     	mov	rdx, qword ptr [r15]
   1c5df: 49 3b 56 10                  	cmp	rdx, qword ptr [r14 + 0x10]
   1c5e3: 75 cb                        	jne	0x1c5b0 <Ep16StdTransparentFindRoute+0x90>
   1c5e5: 4c 89 5d b0                  	mov	qword ptr [rbp - 0x50], r11
   1c5e9: 4c 89 4d b8                  	mov	qword ptr [rbp - 0x48], r9
   1c5ed: 4c 89 55 c0                  	mov	qword ptr [rbp - 0x40], r10
   1c5f1: 48 85 d2                     	test	rdx, rdx
   1c5f4: 74 25                        	je	0x1c61b <Ep16StdTransparentFindRoute+0xfb>
   1c5f6: 49 8b 76 08                  	mov	rsi, qword ptr [r14 + 0x8]
   1c5fa: 49 8b 7f 08                  	mov	rdi, qword ptr [r15 + 0x8]
   1c5fe: 4c 89 45 a8                  	mov	qword ptr [rbp - 0x58], r8
   1c602: e8 79 ff fe ff               	call	0xc580 <memchr@plt+0xc>
   1c607: 4c 8b 45 a8                  	mov	r8, qword ptr [rbp - 0x58]
   1c60b: 4c 8b 55 c0                  	mov	r10, qword ptr [rbp - 0x40]
   1c60f: 4c 8b 4d b8                  	mov	r9, qword ptr [rbp - 0x48]
   1c613: 85 c0                        	test	eax, eax
   1c615: 4c 8b 5d b0                  	mov	r11, qword ptr [rbp - 0x50]
   1c619: 75 95                        	jne	0x1c5b0 <Ep16StdTransparentFindRoute+0x90>
   1c61b: 49 89 de                     	mov	r14, rbx
   1c61e: 4c 89 e3                     	mov	rbx, r12
   1c621: 4d 89 c4                     	mov	r12, r8
   1c624: 49 8b 16                     	mov	rdx, qword ptr [r14]
   1c627: 48 85 d2                     	test	rdx, rdx
   1c62a: 74 25                        	je	0x1c651 <Ep16StdTransparentFindRoute+0x131>
   1c62c: 8b 42 28                     	mov	eax, dword ptr [rdx + 0x28]
   1c62f: 8b 4a 2c                     	mov	ecx, dword ptr [rdx + 0x2c]
   1c632: 49 ff c5                     	inc	r13
   1c635: 48 c1 e1 10                  	shl	rcx, 0x10
   1c639: 48 c1 e0 20                  	shl	rax, 0x20
   1c63d: 48 31 c8                     	xor	rax, rcx
   1c640: 8b 4a 34                     	mov	ecx, dword ptr [rdx + 0x34]
   1c643: 03 4a 30                     	add	ecx, dword ptr [rdx + 0x30]
   1c646: 48 33 4a 38                  	xor	rcx, qword ptr [rdx + 0x38]
   1c64a: 48 31 c8                     	xor	rax, rcx
   1c64d: 48 01 45 a0                  	add	qword ptr [rbp - 0x60], rax
   1c651: 48 ff c3                     	inc	rbx
   1c654: 49 83 c7 10                  	add	r15, 0x10
   1c658: 48 39 5d c8                  	cmp	qword ptr [rbp - 0x38], rbx
   1c65c: 0f 85 fe fe ff ff            	jne	0x1c560 <Ep16StdTransparentFindRoute+0x40>
   1c662: 48 8b 7d a0                  	mov	rdi, qword ptr [rbp - 0x60]
   1c666: 48 b8 15 7c 4a 7f b9 79 37 9e	movabs	rax, -0x61c8864680b583eb
   1c670: 48 83 c4 38                  	add	rsp, 0x38
   1c674: 49 0f af c5                  	imul	rax, r13
   1c678: 5b                           	pop	rbx
   1c679: 41 5c                        	pop	r12
   1c67b: 41 5d                        	pop	r13
   1c67d: 41 5e                        	pop	r14
   1c67f: 41 5f                        	pop	r15
   1c681: 5d                           	pop	rbp
   1c682: 48 31 f8                     	xor	rax, rdi
   1c685: c3                           	ret
   1c686: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
   1c690: 4c 89 e3                     	mov	rbx, r12
   1c693: 4d 89 c4                     	mov	r12, r8
   1c696: eb b9                        	jmp	0x1c651 <Ep16StdTransparentFindRoute+0x131>
   1c698: 48 83 c4 38                  	add	rsp, 0x38
   1c69c: 31 c0                        	xor	eax, eax
   1c69e: 5b                           	pop	rbx
   1c69f: 41 5c                        	pop	r12
   1c6a1: 41 5d                        	pop	r13
   1c6a3: 41 5e                        	pop	r14
   1c6a5: 41 5f                        	pop	r15
   1c6a7: 5d                           	pop	rbp
   1c6a8: c3                           	ret
   1c6a9: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]

/home/kelvingorekore/projects/hotpathcpp/bazel-bin/episodes/16_cpp_hash_maps_deep_dive/challenge:	file format elf64-x86-64

Disassembly of section .text:

000000000001abb0 <Ep16StdTransparentNoexceptFindRoute>:
   1abb0: f3 0f 1e fa                  	endbr64
   1abb4: 55                           	push	rbp
   1abb5: 48 89 e5                     	mov	rbp, rsp
   1abb8: 41 57                        	push	r15
   1abba: 41 56                        	push	r14
   1abbc: 41 55                        	push	r13
   1abbe: 41 54                        	push	r12
   1abc0: 53                           	push	rbx
   1abc1: 48 83 ec 28                  	sub	rsp, 0x28
   1abc5: 48 89 55 c0                  	mov	qword ptr [rbp - 0x40], rdx
   1abc9: 48 85 d2                     	test	rdx, rdx
   1abcc: 0f 84 05 01 00 00            	je	0x1acd7 <Ep16StdTransparentNoexceptFindRoute+0x127>
   1abd2: 49 89 fd                     	mov	r13, rdi
   1abd5: 49 89 f4                     	mov	r12, rsi
   1abd8: 48 c7 45 c8 00 00 00 00      	mov	qword ptr [rbp - 0x38], 0x0
   1abe0: 48 c7 45 b0 00 00 00 00      	mov	qword ptr [rbp - 0x50], 0x0
   1abe8: 48 c7 45 b8 00 00 00 00      	mov	qword ptr [rbp - 0x48], 0x0
   1abf0: 49 8b 34 24                  	mov	rsi, qword ptr [r12]
   1abf4: 49 8b 7c 24 08               	mov	rdi, qword ptr [r12 + 0x8]
   1abf9: ba 07 69 0f c7               	mov	edx, 0xc70f6907
   1abfe: e8 5d 19 ff ff               	call	0xc560 <memcpy@plt+0xc>
   1ac03: 31 d2                        	xor	edx, edx
   1ac05: 49 f7 75 08                  	div	qword ptr [r13 + 0x8]
   1ac09: 49 8b 45 00                  	mov	rax, qword ptr [r13]
   1ac0d: 48 8b 04 d0                  	mov	rax, qword ptr [rax + 8*rdx]
   1ac11: 49 89 d6                     	mov	r14, rdx
   1ac14: 48 85 c0                     	test	rax, rax
   1ac17: 74 7d                        	je	0x1ac96 <Ep16StdTransparentNoexceptFindRoute+0xe6>
   1ac19: 4c 8b 38                     	mov	r15, qword ptr [rax]
   1ac1c: eb 2d                        	jmp	0x1ac4b <Ep16StdTransparentNoexceptFindRoute+0x9b>
   1ac1e: 66 90                        	nop
   1ac20: 49 8b 07                     	mov	rax, qword ptr [r15]
   1ac23: 48 85 c0                     	test	rax, rax
   1ac26: 74 6e                        	je	0x1ac96 <Ep16StdTransparentNoexceptFindRoute+0xe6>
   1ac28: 48 8b 70 10                  	mov	rsi, qword ptr [rax + 0x10]
   1ac2c: 48 8b 78 08                  	mov	rdi, qword ptr [rax + 0x8]
   1ac30: 49 8b 5d 08                  	mov	rbx, qword ptr [r13 + 0x8]
   1ac34: ba 07 69 0f c7               	mov	edx, 0xc70f6907
   1ac39: e8 22 19 ff ff               	call	0xc560 <memcpy@plt+0xc>
   1ac3e: 31 d2                        	xor	edx, edx
   1ac40: 48 f7 f3                     	div	rbx
   1ac43: 49 39 d6                     	cmp	r14, rdx
   1ac46: 75 4e                        	jne	0x1ac96 <Ep16StdTransparentNoexceptFindRoute+0xe6>
   1ac48: 4d 8b 3f                     	mov	r15, qword ptr [r15]
   1ac4b: 49 8b 14 24                  	mov	rdx, qword ptr [r12]
   1ac4f: 49 8b 77 08                  	mov	rsi, qword ptr [r15 + 0x8]
   1ac53: 49 8b 7c 24 08               	mov	rdi, qword ptr [r12 + 0x8]
   1ac58: 49 3b 57 10                  	cmp	rdx, qword ptr [r15 + 0x10]
   1ac5c: 75 c2                        	jne	0x1ac20 <Ep16StdTransparentNoexceptFindRoute+0x70>
   1ac5e: 48 85 d2                     	test	rdx, rdx
   1ac61: 74 09                        	je	0x1ac6c <Ep16StdTransparentNoexceptFindRoute+0xbc>
   1ac63: e8 18 19 ff ff               	call	0xc580 <memchr@plt+0xc>
   1ac68: 85 c0                        	test	eax, eax
   1ac6a: 75 b4                        	jne	0x1ac20 <Ep16StdTransparentNoexceptFindRoute+0x70>
   1ac6c: 41 8b 57 28                  	mov	edx, dword ptr [r15 + 0x28]
   1ac70: 41 8b 4f 2c                  	mov	ecx, dword ptr [r15 + 0x2c]
   1ac74: 41 8b 47 34                  	mov	eax, dword ptr [r15 + 0x34]
   1ac78: 48 ff 45 b0                  	inc	qword ptr [rbp - 0x50]
   1ac7c: 41 03 47 30                  	add	eax, dword ptr [r15 + 0x30]
   1ac80: 49 33 47 38                  	xor	rax, qword ptr [r15 + 0x38]
   1ac84: 48 c1 e2 20                  	shl	rdx, 0x20
   1ac88: 48 c1 e1 10                  	shl	rcx, 0x10
   1ac8c: 48 31 ca                     	xor	rdx, rcx
   1ac8f: 48 31 d0                     	xor	rax, rdx
   1ac92: 48 01 45 b8                  	add	qword ptr [rbp - 0x48], rax
   1ac96: 48 ff 45 c8                  	inc	qword ptr [rbp - 0x38]
   1ac9a: 48 8b 45 c8                  	mov	rax, qword ptr [rbp - 0x38]
   1ac9e: 49 83 c4 10                  	add	r12, 0x10
   1aca2: 48 39 45 c0                  	cmp	qword ptr [rbp - 0x40], rax
   1aca6: 0f 85 44 ff ff ff            	jne	0x1abf0 <Ep16StdTransparentNoexceptFindRoute+0x40>
   1acac: 48 8b 4d b0                  	mov	rcx, qword ptr [rbp - 0x50]
   1acb0: 48 b8 15 7c 4a 7f b9 79 37 9e	movabs	rax, -0x61c8864680b583eb
   1acba: 48 0f af c8                  	imul	rcx, rax
   1acbe: 48 89 c8                     	mov	rax, rcx
   1acc1: 48 8b 4d b8                  	mov	rcx, qword ptr [rbp - 0x48]
   1acc5: 48 83 c4 28                  	add	rsp, 0x28
   1acc9: 5b                           	pop	rbx
   1acca: 41 5c                        	pop	r12
   1accc: 41 5d                        	pop	r13
   1acce: 41 5e                        	pop	r14
   1acd0: 41 5f                        	pop	r15
   1acd2: 5d                           	pop	rbp
   1acd3: 48 31 c8                     	xor	rax, rcx
   1acd6: c3                           	ret
   1acd7: 48 83 c4 28                  	add	rsp, 0x28
   1acdb: 31 c0                        	xor	eax, eax
   1acdd: 5b                           	pop	rbx
   1acde: 41 5c                        	pop	r12
   1ace0: 41 5d                        	pop	r13
   1ace2: 41 5e                        	pop	r14
   1ace4: 41 5f                        	pop	r15
   1ace6: 5d                           	pop	rbp
   1ace7: c3                           	ret
   1ace8: 0f 1f 84 00 00 00 00 00      	nop	dword ptr [rax + rax]

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
# EPISODE 14: THE PHYSICS OF MEMORY SAFETY (AMD RYZEN 9 9950X - ZEN 5)
# Compiler 1: Ubuntu clang version 18.1.3 (-std=c++20 -O3 -march=native)
# Compiler 2: rustc 1.91.0 (-C opt-level=3 -C target-cpu=native)
# ==============================================================================

# --- 1. CLANG 18: Unchecked Batch Gather (AVX-512 vpgatherqq) ---

/tmp/ep14_build/challenge_clang_unchecked:	file format elf64-x86-64

Disassembly of section .text:

0000000000001290 <ep14_gather_batch_unchecked>:
    1290: 48 85 d2                     	test	rdx, rdx
    1293: 74 0f                        	je	0x12a4 <ep14_gather_batch_unchecked+0x14>
    1295: 48 83 fa 20                  	cmp	rdx, 0x20
    1299: 73 0c                        	jae	0x12a7 <ep14_gather_batch_unchecked+0x17>
    129b: 31 c0                        	xor	eax, eax
    129d: 31 c9                        	xor	ecx, ecx
    129f: e9 bc 01 00 00               	jmp	0x1460 <ep14_gather_batch_unchecked+0x1d0>
    12a4: 31 c0                        	xor	eax, eax
    12a6: c3                           	ret
    12a7: 48 89 d1                     	mov	rcx, rdx
    12aa: 48 83 e1 e0                  	and	rcx, -0x20
    12ae: c5 f9 ef c0                  	vpxor	xmm0, xmm0, xmm0
    12b2: 31 c0                        	xor	eax, eax
    12b4: 62 f2 fd 48 7c cf            	vpbroadcastq	zmm1, rdi
    12ba: c5 e9 ef d2                  	vpxor	xmm2, xmm2, xmm2
    12be: c5 e1 ef db                  	vpxor	xmm3, xmm3, xmm3
    12c2: c5 d9 ef e4                  	vpxor	xmm4, xmm4, xmm4
    12c6: 66 2e 0f 1f 84 00 00 00 00 00	nop	word ptr cs:[rax + rax]
    12d0: 62 f2 7d 48 35 2c 86         	vpmovzxdq	zmm5, ymmword ptr [rsi + 4*rax] # zmm5 = mem[0],zero,mem[1],zero,mem[2],zero,mem[3],zero,mem[4],zero,mem[5],zero,mem[6],zero,mem[7],zero
    12d7: 62 f2 7d 48 35 74 86 01      	vpmovzxdq	zmm6, ymmword ptr [rsi + 4*rax + 0x20] # zmm6 = mem[0],zero,mem[1],zero,mem[2],zero,mem[3],zero,mem[4],zero,mem[5],zero,mem[6],zero,mem[7],zero
    12df: 62 f2 7d 48 35 7c 86 02      	vpmovzxdq	zmm7, ymmword ptr [rsi + 4*rax + 0x40] # zmm7 = mem[0],zero,mem[1],zero,mem[2],zero,mem[3],zero,mem[4],zero,mem[5],zero,mem[6],zero,mem[7],zero
    12e7: 62 72 7d 48 35 44 86 03      	vpmovzxdq	zmm8, ymmword ptr [rsi + 4*rax + 0x60] # zmm8 = mem[0],zero,mem[1],zero,mem[2],zero,mem[3],zero,mem[4],zero,mem[5],zero,mem[6],zero,mem[7],zero
    12ef: 62 f1 d5 48 73 f5 05         	vpsllq	zmm5, zmm5, 0x5
    12f6: 62 f1 f5 48 d4 ed            	vpaddq	zmm5, zmm1, zmm5
    12fc: 62 f1 cd 48 73 f6 05         	vpsllq	zmm6, zmm6, 0x5
    1303: 62 f1 f5 48 d4 f6            	vpaddq	zmm6, zmm1, zmm6
    1309: 62 f1 c5 48 73 f7 05         	vpsllq	zmm7, zmm7, 0x5
    1310: 62 f1 f5 48 d4 ff            	vpaddq	zmm7, zmm1, zmm7
    1316: 62 d1 bd 48 73 f0 05         	vpsllq	zmm8, zmm8, 0x5
    131d: 62 51 f5 48 d4 c0            	vpaddq	zmm8, zmm1, zmm8
    1323: c4 41 31 ef c9               	vpxor	xmm9, xmm9, xmm9
    1328: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    132c: 62 72 fd 49 91 0c 2d 08 00 00 00     	vpgatherqq	zmm9 {k1}, zmmword ptr [1*zmm5 + 0x8]
    1337: c4 41 29 ef d2               	vpxor	xmm10, xmm10, xmm10
    133c: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    1340: 62 72 fd 49 91 14 35 08 00 00 00     	vpgatherqq	zmm10 {k1}, zmmword ptr [1*zmm6 + 0x8]
    134b: c4 41 21 ef db               	vpxor	xmm11, xmm11, xmm11
    1350: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    1354: 62 72 fd 49 91 1c 3d 08 00 00 00     	vpgatherqq	zmm11 {k1}, zmmword ptr [1*zmm7 + 0x8]
    135f: c4 41 19 ef e4               	vpxor	xmm12, xmm12, xmm12
    1364: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    1368: 62 32 fd 49 91 24 05 08 00 00 00     	vpgatherqq	zmm12 {k1}, zmmword ptr [1*zmm8 + 0x8]
    1373: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    1377: c4 41 11 ef ed               	vpxor	xmm13, xmm13, xmm13
    137c: 62 72 7d 49 91 2c 2d 10 00 00 00     	vpgatherqd	ymm13 {k1}, ymmword ptr [1*zmm5 + 0x10]
    1387: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    138b: c5 d1 ef ed                  	vpxor	xmm5, xmm5, xmm5
    138f: 62 f2 7d 49 91 2c 35 10 00 00 00     	vpgatherqd	ymm5 {k1}, ymmword ptr [1*zmm6 + 0x10]
    139a: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    139e: c5 c9 ef f6                  	vpxor	xmm6, xmm6, xmm6
    13a2: 62 f2 7d 49 91 34 3d 10 00 00 00     	vpgatherqd	ymm6 {k1}, ymmword ptr [1*zmm7 + 0x10]
    13ad: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    13b1: c5 c1 ef ff                  	vpxor	xmm7, xmm7, xmm7
    13b5: 62 b2 7d 49 91 3c 05 10 00 00 00     	vpgatherqd	ymm7 {k1}, ymmword ptr [1*zmm8 + 0x10]
    13c0: 62 52 7d 48 35 c5            	vpmovzxdq	zmm8, ymm13     # zmm8 = ymm13[0],zero,ymm13[1],zero,ymm13[2],zero,ymm13[3],zero,ymm13[4],zero,ymm13[5],zero,ymm13[6],zero,ymm13[7],zero
    13c6: 62 52 b5 48 40 c0            	vpmullq	zmm8, zmm9, zmm8
    13cc: 62 f1 bd 48 d4 c0            	vpaddq	zmm0, zmm8, zmm0
    13d2: 62 f2 7d 48 35 ed            	vpmovzxdq	zmm5, ymm5      # zmm5 = ymm5[0],zero,ymm5[1],zero,ymm5[2],zero,ymm5[3],zero,ymm5[4],zero,ymm5[5],zero,ymm5[6],zero,ymm5[7],zero
    13d8: 62 f2 ad 48 40 ed            	vpmullq	zmm5, zmm10, zmm5
    13de: 62 f1 d5 48 d4 d2            	vpaddq	zmm2, zmm5, zmm2
    13e4: 62 f2 7d 48 35 ee            	vpmovzxdq	zmm5, ymm6      # zmm5 = ymm6[0],zero,ymm6[1],zero,ymm6[2],zero,ymm6[3],zero,ymm6[4],zero,ymm6[5],zero,ymm6[6],zero,ymm6[7],zero
    13ea: 62 f2 a5 48 40 ed            	vpmullq	zmm5, zmm11, zmm5
    13f0: 62 f1 d5 48 d4 db            	vpaddq	zmm3, zmm5, zmm3
    13f6: 62 f2 7d 48 35 ef            	vpmovzxdq	zmm5, ymm7      # zmm5 = ymm7[0],zero,ymm7[1],zero,ymm7[2],zero,ymm7[3],zero,ymm7[4],zero,ymm7[5],zero,ymm7[6],zero,ymm7[7],zero
    13fc: 62 f2 9d 48 40 ed            	vpmullq	zmm5, zmm12, zmm5
    1402: 62 f1 d5 48 d4 e4            	vpaddq	zmm4, zmm5, zmm4
    1408: 48 83 c0 20                  	add	rax, 0x20
    140c: 48 39 c1                     	cmp	rcx, rax
    140f: 0f 85 bb fe ff ff            	jne	0x12d0 <ep14_gather_batch_unchecked+0x40>
    1415: 62 f1 ed 48 d4 c0            	vpaddq	zmm0, zmm2, zmm0
    141b: 62 f1 e5 48 d4 c0            	vpaddq	zmm0, zmm3, zmm0
    1421: 62 f1 dd 48 d4 c0            	vpaddq	zmm0, zmm4, zmm0
    1427: 62 f3 fd 48 3b c1 01         	vextracti64x4	ymm1, zmm0, 0x1
    142e: 62 f1 fd 48 d4 c1            	vpaddq	zmm0, zmm0, zmm1
    1434: c4 e3 7d 39 c1 01            	vextracti128	xmm1, ymm0, 0x1
    143a: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
    143e: c5 f9 70 c8 ee               	vpshufd	xmm1, xmm0, 0xee        # xmm1 = xmm0[2,3,2,3]
    1443: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
    1447: c4 e1 f9 7e c0               	vmovq	rax, xmm0
    144c: 48 39 d1                     	cmp	rcx, rdx
    144f: 74 2d                        	je	0x147e <ep14_gather_batch_unchecked+0x1ee>
    1451: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nop	word ptr cs:[rax + rax]
    1460: 44 8b 04 8e                  	mov	r8d, dword ptr [rsi + 4*rcx]
    1464: 49 c1 e0 05                  	shl	r8, 0x5
    1468: 46 8b 4c 07 10               	mov	r9d, dword ptr [rdi + r8 + 0x10]
    146d: 4e 0f af 4c 07 08            	imul	r9, qword ptr [rdi + r8 + 0x8]
    1473: 4c 01 c8                     	add	rax, r9
    1476: 48 ff c1                     	inc	rcx
    1479: 48 39 ca                     	cmp	rdx, rcx
    147c: 75 e2                        	jne	0x1460 <ep14_gather_batch_unchecked+0x1d0>
    147e: c5 f8 77                     	vzeroupper
    1481: c3                           	ret
    1482: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00    	nop	word ptr cs:[rax + rax]

# --- 2. CLANG 18: Hardened Fast-Trap Batch Gather (cmp + jae ud2, Scalar) ---

/tmp/ep14_build/challenge_clang_hardened:	file format elf64-x86-64

Disassembly of section .text:

00000000000014a0 <ep14_gather_batch_hardened_trap>:
    14a0: 48 8b 0e                     	mov	rcx, qword ptr [rsi]
    14a3: 48 8b 56 08                  	mov	rdx, qword ptr [rsi + 0x8]
    14a7: 48 29 ca                     	sub	rdx, rcx
    14aa: 74 48                        	je	0x14f4 <ep14_gather_batch_hardened_trap+0x54>
    14ac: 48 8b 37                     	mov	rsi, qword ptr [rdi]
    14af: 48 8b 7f 08                  	mov	rdi, qword ptr [rdi + 0x8]
    14b3: 48 29 f7                     	sub	rdi, rsi
    14b6: 48 c1 ff 05                  	sar	rdi, 0x5
    14ba: 48 c1 fa 02                  	sar	rdx, 0x2
    14be: 48 83 fa 01                  	cmp	rdx, 0x1
    14c2: 48 83 d2 00                  	adc	rdx, 0x0
    14c6: 31 c0                        	xor	eax, eax
    14c8: 45 31 c0                     	xor	r8d, r8d
    14cb: 0f 1f 44 00 00               	nop	dword ptr [rax + rax]
    14d0: 46 8b 0c 81                  	mov	r9d, dword ptr [rcx + 4*r8]
    14d4: 4c 39 cf                     	cmp	rdi, r9
    14d7: 76 1e                        	jbe	0x14f7 <ep14_gather_batch_hardened_trap+0x57>
    14d9: 49 c1 e1 05                  	shl	r9, 0x5
    14dd: 46 8b 54 0e 10               	mov	r10d, dword ptr [rsi + r9 + 0x10]
    14e2: 4e 0f af 54 0e 08            	imul	r10, qword ptr [rsi + r9 + 0x8]
    14e8: 4c 01 d0                     	add	rax, r10
    14eb: 49 ff c0                     	inc	r8
    14ee: 4c 39 c2                     	cmp	rdx, r8
    14f1: 75 dd                        	jne	0x14d0 <ep14_gather_batch_hardened_trap+0x30>
    14f3: c3                           	ret
    14f4: 31 c0                        	xor	eax, eax
    14f6: c3                           	ret
    14f7: 0f 0b                        	ud2
    14f9: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]

# --- 3. CLANG 18: Power-of-Two Bitmask-Safe Gather (idx & 0xFFFF, AVX-512) ---

/tmp/ep14_build/challenge_clang_hardened:	file format elf64-x86-64

Disassembly of section .text:

0000000000001500 <ep14_gather_batch_bitmask_safe>:
    1500: 48 85 d2                     	test	rdx, rdx
    1503: 74 10                        	je	0x1515 <ep14_gather_batch_bitmask_safe+0x15>
    1505: 48 83 fa 20                  	cmp	rdx, 0x20
    1509: 73 0d                        	jae	0x1518 <ep14_gather_batch_bitmask_safe+0x18>
    150b: 31 c0                        	xor	eax, eax
    150d: 45 31 c0                     	xor	r8d, r8d
    1510: e9 cb 01 00 00               	jmp	0x16e0 <ep14_gather_batch_bitmask_safe+0x1e0>
    1515: 31 c0                        	xor	eax, eax
    1517: c3                           	ret
    1518: 49 89 d0                     	mov	r8, rdx
    151b: 49 83 e0 e0                  	and	r8, -0x20
    151f: 62 f2 7d 28 7c c9            	vpbroadcastd	ymm1, ecx
    1525: c5 f9 ef c0                  	vpxor	xmm0, xmm0, xmm0
    1529: 31 c0                        	xor	eax, eax
    152b: 62 f2 fd 48 7c d7            	vpbroadcastq	zmm2, rdi
    1531: c5 e1 ef db                  	vpxor	xmm3, xmm3, xmm3
    1535: c5 d9 ef e4                  	vpxor	xmm4, xmm4, xmm4
    1539: c5 d1 ef ed                  	vpxor	xmm5, xmm5, xmm5
    153d: 0f 1f 00                     	nop	dword ptr [rax]
    1540: c5 f5 db 34 86               	vpand	ymm6, ymm1, ymmword ptr [rsi + 4*rax]
    1545: c5 f5 db 7c 86 20            	vpand	ymm7, ymm1, ymmword ptr [rsi + 4*rax + 0x20]
    154b: c5 75 db 44 86 40            	vpand	ymm8, ymm1, ymmword ptr [rsi + 4*rax + 0x40]
    1551: c5 75 db 4c 86 60            	vpand	ymm9, ymm1, ymmword ptr [rsi + 4*rax + 0x60]
    1557: 62 f2 7d 48 35 f6            	vpmovzxdq	zmm6, ymm6      # zmm6 = ymm6[0],zero,ymm6[1],zero,ymm6[2],zero,ymm6[3],zero,ymm6[4],zero,ymm6[5],zero,ymm6[6],zero,ymm6[7],zero
    155d: 62 f2 7d 48 35 ff            	vpmovzxdq	zmm7, ymm7      # zmm7 = ymm7[0],zero,ymm7[1],zero,ymm7[2],zero,ymm7[3],zero,ymm7[4],zero,ymm7[5],zero,ymm7[6],zero,ymm7[7],zero
    1563: 62 52 7d 48 35 c0            	vpmovzxdq	zmm8, ymm8      # zmm8 = ymm8[0],zero,ymm8[1],zero,ymm8[2],zero,ymm8[3],zero,ymm8[4],zero,ymm8[5],zero,ymm8[6],zero,ymm8[7],zero
    1569: 62 52 7d 48 35 c9            	vpmovzxdq	zmm9, ymm9      # zmm9 = ymm9[0],zero,ymm9[1],zero,ymm9[2],zero,ymm9[3],zero,ymm9[4],zero,ymm9[5],zero,ymm9[6],zero,ymm9[7],zero
    156f: 62 f1 cd 48 73 f6 05         	vpsllq	zmm6, zmm6, 0x5
    1576: 62 f1 ed 48 d4 f6            	vpaddq	zmm6, zmm2, zmm6
    157c: 62 f1 c5 48 73 f7 05         	vpsllq	zmm7, zmm7, 0x5
    1583: 62 f1 ed 48 d4 ff            	vpaddq	zmm7, zmm2, zmm7
    1589: 62 d1 bd 48 73 f0 05         	vpsllq	zmm8, zmm8, 0x5
    1590: 62 51 ed 48 d4 c0            	vpaddq	zmm8, zmm2, zmm8
    1596: 62 d1 b5 48 73 f1 05         	vpsllq	zmm9, zmm9, 0x5
    159d: 62 51 ed 48 d4 c9            	vpaddq	zmm9, zmm2, zmm9
    15a3: c4 41 29 ef d2               	vpxor	xmm10, xmm10, xmm10
    15a8: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    15ac: 62 72 fd 49 91 14 35 08 00 00 00     	vpgatherqq	zmm10 {k1}, zmmword ptr [1*zmm6 + 0x8]
    15b7: c4 41 21 ef db               	vpxor	xmm11, xmm11, xmm11
    15bc: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    15c0: 62 72 fd 49 91 1c 3d 08 00 00 00     	vpgatherqq	zmm11 {k1}, zmmword ptr [1*zmm7 + 0x8]
    15cb: c4 41 19 ef e4               	vpxor	xmm12, xmm12, xmm12
    15d0: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    15d4: 62 32 fd 49 91 24 05 08 00 00 00     	vpgatherqq	zmm12 {k1}, zmmword ptr [1*zmm8 + 0x8]
    15df: c4 41 11 ef ed               	vpxor	xmm13, xmm13, xmm13
    15e4: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    15e8: 62 32 fd 49 91 2c 0d 08 00 00 00     	vpgatherqq	zmm13 {k1}, zmmword ptr [1*zmm9 + 0x8]
    15f3: c4 41 09 ef f6               	vpxor	xmm14, xmm14, xmm14
    15f8: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    15fc: 62 72 7d 49 91 34 35 10 00 00 00     	vpgatherqd	ymm14 {k1}, ymmword ptr [1*zmm6 + 0x10]
    1607: c5 c9 ef f6                  	vpxor	xmm6, xmm6, xmm6
    160b: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    160f: 62 f2 7d 49 91 34 3d 10 00 00 00     	vpgatherqd	ymm6 {k1}, ymmword ptr [1*zmm7 + 0x10]
    161a: c5 c1 ef ff                  	vpxor	xmm7, xmm7, xmm7
    161e: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    1622: 62 b2 7d 49 91 3c 05 10 00 00 00     	vpgatherqd	ymm7 {k1}, ymmword ptr [1*zmm8 + 0x10]
    162d: c4 41 39 ef c0               	vpxor	xmm8, xmm8, xmm8
    1632: c5 fc 46 c8                  	kxnorw	k1, k0, k0
    1636: 62 32 7d 49 91 04 0d 10 00 00 00     	vpgatherqd	ymm8 {k1}, ymmword ptr [1*zmm9 + 0x10]
    1641: 62 52 7d 48 35 ce            	vpmovzxdq	zmm9, ymm14     # zmm9 = ymm14[0],zero,ymm14[1],zero,ymm14[2],zero,ymm14[3],zero,ymm14[4],zero,ymm14[5],zero,ymm14[6],zero,ymm14[7],zero
    1647: 62 52 ad 48 40 c9            	vpmullq	zmm9, zmm10, zmm9
    164d: 62 f1 b5 48 d4 c0            	vpaddq	zmm0, zmm9, zmm0
    1653: 62 f2 7d 48 35 f6            	vpmovzxdq	zmm6, ymm6      # zmm6 = ymm6[0],zero,ymm6[1],zero,ymm6[2],zero,ymm6[3],zero,ymm6[4],zero,ymm6[5],zero,ymm6[6],zero,ymm6[7],zero
    1659: 62 f2 a5 48 40 f6            	vpmullq	zmm6, zmm11, zmm6
    165f: 62 f1 cd 48 d4 db            	vpaddq	zmm3, zmm6, zmm3
    1665: 62 f2 7d 48 35 f7            	vpmovzxdq	zmm6, ymm7      # zmm6 = ymm7[0],zero,ymm7[1],zero,ymm7[2],zero,ymm7[3],zero,ymm7[4],zero,ymm7[5],zero,ymm7[6],zero,ymm7[7],zero
    166b: 62 f2 9d 48 40 f6            	vpmullq	zmm6, zmm12, zmm6
    1671: 62 f1 cd 48 d4 e4            	vpaddq	zmm4, zmm6, zmm4
    1677: 62 d2 7d 48 35 f0            	vpmovzxdq	zmm6, ymm8      # zmm6 = ymm8[0],zero,ymm8[1],zero,ymm8[2],zero,ymm8[3],zero,ymm8[4],zero,ymm8[5],zero,ymm8[6],zero,ymm8[7],zero
    167d: 62 f2 95 48 40 f6            	vpmullq	zmm6, zmm13, zmm6
    1683: 62 f1 cd 48 d4 ed            	vpaddq	zmm5, zmm6, zmm5
    1689: 48 83 c0 20                  	add	rax, 0x20
    168d: 49 39 c0                     	cmp	r8, rax
    1690: 0f 85 aa fe ff ff            	jne	0x1540 <ep14_gather_batch_bitmask_safe+0x40>
    1696: 62 f1 e5 48 d4 c0            	vpaddq	zmm0, zmm3, zmm0
    169c: 62 f1 dd 48 d4 c0            	vpaddq	zmm0, zmm4, zmm0
    16a2: 62 f1 d5 48 d4 c0            	vpaddq	zmm0, zmm5, zmm0
    16a8: 62 f3 fd 48 3b c1 01         	vextracti64x4	ymm1, zmm0, 0x1
    16af: 62 f1 fd 48 d4 c1            	vpaddq	zmm0, zmm0, zmm1
    16b5: c4 e3 7d 39 c1 01            	vextracti128	xmm1, ymm0, 0x1
    16bb: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
    16bf: c5 f9 70 c8 ee               	vpshufd	xmm1, xmm0, 0xee        # xmm1 = xmm0[2,3,2,3]
    16c4: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
    16c8: c4 e1 f9 7e c0               	vmovq	rax, xmm0
    16cd: 49 39 d0                     	cmp	r8, rdx
    16d0: 74 2f                        	je	0x1701 <ep14_gather_batch_bitmask_safe+0x201>
    16d2: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00    	nop	word ptr cs:[rax + rax]
    16e0: 46 8b 0c 86                  	mov	r9d, dword ptr [rsi + 4*r8]
    16e4: 41 21 c9                     	and	r9d, ecx
    16e7: 49 c1 e1 05                  	shl	r9, 0x5
    16eb: 46 8b 54 0f 10               	mov	r10d, dword ptr [rdi + r9 + 0x10]
    16f0: 4e 0f af 54 0f 08            	imul	r10, qword ptr [rdi + r9 + 0x8]
    16f6: 4c 01 d0                     	add	rax, r10
    16f9: 49 ff c0                     	inc	r8
    16fc: 4c 39 c2                     	cmp	rdx, r8
    16ff: 75 df                        	jne	0x16e0 <ep14_gather_batch_bitmask_safe+0x1e0>
    1701: c5 f8 77                     	vzeroupper
    1704: c3                           	ret
    1705: 66 66 2e 0f 1f 84 00 00 00 00 00     	nop	word ptr cs:[rax + rax]

# --- 4. CLANG 18: Dependent Probe Chain (Unchecked vs. Hardened Trap) ---

/tmp/ep14_build/challenge_clang_unchecked:	file format elf64-x86-64

Disassembly of section .text:

0000000000001700 <ep14_probe_dependent_unchecked>:
    1700: 45 31 c0                     	xor	r8d, r8d
    1703: 48 85 d2                     	test	rdx, rdx
    1706: 74 0d                        	je	0x1715 <ep14_probe_dependent_unchecked+0x15>
    1708: 48 83 fa 01                  	cmp	rdx, 0x1
    170c: 75 0a                        	jne	0x1718 <ep14_probe_dependent_unchecked+0x18>
    170e: 31 c0                        	xor	eax, eax
    1710: 45 31 c9                     	xor	r9d, r9d
    1713: eb 5f                        	jmp	0x1774 <ep14_probe_dependent_unchecked+0x74>
    1715: 31 c0                        	xor	eax, eax
    1717: c3                           	ret
    1718: 49 89 d2                     	mov	r10, rdx
    171b: 49 83 e2 fe                  	and	r10, -0x2
    171f: 31 c0                        	xor	eax, eax
    1721: 45 31 c0                     	xor	r8d, r8d
    1724: 45 31 c9                     	xor	r9d, r9d
    1727: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]
    1730: 46 33 04 8e                  	xor	r8d, dword ptr [rsi + 4*r9]
    1734: 41 21 c8                     	and	r8d, ecx
    1737: 49 c1 e0 05                  	shl	r8, 0x5
    173b: 46 8b 5c 07 10               	mov	r11d, dword ptr [rdi + r8 + 0x10]
    1740: 4e 0f af 5c 07 08            	imul	r11, qword ptr [rdi + r8 + 0x8]
    1746: 4c 01 d8                     	add	rax, r11
    1749: 45 0f b6 c3                  	movzx	r8d, r11b
    174d: 46 33 44 8e 04               	xor	r8d, dword ptr [rsi + 4*r9 + 0x4]
    1752: 41 21 c8                     	and	r8d, ecx
    1755: 49 c1 e0 05                  	shl	r8, 0x5
    1759: 46 8b 5c 07 10               	mov	r11d, dword ptr [rdi + r8 + 0x10]
    175e: 4e 0f af 5c 07 08            	imul	r11, qword ptr [rdi + r8 + 0x8]
    1764: 4c 01 d8                     	add	rax, r11
    1767: 45 0f b6 c3                  	movzx	r8d, r11b
    176b: 49 83 c1 02                  	add	r9, 0x2
    176f: 4d 39 ca                     	cmp	r10, r9
    1772: 75 bc                        	jne	0x1730 <ep14_probe_dependent_unchecked+0x30>
    1774: f6 c2 01                     	test	dl, 0x1
    1777: 74 19                        	je	0x1792 <ep14_probe_dependent_unchecked+0x92>
    1779: 46 33 04 8e                  	xor	r8d, dword ptr [rsi + 4*r9]
    177d: 41 21 c8                     	and	r8d, ecx
    1780: 49 c1 e0 05                  	shl	r8, 0x5
    1784: 42 8b 4c 07 10               	mov	ecx, dword ptr [rdi + r8 + 0x10]
    1789: 4a 0f af 4c 07 08            	imul	rcx, qword ptr [rdi + r8 + 0x8]
    178f: 48 01 c8                     	add	rax, rcx
    1792: c3                           	ret
    1793: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00       	nop	word ptr cs:[rax + rax]

/tmp/ep14_build/challenge_clang_hardened:	file format elf64-x86-64

Disassembly of section .text:

00000000000017b0 <ep14_probe_dependent_hardened_trap>:
    17b0: 48 8b 0e                     	mov	rcx, qword ptr [rsi]
    17b3: 48 8b 76 08                  	mov	rsi, qword ptr [rsi + 0x8]
    17b7: 45 31 c9                     	xor	r9d, r9d
    17ba: 48 29 ce                     	sub	rsi, rcx
    17bd: 74 4c                        	je	0x180b <ep14_probe_dependent_hardened_trap+0x5b>
    17bf: 4c 8b 07                     	mov	r8, qword ptr [rdi]
    17c2: 48 8b 7f 08                  	mov	rdi, qword ptr [rdi + 0x8]
    17c6: 4c 29 c7                     	sub	rdi, r8
    17c9: 48 c1 ff 05                  	sar	rdi, 0x5
    17cd: 48 c1 fe 02                  	sar	rsi, 0x2
    17d1: 48 83 fe 01                  	cmp	rsi, 0x1
    17d5: 48 83 d6 00                  	adc	rsi, 0x0
    17d9: 31 c0                        	xor	eax, eax
    17db: 45 31 d2                     	xor	r10d, r10d
    17de: 66 90                        	nop
    17e0: 46 33 0c 91                  	xor	r9d, dword ptr [rcx + 4*r10]
    17e4: 41 21 d1                     	and	r9d, edx
    17e7: 4c 39 cf                     	cmp	rdi, r9
    17ea: 76 22                        	jbe	0x180e <ep14_probe_dependent_hardened_trap+0x5e>
    17ec: 49 c1 e1 05                  	shl	r9, 0x5
    17f0: 47 8b 5c 08 10               	mov	r11d, dword ptr [r8 + r9 + 0x10]
    17f5: 4f 0f af 5c 08 08            	imul	r11, qword ptr [r8 + r9 + 0x8]
    17fb: 4c 01 d8                     	add	rax, r11
    17fe: 45 0f b6 cb                  	movzx	r9d, r11b
    1802: 49 ff c2                     	inc	r10
    1805: 4c 39 d6                     	cmp	rsi, r10
    1808: 75 d6                        	jne	0x17e0 <ep14_probe_dependent_hardened_trap+0x30>
    180a: c3                           	ret
    180b: 31 c0                        	xor	eax, eax
    180d: c3                           	ret
    180e: 0f 0b                        	ud2

# --- 5. CLANG 18: 64-Bit Generational Slab Handle Lookup ---

/tmp/ep14_build/challenge_clang_hardened:	file format elf64-x86-64

Disassembly of section .text:

0000000000001810 <ep14_temporal_generational_lookup>:
    1810: 53                           	push	rbx
    1811: 48 8b 0a                     	mov	rcx, qword ptr [rdx]
    1814: 48 8b 52 08                  	mov	rdx, qword ptr [rdx + 0x8]
    1818: 48 29 ca                     	sub	rdx, rcx
    181b: 74 77                        	je	0x1894 <ep14_temporal_generational_lookup+0x84>
    181d: 48 c1 fa 02                  	sar	rdx, 0x2
    1821: 4c 8b 06                     	mov	r8, qword ptr [rsi]
    1824: 48 8b 76 08                  	mov	rsi, qword ptr [rsi + 0x8]
    1828: 4c 29 c6                     	sub	rsi, r8
    182b: 48 c1 fe 03                  	sar	rsi, 0x3
    182f: 4c 8b 0f                     	mov	r9, qword ptr [rdi]
    1832: 48 8b 7f 08                  	mov	rdi, qword ptr [rdi + 0x8]
    1836: 4c 29 cf                     	sub	rdi, r9
    1839: 48 c1 ff 06                  	sar	rdi, 0x6
    183d: 48 83 fa 01                  	cmp	rdx, 0x1
    1841: 48 83 d2 00                  	adc	rdx, 0x0
    1845: 31 c0                        	xor	eax, eax
    1847: 45 31 d2                     	xor	r10d, r10d
    184a: 66 0f 1f 44 00 00            	nop	word ptr [rax + rax]
    1850: 46 8b 1c 91                  	mov	r11d, dword ptr [rcx + 4*r10]
    1854: 4c 39 de                     	cmp	rsi, r11
    1857: 76 3f                        	jbe	0x1898 <ep14_temporal_generational_lookup+0x88>
    1859: 4b 8b 1c d8                  	mov	rbx, qword ptr [r8 + 8*r11]
    185d: 41 89 db                     	mov	r11d, ebx
    1860: 49 39 fb                     	cmp	r11, rdi
    1863: 73 25                        	jae	0x188a <ep14_temporal_generational_lookup+0x7a>
    1865: 48 c1 eb 20                  	shr	rbx, 0x20
    1869: 49 c1 e3 06                  	shl	r11, 0x6
    186d: 43 39 5c 19 20               	cmp	dword ptr [r9 + r11 + 0x20], ebx
    1872: 75 16                        	jne	0x188a <ep14_temporal_generational_lookup+0x7a>
    1874: 43 80 7c 19 28 00            	cmp	byte ptr [r9 + r11 + 0x28], 0x0
    187a: 74 0e                        	je	0x188a <ep14_temporal_generational_lookup+0x7a>
    187c: 43 8b 5c 19 10               	mov	ebx, dword ptr [r9 + r11 + 0x10]
    1881: 4b 0f af 5c 19 08            	imul	rbx, qword ptr [r9 + r11 + 0x8]
    1887: 48 01 d8                     	add	rax, rbx
    188a: 49 ff c2                     	inc	r10
    188d: 4c 39 d2                     	cmp	rdx, r10
    1890: 75 be                        	jne	0x1850 <ep14_temporal_generational_lookup+0x40>
    1892: 5b                           	pop	rbx
    1893: c3                           	ret
    1894: 31 c0                        	xor	eax, eax
    1896: 5b                           	pop	rbx
    1897: c3                           	ret
    1898: 48 8d 3d 4f 48 00 00         	lea	rdi, [rip + 0x484f]     # 0x60ee <_IO_stdin_used+0xee>
    189f: 48 8d 15 33 49 00 00         	lea	rdx, [rip + 0x4933]     # 0x61d9 <_IO_stdin_used+0x1d9>
    18a6: 48 8d 0d 19 49 00 00         	lea	rcx, [rip + 0x4919]     # 0x61c6 <_IO_stdin_used+0x1c6>
    18ad: be 7b 04 00 00               	mov	esi, 0x47b
    18b2: e8 99 f7 ff ff               	call	0x1050 <_ZSt21__glibcxx_assert_failPKciS0_S0_@plt>
    18b7: 66 0f 1f 84 00 00 00 00 00   	nop	word ptr [rax + rax]

# --- 6. RUST 1.91: Checked vs. Unchecked vs. Bitmask-Safe Gather ---

/tmp/ep14_build/bench_rust:	file format elf64-x86-64

Disassembly of section .text:

000000000001b4b0 <rust_gather_batch_unchecked>:
   1b4b0: 48 85 c9                     	test	rcx, rcx
   1b4b3: 74 1c                        	je	0x1b4d1 <rust_gather_batch_unchecked+0x21>
   1b4b5: 48 8d 04 8d 00 00 00 00      	lea	rax, [4*rcx]
   1b4bd: 48 83 c0 fc                  	add	rax, -0x4
   1b4c1: 48 83 f8 1c                  	cmp	rax, 0x1c
   1b4c5: 73 0d                        	jae	0x1b4d4 <rust_gather_batch_unchecked+0x24>
   1b4c7: 31 c0                        	xor	eax, eax
   1b4c9: 49 89 d2                     	mov	r10, rdx
   1b4cc: e9 2a 02 00 00               	jmp	0x1b6fb <rust_gather_batch_unchecked+0x24b>
   1b4d1: 31 c0                        	xor	eax, eax
   1b4d3: c3                           	ret
   1b4d4: 48 89 c6                     	mov	rsi, rax
   1b4d7: 48 c1 ee 02                  	shr	rsi, 0x2
   1b4db: 49 b8 e0 ff ff ff ff ff ff 7f	movabs	r8, 0x7fffffffffffffe0
   1b4e5: 48 ff c6                     	inc	rsi
   1b4e8: 48 83 f8 7c                  	cmp	rax, 0x7c
   1b4ec: 73 0a                        	jae	0x1b4f8 <rust_gather_batch_unchecked+0x48>
   1b4ee: 45 31 c9                     	xor	r9d, r9d
   1b4f1: 31 c0                        	xor	eax, eax
   1b4f3: e9 77 01 00 00               	jmp	0x1b66f <rust_gather_batch_unchecked+0x1bf>
   1b4f8: 49 89 f1                     	mov	r9, rsi
   1b4fb: 4d 21 c1                     	and	r9, r8
   1b4fe: c5 f9 ef c0                  	vpxor	xmm0, xmm0, xmm0
   1b502: 31 c0                        	xor	eax, eax
   1b504: c5 f1 ef c9                  	vpxor	xmm1, xmm1, xmm1
   1b508: c5 e9 ef d2                  	vpxor	xmm2, xmm2, xmm2
   1b50c: c5 e1 ef db                  	vpxor	xmm3, xmm3, xmm3
   1b510: 62 f2 7d 48 35 24 82         	vpmovzxdq	zmm4, ymmword ptr [rdx + 4*rax] # zmm4 = mem[0],zero,mem[1],zero,mem[2],zero,mem[3],zero,mem[4],zero,mem[5],zero,mem[6],zero,mem[7],zero
   1b517: 62 f2 7d 48 35 6c 82 01      	vpmovzxdq	zmm5, ymmword ptr [rdx + 4*rax + 0x20] # zmm5 = mem[0],zero,mem[1],zero,mem[2],zero,mem[3],zero,mem[4],zero,mem[5],zero,mem[6],zero,mem[7],zero
   1b51f: 62 f2 7d 48 35 74 82 02      	vpmovzxdq	zmm6, ymmword ptr [rdx + 4*rax + 0x40] # zmm6 = mem[0],zero,mem[1],zero,mem[2],zero,mem[3],zero,mem[4],zero,mem[5],zero,mem[6],zero,mem[7],zero
   1b527: 62 f2 7d 48 35 7c 82 03      	vpmovzxdq	zmm7, ymmword ptr [rdx + 4*rax + 0x60] # zmm7 = mem[0],zero,mem[1],zero,mem[2],zero,mem[3],zero,mem[4],zero,mem[5],zero,mem[6],zero,mem[7],zero
   1b52f: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b533: c4 41 39 ef c0               	vpxor	xmm8, xmm8, xmm8
   1b538: c4 41 31 ef c9               	vpxor	xmm9, xmm9, xmm9
   1b53d: c4 41 29 ef d2               	vpxor	xmm10, xmm10, xmm10
   1b542: c4 41 21 ef db               	vpxor	xmm11, xmm11, xmm11
   1b547: c4 41 19 ef e4               	vpxor	xmm12, xmm12, xmm12
   1b54c: 48 83 c0 20                  	add	rax, 0x20
   1b550: 62 f1 dd 48 73 f4 02         	vpsllq	zmm4, zmm4, 0x2
   1b557: 62 f1 d5 48 73 f5 02         	vpsllq	zmm5, zmm5, 0x2
   1b55e: 62 f1 cd 48 73 f6 02         	vpsllq	zmm6, zmm6, 0x2
   1b565: 62 f1 c5 48 73 f7 02         	vpsllq	zmm7, zmm7, 0x2
   1b56c: 62 72 fd 49 91 44 e7 01      	vpgatherqq	zmm8 {k1}, zmmword ptr [rdi + 8*zmm4 + 0x8]
   1b574: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b578: 62 72 fd 49 91 4c ef 01      	vpgatherqq	zmm9 {k1}, zmmword ptr [rdi + 8*zmm5 + 0x8]
   1b580: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b584: 62 72 fd 49 91 54 f7 01      	vpgatherqq	zmm10 {k1}, zmmword ptr [rdi + 8*zmm6 + 0x8]
   1b58c: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b590: 62 72 fd 49 91 5c ff 01      	vpgatherqq	zmm11 {k1}, zmmword ptr [rdi + 8*zmm7 + 0x8]
   1b598: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b59c: 62 72 7d 49 91 64 e7 04      	vpgatherqd	ymm12 {k1}, ymmword ptr [rdi + 8*zmm4 + 0x10]
   1b5a4: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b5a8: c5 d9 ef e4                  	vpxor	xmm4, xmm4, xmm4
   1b5ac: 62 f2 7d 49 91 64 ef 04      	vpgatherqd	ymm4 {k1}, ymmword ptr [rdi + 8*zmm5 + 0x10]
   1b5b4: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b5b8: c5 d1 ef ed                  	vpxor	xmm5, xmm5, xmm5
   1b5bc: 62 f2 7d 49 91 6c f7 04      	vpgatherqd	ymm5 {k1}, ymmword ptr [rdi + 8*zmm6 + 0x10]
   1b5c4: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b5c8: c5 c9 ef f6                  	vpxor	xmm6, xmm6, xmm6
   1b5cc: 62 f2 7d 49 91 74 ff 04      	vpgatherqd	ymm6 {k1}, ymmword ptr [rdi + 8*zmm7 + 0x10]
   1b5d4: 62 d2 7d 48 35 fc            	vpmovzxdq	zmm7, ymm12     # zmm7 = ymm12[0],zero,ymm12[1],zero,ymm12[2],zero,ymm12[3],zero,ymm12[4],zero,ymm12[5],zero,ymm12[6],zero,ymm12[7],zero
   1b5da: 62 f2 7d 48 35 e4            	vpmovzxdq	zmm4, ymm4      # zmm4 = ymm4[0],zero,ymm4[1],zero,ymm4[2],zero,ymm4[3],zero,ymm4[4],zero,ymm4[5],zero,ymm4[6],zero,ymm4[7],zero
   1b5e0: 62 f2 bd 48 40 ff            	vpmullq	zmm7, zmm8, zmm7
   1b5e6: 62 f2 b5 48 40 e4            	vpmullq	zmm4, zmm9, zmm4
   1b5ec: 62 f1 c5 48 d4 c0            	vpaddq	zmm0, zmm7, zmm0
   1b5f2: 62 f2 7d 48 35 fd            	vpmovzxdq	zmm7, ymm5      # zmm7 = ymm5[0],zero,ymm5[1],zero,ymm5[2],zero,ymm5[3],zero,ymm5[4],zero,ymm5[5],zero,ymm5[6],zero,ymm5[7],zero
   1b5f8: 62 f2 7d 48 35 ee            	vpmovzxdq	zmm5, ymm6      # zmm5 = ymm6[0],zero,ymm6[1],zero,ymm6[2],zero,ymm6[3],zero,ymm6[4],zero,ymm6[5],zero,ymm6[6],zero,ymm6[7],zero
   1b5fe: 62 f1 dd 48 d4 c9            	vpaddq	zmm1, zmm4, zmm1
   1b604: 62 f2 ad 48 40 e7            	vpmullq	zmm4, zmm10, zmm7
   1b60a: 62 f1 dd 48 d4 d2            	vpaddq	zmm2, zmm4, zmm2
   1b610: 62 f2 a5 48 40 e5            	vpmullq	zmm4, zmm11, zmm5
   1b616: 62 f1 dd 48 d4 db            	vpaddq	zmm3, zmm4, zmm3
   1b61c: 49 39 c1                     	cmp	r9, rax
   1b61f: 0f 85 eb fe ff ff            	jne	0x1b510 <rust_gather_batch_unchecked+0x60>
   1b625: 62 f1 f5 48 d4 c0            	vpaddq	zmm0, zmm1, zmm0
   1b62b: 62 f1 ed 48 d4 c0            	vpaddq	zmm0, zmm2, zmm0
   1b631: 62 f1 e5 48 d4 c0            	vpaddq	zmm0, zmm3, zmm0
   1b637: 62 f3 fd 48 3b c1 01         	vextracti64x4	ymm1, zmm0, 0x1
   1b63e: 62 f1 fd 48 d4 c1            	vpaddq	zmm0, zmm0, zmm1
   1b644: c4 e3 7d 39 c1 01            	vextracti128	xmm1, ymm0, 0x1
   1b64a: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
   1b64e: c5 f9 70 c8 ee               	vpshufd	xmm1, xmm0, 0xee        # xmm1 = xmm0[2,3,2,3]
   1b653: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
   1b657: c4 e1 f9 7e c0               	vmovq	rax, xmm0
   1b65c: 4c 39 ce                     	cmp	rsi, r9
   1b65f: 0f 84 8e 00 00 00            	je	0x1b6f3 <rust_gather_batch_unchecked+0x243>
   1b665: 40 f6 c6 18                  	test	sil, 0x18
   1b669: 0f 84 88 00 00 00            	je	0x1b6f7 <rust_gather_batch_unchecked+0x247>
   1b66f: 49 83 c0 18                  	add	r8, 0x18
   1b673: c4 e1 f9 6e c0               	vmovq	xmm0, rax
   1b678: 49 21 f0                     	and	r8, rsi
   1b67b: 4e 8d 14 82                  	lea	r10, [rdx + 4*r8]
   1b67f: 90                           	nop
   1b680: 62 b2 7d 48 35 0c 8a         	vpmovzxdq	zmm1, ymmword ptr [rdx + 4*r9] # zmm1 = mem[0],zero,mem[1],zero,mem[2],zero,mem[3],zero,mem[4],zero,mem[5],zero,mem[6],zero,mem[7],zero
   1b687: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b68b: c5 e9 ef d2                  	vpxor	xmm2, xmm2, xmm2
   1b68f: c5 e1 ef db                  	vpxor	xmm3, xmm3, xmm3
   1b693: 49 83 c1 08                  	add	r9, 0x8
   1b697: 62 f1 f5 48 73 f1 02         	vpsllq	zmm1, zmm1, 0x2
   1b69e: 62 f2 fd 49 91 54 cf 01      	vpgatherqq	zmm2 {k1}, zmmword ptr [rdi + 8*zmm1 + 0x8]
   1b6a6: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b6aa: 62 f2 7d 49 91 5c cf 04      	vpgatherqd	ymm3 {k1}, ymmword ptr [rdi + 8*zmm1 + 0x10]
   1b6b2: 62 f2 7d 48 35 cb            	vpmovzxdq	zmm1, ymm3      # zmm1 = ymm3[0],zero,ymm3[1],zero,ymm3[2],zero,ymm3[3],zero,ymm3[4],zero,ymm3[5],zero,ymm3[6],zero,ymm3[7],zero
   1b6b8: 62 f2 ed 48 40 c9            	vpmullq	zmm1, zmm2, zmm1
   1b6be: 62 f1 f5 48 d4 c0            	vpaddq	zmm0, zmm1, zmm0
   1b6c4: 4d 39 c8                     	cmp	r8, r9
   1b6c7: 75 b7                        	jne	0x1b680 <rust_gather_batch_unchecked+0x1d0>
   1b6c9: 62 f3 fd 48 3b c1 01         	vextracti64x4	ymm1, zmm0, 0x1
   1b6d0: 62 f1 fd 48 d4 c1            	vpaddq	zmm0, zmm0, zmm1
   1b6d6: c4 e3 7d 39 c1 01            	vextracti128	xmm1, ymm0, 0x1
   1b6dc: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
   1b6e0: c5 f9 70 c8 ee               	vpshufd	xmm1, xmm0, 0xee        # xmm1 = xmm0[2,3,2,3]
   1b6e5: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
   1b6e9: c4 e1 f9 7e c0               	vmovq	rax, xmm0
   1b6ee: 4c 39 c6                     	cmp	rsi, r8
   1b6f1: 75 08                        	jne	0x1b6fb <rust_gather_batch_unchecked+0x24b>
   1b6f3: c5 f8 77                     	vzeroupper
   1b6f6: c3                           	ret
   1b6f7: 4e 8d 14 8a                  	lea	r10, [rdx + 4*r9]
   1b6fb: 48 8d 0c 8a                  	lea	rcx, [rdx + 4*rcx]
   1b6ff: 90                           	nop
   1b700: 41 8b 12                     	mov	edx, dword ptr [r10]
   1b703: 49 83 c2 04                  	add	r10, 0x4
   1b707: 48 c1 e2 05                  	shl	rdx, 0x5
   1b70b: 8b 74 17 10                  	mov	esi, dword ptr [rdi + rdx + 0x10]
   1b70f: 48 0f af 74 17 08            	imul	rsi, qword ptr [rdi + rdx + 0x8]
   1b715: 48 01 f0                     	add	rax, rsi
   1b718: 49 39 ca                     	cmp	r10, rcx
   1b71b: 75 e3                        	jne	0x1b700 <rust_gather_batch_unchecked+0x250>
   1b71d: eb d4                        	jmp	0x1b6f3 <rust_gather_batch_unchecked+0x243>
   1b71f: cc                           	int3

/tmp/ep14_build/bench_rust:	file format elf64-x86-64

Disassembly of section .text:

000000000001b180 <rust_gather_batch_checked>:
   1b180: 50                           	push	rax
   1b181: 48 85 c9                     	test	rcx, rcx
   1b184: 74 30                        	je	0x1b1b6 <rust_gather_batch_checked+0x36>
   1b186: 48 c1 e1 02                  	shl	rcx, 0x2
   1b18a: 45 31 c9                     	xor	r9d, r9d
   1b18d: 31 c0                        	xor	eax, eax
   1b18f: 90                           	nop
   1b190: 46 8b 04 0a                  	mov	r8d, dword ptr [rdx + r9]
   1b194: 4c 39 c6                     	cmp	rsi, r8
   1b197: 76 21                        	jbe	0x1b1ba <rust_gather_batch_checked+0x3a>
   1b199: 49 c1 e0 05                  	shl	r8, 0x5
   1b19d: 49 83 c1 04                  	add	r9, 0x4
   1b1a1: 46 8b 54 07 10               	mov	r10d, dword ptr [rdi + r8 + 0x10]
   1b1a6: 4e 0f af 54 07 08            	imul	r10, qword ptr [rdi + r8 + 0x8]
   1b1ac: 4c 01 d0                     	add	rax, r10
   1b1af: 4c 39 c9                     	cmp	rcx, r9
   1b1b2: 75 dc                        	jne	0x1b190 <rust_gather_batch_checked+0x10>
   1b1b4: 59                           	pop	rcx
   1b1b5: c3                           	ret
   1b1b6: 31 c0                        	xor	eax, eax
   1b1b8: 59                           	pop	rcx
   1b1b9: c3                           	ret
   1b1ba: 48 8d 15 6f 83 04 00         	lea	rdx, [rip + 0x4836f]    # 0x63530 <writev+0x63530>
   1b1c1: 4c 89 c7                     	mov	rdi, r8
   1b1c4: ff 15 e6 b4 04 00            	call	qword ptr [rip + 0x4b4e6] # 0x666b0 <writev+0x666b0>
   1b1ca: 0f 0b                        	ud2
   1b1cc: ff 15 76 b5 04 00            	call	qword ptr [rip + 0x4b576] # 0x66748 <writev+0x66748>
   1b1d2: cc                           	int3
   1b1d3: cc                           	int3
   1b1d4: cc                           	int3
   1b1d5: cc                           	int3
   1b1d6: cc                           	int3
   1b1d7: cc                           	int3
   1b1d8: cc                           	int3
   1b1d9: cc                           	int3
   1b1da: cc                           	int3
   1b1db: cc                           	int3
   1b1dc: cc                           	int3
   1b1dd: cc                           	int3
   1b1de: cc                           	int3
   1b1df: cc                           	int3

/tmp/ep14_build/bench_rust:	file format elf64-x86-64

Disassembly of section .text:

000000000001ae90 <rust_gather_batch_bitmask_safe>:
   1ae90: 50                           	push	rax
   1ae91: 48 81 fe ff ff 00 00         	cmp	rsi, 0xffff
   1ae98: 0f 86 b4 02 00 00            	jbe	0x1b152 <rust_gather_batch_bitmask_safe+0x2c2>
   1ae9e: 48 85 c9                     	test	rcx, rcx
   1aea1: 74 1c                        	je	0x1aebf <rust_gather_batch_bitmask_safe+0x2f>
   1aea3: 48 8d 04 8d 00 00 00 00      	lea	rax, [4*rcx]
   1aeab: 48 83 c0 fc                  	add	rax, -0x4
   1aeaf: 48 83 f8 1b                  	cmp	rax, 0x1b
   1aeb3: 77 0e                        	ja	0x1aec3 <rust_gather_batch_bitmask_safe+0x33>
   1aeb5: 31 c0                        	xor	eax, eax
   1aeb7: 49 89 d2                     	mov	r10, rdx
   1aeba: e9 60 02 00 00               	jmp	0x1b11f <rust_gather_batch_bitmask_safe+0x28f>
   1aebf: 31 c0                        	xor	eax, eax
   1aec1: 59                           	pop	rcx
   1aec2: c3                           	ret
   1aec3: 48 89 c6                     	mov	rsi, rax
   1aec6: 48 c1 ee 02                  	shr	rsi, 0x2
   1aeca: 49 b8 e0 ff ff ff ff ff ff 7f	movabs	r8, 0x7fffffffffffffe0
   1aed4: 48 ff c6                     	inc	rsi
   1aed7: 48 83 f8 7c                  	cmp	rax, 0x7c
   1aedb: 73 0a                        	jae	0x1aee7 <rust_gather_batch_bitmask_safe+0x57>
   1aedd: 45 31 c9                     	xor	r9d, r9d
   1aee0: 31 c0                        	xor	eax, eax
   1aee2: e9 a0 01 00 00               	jmp	0x1b087 <rust_gather_batch_bitmask_safe+0x1f7>
   1aee7: 49 89 f1                     	mov	r9, rsi
   1aeea: 4d 21 c1                     	and	r9, r8
   1aeed: c5 f9 ef c0                  	vpxor	xmm0, xmm0, xmm0
   1aef1: 31 c0                        	xor	eax, eax
   1aef3: c5 f1 ef c9                  	vpxor	xmm1, xmm1, xmm1
   1aef7: c5 e9 ef d2                  	vpxor	xmm2, xmm2, xmm2
   1aefb: c5 e1 ef db                  	vpxor	xmm3, xmm3, xmm3
   1aeff: c5 d9 ef e4                  	vpxor	xmm4, xmm4, xmm4
   1af03: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00       	nop	word ptr cs:[rax + rax]
   1af10: c4 e3 75 0e 2c 82 55         	vpblendw	ymm5, ymm1, ymmword ptr [rdx + 4*rax], 0x55 # ymm5 = mem[0],ymm1[1],mem[2],ymm1[3],mem[4],ymm1[5],mem[6],ymm1[7],mem[8],ymm1[9],mem[10],ymm1[11],mem[12],ymm1[13],mem[14],ymm1[15]
   1af17: c4 e3 75 0e 74 82 20 55      	vpblendw	ymm6, ymm1, ymmword ptr [rdx + 4*rax + 0x20], 0x55 # ymm6 = mem[0],ymm1[1],mem[2],ymm1[3],mem[4],ymm1[5],mem[6],ymm1[7],mem[8],ymm1[9],mem[10],ymm1[11],mem[12],ymm1[13],mem[14],ymm1[15]
   1af1f: c4 e3 75 0e 7c 82 40 55      	vpblendw	ymm7, ymm1, ymmword ptr [rdx + 4*rax + 0x40], 0x55 # ymm7 = mem[0],ymm1[1],mem[2],ymm1[3],mem[4],ymm1[5],mem[6],ymm1[7],mem[8],ymm1[9],mem[10],ymm1[11],mem[12],ymm1[13],mem[14],ymm1[15]
   1af27: c4 63 75 0e 44 82 60 55      	vpblendw	ymm8, ymm1, ymmword ptr [rdx + 4*rax + 0x60], 0x55 # ymm8 = mem[0],ymm1[1],mem[2],ymm1[3],mem[4],ymm1[5],mem[6],ymm1[7],mem[8],ymm1[9],mem[10],ymm1[11],mem[12],ymm1[13],mem[14],ymm1[15]
   1af2f: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1af33: c4 41 31 ef c9               	vpxor	xmm9, xmm9, xmm9
   1af38: c4 41 29 ef d2               	vpxor	xmm10, xmm10, xmm10
   1af3d: c4 41 21 ef db               	vpxor	xmm11, xmm11, xmm11
   1af42: c4 41 19 ef e4               	vpxor	xmm12, xmm12, xmm12
   1af47: c4 41 11 ef ed               	vpxor	xmm13, xmm13, xmm13
   1af4c: 48 83 c0 20                  	add	rax, 0x20
   1af50: 62 f2 7d 48 35 ed            	vpmovzxdq	zmm5, ymm5      # zmm5 = ymm5[0],zero,ymm5[1],zero,ymm5[2],zero,ymm5[3],zero,ymm5[4],zero,ymm5[5],zero,ymm5[6],zero,ymm5[7],zero
   1af56: 62 f2 7d 48 35 f6            	vpmovzxdq	zmm6, ymm6      # zmm6 = ymm6[0],zero,ymm6[1],zero,ymm6[2],zero,ymm6[3],zero,ymm6[4],zero,ymm6[5],zero,ymm6[6],zero,ymm6[7],zero
   1af5c: 62 f2 7d 48 35 ff            	vpmovzxdq	zmm7, ymm7      # zmm7 = ymm7[0],zero,ymm7[1],zero,ymm7[2],zero,ymm7[3],zero,ymm7[4],zero,ymm7[5],zero,ymm7[6],zero,ymm7[7],zero
   1af62: 62 52 7d 48 35 c0            	vpmovzxdq	zmm8, ymm8      # zmm8 = ymm8[0],zero,ymm8[1],zero,ymm8[2],zero,ymm8[3],zero,ymm8[4],zero,ymm8[5],zero,ymm8[6],zero,ymm8[7],zero
   1af68: 62 f1 d5 48 73 f5 02         	vpsllq	zmm5, zmm5, 0x2
   1af6f: 62 f1 cd 48 73 f6 02         	vpsllq	zmm6, zmm6, 0x2
   1af76: 62 f1 c5 48 73 f7 02         	vpsllq	zmm7, zmm7, 0x2
   1af7d: 62 d1 bd 48 73 f0 02         	vpsllq	zmm8, zmm8, 0x2
   1af84: 62 72 fd 49 91 4c ef 01      	vpgatherqq	zmm9 {k1}, zmmword ptr [rdi + 8*zmm5 + 0x8]
   1af8c: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1af90: 62 72 fd 49 91 54 f7 01      	vpgatherqq	zmm10 {k1}, zmmword ptr [rdi + 8*zmm6 + 0x8]
   1af98: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1af9c: 62 72 fd 49 91 5c ff 01      	vpgatherqq	zmm11 {k1}, zmmword ptr [rdi + 8*zmm7 + 0x8]
   1afa4: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1afa8: 62 32 fd 49 91 64 c7 01      	vpgatherqq	zmm12 {k1}, zmmword ptr [rdi + 8*zmm8 + 0x8]
   1afb0: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1afb4: 62 72 7d 49 91 6c ef 04      	vpgatherqd	ymm13 {k1}, ymmword ptr [rdi + 8*zmm5 + 0x10]
   1afbc: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1afc0: c5 d1 ef ed                  	vpxor	xmm5, xmm5, xmm5
   1afc4: 62 f2 7d 49 91 6c f7 04      	vpgatherqd	ymm5 {k1}, ymmword ptr [rdi + 8*zmm6 + 0x10]
   1afcc: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1afd0: c5 c9 ef f6                  	vpxor	xmm6, xmm6, xmm6
   1afd4: 62 f2 7d 49 91 74 ff 04      	vpgatherqd	ymm6 {k1}, ymmword ptr [rdi + 8*zmm7 + 0x10]
   1afdc: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1afe0: c5 c1 ef ff                  	vpxor	xmm7, xmm7, xmm7
   1afe4: 62 b2 7d 49 91 7c c7 04      	vpgatherqd	ymm7 {k1}, ymmword ptr [rdi + 8*zmm8 + 0x10]
   1afec: 62 52 7d 48 35 c5            	vpmovzxdq	zmm8, ymm13     # zmm8 = ymm13[0],zero,ymm13[1],zero,ymm13[2],zero,ymm13[3],zero,ymm13[4],zero,ymm13[5],zero,ymm13[6],zero,ymm13[7],zero
   1aff2: 62 f2 7d 48 35 ed            	vpmovzxdq	zmm5, ymm5      # zmm5 = ymm5[0],zero,ymm5[1],zero,ymm5[2],zero,ymm5[3],zero,ymm5[4],zero,ymm5[5],zero,ymm5[6],zero,ymm5[7],zero
   1aff8: 62 52 b5 48 40 c0            	vpmullq	zmm8, zmm9, zmm8
   1affe: 62 f2 ad 48 40 ed            	vpmullq	zmm5, zmm10, zmm5
   1b004: 62 f1 bd 48 d4 c0            	vpaddq	zmm0, zmm8, zmm0
   1b00a: 62 72 7d 48 35 c6            	vpmovzxdq	zmm8, ymm6      # zmm8 = ymm6[0],zero,ymm6[1],zero,ymm6[2],zero,ymm6[3],zero,ymm6[4],zero,ymm6[5],zero,ymm6[6],zero,ymm6[7],zero
   1b010: 62 f2 7d 48 35 f7            	vpmovzxdq	zmm6, ymm7      # zmm6 = ymm7[0],zero,ymm7[1],zero,ymm7[2],zero,ymm7[3],zero,ymm7[4],zero,ymm7[5],zero,ymm7[6],zero,ymm7[7],zero
   1b016: 62 f1 d5 48 d4 d2            	vpaddq	zmm2, zmm5, zmm2
   1b01c: 62 d2 a5 48 40 e8            	vpmullq	zmm5, zmm11, zmm8
   1b022: 62 f1 d5 48 d4 db            	vpaddq	zmm3, zmm5, zmm3
   1b028: 62 f2 9d 48 40 ee            	vpmullq	zmm5, zmm12, zmm6
   1b02e: 62 f1 d5 48 d4 e4            	vpaddq	zmm4, zmm5, zmm4
   1b034: 49 39 c1                     	cmp	r9, rax
   1b037: 0f 85 d3 fe ff ff            	jne	0x1af10 <rust_gather_batch_bitmask_safe+0x80>
   1b03d: 62 f1 ed 48 d4 c0            	vpaddq	zmm0, zmm2, zmm0
   1b043: 62 f1 e5 48 d4 c0            	vpaddq	zmm0, zmm3, zmm0
   1b049: 62 f1 dd 48 d4 c0            	vpaddq	zmm0, zmm4, zmm0
   1b04f: 62 f3 fd 48 3b c1 01         	vextracti64x4	ymm1, zmm0, 0x1
   1b056: 62 f1 fd 48 d4 c1            	vpaddq	zmm0, zmm0, zmm1
   1b05c: c4 e3 7d 39 c1 01            	vextracti128	xmm1, ymm0, 0x1
   1b062: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
   1b066: c5 f9 70 c8 ee               	vpshufd	xmm1, xmm0, 0xee        # xmm1 = xmm0[2,3,2,3]
   1b06b: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
   1b06f: c4 e1 f9 7e c0               	vmovq	rax, xmm0
   1b074: 4c 39 ce                     	cmp	rsi, r9
   1b077: 0f 84 d0 00 00 00            	je	0x1b14d <rust_gather_batch_bitmask_safe+0x2bd>
   1b07d: 40 f6 c6 18                  	test	sil, 0x18
   1b081: 0f 84 94 00 00 00            	je	0x1b11b <rust_gather_batch_bitmask_safe+0x28b>
   1b087: 49 83 c0 18                  	add	r8, 0x18
   1b08b: c4 e1 f9 6e c0               	vmovq	xmm0, rax
   1b090: c5 f1 ef c9                  	vpxor	xmm1, xmm1, xmm1
   1b094: 49 21 f0                     	and	r8, rsi
   1b097: 4e 8d 14 82                  	lea	r10, [rdx + 4*r8]
   1b09b: 0f 1f 44 00 00               	nop	dword ptr [rax + rax]
   1b0a0: c4 a3 75 0e 14 8a 55         	vpblendw	ymm2, ymm1, ymmword ptr [rdx + 4*r9], 0x55 # ymm2 = mem[0],ymm1[1],mem[2],ymm1[3],mem[4],ymm1[5],mem[6],ymm1[7],mem[8],ymm1[9],mem[10],ymm1[11],mem[12],ymm1[13],mem[14],ymm1[15]
   1b0a7: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b0ab: c5 e1 ef db                  	vpxor	xmm3, xmm3, xmm3
   1b0af: c5 d9 ef e4                  	vpxor	xmm4, xmm4, xmm4
   1b0b3: 49 83 c1 08                  	add	r9, 0x8
   1b0b7: 62 f2 7d 48 35 d2            	vpmovzxdq	zmm2, ymm2      # zmm2 = ymm2[0],zero,ymm2[1],zero,ymm2[2],zero,ymm2[3],zero,ymm2[4],zero,ymm2[5],zero,ymm2[6],zero,ymm2[7],zero
   1b0bd: 62 f1 ed 48 73 f2 02         	vpsllq	zmm2, zmm2, 0x2
   1b0c4: 62 f2 fd 49 91 5c d7 01      	vpgatherqq	zmm3 {k1}, zmmword ptr [rdi + 8*zmm2 + 0x8]
   1b0cc: c5 fc 46 c8                  	kxnorw	k1, k0, k0
   1b0d0: 62 f2 7d 49 91 64 d7 04      	vpgatherqd	ymm4 {k1}, ymmword ptr [rdi + 8*zmm2 + 0x10]
   1b0d8: 62 f2 7d 48 35 d4            	vpmovzxdq	zmm2, ymm4      # zmm2 = ymm4[0],zero,ymm4[1],zero,ymm4[2],zero,ymm4[3],zero,ymm4[4],zero,ymm4[5],zero,ymm4[6],zero,ymm4[7],zero
   1b0de: 62 f2 e5 48 40 d2            	vpmullq	zmm2, zmm3, zmm2
   1b0e4: 62 f1 ed 48 d4 c0            	vpaddq	zmm0, zmm2, zmm0
   1b0ea: 4d 39 c8                     	cmp	r8, r9
   1b0ed: 75 b1                        	jne	0x1b0a0 <rust_gather_batch_bitmask_safe+0x210>
   1b0ef: 62 f3 fd 48 3b c1 01         	vextracti64x4	ymm1, zmm0, 0x1
   1b0f6: 62 f1 fd 48 d4 c1            	vpaddq	zmm0, zmm0, zmm1
   1b0fc: c4 e3 7d 39 c1 01            	vextracti128	xmm1, ymm0, 0x1
   1b102: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
   1b106: c5 f9 70 c8 ee               	vpshufd	xmm1, xmm0, 0xee        # xmm1 = xmm0[2,3,2,3]
   1b10b: c5 f9 d4 c1                  	vpaddq	xmm0, xmm0, xmm1
   1b10f: c4 e1 f9 7e c0               	vmovq	rax, xmm0
   1b114: 4c 39 c6                     	cmp	rsi, r8
   1b117: 75 06                        	jne	0x1b11f <rust_gather_batch_bitmask_safe+0x28f>
   1b119: eb 32                        	jmp	0x1b14d <rust_gather_batch_bitmask_safe+0x2bd>
   1b11b: 4e 8d 14 8a                  	lea	r10, [rdx + 4*r9]
   1b11f: 48 8d 0c 8a                  	lea	rcx, [rdx + 4*rcx]
   1b123: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00       	nop	word ptr cs:[rax + rax]
   1b130: 41 0f b7 12                  	movzx	edx, word ptr [r10]
   1b134: 49 83 c2 04                  	add	r10, 0x4
   1b138: c1 e2 05                     	shl	edx, 0x5
   1b13b: 8b 74 17 10                  	mov	esi, dword ptr [rdi + rdx + 0x10]
   1b13f: 48 0f af 74 17 08            	imul	rsi, qword ptr [rdi + rdx + 0x8]
   1b145: 48 01 f0                     	add	rax, rsi
   1b148: 49 39 ca                     	cmp	r10, rcx
   1b14b: 75 e3                        	jne	0x1b130 <rust_gather_batch_bitmask_safe+0x2a0>
   1b14d: 59                           	pop	rcx
   1b14e: c5 f8 77                     	vzeroupper
   1b151: c3                           	ret
   1b152: 48 8d 3d ef cf fe ff         	lea	rdi, [rip - 0x13011]    # 0x8148 <writev+0x8148>
   1b159: 48 8d 15 b8 83 04 00         	lea	rdx, [rip + 0x483b8]    # 0x63518 <writev+0x63518>
   1b160: be 3d 00 00 00               	mov	esi, 0x3d
   1b165: ff 15 d5 b5 04 00            	call	qword ptr [rip + 0x4b5d5] # 0x66740 <writev+0x66740>
   1b16b: 0f 0b                        	ud2
   1b16d: ff 15 d5 b5 04 00            	call	qword ptr [rip + 0x4b5d5] # 0x66748 <writev+0x66748>
   1b173: cc                           	int3
   1b174: cc                           	int3
   1b175: cc                           	int3
   1b176: cc                           	int3
   1b177: cc                           	int3
   1b178: cc                           	int3
   1b179: cc                           	int3
   1b17a: cc                           	int3
   1b17b: cc                           	int3
   1b17c: cc                           	int3
   1b17d: cc                           	int3
   1b17e: cc                           	int3
   1b17f: cc                           	int3

# --- 7. RUST 1.91: 64-Bit Generational Slab Handle Lookup ---

/tmp/ep14_build/bench_rust:	file format elf64-x86-64

Disassembly of section .text:

000000000001bf70 <rust_temporal_generational_lookup>:
   1bf70: 53                           	push	rbx
   1bf71: 4d 85 c0                     	test	r8, r8
   1bf74: 74 5b                        	je	0x1bfd1 <rust_temporal_generational_lookup+0x61>
   1bf76: 4c 8b 4f 08                  	mov	r9, qword ptr [rdi + 0x8]
   1bf7a: 4c 8b 57 10                  	mov	r10, qword ptr [rdi + 0x10]
   1bf7e: 49 c1 e0 02                  	shl	r8, 0x2
   1bf82: 45 31 db                     	xor	r11d, r11d
   1bf85: 31 c0                        	xor	eax, eax
   1bf87: eb 10                        	jmp	0x1bf99 <rust_temporal_generational_lookup+0x29>
   1bf89: 0f 1f 80 00 00 00 00         	nop	dword ptr [rax]
   1bf90: 49 83 c3 04                  	add	r11, 0x4
   1bf94: 4d 39 d8                     	cmp	r8, r11
   1bf97: 74 36                        	je	0x1bfcf <rust_temporal_generational_lookup+0x5f>
   1bf99: 42 8b 3c 19                  	mov	edi, dword ptr [rcx + r11]
   1bf9d: 48 39 fa                     	cmp	rdx, rdi
   1bfa0: 76 33                        	jbe	0x1bfd5 <rust_temporal_generational_lookup+0x65>
   1bfa2: 8b 1c fe                     	mov	ebx, dword ptr [rsi + 8*rdi]
   1bfa5: 49 39 da                     	cmp	r10, rbx
   1bfa8: 76 e6                        	jbe	0x1bf90 <rust_temporal_generational_lookup+0x20>
   1bfaa: 48 c1 e3 06                  	shl	rbx, 0x6
   1bfae: 41 80 7c 19 28 00            	cmp	byte ptr [r9 + rbx + 0x28], 0x0
   1bfb4: 74 da                        	je	0x1bf90 <rust_temporal_generational_lookup+0x20>
   1bfb6: 8b 7c fe 04                  	mov	edi, dword ptr [rsi + 8*rdi + 0x4]
   1bfba: 4c 01 cb                     	add	rbx, r9
   1bfbd: 39 7b 20                     	cmp	dword ptr [rbx + 0x20], edi
   1bfc0: 75 ce                        	jne	0x1bf90 <rust_temporal_generational_lookup+0x20>
   1bfc2: 8b 7b 10                     	mov	edi, dword ptr [rbx + 0x10]
   1bfc5: 48 0f af 7b 08               	imul	rdi, qword ptr [rbx + 0x8]
   1bfca: 48 01 f8                     	add	rax, rdi
   1bfcd: eb c1                        	jmp	0x1bf90 <rust_temporal_generational_lookup+0x20>
   1bfcf: 5b                           	pop	rbx
   1bfd0: c3                           	ret
   1bfd1: 31 c0                        	xor	eax, eax
   1bfd3: 5b                           	pop	rbx
   1bfd4: c3                           	ret
   1bfd5: 48 8d 05 84 75 04 00         	lea	rax, [rip + 0x47584]    # 0x63560 <writev+0x63560>
   1bfdc: 48 89 d6                     	mov	rsi, rdx
   1bfdf: 48 89 c2                     	mov	rdx, rax
   1bfe2: ff 15 c8 a6 04 00            	call	qword ptr [rip + 0x4a6c8] # 0x666b0 <writev+0x666b0>
   1bfe8: 0f 0b                        	ud2
   1bfea: ff 15 58 a7 04 00            	call	qword ptr [rip + 0x4a758] # 0x66748 <writev+0x66748>

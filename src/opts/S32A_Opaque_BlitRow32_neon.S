/*
 * Copyright (c) 2005-2008, The Android Open Source Project
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

    .code 32
    .fpu neon
    .align 8

    .global S32A_Opaque_BlitRow32_neon_o
    .type   S32A_Opaque_BlitRow32_neon_o, %function
    .func

S32A_Opaque_BlitRow32_neon_o:
    cmp         r2, #0
    bxeq        lr
    vmov.i16    q15, #0x100
    cmp         r2,  #16
    blt         .LOpaque_less16
.LOpaque_loop16:
    vld4.8      {d0, d1, d2, d3}, [r1]!
    pld         [r1, #64]
    sub         r2, r2, #16
    vmov        r3, r12, s6, s7
    cmp         r3,  #0xffffffff
    cmpeq       r12, #0xffffffff
    beq         .LOpaque_all_ff1
    vld4.8      {d4, d5, d6, d7}, [r0]
    pld         [r0, #64]
    vsubw.u8    q14, q15, d3
    vshll.u8    q8,  d0, #8
    vshll.u8    q9,  d1, #8
    vshll.u8    q10, d2, #8
    vshll.u8    q11, d3, #8
    vmovl.u8    q0,  d4
    vmovl.u8    q1,  d5
    vmovl.u8    q2,  d6
    vmovl.u8    q3,  d7
    vmla.i16    q8,  q0, q14
    vmla.i16    q9,  q1, q14
    vmla.i16    q10, q2, q14
    vmla.i16    q11, q3, q14
    vshrn.i16   d0,  q8,  #8
    vshrn.i16   d1,  q9,  #8
    vshrn.i16   d2,  q10, #8
    vshrn.i16   d3,  q11, #8
.LOpaque_all_ff1:
    vld4.8      {d4, d5, d6, d7}, [r1]!
    pld         [r1, #64]
    vst4.8      {d0, d1, d2, d3}, [r0]!
    vmov        r3, r12, s14, s15
    cmp         r3,  #0xffffffff
    cmpeq       r12, #0xffffffff
    beq         .LOpaque_all_ff2
    vld4.8      {d0, d1, d2, d3}, [r0]
    pld         [r0, #64]
    vsubw.u8    q14, q15, d7
    vshll.u8    q11, d7, #8
    vshll.u8    q10, d6, #8
    vshll.u8    q9,  d5, #8
    vshll.u8    q8,  d4, #8
    vmovl.u8    q3,  d3
    vmovl.u8    q2,  d2
    vmovl.u8    q1,  d1
    vmovl.u8    q0,  d0
    vmla.i16    q11, q3, q14
    vmla.i16    q10, q2, q14
    vmla.i16    q9,  q1, q14
    vmla.i16    q8,  q0, q14
    vshrn.i16   d7,  q11, #8
    vshrn.i16   d6,  q10, #8
    vshrn.i16   d5,  q9,  #8
    vshrn.i16   d4,  q8,  #8
.LOpaque_all_ff2:
    cmp         r2,  #16
    vst4.8      {d4, d5, d6, d7}, [r0]!
    bge         .LOpaque_loop16
    cmp         r2, #0
    bxeq        lr

.LOpaque_less16:
    cmp         r2, #8
    blt         .LOpaque_less8
    vld4.8      {d0, d1, d2, d3}, [r1]!
    sub         r2, r2, #8
    vmov        r3, r12, s6, s7
    cmp         r3,  #0xffffffff
    cmpeq       r12, #0xffffffff
    beq         .LOpaque_all_ff3
    vld4.8      {d4, d5, d6, d7}, [r0]
    vsubw.u8    q14, q15, d3
    vshll.u8    q8,  d0, #8
    vshll.u8    q9,  d1, #8
    vshll.u8    q10, d2, #8
    vshll.u8    q11, d3, #8
    vmovl.u8    q0,  d4
    vmovl.u8    q1,  d5
    vmovl.u8    q2,  d6
    vmovl.u8    q3,  d7
    vmla.i16    q8,  q0, q14
    vmla.i16    q9,  q1, q14
    vmla.i16    q10, q2, q14
    vmla.i16    q11, q3, q14
    vshrn.i16   d0,  q8,  #8
    vshrn.i16   d1,  q9,  #8
    vshrn.i16   d2,  q10, #8
    vshrn.i16   d3,  q11, #8
.LOpaque_all_ff3:
    vst4.8      {d0, d1, d2, d3}, [r0]!
    cmp         r2, #0
    bxeq        lr

.LOpaque_less8:
    cmp         r2, #4
    blt         .LOpaque_less4
    ldr         r3, =0x03030303
    ldr         r12, =0x07070707
    vmov        d27, r3, r12
    vldm        r1!, {d0, d1}
    vtbl.8      d2, {d0}, d27
    vtbl.8      d3, {d1}, d27
    vand        d4, d2, d3
    vmov        r3, r12, s8, s9
    cmp         r3,  #0xffffffff
    cmpeq       r12, #0xffffffff
    beq         .LOpaque_all_ff4
    vldmia      r0, {d4, d5}
    vsubw.u8    q14, q15, d2
    vsubw.u8    q12, q15, d3
    vshll.u8    q8,  d0, #8
    vshll.u8    q9,  d1, #8
    vmovl.u8    q10, d4
    vmovl.u8    q11, d5
    vmla.i16    q8,  q10, q14
    vmla.i16    q9,  q11, q12
    vshrn.i16   d0,  q8, #8
    vshrn.i16   d1,  q9, #8
.LOpaque_all_ff4:
    subs        r2, r2, #4
    vstmia      r0!, {d0, d1}
    bxeq        lr

.LOpaque_less4:
    push        {r8, r9, lr}
    ldr         lr, =0x00ff00ff
.LOpaque_loop1:
    ldr         r3, [r1], #4
    sub         r2, r2, #1
    lsr         r12, r3, #24
    cmp         r12, #0xff
    beq         .LOpaque_all_ff5
    ldr         r8, [r0]
    rsb         r12, r12, #256
    and         r9, r8, lr                                  // B & R
    and         r8, lr, r8, lsr #8                          // A & G
    mul         r9, r9, r12
    mul         r8, r8, r12
    and         r9, lr, r9, lsr #8
    and         r8, r8, lr, lsl #8
    orr         r9, r9, r8
    add         r3, r3, r9
.LOpaque_all_ff5:
    str         r3, [r0], #4
    cmp         r2, #0
    bne         .LOpaque_loop1

    pop         {r8, r9, pc}

.endfunc

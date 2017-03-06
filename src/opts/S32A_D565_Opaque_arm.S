/*
 * Copyright 2006, The Android Open Source Project
 * Copyright (c) 2009, Code Aurora Forum.
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


/*
 * This file is derived from libpixelflinger version of BLIT routine.
 * Algorithm used for BLIT operation here is equivalent to the one in
 * C function, S32A_D565_Opaque. Use neon instructions to process 16 pixels
 * at-a-time on armv7. If the number of pixels is less than 16 and/or the
 * architecture is armv6 and below, use regular arm instructions. Regular
 * arm code combines two 16-bit writes into one 32-bit write to destination,
 * uses destination and source pre-loads, and unrolls the main loop thrice.
 */
    .text
    .align

    .global S32A_D565_Opaque_arm
/*
 * extern "C" void S32A_D565_Opaque_arm(uint16_t*, uint32_t*, size_t);
 * tao.zeng, optimize this funciton
 * ARM register allocate:
 * r0  --> dst ptr              r1 --> src ptr
 * r2  --> count                r3 -->
 *
 * NEON register allocate:
 * q1  --> src R
 * q2  --> src G
 * q3  --> src B
 * q4  --> src A
 * q5  --> dst[ 0 - 7]
 * q6  --> dst[15 - 8]
 * q8  --> 0xffffffff
 */
S32A_D565_Opaque_arm:
    cmp         r2, #0
    bxeq        lr
    cmp         r2, #16                                         // count > 16
    vmov.i8     q8,  #0x07                                      // mask
    blt         S32A_D565_Opaque_arm_less_16
    pld         [r1, #128]
    pld         [r0, #128]
    vpush       {d15}

S32A_D565_Opaque_arm_loop16:
    vld4.8      {d0, d2, d4, d6}, [r1]!                         // load 16 src data
    vld4.8      {d1, d3, d5, d7}, [r1]!
    vld2.8      {q14, q15}, [r0]                                // load 16 dst data
    sub         r2,  r2,  #16                                   // count -=16
    vand.u8     d15,  d6,  d7
    vmov        r3, r12, s30, s31
    cmp         r3,  #0xffffffff
    cmpeq       r12, #0xffffffff
    beq         S32A_D565_Opaque_arm_alpha255
    /*
     * alpha is not 255
     */
    vshl.i8     q9,  q15, #5                                    // q9  = high 3 bits of G
    vshl.i8     q10, q14, #3                                    // q10 = B
    vmvn.u8     q3,  q3                                         // alpha = 255 - alpha
    vsri.u8     q9,  q14, #3                                    // q9  = G
    vbic.i8     q15, q15, q8                                    // q15 = R

    vmull.u8    q11, d21, d7                                    // q11 = B[15 - 8].16
    vmull.u8    q10, d20, d6                                    // q10 = B[ 7 - 0].16
    vmull.u8    q12, d19, d7                                    // q12 = G[15 - 8].16
    vmull.u8    q9,  d18, d6                                    // q9  = G[ 7 - 0].16
    vmull.u8    q13, d31, d7                                    // q13 = R[15 - 8].16
    vmull.u8    q14, d30, d6                                    // q14 = R[ 7 - 0].16

    vshrn.i16   d23, q11, #8                                    // d23 = B[15 - 8].8
    vshrn.i16   d22, q10, #8                                    // d22 = B[ 7 - 0].8, q11
    vshrn.i16   d21, q12, #8                                    // d21 = G[15 - 8].8
    vshrn.i16   d20, q9,  #8                                    // d20 = G[ 7 - 0].8, q10
    vshrn.i16   d19, q13, #8                                    // d19 = R[15 - 8].8
    vshrn.i16   d18, q14, #8                                    // d18 = R[ 7 - 0].8, q9

    vqadd.u8    q2,  q2, q11                                    // q2  = result B
    vqadd.u8    q1,  q1, q10                                    // q1  = result G
    vqadd.u8    q0,  q0, q9                                     // q0  = result R

S32A_D565_Opaque_arm_alpha255:
    pld         [r1, #128]
    vshl.u8     q14, q1, #3                                     // low 3bits of G
    cmp         r2, #16

    vmov        q15, q0
    vsri.u8     q14, q2, #3                                     // blue
    vsri.u8     q15, q1, #5                                     // high 3 bits of G
    pld         [r0, #128]

    vst2.8      {d28, d30}, [r0]!
    vst2.8      {d29, d31}, [r0]!
    bge         S32A_D565_Opaque_arm_loop16
    cmp         r2, #0
    vpop        {d15}
    bxeq        lr

S32A_D565_Opaque_arm_less_16:
    cmp         r2, #8
    blt         S32A_D565_Opaque_arm_less_8

    vld4.8      {d2, d3, d4, d5}, [r1]!                         // load 16 src data
    vld2.8      {d28, d29}, [r0]                                // load 16 dst data
    vmov        r3, r12, s10, s11
    cmp         r3,  #0xffffffff
    cmpeq       r12, #0xffffffff
    beq         S32A_D565_Opaque_arm_alpha255_1
    /*
     * alpha is not 255
     */
    vshl.i8     d18, d29, #5                                    // d18 = high 3 bits of G
    vshl.i8     d20, d28, #3                                    // d20 = B
    vmvn.u8     d0,  d5                                         // alpha = 255 - alpha
    vsri.u8     d18, d28, #3                                    // d18 = G
    vbic.i8     d29, d29, d16                                   // d29 = R

    vmull.u8    q10, d20, d0                                    // q10 = B[ 7 - 0].16
    vmull.u8    q9,  d18, d0                                    // q9  = G[ 7 - 0].16
    vmull.u8    q12, d29, d0                                    // q12 = R[ 7 - 0].16

    vshrn.i16   d22, q10, #8                                    // d22 = B[ 7 - 0].8, q11
    vshrn.i16   d20, q9,  #8                                    // d20 = G[ 7 - 0].8, q10
    vshrn.i16   d18, q12, #8                                    // d18 = R[ 7 - 0].8, q9

    vqadd.u8    d4,  d4, d22                                    // d4  = result B
    vqadd.u8    d3,  d3, d20                                    // d3  = result G
    vqadd.u8    d2,  d2, d18                                    // d2  = result R

S32A_D565_Opaque_arm_alpha255_1:
    vsri.8      d2,  d3, #5                                     // d2  = result R | high 3 bits of G
    vshl.i8     d1,  d3, #3                                     // d1  = result low 3 bits of G
    vsri.8      d1,  d4, #3                                     // d1  = insirt result B
    subs        r2,  r2,  #8                                    // count -= 8
    vst2.8      {d1, d2}, [r0]!
    bxeq        lr

    /*
     * remain pixel, use ARM instruciton
     */
S32A_D565_Opaque_arm_less_8:
    stmfd       sp!, {r4, r6 - r8, lr}                          // reserve stack
    ldr         lr, =0x00ff00ff

one_pixel:
    ldr         r4, [r1], #4                                    // r4 = A0 B0 G0 R0
    ldrh        r3, [r0]                                        // r3 = 00 00 00 | r0 g0 b0, rgb565

    mov         r7, #0xff
    subs        r12, r7,  r4,  lsr #24                          // r12 = scale
    beq         zero_alpha_one

    and         r6,  lr,  r3,  lsr #8                           // r6  = 00 00 00 r0
    and         r8,  lr,  r3,  lsl #19                          // r8  = 00 b0 00 00
    orr         r6,  r8,  r6                                    // r6  = 00 b0 00 r0
    and         r7,  lr,  r3,  lsr #3                           // r7  = 00 00 00 g0
    mul         r6,  r6,  r12                                   // r6  = B0 B0 R0 R0
    mul         r7,  r7,  r12                                   // r7  = 00 00 G0 G0
    and         r6,  lr,  r6,  lsr #8                           // r6  = 00 B0 00 R0
    and         r7,  r7,  #0xff00                               // r7  = 00 00 G0 00
    orr         r7,  r6,  r7                                    // r7  = 00 B0 G0 R0
    add         r4,  r4,  r7

zero_alpha_one:                                                 // down scale ARGB8888 to RGB565
    mov         r7,  r4,  lsl #8                                // r7  = B0 G0 R0 00
    mov         r8,  r4,  lsr #5                                // r8  = ** ** *g g*
    and         r6,  r7,  #0xf800                               // r6  = 00 00 r0 00
    and         r8,  r8,  #0x7e0                                // r8  = 00 00 0g g0
    orr         r12, r6,  r8                                    // r12 = 00 00 rg g0
    orr         r12, r12, r7,  lsr #27                          // r12 = r0 g0 b0, RGB565

    subs        r2, #1
    strh        r12, [r0], #2
    bgt         one_pixel

    ldmfd       sp!, {r4, r6 - r8, lr}                          // reserve stack
out:
    bx          lr

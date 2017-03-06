/*
 * Copyright (c) 2005-2008, The Android Open Source Project
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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


    .text
    .align


    .global Blit_Pixel16ToPixel32
    .func Blit_Pixel16ToPixel32

//void Blit_Pixel16ToPixel32( uint32_t * colors, const uint16_t *srcAddr, int n );
// r0:  dst ptr
// r1:  src ptr
// r2:  n

Blit_Pixel16ToPixel32:
    /*
     * Register allcote
     * r0:  dst ptr
     * r1:  src ptr
     * r2:  n
     */
    cmp         r2,  #0
    bxeq        lr                                  // return if n == 0
    cmp         r2,  #16                            // r2 < 16?
    vmov.i8     d23, #0xff                          // q11 = [ff ff ff ff ff ff ff ff]
    vmov.i8     d19, #0xff                          // q9  = [ff ff ff ff ff ff ff ff]
    pld         [r1, #128]                          // preload data
    blt         Blit_P16ToP32_less_16

Blit_P16ToP32_loop_16:
    vld1.16     {d0, d1, d2, d3}, [r1]!             // load 16 pixel data from src ptr
    sub         r2,  #16                            // n -= 16
    cmp         r2,  #16

    vshl.i16    q2,  q0, #5                         // q2  = channel G
    vshl.i16    q3,  q0, #11                        // q3  = channel B
    vshl.i16    q12, q1, #5                         // q12 = channel G
    vshl.i16    q13, q1, #11                        // q13 = channel B

    vshrn.i16   d20, q0, #8                         // d20 = channel R
    vshrn.i16   d21, q2, #8                         // d21 = channel G
    vshrn.i16   d22, q3, #8                         // d22 = channel B

    vshrn.i16   d16, q1, #8                         // d16 = channel R
    vshrn.i16   d17, q12, #8                        // d17 = channel G
    vshrn.i16   d18, q13, #8                        // d18 = channel B

    pld         [r1, #128]                          // preload data
    vst4.8      {d20, d21, d22, d23}, [r0]!         // store data
    vst4.8      {d16, d17, d18, d19}, [r0]!         // store data

    bge         Blit_P16ToP32_loop_16

    cmp         r2,  #0                             // remain 0?
    bxeq        lr

Blit_P16ToP32_less_16:
    cmp         r2,  #8
    blt         Blit_P16ToP32_less_8

    vld1.16     {d0, d1}, [r1]!                     // load 8 pixel data from src ptr
    sub         r2,  #8
    cmp         r2,  #0                             // remain 0?

    vshl.i16    q2,  q0, #5                         // q2  = channel G
    vshl.i16    q3,  q0, #11                        // q3  = channel B

    vshrn.i16   d20, q0, #8                         // d20 = channel R
    vshrn.i16   d21, q2, #8                         // d21 = channel G
    vshrn.i16   d22, q3, #8                         // d22 = channel B

    vst4.8      {d20, d21, d22, d23}, [r0]!         // store data
    bxeq        lr

Blit_P16ToP32_less_8:
    cmp         r2,  #4
    ldr         r12, =0x00ff00ff
    push        {r4, r5, r6, r7, r8, r9}
    blt         Blit_P16ToP32_less_4                // n < 4

    vld1.16     {d0}, [r1]!
    sub         r2,  #4
    cmp         r2,  #0

    vshl.i16    q2,  q0, #5                         // q2  = channel G
    vshl.i16    q3,  q0, #11                        // q3  = channel B

    vshrn.i16   d20, q0, #8                         // d20 = channel R
    vshrn.i16   d21, q2, #8                         // d21 = channel G
    vshrn.i16   d22, q3, #8                         // d22 = channel B

    vst4.8      {d20[0], d21[0], d22[0], d23[0]}, [r0]!         // store data
    vst4.8      {d20[1], d21[1], d22[1], d23[1]}, [r0]!         // store data
    vst4.8      {d20[2], d21[2], d22[2], d23[2]}, [r0]!         // store data
    vst4.8      {d20[3], d21[3], d22[3], d23[3]}, [r0]!         // store data
    popeq       {r4, r5, r6, r7, r8, r9}
    bxeq        lr

Blit_P16ToP32_less_4:
    cmp         r2,  #2
    blt         Blit_P16ToP32_remain_1

    ldrh        r4, [r1], #2
    ldrh        r5, [r1], #2
    sub         r2,  #2
    cmp         r2,  #0

    orr         r4,  r4,  r5, lsl #16
    and         r6, r12, r4, LSR #8                 // r6 = 00 r1 00 r0
    and         r7, r12, r4, LSR #3                 // r7 = 00 g1 00 g0
    and         r4, r12, r4, LSL #3                 // r4 = 00 b1 00 b0
    orr         r6, r6, r7,  LSL #8                 // r6 = g1 r1 g0 r0
    orr         r4, r4, r12, LSL #8                 // r4 = ff b1 ff b0
    pkhtb       r7, r4, r6,  ASR #16                // r7 = ff b1 g1 r1
    pkhbt       r6, r6, r4,  LSL #16                // r6 = ff b0 g0 r0

    stmia       r0!, {r6, r7}
    popeq       {r4, r5, r6, r7, r8, r9}
    bxeq        lr

Blit_P16ToP32_remain_1:
    ldrh        r4, [r1], #2
    mov         r8, #0xff000000                     // r8 = ff 00 00 00
    and         r5, r4, #0x7e0                      // r5 = 00 00 0g g0
    and         r6, r4, #0xf800                     // r6 = 00 00 rr 00
    and         r7, r4, #0x1f                       // r7 = 00 00 00 bb
    add         r5, r8, r5, LSL #5                  // r5 = ff 00 gg 00
    add         r5, r5, r6, LSR #8                  // r5 = ff 00 gg rr
    add         r5, r5, r7, LSL #19                 // r5 = ff bb gg rr
    str         r5, [r0], #4

    pop         {r4, r5, r6, r7, r8, r9}
    bx          lr


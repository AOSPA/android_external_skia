
/*
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

// S32_Opaque_D32_filter_DX_shaderproc_neon(row0, row1, fx, maxX, subY, colors, dx, count);

#include "SkFixed.h"

void S32_Opaque_D32_filter_DX_shaderproc_neon(SkFixed fx,
                                              unsigned int subY,
                                              SkFixed dx,
                                              int count,
                                              const unsigned int* image0,
                                              const unsigned int* image1,
                                              unsigned int* SK_RESTRICT colors)
{
    /*
     * tao.zeng, optimize this function
     * register allocator:
     * r0 --> fx,       r1 --> subY,    r2 --> dx,  r3 --> count
     * r4 --> colors,   r5 --> image1,  r6 --> image0
     *
     * d31 --> y,       d30 --> 16 - y
     * d29 --> 16,      d28 -->
     *
     */
    asm volatile (
        "vpush          {q4 - q7}                               \n"
        "vdup.8         d31, r1                                 \n"     // d31 = [y]
        "vmov.i8        d29, #16                                \n"
        "vsub.i8        d30, d29, d31                           \n"     // d30 = [16 - y]
        "vmov.i16       q14, #16                                \n"     // q14 = [16].16
        "cmp            r3, #4                                  \n"
        "ldr            r14, =0x3fffc                           \n"     // r14 as mask
        "blt            S32_Opaque_D32_filter_DX_less4          \n"
        "sub            r3, #4                                  \n"

        "ubfx           r12, r0,  #12, #4                       \n"     // r12 = Xa
        "and            r7,  r14, r0, lsr #14                   \n"
        "add            r8,  r7, %[image0]                      \n"
        "add            r7,  r7, %[image1]                      \n"
        "add            r0,  r0, r2                             \n"     // fx += dx
        "vdup.16        d26, r12                                \n"     // d26 = [Xa].16

    "S32_Opaque_D32_filter_DX_loop4:                            \n"
        // pixel 1 ~ 2
        "vldr.32        d0,  [r8]                               \n"     // d0  = [a01, a00]
        "vldr.32        d1,  [r7]                               \n"     // d1  = [a11, a10]
        "ubfx           r12, r0, #12, #4                        \n"     // r12 = Xb
        "vmull.u8       q3,  d0, d30                            \n"     // q3  = [a01, a00] * (16-y)
        "vdup.16        d27, r12                                \n"     // d27 = [Xb]
        "vmull.u8       q4,  d1, d31                            \n"     // q4  = [a11, a10] * y

        "and            r7,  r14, r0, lsr #14                   \n"
        "add            r8,  r7,  %[image0]                     \n"
        "add            r7,  r7,  %[image1]                     \n"
        "vldr.32        d2,  [r8]                               \n"     // d2  = [b01, b00]
        "vldr.32        d3,  [r7]                               \n"     // d3  = [b11, b10]
        "add            r0,  r0,  r2                            \n"     // fx += dx
        "vsub.i16       q12, q14, q13                           \n"     // q12 = [16-Xb, 16-Xa].16
        "vmull.u8       q5,  d2, d30                            \n"     // q5  = [b01, b00] * (16 - y)
        "vmull.u8       q6,  d3, d31                            \n"     // q6  = [b11, b00] *  y

        // pixel 3 ~ 4
        "ubfx           r12, r0,  #12, #4                       \n"     // r12 = Xc
        "and            r7,  r14, r0, lsr #14                   \n"
        "vdup.16        d22, r12                                \n"     // d22 = [Xc].16
        "add            r8,  r7, %[image0]                      \n"
        "add            r7,  r7, %[image1]                      \n"
        "add            r0,  r0, r2                             \n"     // fx += dx
        "vswp           d7,  d10                                \n"     // q3  = [b00, a00].16, q5 = [b01, a01]*(16-y)
        "vswp           d9,  d12                                \n"     // q4  = [b10, a10].16, q6 = [b11, a11].16*y
        "vldr.32        d0,  [r8]                               \n"     // d0  = [c01, c00]
        "vldr.32        d1,  [r7]                               \n"     // d1  = [c11, c10]
        "ubfx           r12, r0, #12, #4                        \n"     // r12 = Xd
        "vmull.u8       q7,  d0, d30                            \n"     // q7  = [c01, c00] * (16-y)
        "vdup.16        d23, r12                                \n"     // d23 = [Xd]
        "vmull.u8       q8,  d1, d31                            \n"     // q8  = [c11, c10] * y

        "and            r7,  r14, r0, lsr #14                   \n"
        "add            r8,  r7,  %[image0]                     \n"
        "add            r7,  r7,  %[image1]                     \n"
        "vldr.32        d2,  [r8]                               \n"     // d2  = [d01, d00]
        "vldr.32        d3,  [r7]                               \n"     // d3  = [d11, d10]
        "add            r0,  r0,  r2                            \n"     // fx += dx
        "vsub.i16       q0,  q14, q11                           \n"     // q0  = [16-Xd, 16-Xc].16
        "vmull.u8       q9,  d2, d30                            \n"     // q9  = [d01, d00] * (16 - y)
        "vmull.u8       q10, d3, d31                            \n"     // q10 = [d11, d00] *  y

        "subs           r3,  r3, #4                             \n"
        "vswp           d15, d18                                \n"     // q7  = [d00, c00].16, q9 = [d01, c01].16*(16-y)
        "vswp           d17, d20                                \n"     // q8  = [d10, c10].16, q10= [d11, c11].16*y

        "ubfx           r12, r0,  #12, #4                       \n"     // r12 = Xa

        "vmul.i16       q3,  q3,  q12                           \n"     // q3  = [b00, a00]*(16-y)*[16-Xb, 16-Xa]
        "vmul.i16       q7,  q7,  q0                            \n"     // q7  = [d00, c00]*(16-y)*[16-Xd, 16-Xc]
        "and            r7,  r14, r0, lsr #14                   \n"
        "vmla.i16       q3,  q5,  q13                           \n"     // q3 += [b01, a01]*(16-y)*[Xb, Xa]
        "vmla.i16       q7,  q9,  q11                           \n"     // q7 += [d01, c01]*(16-y)*[Xd, Xc]
        "add            r8,  r7, %[image0]                      \n"
        "vmla.i16       q3,  q4,  q12                           \n"     // q3 += [b10, a10]*y*[16-Xb, 16-Xa]
        "vmla.i16       q7,  q8,  q0                            \n"     // q7 += [d10, c10]*y*[16-Xd, 16-Xc]
        "add            r7,  r7, %[image1]                      \n"
        "vmla.i16       q3,  q6,  q13                           \n"     // q3 += [b11, a11]*y*[Xb, Xa]
        "vmla.i16       q7,  q10, q11                           \n"     // q7 += [d11, c11]*y*[Xd, Xc]
        "add            r0,  r0, r2                             \n"     // fx += dx
        "vshrn.i16      d3,  q3, #8                             \n"     // result
        "vshrn.i16      d4,  q7, #8                             \n"     // result
        "vdup.16        d26, r12                                \n"     // d26 = [Xa].16
        "vstmia         %[colors]!, {d3, d4}                    \n"     // store data
        "bge            S32_Opaque_D32_filter_DX_loop4          \n"
        "adds           r3,  r3, #4                             \n"
        "beq            out                                     \n"
        "sub            r0,  r0, r2                             \n"     // because fx is added once more

    "S32_Opaque_D32_filter_DX_less4:                            \n"
        "cmp            r3, #2                                  \n"
        "blt            S32_Opaque_D32_filter_DX_less2          \n"
        // pixel 1 ~ 2
        "ubfx           r12, r0,  #12, #4                       \n"     // r12 = Xa
        "and            r7,  r14, r0, lsr #14                   \n"
        "vdup.16        d26, r12                                \n"     // d26 = [Xa].16
        "add            r8,  r7, %[image0]                      \n"
        "add            r0,  r0, r2                             \n"     // fx += dx
        "vldr.32        d0,  [r8]                               \n"     // d0  = [a01, a00]
        "add            r8,  r7, %[image1]                      \n"
        "vldr.32        d1,  [r8]                               \n"     // d1  = [a11, a10]
        "ubfx           r12, r0, #12, #4                        \n"     // r12 = Xb
        "vmull.u8       q3,  d0, d30                            \n"     // q3  = [a01, a00] * (16-y)
        "vdup.16        d27, r12                                \n"     // d27 = [Xb]
        "vmull.u8       q4,  d1, d31                            \n"     // q4  = [a11, a10] * y

        "and            r7,  r14, r0, lsr #14                   \n"
        "add            r8,  r7,  %[image0]                     \n"
        "vldr.32        d2,  [r8]                               \n"     // d2  = [b01, b00]
        "add            r8,  r7,  %[image1]                     \n"
        "vldr.32        d3,  [r8]                               \n"     // d3  = [b11, b10]
        "add            r0,  r0,  r2                            \n"     // fx += dx
        "vsub.i16       q12, q14, q13                           \n"     // q12 = [16-Xb, 16-Xa].16
        "vmull.u8       q5,  d2, d30                            \n"     // q5  = [b01, b00] * (16 - y)
        "vmull.u8       q6,  d3, d31                            \n"     // q6  = [b11, b00] *  y
        "subs           r3,  #2                                 \n"
        "vswp           d7,  d10                                \n"     // q3  = [b00, a00].16, q5 = [b01, a01]*(16-y)
        "vswp           d9,  d12                                \n"     // q4  = [b10, a10].16, q6 = [b11, a11].16*y

        "vmul.i16       q3,  q3,  q12                           \n"     // q3  = [b00, a00]*(16-y)*[16-Xb, 16-Xa]
        "vmla.i16       q3,  q5,  q13                           \n"     // q3 += [b01, a01]*(16-y)*[Xb, Xa]
        "vmla.i16       q3,  q4,  q12                           \n"     // q3 += [b10, a10]*y*[16-Xb, 16-Xa]
        "vmla.i16       q3,  q6,  q13                           \n"     // q3 += [b11, a11]*y*[Xb, Xa]
        "vshrn.i16      d3,  q3, #8                             \n"     // result
        "vstmia         %[colors]!, {d3}                        \n"     // store data
        "beq            out                                     \n"

    "S32_Opaque_D32_filter_DX_less2:                            \n"
        "ubfx           r12, r0,  #12, #4                       \n"     // r12 = Xa
        "and            r7,  r14, r0, lsr #14                   \n"
        "vdup.16        d26, r12                                \n"     // d26 = [Xa].16
        "add            r8,  r7, %[image0]                      \n"
        "vldr.32        d0,  [r8]                               \n"     // d0  = [a01, a00]
        "add            r8,  r7, %[image1]                      \n"
        "vldr.32        d1,  [r8]                               \n"     // d1  = [a11, a10]
        "vmull.u8       q3,  d0, d30                            \n"     // q3  = [a01, a00] * (16-y)
        "vmull.u8       q4,  d1, d31                            \n"     // q4  = [a11, a10] * y
        "vsub.i16       d27, d28, d26                           \n"     // d27 = [16-Xa].16

        "vmul.i16       d4,  d7,  d26                           \n"     // d4  = a01*(16-y)*x
        "vmla.i16       d4,  d9,  d26                           \n"     // d4 += a11*y*x
        "vmla.i16       d4,  d6,  d27                           \n"     // d4 += a00*(16-y)*(16-x)
        "vmla.i16       d4,  d8,  d27                           \n"     // d4 += a10*y*(16-x)
        "vshrn.i16      d4,  q2,  #8                            \n"
        "vst1.32        {d4[0]}, [%[colors]]!                   \n"     // store data

    "out:                                                       \n"
        "vpop           {q4 - q7}                               \n"
        :
        : [colors] "r" (colors), [image0] "r" (image0), [image1] "r" (image1), [fx] "r" (fx),
          [dx] "r" (dx), [subY] "r" (subY), [count] "r" (count)
        : "cc", "memory", "r7", "r8", "r12", "lr"
    );
}

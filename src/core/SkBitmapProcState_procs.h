/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Define NAME_WRAP(x) before including this header to perform name-wrapping
// E.g. for ARM NEON, defined it as 'x ## _neon' to ensure all important
// identifiers have a _neon suffix.
#ifndef NAME_WRAP
#error "Please define NAME_WRAP() before including this file"
#endif

// returns expanded * 5bits
static inline uint32_t Filter_565_Expanded(unsigned x, unsigned y,
                                           uint32_t a00, uint32_t a01,
                                           uint32_t a10, uint32_t a11) {
    SkASSERT((unsigned)x <= 0xF);
    SkASSERT((unsigned)y <= 0xF);

    a00 = SkExpand_rgb_16(a00);
    a01 = SkExpand_rgb_16(a01);
    a10 = SkExpand_rgb_16(a10);
    a11 = SkExpand_rgb_16(a11);

    int xy = x * y >> 3;
    return  a00 * (32 - 2*y - 2*x + xy) +
            a01 * (2*x - xy) +
            a10 * (2*y - xy) +
            a11 * xy;
}

// turn an expanded 565 * 5bits into SkPMColor
// g:11 | r:10 | x:1 | b:10
static inline SkPMColor SkExpanded_565_To_PMColor(uint32_t c) {
    unsigned r = (c >> 13) & 0xFF;
    unsigned g = (c >> 24);
    unsigned b = (c >> 2) & 0xFF;
    return SkPackARGB32(0xFF, r, g, b);
}

// returns answer in SkPMColor format
static inline SkPMColor Filter_4444_D32(unsigned x, unsigned y,
                                        uint32_t a00, uint32_t a01,
                                        uint32_t a10, uint32_t a11) {
    SkASSERT((unsigned)x <= 0xF);
    SkASSERT((unsigned)y <= 0xF);

    a00 = SkExpand_4444(a00);
    a01 = SkExpand_4444(a01);
    a10 = SkExpand_4444(a10);
    a11 = SkExpand_4444(a11);

    int xy = x * y >> 4;
    uint32_t result =   a00 * (16 - y - x + xy) +
                        a01 * (x - xy) +
                        a10 * (y - xy) +
                        a11 * xy;

    return SkCompact_8888(result);
}

static inline U8CPU Filter_8(unsigned x, unsigned y,
                             U8CPU a00, U8CPU a01,
                             U8CPU a10, U8CPU a11) {
    SkASSERT((unsigned)x <= 0xF);
    SkASSERT((unsigned)y <= 0xF);

    int xy = x * y;
    unsigned result =   a00 * (256 - 16*y - 16*x + xy) +
                        a01 * (16*x - xy) +
                        a10 * (16*y - xy) +
                        a11 * xy;

    return result >> 8;
}

/*****************************************************************************
 *
 *  D32 functions
 *
 */

// SRC == 8888

#define FILTER_PROC(x, y, a, b, c, d, dst)   NAME_WRAP(Filter_32_opaque)(x, y, a, b, c, d, dst)

#define MAKENAME(suffix)        NAME_WRAP(S32_opaque_D32 ## suffix)
#define SRCTYPE                 SkPMColor
#define CHECKSTATE(state)       SkASSERT(4 == state.fPixmap.info().bytesPerPixel()); \
                                SkASSERT(state.fAlphaScale == 256)
#define RETURNDST(src)          src
#define SRC_TO_FILTER(src)      src
#if !SK_ARM_NEON_IS_NONE && !defined(__aarch64__)
#include "SkUtils.h"

#if DSTSIZE==32
    #define DSTTYPE SkPMColor
#elif DSTSIZE==16
    #define DSTTYPE uint16_t
#else
    #error "need DSTSIZE to be 32 or 16"
#endif

#if (DSTSIZE == 32)
    #define BITMAPPROC_MEMSET(ptr, value, n) sk_memset32(ptr, value, n)
#elif (DSTSIZE == 16)
    #define BITMAPPROC_MEMSET(ptr, value, n) sk_memset16(ptr, value, n)
#else
    #error "unsupported DSTSIZE"
#endif
void NAME_WRAP(S32_opaque_D32_nofilter_DX_t)(const SkBitmapProcState& s,
                                  const uint32_t* SK_RESTRICT xy,
                                  int count, DSTTYPE* SK_RESTRICT colors)
{
    SkASSERT(count > 0 && colors != NULL);
    SkASSERT(s.fInvType <= (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask));
    SkASSERT(s.fDoFilter == false);
    SkDEBUGCODE(CHECKSTATE(s);)

#ifdef PREAMBLE
    PREAMBLE(s);
#endif
    const SRCTYPE* SK_RESTRICT srcAddr = (const SRCTYPE*)s.fBitmap->getPixels();

    // buffer is y32, x16, x16, x16, x16, x16
    // bump srcAddr to the proper row, since we're told Y never changes
    SkASSERT((unsigned)xy[0] < (unsigned)s.fBitmap->height());
    srcAddr = (const SRCTYPE*)((const char*)srcAddr +
                                                xy[0] * s.fBitmap->rowBytes());
    xy += 1;

    SRCTYPE src;

    if (1 == s.fBitmap->width()) {
        src = srcAddr[0];
        DSTTYPE dstValue = RETURNDST(src);
        BITMAPPROC_MEMSET(colors, dstValue, count);
    } else {
        asm volatile (
        ".LS32_Opaque_D32_nofilter_DX_t:                            \n"
            "push       {r4 - r11, lr}                              \n"
            "ldr        r14, =0xffff                                \n"         // data mask
            "movs       r12, %[count], lsr #3                       \n"         // r12 = r2 / 8
            "beq        .LS32_Opaque_D32_nofilter_DX_less_than_8    \n"
            "ldmia      %[xy]!, {r4, r6, r8, r10}                   \n"         // preload first loop data

        ".LS32_Opaque_D32_nofilter_DX_loop_8:                       \n"
            "subs       r12, #1                                     \n"         // count -= 8
            "pld        [%[xy], #128]                               \n"
            "lsr        r5,  r4,  #16                               \n"         // r5  = high 16 bits of r4
            "and        r4,  r4,  r14                               \n"         // r4  = low  16 bits of r4
            "lsr        r7,  r6,  #16                               \n"         // r7  = high 16 bits of r6
            "and        r6,  r6,  r14                               \n"         // r6  = low  16 bits of r6
            "lsr        r9,  r8,  #16                               \n"         // r9  = high 16 bits of r8
            "and        r8,  r8,  r14                               \n"         // r8  = low  16 bits of r8
            "lsr        r11, r10, #16                               \n"         // r11 = high 16 bits of r10
            "and        r10, r10, r14                               \n"         // r10 = low  16 bits of r10
            "ldr        r4,  [%[srcAddr], r4,  lsl #2]              \n"
            "ldr        r5,  [%[srcAddr], r5,  lsl #2]              \n"
            "ldr        r6,  [%[srcAddr], r6,  lsl #2]              \n"
            "ldr        r7,  [%[srcAddr], r7,  lsl #2]              \n"
            "ldr        r8,  [%[srcAddr], r8,  lsl #2]              \n"
            "ldr        r9,  [%[srcAddr], r9,  lsl #2]              \n"
            "ldr        r10, [%[srcAddr], r10, lsl #2]              \n"
            "ldr        r11, [%[srcAddr], r11, lsl #2]              \n"
            "stm        %[colors]!, {r4 - r11}                      \n"         // store data
            "it         ne                                          \n"
            "ldmne      %[xy]!, {r4, r6, r8, r10}                   \n"         // load data for next loop
            "bne        .LS32_Opaque_D32_nofilter_DX_loop_8         \n"

        ".LS32_Opaque_D32_nofilter_DX_less_than_8:                  \n"
            "ands       %[count], %[count], #7                      \n"
            "beq        .LS32_Opaque_D32_nofilter_DX_out            \n"
            "cmp        %[count], #4                                \n"
            "blt        .LS32_Opaque_D32_nofilter_DX_less_than_4    \n"

            "ldm        %[xy]!, {r4, r6}                            \n"
            "subs       %[count], #4                                \n"
            "lsr        r5, r4, #16                                 \n"
            "and        r4, r4, r14                                 \n"
            "lsr        r7, r6, #16                                 \n"
            "and        r6, r6, r14                                 \n"
            "ldr        r4,  [%[srcAddr], r4,  lsl #2]              \n"
            "ldr        r5,  [%[srcAddr], r5,  lsl #2]              \n"
            "ldr        r6,  [%[srcAddr], r6,  lsl #2]              \n"
            "ldr        r7,  [%[srcAddr], r7,  lsl #2]              \n"
            "stm        %[colors]!, {r4 - r7}                       \n"
            "beq        .LS32_Opaque_D32_nofilter_DX_out            \n"

        ".LS32_Opaque_D32_nofilter_DX_less_than_4:                  \n"
            "cmp        %[count], #2                                \n"
            "blt        .LS32_Opaque_D32_nofilter_DX_less_than_2    \n"

            "ldr        r4, [%[xy]], #4                             \n"
            "subs       %[count], #2                                \n"
            "lsr        r5, r4, #16                                 \n"
            "and        r4, r4, r14                                 \n"
            "ldr        r4,  [%[srcAddr], r4,  lsl #2]              \n"
            "ldr        r5,  [%[srcAddr], r5,  lsl #2]              \n"
            "stm        %[colors]!, {r4, r5}                        \n"
            "beq        .LS32_Opaque_D32_nofilter_DX_out            \n"

        ".LS32_Opaque_D32_nofilter_DX_less_than_2:                  \n"
            "ldrh       r4, [%[xy]], #2                             \n"
            "ldr        r5, [%[srcAddr], r4, lsl #2]                \n"
            "str        r5, [%[colors]], #4                         \n"
        ".LS32_Opaque_D32_nofilter_DX_out:                          \n"
            "pop        {r4 - r11, lr}                              \n"
            :
            :[xy] "r" (xy), [srcAddr] "r" (srcAddr), [count] "r" (count),
             [colors] "r" (colors)
            : "memory", "cc", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
              "r11", "r12", "r14"
        );
    }

#ifdef POSTAMBLE
    POSTAMBLE(s);
#endif
}
#endif  //SK_ARM_NEON_IS_NONE
#include "SkBitmapProcState_sample.h"

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst)   NAME_WRAP(Filter_32_alpha)(x, y, a, b, c, d, dst, alphaScale)

#define MAKENAME(suffix)        NAME_WRAP(S32_alpha_D32 ## suffix)
#define SRCTYPE                 SkPMColor
#define CHECKSTATE(state)       SkASSERT(4 == state.fPixmap.info().bytesPerPixel()); \
                                SkASSERT(state.fAlphaScale < 256)
#define PREAMBLE(state)         unsigned alphaScale = state.fAlphaScale
#define RETURNDST(src)          SkAlphaMulQ(src, alphaScale)
#define SRC_TO_FILTER(src)      src
#include "SkBitmapProcState_sample.h"

// SRC == 565

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst) \
    do {                                                        \
        uint32_t tmp = Filter_565_Expanded(x, y, a, b, c, d);   \
        *(dst) = SkExpanded_565_To_PMColor(tmp);                \
    } while (0)

#define MAKENAME(suffix)        NAME_WRAP(S16_opaque_D32 ## suffix)
#define SRCTYPE                 uint16_t
#define CHECKSTATE(state)       SkASSERT(kRGB_565_SkColorType == state.fPixmap.colorType()); \
                                SkASSERT(state.fAlphaScale == 256)
#define RETURNDST(src)          SkPixel16ToPixel32(src)
#define SRC_TO_FILTER(src)      src
#if !SK_ARM_NEON_IS_NONE && !defined(__aarch64__)
#if DSTSIZE==32
    #define DSTTYPE SkPMColor
#elif DSTSIZE==16
    #define DSTTYPE uint16_t
#else
    #error "need DSTSIZE to be 32 or 16"
#endif

#if (DSTSIZE == 32)
    #define BITMAPPROC_MEMSET(ptr, value, n) sk_memset32(ptr, value, n)
#elif (DSTSIZE == 16)
    #define BITMAPPROC_MEMSET(ptr, value, n) sk_memset16(ptr, value, n)
#else
    #error "unsupported DSTSIZE"
#endif

void NAME_WRAP(S16_opaque_D32_nofilter_DX_t)(const SkBitmapProcState& s,
                                  const uint32_t* SK_RESTRICT xy,
                                  int count,
                                  DSTTYPE* SK_RESTRICT colors) {

    SkASSERT(count > 0 && colors != NULL);
    SkASSERT(s.fInvType <= (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask));
    SkASSERT(s.fDoFilter == false);
    SkDEBUGCODE(CHECKSTATE(s);)

#ifdef PREAMBLE
    PREAMBLE(s);
#endif
    const SRCTYPE* SK_RESTRICT srcAddr = (const SRCTYPE*)s.fBitmap->getPixels();

    // buffer is y32, x16, x16, x16, x16, x16
    // bump srcAddr to the proper row, since we're told Y never changes
    SkASSERT((unsigned)xy[0] < (unsigned)s.fBitmap->height());
    srcAddr = (const SRCTYPE*)((const char*)srcAddr +
                                                xy[0] * s.fBitmap->rowBytes());
    xy += 1;

    SRCTYPE src;

    if (1 == s.fBitmap->width()) {
        src = srcAddr[0];
        DSTTYPE dstValue = RETURNDST(src);
        BITMAPPROC_MEMSET(colors, dstValue, count);
    } else {
        asm volatile (
    ".LS16_opaque_D32_nofilter_t:                           \n"
        "push           {r4 - r11, r14}                     \n"
        "ldr            r11, =0x1fffe                       \n"         // r11 = 0x1fffe, mask_t

        "mov            r12, %[count]                       \n"
        "subs           r12, #8                             \n"
        "blt            .LS16_opaque_D32_nofilter_DX_remain_7 \n"
        "pld            [%[xy], #128]                       \n"
        "ldmia          %[xy]!, {r8, r9}                    \n"         // preload data for first loop
        "vmov.i8        d31,  #0xff                         \n"         // d31 = [ff ff ff ff ff ff ff ff]

    ".LS16_opaque_D32_nofilter_DX_loop_8:                   \n"
        "and            r6, r11, r8, LSL #1                 \n"         // r6 = r8 low
        "and            r7, r11, r8, LSR #15                \n"         // r7 = r8 high
        "and            r8, r11, r9, LSL #1                 \n"         // r8 = r9 low
        "and            r9, r11, r9, LSR #15                \n"         // r9 = r9 high
        "ldrh           r7, [%[srcAddr], r7]                \n"         // get 4 RGB565 data
        "ldrh           r6, [%[srcAddr], r6]                \n"
        "ldrh           r9, [%[srcAddr], r9]                \n"
        "ldrh           r8, [%[srcAddr], r8]                \n"
        "ldmia          %[xy]!, {r10, r14}                  \n"

        "orr            r4, r6,  r7, LSL #16                \n"         // r4 = r1 g1 b1 | r0 g0 b0
        "orr            r5, r8,  r9, LSL #16                \n"         // r5 = r3 g3 b3 | r4 g4 b4
        "vmov           d0, r4,  r5                         \n"         // d0 = src3 src2 src1 src0

        "and            r6, r11, r10, LSL #1                \n"         // r6 = r8 low
        "and            r7, r11, r10, LSR #15               \n"         // r7 = r8 high
        "and            r8, r11, r14, LSL #1                \n"         // r8 = r9 low
        "and            r9, r11, r14, LSR #15               \n"         // r9 = r9 high
        "ldrh           r7, [%[srcAddr], r7]                \n"
        "ldrh           r6, [%[srcAddr], r6]                \n"         // get 4 RGB565 data
        "ldrh           r9, [%[srcAddr], r9]                \n"
        "ldrh           r8, [%[srcAddr], r8]                \n"
        "orr            r4, r6,  r7, LSL #16                \n"         // r4 = r4 g4 b4 | r5 g5 b5
        "orr            r5, r8,  r9, LSL #16                \n"         // r5 = r7 g7 b7 | r6 g6 b6
        "subs           r12, #8                             \n"         // count -= 8
        "vmov           d1, r4,  r5                         \n"         // d1 = src7 src6 src5 src4
        "it             ge                                  \n"
        "ldmge          %[xy]!, {r8, r9}                    \n"         // load data for next loop

        "vshl.i16       q1, q0, #5                          \n"         // q1 = channel G
        "vshl.i16       q2, q0, #11                         \n"         // q2 = channel B
        "pld            [%[xy], #128]                       \n"

        "vshrn.i16      d28, q0, #8                         \n"         // d28 = channel R
        "vshrn.i16      d29, q1, #8                         \n"         // d29 = channel G
        "vshrn.i16      d30, q2, #8                         \n"         // d30 = channel B
        "vst4.8         {d28, d29, d30, d31}, [%[colors]]!  \n"         // store data
        "bge            .LS16_opaque_D32_nofilter_DX_loop_8 \n"

    ".LS16_opaque_D32_nofilter_DX_remain_7:                 \n"
        "ands           r12, %[count], #7                   \n"
        "beq            .LS16_opaque_D32_nofilter_out       \n"

        "cmp            r12, #4                             \n"
        "blt            .LS16_opaque_D32_nofilter_remain_3  \n"

        "ldmia          %[xy]!, {r8, r9}                    \n"         // preload data for first loop
        "subs           r12, #4                             \n"         // count -= 4
        "and            r6, r11, r8, LSL #1                 \n"         // r6 = r8 low
        "and            r7, r11, r8, LSR #15                \n"         // r7 = r8 high
        "and            r8, r11, r9, LSL #1                 \n"         // r8 = r9 low
        "and            r9, r11, r9, LSR #15                \n"         // r9 = r9 high
        "ldrh           r7, [%[srcAddr], r7]                \n"         // get 4 RGB565 data
        "ldrh           r6, [%[srcAddr], r6]                \n"
        "ldrh           r9, [%[srcAddr], r9]                \n"
        "ldrh           r8, [%[srcAddr], r8]                \n"
        "orr            r4, r6,  r7, LSL #16                \n"         // r4 = r1 g1 b1 | r0 g0 b0
        "orr            r5, r8,  r9, LSL #16                \n"         // r5 = r3 g3 b3 | r4 g4 b4
        "vmov           d0, r4, r5                          \n"         // d0 = src3 src2 src1 src0

        "vshl.i16       q1, q0, #5                          \n"         // q1 = channel G
        "vshl.i16       q2, q0, #11                         \n"         // q2 = channel B

        "vshrn.i16      d28, q0, #8                         \n"         // d28 = channel R
        "vshrn.i16      d29, q1, #8                         \n"         // d29 = channel G
        "vshrn.i16      d30, q2, #8                         \n"         // d30 = channel B
        "vst4.8        {d28[0], d29[0], d30[0], d31[0]}, [%[colors]]!  \n"         // store data[0]
        "vst4.8        {d28[1], d29[1], d30[1], d31[1]}, [%[colors]]!  \n"         // store data[1]
        "vst4.8        {d28[2], d29[2], d30[2], d31[2]}, [%[colors]]!  \n"         // store data[2]
        "vst4.8        {d28[3], d29[3], d30[3], d31[3]}, [%[colors]]!  \n"         // store data[3]

        "beq            .LS16_opaque_D32_nofilter_out       \n"

    ".LS16_opaque_D32_nofilter_remain_3:                    \n"
        "ands           r12, %[count], #3                   \n"         // r12 = count & 3
        "beq            .LS16_opaque_D32_nofilter_out       \n"
        "cmp            r12, #2                             \n"
        "blt            .LS16_opaque_D32_nofilter_remian_1  \n"         // count < 2

        "ldr            r10, =0xff00ff                      \n"         // r10 = 0xff00ff, mask
        "ldr            r4, [%[xy]], #4                     \n"         // count >= 2
        "subs           r12, #2                             \n"
        "and            r6, r11, r4, LSL #1                 \n"
        "and            r7, r11, r4, LSR #15                \n"
        "ldrh           r6, [%[srcAddr], r6]                \n"
        "ldrh           r7, [%[srcAddr], r7]                \n"
        "orr            r4, r6, r7, LSL #16                 \n"         // r4 = r1 g1 b1 | r0 g0 b0
        "and            r6, r10, r4, LSR #8                 \n"         // r6 = 00 r1 00 r0;
        "and            r7, r10, r4, LSR #3                 \n"         // r7 = 00 g1 00 g0;
        "and            r4, r10, r4, LSL #3                 \n"         // r4 = 00 b1 00 b0;
        "orr            r6, r6, r7,  LSL #8                 \n"         // r6 = g1 r1 g0 r0;
        "orr            r4, r4, r10, LSL #8                 \n"         // r4 = ff b1 ff b0;
        "pkhtb          r7, r4, r6,  ASR #16                \n"         // r7 = ff b1 g1 r1
        "pkhbt          r6, r6, r4,  LSL #16                \n"         // r6 = ff b0 g0 r0
        "stmia          %[colors]!, {r6, r7}                \n"
        "beq            .LS16_opaque_D32_nofilter_out       \n"

    ".LS16_opaque_D32_nofilter_remian_1:                    \n"         // remain 1
        "ldrh           r4, [%[xy]], #2                     \n"         // r4 = *xx++
        "lsl            r4, r4, #1                          \n"
        "ldrh           r4, [%[srcAddr], r4]                \n"         // r4 = srcAddr[*xx++]
        "mov            r8, #0xff000000                     \n"         // r8 = ff 00 00 00
        "and            r5, r4, #0x7e0                      \n"         // r5 = 00 00 0g g0
        "and            r6, r4, #0xf800                     \n"         // r6 = 00 00 rr 00
        "and            r7, r4, #0x1f                       \n"         // r7 = 00 00 00 bb
        "add            r5, r8, r5, LSL #5                  \n"         // r5 = ff 00 gg 00
        "add            r5, r5, r6, LSR #8                  \n"         // r5 = 00 00 gg rr
        "add            r5, r5, r7, LSL #19                 \n"         // r5 = ff bb gg rr
        "str            r5, [%[colors]], #4                 \n"         // *colors++

    ".LS16_opaque_D32_nofilter_out:                         \n"
        "pop            {r4 - r11, lr}                      \n"
            :
            :[xy] "r" (xy), [srcAddr] "r" (srcAddr), [count] "r" (count),
             [colors] "r" (colors)
            : "memory", "cc", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
              "r11", "r12", "r14"
        );
    }

#ifdef POSTAMBLE
    POSTAMBLE(s);
#endif
}
#endif
#include "SkBitmapProcState_sample.h"

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst) \
    do {                                                                    \
        uint32_t tmp = Filter_565_Expanded(x, y, a, b, c, d);               \
        *(dst) = SkAlphaMulQ(SkExpanded_565_To_PMColor(tmp), alphaScale);   \
    } while (0)

#define MAKENAME(suffix)        NAME_WRAP(S16_alpha_D32 ## suffix)
#define SRCTYPE                 uint16_t
#define CHECKSTATE(state)       SkASSERT(kRGB_565_SkColorType == state.fPixmap.colorType()); \
                                SkASSERT(state.fAlphaScale < 256)
#define PREAMBLE(state)         unsigned alphaScale = state.fAlphaScale
#define RETURNDST(src)          SkAlphaMulQ(SkPixel16ToPixel32(src), alphaScale)
#define SRC_TO_FILTER(src)      src
#include "SkBitmapProcState_sample.h"

// SRC == Index8

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst)   NAME_WRAP(Filter_32_opaque)(x, y, a, b, c, d, dst)

#define MAKENAME(suffix)        NAME_WRAP(SI8_opaque_D32 ## suffix)
#define SRCTYPE                 uint8_t
#define CHECKSTATE(state)       SkASSERT(kIndex_8_SkColorType == state.fPixmap.colorType()); \
                                SkASSERT(state.fAlphaScale == 256)
#define PREAMBLE(state)         const SkPMColor* SK_RESTRICT table = state.fPixmap.ctable()->readColors()
#define RETURNDST(src)          table[src]
#define SRC_TO_FILTER(src)      table[src]
#define POSTAMBLE(state)
#if !SK_ARM_NEON_IS_NONE && !defined(__aarch64__)
#if DSTSIZE==32
    #define DSTTYPE SkPMColor
#elif DSTSIZE==16
    #define DSTTYPE uint16_t
#else
    #error "need DSTSIZE to be 32 or 16"
#endif

#if (DSTSIZE == 32)
    #define BITMAPPROC_MEMSET(ptr, value, n) sk_memset32(ptr, value, n)
#elif (DSTSIZE == 16)
    #define BITMAPPROC_MEMSET(ptr, value, n) sk_memset16(ptr, value, n)
#else
    #error "unsupported DSTSIZE"
#endif
void NAME_WRAP(SI8_opaque_D32_filter_DX_t)(const SkBitmapProcState& s,
                                const uint32_t *SK_RESTRICT xy,
                                int             count,
                                DSTTYPE *SK_RESTRICT colors) {
    SkASSERT(count > 0 && colors != NULL);
    SkASSERT(s.fDoFilter);
    SkDEBUGCODE(CHECKSTATE(s);)

#ifdef PREAMBLE
    PREAMBLE(s);
#endif
    const char* SK_RESTRICT srcAddr = (const char*)s.fBitmap->getPixels();
    unsigned rb = s.fBitmap->rowBytes();
    unsigned subY;
    const SRCTYPE* SK_RESTRICT row0;
    const SRCTYPE* SK_RESTRICT row1;

    // setup row ptrs and update proc_table
    {
        uint32_t XY = *xy++;
        unsigned y0 = XY >> 14;
        row0 = (const SRCTYPE*)(srcAddr + (y0 >> 4) * rb);
        row1 = (const SRCTYPE*)(srcAddr + (XY & 0x3FFF) * rb);
        subY = y0 & 0xF;
    }

    do {
        uint32_t XX = *xy++;    // x0:14 | 4 | x1:14
        unsigned x0 = XX >> 14;
        unsigned x1 = XX & 0x3FFF;
        unsigned subX = x0 & 0xF;
        x0 >>= 4;

        FILTER_PROC(subX, subY,
                    SRC_TO_FILTER(row0[x0]),
                    SRC_TO_FILTER(row0[x1]),
                    SRC_TO_FILTER(row1[x0]),
                    SRC_TO_FILTER(row1[x1]),
                    colors);
        colors += 1;

    } while (--count != 0);
    asm volatile (
        "rsb        r12, %[subY], #16                       \n"
        "vdup.16    q0,  %[subY]                            \n"         // q0  = [00 y 00 y ...].16
        "vdup.16    q1,  r12                                \n"         // q1  = [00 16-y 00 16-y ...].16
        :
        : [subY] "r" (subY)
        : "memory", "cc", "r12", "q0", "q1"

    );
    // row0, row1, colors, xy, table, count
    asm volatile (
    ".LSI8_opaque_D32_filter_DX_t:                          \n"
        "push       {r6 - r11, lr}                          \n"
        "mov        r14, #0xf                               \n"
        "cmp        %[count],  #2                           \n"
        "it         ge                                      \n"
        "ldmge      %[xy]!, {r6, r8}                        \n"         // r6  = xy[0], r0 = xy[1]
        "pld        [%[xy], #128]                           \n"
        "blt        .LSI8_opaque_D32_filter_DX_t_remain_1   \n"
        "mov        r11, #0xf                               \n"

        /*
         * main loop, process 2 data
         */
    ".LSI8_opaque_D32_filter_DX_t_loop_2:                   \n"
        "and        r9,  r14, r6, lsr #14                   \n"         // r9  = x0
        "and        r10, r14, r8, lsr #14                   \n"         // r10 = x1
        "rsb        r11, r9,  #16                           \n"         // r11 = 16 - x0
        "rsb        r12, r10, #16                           \n"         // r12 = 16 - x1
        "lsr        r7,  r6,  #18                           \n"
        "vdup.16    d6,  r11                                \n"         // d6  = [00 16-x0 00 16-x0].16
        "vdup.16    d7,  r12                                \n"         // d7  = [00 16-x1 00 16-x1].16
        "vdup.16    d4,  r9                                 \n"         // d4  = [00 x0 00 x0].16
        "vdup.16    d5,  r10                                \n"         // d5  = [00 x1 00 x1].16
        "ldrb       r9,  [%[row0], r7]                      \n"         // r9  = row0[x00] = a00
        "ldrb       r11, [%[row1], r7]                      \n"         // r11 = row1[x00] = a10
        "lsr        r7,  r8, #18                            \n"
        "ldrb       r10, [%[row0], r7]                      \n"         // r10 = row0[x10] = b00
        "vmul.i16   q14, q3,  q0                            \n"         // q14 = [(16-x1)*y, (16-x0)y].16
        "ldrb       r12, [%[row1], r7]                      \n"         // r12 = row1[x10] = b10
        "vmul.i16   q15,  q3,  q1                           \n"         // q15 = [(16-x1)*(16-y), (16-x0)(16-y)].16
        "ldr        r9,  [%[table], r9,  lsl #2]            \n"         // r9  = SRC_TO_FILTER(row0[x00])
        "ldr        r10, [%[table], r10, lsl #2]            \n"         // r10 = SRC_TO_FILTER(row0[x10])
        "ldr        r11, [%[table], r11, lsl #2]            \n"         // r11 = SRC_TO_FILTER(row1[x00])
        "ldr        r12, [%[table], r12, lsl #2]            \n"         // r12 = SRC_TO_FILTER(row1[x10])
        "vmov       d18, r9,  r10                           \n"         // d18 = [b00 a00]
        "vmov       d19, r11, r12                           \n"         // d19 = [b10 a10]
        "ubfx       r11, r6, #0, #14                        \n"
        "ubfx       r12, r8, #0, #14                        \n"
        "vmovl.u8   q10, d18                                \n"         // q10 = [b00, a00].16
        "vmovl.u8   q11, d19                                \n"         // q11 = [b10, a10].16
        "ldrb       r9,  [%[row0], r11]                     \n"         // r9  = row0[x01] = a01
        "vmul.i16   q8,  q10, q15                           \n"         // q8  = [b00*(16-x1)(x6-y), a00*(16-x0)(16-y)]
        "ldrb       r10, [%[row0], r12]                     \n"         // r10 = row0[x11] = b01
        "ldrb       r11, [%[row1], r11]                     \n"         // r11 = row1[x01] = a11
        "ldrb       r12, [%[row1], r12]                     \n"         // r12 = row1[x11] = b11
        "vmul.i16   q15, q2,  q1                            \n"         // q15 = [x1*(16-y), x0(16-y)].16
        "ldr        r9,  [%[table], r9,  lsl #2]            \n"         // r9  = SRC_TO_FILTER(row0[x10])
        "ldr        r10, [%[table], r10, lsl #2]            \n"         // r10 = SRC_TO_FILTER(row0[x11])
        "vmla.i16   q8,  q11, q14                           \n"         // q8 += [b10*(16-x1)y, a10*(16-x0)y]
        "vmul.i16   q14, q2,  q0                            \n"         // q14 = [x1*y, x0*y].16
        "ldr        r11, [%[table], r11, lsl #2]            \n"         // r11 = SRC_TO_FILTER(row1[x10])
        "ldr        r12, [%[table], r12, lsl #2]            \n"         // r12 = SRC_TO_FILTER(row1[x11])
        "vmov       d18, r9,  r10                           \n"         // d18 = [b01 a01]
        "vmov       d19, r11, r12                           \n"         // d19 = [b11 a11]
        "vmovl.u8   q12, d18                                \n"         // q12 = [b01, a01].16
        "vmovl.u8   q13, d19                                \n"         // q13 = [b11, a11].16
        "vmla.i16   q8,  q12, q15                           \n"         // q8 += [b01*(16-y)x1, a01*(16-y)x0]
        "vmla.i16   q8,  q13, q14                           \n"         // q8 += [b11*x1*y, a11*x0*y]
        "pld        [%[xy], #128]                           \n"
        "sub        %[count],  #2                           \n"
        "cmp        %[count],  #2                           \n"         // count -= 2
        "it         ge                                      \n"
        "ldmge      %[xy]!, {r6, r8}                        \n"         // load data for next loop
        "vshrn.i16  d18, q8, #8                             \n"         // d16 = result[x1, x0]
        "vst1.32    {d18}, [%[colors]]!                     \n"         // store data
        "bge        .LSI8_opaque_D32_filter_DX_t_loop_2     \n"

    ".LSI8_opaque_D32_filter_DX_t_remain_1:                 \n"
        "ands       %[count], #1                            \n"
        "beq        .LSI8_opaque_D32_filter_DX_out          \n"

        "ldr        r6, [%[xy]], #4                         \n"
        "and        r9,  r14, r6, lsr #14                   \n"
        "rsb        r11, r9,  #16                           \n"         // r11 = 16 - x0
        "vdup.16    d5,  r9                                 \n"         // d5  = [00 x0].16
        "vdup.16    d7,  r11                                \n"         // d7  = [00 16-x0].16
        "ubfx       r12, r6, #0, #14                        \n"         // r12 = x1
        "lsr        r6,  r6, #18                            \n"
        "ldrb       r9,  [%[row0], r6]                      \n"         // r9  = row0[x0]
        "ldrb       r10, [%[row0], r12]                     \n"         // r10 = row0[x1]
        "ldrb       r11, [%[row1], r6]                      \n"         // r11 = row1[x0]
        "ldrb       r12, [%[row1], r12]                     \n"         // r12 = row1[x1]
        "vmul.i16   d4,  d0, d5                             \n"         // d4  = xy
        "vmul.i16   d5,  d2, d5                             \n"         // d5  = (16-y)x
        "vmul.i16   d6,  d0, d7                             \n"         // d6  = (16-x)y
        "vmul.i16   d7,  d2, d7                             \n"         // d7  = (16-x)(16-y)
        "ldr        r9,  [%[table], r9,  lsl #2]            \n"         // r9  = SRC_TO_FILTER(row0[x00])
        "ldr        r10, [%[table], r10, lsl #2]            \n"         // r10 = SRC_TO_FILTER(row0[x01])
        "ldr        r11, [%[table], r11, lsl #2]            \n"         // r11 = SRC_TO_FILTER(row1[x00])
        "ldr        r12, [%[table], r12, lsl #2]            \n"         // r12 = SRC_TO_FILTER(row1[x01])
        "vmov.32    d28[0], r9                              \n"         // d28 = [a00].32
        "vmov.32    d29[0], r10                             \n"         // d29 = [a01].32
        "vmov.32    d30[0], r11                             \n"         // d30 = [a10].32
        "vmov.32    d31[0], r12                             \n"         // d31 = [a11].32
        "vmovl.u8   q8,  d28                                \n"         // d16 = [a00].32
        "vmovl.u8   q9,  d29                                \n"         // d18 = [a01].32
        "vmovl.u8   q14, d30                                \n"         // d28 = [a10].32
        "vmovl.u8   q15, d31                                \n"         // d30 = [a11].32

        "vmul.i16   d16, d16,  d7                           \n"         // q8  = [a100(16-x1)(16-y), a000(16-x0)(16-y)]
        "vmla.i16   d16, d18,  d6                           \n"         // q8 += [a110(16-x1)y, a010(16-x0)y]
        "vmla.i16   d16, d28,  d5                           \n"         // q8 += [a100*x1(16-y), a000*x0(16-y)]
        "vmla.i16   d16, d30,  d4                           \n"         // q8 += [a100*x1*y, a000*x0*y]
        "vshrn.i16  d18, q8, #8                             \n"         // d16 = result[x1, x0]
        "vst1.32    {d18[0]}, [%[colors]]!                  \n"

    ".LSI8_opaque_D32_filter_DX_out:                        \n"
        "pop        {r6 - r11, r14}                         \n"
        :
        :[table] "r" (table), [xy] "r" (xy), [colors] "r" (colors),
         [row0] "r" (row0), [row1] "r" (row1), [count] "r" (count)
        : "memory", "cc", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r14"
    );

#ifdef POSTAMBLE
    POSTAMBLE(s);
#endif
}
#endif
#include "SkBitmapProcState_sample.h"

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst)   NAME_WRAP(Filter_32_alpha)(x, y, a, b, c, d, dst, alphaScale)

#define MAKENAME(suffix)        NAME_WRAP(SI8_alpha_D32 ## suffix)
#define SRCTYPE                 uint8_t
#define CHECKSTATE(state)       SkASSERT(kIndex_8_SkColorType == state.fPixmap.colorType()); \
                                SkASSERT(state.fAlphaScale < 256)
#define PREAMBLE(state)         unsigned alphaScale = state.fAlphaScale; \
                                const SkPMColor* SK_RESTRICT table = state.fPixmap.ctable()->readColors()
#define RETURNDST(src)          SkAlphaMulQ(table[src], alphaScale)
#define SRC_TO_FILTER(src)      table[src]
#define POSTAMBLE(state)
#include "SkBitmapProcState_sample.h"

// SRC == 4444

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst)  *(dst) = Filter_4444_D32(x, y, a, b, c, d)

#define MAKENAME(suffix)        NAME_WRAP(S4444_opaque_D32 ## suffix)
#define SRCTYPE                 SkPMColor16
#define CHECKSTATE(state)       SkASSERT(kARGB_4444_SkColorType == state.fPixmap.colorType()); \
                                SkASSERT(state.fAlphaScale == 256)
#define RETURNDST(src)          SkPixel4444ToPixel32(src)
#define SRC_TO_FILTER(src)      src
#include "SkBitmapProcState_sample.h"

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst)  \
    do {                                                    \
        uint32_t tmp = Filter_4444_D32(x, y, a, b, c, d);   \
        *(dst) = SkAlphaMulQ(tmp, alphaScale);              \
    } while (0)

#define MAKENAME(suffix)        NAME_WRAP(S4444_alpha_D32 ## suffix)
#define SRCTYPE                 SkPMColor16
#define CHECKSTATE(state)       SkASSERT(kARGB_4444_SkColorType == state.fPixmap.colorType()); \
                                SkASSERT(state.fAlphaScale < 256)
#define PREAMBLE(state)         unsigned alphaScale = state.fAlphaScale
#define RETURNDST(src)          SkAlphaMulQ(SkPixel4444ToPixel32(src), alphaScale)
#define SRC_TO_FILTER(src)      src
#include "SkBitmapProcState_sample.h"

// SRC == A8

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst) \
    do {                                                        \
        unsigned tmp = Filter_8(x, y, a, b, c, d);              \
        *(dst) = SkAlphaMulQ(pmColor, SkAlpha255To256(tmp));    \
    } while (0)

#define MAKENAME(suffix)        NAME_WRAP(SA8_alpha_D32 ## suffix)
#define SRCTYPE                 uint8_t
#define CHECKSTATE(state)       SkASSERT(kAlpha_8_SkColorType == state.fPixmap.colorType());
#define PREAMBLE(state)         const SkPMColor pmColor = state.fPaintPMColor;
#define RETURNDST(src)          SkAlphaMulQ(pmColor, SkAlpha255To256(src))
#define SRC_TO_FILTER(src)      src
#include "SkBitmapProcState_sample.h"

// SRC == Gray8

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst) \
    do {                                                        \
        unsigned tmp = Filter_8(x, y, a, b, c, d);              \
        SkPMColor color = SkPackARGB32(0xFF, tmp, tmp, tmp);    \
        *(dst) = SkAlphaMulQ(color, alphaScale);                \
    } while (0)

#define MAKENAME(suffix)        NAME_WRAP(SG8_alpha_D32 ## suffix)
#define SRCTYPE                 uint8_t
#define CHECKSTATE(state)       SkASSERT(kGray_8_SkColorType == state.fPixmap.colorType());
#define PREAMBLE(state)         unsigned alphaScale = state.fAlphaScale
#define RETURNDST(src)          SkAlphaMulQ(SkPackARGB32(0xFF, src, src, src), alphaScale)
#define SRC_TO_FILTER(src)      src
#include "SkBitmapProcState_sample.h"


#define TILEX_PROCF(fx, max)    SkClampMax((fx) >> 16, max)
#define TILEY_PROCF(fy, max)    SkClampMax((fy) >> 16, max)
#define TILEX_LOW_BITS(fx, max) (((fx) >> 12) & 0xF)
#define TILEY_LOW_BITS(fy, max) (((fy) >> 12) & 0xF)

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst)   NAME_WRAP(Filter_32_opaque)(x, y, a, b, c, d, dst)
#define MAKENAME(suffix)        NAME_WRAP(Clamp_SI8_opaque_D32 ## suffix)
#define SRCTYPE                 uint8_t
#define CHECKSTATE(state)       SkASSERT(kIndex_8_SkColorType == state.fPixmap.colorType())
#define PREAMBLE(state)         const SkPMColor* SK_RESTRICT table = state.fPixmap.ctable()->readColors()
#define SRC_TO_FILTER(src)      table[src]
#define POSTAMBLE(state)
#include "SkBitmapProcState_shaderproc.h"

#if !SK_ARM_NEON_IS_NONE && !defined(__aarch64__)
#define TILEX_PROCF(fx, max)    SkClampMax((fx) >> 16, max)
#define TILEY_PROCF(fy, max)    SkClampMax((fy) >> 16, max)
#define TILEX_LOW_BITS(fx, max) (((fx) >> 12) & 0xF)
#define TILEY_LOW_BITS(fy, max) (((fy) >> 12) & 0xF)

#undef FILTER_PROC
#define FILTER_PROC(x, y, a, b, c, d, dst)   Filter_32_opaque(x, y, a, b, c, d, dst)
#define MAKENAME(suffix)        NAME_WRAP(S32_Opaque_D32 ## suffix)
#define SRCTYPE                 uint32_t
#define DSTTYPE                 uint32_t
#define SRC_TO_FILTER(src)      src
#define S32_OPAQUE_D32_FILTER_DX_NEON
#include "SkBitmapProcState_shaderproc.h"
#undef S32_OPAQUE_D32_FILTER_DX_NEON
#endif //SK_ARM_NEON_IS_NONE

#undef NAME_WRAP

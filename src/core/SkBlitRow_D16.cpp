/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBlitRow.h"
#include "SkColorPriv.h"
#include "SkDither.h"
#include "SkMathPriv.h"

///////////////////////////////////////////////////////////////////////////////

static void S32_D565_Opaque(uint16_t* SK_RESTRICT dst,
                            const SkPMColor* SK_RESTRICT src, int count,
                            U8CPU alpha, int /*x*/, int /*y*/) {
    SkASSERT(255 == alpha);

    if (count > 0) {
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            *dst++ = SkPixel32ToPixel16_ToU16(c);
        } while (--count != 0);
    }
}

static void S32_D565_Blend(uint16_t* SK_RESTRICT dst,
                             const SkPMColor* SK_RESTRICT src, int count,
                             U8CPU alpha, int /*x*/, int /*y*/) {
    SkASSERT(255 > alpha);

    if (count > 0) {
        int scale = SkAlpha255To256(alpha);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            uint16_t d = *dst;
            *dst++ = SkPackRGB16(
                    SkAlphaBlend(SkPacked32ToR16(c), SkGetPackedR16(d), scale),
                    SkAlphaBlend(SkPacked32ToG16(c), SkGetPackedG16(d), scale),
                    SkAlphaBlend(SkPacked32ToB16(c), SkGetPackedB16(d), scale));
        } while (--count != 0);
    }
}

static void S32A_D565_Opaque(uint16_t* SK_RESTRICT dst,
                               const SkPMColor* SK_RESTRICT src, int count,
                               U8CPU alpha, int /*x*/, int /*y*/) {
#if defined(__aarch64__)
    if (count <= 0) {
        return ;
    }
    asm volatile (
        "cmp        %[count], #16                           \n" // count > 16
        "movi       v16.16b,  #0x07                         \n" // mask
        "b.lt       S32A_D565_Opaque_arm_less_16            \n"

    "S32A_D565_Opaque_arm_loop16:                           \n"
        "ld4        {v0.16b - v3.16b}, [%[src]], #64        \n" // load 16 src
        "ld2        {v4.16b, v5.16b}, [%[dst]]              \n" // load 16 dst
        "sub        %[count],  %[count],  #16               \n" // count -=16
        "mov        x6, v3.d[0]                             \n"
        "mov        x7, v3.d[1]                             \n"
        "and        x6, x6, x7                              \n"
        "cmp        x6, #0xffffffffffffffff                 \n"
        "b.eq       S32A_D565_Opaque_arm_alpha255           \n"
        /*
         * alpha is not 255
         */
        "shl        v7.16b, v5.16b, #5                      \n" // v7 = high 3 bits of G
        "shl        v6.16b, v4.16b, #3                      \n" // v6 = B
        "mvn        v3.16b, v3.16b                          \n" // alpha = 255 - alpha
        "sri        v7.16b, v4.16b, #3                      \n" // v7 = G
        "bic        v5.16b, v5.16b, v16.16b                 \n" // v5 = R

        "umull2     v18.8h, v6.16b, v3.16b                  \n" // v18 = B[15 - 8].16
        "umull      v17.8h, v6.8b, v3.8b                    \n" // v17 = B[ 7 - 0].16
        "umull2     v20.8h, v7.16b, v3.16b                  \n" // v20 = G[15 - 8].16
        "umull      v19.8h, v7.8b, v3.8b                    \n" // v19 = G[ 7 - 0].16
        "umull2     v22.8h, v5.16b, v3.16b                  \n" // v22 = R[15 - 8].16
        "umull      v21.8h, v5.8b, v3.8b                    \n" // v21 = R[ 7 - 0].16


        "shrn       v23.8b,  v17.8h, #8                     \n" //v23 = B[ 7 - 8].8
        "shrn2      v23.16b, v18.8h, #8                     \n" //v23 = B[15 - 8].8
        "shrn       v24.8b,  v19.8h, #8                     \n" //v24 = G[ 7 - 8].8
        "shrn2      v24.16b, v20.8h, #8                     \n" //v24 = G[15 - 8].8
        "shrn       v25.8b,  v21.8h, #8                     \n" //v25 = R[ 7 - 8].8
        "shrn2      v25.16b, v22.8h, #8                     \n" //v25 = R[15 - 8].8

        "uqadd      v2.16b, v2.16b, v23.16b                 \n" // v2 = result B
        "uqadd      v1.16b, v1.16b, v24.16b                 \n" // v1 = result G
        "uqadd      v0.16b, v0.16b, v25.16b                 \n" // v0 = result R

    "S32A_D565_Opaque_arm_alpha255:                         \n"
        "shl        v18.16b, v1.16b, #3                     \n" // low 3bits of G
        "cmp        %[count], #16                           \n"

        "mov        v19.16b, v0.16b                         \n"     // R
        "sri        v18.16b, v2.16b, #3                     \n"     // B
        "sri        v19.16b, v1.16b, #5                     \n"     // high 3 bits of G

        "st2        {v18.16b, v19.16b}, [%[dst]], #32       \n"
        "b.ge       S32A_D565_Opaque_arm_loop16             \n"
        "cmp        %[count], #0                            \n"
        "b.eq       out                                     \n"

    "S32A_D565_Opaque_arm_less_16:                          \n"
        "cmp        %[count], #8                            \n"
        "b.lt       S32A_D565_Opaque_arm_less_8             \n"

        "ld4        {v0.8b - v3.8b}, [%[src]], #32          \n"
        "ld2        {v4.8b, v5.8b}, [%[dst]]                \n"
        "mov        x6, v3.d[0]                             \n"
        "cmp        x6, #0xffffffffffffffff                 \n"
        "b.eq       S32A_D565_Opaque_arm_alpha255_1         \n"
        /*
         * alpha is not 255
         */
        "shl        v7.8b, v5.8b, #5                        \n" // v7 = high 3 bits of G
        "shl        v6.8b, v4.8b, #3                        \n" // v6 = B
        "mvn        v3.8b, v3.8b                            \n" // alpha = 255 - alpha
        "sri        v7.8b, v4.8b, #3                        \n" // v7 = G
        "bic        v5.8b, v5.8b, v16.8b                    \n" // v5 = R

        "umull      v17.8h, v6.8b, v3.8b                    \n" // v17 = B[ 7 - 0].16
        "umull      v19.8h, v7.8b, v3.8b                    \n" // v19 = G[ 7 - 0].16
        "umull      v21.8h, v5.8b, v3.8b                    \n" // v21 = R[ 7 - 0].16

        "shrn       v18.8b,  v17.8h, #8                     \n" //v18 = B[ 7 - 8].8
        "shrn       v20.8b,  v19.8h, #8                     \n" //v20 = G[ 7 - 8].8
        "shrn       v22.8b,  v21.8h, #8                     \n" //v22 = R[ 7 - 8].8

        "uqadd      v5.8b, v2.8b, v18.8b                    \n" // v5 = result B
        "uqadd      v6.8b, v1.8b, v20.8b                    \n" // v6 = result G
        "uqadd      v7.8b, v0.8b, v22.8b                    \n" // v7 = result R

    "S32A_D565_Opaque_arm_alpha255_1:                       \n"
        "sri        v7.8b, v6.8b, #5                        \n" // v7 = result R | high 3 bits of G
        "shl        v6.8b, v6.8b, #3                        \n" // v6 = result low 3 bits of G
        "sri        v6.8b, v5.8b, #3                        \n" // v6 = insirt result B
        "subs       %[count],  %[count],  #8                \n" // count -= 8
        "st2        {v6.8b, v7.8b}, [%[dst]], #16           \n"
        "b.eq       out                                     \n"

        /*
         * remain pixel, use ARM instruciton
         */
    "S32A_D565_Opaque_arm_less_8:                           \n"
        "ldr        x13, =0x00ff00ff                        \n"

    "one_pixel:                                             \n"
        "ldr        w9, [%[src]], #4                        \n" // x9 = A0 B0 G0 R0
        "ldrh       w8, [%[dst]]                            \n" // x8 = 00 00 00 | r0 g0 b0, rgb565

        "mov        x11, #0xff                              \n"
        "subs       x14, x11,  x9,  lsr #24                 \n" // x14 = scale
        "b.eq       zero_alpha_one                          \n"

        "and        x10,  x13,  x8,  lsr #8                 \n" // x10  = 00 00 00 r0
        "and        x12,  x13,  x8,  lsl #19                \n" // x12  = 00 b0 00 00
        "orr        x10,  x12,  x10                         \n" // x10  = 00 b0 00 r0
        "and        x11,  x13,  x8,  lsr #3                 \n" // x11  = 00 00 00 g0
        "mul        x10,  x10,  x14                         \n" // x10  = B0 B0 R0 R0
        "mul        x11,  x11,  x14                         \n" // x11  = 00 00 G0 G0
        "and        x10,  x13,  x10,  lsr #8                \n" // x10  = 00 B0 00 R0
        "and        x11,  x11,  #0xff00                     \n" // x11  = 00 00 G0 00
        "orr        x11,  x10,  x11                         \n" // x11  = 00 B0 G0 R0
        "add        x9,  x9,  x11                           \n"

    "zero_alpha_one:                                        \n" // down scale ARGB8888 to RGB565
        "lsl        x11,  x9,  #8                           \n" // x11  = B0 G0 R0 00
        "lsr        x12,  x9,  #5                           \n" // x12  = ** ** *g g*
        "and        x10,  x11,  #0xf800                     \n" // x10  = 00 00 r0 00
        "and        x12,  x12,  #0x7e0                      \n" // x12  = 00 00 0g g0
        "orr        x14, x10,  x12                          \n" // x14 = 00 00 rg g0
        "orr        x14, x14, x11,  lsr #27                 \n" // x14 = r0 g0 b0, RGB565

        "subs       %[count], %[count], #1                  \n"
        "strh       w14, [%[dst]], #2                       \n"
        "b.gt       one_pixel                               \n"

    "out:                                                   \n"
        :[dst] "+r" (dst), [count] "+r" (count), [src] "+r" (src)
        :
        :"memory", "cc", "x6", "x7", "x8", "x9", "x10", "x11",
         "x12", "x13"
    );
#else
    SkASSERT(255 == alpha);

    if (count > 0) {
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
//            if (__builtin_expect(c!=0, 1))
            if (c) {
                *dst = SkSrcOver32To16(c, *dst);
            }
            dst += 1;
        } while (--count != 0);
    }
#endif
}

static void S32A_D565_Blend(uint16_t* SK_RESTRICT dst,
                              const SkPMColor* SK_RESTRICT src, int count,
                               U8CPU alpha, int /*x*/, int /*y*/) {
    SkASSERT(255 > alpha);

    if (count > 0) {
        do {
            SkPMColor sc = *src++;
            SkPMColorAssert(sc);
            if (sc) {
                uint16_t dc = *dst;
                SkPMColor res = SkBlendARGB32(sc, SkPixel16ToPixel32(dc), alpha);
                *dst = SkPixel32ToPixel16(res);
            }
            dst += 1;
        } while (--count != 0);
    }
}

/////////////////////////////////////////////////////////////////////////////

static void S32_D565_Opaque_Dither(uint16_t* SK_RESTRICT dst,
                                     const SkPMColor* SK_RESTRICT src,
                                     int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 == alpha);

    if (count > 0) {
        DITHER_565_SCAN(y);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);

            unsigned dither = DITHER_VALUE(x);
            *dst++ = SkDitherRGB32To565(c, dither);
            DITHER_INC_X(x);
        } while (--count != 0);
    }
}

static void S32_D565_Blend_Dither(uint16_t* SK_RESTRICT dst,
                                    const SkPMColor* SK_RESTRICT src,
                                    int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 > alpha);

    if (count > 0) {
        int scale = SkAlpha255To256(alpha);
        DITHER_565_SCAN(y);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);

            int dither = DITHER_VALUE(x);
            int sr = SkGetPackedR32(c);
            int sg = SkGetPackedG32(c);
            int sb = SkGetPackedB32(c);
            sr = SkDITHER_R32To565(sr, dither);
            sg = SkDITHER_G32To565(sg, dither);
            sb = SkDITHER_B32To565(sb, dither);

            uint16_t d = *dst;
            *dst++ = SkPackRGB16(SkAlphaBlend(sr, SkGetPackedR16(d), scale),
                                 SkAlphaBlend(sg, SkGetPackedG16(d), scale),
                                 SkAlphaBlend(sb, SkGetPackedB16(d), scale));
            DITHER_INC_X(x);
        } while (--count != 0);
    }
}

static void S32A_D565_Opaque_Dither(uint16_t* SK_RESTRICT dst,
                                      const SkPMColor* SK_RESTRICT src,
                                      int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 == alpha);

    if (count > 0) {
        DITHER_565_SCAN(y);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            if (c) {
                unsigned a = SkGetPackedA32(c);

                int d = SkAlphaMul(DITHER_VALUE(x), SkAlpha255To256(a));

                unsigned sr = SkGetPackedR32(c);
                unsigned sg = SkGetPackedG32(c);
                unsigned sb = SkGetPackedB32(c);
                sr = SkDITHER_R32_FOR_565(sr, d);
                sg = SkDITHER_G32_FOR_565(sg, d);
                sb = SkDITHER_B32_FOR_565(sb, d);

                uint32_t src_expanded = (sg << 24) | (sr << 13) | (sb << 2);
                uint32_t dst_expanded = SkExpand_rgb_16(*dst);
                dst_expanded = dst_expanded * (SkAlpha255To256(255 - a) >> 3);
                // now src and dst expanded are in g:11 r:10 x:1 b:10
                *dst = SkCompact_rgb_16((src_expanded + dst_expanded) >> 5);
            }
            dst += 1;
            DITHER_INC_X(x);
        } while (--count != 0);
    }
}

static void S32A_D565_Blend_Dither(uint16_t* SK_RESTRICT dst,
                                     const SkPMColor* SK_RESTRICT src,
                                     int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 > alpha);

    if (count > 0) {
        int src_scale = SkAlpha255To256(alpha);
        DITHER_565_SCAN(y);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            if (c)
            {
                unsigned d = *dst;
                int sa = SkGetPackedA32(c);
                int dst_scale = SkAlpha255To256(255 - SkAlphaMul(sa, src_scale));
                int dither = DITHER_VALUE(x);

                int sr = SkGetPackedR32(c);
                int sg = SkGetPackedG32(c);
                int sb = SkGetPackedB32(c);
                sr = SkDITHER_R32To565(sr, dither);
                sg = SkDITHER_G32To565(sg, dither);
                sb = SkDITHER_B32To565(sb, dither);

                int dr = (sr * src_scale + SkGetPackedR16(d) * dst_scale) >> 8;
                int dg = (sg * src_scale + SkGetPackedG16(d) * dst_scale) >> 8;
                int db = (sb * src_scale + SkGetPackedB16(d) * dst_scale) >> 8;

                *dst = SkPackRGB16(dr, dg, db);
            }
            dst += 1;
            DITHER_INC_X(x);
        } while (--count != 0);
    }
}

///////////////////////////////////////////////////////////////////////////////

static uint32_t pmcolor_to_expand16(SkPMColor c) {
    unsigned r = SkGetPackedR32(c);
    unsigned g = SkGetPackedG32(c);
    unsigned b = SkGetPackedB32(c);
    return (g << 24) | (r << 13) | (b << 2);
}

static void Color32A_D565(uint16_t dst[], SkPMColor src, int count, int x, int y) {
    SkASSERT(count > 0);
    uint32_t src_expand = pmcolor_to_expand16(src);
    unsigned scale = SkAlpha255To256(0xFF - SkGetPackedA32(src)) >> 3;
    do {
        *dst = SkBlend32_RGB16(src_expand, *dst, scale);
        dst += 1;
    } while (--count != 0);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static const SkBlitRow::Proc16 gDefault_565_Procs[] = {
    // no dither
    S32_D565_Opaque,
    S32_D565_Blend,

    S32A_D565_Opaque,
    S32A_D565_Blend,

    // dither
    S32_D565_Opaque_Dither,
    S32_D565_Blend_Dither,

    S32A_D565_Opaque_Dither,
    S32A_D565_Blend_Dither
};

SkBlitRow::Proc16 SkBlitRow::Factory16(unsigned flags) {
    SkASSERT(flags < SK_ARRAY_COUNT(gDefault_565_Procs));
    // just so we don't crash
    flags &= kFlags16_Mask;

    SkBlitRow::Proc16 proc = PlatformFactory565(flags);
    if (nullptr == proc) {
        proc = gDefault_565_Procs[flags];
    }
    return proc;
}

static const SkBlitRow::ColorProc16 gDefault_565_ColorProcs[] = {
#if 0
    Color32A_D565,
    Color32A_D565_Dither
#else
    // TODO: stop cheating and fill dither from the above specializations!
    Color32A_D565,
    Color32A_D565,
#endif
};

SkBlitRow::ColorProc16 SkBlitRow::ColorFactory16(unsigned flags) {
    SkASSERT((flags & ~kFlags16_Mask) == 0);
    // just so we don't crash
    flags &= kFlags16_Mask;
    // we ignore both kGlobalAlpha_Flag and kSrcPixelAlpha_Flag, so shift down
    // no need for the additional code specializing on opaque alpha at this time
    flags >>= 2;

    SkASSERT(flags < SK_ARRAY_COUNT(gDefault_565_ColorProcs));

    SkBlitRow::ColorProc16 proc = PlatformColorFactory565(flags);
    if (nullptr == proc) {
        proc = gDefault_565_ColorProcs[flags];
    }
    return proc;
}

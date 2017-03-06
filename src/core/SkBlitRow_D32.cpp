/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBlitRow.h"
#include "SkBlitMask.h"
#include "SkColorPriv.h"
#include "SkOpts.h"
#include "SkUtils.h"

#define UNROLL

static void S32_Opaque_BlitRow32(SkPMColor* SK_RESTRICT dst,
                                 const SkPMColor* SK_RESTRICT src,
                                 int count, U8CPU alpha) {
#if defined(__aarch64__)
    /*
     * TODO: optimize for AARCH64
     */
    SkASSERT(255 == alpha);
    memcpy(dst, src, count * 4);
#else
    /*
     * tao.zeng@amlogic.com, src and dst are algined by 4
     */
    asm volatile (
        "cmp        %[count], #0                    \n"
        "it         eq                              \n"
        "bxeq       lr                              \n"                 // if count == 0, return
        "pld        [%[src], #32]                   \n"
        "tst        %[src], #0x04                   \n"                 // make aligne to 8 bytes
        "ittt       ne                              \n"
        "ldrne      r12, [%[src]], #4               \n"
        "strne      r12, [%[dst]], #4               \n"
        "subne      %[count], %[count], #1          \n"
        "cmp        %[count], #16                   \n"                 //
        "blt        S32_Opaque_BlitRow32_less16     \n"                 //
        "pld        [%[src], #64]                   \n"
        "sub        %[count], #16                   \n"
    "S32_Opaque_BlitRow32_loop16:                   \n"
        "vldmia     %[src]!, {q0, q1, q2, q3}       \n"
        "pld        [%[src], #64]                   \n"
        "pld        [%[src], #96]                   \n"
        "subs       %[count], %[count], #16         \n"
        "vstmia     %[dst]!, {q0, q1, q2, q3}       \n"
        "bge        S32_Opaque_BlitRow32_loop16     \n"
        "adds       %[count], %[count], #16         \n"
        "cmp        %[count], #0                    \n"
        "it         eq                              \n"
        "bxeq       lr                              \n"

    "S32_Opaque_BlitRow32_less16:                   \n"
        "cmp        %[count], #8                    \n"
        "blt        S32_Opaque_BlitRow32_less8      \n"
        "vldmia     %[src]!, {q0, q1}               \n"
        "subs       %[count], %[count], #8          \n"
        "vstmia     %[dst]!, {q0, q1}               \n"
        "it         eq                              \n"
        "bxeq       lr                              \n"
    "S32_Opaque_BlitRow32_less8:                    \n"
        "cmp        %[count], #4                    \n"
        "blt        S32_Opaque_BlitRow32_less4      \n"
        "vldmia     %[src]!, {d0, d1}               \n"
        "subs       %[count], %[count], #4          \n"
        "vstmia     %[dst]!, {d0, d1}               \n"
        "it         eq                              \n"
        "bxeq       lr                              \n"
    "S32_Opaque_BlitRow32_less4:                    \n"
        "cmp        %[count], #2                    \n"
        "itt        ge                              \n"
        "ldmge      %[src]!, {%[alpha], r12}        \n"
        "subges     %[count], #2                    \n"
        "it         ge                              \n"
        "stmge      %[dst]!, {%[alpha], r12}        \n"
        "it         eq                              \n"
        "bxeq       lr                              \n"
        "cmp        %[count], #0                    \n"
        "it         eq                              \n"
        "bxeq       lr                              \n"
        "ldr        r12, [%[src]], #4               \n"
        "str        r12, [%[dst]], #4               \n"
        "bx         lr                              \n"
        :
        :[src] "r" (src), [dst] "r" (dst), [count] "r" (count), [alpha] "r" (alpha)
        :"cc", "memory", "r12"
    );
#endif
}

static void S32_Blend_BlitRow32(SkPMColor* SK_RESTRICT dst,
                                const SkPMColor* SK_RESTRICT src,
                                int count, U8CPU alpha) {
    SkASSERT(alpha <= 255);
    if (count > 0) {
        unsigned src_scale = SkAlpha255To256(alpha);
        unsigned dst_scale = 256 - src_scale;

#ifdef UNROLL
        if (count & 1) {
            *dst = SkAlphaMulQ(*(src++), src_scale) + SkAlphaMulQ(*dst, dst_scale);
            dst += 1;
            count -= 1;
        }

        const SkPMColor* SK_RESTRICT srcEnd = src + count;
        while (src != srcEnd) {
            *dst = SkAlphaMulQ(*(src++), src_scale) + SkAlphaMulQ(*dst, dst_scale);
            dst += 1;
            *dst = SkAlphaMulQ(*(src++), src_scale) + SkAlphaMulQ(*dst, dst_scale);
            dst += 1;
        }
#else
        do {
            *dst = SkAlphaMulQ(*src, src_scale) + SkAlphaMulQ(*dst, dst_scale);
            src += 1;
            dst += 1;
        } while (--count > 0);
#endif
    }
}

static void S32A_Opaque_BlitRow32(SkPMColor* SK_RESTRICT dst,
                                  const SkPMColor* SK_RESTRICT src,
                                  int count, U8CPU alpha) {
    SkASSERT(255 == alpha);
    if (count > 0) {
#ifdef UNROLL
        if (count & 1) {
            *dst = SkPMSrcOver(*(src++), *dst);
            dst += 1;
            count -= 1;
        }

        const SkPMColor* SK_RESTRICT srcEnd = src + count;
        while (src != srcEnd) {
            *dst = SkPMSrcOver(*(src++), *dst);
            dst += 1;
            *dst = SkPMSrcOver(*(src++), *dst);
            dst += 1;
        }
#else
        do {
            *dst = SkPMSrcOver(*src, *dst);
            src += 1;
            dst += 1;
        } while (--count > 0);
#endif
    }
}

static void S32A_Blend_BlitRow32(SkPMColor* SK_RESTRICT dst,
                                 const SkPMColor* SK_RESTRICT src,
                                 int count, U8CPU alpha) {
    SkASSERT(alpha <= 255);
    if (count > 0) {
#ifdef UNROLL
        if (count & 1) {
            *dst = SkBlendARGB32(*(src++), *dst, alpha);
            dst += 1;
            count -= 1;
        }

        const SkPMColor* SK_RESTRICT srcEnd = src + count;
        while (src != srcEnd) {
            *dst = SkBlendARGB32(*(src++), *dst, alpha);
            dst += 1;
            *dst = SkBlendARGB32(*(src++), *dst, alpha);
            dst += 1;
        }
#else
        do {
            *dst = SkBlendARGB32(*src, *dst, alpha);
            src += 1;
            dst += 1;
        } while (--count > 0);
#endif
    }
}

///////////////////////////////////////////////////////////////////////////////

static const SkBlitRow::Proc32 gDefault_Procs32[] = {
    S32_Opaque_BlitRow32,
    S32_Blend_BlitRow32,
    S32A_Opaque_BlitRow32,
    S32A_Blend_BlitRow32
};

SkBlitRow::Proc32 SkBlitRow::Factory32(unsigned flags) {
    SkASSERT(flags < SK_ARRAY_COUNT(gDefault_Procs32));
    // just so we don't crash
    flags &= kFlags32_Mask;

    SkBlitRow::Proc32 proc = PlatformProcs32(flags);
    if (nullptr == proc) {
        proc = gDefault_Procs32[flags];
    }
    SkASSERT(proc);
    return proc;
}

void SkBlitRow::Color32(SkPMColor dst[], const SkPMColor src[], int count, SkPMColor color) {
    switch (SkGetPackedA32(color)) {
        case   0: memmove(dst, src, count * sizeof(SkPMColor)); return;
        case 255: sk_memset32(dst, color, count);               return;
    }
    return SkOpts::blit_row_color32(dst, src, count, color);
}

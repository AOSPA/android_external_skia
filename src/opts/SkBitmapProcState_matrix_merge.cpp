
#include "SkBitmapProcState_filter.h"
#include <cutils/log.h>
#include "SkBitmapProcState.h"
#include <SkUtils.h>
#include <SkBitmapProcState_utils.h>

#if !defined(__aarch64__)

/*
 * For clamp
 */
#define TILEX_PROCF(fx, max)    SkClampMax((fx) >> 16, max)
#define TILEY_PROCF(fy, max)    SkClampMax((fy) >> 16, max)
#define TILEX_LOW_BITS(fx, max) (((fx) >> 12) & 0xF)
#define TILEY_LOW_BITS(fy, max) (((fy) >> 12) & 0xF)
#define CHECK_FOR_DECAL

/*
 * , optimize this function
 * Merage opreation in  ClampX_ClampY_nofilter_scale_neon and
 * S32_opaque_D32_nofilter_DX, in order to avoid buffer load and store
 * opreations, and get more efficient
 */
void S32_D32_decal_nofilter_scale_t(const SkPMColor* srcAddr, int fx, int dx, SkPMColor dstC[], int count)
{
    /*
     * @amlogic.com, use assemble code to optimze this branch,
     * see C code before call this function
     *
     * Note NEON is not fit for this condition, because there are many
     * look up table operation, and can not handled by NEON registers
     */
    asm volatile (
        "ldr            r12, [sp]                           \n"     // r12 = count
        "cmp            r2, #0x10000                        \n"
        "beq            S32_D32_decal_nofilter_scale_ldm    \n"     // best case
        "push           {r4 - r11, lr}                      \n"     // reserve stack
        "cmp            r2, #0                              \n"     // dx < 0?
        "blt            S32_D32_decal_nofilter_scale_worst  \n"
        "cmp            r2, #0x2000                         \n"     // gather 8 case
        "blt            S32_D32_decal_nofilter_scale_g8     \n"
        "cmp            r2, #0x4000                         \n"     // gather 4 case
        "blt            S32_D32_decal_nofilter_scale_g4     \n"     //

        /*
         * this branch is worst case
         */
    "S32_D32_decal_nofilter_scale_worst:                    \n"
        "cmp            r12, #8                             \n"     //
        "blt            S32_D32_decal_nofilter_scale_less8  \n"
        "sub            r12, #8                             \n"
    "S32_D32_decal_nofilter_scale_loop8:                    \n"
        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "lsr            r4,  r1,  #16                       \n"     // get offset
        "lsr            r5,  r5,  #16                       \n"     //
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "ldr            r4,  [r0, r4,  lsl #2]              \n"     // load data
        "ldr            r5,  [r0, r5,  lsl #2]              \n"
        "lsr            r6,  r6,  #16                       \n"     //
        "lsr            r7,  r7,  #16                       \n"     //
        "ldr            r6,  [r0, r6,  lsl #2]              \n"
        "ldr            r7,  [r0, r7,  lsl #2]              \n"

        "add            r9,  r1, r2                         \n"     // r9  = fx + 5dx
        "add            r10, r1, r2, lsl #1                 \n"     // r10 = fx + 6dx
        "add            r11, r9, r2, lsl #1                 \n"     // r11 = fx + 7dx
        "lsr            r8,  r1,  #16                       \n"     //
        "lsr            r9,  r9,  #16                       \n"     //
        "ldr            r8,  [r0, r8,  lsl #2]              \n"
        "ldr            r9,  [r0, r9,  lsl #2]              \n"
        "lsr            r10, r10, #16                       \n"     //
        "lsr            r11, r11, #16                       \n"     //
        "ldr            r10, [r0, r10, lsl #2]              \n"
        "ldr            r11, [r0, r11, lsl #2]              \n"

        "subs           r12, r12, #8                        \n"     // count -= 8
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "stmia          r3!, {r4 - r11}                     \n"
        "bge            S32_D32_decal_nofilter_scale_loop8  \n"

        /*
         * this branch is commen to each loop
         */
    "S32_D32_decal_nofilter_scale_out8:                     \n"
        "adds           r12, r12, #8                        \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"     // return

    "S32_D32_decal_nofilter_scale_less8:                    \n"
        "cmp            r12, #4                             \n"
        "blt            S32_D32_decal_nofilter_scale_less4  \n"
        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "lsr            r4,  r1,  #16                       \n"     // get offset
        "lsr            r5,  r5,  #16                       \n"     //
        "lsr            r6,  r6,  #16                       \n"     //
        "lsr            r7,  r7,  #16                       \n"     //
        "ldr            r4,  [r0, r4,  lsl #2]              \n"     // load data
        "ldr            r5,  [r0, r5,  lsl #2]              \n"
        "ldr            r6,  [r0, r6,  lsl #2]              \n"
        "ldr            r7,  [r0, r7,  lsl #2]              \n"
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "subs           r12, r12, #4                        \n"     // count -= 4
        "stmia          r3!, {r4 - r7}                      \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"

    "S32_D32_decal_nofilter_scale_less4:                    \n"
        "cmp            r12, #2                             \n"
        "blt            S32_D32_decal_nofilter_scale_less2  \n"
        "mov            r4, r1                              \n"     // r4 = fx
        "add            r5, r1, r2                          \n"     // r5 = fx + dx
        "lsr            r4, r4, #16                         \n"
        "lsr            r5, r5, #16                         \n"
        "ldr            r4, [r0, r4, lsl #2]                \n"     // load data
        "ldr            r5, [r0, r5, lsl #2]                \n"
        "subs           r12, r12, #2                        \n"     // count -= 2
        "add            r1,  r1, r2, lsl #1                 \n"     // fx += 2dx
        "stmia          r3!, {r4, r5}                       \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"

    "S32_D32_decal_nofilter_scale_less2:                    \n"
        "lsr            r1, r1, #16                         \n"
        "ldr            r4, [r0, r1, lsl #2]                \n"
        "str            r4, [r3], #4                        \n"
        "pop            {r4 - r11, pc}                      \n"

        /*
         * because dx is 0x10000, so, we can use LDM instruction now,
         * like memcpy, this is the best condition
         */
    "S32_D32_decal_nofilter_scale_ldm:                      \n"
        "lsr            r2, r1, #16                         \n"
        "add            r0, r0, r2, lsl #2                  \n"     // add offset
        "cmp            r12, #8                             \n"
        "blt            S32_D32_decal_nofilter_scale_ldm_4  \n"
    "S32_D32_decal_nofilter_scale_ldm_8:                    \n"
        "vldmia         r0!, {d0, d1, d2, d3}               \n"     // load 8 data
        "sub            r12, r12, #8                        \n"
        "pld            [r0, #128]                          \n"
        "cmp            r12, #8                             \n"
        "vstmia         r3!, {d0, d1, d2, d3}               \n"
        "bge            S32_D32_decal_nofilter_scale_ldm_8  \n"
        "cmp            r12, #0                             \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"
    "S32_D32_decal_nofilter_scale_ldm_4:                    \n"
        "cmp            r12, #4                             \n"
        "blt            S32_D32_decal_nofilter_scale_ldm_2  \n"
        "vldmia         r0!, {d0, d1}                       \n"     // load 4 data
        "subs           r12, #4                             \n"
        "vstmia         r3!, {d0, d1}                       \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"
    "S32_D32_decal_nofilter_scale_ldm_2:                    \n"
        "cmp            r12, #2                             \n"
        "itt            ge                                  \n"
        "ldmge          r0!, {r1, r2}                       \n"
        "subges         r12, #2                             \n"
        "it             ge                                  \n"
        "stmge          r3!, {r1, r2}                       \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"
        "ldr            r1, [r0], #4                        \n"
        "str            r1, [r3], #4                        \n"
        "bx             lr                                  \n"

        /*
         * dx < 0x2000, so fx add 7dx may not generate carry bits
         * we can combine 8 load
         */
    "S32_D32_decal_nofilter_scale_g8:                       \n"
        "add            lr,  r2, r2, lsl #1                 \n"     // lr = 3dx
        "add            lr,  lr, r2, lsl #2                 \n"     // lr = 7dx
        "lsl            lr,  lr, #16                        \n"
        "cmp            r12, #8                             \n"     // count >= 8
        "blt            S32_D32_decal_nofilter_scale_less8  \n"
        "sub            r12, #8                             \n"

    "S32_D32_decal_nofilter_scale_g88:                      \n"
        "adds           r8,  lr, r1, lsl #16                \n"     // r8  = fx + 7dx
        "lsr            r4,  r1, #16                        \n"
        "bcs            S32_D32_decal_nofilter_scale_g881   \n"
        "add            r4,  r0, r4, lsl #2                 \n"     // get base address
        "vld1.32        {d0[], d1[]}, [r4]                  \n"     // load same 4 data
        "vld1.32        {d2[], d3[]}, [r4]                  \n"     // load same 4 data
        "add            r1,  r1, r2, lsl #3                 \n"     // fx += 8dx
        "subs           r12, r12, #8                        \n"
        "vstmia         r3!, {q0, q1}                       \n"
        "bge            S32_D32_decal_nofilter_scale_g88    \n"
        "b              S32_D32_decal_nofilter_scale_out8   \n"

    "S32_D32_decal_nofilter_scale_g881:                     \n"
        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "lsr            r5,  r5,  #16                       \n"     //
        "lsr            r6,  r6,  #16                       \n"     //
        "lsr            r7,  r7,  #16                       \n"     //
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "ldr            r4,  [r0, r4,  lsl #2]              \n"     // load data
        "ldr            r5,  [r0, r5,  lsl #2]              \n"
        "ldr            r6,  [r0, r6,  lsl #2]              \n"
        "ldr            r7,  [r0, r7,  lsl #2]              \n"

        "add            r9,  r1, r2                         \n"     // r9  = fx + 5dx
        "add            r10, r1, r2, lsl #1                 \n"     // r10 = fx + 6dx
        "add            r11, r9, r2, lsl #1                 \n"     // r11 = fx + 7dx
        "lsr            r8,  r1,  #16                       \n"     //
        "lsr            r9,  r9,  #16                       \n"     //
        "lsr            r10, r10, #16                       \n"     //
        "lsr            r11, r11, #16                       \n"     //
        "ldr            r8,  [r0, r8,  lsl #2]              \n"
        "ldr            r9,  [r0, r9,  lsl #2]              \n"
        "ldr            r10, [r0, r10, lsl #2]              \n"
        "ldr            r11, [r0, r11, lsl #2]              \n"

        "subs           r12, r12, #8                        \n"     // count -= 8
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "stmia          r3!, {r4 - r11}                     \n"
        "bge            S32_D32_decal_nofilter_scale_g88    \n"
        "b              S32_D32_decal_nofilter_scale_out8   \n"


        /*
         * dx < 0x4000, so fx add 3dx may not generate carry bits
         * so we can combine 4 load
         */
    "S32_D32_decal_nofilter_scale_g4:                       \n"
        "ldr            lr, =0xffff                         \n"
        "add            r9,  r2, r2, lsl #1                 \n"     // r9 = 3dx
        "cmp            r12, #4                             \n"     // count >= 8
        "lsl            r9,  r9, #16                        \n"
        "blt            S32_D32_decal_nofilter_scale_less4  \n"
        "sub            r12, #4                             \n"

    "S32_D32_decal_nofilter_scale_g44:                      \n"
        "adds           r8,  r9, r1, lsl #16                \n"     // r8  = fx + 3dx
        "lsr            r4,  r1, #16                        \n"
        "bcs            S32_D32_decal_nofilter_scale_g441   \n"
        "add            r4,  r0, r4, lsl #2                 \n"     // get base address
        "vld1.32        {d0[], d1[]}, [r4]                  \n"     // load same 4 data
        "subs           r12, r12, #4                        \n"
        "add            r1,  r1, r2, lsl #2                 \n"
        "vstmia         r3!, {q0}                           \n"
        "bge            S32_D32_decal_nofilter_scale_g44    \n"
        "b              S32_D32_decal_nofilter_scale_g442   \n"

    "S32_D32_decal_nofilter_scale_g441:                     \n"
        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "lsr            r5,  r5,  #16                       \n"     //
        "lsr            r6,  r6,  #16                       \n"     //
        "lsr            r7,  r7,  #16                       \n"     //
        "ldr            r4,  [r0, r4,  lsl #2]              \n"     // load data
        "ldr            r5,  [r0, r5,  lsl #2]              \n"
        "ldr            r6,  [r0, r6,  lsl #2]              \n"
        "ldr            r7,  [r0, r7,  lsl #2]              \n"
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "subs           r12, r12, #4                        \n"     // count -= 4
        "stmia          r3!, {r4 - r7}                      \n"
        "bge            S32_D32_decal_nofilter_scale_g44    \n"

    "S32_D32_decal_nofilter_scale_g442:                     \n"
        "adds           r12, r12, #4                        \n"
        "cmp            r12, #0                             \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"
        "b              S32_D32_decal_nofilter_scale_less4  \n"
    );
}

// S32_opaque_D32_nofilter_DX_t + CLAMPX_CLAMPY_NOFILTER_SCALE_NEON
void ClampX_S32_D32_nofilter_scale_t(const     SkBitmapProcState& s,
                                     int       x,
                                     int       y,
                                     SkPMColor dstC[],
                                     int       count)
{
    if (count == 0) {
        return ;
    }

    const SkPMColor* SK_RESTRICT srcAddr = (const SkPMColor*)s.fBitmap->getPixels();
    const unsigned maxX = s.fBitmap->width() - 1;
    SkFractionalInt fx;
    {
        SkPoint pt;
        s.fInvProc(s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                SkIntToScalar(y) + SK_ScalarHalf, &pt);
        fx = SkScalarToFractionalInt(pt.fY);
        const unsigned maxY = s.fBitmap->height() - 1;
        srcAddr = (const SkPMColor*)((const char*)srcAddr +
                (TILEY_PROCF(SkFractionalIntToFixed(fx), maxY))* s.fBitmap->rowBytes());
        fx = SkScalarToFractionalInt(pt.fX);
    }

    if (0 == maxX) {
        // all of the following X values must be 0
        sk_memset32(dstC, srcAddr[0], count);
        return;
    }

    const SkFractionalInt dx = s.fInvSxFractionalInt;

#ifdef CHECK_FOR_DECAL
    // test if we don't need to apply the tile proc
    //if (can_truncate_to_fixed_for_decal(fx, dx, count, maxX)) {
    if ((unsigned)(SkFractionalIntToFixed(fx) >> 16) <= maxX &&
        (unsigned)((SkFractionalIntToFixed(fx) + SkFractionalIntToFixed(dx) * (count - 1)) >> 16) <= maxX) {
        S32_D32_decal_nofilter_scale_t(srcAddr,
                                       SkFractionalIntToFixed(fx),
                                       SkFractionalIntToFixed(dx),
                                       dstC, count);
        return;
    }
#endif

    for (int i = count >> 2; i > 0; --i) {
        *dstC++ = srcAddr[TILEX_PROCF(SkFractionalIntToFixed(fx), maxX)];
        fx += dx;
        *dstC++ = srcAddr[TILEX_PROCF(SkFractionalIntToFixed(fx), maxX)];
        fx += dx;
        *dstC++ = srcAddr[TILEX_PROCF(SkFractionalIntToFixed(fx), maxX)];
        fx += dx;
        *dstC++ = srcAddr[TILEX_PROCF(SkFractionalIntToFixed(fx), maxX)];
        fx += dx;
    }
    for (int i = count & 3; i > 0; --i) {
        *dstC++ = srcAddr[TILEX_PROCF(SkFractionalIntToFixed(fx), maxX)];
        fx += dx;
    }
}

void S16_D32_decal_nofilter_scale_t(uint16_t *srcAddr, int fx, int dx, SkPMColor dstC[], int count)
{
    /*
     * @amlogic.com, use assemble code to optimze this branch,
     * see C code before call this function
     */
    asm volatile (
        "ldr            r12, [sp]                           \n"     // r12 = count
        "vmov.i8        d31, #0xff                          \n"     // d31 = [ff ff ff ff ff ff ff ff]
        "cmp            r2, #0x10000                        \n"
        "beq            S16_D32_decal_nofilter_scale_ldm    \n"     // best case
        "push           {r4 - r11, lr}                      \n"     // reserve stack
        "ldr            lr, =0x1fffe                        \n"     // lr as mask
        "bgt            S16_D32_decal_nofilter_scale_worst  \n"
        "cmp            r2, #0x0                            \n"     // dx < 0?
        "blt            S16_D32_decal_nofilter_scale_worst  \n"
        "cmp            r2, #0x2000                         \n"     // gather 8 case
        "ble            S16_D32_decal_nofilter_scale_g8     \n"
        "bgt            S16_D32_decal_nofilter_scale_g2     \n"     //

        /*
         * this branch is worst case
         */
    "S16_D32_decal_nofilter_scale_worst:                    \n"
        "cmp            r12, #8                             \n"     //
        "blt            S16_D32_decal_nofilter_scale_less8  \n"
        "sub            r12, #8                             \n"

        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "and            r4,  lr, r1, lsr #15                \n"
        "and            r5,  lr, r5, lsr #15                \n"
        "and            r6,  lr, r6, lsr #15                \n"
        "and            r7,  lr, r7, lsr #15                \n"

    "S16_D32_decal_nofilter_scale_loop8:                    \n"
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "ldrh           r4,  [r0, r4]                       \n"     // load data
        "ldrh           r5,  [r0, r5]                       \n"
        "ldrh           r6,  [r0, r6]                       \n"
        "ldrh           r7,  [r0, r7]                       \n"

        "add            r9,  r1,  r2                        \n"     // r9  = fx + 5dx
        "add            r10, r1,  r2,  lsl #1               \n"     // r10 = fx + 6dx
        "add            r11, r9,  r2,  lsl #1               \n"     // r11 = fx + 7dx
        "and            r8,  lr,  r1,  lsr #15              \n"     //
        "and            r9,  lr,  r9,  lsr #15              \n"     //
        "and            r10, lr,  r10, lsr #15              \n"     //
        "and            r11, lr,  r11, lsr #15              \n"     //
        "ldrh           r9,  [r0, r9]                       \n"
        "ldrh           r8,  [r0, r8]                       \n"
        "ldrh           r11, [r0, r11]                      \n"
        "ldrh           r10, [r0, r10]                      \n"
        "orr            r4,  r4,  r5,  lsl #16              \n"
        "orr            r5,  r6,  r7,  lsl #16              \n"
        "orr            r6,  r8,  r9,  lsl #16              \n"
        "orr            r7,  r10, r11, lsl #16              \n"
        "vmov           d0,  r4,  r5                        \n"
        "vmov           d1,  r6,  r7                        \n"
        "add            r1,  r1,  r2, lsl #2                \n"
        "add            r5,  r1,  r2                        \n"     // r5  = fx +  dx
        "vshl.i16       q1,  q0,  #5                        \n"     // channel G
        "vshl.i16       q2,  q0,  #11                       \n"     // channel B
        "subs           r12, r12, #8                        \n"     // count -= 8
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "vshrn.i16      d28, q0,  #8                        \n"     // channel R
        "vshrn.i16      d29, q1,  #8                        \n"
        "vshrn.i16      d30, q2,  #8                        \n"
        "and            r4,  lr, r1, lsr #15                \n"
        "and            r5,  lr, r5, lsr #15                \n"
        "and            r6,  lr, r6, lsr #15                \n"
        "and            r7,  lr, r7, lsr #15                \n"
        "vst4.8         {d28, d29, d30, d31}, [r3]!         \n"
        "bge            S16_D32_decal_nofilter_scale_loop8  \n"

        /*
         * this branch is commen to each loop
         */
    "S16_D32_decal_nofilter_scale_out8:                     \n"
        "adds           r12, r12, #8                        \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"     // return

    "S16_D32_decal_nofilter_scale_less8:                    \n"
        "cmp            r12, #4                             \n"
        "blt            S16_D32_decal_nofilter_scale_less4  \n"
        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "and            r4,  lr, r1, lsr #15                \n"
        "and            r5,  lr, r5, lsr #15                \n"
        "and            r6,  lr, r6, lsr #15                \n"
        "and            r7,  lr, r7, lsr #15                \n"
        "ldrh           r4,  [r0, r4]                       \n"     // load data
        "ldrh           r5,  [r0, r5]                       \n"
        "ldrh           r7,  [r0, r7]                       \n"
        "ldrh           r6,  [r0, r6]                       \n"
        "orr            r4,  r4,  r5,  lsl #16              \n"
        "orr            r5,  r6,  r7,  lsl #16              \n"
        "vmov           d0,  r4,  r5                        \n"
        "vshl.i16       q1,  q0,  #5                        \n"
        "vshl.i16       q2,  q0,  #11                       \n"
        "vshrn.i16      d28, q0,  #8                        \n"
        "vshrn.i16      d29, q1,  #8                        \n"
        "vshrn.i16      d30, q2,  #8                        \n"
        "subs           r12, r12, #4                        \n"     // count -= 4
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "vst4.8        {d28[0], d29[0], d30[0], d31[0]}, [r3]!  \n"
        "vst4.8        {d28[1], d29[1], d30[1], d31[1]}, [r3]!  \n"
        "vst4.8        {d28[2], d29[2], d30[2], d31[2]}, [r3]!  \n"
        "vst4.8        {d28[3], d29[3], d30[3], d31[3]}, [r3]!  \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"

    "S16_D32_decal_nofilter_scale_less4:                    \n"
        "cmp            r12, #2                             \n"
        "blt            S16_D32_decal_nofilter_scale_less2  \n"
        "mov            r4, r1                              \n"     // r4 = fx
        "add            r5, r1, r2                          \n"     // r5 = fx + dx
        "and            r4,  lr, r1, lsr #15                \n"
        "and            r5,  lr, r5, lsr #15                \n"
        "ldr            r8, =0x00ff00ff                     \n"     // mask
        "ldrh           r5,  [r0, r5]                       \n"
        "ldrh           r4,  [r0, r4]                       \n"
        "subs           r12, r12, #2                        \n"     // count -= 2
        "add            r1,  r1, r2, lsl #1                 \n"     // fx += 2dx
        "orr            r4,  r4, r5, lsl #16                \n"
        "and            r6,  r8, r4, LSR #8                 \n"
        "and            r7,  r8, r4, LSR #3                 \n"
        "and            r4,  r8, r4, LSL #3                 \n"
        "orr            r6,  r6, r7, LSL #8                 \n"
        "orr            r4,  r4, r8, LSL #8                 \n"
        "pkhtb          r7,  r4, r6, ASR #16                \n"
        "pkhbt          r6,  r6, r4, LSL #16                \n"
        "stmia          r3!, {r6, r7}                       \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"

    "S16_D32_decal_nofilter_scale_less2:                    \n"
        "and            r1, lr, r1, lsr #15                 \n"
        "ldrh           r4, [r0, r1]                        \n"
        "mov            r8, #0xff000000                     \n"
        "and            r5, r4, #0x7e0                      \n"
        "and            r6, r4, #0xf800                     \n"
        "and            r7, r4, #0x1f                       \n"
        "add            r5, r8, r5, LSL #5                  \n"
        "add            r5, r5, r6, LSR #8                  \n"
        "add            r5, r5, r7, LSL #19                 \n"
        "str            r5, [r3], #4                        \n"
        "pop            {r4 - r11, pc}                      \n"

        /*
         * because dx is 0x10000, so, we can use LDM instruction now,
         * this is the best condition
         */
    "S16_D32_decal_nofilter_scale_ldm:                      \n"
        "lsr            r2, r1, #16                         \n"
        "add            r0, r0, r2, lsl #1                  \n"     // add offset
        "cmp            r12, #16                            \n"
        "blt            S16_D32_decal_nofilter_scale_ldm_8  \n"
        "vmov.i8        d25, #0xff                          \n"
    "S16_D32_decal_nofilter_scale_ldm_16:                   \n"
        "vld1.16        {d0, d1, d2, d3}, [r0]!             \n"     // load 8 data
        "sub            r12, r12, #16                       \n"
        "pld            [r0, #128]                          \n"
        "cmp            r12, #16                            \n"
        "vshl.i16       q2,  q0,  #5                        \n"     // channel G first 8 data
        "vshl.i16       q3,  q0,  #11                       \n"     // channel B second 8 data
        "vshl.i16       q9,  q1,  #5                        \n"
        "vshl.i16       q10, q1,  #11                       \n"
        "vshrn.i16      d28, q0,  #8                        \n"     // channel R
        "vshrn.i16      d29, q2,  #8                        \n"     // channel G
        "vshrn.i16      d30, q3,  #8                        \n"     // channel B
        "vshrn.i16      d22, q1,  #8                        \n"
        "vshrn.i16      d23, q9,  #8                        \n"
        "vshrn.i16      d24, q10, #8                        \n"
        "vst4.8         {d28 - d31}, [r3]!                  \n"     // store data
        "vst4.8         {d22 - d25}, [r3]!                  \n"
        "bge            S16_D32_decal_nofilter_scale_ldm_16 \n"
        "cmp            r12, #0                             \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"

    "S16_D32_decal_nofilter_scale_ldm_8:                    \n"
        "cmp            r12, #8                             \n"
        "blt            S16_D32_decal_nofilter_scale_ldm_4  \n"
        "vld1.16        {d0, d1}, [r0]!                     \n"
        "subs           r12, #8                             \n"
        "vshl.i16       q2,  q0, #5                         \n"
        "vshl.i16       q3,  q0, #11                        \n"
        "vshrn.i16      d28, q0, #8                         \n"
        "vshrn.i16      d29, q2, #8                         \n"
        "vshrn.i16      d30, q3, #8                         \n"
        "vst4.8         {d28 - d31}, [r3]!                  \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"

    "S16_D32_decal_nofilter_scale_ldm_4:                    \n"
        "cmp            r12, #4                             \n"
        "blt            S16_D32_decal_nofilter_scale_ldm_2  \n"
        "vld1.16        {d0}, [r0]!                         \n"     // load 4 data
        "subs           r12, #4                             \n"
        "vshl.i16       q2,  q0, #5                         \n"
        "vshl.i16       q3,  q0, #11                        \n"
        "vshrn.i16      d28, q0, #8                         \n"
        "vshrn.i16      d29, q2, #8                         \n"
        "vshrn.i16      d30, q3, #8                         \n"
        "vst4.8        {d28[0], d29[0], d30[0], d31[0]}, [r3]!  \n"
        "vst4.8        {d28[1], d29[1], d30[1], d31[1]}, [r3]!  \n"
        "vst4.8        {d28[2], d29[2], d30[2], d31[2]}, [r3]!  \n"
        "vst4.8        {d28[3], d29[3], d30[3], d31[3]}, [r3]!  \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"

    "S16_D32_decal_nofilter_scale_ldm_2:                    \n"
        "cmp            r12, #2                             \n"
        "blt            S16_D32_decal_nofilter_scale_ldm_1  \n"
        "ldr            r1, [r0], #4                        \n"
        "push           {r4, lr}                            \n"
        "ldr            lr, =0x00ff00ff                     \n"
        "subs           r12, #2                             \n"
        "and            r2, lr, r1, lsr #8                  \n"     // r
        "and            r4, lr, r1, lsr #3                  \n"     // g
        "and            r1, lr, r1, lsl #3                  \n"     // b
        "orr            r2, r2, r4, lsl #8                  \n"     // gr
        "orr            r1, r1, lr, lsl #8                  \n"     // ab
        "pkhtb          r4, r1, r2, ASR #16                 \n"     // abgr
        "pkhbt          r2, r2, r1, LSL #16                 \n"     // abgr
        "stmia          r3!, {r2, r4}                       \n"
        "pop            {r4, lr}                            \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"

    "S16_D32_decal_nofilter_scale_ldm_1:                    \n"
        "ldrh           r1, [r0], #2                        \n"
        "mov            r0, #0xff000000                     \n"     // a
        "and            r12, r1, #0x7e0                     \n"     // g
        "and            r2,  r1, #0xf800                    \n"     // r
        "and            r1,  r1, #0x1f                      \n"     // b
        "add            r12, r0,  r12, lsl #5               \n"     // a.g.
        "add            r12, r12, r2,  lsr #8               \n"     // a.gr
        "add            r12, r12, r1,  lsl #19              \n"     // abgr
        "str            r12, [r3], #4                       \n"
        "bx             lr                                  \n"

        /*
         * dx < 0x2000, so fx add 7dx may not generate carry bits
         * we can combine 8 load
         */
    "S16_D32_decal_nofilter_scale_g8:                       \n"
        "add            r11,  r2,  r2, lsl #1               \n"     // r11 = 3dx
        "add            r11,  r11, r2, lsl #2               \n"     // r11 = 7dx
        "cmp            r12, #8                             \n"     // count >= 8
        "lsl            r11, r11, #16                       \n"     // r11 = 7dx high bits
        "blt            S16_D32_decal_nofilter_scale_less8  \n"
        "sub            r12, #8                             \n"

    "S16_D32_decal_nofilter_scale_g88:                      \n"
        "adds           r8,  r11, r1, lsl #16               \n"     // r8  = fx + 7dx
        "and            r4,  lr,  r1, lsr #15               \n"
        "bcs            S16_D32_decal_nofilter_scale_g881   \n"     // got carry
        "add            r4,  r0, r4                         \n"     // get base address
        "vld1.16       {d0[], d1[]}, [r4]                   \n"     // load same 8 data
        "add            r1,  r1, r2, lsl #3                 \n"     // fx += 8dx
        "subs           r12, r12, #8                        \n"
        "vshl.i16       q2,  q0, #5                         \n"
        "vshl.i16       q3,  q0, #11                        \n"
        "vshrn.i16      d28, q0, #8                         \n"
        "vshrn.i16      d29, q2, #8                         \n"
        "vshrn.i16      d30, q3, #8                         \n"
        "vst4.8         {d28 - d31}, [r3]!                  \n"
        "bge            S16_D32_decal_nofilter_scale_g88    \n"
        "b              S16_D32_decal_nofilter_scale_out8   \n"

    "S16_D32_decal_nofilter_scale_g881:                     \n"
        "add            r5,  r1,  r2                        \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "and            r5,  lr, r5, lsr #15                \n"
        "and            r6,  lr, r6, lsr #15                \n"
        "and            r7,  lr, r7, lsr #15                \n"
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "ldrh           r4,  [r0, r4]                       \n"     // load data
        "ldrh           r5,  [r0, r5]                       \n"
        "ldrh           r6,  [r0, r6]                       \n"
        "ldrh           r7,  [r0, r7]                       \n"

        "and            r8,  lr,  r1,  lsr #15              \n"     //
        "add            r9,  r1,  r2                        \n"     // r9  = fx + 5dx
        "add            r10, r1,  r2,  lsl #1               \n"     // r10 = fx + 6dx
        "orr            r4,  r4,  r5,  lsl #16              \n"
        "orr            r5,  r6,  r7,  lsl #16              \n"
        "vmov           d0,  r4,  r5                        \n"
        "add            r6,  r9,  r2,  lsl #1               \n"     // r6  = fx + 7dx
        "and            r9,  lr,  r9,  lsr #15              \n"     //
        "and            r10, lr,  r10, lsr #15              \n"     //
        "and            r6,  lr,  r6,  lsr #15              \n"     //
        "ldrh           r9,  [r0, r9]                       \n"
        "ldrh           r8,  [r0, r8]                       \n"
        "ldrh           r6,  [r0, r6]                       \n"
        "ldrh           r10, [r0, r10]                      \n"
        "orr            r7,  r8,  r9,  lsl #16              \n"
        "orr            r6,  r10, r6,  lsl #16              \n"
        "vmov           d1,  r7,  r6                        \n"
        "add            r1,  r1,  r2, lsl #2                \n"
        "vshl.i16       q1,  q0,  #5                        \n"     // channel G
        "vshl.i16       q2,  q0,  #11                       \n"     // channel B
        "subs           r12, r12, #8                        \n"     // count -= 8
        "vshrn.i16      d28, q0,  #8                        \n"     // channel R
        "vshrn.i16      d29, q1,  #8                        \n"
        "vshrn.i16      d30, q2,  #8                        \n"
        "vst4.8         {d28, d29, d30, d31}, [r3]!         \n"
        "bge            S16_D32_decal_nofilter_scale_g88    \n"
        "b              S16_D32_decal_nofilter_scale_out8   \n"


        /*
         * 0x2000 <= dx < 0x10000, fit for quardrant, ^_^
         */
    "S16_D32_decal_nofilter_scale_g2:                       \n"
        "lsl            r11, r2, #2                         \n"     // r11 = 4dx;
        "vdup.32        q10, r11                            \n"     // q10 = 4dx;
        "lsl            r11, r11, #1                        \n"     // r11 = 8dx
        "vdup.16        d27, r11                            \n"     // d27 = 8dx
        "vdup.16        d26, r1                             \n"
        "lsl            r10, r2, #1                         \n"     // r10 = 2dx;
        "add            r9,  r2, r2, lsl #1                 \n"     // r9  = 3dx;
        "vmov           d29, r10, r9                        \n"     // d29 = [3dx, 2dx]
        "mov            r8,  #0                             \n"
        "vmov           d28, r8, r2                         \n"     // d28 = [dx,  0]
        "vadd.i32       q10, q10, q14                       \n"     // q10 = [7dx,6dx,5dx,4dx]
        "vmov.i16       q9, #1                              \n"
        "cmp            r12, #8                             \n"     // count >= 8
        "blt            S16_D32_decal_nofilter_scale_less8  \n"
        "sub            r12, #8                             \n"
        "vmovl.u16      q12, d26                            \n"     //
        "vmov.i8        d7,  #0xff                          \n"

    "S16_D32_decal_nofilter_scale_g28:                      \n"
        "vadd.i32       q11, q12, q14                       \n"
        "vadd.i32       q12, q12, q10                       \n"
        "vadd.i16       d26, d26, d27                       \n"     // add 8dx
        "and            r8, lr, r1, lsr #15                 \n"
        "add            r11, r8, r0                         \n"
        "vuzp.16        q11, q12                            \n"
        "vld1.16        {d16, d17}, [r11]                   \n"     // load 8 data
        "add            r1, r1, r2, lsl #3                  \n"     // fx += 8dx
        "vshl.i16       q12, q12, #1                        \n"
        "vadd.i16       q11, q12, q9                        \n"
        "vmovn.i16      d24, q12                            \n"     // index
        "vmovn.i16      d25, q11                            \n"
        "vtbl.8         d0, {d16, d17}, d24                 \n"
        "vtbl.8         d1, {d16, d17}, d25                 \n"
        "vzip.8         d0, d1                              \n"

        "subs           r12, #8                             \n"
        "add            r8,  r8,  #64                       \n"
        "vshl.i16       q2,  q0,  #11                       \n"     // channel B
        "vshl.i16       q1,  q0,  #5                        \n"     // channel G
        "pld            [r0, r8]                            \n"
        "vshrn.i16      d6,  q2,  #8                        \n"
        "vshrn.i16      d4,  q0,  #8                        \n"
        "vshrn.i16      d5,  q1,  #8                        \n"
        "vmovl.u16      q12, d26                            \n"     //
        "vst4.8         {d4, d5, d6, d7}, [r3]!             \n"
        "bge            S16_D32_decal_nofilter_scale_g28    \n"
        "b              S16_D32_decal_nofilter_scale_out8   \n"
    );
}

#include "SkColorPriv.h"
// S16_opaque_D32_nofilter_DX_t + CLAMPX_CLAMPY_NOFILTER_SCALE_NEON
void ClampX_S16_D32_nofilter_scale_t(const     SkBitmapProcState& s,
                                     int       x,
                                     int       y,
                                     SkPMColor dstC[],
                                     int       count)
{
    if (count == 0) {
        return ;
    }

    uint16_t* SK_RESTRICT srcAddr = (uint16_t *)s.fBitmap->getPixels();
    const unsigned maxX = s.fBitmap->width() - 1;
    SkFixed fx;
    {
        SkPoint pt;
        s.fInvProc(s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                                  SkIntToScalar(y) + SK_ScalarHalf, &pt);
        fx = SkScalarToFixed(pt.fY);
        const unsigned maxY = s.fBitmap->height() - 1;
        srcAddr = (uint16_t *)((char*)srcAddr +
                   TILEY_PROCF(fx, maxY) * s.fBitmap->rowBytes());
        fx = SkScalarToFixed(pt.fX);
    }

    if (0 == maxX) {
        sk_memset32(dstC, SkPixel16ToPixel32(srcAddr[0]), count);
        return;
    }

    const SkFixed dx = s.fInvSx;

    //LOGD(">>>> fx:%8x, dx:%8x, cnt:%3d, mla:%8x", fx, dx, count, fx + dx * count);
    int i;
#ifdef CHECK_FOR_DECAL
    // test if we don't need to apply the tile proc
    if ((unsigned)(fx >> 16) <= maxX &&
        (unsigned)((fx + dx * (count - 1)) >> 16) <= maxX) {
        S16_D32_decal_nofilter_scale_t(srcAddr, fx, dx, dstC, count);
        return;
    }
#endif

    /*
     * this branch hardly appear
     */
    for (i = count >> 2; i > 0; --i) {
        *dstC++ = SkPixel16ToPixel32(srcAddr[TILEX_PROCF(fx, maxX)]); fx += dx;
        *dstC++ = SkPixel16ToPixel32(srcAddr[TILEX_PROCF(fx, maxX)]); fx += dx;
        *dstC++ = SkPixel16ToPixel32(srcAddr[TILEX_PROCF(fx, maxX)]); fx += dx;
        *dstC++ = SkPixel16ToPixel32(srcAddr[TILEX_PROCF(fx, maxX)]); fx += dx;
    }
    for (i = count & 3; i > 0; --i) {
        *dstC++ = SkPixel16ToPixel32(srcAddr[TILEX_PROCF(fx, maxX)]); fx += dx;
    }
}

// SI8_opaque_D32_nofilter_DX + ClampX_ClampY_nofilter_scale_neon
void ClampX_SI8_D32_nofilter_scale_t(const     SkBitmapProcState& s,
                                     int       x,
                                     int       y,
                                     SkPMColor dstC[],
                                     int       count)
{
    if (count == 0) {
        return ;
    }

    const SkPMColor* SK_RESTRICT table = s.fBitmap->getColorTable()->readColors();      // PREAMBLE(s)
    uint8_t* SK_RESTRICT srcAddr = (uint8_t *)s.fBitmap->getPixels();
    const unsigned maxX = s.fBitmap->width() - 1;
    SkFixed fx;
    {
        SkPoint pt;
        s.fInvProc(s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                                  SkIntToScalar(y) + SK_ScalarHalf, &pt);
        fx = SkScalarToFixed(pt.fY);
        const unsigned maxY = s.fBitmap->height() - 1;
        srcAddr = (uint8_t *)((char*)srcAddr +
                   TILEY_PROCF(fx, maxY) * s.fBitmap->rowBytes());
        fx = SkScalarToFixed(pt.fX);
    }

    const SkFixed dx = s.fInvSx;

    if (0 == maxX) {
        sk_memset32(dstC, table[srcAddr[0]], count);
        return;
    }

    int i;
#ifdef CHECK_FOR_DECAL
    // test if we don't need to apply the tile proc
    if ((unsigned)(fx >> 16) <= maxX &&
        (unsigned)((fx + dx * (count - 1)) >> 16) <= maxX) {
    #if 1
        for (i = count >> 2; i > 0; --i) {
            *dstC++ = table[srcAddr[fx >> 16]]; fx += dx;
            *dstC++ = table[srcAddr[fx >> 16]]; fx += dx;
            *dstC++ = table[srcAddr[fx >> 16]]; fx += dx;
            *dstC++ = table[srcAddr[fx >> 16]]; fx += dx;
        }
        for (i = count & 3; i > 0; --i) {
            *dstC++ = table[srcAddr[fx >> 16]]; fx += dx;
        }
    #else
        // need to add optimize code when count is large enough
    #endif
        return;
    }
#endif

    /*
     * this branch hardly appear
     */
    for (i = count >> 2; i > 0; --i) {
        *dstC++ = table[srcAddr[TILEX_PROCF(fx, maxX)]]; fx += dx;
        *dstC++ = table[srcAddr[TILEX_PROCF(fx, maxX)]]; fx += dx;
        *dstC++ = table[srcAddr[TILEX_PROCF(fx, maxX)]]; fx += dx;
        *dstC++ = table[srcAddr[TILEX_PROCF(fx, maxX)]]; fx += dx;
    }
    for (i = count & 3; i > 0; --i) {
        *dstC++ = table[srcAddr[TILEX_PROCF(fx, maxX)]]; fx += dx;
    }
}

void S32A_D32_nofilter_scale_t(uint32_t *srcAddr, int fx, int dx, SkPMColor dstC[], int count)
{
    // alphaScale is stored in d31
    asm volatile (
        "ldr            r12, [sp]                           \n"     // r12 = count
        "cmp            r2, #0x10000                        \n"
        "beq            S32A_D32_decal_nofilter_scale_ldm   \n"     // best case
        "push           {r4 - r11, lr}                      \n"     // reserve stack
        "cmp            r2, #0                              \n"     // r2 < 0
        "blt            S32A_D32_decal_nofilter_scale_worst \n"
        "cmp            r2, #0x2000                         \n"     // gather 8 case
        "blt            S32A_D32_decal_nofilter_scale_g8    \n"

        /*
         * this branch is worst case
         */
    "S32A_D32_decal_nofilter_scale_worst:                   \n"
        "cmp            r12, #8                             \n"     //
        "blt            S32A_D32_decal_nofilter_scale_less8 \n"
        "sub            r12, #8                             \n"
        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
    "S32A_D32_decal_nofilter_scale_loop8:                   \n"
        "lsr            r4,  r1,  #16                       \n"     // get offset
        "lsr            r5,  r5,  #16                       \n"     //
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "ldr            r4,  [r0, r4,  lsl #2]              \n"     // load data
        "ldr            r5,  [r0, r5,  lsl #2]              \n"
        "lsr            r6,  r6,  #16                       \n"     //
        "lsr            r7,  r7,  #16                       \n"     //
        "ldr            r6,  [r0, r6,  lsl #2]              \n"
        "ldr            r7,  [r0, r7,  lsl #2]              \n"

        "add            r9,  r1, r2                         \n"     // r9  = fx + 5dx
        "add            r10, r1, r2, lsl #1                 \n"     // r10 = fx + 6dx
        "add            r11, r9, r2, lsl #1                 \n"     // r11 = fx + 7dx
        "lsr            r8,  r1,  #16                       \n"     //
        "lsr            r9,  r9,  #16                       \n"     //
        "ldr            r8,  [r0, r8,  lsl #2]              \n"
        "ldr            r9,  [r0, r9,  lsl #2]              \n"
        "lsr            r10, r10, #16                       \n"     //
        "lsr            r11, r11, #16                       \n"     //
        "ldr            r10, [r0, r10, lsl #2]              \n"
        "ldr            r11, [r0, r11, lsl #2]              \n"

        "vmov           d0, r4, r5                          \n"
        "vmov           d1, r6, r7                          \n"
        "vmov           d2, r8, r9                          \n"
        "vmov           d3, r10, r11                        \n"
        "vmull.u8       q2, d0, d31                         \n"     // alpha scale
        "vmull.u8       q3, d1, d31                         \n"
        "vmull.u8       q8, d2, d31                         \n"
        "vmull.u8       q9, d3, d31                         \n"
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "subs           r12, r12, #8                        \n"     // count -= 8
        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "vshrn.i16      d0, q2, #8                          \n"     // shift back
        "vshrn.i16      d1, q3, #8                          \n"     // shift back
        "vshrn.i16      d2, q8, #8                          \n"     // shift back
        "vshrn.i16      d3, q9, #8                          \n"     // shift back
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "vstmia         r3!, {d0, d1, d2, d3}               \n"
        "bge            S32A_D32_decal_nofilter_scale_loop8 \n"

        /*
         * this branch is commen to each loop
         */
    "S32A_D32_decal_nofilter_scale_out8:                    \n"
        "adds           r12, r12, #8                        \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"     // return

    "S32A_D32_decal_nofilter_scale_less8:                   \n"
        "cmp            r12, #4                             \n"
        "blt            S32A_D32_decal_nofilter_scale_less4 \n"
        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "lsr            r4,  r1,  #16                       \n"     // get offset
        "lsr            r5,  r5,  #16                       \n"     //
        "lsr            r6,  r6,  #16                       \n"     //
        "lsr            r7,  r7,  #16                       \n"     //
        "ldr            r4,  [r0, r4,  lsl #2]              \n"     // load data
        "ldr            r5,  [r0, r5,  lsl #2]              \n"
        "ldr            r6,  [r0, r6,  lsl #2]              \n"
        "ldr            r7,  [r0, r7,  lsl #2]              \n"
        "vmov           d0, r4, r5                          \n"
        "vmov           d1, r6, r7                          \n"
        "vmull.u8       q2, d0, d31                         \n"     // alpha scale
        "vmull.u8       q3, d1, d31                         \n"
        "vshrn.i16      d0, q2, #8                          \n"     // shift back
        "vshrn.i16      d1, q3, #8                          \n"     // shift back
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "subs           r12, r12, #4                        \n"     // count -= 4
        "vstmia         r3!, {d0, d1}                       \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"

    "S32A_D32_decal_nofilter_scale_less4:                   \n"
        "cmp            r12, #2                             \n"
        "blt            S32A_D32_decal_nofilter_scale_less2 \n"
        "mov            r4, r1                              \n"     // r4 = fx
        "add            r5, r1, r2                          \n"     // r5 = fx + dx
        "lsr            r4, r4, #16                         \n"
        "lsr            r5, r5, #16                         \n"
        "ldr            r4, [r0, r4, lsl #2]                \n"     // load data
        "ldr            r5, [r0, r5, lsl #2]                \n"
        "vmov           d0, r4, r5                          \n"
        "vmull.u8       q2, d0, d31                         \n"     // alpha scale
        "subs           r12, r12, #2                        \n"     // count -= 2
        "vshrn.i16      d0, q2, #8                          \n"     // shift back
        "add            r1,  r1, r2, lsl #1                 \n"     // fx += 2dx
        "vstmia         r3!, {d0}                           \n"
        "it             eq                                  \n"
        "popeq          {r4 - r11, pc}                      \n"

    "S32A_D32_decal_nofilter_scale_less2:                   \n"
        "lsr            r1, r1, #16                         \n"
        "ldr            r4, [r0, r1, lsl #2]                \n"
        "vmov           d0, r4, r5                          \n"
        "vmull.u8       q2, d0, d31                         \n"     // alpha scale
        "vshrn.i16      d0, q2, #8                          \n"     // shift back
        "vst1.32        {d0[0]}, [r3]!                      \n"
        "pop            {r4 - r11, pc}                      \n"

        /*
         * because dx is 0x10000, so, we can use LDM instruction now,
         * like memcpy, this is the best condition
         */
    "S32A_D32_decal_nofilter_scale_ldm:                     \n"
        "lsr            r2, r1, #16                         \n"
        "add            r0, r0, r2, lsl #2                  \n"     // add offset
        "cmp            r12, #8                             \n"
        "blt            S32A_D32_decal_nofilter_scale_ldm_4 \n"
    "S32A_D32_decal_nofilter_scale_ldm_8:                   \n"
        "vldmia         r0!, {d0, d1, d2, d3}               \n"     // load 8 data
        "sub            r12, r12, #8                        \n"
        "pld            [r0, #128]                          \n"
        "vmull.u8       q2, d0, d31                         \n"     // alpha scale
        "vmull.u8       q3, d1, d31                         \n"
        "vmull.u8       q8, d2, d31                         \n"
        "vmull.u8       q9, d3, d31                         \n"
        "cmp            r12, #8                             \n"
        "vshrn.i16      d0, q2, #8                          \n"     // shift back
        "vshrn.i16      d1, q3, #8                          \n"     // shift back
        "vshrn.i16      d2, q8, #8                          \n"     // shift back
        "vshrn.i16      d3, q9, #8                          \n"     // shift back
        "vstmia         r3!, {d0, d1, d2, d3}               \n"
        "bge            S32A_D32_decal_nofilter_scale_ldm_8 \n"
        "cmp            r12, #0                             \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"
    "S32A_D32_decal_nofilter_scale_ldm_4:                   \n"
        "cmp            r12, #4                             \n"
        "blt            S32A_D32_decal_nofilter_scale_ldm_2 \n"
        "vldmia         r0!, {d0, d1}                       \n"     // load 4 data
        "subs           r12, #4                             \n"
        "vmull.u8       q2, d0, d31                         \n"     // alpha scale
        "vmull.u8       q3, d1, d31                         \n"
        "vshrn.i16      d0, q2, #8                          \n"     // shift back
        "vshrn.i16      d1, q3, #8                          \n"     // shift back
        "vstmia         r3!, {d0, d1}                       \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"
    "S32A_D32_decal_nofilter_scale_ldm_2:                   \n"
        "cmp            r12, #2                             \n"
        "blt            S32A_D32_decal_nofilter_scale_ldm_1 \n"
        "vldmia         r0!, {d0}                           \n"
        "subs           r12, #2                             \n"
        "vmull.u8       q2, d0, d31                         \n"
        "vshrn.i16      d0, q2, #8                          \n"
        "vstmia         r3!, {d0}                           \n"
        "it             eq                                  \n"
        "bxeq           lr                                  \n"
    "S32A_D32_decal_nofilter_scale_ldm_1:                   \n"
        "vld1.32        {d0[0]}, [r0]!                      \n"
        "vmull.u8       q2, d0, d31                         \n"
        "vshrn.i16      d0, q2, #8                          \n"
        "vst1.32        {d0[0]}, [r3]!                      \n"
        "bx             lr                                  \n"

        /*
         * dx < 0x2000, so fx add 7dx may not generate carry bits
         * we can combine 8 load
         */
    "S32A_D32_decal_nofilter_scale_g8:                      \n"
        "add            lr,  r2, r2, lsl #1                 \n"     // lr = 3dx
        "add            lr,  lr, r2, lsl #2                 \n"     // lr = 7dx
        "lsl            lr,  lr, #16                        \n"
        "cmp            r12, #8                             \n"     // count >= 8
        "blt            S32A_D32_decal_nofilter_scale_less8 \n"
        "sub            r12, #8                             \n"

    "S32A_D32_decal_nofilter_scale_g88:                     \n"
        "adds           r8,  lr, r1, lsl #16                \n"     // r8  = fx + 7dx
        "lsr            r4,  r1, #16                        \n"
        "bcs            S32A_D32_decal_nofilter_scale_g881  \n"
        "add            r4,  r0, r4, lsl #2                 \n"     // get base address
        "vld4.8         {d0[], d1[], d2[], d3[]}, [r4]      \n"     // load same 4 data
        "add            r1,  r1, r2, lsl #3                 \n"     // fx += 8dx
        "vmull.u8       q2, d0, d31                         \n"     // alpha scale
        "vmull.u8       q3, d1, d31                         \n"
        "vmull.u8       q8, d2, d31                         \n"
        "vmull.u8       q9, d3, d31                         \n"
        "vshrn.i16      d0, q2, #8                          \n"     // shift back
        "vshrn.i16      d1, q3, #8                          \n"     // shift back
        "vshrn.i16      d2, q8, #8                          \n"     // shift back
        "vshrn.i16      d3, q9, #8                          \n"     // shift back
        "subs           r12, r12, #8                        \n"
        "vst4.8         {d0, d1, d2, d3}, [r3]!             \n"
        "bge            S32A_D32_decal_nofilter_scale_g88   \n"
        "b              S32A_D32_decal_nofilter_scale_out8  \n"

    "S32A_D32_decal_nofilter_scale_g881:                    \n"
        "add            r5,  r1, r2                         \n"     // r5  = fx +  dx
        "add            r6,  r1, r2, lsl #1                 \n"     // r6  = fx + 2dx
        "add            r7,  r5, r2, lsl #1                 \n"     // r7  = fx + 3dx
        "lsr            r5,  r5,  #16                       \n"     //
        "lsr            r6,  r6,  #16                       \n"     //
        "lsr            r7,  r7,  #16                       \n"     //
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "ldr            r4,  [r0, r4,  lsl #2]              \n"     // load data
        "ldr            r5,  [r0, r5,  lsl #2]              \n"
        "ldr            r6,  [r0, r6,  lsl #2]              \n"
        "ldr            r7,  [r0, r7,  lsl #2]              \n"

        "add            r9,  r1, r2                         \n"     // r9  = fx + 5dx
        "add            r10, r1, r2, lsl #1                 \n"     // r10 = fx + 6dx
        "add            r11, r9, r2, lsl #1                 \n"     // r11 = fx + 7dx
        "lsr            r8,  r1,  #16                       \n"     //
        "lsr            r9,  r9,  #16                       \n"     //
        "lsr            r10, r10, #16                       \n"     //
        "lsr            r11, r11, #16                       \n"     //
        "ldr            r8,  [r0, r8,  lsl #2]              \n"
        "ldr            r9,  [r0, r9,  lsl #2]              \n"
        "ldr            r10, [r0, r10, lsl #2]              \n"
        "ldr            r11, [r0, r11, lsl #2]              \n"

        "vmov           d0, r4, r5                          \n"
        "vmov           d1, r6, r7                          \n"
        "vmov           d2, r8, r9                          \n"
        "vmov           d3, r10, r11                        \n"
        "vmull.u8       q2, d0, d31                         \n"     // alpha scale
        "vmull.u8       q3, d1, d31                         \n"
        "vmull.u8       q8, d2, d31                         \n"
        "vmull.u8       q9, d3, d31                         \n"
        "vshrn.i16      d0, q2, #8                          \n"     // shift back
        "vshrn.i16      d1, q3, #8                          \n"     // shift back
        "vshrn.i16      d2, q8, #8                          \n"     // shift back
        "vshrn.i16      d3, q9, #8                          \n"     // shift back
        "subs           r12, r12, #8                        \n"     // count -= 8
        "add            r1,  r1, r2, lsl #2                 \n"     // fx += 4dx
        "vstmia         r3!, {d0, d1, d2, d3}               \n"
        "bge            S32A_D32_decal_nofilter_scale_g88   \n"
        "b              S32A_D32_decal_nofilter_scale_out8  \n"
    );
}

// S32_alpha_D32_nofilter_DX + ClampX_ClampY_nofilter_scale_neon
void ClampX_S32_Alpha_D32_nofilter_scale_t(const     SkBitmapProcState& s,
                                           int       x,
                                           int       y,
                                           SkPMColor dstC[],
                                           int       count)
{
    //PREAMBLE(s);

    if (count == 0) {
        return ;
    }

    unsigned alphaScale = s.fAlphaScale;
    uint32_t* SK_RESTRICT srcAddr = (uint32_t *)s.fBitmap->getPixels();
    const unsigned maxX = s.fBitmap->width() - 1;
    SkFixed fx;
    {
        SkPoint pt;
        s.fInvProc(s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                                  SkIntToScalar(y) + SK_ScalarHalf, &pt);
        fx = SkScalarToFixed(pt.fY);
        const unsigned maxY = s.fBitmap->height() - 1;
        srcAddr = (uint32_t *)((char*)srcAddr +
                   TILEY_PROCF(fx, maxY) * s.fBitmap->rowBytes());
        fx = SkScalarToFixed(pt.fX);
    }

    const SkFixed dx = s.fInvSx;

    if (count >= 8) {
    //  LOGD(">> s32_alpha_d32, fx:%8x, dx:%8x, cnt:%3d, mla:%8x", fx, dx, count, fx + dx * count);
    //    , need optimize this branch in ICS
    }
    if (0 == maxX) {
        sk_memset32(dstC, SkAlphaMulQ(srcAddr[0], alphaScale), count);
        return;
    }

    int i;
#ifdef CHECK_FOR_DECAL
    // test if we don't need to apply the tile proc
    if ((unsigned)(fx >> 16) <= maxX &&
        (unsigned)((fx + dx * (count - 1)) >> 16) <= maxX) {
    #if 0
        for (i = count >> 2; i > 0; --i) {
            *dstC++ = SkAlphaMulQ(srcAddr[fx >> 16], alphaScale); fx += dx;
            *dstC++ = SkAlphaMulQ(srcAddr[fx >> 16], alphaScale); fx += dx;
            *dstC++ = SkAlphaMulQ(srcAddr[fx >> 16], alphaScale); fx += dx;
            *dstC++ = SkAlphaMulQ(srcAddr[fx >> 16], alphaScale); fx += dx;
        }
        for (i = count & 3; i > 0; --i) {
            *dstC++ = SkAlphaMulQ(srcAddr[fx >> 16], alphaScale); fx += dx;
        }
    #else
        // need to add optimize code when count is large enough
        if (alphaScale == 256) {
            alphaScale -= 1;
        }
        asm volatile (
            "vdup.8       d31, %[alphaScale]  \n"
            :
            :[alphaScale] "r" (alphaScale)
            :
        );
        S32A_D32_nofilter_scale_t(srcAddr, fx, dx, dstC, count);
    #endif
        return;
    }
#endif

    /*
     * this branch hardly appear
     */
    for (i = count >> 2; i > 0; --i) {
        *dstC++ = SkAlphaMulQ(srcAddr[TILEX_PROCF(fx, maxX)], alphaScale); fx += dx;
        *dstC++ = SkAlphaMulQ(srcAddr[TILEX_PROCF(fx, maxX)], alphaScale); fx += dx;
        *dstC++ = SkAlphaMulQ(srcAddr[TILEX_PROCF(fx, maxX)], alphaScale); fx += dx;
        *dstC++ = SkAlphaMulQ(srcAddr[TILEX_PROCF(fx, maxX)], alphaScale); fx += dx;
    }
    for (i = count & 3; i > 0; --i) {
        *dstC++ = SkAlphaMulQ(srcAddr[TILEX_PROCF(fx, maxX)], alphaScale); fx += dx;
    }
}


/*
 * , x, dx, y, dy are in proper range
 */
void S32_D32_nofilter_persp_dxdy_a_t(uint32_t *dstC, int count)
{
    asm volatile (
        "cmp        r1, #8                              \n"
        "push       {r4 - r10, r14}                     \n"             // reserve stack
        "blt        S32_D32_nofilter_persp_dxdy_a_less8 \n"
        "sub        r1, #8                              \n"
        "vshrn.i32  d2,  q9,  #16                       \n"
        "vshrn.i32  d3,  q10, #16                       \n"             // [x+7dx..x+4dx] >> 16
        "vshrn.i32  d6,  q6,  #16                       \n"
        "vshrn.i32  d7,  q7,  #16                       \n"             // [y+7dy..y+4dy] >> 16

    "S32_D32_nofilter_persp_dxdy_a_loop8:               \n"
        "vadd.i32   q10, q8,  q10                       \n"             // x += 8dx
        "vadd.i32   q9,  q8,  q9                        \n"             // x += 8dx
        "vshll.u16  q0,  d2,  #2                        \n"             // [0 .. 3]
        "vshll.u16  q4,  d3,  #2                        \n"             // [4 .. 7]
        "vadd.i32   q7,  q5,  q7                        \n"             // y += 8dx
        "vadd.i32   q6,  q5,  q6                        \n"             // y += 8dx
        "vadd.i32   q0,  q0,  q15                       \n"             // x << 2 + srcAddr
        "vadd.i32   q4,  q4,  q15                       \n"             // x << 2 + srcAddr
        "vmlal.s16  q0,  d6,  d28                       \n"             // srcAddr + x << 2 + y * rb
        "vmlal.s16  q4,  d7,  d28                       \n"             // srcAddr + x << 2 + y * rb
        "subs       r1,  r1,  #8                        \n"
        "vshrn.i32  d2,  q9,  #16                       \n"
        "vshrn.i32  d3,  q10, #16                       \n"             // [x+7dx..x+4dx] >> 16
        "vmov       r4,  r5,  s0,  s1                   \n"             // d0
        "vmov       r6,  r7,  s2,  s3                   \n"             // d1
        "vmov       r8,  r9,  s16, s17                  \n"             // d8
        "vmov       r10, r14, s18, s19                  \n"             // d9
        "ldr        r4,  [r4]                           \n"             // get color
        "ldr        r5,  [r5]                           \n"
        "ldr        r6,  [r6]                           \n"
        "ldr        r7,  [r7]                           \n"
        "ldr        r8,  [r8]                           \n"
        "ldr        r9,  [r9]                           \n"
        "ldr        r10, [r10]                          \n"
        "ldr        r14, [r14]                          \n"
        "vshrn.i32  d6,  q6,  #16                       \n"
        "vshrn.i32  d7,  q7,  #16                       \n"             // [y+7dy..y+4dy] >> 16
        "stmia      r0!, {r4 - r10, r14}                \n"
        "bge        S32_D32_nofilter_persp_dxdy_a_loop8 \n"
        "adds       r1,  r1, #8                         \n"
        "it         eq                                  \n"
        "popeq      {r4 - r10, pc}                      \n"             // return

    "S32_D32_nofilter_persp_dxdy_a_less8:               \n"
        "cmp        r1, #4                              \n"
        "blt        S32_D32_nofilter_persp_dxdy_a_less4 \n"
        "vshrn.i32  d0, q9, #16                         \n"             // x >> 16
        "vshrn.i32  d1, q6, #16                         \n"             // y >> 16
        "vmov       q9, q10                             \n"             // x += 4dx
        "vmov       q6, q7                              \n"             // y += 4dy
        "vshll.u16  q1, d0, #2                          \n"             // x << 2
        "vadd.i32   q1, q1, q15                         \n"             // x += srcAddr
        "vmlal.s16  q1, d1, d28                         \n"             // + y * rb
        "subs       r1, r1, #4                          \n"
        "vmov       r4, r5, s4, s5                      \n"             // d2
        "vmov       r6, r7, s6, s7                      \n"             // d3
        "ldr        r4,  [r4]                           \n"             // get color
        "ldr        r5,  [r5]                           \n"
        "ldr        r6,  [r6]                           \n"
        "ldr        r7,  [r7]                           \n"
        "stmia      r0!, {r4 - r7}                      \n"
        "it         eq                                  \n"
        "popeq      {r4 - r10, pc}                      \n"
    "S32_D32_nofilter_persp_dxdy_a_less4:               \n"
        "cmp        r1, #2                              \n"
        "vmovl.u16  q14, d28                            \n"             // exprand rb to 32bits
        "blt        S32_D32_nofilter_persp_dxdy_a_less2 \n"
        "vshr.s32   d0, d18, #16                        \n"             // x >> 16
        "vshr.s32   d1, d12, #16                        \n"             // y >> 16
        "vmov       d18, d19                            \n"             // x += 2dx
        "vmov       d12, d13                            \n"             // y += 2dy
        "vshl.u32   d0, d0, #2                          \n"             // x << 2
        "vadd.i32   d2, d0, d30                         \n"             // x += srcAddr
        "vmla.i32   d2, d1, d28                         \n"             // + y * rb
        "subs       r1, r1, #2                          \n"
        "vmov       r4, r5, s4, s5                      \n"             // d2
        "ldr        r4,  [r4]                           \n"             // get color
        "ldr        r5,  [r5]                           \n"
        "stmia      r0!, {r4 - r5}                      \n"
        "it         eq                                  \n"
        "popeq      {r4 - r10, pc}                      \n"

    "S32_D32_nofilter_persp_dxdy_a_less2:               \n"
        "vshr.s32   d0, d18, #16                        \n"             // x >> 16
        "vshr.s32   d1, d12, #16                        \n"             // y >> 16
        "vshl.u32   d0, d0, #2                          \n"             // x << 2
        "vadd.i32   d2, d0, d30                         \n"             // x += srcAddr
        "vmla.i32   d2, d1, d28                         \n"             // + y * rb
        "vmov       r4, s4                              \n"             // d2
        "ldr        r4, [r4]                            \n"             // get color
        "str        r4, [r0], #4                        \n"
        "pop        {r4 - r10, pc}                      \n"
    );
}

/*
 * , input is not in range
 */
void S32_D32_nofilter_persp_dxdy_b_t(uint32_t *dstC, int count)
{
    asm volatile (
        "cmp        r1, #8                              \n"
        "push       {r4 - r10, r14}                     \n"             // reserve stack
        "blt        S32_D32_nofilter_persp_dxdy_b_less8 \n"
        "sub        r1, #8                              \n"
        "vshrn.i32  d2,  q9,  #16                       \n"
        "vshrn.i32  d3,  q10, #16                       \n"             // [x+7dx..x+4dx] >> 16
        "vshrn.i32  d6,  q6,  #16                       \n"
        "vshrn.i32  d7,  q7,  #16                       \n"             // [y+7dy..y+4dy] >> 16

    "S32_D32_nofilter_persp_dxdy_b_loop8:               \n"
        "vmax.s16   q1,  q1,  q11                       \n"             // clamp x
        "vmax.s16   q3,  q3,  q11                       \n"
        "vadd.i32   q7,  q5,  q7                        \n"             // y += 8dx
        "vmin.s16   q1,  q1,  q13                       \n"             // clamp y
        "vmin.s16   q3,  q3,  q12                       \n"
        "vadd.i32   q6,  q5,  q6                        \n"             // y += 8dx
        "vadd.i32   q10, q8,  q10                       \n"             // x += 8dx
        "vadd.i32   q9,  q8,  q9                        \n"             // x += 8dx
        "vshll.u16  q0,  d2,  #2                        \n"             // [0 .. 3]
        "vshll.u16  q4,  d3,  #2                        \n"             // [4 .. 7]
        "vadd.i32   q0,  q0,  q15                       \n"             // x << 2 + srcAddr
        "vadd.i32   q4,  q4,  q15                       \n"             // x << 2 + srcAddr
        "vmlal.s16  q0,  d6,  d28                       \n"             // srcAddr + x << 2 + y * rb
        "vmlal.s16  q4,  d7,  d28                       \n"             // srcAddr + x << 2 + y * rb
        "subs       r1,  r1,  #8                        \n"
        "vshrn.i32  d2,  q9,  #16                       \n"
        "vshrn.i32  d3,  q10, #16                       \n"             // [x+7dx..x+4dx] >> 16
        "vmov       r4,  r5,  s0,  s1                   \n"             // d0
        "vmov       r6,  r7,  s2,  s3                   \n"             // d1
        "vmov       r8,  r9,  s16, s17                  \n"             // d8
        "vmov       r10, r14, s18, s19                  \n"             // d9
        "ldr        r4,  [r4]                           \n"             // get color
        "ldr        r5,  [r5]                           \n"
        "ldr        r6,  [r6]                           \n"
        "ldr        r7,  [r7]                           \n"
        "ldr        r8,  [r8]                           \n"
        "ldr        r9,  [r9]                           \n"
        "ldr        r10, [r10]                          \n"
        "ldr        r14, [r14]                          \n"
        "vshrn.i32  d6,  q6,  #16                       \n"
        "vshrn.i32  d7,  q7,  #16                       \n"             // [y+7dy..y+4dy] >> 16
        "stmia      r0!, {r4 - r10, r14}                \n"
        "bge        S32_D32_nofilter_persp_dxdy_b_loop8 \n"
        "adds       r1,  r1, #8                         \n"
        "it         eq                                  \n"
        "popeq      {r4 - r10, pc}                      \n"             // return

    "S32_D32_nofilter_persp_dxdy_b_less8:               \n"
        "cmp        r1, #4                              \n"
        "blt        S32_D32_nofilter_persp_dxdy_b_less4 \n"
        "vshrn.i32  d0, q9, #16                         \n"             // x >> 16
        "vshrn.i32  d1, q6, #16                         \n"             // y >> 16
        "vmov       q9, q10                             \n"             // x += 4dx
        "vmov       q6, q7                              \n"             // y += 4dy
        "vmax.s16   d0, d0, d22                         \n"
        "vmax.s16   d1, d1, d22                         \n"
        "vmin.s16   d0, d0, d26                         \n"
        "vmin.s16   d1, d1, d24                         \n"
        "vshll.u16  q1, d0, #2                          \n"             // x << 2
        "vadd.i32   q1, q1, q15                         \n"             // x += srcAddr
        "vmlal.s16  q1, d1, d28                         \n"             // + y * rb
        "subs       r1, r1, #4                          \n"
        "vmov       r4, r5, s4, s5                      \n"             // d2
        "vmov       r6, r7, s6, s7                      \n"             // d3
        "ldr        r4,  [r4]                           \n"             // get color
        "ldr        r5,  [r5]                           \n"
        "ldr        r6,  [r6]                           \n"
        "ldr        r7,  [r7]                           \n"
        "stmia      r0!, {r4 - r7}                      \n"
        "it         eq                                  \n"
        "popeq      {r4 - r10, pc}                      \n"
    "S32_D32_nofilter_persp_dxdy_b_less4:               \n"
        "cmp        r1, #2                              \n"
        "vmovl.u16  q14, d28                            \n"             // exprand rb to 32bits
        "blt        S32_D32_nofilter_persp_dxdy_b_less2 \n"

        "vshr.s32   d0, d18, #16                        \n"             // x >> 16
        "vshr.s32   d1, d12, #16                        \n"             // y >> 16
        "vmov       d18, d19                            \n"             // x += 2dx
        "vmov       d12, d13                            \n"             // y += 2dy
        "vmax.s16   d0, d0, d22                         \n"
        "vmax.s16   d1, d1, d22                         \n"
        "vmin.s16   d0, d0, d26                         \n"
        "vmin.s16   d1, d1, d24                         \n"
        "vshl.u32   d0, d0, #2                          \n"             // x << 2
        "vadd.i32   d2, d0, d30                         \n"             // x += srcAddr
        "vmla.i32   d2, d1, d28                         \n"             // + y * rb
        "subs       r1, r1, #2                          \n"
        "vmov       r4, r5, s4, s5                      \n"             // d2
        "ldr        r4,  [r4]                           \n"             // get color
        "ldr        r5,  [r5]                           \n"
        "stmia      r0!, {r4 - r5}                      \n"
        "it         eq                                  \n"
        "popeq      {r4 - r10, pc}                      \n"

    "S32_D32_nofilter_persp_dxdy_b_less2:               \n"
        "vshr.s32   d0, d18, #16                        \n"             // x >> 16
        "vshr.s32   d1, d12, #16                        \n"             // y >> 16
        "vmax.s16   d0, d0, d22                         \n"
        "vmax.s16   d1, d1, d22                         \n"
        "vmin.s16   d0, d0, d26                         \n"
        "vmin.s16   d1, d1, d24                         \n"
        "vshl.u32   d0, d0, #2                          \n"             // x << 2
        "vadd.i32   d2, d0, d30                         \n"             // x += srcAddr
        "vmla.i32   d2, d1, d28                         \n"             // + y * rb
        "vmov       r4, s4                              \n"             // d2
        "ldr        r4,  [r4]                           \n"             // get color
        "str        r4,  [r0], #4                       \n"
        "pop        {r4 - r10, pc}                      \n"
    );
}

/*
 * @amlogic.com combined operation of
 * PERSP_NOFILTER_NAME + S32_opaque_D32_nofilter_DXDY + SkPerspIter::next()
 */
void ClampXY_S32_D32_nofilter_persp_t(const     SkBitmapProcState& s,
                                      int       x,
                                      int       y,
                                      SkPMColor dstC[],
                                      int       count)
{
    SkASSERT(s.fInvType & SkMatrix::kPerspective_Mask);

    if (count == 0) {
        return;
    }
    //PREAMBLE(s);
    /* max{X,Y} are int here, but later shown/assumed to fit in 16 bits */
    int maxX = s.fBitmap->width() - 1;
    int maxY = s.fBitmap->height() - 1;
    uint32_t  save[8] = {};
    uint32_t *save_q = save;

    const char* SK_RESTRICT srcAddr = (const char*)s.fBitmap->getPixels();
    int rb = s.fBitmap->rowBytes();


    SkPoint pt;
    SkFixed x_x;
    SkFixed y_y;
    SkFixed fSX, fSY;
    SkFixed dx, dy;

    SkMatrix::Persp_xy(s.fInvMatrix,
                       SkIntToScalar(x) + SK_ScalarHalf,
                       SkIntToScalar(y) + SK_ScalarHalf , &pt);

    fSX = SkIntToScalar(x) + SK_ScalarHalf;
    fSY = SkIntToScalar(y) + SK_ScalarHalf;
    x_x = SkScalarToFixed(pt.fX);
    y_y = SkScalarToFixed(pt.fY);

    fSX += SkIntToScalar(count);
    SkMatrix::Persp_xy(s.fInvMatrix, fSX, fSY, &pt);
    dx = (SkScalarToFixed(pt.fX) - x_x) / count;
    dy = (SkScalarToFixed(pt.fY) - y_y) / count;

    /*
     * set up needed const
     */
    asm volatile (
        "vstmia         %[save_q], {q4 - q7}        \n"
        "vdup.32        q15, %[srcAddr]             \n"                 // q15 = [srcAddr].32
        "vdup.16        q14, %[rb]                  \n"                 // q14 = [rb].16
        "add            r12, %[x_x], %[dx]          \n"                 // x_x + dx
        "vmov           d18, %[x_x], r12            \n"
        "add            r11, %[x_x], %[dx], lsl #1  \n"                 // x_x + 2dx
        "add            r10, r12, %[dx], lsl #1     \n"                 // x_x + 3dx
        "vmov           d19, r11, r10               \n"                 // d19 = [x_x+3dx, x_x+2dx]
        "lsl            r12, %[dx], #2              \n"                 // r12 = 4dx
        "vdup.32        q8,  r12                    \n"                 // q8  = [4dx].32
        "vadd.i32       q10, q9, q8                 \n"                 // q10 = [x+7dx, x+6dx...].32
        "vshl.i32       q8,  q8, #1                 \n"                 // q8  = 8dx
        "add            r12, %[y_y], %[dy]          \n"                 //
        "vmov           d12, %[y_y], r12            \n"
        "add            r11, %[y_y], %[dy], lsl #1  \n"                 //
        "add            r10, r12, %[dy], lsl #1     \n"                 //
        "vmov           d13, r11, r10               \n"                 //
        "lsl            r12, %[dy], #2              \n"                 //
        "vdup.32        q5,  r12                    \n"                 //
        "vadd.i32       q7,  q5, q6                 \n"                 //
        "vshl.i32       q5,  q5, #1                 \n"                 // q5  = 8dy
        :
        : [srcAddr] "r" (srcAddr), [rb] "r" (rb),
          [x_x] "r" (x_x), [y_y] "r" (y_y),
          [dx] "r" (dx), [dy] "r" (dy), [save_q] "r" (save_q)
        : "cc", "memory", "r10", "r11", "r12"
    );
    /*
     * check if final x, y and start x, y is in proper value
     * most is the follow case
     */
    SkFixed final_x, final_y;
    final_x = (x_x + (count - 1) * dx) >> 16;
    final_y = (y_y + (count - 1) * dy) >> 16;
    if ((final_x <= maxX) && (final_x >= 0) &&
        (final_y <= maxY) && (final_y >= 0) &&
        ((x_x >> 16) <= maxX) && ((x_x >> 16) >= 0) &&
        ((y_y >> 16) <= maxY) && ((y_y >> 16) >= 0)){
    #if 0
        uint32_t i = count >> 2;
        while (i > 0) {
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            i--;
        }
        i = count & 3;
        while (i > 0) {
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            i--;
        }
    #else
        /*
         * neon opt code here
         */
        S32_D32_nofilter_persp_dxdy_a_t(dstC, count);
    #endif
    } else {
    #if 0
        uint32_t i = count >> 2;
        while (i > 0) {
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            i--;
        }
        i = count & 3;
        while (i > 0) {
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            i--;
        }
    #else
        asm volatile (
            "vdup.16        q13, %[maxX]                \n"                 // q13 = [maxX]
            "vdup.16        q12, %[maxY]                \n"
            "vmov.i16       q11, #0                     \n"
            :
            : [maxX] "r" (maxX), [maxY] "r" (maxY)
            : "memory", "cc"
        );
        /*
         * neon opt code
         */
        S32_D32_nofilter_persp_dxdy_b_t(dstC, count);
    #endif
    }
    asm volatile (
        "vldmia         %[save_q], {q4 - q7}            \n"
        :
        :[save_q] "r" (save_q)
        :"memory", "cc"
    );
}

/*
 * , x, dx, y, dy are in proper range
 */
void S32A_D32_nofilter_persp_dxdy_a_t(uint32_t *dstC, int count)
{
    asm volatile (
        "cmp        r1, #8                                  \n"
        "push       {r4 - r10, r14}                         \n"         // reserve stack
        "blt        S32A_D32_nofilter_persp_dxdy_a_less8    \n"
        "sub        r1, #8                                  \n"
        "vshrn.i32  d2,  q9,  #16                           \n"
        "vshrn.i32  d3,  q10, #16                           \n"         // [x+7dx..x+4dx] >> 16
        "vshrn.i32  d22, q6,  #16                           \n"
        "vshrn.i32  d23, q7,  #16                           \n"         // [y+7dy..y+4dy] >> 16

    "S32A_D32_nofilter_persp_dxdy_a_loop8:                  \n"
        "vadd.i32   q10, q8,  q10                           \n"         // x += 8dx
        "vadd.i32   q9,  q8,  q9                            \n"         // x += 8dx
        "vshll.u16  q0,  d2,  #2                            \n"         // [0 .. 3]
        "vshll.u16  q4,  d3,  #2                            \n"         // [4 .. 7]
        "vadd.i32   q7,  q5,  q7                            \n"         // y += 8dx
        "vadd.i32   q6,  q5,  q6                            \n"         // y += 8dx
        "vadd.i32   q0,  q0,  q15                           \n"         // x << 2 + srcAddr
        "vadd.i32   q4,  q4,  q15                           \n"         // x << 2 + srcAddr
        "vmlal.s16  q0,  d22, d28                           \n"         // srcAddr + x << 2 + y * rb
        "vmlal.s16  q4,  d23, d28                           \n"         // srcAddr + x << 2 + y * rb
        "subs       r1,  r1,  #8                            \n"
        "vshrn.i32  d2,  q9,  #16                           \n"
        "vshrn.i32  d3,  q10, #16                           \n"         // [x+7dx..x+4dx] >> 16
        "vmov       r4,  r5,  s0,  s1                       \n"         // d0
        "vmov       r6,  r7,  s2,  s3                       \n"         // d1
        "vmov       r8,  r9,  s16, s17                      \n"         // d8
        "vmov       r10, r14, s18, s19                      \n"         // d9
        "vldr.32    s0,  [r4]                               \n"         // get color
        "vldr.32    s1,  [r5]                               \n"
        "vldr.32    s2,  [r6]                               \n"
        "vldr.32    s3,  [r7]                               \n"
        "vldr.32    s16, [r8]                               \n"
        "vldr.32    s17, [r9]                               \n"
        "vldr.32    s18, [r10]                              \n"
        "vldr.32    s19, [r14]                              \n"
        "vmull.u8   q2,  d0,  d29                           \n"         // alpha scale
        "vmull.u8   q0,  d1,  d29                           \n"
        "vmull.u8   q3,  d8,  d29                           \n"
        "vmull.u8   q4,  d9,  d29                           \n"
        "vshrn.i32  d22, q6,  #16                           \n"
        "vshrn.i16  d4,  q2,  #8                            \n"
        "vshrn.i16  d5,  q0,  #8                            \n"
        "vshrn.i16  d6,  q3,  #8                            \n"
        "vshrn.i16  d7,  q4,  #8                            \n"
        "vshrn.i32  d23, q7,  #16                           \n"         // [y+7dy..y+4dy] >> 16
        "vstmia     r0!, {d4, d5, d6, d7}                   \n"
        "bge        S32A_D32_nofilter_persp_dxdy_a_loop8    \n"
        "adds       r1,  r1, #8                             \n"
        "it         eq                                      \n"
        "popeq      {r4 - r10, pc}                          \n"         // return

    "S32A_D32_nofilter_persp_dxdy_a_less8:                  \n"
        "cmp        r1, #4                                  \n"
        "blt        S32A_D32_nofilter_persp_dxdy_a_less4    \n"
        "vshrn.i32  d0, q9, #16                             \n"         // x >> 16
        "vshrn.i32  d1, q6, #16                             \n"         // y >> 16
        "vmov       q9, q10                                 \n"         // x += 4dx
        "vmov       q6, q7                                  \n"         // y += 4dy
        "vshll.u16  q1, d0, #2                              \n"         // x << 2
        "vadd.i32   q1, q1, q15                             \n"         // x += srcAddr
        "vmlal.s16  q1, d1, d28                             \n"         // + y * rb
        "subs       r1, r1, #4                              \n"
        "vmov       r4, r5, s4, s5                          \n"         // d2
        "vmov       r6, r7, s6, s7                          \n"         // d3
        "vldr.32    s4,  [r4]                               \n"         // get color
        "vldr.32    s5,  [r5]                               \n"
        "vldr.32    s6,  [r6]                               \n"
        "vldr.32    s7,  [r7]                               \n"
        "vmull.u8   q2,  d2,  d29                           \n"
        "vmull.u8   q3,  d3,  d29                           \n"
        "vshrn.i16  d2,  q2,  #8                            \n"
        "vshrn.i16  d3,  q3,  #8                            \n"
        "vstmia     r0!, {d2, d3}                           \n"
        "it         eq                                      \n"
        "popeq      {r4 - r10, pc}                          \n"
    "S32A_D32_nofilter_persp_dxdy_a_less4:                  \n"
        "cmp        r1, #2                                  \n"
        "vmovl.u16  q14, d28                                \n"         // exprand rb to 32bits
        "blt        S32A_D32_nofilter_persp_dxdy_a_less2    \n"

        "vshr.s32   d0, d18, #16                            \n"         // x >> 16
        "vshr.s32   d1, d12, #16                            \n"         // y >> 16
        "vmov       d18, d19                                \n"         // x += 2dx
        "vmov       d12, d13                                \n"         // y += 2dy
        "vshl.u32   d0, d0, #2                              \n"         // x << 2
        "vadd.i32   d2, d0, d30                             \n"         // x += srcAddr
        "vmla.i32   d2, d1, d28                             \n"         // + y * rb
        "subs       r1, r1, #2                              \n"
        "vmov       r4, r5, s4, s5                          \n"         // d2
        "vldr.32    s4,  [r4]                               \n"         // get color
        "vldr.32    s5,  [r5]                               \n"
        "vmull.u8   q2,  d2, d29                            \n"
        "vshrn.i16  d2,  q2, #8                             \n"
        "vstmia     r0!, {d2}                               \n"
        "it         eq                                      \n"
        "popeq      {r4 - r10, pc}                          \n"

    "S32A_D32_nofilter_persp_dxdy_a_less2:                  \n"
        "vshr.s32   d0, d18, #16                            \n"         // x >> 16
        "vshr.s32   d1, d12, #16                            \n"         // y >> 16
        "vshl.u32   d0, d0, #2                              \n"         // x << 2
        "vadd.i32   d2, d0, d30                             \n"         // x += srcAddr
        "vmla.i32   d2, d1, d28                             \n"         // + y * rb
        "vmov       r4, s4                                  \n"         // d2
        "vldr.32    s4,  [r4]                               \n"         // get color
        "vmull.u8   q2,  d2, d29                            \n"
        "vshrn.i16  d2,  q2, #8                             \n"
        "vst1.32    {d2[0]}, [r0]!                          \n"
        "pop        {r4 - r10, pc}                          \n"
    );
}

/*
 * , input is not in range
 */
void S32A_D32_nofilter_persp_dxdy_b_t(uint32_t *dstC, int count)
{
    asm volatile (
        "cmp        r1, #8                                  \n"
        "push       {r4 - r10, r14}                         \n"         // reserve stack
        "blt        S32A_D32_nofilter_persp_dxdy_b_less8    \n"
        "sub        r1, #8                                  \n"
        "vshrn.i32  d2,  q9,  #16                           \n"
        "vshrn.i32  d3,  q10, #16                           \n"         // [x+7dx..x+4dx] >> 16

    "S32A_D32_nofilter_persp_dxdy_b_loop8:                  \n"
        "vshrn.i32  d6,  q6,  #16                           \n"
        "vshrn.i32  d7,  q7,  #16                           \n"         // [y+7dy..y+4dy] >> 16
        "vmax.s16   q1,  q1,  q11                           \n"         // clamp x
        "vmax.s16   q3,  q3,  q11                           \n"
        "vadd.i32   q7,  q5,  q7                            \n"         // y += 8dx
        "vmin.s16   q1,  q1,  q13                           \n"         // clamp y
        "vmin.s16   q3,  q3,  q12                           \n"
        "vadd.i32   q6,  q5,  q6                            \n"         // y += 8dx
        "vadd.i32   q10, q8,  q10                           \n"         // x += 8dx
        "vadd.i32   q9,  q8,  q9                            \n"         // x += 8dx
        "vshll.u16  q0,  d2,  #2                            \n"         // [0 .. 3]
        "vshll.u16  q4,  d3,  #2                            \n"         // [4 .. 7]
        "vadd.i32   q0,  q0,  q15                           \n"         // x << 2 + srcAddr
        "vadd.i32   q4,  q4,  q15                           \n"         // x << 2 + srcAddr
        "vmlal.s16  q0,  d6,  d28                           \n"         // srcAddr + x << 2 + y * rb
        "vmlal.s16  q4,  d7,  d28                           \n"         // srcAddr + x << 2 + y * rb
        "subs       r1,  r1,  #8                            \n"
        "vshrn.i32  d2,  q9,  #16                           \n"
        "vshrn.i32  d3,  q10, #16                           \n"         // [x+7dx..x+4dx] >> 16
        "vmov       r4,  r5,  s0,  s1                       \n"         // d0
        "vmov       r6,  r7,  s2,  s3                       \n"         // d1
        "vmov       r8,  r9,  s16, s17                      \n"         // d8
        "vmov       r10, r14, s18, s19                      \n"         // d9
        "vldr.32    s0,  [r4]                               \n"         // get color
        "vldr.32    s1,  [r5]                               \n"
        "vldr.32    s2,  [r6]                               \n"
        "vldr.32    s3,  [r7]                               \n"
        "vldr.32    s16, [r8]                               \n"
        "vldr.32    s17, [r9]                               \n"
        "vldr.32    s18, [r10]                              \n"
        "vldr.32    s19, [r14]                              \n"
        "vmull.u8   q2,  d0,  d29                           \n"         // alpha scale
        "vmull.u8   q0,  d1,  d29                           \n"
        "vmull.u8   q3,  d8,  d29                           \n"
        "vmull.u8   q4,  d9,  d29                           \n"
        "vshrn.i16  d4,  q2,  #8                            \n"
        "vshrn.i16  d5,  q0,  #8                            \n"
        "vshrn.i16  d6,  q3,  #8                            \n"
        "vshrn.i16  d7,  q4,  #8                            \n"
        "vstmia     r0!, {d4, d5, d6, d7}                   \n"
        "bge        S32A_D32_nofilter_persp_dxdy_b_loop8    \n"
        "adds       r1,  r1, #8                             \n"
        "it         eq                                      \n"
        "popeq      {r4 - r10, pc}                          \n"         // return

    "S32A_D32_nofilter_persp_dxdy_b_less8:                  \n"
        "cmp        r1, #4                                  \n"
        "blt        S32A_D32_nofilter_persp_dxdy_b_less4    \n"
        "vshrn.i32  d0, q9, #16                             \n"         // x >> 16
        "vshrn.i32  d1, q6, #16                             \n"         // y >> 16
        "vmov       q9, q10                                 \n"         // x += 4dx
        "vmov       q6, q7                                  \n"         // y += 4dy
        "vmax.s16   d0, d0, d22                             \n"
        "vmax.s16   d1, d1, d22                             \n"
        "vmin.s16   d0, d0, d26                             \n"
        "vmin.s16   d1, d1, d24                             \n"
        "vshll.u16  q1, d0, #2                              \n"         // x << 2
        "vadd.i32   q1, q1, q15                             \n"         // x += srcAddr
        "vmlal.s16  q1, d1, d28                             \n"         // + y * rb
        "subs       r1, r1, #4                              \n"
        "vmov       r4, r5, s4, s5                          \n"         // d2
        "vmov       r6, r7, s6, s7                          \n"         // d3
        "vldr.32    s4,  [r4]                               \n"         // get color
        "vldr.32    s5,  [r5]                               \n"
        "vldr.32    s6,  [r6]                               \n"
        "vldr.32    s7,  [r7]                               \n"
        "vmull.u8   q2,  d2,  d29                           \n"
        "vmull.u8   q3,  d3,  d29                           \n"
        "vshrn.i16  d2,  q2,  #8                            \n"
        "vshrn.i16  d3,  q3,  #8                            \n"
        "vstmia     r0!, {d2, d3}                           \n"
        "it         eq                                      \n"
        "popeq      {r4 - r10, pc}                          \n"

    "S32A_D32_nofilter_persp_dxdy_b_less4:                  \n"
        "cmp        r1, #2                                  \n"
        "vmovl.u16  q14, d28                                \n"         // exprand rb to 32bits
        "blt        S32A_D32_nofilter_persp_dxdy_b_less2    \n"
        "vshr.s32   d0, d18, #16                            \n"         // x >> 16
        "vshr.s32   d1, d12, #16                            \n"         // y >> 16
        "vmov       d18, d19                                \n"         // x += 2dx
        "vmov       d12, d13                                \n"         // y += 2dy
        "vmax.s16   d0, d0, d22                             \n"
        "vmax.s16   d1, d1, d22                             \n"
        "vmin.s16   d0, d0, d26                             \n"
        "vmin.s16   d1, d1, d24                             \n"
        "vshl.u32   d0, d0, #2                              \n"         // x << 2
        "vadd.i32   d2, d0, d30                             \n"         // x += srcAddr
        "vmla.i32   d2, d1, d28                             \n"         // + y * rb
        "subs       r1, r1, #2                              \n"
        "vmov       r4, r5, s4, s5                          \n"         // d2
        "vldr.32    s4,  [r4]                               \n"         // get color
        "vldr.32    s5,  [r5]                               \n"
        "vmull.u8   q2,  d2, d29                            \n"
        "vshrn.i16  d2,  q2, #8                             \n"
        "vstmia     r0!, {d2}                               \n"
        "it         eq                                      \n"
        "popeq      {r4 - r10, pc}                          \n"

    "S32A_D32_nofilter_persp_dxdy_b_less2:                  \n"
        "vshr.s32   d0, d18, #16                            \n"         // x >> 16
        "vshr.s32   d1, d12, #16                            \n"         // y >> 16
        "vmax.s16   d0, d0, d22                             \n"
        "vmax.s16   d1, d1, d22                             \n"
        "vmin.s16   d0, d0, d26                             \n"
        "vmin.s16   d1, d1, d24                             \n"
        "vshl.u32   d0, d0, #2                              \n"         // x << 2
        "vadd.i32   d2, d0, d30                             \n"         // x += srcAddr
        "vmla.i32   d2, d1, d28                             \n"         // + y * rb
        "vmov       r4, s4                                  \n"         // d2
        "vldr.32    s4,  [r4]                               \n"         // get color
        "vmull.u8   q2,  d2, d29                            \n"
        "vshrn.i16  d2,  q2, #8                             \n"
        "vst1.32    {d2[0]}, [r0]!                          \n"
        "pop        {r4 - r10, pc}                          \n"
    );
}

/*
 * @amlogic.com combined operation of
 * PERSP_NOFILTER_NAME + S32_alpha_D32_nofilter_DXDY + SkPerspIter::next()
 */
void ClampXY_S32A_D32_nofilter_persp_t(const     SkBitmapProcState& s,
                                       int       x,
                                       int       y,
                                       SkPMColor dstC[],
                                       int       count)
{
    SkASSERT(s.fInvType & SkMatrix::kPerspective_Mask);

    if (count == 0) {
        return;
    }
    //PREAMBLE(s);
    uint32_t  save[8];
    uint32_t *save_q = save;
    /* max{X,Y} are int here, but later shown/assumed to fit in 16 bits */
    int maxX = s.fBitmap->width() - 1;
    int maxY = s.fBitmap->height() - 1;

    unsigned alphaScale = s.fAlphaScale;
    const char* SK_RESTRICT srcAddr = (const char*)s.fBitmap->getPixels();
    int rb = s.fBitmap->rowBytes();


    SkPoint pt;
    SkFixed x_x;
    SkFixed y_y;
    SkFixed fSX, fSY;
    SkFixed dx, dy;

    SkMatrix::Persp_xy(s.fInvMatrix,
                       SkIntToScalar(x) + SK_ScalarHalf,
                       SkIntToScalar(y) + SK_ScalarHalf , &pt);

    fSX = SkIntToScalar(x) + SK_ScalarHalf;
    fSY = SkIntToScalar(y) + SK_ScalarHalf;
    x_x = SkScalarToFixed(pt.fX);
    y_y = SkScalarToFixed(pt.fY);

    fSX += SkIntToScalar(count);
    SkMatrix::Persp_xy(s.fInvMatrix, fSX, fSY, &pt);
    dx = (SkScalarToFixed(pt.fX) - x_x) / count;
    dy = (SkScalarToFixed(pt.fY) - y_y) / count;

    if (alphaScale == 256) {
        alphaScale -= 1;
    }
    /*
     * set up needed const
     */
    asm volatile (
        "vstmia         %[save_q], {q4 - q7}        \n"
        "vdup.32        q15, %[srcAddr]             \n"                 // q15 = [srcAddr].32
        "vdup.16        d28, %[rb]                  \n"                 // d28 = [rb].16
        "vdup.8         d29, %[alphaScale]          \n"                 // d29 = alpha
        "add            r12, %[x_x], %[dx]          \n"                 // x_x + dx
        "vmov           d18, %[x_x], r12            \n"
        "add            r11, %[x_x], %[dx], lsl #1  \n"                 // x_x + 2dx
        "add            r10, r12, %[dx], lsl #1     \n"                 // x_x + 3dx
        "vmov           d19, r11, r10               \n"                 // d19 = [x_x+3dx, x_x+2dx]
        "lsl            r12, %[dx], #2              \n"                 // r12 = 4dx
        "vdup.32        q8,  r12                    \n"                 // q8  = [4dx].32
        "vadd.i32       q10, q9, q8                 \n"                 // q10 = [x+7dx, x+6dx...].32
        "vshl.i32       q8,  q8, #1                 \n"                 // q8  = 8dx
        "add            r12, %[y_y], %[dy]          \n"                 //
        "vmov           d12, %[y_y], r12            \n"
        "add            r11, %[y_y], %[dy], lsl #1  \n"                 //
        "add            r10, r12, %[dy], lsl #1     \n"                 //
        "vmov           d13, r11, r10               \n"                 //
        "lsl            r12, %[dy], #2              \n"                 //
        "vdup.32        q5,  r12                    \n"                 //
        "vadd.i32       q7,  q5, q6                 \n"                 //
        "vshl.i32       q5,  q5, #1                 \n"                 // q5  = 8dy
        :
        : [srcAddr] "r" (srcAddr), [rb] "r" (rb),
          [x_x] "r" (x_x), [y_y] "r" (y_y),
          [dx] "r" (dx), [dy] "r" (dy), [alphaScale] "r" (alphaScale),
          [save_q] "r" (save_q)
        : "cc", "memory", "r10", "r11", "r12"
    );

    /*
     * check if final x, y and start x, y is in proper value
     * most is the follow case
     */
    SkFixed final_x, final_y;
    final_x = (x_x + (count - 1) * dx) >> 16;
    final_y = (y_y + (count - 1) * dy) >> 16;
    if ((final_x <= maxX) && (final_x >= 0) &&
        (final_y <= maxY) && (final_y >= 0) &&
        ((x_x >> 16) <= maxX) && ((x_x >> 16) >= 0) &&
        ((y_y >> 16) <= maxY) && ((y_y >> 16) >= 0)){
    #if 0
        uint32_t i = count >> 2;
        while (i > 0) {
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            i--;
        }
        i = count & 3;
        while (i > 0) {
            *dstC++ = ((const uint32_t*)(srcAddr + (y_y >> 16)*rb))[x_x >> 16]; x_x += dx; y_y += dy;
            i--;
        }
    #else
        /*
         * neon opt code here
         */
        S32A_D32_nofilter_persp_dxdy_a_t(dstC, count);
    #endif
    } else {
    #if 0
        uint32_t i = count >> 2;
        while (i > 0) {
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            i--;
        }
        i = count & 3;
        while (i > 0) {
            *dstC++ = ((const uint32_t*)(srcAddr + TILEY_PROCF(y_y, maxY) * rb))[TILEX_PROCF(x_x, maxX)];
            x_x += dx; y_y += dy;
            i--;
        }
    #else
        asm volatile (
            "vdup.16        q13, %[maxX]                \n"                 // q13 = [maxX]
            "vdup.16        q12, %[maxY]                \n"
            "vmov.i16       q11, #0                     \n"
            :
            : [maxX] "r" (maxX), [maxY] "r" (maxY)
            : "memory", "cc"
        );
        /*
         * neon opt code
         */
        S32A_D32_nofilter_persp_dxdy_b_t(dstC, count);
    #endif
    }
    asm volatile (
        "vldmia         %[save_q], {q4 - q7}            \n"
        :
        : [save_q] "r" (save_q)
        : "memory", "cc"
    );
}

/*
 * , this branch is in proper range
 */
void SI8_D32_Clamp_filter_DX_a_t(SkFixed fx,
                                 SkFixed dx,
                                 int   count,
                                 const uint8_t * image0,
                                 const uint8_t * image1,
                                 uint32_t * colors,
                                 const uint32_t * table)
{
    /*
     * , optimize this function
     * register allocator:
     * r0 --> fx,       r1 --> dx,  r2 --> count
     *
     *
     * d31 --> y,       d30 --> 16 - y
     * d29 --> 16,      d28 -->
     *
     */
    asm volatile (
        "cmp            r2, #4                              \n"
        "vpush          {q4 - q7}                           \n"
        "blt            SI8_D32_Clamp_filter_DX_a_less4     \n"
        "sub            r2, #4                              \n"

        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xa
        "lsr            r7,  r0, #16                        \n"
        "add            r8,  r7, %[image0]                  \n"
        "add            r7,  r7, %[image1]                  \n"
        "add            r0,  r0, r1                         \n"     // fx += dx
        "vdup.16        d26, r12                            \n"     // d26 = [Xa].16

    "SI8_D32_Clamp_filter_DX_a_loop4:                       \n"
        // pixel 1 ~ 2
        "ldrb           r10, [r8]                           \n"
        "ldrb           r8,  [r8, #1]                       \n"
        "ldrb           r9,  [r7]                           \n"
        "ldrb           r7,  [r7, #1]                       \n"
        "add            r10, %[table], r10, lsl #2          \n"
        "add            r8,  %[table], r8,  lsl #2          \n"
        "add            r9,  %[table], r9,  lsl #2          \n"     // table + row1[x0]
        "add            r7,  %[table], r7,  lsl #2          \n"     // table + row1[x1]
        "vldr.32        s0,  [r10]                          \n"
        "vldr.32        s1,  [r8]                           \n"     // d0  = [a01, a00]
        "vldr.32        s2,  [r9]                           \n"
        "vldr.32        s3,  [r7]                           \n"     // d1  = [a11, a10]
        "ubfx           r12, r0, #12, #4                    \n"     // r12 = Xb
        "vmull.u8       q3,  d0, d30                        \n"     // q3  = [a01, a00] * (16-y)
        "vdup.16        d27, r12                            \n"     // d27 = [Xb]
        "vmull.u8       q4,  d1, d31                        \n"     // q4  = [a11, a10] * y

        "lsr            r7,  r0, #16                        \n"
        "add            r8,  r7,  %[image0]                 \n"
        "add            r7,  r7,  %[image1]                 \n"
        "ldrb           r10, [r8]                           \n"
        "ldrb           r8,  [r8, #1]                       \n"
        "ldrb           r9,  [r7]                           \n"
        "ldrb           r7,  [r7, #1]                       \n"
        "add            r10, %[table], r10, lsl #2          \n"
        "add            r8,  %[table], r8,  lsl #2          \n"
        "add            r9,  %[table], r9,  lsl #2          \n"     // table + row1[x0]
        "add            r7,  %[table], r7,  lsl #2          \n"     // table + row1[x1]
        "vldr.32        s4,  [r10]                          \n"
        "vldr.32        s5,  [r8]                           \n"     // d2  = [b01, b00]
        "vldr.32        s6,  [r9]                           \n"
        "vldr.32        s7,  [r7]                           \n"     // d3  = [b11, b10]
        "add            r0,  r0,  r1                        \n"     // fx += dx
        "vsub.i16       q12, q14, q13                       \n"     // q12 = [16-Xb, 16-Xa].16
        "vmull.u8       q5,  d2, d30                        \n"     // q5  = [b01, b00] * (16 - y)
        "vmull.u8       q6,  d3, d31                        \n"     // q6  = [b11, b00] *  y

        // pixel 3 ~ 4
        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xc
        "lsr            r7,  r0, #16                        \n"
        "vdup.16        d22, r12                            \n"     // d22 = [Xc].16
        "add            r8,  r7, %[image0]                  \n"
        "add            r7,  r7, %[image1]                  \n"
        "ldrb           r10, [r8]                           \n"
        "ldrb           r8,  [r8, #1]                       \n"
        "ldrb           r9,  [r7]                           \n"
        "ldrb           r7,  [r7, #1]                       \n"
        "add            r10, %[table], r10, lsl #2          \n"
        "add            r8,  %[table], r8,  lsl #2          \n"
        "add            r9,  %[table], r9,  lsl #2          \n"     // table + row1[x0]
        "add            r7,  %[table], r7,  lsl #2          \n"     // table + row1[x1]
        "vldr.32        s0,  [r10]                          \n"
        "vldr.32        s1,  [r8]                           \n"     // d0  = [c01, c00]
        "vldr.32        s2,  [r9]                           \n"
        "vldr.32        s3,  [r7]                           \n"     // d1  = [c11, c10]
        "add            r0,  r0, r1                         \n"     // fx += dx
        "vswp           d7,  d10                            \n"     // q3  = [b00, a00].16, q5 = [b01, a01]*(16-y)
        "vswp           d9,  d12                            \n"     // q4  = [b10, a10].16, q6 = [b11, a11].16*y
        "ubfx           r12, r0, #12, #4                    \n"     // r12 = Xd
        "vmull.u8       q7,  d0, d30                        \n"     // q7  = [c01, c00] * (16-y)
        "vdup.16        d23, r12                            \n"     // d23 = [Xd]
        "vmull.u8       q8,  d1, d31                        \n"     // q8  = [c11, c10] * y

        "lsr            r7,  r0,  #16                       \n"
        "add            r8,  r7,  %[image0]                 \n"
        "add            r7,  r7,  %[image1]                 \n"
        "ldrb           r10, [r8]                           \n"
        "ldrb           r8,  [r8, #1]                       \n"
        "ldrb           r9,  [r7]                           \n"
        "ldrb           r7,  [r7, #1]                       \n"
        "add            r10, %[table], r10, lsl #2          \n"
        "add            r8,  %[table], r8,  lsl #2          \n"
        "add            r9,  %[table], r9,  lsl #2          \n"     // table + row1[x0]
        "add            r7,  %[table], r7,  lsl #2          \n"     // table + row1[x1]
        "vldr.32        s4,  [r10]                          \n"
        "vldr.32        s5,  [r8]                           \n"     // d2  = [d01, d00]
        "vldr.32        s6,  [r9]                           \n"
        "vldr.32        s7,  [r7]                           \n"     // d3  = [d11, d10]
        "add            r0,  r0,  r1                        \n"     // fx += dx
        "vsub.i16       q0,  q14, q11                       \n"     // q0  = [16-Xd, 16-Xc].16
        "vmull.u8       q9,  d2, d30                        \n"     // q9  = [d01, d00] * (16 - y)
        "vmull.u8       q10, d3, d31                        \n"     // q10 = [d11, d00] *  y

        "subs           r2,  r2, #4                         \n"
        "vswp           d15, d18                            \n"     // q7  = [d00, c00].16, q9 = [d01, c01].16*(16-y)
        "vswp           d17, d20                            \n"     // q8  = [d10, c10].16, q10= [d11, c11].16*y

        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xa

        "vmul.i16       q3,  q3,  q12                       \n"     // q3  = [b00, a00]*(16-y)*[16-Xb, 16-Xa]
        "vmul.i16       q7,  q7,  q0                        \n"     // q7  = [d00, c00]*(16-y)*[16-Xd, 16-Xc]
        "lsr            r7,  r0,  #16                       \n"
        "vmla.i16       q3,  q5,  q13                       \n"     // q3 += [b01, a01]*(16-y)*[Xb, Xa]
        "vmla.i16       q7,  q9,  q11                       \n"     // q7 += [d01, c01]*(16-y)*[Xd, Xc]
        "add            r8,  r7, %[image0]                  \n"
        "vmla.i16       q3,  q4,  q12                       \n"     // q3 += [b10, a10]*y*[16-Xb, 16-Xa]
        "vmla.i16       q7,  q8,  q0                        \n"     // q7 += [d10, c10]*y*[16-Xd, 16-Xc]
        "add            r7,  r7, %[image1]                  \n"
        "vmla.i16       q3,  q6,  q13                       \n"     // q3 += [b11, a11]*y*[Xb, Xa]
        "vmla.i16       q7,  q10, q11                       \n"     // q7 += [d11, c11]*y*[Xd, Xc]
        "add            r0,  r0, r1                         \n"     // fx += dx
        "vshrn.i16      d2,  q3, #8                         \n"     // result
        "vshrn.i16      d3,  q7, #8                         \n"     // result
        "vdup.16        d26, r12                            \n"     // d26 = [Xa].16
        "vmull.u8       q3,  d2, d4                         \n"     // multiply alpha scale
        "vmull.u8       q7,  d3, d4                         \n"
        "vshrn.i16      d2,  q3, #8                         \n"
        "vshrn.i16      d3,  q7, #8                         \n"
        "vstmia         %[colors]!, {d2, d3}                \n"     // store data
        "bge            SI8_D32_Clamp_filter_DX_a_loop4     \n"
        "adds           r2,  r2, #4                         \n"
        "beq            SI8_D32_Clamp_filter_DX_a_out       \n"
        "sub            r0,  r0, r1                         \n"     // because fx is added once more

    "SI8_D32_Clamp_filter_DX_a_less4:                       \n"
        "cmp            r2, #2                              \n"
        "blt            SI8_D32_Clamp_filter_DX_a_less2     \n"
        // pixel 1 ~ 2
        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xa
        "lsr            r7,  r0, #16                        \n"
        "vdup.16        d26, r12                            \n"     // d26 = [Xa].16
        "add            r8,  r7, %[image0]                  \n"
        "add            r7,  r7, %[image1]                  \n"
        "add            r0,  r0, r1                         \n"     // fx += dx
        "ldrb           r10, [r8]                           \n"
        "ldrb           r8,  [r8, #1]                       \n"
        "ldrb           r9,  [r7]                           \n"
        "ldrb           r7,  [r7, #1]                       \n"
        "add            r10, %[table], r10, lsl #2          \n"
        "add            r8,  %[table], r8,  lsl #2          \n"
        "add            r9,  %[table], r9,  lsl #2          \n"     // table + row1[x0]
        "add            r7,  %[table], r7,  lsl #2          \n"     // table + row1[x1]
        "vldr.32        s0,  [r10]                          \n"
        "vldr.32        s1,  [r8]                           \n"     // d0  = [a01, a00]
        "vldr.32        s2,  [r9]                           \n"
        "vldr.32        s3,  [r7]                           \n"     // d1  = [a11, a10]
        "ubfx           r12, r0, #12, #4                    \n"     // r12 = Xb
        "vmull.u8       q3,  d0, d30                        \n"     // q3  = [a01, a00] * (16-y)
        "vdup.16        d27, r12                            \n"     // d27 = [Xb]
        "vmull.u8       q4,  d1, d31                        \n"     // q4  = [a11, a10] * y

        "lsr            r7,  r0,  #16                       \n"
        "add            r8,  r7,  %[image0]                 \n"
        "add            r7,  r7,  %[image1]                 \n"
        "ldrb           r10, [r8]                           \n"
        "ldrb           r8,  [r8, #1]                       \n"
        "ldrb           r9,  [r7]                           \n"
        "ldrb           r7,  [r7, #1]                       \n"
        "add            r10, %[table], r10, lsl #2          \n"
        "add            r8,  %[table], r8,  lsl #2          \n"
        "add            r9,  %[table], r9,  lsl #2          \n"     // table + row1[x0]
        "add            r7,  %[table], r7,  lsl #2          \n"     // table + row1[x1]
        "vldr.32        s4,  [r10]                          \n"
        "vldr.32        s5,  [r8]                           \n"     // d2  = [b01, b00]
        "vldr.32        s6,  [r9]                           \n"
        "vldr.32        s7,  [r7]                           \n"     // d3  = [b11, b10]
        "add            r0,  r0,  r1                        \n"     // fx += dx
        "vsub.i16       q12, q14, q13                       \n"     // q12 = [16-Xb, 16-Xa].16
        "vmull.u8       q5,  d2, d30                        \n"     // q5  = [b01, b00] * (16 - y)
        "vmull.u8       q6,  d3, d31                        \n"     // q6  = [b11, b00] *  y
        "subs           r2,  #2                             \n"
        "vswp           d7,  d10                            \n"     // q3  = [b00, a00].16, q5 = [b01, a01]*(16-y)
        "vswp           d9,  d12                            \n"     // q4  = [b10, a10].16, q6 = [b11, a11].16*y

        "vmul.i16       q3,  q3,  q12                       \n"     // q3  = [b00, a00]*(16-y)*[16-Xb, 16-Xa]
        "vmla.i16       q3,  q5,  q13                       \n"     // q3 += [b01, a01]*(16-y)*[Xb, Xa]
        "vmla.i16       q3,  q4,  q12                       \n"     // q3 += [b10, a10]*y*[16-Xb, 16-Xa]
        "vmla.i16       q3,  q6,  q13                       \n"     // q3 += [b11, a11]*y*[Xb, Xa]
        "vshrn.i16      d3,  q3,  #8                        \n"     // result
        "vmull.u8       q3,  d3,  d4                        \n"     // alpha scale
        "vshrn.i16      d3,  q3,  #8                        \n"
        "vstmia         %[colors]!, {d3}                    \n"     // store data
        "beq            SI8_D32_Clamp_filter_DX_a_out       \n"

    "SI8_D32_Clamp_filter_DX_a_less2:                       \n"
        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xa
        "lsr            r7,  r0,  #16                       \n"
        "vdup.16        d26, r12                            \n"     // d26 = [Xa].16
        "add            r8,  r7, %[image0]                  \n"
        "add            r7,  r7, %[image1]                  \n"
        "ldrb           r10, [r8]                           \n"
        "ldrb           r8,  [r8, #1]                       \n"
        "ldrb           r9,  [r7]                           \n"
        "ldrb           r7,  [r7, #1]                       \n"
        "add            r10, %[table], r10, lsl #2          \n"
        "add            r8,  %[table], r8,  lsl #2          \n"
        "add            r9,  %[table], r9,  lsl #2          \n"     // table + row1[x0]
        "add            r7,  %[table], r7,  lsl #2          \n"     // table + row1[x1]
        "vldr.32        s0,  [r10]                          \n"
        "vldr.32        s1,  [r8]                           \n"     // d0  = [a01, a00]
        "vldr.32        s2,  [r9]                           \n"
        "vldr.32        s3,  [r7]                           \n"     // d1  = [a11, a10]
        "vmull.u8       q3,  d0, d30                        \n"     // q3  = [a01, a00] * (16-y)
        "vmull.u8       q4,  d1, d31                        \n"     // q4  = [a11, a10] * y
        "vsub.i16       d27, d28, d26                       \n"     // d27 = [16-Xa].16

        "vmul.i16       d2,  d7,  d26                       \n"     // d4  = a01*(16-y)*x
        "vmla.i16       d2,  d9,  d26                       \n"     // d4 += a11*y*x
        "vmla.i16       d2,  d6,  d27                       \n"     // d4 += a00*(16-y)*(16-x)
        "vmla.i16       d2,  d8,  d27                       \n"     // d4 += a10*y*(16-x)
        "vshrn.i16      d2,  q1,  #8                        \n"
        "vmull.u8       q3,  d2,  d4                        \n"
        "vshrn.i16      d2,  q3,  #8                        \n"
        "vst1.32        {d2[0]}, [%[colors]]!               \n"     // store data

    "SI8_D32_Clamp_filter_DX_a_out:                         \n"
        "vpop           {q4 - q7}                           \n"
        :
        : [colors] "r" (colors), [image0] "r" (image0), [image1] "r" (image1), [fx] "r" (fx),
          [dx] "r" (dx), [count] "r" (count), [table] "r" (table)
        : "cc", "memory", "r7", "r8", "r9", "r10", "r12", "lr"
    );
}

// SI8_opaque_D32_filter_DX + ClampX_ClampY_filter_scale_neon
void Clamp_SI8_D32_filter_sale_t(const     SkBitmapProcState& s,
                                  int       x,
                                  int       y,
                                  SkPMColor dstC[],
                                  int       count)
{
    //PREAMBLE(s);

    const unsigned maxX = s.fBitmap->width() - 1;
    const SkFixed one = s.fFilterOneX;
    const SkFixed dx = s.fInvSx;

    const char     *srcAddr = (const char*)s.fBitmap->getPixels();
    const uint32_t *table   = s.fBitmap->getColorTable()->readColors();
    unsigned rb = s.fBitmap->rowBytes();
    unsigned subY;
    const uint8_t* SK_RESTRICT row0;
    const uint8_t* SK_RESTRICT row1;

    SkFixed fx;
    {
        SkPoint pt;
        s.fInvProc(s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                                  SkIntToScalar(y) + SK_ScalarHalf, &pt);
        const SkFixed fy = SkScalarToFixed(pt.fY) - (s.fFilterOneY >> 1);
        const unsigned maxY = s.fBitmap->height() - 1;
        // compute our two Y values up front

        row0 = (const uint8_t*)(srcAddr + TILEY_PROCF(fy, maxY) * rb);
        row1 = (const uint8_t*)(srcAddr + TILEY_PROCF((fy + s.fFilterOneY), maxY) * rb);
        subY = TILEY_LOW_BITS(fy, maxY);

        // now initialize fx
        fx = SkScalarToFixed(pt.fX) - (one >> 1);
    }

    int       post_count;
    SkFixed   post_fx;
    uint32_t *post_colors;
    int       num;

    post_count  = count;
    post_fx     = fx;
    post_colors = dstC;

    /*
     * process some small data out of range
     */
    if (dx > 0) {
        int end = ((int)maxX - 1) << 16;
        num = (end - fx) / dx;

        if (num < 0) {                                                  // fx > end?
            num = 0;
        }

        if (num < count) {                                              // finally out of maxX
            count       = num;
            post_count  = post_count - count;
            post_fx     = fx + count * dx;
            post_colors = post_colors + count;
        } else {
            post_count  = 0;
        }

        while (fx < 0 && count) {
            unsigned subX = TILEX_LOW_BITS(fx, maxX);
            unsigned x0   = TILEX_PROCF(fx, maxX);
            unsigned x1   = TILEX_PROCF((fx + one), maxX);

            Filter_32_opaque(subX,     subY,
                             table[row0[x0]], table[row0[x1]],
                             table[row1[x0]], table[row1[x1]],
                             dstC);
            dstC += 1;
            fx += dx;
            count--;
        }
    } else {
        int end = 0;
        int maxXFix = ((int)maxX - 1) << 16;

        num = (end - fx) / dx;
        if (num < 0) {
            num = 0;                                                    // fx < 0
        }

        if (num<count) {
            count       = num;
            post_count  = post_count - count;
            post_fx     = fx + count * dx;
            post_colors = post_colors + count;
        } else {
            post_count  = 0;
        }

        while (fx < 0 && count) {
            unsigned subX = TILEX_LOW_BITS(fx, maxX);
            unsigned x0 = TILEX_PROCF(fx, maxX);
            unsigned x1 = TILEX_PROCF((fx + one), maxX);

            Filter_32_opaque(subX,     subY,
                             table[row0[x0]], table[row0[x1]],
                             table[row1[x0]], table[row1[x1]],
                             dstC);
            dstC += 1;
            fx += dx;
            count--;
        }
    }

    if (count) {
        /*
         * set up needed const
         */
        asm volatile (
            "vdup.8         d31, %[subY]                        \n"         // d31 = [y]
            "vmov.i8        d29, #16                            \n"
            "vsub.i8        d30, d29, d31                       \n"         // d30 = [16 - y]
            "vmov.i16       q14, #16                            \n"         // q14 = [16].16
            ::[subY] "r" (subY):
        );
        SI8_D32_Clamp_filter_DX_a_t(fx, dx, count, row0, row1, dstC, table);
    }
    /*
     * process remain data
     */
    if (post_count) {
        fx   = post_fx;
        dstC = post_colors;
        while (post_count) {
            unsigned subX = TILEX_LOW_BITS(fx, maxX);
            unsigned x0   = TILEX_PROCF(fx, maxX);
            unsigned x1   = TILEX_PROCF((fx + one), maxX);

            Filter_32_opaque(subX,     subY,
                             table[row0[x0]], table[row0[x1]],
                             table[row1[x0]], table[row1[x1]],
                             dstC);
            dstC += 1;
            fx += dx;
            post_count--;
        }
    }
}

/*
 * , this branch is in proper range
 */
void S32A_D32_Clamp_filter_DX_a_t(SkFixed fx,
                                  SkFixed dx,
                                  int   count,
                                  const uint32_t * image0,
                                  const uint32_t * image1,
                                  uint32_t * colors)
{
    /*
     * , optimize this function
     * register allocator:
     * r0 --> fx,       r1 --> dx,  r2 --> count
     * r3 --> colors
     *
     * d31 --> y,       d30 --> 16 - y
     * d29 --> 16,      d28 -->
     *
     */
    asm volatile (
        "cmp            r2, #4                              \n"
        "vpush          {q4 - q7}                           \n"
        "blt            S32A_D32_Clamp_filter_DX_a_less4    \n"
        "ldr            r14, =0x3fffc                       \n"     // r14 as mask
        "sub            r2, #4                              \n"

        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xa
        "and            r7,  r14, r0, lsr #14               \n"
        "add            r8,  r7, %[image0]                  \n"
        "add            r7,  r7, %[image1]                  \n"
        "add            r0,  r0, r1                         \n"     // fx += dx
        "vdup.16        d26, r12                            \n"     // d26 = [Xa].16

    "S32A_D32_Clamp_filter_DX_a_loop4:                      \n"
        // pixel 1 ~ 2
        "vldr.32        d0,  [r8]                           \n"     // d0  = [a01, a00]
        "vldr.32        d1,  [r7]                           \n"     // d1  = [a11, a10]
        "ubfx           r12, r0, #12, #4                    \n"     // r12 = Xb
        "vmull.u8       q3,  d0, d30                        \n"     // q3  = [a01, a00] * (16-y)
        "vdup.16        d27, r12                            \n"     // d27 = [Xb]
        "vmull.u8       q4,  d1, d31                        \n"     // q4  = [a11, a10] * y

        "and            r7,  r14, r0, lsr #14               \n"
        "add            r8,  r7,  %[image0]                 \n"
        "add            r7,  r7,  %[image1]                 \n"
        "vldr.32        d2,  [r8]                           \n"     // d2  = [b01, b00]
        "vldr.32        d3,  [r7]                           \n"     // d3  = [b11, b10]
        "add            r0,  r0,  r1                        \n"     // fx += dx
        "vsub.i16       q12, q14, q13                       \n"     // q12 = [16-Xb, 16-Xa].16
        "vmull.u8       q5,  d2, d30                        \n"     // q5  = [b01, b00] * (16 - y)
        "vmull.u8       q6,  d3, d31                        \n"     // q6  = [b11, b00] *  y

        // pixel 3 ~ 4
        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xc
        "and            r7,  r14, r0, lsr #14               \n"
        "vdup.16        d22, r12                            \n"     // d22 = [Xc].16
        "add            r8,  r7, %[image0]                  \n"
        "add            r7,  r7, %[image1]                  \n"
        "add            r0,  r0, r1                         \n"     // fx += dx
        "vswp           d7,  d10                            \n"     // q3  = [b00, a00].16, q5 = [b01, a01]*(16-y)
        "vswp           d9,  d12                            \n"     // q4  = [b10, a10].16, q6 = [b11, a11].16*y
        "vldr.32        d0,  [r8]                           \n"     // d0  = [c01, c00]
        "vldr.32        d1,  [r7]                           \n"     // d1  = [c11, c10]
        "ubfx           r12, r0, #12, #4                    \n"     // r12 = Xd
        "vmull.u8       q7,  d0, d30                        \n"     // q7  = [c01, c00] * (16-y)
        "vdup.16        d23, r12                            \n"     // d23 = [Xd]
        "vmull.u8       q8,  d1, d31                        \n"     // q8  = [c11, c10] * y

        "and            r7,  r14, r0, lsr #14               \n"
        "add            r8,  r7,  %[image0]                 \n"
        "add            r7,  r7,  %[image1]                 \n"
        "vldr.32        d2,  [r8]                           \n"     // d2  = [d01, d00]
        "vldr.32        d3,  [r7]                           \n"     // d3  = [d11, d10]
        "add            r0,  r0,  r1                        \n"     // fx += dx
        "vsub.i16       q0,  q14, q11                       \n"     // q0  = [16-Xd, 16-Xc].16
        "vmull.u8       q9,  d2, d30                        \n"     // q9  = [d01, d00] * (16 - y)
        "vmull.u8       q10, d3, d31                        \n"     // q10 = [d11, d00] *  y

        "subs           r2,  r2, #4                         \n"
        "vswp           d15, d18                            \n"     // q7  = [d00, c00].16, q9 = [d01, c01].16*(16-y)
        "vswp           d17, d20                            \n"     // q8  = [d10, c10].16, q10= [d11, c11].16*y

        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xa

        "vmul.i16       q3,  q3,  q12                       \n"     // q3  = [b00, a00]*(16-y)*[16-Xb, 16-Xa]
        "vmul.i16       q7,  q7,  q0                        \n"     // q7  = [d00, c00]*(16-y)*[16-Xd, 16-Xc]
        "and            r7,  r14, r0, lsr #14               \n"
        "vmla.i16       q3,  q5,  q13                       \n"     // q3 += [b01, a01]*(16-y)*[Xb, Xa]
        "vmla.i16       q7,  q9,  q11                       \n"     // q7 += [d01, c01]*(16-y)*[Xd, Xc]
        "add            r8,  r7, %[image0]                  \n"
        "vmla.i16       q3,  q4,  q12                       \n"     // q3 += [b10, a10]*y*[16-Xb, 16-Xa]
        "vmla.i16       q7,  q8,  q0                        \n"     // q7 += [d10, c10]*y*[16-Xd, 16-Xc]
        "add            r7,  r7, %[image1]                  \n"
        "vmla.i16       q3,  q6,  q13                       \n"     // q3 += [b11, a11]*y*[Xb, Xa]
        "vmla.i16       q7,  q10, q11                       \n"     // q7 += [d11, c11]*y*[Xd, Xc]
        "add            r0,  r0, r1                         \n"     // fx += dx
        "vshrn.i16      d2,  q3, #8                         \n"     // result
        "vshrn.i16      d3,  q7, #8                         \n"     // result
        "vdup.16        d26, r12                            \n"     // d26 = [Xa].16
        "vmull.u8       q3,  d2, d4                         \n"     // multiply alpha scale
        "vmull.u8       q7,  d3, d4                         \n"
        "vshrn.i16      d2,  q3, #8                         \n"
        "vshrn.i16      d3,  q7, #8                         \n"
        "vstmia         %[colors]!, {d2, d3}                \n"     // store data
        "bge            S32A_D32_Clamp_filter_DX_a_loop4    \n"
        "adds           r2,  r2, #4                         \n"
        "beq            S32A_D32_Clamp_filter_DX_a_out      \n"
        "sub            r0,  r0, r1                         \n"     // because fx is added once more

    "S32A_D32_Clamp_filter_DX_a_less4:                      \n"
        "cmp            r2, #2                              \n"
        "blt            S32A_D32_Clamp_filter_DX_a_less2    \n"
        // pixel 1 ~ 2
        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xa
        "and            r7,  r14, r0, lsr #14               \n"
        "vdup.16        d26, r12                            \n"     // d26 = [Xa].16
        "add            r8,  r7, %[image0]                  \n"
        "add            r0,  r0, r1                         \n"     // fx += dx
        "vldr.32        d0,  [r8]                           \n"     // d0  = [a01, a00]
        "add            r8,  r7, %[image1]                  \n"
        "vldr.32        d1,  [r8]                           \n"     // d1  = [a11, a10]
        "ubfx           r12, r0, #12, #4                    \n"     // r12 = Xb
        "vmull.u8       q3,  d0, d30                        \n"     // q3  = [a01, a00] * (16-y)
        "vdup.16        d27, r12                            \n"     // d27 = [Xb]
        "vmull.u8       q4,  d1, d31                        \n"     // q4  = [a11, a10] * y

        "and            r7,  r14, r0, lsr #14               \n"
        "add            r8,  r7,  %[image0]                 \n"
        "vldr.32        d2,  [r8]                           \n"     // d2  = [b01, b00]
        "add            r8,  r7,  %[image1]                 \n"
        "vldr.32        d3,  [r8]                           \n"     // d3  = [b11, b10]
        "add            r0,  r0,  r1                        \n"     // fx += dx
        "vsub.i16       q12, q14, q13                       \n"     // q12 = [16-Xb, 16-Xa].16
        "vmull.u8       q5,  d2, d30                        \n"     // q5  = [b01, b00] * (16 - y)
        "vmull.u8       q6,  d3, d31                        \n"     // q6  = [b11, b00] *  y
        "subs           r2,  #2                             \n"
        "vswp           d7,  d10                            \n"     // q3  = [b00, a00].16, q5 = [b01, a01]*(16-y)
        "vswp           d9,  d12                            \n"     // q4  = [b10, a10].16, q6 = [b11, a11].16*y

        "vmul.i16       q3,  q3,  q12                       \n"     // q3  = [b00, a00]*(16-y)*[16-Xb, 16-Xa]
        "vmla.i16       q3,  q5,  q13                       \n"     // q3 += [b01, a01]*(16-y)*[Xb, Xa]
        "vmla.i16       q3,  q4,  q12                       \n"     // q3 += [b10, a10]*y*[16-Xb, 16-Xa]
        "vmla.i16       q3,  q6,  q13                       \n"     // q3 += [b11, a11]*y*[Xb, Xa]
        "vshrn.i16      d3,  q3,  #8                        \n"     // result
        "vmull.u8       q3,  d3,  d4                        \n"     // alpha scale
        "vshrn.i16      d3,  q3,  #8                        \n"
        "vstmia         %[colors]!, {d3}                    \n"     // store data
        "beq            S32A_D32_Clamp_filter_DX_a_out      \n"

    "S32A_D32_Clamp_filter_DX_a_less2:                      \n"
        "ubfx           r12, r0,  #12, #4                   \n"     // r12 = Xa
        "and            r7,  r14, r0, lsr #14               \n"
        "vdup.16        d26, r12                            \n"     // d26 = [Xa].16
        "add            r8,  r7, %[image0]                  \n"
        "vldr.32        d0,  [r8]                           \n"     // d0  = [a01, a00]
        "add            r8,  r7, %[image1]                  \n"
        "vldr.32        d1,  [r8]                           \n"     // d1  = [a11, a10]
        "vmull.u8       q3,  d0, d30                        \n"     // q3  = [a01, a00] * (16-y)
        "vmull.u8       q4,  d1, d31                        \n"     // q4  = [a11, a10] * y
        "vsub.i16       d27, d28, d26                       \n"     // d27 = [16-Xa].16

        "vmul.i16       d2,  d7,  d26                       \n"     // d4  = a01*(16-y)*x
        "vmla.i16       d2,  d9,  d26                       \n"     // d4 += a11*y*x
        "vmla.i16       d2,  d6,  d27                       \n"     // d4 += a00*(16-y)*(16-x)
        "vmla.i16       d2,  d8,  d27                       \n"     // d4 += a10*y*(16-x)
        "vshrn.i16      d2,  q1,  #8                        \n"
        "vmull.u8       q3,  d2,  d4                        \n"
        "vshrn.i16      d2,  q3,  #8                        \n"
        "vst1.32        {d2[0]}, [%[colors]]!               \n"     // store data

    "S32A_D32_Clamp_filter_DX_a_out:                        \n"
        "vpop           {q4 - q7}                           \n"
        :
        : [colors] "r" (colors), [image0] "r" (image0), [image1] "r" (image1), [fx] "r" (fx),
          [dx] "r" (dx), [count] "r" (count)
        : "cc", "memory", "r7", "r8", "r12", "lr"
    );
}

// S32_alpha_D32_filter_DX + ClampX_ClampY_filter_scale_neon
void Clamp_S32A_D32_filter_sale_t(const     SkBitmapProcState& s,
                                  int       x,
                                  int       y,
                                  SkPMColor dstC[],
                                  int       count)
{
    //PREAMBLE(s);

    const unsigned maxX = s.fBitmap->width() - 1;
    const SkFixed one = s.fFilterOneX;
    const SkFixed dx = s.fInvSx;

    unsigned alphaScale = s.fAlphaScale;
    const char* SK_RESTRICT srcAddr = (const char*)s.fBitmap->getPixels();
    unsigned rb = s.fBitmap->rowBytes();
    unsigned subY;
    const uint32_t* SK_RESTRICT row0;
    const uint32_t* SK_RESTRICT row1;

    SkFixed fx;
    {
        SkPoint pt;
        s.fInvProc(s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                                  SkIntToScalar(y) + SK_ScalarHalf, &pt);
        const SkFixed fy = SkScalarToFixed(pt.fY) - (s.fFilterOneY >> 1);
        const unsigned maxY = s.fBitmap->height() - 1;
        // compute our two Y values up front

        row0 = (const uint32_t*)(srcAddr + TILEY_PROCF(fy, maxY) * rb);
        row1 = (const uint32_t*)(srcAddr + TILEY_PROCF((fy + s.fFilterOneY), maxY) * rb);
        subY = TILEY_LOW_BITS(fy, maxY);

        // now initialize fx
        fx = SkScalarToFixed(pt.fX) - (one >> 1);
    }

    if (alphaScale == 256) {                                            // of course, scale alpha to 8 bits
        alphaScale -= 1;
    }

    int       post_count;
    SkFixed   post_fx;
    uint32_t *post_colors;
    int       num;

    post_count  = count;
    post_fx     = fx;
    post_colors = dstC;

    /*
     * process some small data out of range
     */
    if (dx > 0) {
        int end = ((int)maxX - 1) << 16;
        num = (end - fx) / dx;

        if (num < 0) {                                                  // fx > end?
            num = 0;
        }

        if (num < count) {                                              // finally out of maxX
            count       = num;
            post_count  = post_count - count;
            post_fx     = fx + count * dx;
            post_colors = post_colors + count;
        } else {
            post_count  = 0;
        }

        while (fx < 0 && count) {
            unsigned subX = TILEX_LOW_BITS(fx, maxX);
            unsigned x0   = TILEX_PROCF(fx, maxX);
            unsigned x1   = TILEX_PROCF((fx + one), maxX);

            Filter_32_alpha(subX,     subY,
                            row0[x0], row0[x1],
                            row1[x0], row1[x1],
                            dstC,     alphaScale);
            dstC += 1;
            fx += dx;
            count--;
        }
    } else {
        int end = 0;
        int maxXFix = ((int)maxX - 1) << 16;

        num = (end - fx) / dx;
        if (num < 0) {
            num = 0;                                                    // fx < 0
        }

        if (num<count) {
            count       = num;
            post_count  = post_count - count;
            post_fx     = fx + count * dx;
            post_colors = post_colors + count;
        } else {
            post_count  = 0;
        }

        while (fx < 0 && count) {
            unsigned subX = TILEX_LOW_BITS(fx, maxX);
            unsigned x0 = TILEX_PROCF(fx, maxX);
            unsigned x1 = TILEX_PROCF((fx + one), maxX);

            Filter_32_alpha(subX,     subY,
                            row0[x0], row0[x1],
                            row1[x0], row1[x1],
                            dstC,     alphaScale);
            dstC += 1;
            fx += dx;
            count--;
        }
    }

    if (count) {
        /*
         * set up needed const
         */
        asm volatile (
            "vdup.8         d4,  %[alpha]                       \n"
            "vdup.8         d31, %[subY]                        \n"         // d31 = [y]
            "vmov.i8        d29, #16                            \n"
            "vsub.i8        d30, d29, d31                       \n"         // d30 = [16 - y]
            "vmov.i16       q14, #16                            \n"         // q14 = [16].16
            ::[subY] "r" (subY), [alpha] "r" (alphaScale) :
        );
        S32A_D32_Clamp_filter_DX_a_t(fx, dx, count, row0, row1, dstC);
    }

    /*
     * process remain data
     */
    if (post_count) {
        fx   = post_fx;
        dstC = post_colors;
        while (post_count) {
            unsigned subX = TILEX_LOW_BITS(fx, maxX);
            unsigned x0   = TILEX_PROCF(fx, maxX);
            unsigned x1   = TILEX_PROCF((fx + one), maxX);

            Filter_32_alpha(subX,     subY,
                            row0[x0], row0[x1],
                            row1[x0], row1[x1],
                            dstC,     alphaScale);
            dstC += 1;
            fx += dx;
            post_count--;
        }
    }
}

// S32_opaque_D32_filter_DXDY + ClampX_ClampY_filter_persp_neon
void ClampXY_S32_D32_filter_persp_t(const     SkBitmapProcState& s,
                                    int       x,
                                    int       y,
                                    SkPMColor dstC[],
                                    int       count)
{
    SkASSERT(s.fInvType & SkMatrix::kPerspective_Mask);

    if (count == 0) {
        return;
    }
    //PREAMBLE(s);
    /* max{X,Y} are int here, but later shown/assumed to fit in 16 bits */
    int maxX = s.fBitmap->width() - 1;
    int maxY = s.fBitmap->height() - 1;
    SkFixed oneX = SK_Fixed1;
    SkFixed oneY = SK_Fixed1;

    const char* SK_RESTRICT srcAddr = (const char*)s.fBitmap->getPixels();
    int rb = s.fBitmap->rowBytes();

    SkPoint pt;
    SkFixed x_x;
    SkFixed y_y;
    SkFixed fSX, fSY;
    SkFixed dx, dy;

    SkMatrix::Persp_xy(s.fInvMatrix,
                       SkIntToScalar(x) + SK_ScalarHalf,
                       SkIntToScalar(y) + SK_ScalarHalf , &pt);

    fSX = SkIntToScalar(x) + SK_ScalarHalf;
    fSY = SkIntToScalar(y) + SK_ScalarHalf;
    x_x = SkScalarToFixed(pt.fX);
    y_y = SkScalarToFixed(pt.fY);

    fSX += SkIntToScalar(count);
    SkMatrix::Persp_xy(s.fInvMatrix, fSX, fSY, &pt);
    dx = (SkScalarToFixed(pt.fX) - x_x) / count;
    dy = (SkScalarToFixed(pt.fY) - y_y) / count;

    /*
     * check if final x, y and start x, y is in proper value
     * most is the follow case
     */
#if 0
    SkFixed final_x, final_y;
    if (dx > 0) {
        final_x = (x_x + (count - 1) * dx + (oneX >> 1)) >> 16;
    } else {
        final_x = (x_x + (count - 1) * dx - (oneX >> 1)) >> 16;
    }
    if (dy > 0) {
        final_y = (y_y + (count - 1) * dy + (oneY >> 1)) >> 16;
    } else {
        final_y = (y_y + (count - 1) * dy - (oneY >> 1)) >> 16;
    }
    if ((final_x <= maxX) && (final_x >= 0) &&
        (final_y <= maxY) && (final_y >= 0) &&
        ((x_x >> 16) <= maxX) && ((x_x >> 16) >= 0) &&
        ((y_y >> 16) <= maxY) && ((y_y >> 16) >= 0)){

        while (count > 0) {
            unsigned y0 = (y_y - (oneY >> 1)) >> 16;
            unsigned y1 = (y_y - (oneY >> 1) + oneY) >> 16;
            unsigned subY = TILEY_LOW_BITS(y_y - (oneY >> 1), maxY);

            unsigned x0 = (x_x - (oneX >> 1)) >> 16;
            unsigned x1 = (x_x - (oneX >> 1) + oneX) >> 16;
            unsigned subX = TILEX_LOW_BITS(x_x - (oneX >> 1), maxX);

            const uint32_t* SK_RESTRICT row0 = (const uint32_t*)(srcAddr + y0 * rb);
            const uint32_t* SK_RESTRICT row1 = (const uint32_t*)(srcAddr + y1 * rb);

            Filter_32_opaque(subX,     subY,
                             row0[x0], row0[x1],
                             row1[x0], row1[x1],
                             dstC);
            dstC += 1;
            x_x += dx;
            y_y += dy;
            count--;
        }
    } else
#endif
    {
        while (count > 0) {
            unsigned y0 = TILEY_PROCF(y_y - (oneY >> 1), maxY);
            unsigned y1 = TILEY_PROCF((y_y -(oneY >> 1) + oneY), maxY);
            unsigned subY = TILEY_LOW_BITS(y_y - (oneY >> 1), max);

            unsigned x0 = TILEX_PROCF(x_x - (oneX >> 1), maxX);
            unsigned x1 = TILEX_PROCF(x_x - (oneX >> 1) + oneX, maxX);
            unsigned subX = TILEX_LOW_BITS(x_x - (oneX >> 1), maxX);

            const uint32_t* SK_RESTRICT row0 = (const uint32_t*)(srcAddr + y0 * rb);
            const uint32_t* SK_RESTRICT row1 = (const uint32_t*)(srcAddr + y1 * rb);

            Filter_32_opaque(subX,     subY,
                             row0[x0], row0[x1],
                             row1[x0], row1[x1],
                             dstC);
            dstC += 1;
            x_x += dx;
            y_y += dy;
            count--;
        }
    }
}

// S32_alpha_D32_filter_DXDY + ClampX_ClampY_filter_persp_neon
void ClampXY_S32A_D32_filter_persp_t(const     SkBitmapProcState& s,
                                     int       x,
                                     int       y,
                                     SkPMColor dstC[],
                                     int       count)
{
    SkASSERT(s.fInvType & SkMatrix::kPerspective_Mask);

    if (count == 0) {
        return;
    }
    //PREAMBLE(s);
    /* max{X,Y} are int here, but later shown/assumed to fit in 16 bits */
    int maxX = s.fBitmap->width() - 1;
    int maxY = s.fBitmap->height() - 1;
    SkFixed oneX = SK_Fixed1;
    SkFixed oneY = SK_Fixed1;

    const char* SK_RESTRICT srcAddr = (const char*)s.fBitmap->getPixels();
    int rb = s.fBitmap->rowBytes();
    unsigned alphaScale = s.fAlphaScale;

    SkPoint pt;
    SkFixed x_x;
    SkFixed y_y;
    SkFixed fSX, fSY;
    SkFixed dx, dy;

    SkMatrix::Persp_xy(s.fInvMatrix,
                       SkIntToScalar(x) + SK_ScalarHalf,
                       SkIntToScalar(y) + SK_ScalarHalf , &pt);

    fSX = SkIntToScalar(x) + SK_ScalarHalf;
    fSY = SkIntToScalar(y) + SK_ScalarHalf;
    x_x = SkScalarToFixed(pt.fX);
    y_y = SkScalarToFixed(pt.fY);

    fSX += SkIntToScalar(count);
    SkMatrix::Persp_xy(s.fInvMatrix, fSX, fSY, &pt);
    dx = (SkScalarToFixed(pt.fX) - x_x) / count;
    dy = (SkScalarToFixed(pt.fY) - y_y) / count;

    /*
     * check if final x, y and start x, y is in proper value
     * most is the follow case
     */
#if 0
    SkFixed final_x, final_y;
    if (dx > 0) {
        final_x = (x_x + (count - 1) * dx + (oneX >> 1)) >> 16;
    } else {
        final_x = (x_x + (count - 1) * dx - (oneX >> 1)) >> 16;
    }
    if (dy > 0) {
        final_y = (y_y + (count - 1) * dy + (oneY >> 1)) >> 16;
    } else {
        final_y = (y_y + (count - 1) * dy - (oneY >> 1)) >> 16;
    }
    if ((final_x <= maxX) && (final_x >= 0) &&
        (final_y <= maxY) && (final_y >= 0) &&
        ((x_x >> 16) <= maxX) && ((x_x >> 16) >= 0) &&
        ((y_y >> 16) <= maxY) && ((y_y >> 16) >= 0)){

        while (count > 0) {
            unsigned y0 = (y_y - (oneY >> 1)) >> 16;
            unsigned y1 = (y_y - (oneY >> 1) + oneY) >> 16;
            unsigned subY = TILEY_LOW_BITS(y_y - (oneY >> 1), maxY);

            unsigned x0 = (x_x - (oneX >> 1)) >> 16;
            unsigned x1 = (x_x - (oneX >> 1) + oneX) >> 16;
            unsigned subX = TILEX_LOW_BITS(x_x - (oneX >> 1), maxX);

            const uint32_t* SK_RESTRICT row0 = (const uint32_t*)(srcAddr + y0 * rb);
            const uint32_t* SK_RESTRICT row1 = (const uint32_t*)(srcAddr + y1 * rb);

            Filter_32_opaque(subX,     subY,
                             row0[x0], row0[x1],
                             row1[x0], row1[x1],
                             dstC);
            dstC += 1;
            x_x += dx;
            y_y += dy;
            count--;
        }
    } else
#endif
    {
        while (count > 0) {
            unsigned y0 = TILEY_PROCF(y_y - (oneY >> 1), maxY);
            unsigned y1 = TILEY_PROCF((y_y -(oneY >> 1) + oneY), maxY);
            unsigned subY = TILEY_LOW_BITS(y_y - (oneY >> 1), max);

            unsigned x0 = TILEX_PROCF(x_x - (oneX >> 1), maxX);
            unsigned x1 = TILEX_PROCF(x_x - (oneX >> 1) + oneX, maxX);
            unsigned subX = TILEX_LOW_BITS(x_x - (oneX >> 1), maxX);

            const uint32_t* SK_RESTRICT row0 = (const uint32_t*)(srcAddr + y0 * rb);
            const uint32_t* SK_RESTRICT row1 = (const uint32_t*)(srcAddr + y1 * rb);

            Filter_32_alpha(subX,     subY,
                            row0[x0], row0[x1],
                            row1[x0], row1[x1],
                            dstC,     alphaScale);
            dstC += 1;
            x_x += dx;
            y_y += dy;
            count--;
        }
    }
}
#undef MAKENAME
#undef TILEX_PROCF
#undef TILEY_PROCF
#ifdef CHECK_FOR_DECAL
    #undef CHECK_FOR_DECAL
#endif

#undef SCALE_FILTER_NAME
#undef AFFINE_FILTER_NAME
#undef PERSP_FILTER_NAME

#undef PREAMBLE
#undef PREAMBLE_PARAM_X
#undef PREAMBLE_PARAM_Y
#undef PREAMBLE_ARG_X
#undef PREAMBLE_ARG_Y

#undef TILEX_LOW_BITS
#undef TILEY_LOW_BITS

/////////////////////////////////////////////////////////////////////
// fre repeat
#define TILEX_PROCF(fx, max)    SK_USHIFT16(((fx) & 0xFFFF) * ((max) + 1))
#define TILEY_PROCF(fy, max)    SK_USHIFT16(((fy) & 0xFFFF) * ((max) + 1))
#define TILEX_LOW_BITS(fx, max) ((((fx) & 0xFFFF) * ((max) + 1) >> 12) & 0xF)
#define TILEY_LOW_BITS(fy, max) ((((fy) & 0xFFFF) * ((max) + 1) >> 12) & 0xF)

void SI8_D32_RepeatX_nofilter_t(const uint32_t *table, uint32_t *dstC, int count)
{
    asm volatile (
        "cmp        %[count], #8                                \n"     // count < 8
        "blt        RepeatX_SI8_D32_nofilter_scale_t_less8      \n"

    "RepeatX_SI8_D32_nofilter_scale_t_loop8:                    \n"
        "vmull.u16  q8, d0, d4                                  \n"     // [fx + 3dx .. fx] * (maxX + 1)
        "vmull.u16  q9, d1, d4                                  \n"     // [fx + 7dx .. fx + 4dx] * (maxX +1)
        "vadd.i16   d0, d0, d2                                  \n"     // [fx + 3dx .. fx] + 8dx
        "vadd.i16   d1, d1, d2                                  \n"     // [fx + 7dx .. fx + 4dx] + 8dx
        "sub        %[count], %[count], #8                      \n"     // count -= 8
        "vshr.u32   q8, q8, #16                                 \n"     // >> 16
        "vshr.u32   q9, q9, #16                                 \n"     // >> 16

        "vadd.i32   q8, q8, q15                                 \n"     // add srcAddr
        "vadd.i32   q9, q9, q15                                 \n"     // add srcAddr
        "vmov       r4,  r5,  d16                               \n"     // d16
        "vmov       r6,  r7,  d17                               \n"     // d17
        "vmov       r8,  r9,  d18                               \n"     // d18
        "vmov       r10, r11, d19                               \n"     // d19
        "ldrb       r4,  [r4]                                   \n"
        "ldrb       r5,  [r5]                                   \n"
        "ldrb       r6,  [r6]                                   \n"
        "ldrb       r7,  [r7]                                   \n"
        "ldrb       r8,  [r8]                                   \n"
        "ldrb       r9,  [r9]                                   \n"
        "ldrb       r10, [r10]                                  \n"
        "ldrb       r11, [r11]                                  \n"
        "ldr        r4,  [%[table], r4,  lsl #2]                \n"
        "ldr        r5,  [%[table], r5,  lsl #2]                \n"
        "ldr        r6,  [%[table], r6,  lsl #2]                \n"
        "ldr        r7,  [%[table], r7,  lsl #2]                \n"
        "ldr        r8,  [%[table], r8,  lsl #2]                \n"
        "ldr        r9,  [%[table], r9,  lsl #2]                \n"
        "ldr        r10, [%[table], r10, lsl #2]                \n"
        "ldr        r11, [%[table], r11, lsl #2]                \n"
        "cmp        %[count], #8                                \n"
        "stmia      %[dstC]!, {r4 - r11}                        \n"
        "bge        RepeatX_SI8_D32_nofilter_scale_t_loop8      \n"
        "cmp        %[count], #0                                \n"
        "beq        RepeatX_SI8_D32_nofilter_scale_t_out        \n"

    "RepeatX_SI8_D32_nofilter_scale_t_less8:                    \n"
        "cmp        %[count], #4                                \n"
        "blt        RepeatX_SI8_D32_nofilter_scale_t_less4      \n"
        "vmull.u16  q8, d0, d4                                  \n"     // [fx + 3dx .. fx] * (maxX + 1)
        "vadd.i16   d0, d0, d3                                  \n"     // [fx + 3dx .. fx] + 4dx
        "subs       %[count], %[count], #4                      \n"     // count -= 4
        "vshr.u32   q8, q8, #16                                 \n"     // >> 16
        "vadd.i32   q8, q8, q15                                 \n"     // add srcAddr
        "vmov       r4,  r5,  d16                               \n"     // d16
        "vmov       r6,  r7,  d17                               \n"     // d17
        "ldrb       r4,  [r4]                                   \n"
        "ldrb       r5,  [r5]                                   \n"
        "ldrb       r6,  [r6]                                   \n"
        "ldrb       r7,  [r7]                                   \n"
        "ldr        r4,  [%[table], r4,  lsl #2]                \n"
        "ldr        r5,  [%[table], r5,  lsl #2]                \n"
        "ldr        r6,  [%[table], r6,  lsl #2]                \n"
        "ldr        r7,  [%[table], r7,  lsl #2]                \n"
        "stmia      %[dstC]!, {r4 - r7}                         \n"
        "beq        RepeatX_SI8_D32_nofilter_scale_t_out        \n"

    "RepeatX_SI8_D32_nofilter_scale_t_less4:                    \n"
        "vmull.u16  q8, d0, d4                                  \n"     // [fx + 3dx .. fx] * (maxX + 1)
        "vadd.i16   d0, d0, d3                                  \n"     // [fx + 3dx .. fx] + 2dx
        "vshr.u32   q8, q8, #16                                 \n"     // >> 16
        "vadd.i32   q8, q8, q15                                 \n"     // add srcAddr
        "vmov       r4, r5, d16                                 \n"     // d16
        "ldrb       r4,  [r4]                                   \n"
        "ldrb       r5,  [r5]                                   \n"
        "ldr        r4,  [%[table], r4,  lsl #2]                \n"
        "ldr        r5,  [%[table], r5,  lsl #2]                \n"
        "stmia      %[dstC]!, {r4 - r5}                         \n"
    "RepeatX_SI8_D32_nofilter_scale_t_out:                      \n"
        :
        :[count] "r" (count), [dstC] "r" (dstC),
         [table] "r" (table)
        :"memory", "cc", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11"
    );
}

// SI8_opaque_D32_nofilter_DX + RepeatX_RepeatY_nofilter_scale_neon
void RepeatX_SI8_D32_nofilter_scale_t(const     SkBitmapProcState& s,
                                      int       x,
                                      int       y,
                                      SkPMColor dstC[],
                                      int       count)
{
    SkASSERT((s.fInvType & ~(SkMatrix::kTranslate_Mask |
                             SkMatrix::kScale_Mask)) == 0);

    //PREAMBLE(s);

    const unsigned maxX = s.fBitmap->width() - 1;
    const uint32_t *table = s.fBitmap->getColorTable()->readColors();
    const uint8_t  *srcAddr = (const uint8_t*)s.fBitmap->getPixels();
    SkFixed fx;
    {
        SkPoint pt;
        s.fInvProc(s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                                  SkIntToScalar(y) + SK_ScalarHalf, &pt);
        fx = SkScalarToFixed(pt.fY);
        const unsigned maxY = s.fBitmap->height() - 1;
        srcAddr = (const uint8_t*)((const char*)srcAddr + TILEY_PROCF(fx, maxY) * s.fBitmap->rowBytes());
        fx = SkScalarToFixed(pt.fX);
    }

    if (0 == maxX) {
        // all of the following X values must be 0
        sk_memset32(dstC, table[srcAddr[0]], count);
        return;
    }

    const SkFixed dx = s.fInvSx;

    if (count & 0x01) {
        *dstC++ = table[srcAddr[TILEX_PROCF(fx, maxX)]];
        fx += dx;
        count -= 1;
    }

    if (count) {
        // set up needed const
        asm volatile (
            "vdup.16    d2, %[dx]                                   \n"     // d2 = [dx, dx, dx, dx].16
            "add        r4, %[maxX], #1                             \n"     // r4 = maxX + 1
            "vdup.16    d4, r4                                      \n"     // d4 = maxX + 1
            "vmov.16    d0[0], %[fx]                                \n"     // d0[0] = fx & 0xffff
            "vshl.i16   d3, d2, #2                                  \n"     // d3 = [4dx, 4dx, 4dx, 4dx].16
            "vshl.i16   d2, d2, #3                                  \n"     // d2 = [8dx, 8dx, 8dx, 8dx].16
            "add        r4, %[fx], %[dx]                            \n"     // r4 = fx + dx
            "vmov.16    d0[1], r4                                   \n"     // d0[1] = (fx +  dx) & 0xffff
            "add        r4, r4, %[dx]                               \n"     // r4 += dx
            "vmov.16    d0[2], r4                                   \n"     // d0[2] = (fx + 2dx) & 0xffff
            "add        r4, r4, %[dx]                               \n"     // r4 += dx
            "vmov.16    d0[3], r4                                   \n"     // d0[3] = (fx + 3dx) & 0xffff
            "vadd.i16   d1, d0, d3                                  \n"     // d1 = [fx + 7dx, fx + 6dx ..].16
            "vdup.32    q15, %[srcAddr]                             \n"     // q15 = [srcAddr]
            :
            : [dx] "r" (dx), [maxX] "r" (maxX), [fx] "r" (fx),
              [srcAddr] "r" (srcAddr)
            :
        );
        SI8_D32_RepeatX_nofilter_t(table, dstC, count);
    }
}


// tao.zeng, add
// SI8_opaque_D32_filter_DX + RepeatX_RepeatY_filter_scale
void RepeatX_SI8_D32_filter_scale_t(const     SkBitmapProcState& s,
                                    int       x,
                                    int       y,
                                    SkPMColor dstC[],
                                    int       count)
{
    SkASSERT((s.fInvType & ~(SkMatrix::kTranslate_Mask |
                             SkMatrix::kScale_Mask)) == 0);
    SkASSERT(s.fInvKy == 0);

    //PREAMBLE(s);

    const uint32_t *table   = s.fBitmap->getColorTable()->readColors();
    const uint8_t  *srcAddr = (const uint8_t*)s.fBitmap->getPixels();
    const uint8_t  *row0;
    const uint8_t  *row1;
    unsigned rb = s.fBitmap->rowBytes();
    unsigned subY;
    const unsigned maxX = s.fBitmap->width() - 1;
    const SkFixed one = s.fFilterOneX;
    const SkFixed dx = s.fInvSx;
    SkFixed fx;

    {
        SkPoint pt;
        s.fInvProc(s.fInvMatrix, SkIntToScalar(x) + SK_ScalarHalf,
                                  SkIntToScalar(y) + SK_ScalarHalf, &pt);
        const SkFixed fy = SkScalarToFixed(pt.fY) - (s.fFilterOneY >> 1);
        const unsigned maxY = s.fBitmap->height() - 1;
        // compute our two Y values up front
        row0 = (const uint8_t*)(srcAddr + TILEY_PROCF(fy, maxY) * rb);
        row1 = (const uint8_t*)(srcAddr + TILEY_PROCF(fy + s.fFilterOneY, maxY) * rb);
        subY = TILEY_LOW_BITS(fy, maxY);
        // now initialize fx
        fx = SkScalarToFixed(pt.fX) - (one >> 1);
    }

    do {
        unsigned x0 = TILEX_PROCF(fx, maxX);
        unsigned x1 = TILEX_PROCF((fx + one), maxX);
        unsigned subX = TILEX_LOW_BITS(fx, maxX);

        Filter_32_opaque(subX, subY,
                         table[row0[x0]],
                         table[row0[x1]],
                         table[row1[x0]],
                         table[row1[x1]],
                         dstC);
        dstC += 1;
        fx += dx;
    } while (--count != 0);
}

//////////////////////////////////////////////////////////////////////////////
#define DEBUG_MERGE                             0

#define CLAMPX_NOFILTER_TRANS                   0
#define REPEATX_NOFILTER_TRANS                  1
#define MIRRORX_NOFILTER_TRANS                  2
#define CLAMPX_CLAMPY_NOFILTER_SCALE_NEON       3
#define CLAMPX_CLAMPY_FILTER_SCALE_NEON         4
#define CLAMPX_CLAMPY_NOFILTER_AFFINE_NEON      5
#define CLAMPX_CLAMPY_FILTER_AFFINE_NEON        6
#define CLAMPX_CLAMPY_NOFILTER_PERSP_NEON       7
#define CLAMPX_CLAMPY_FILTER_PERSP_NEON         8
#define REPEATX_REPEATY_NOFILTER_SCALE_NEON     9
#define REPEATX_REPEATY_FILTER_SCALE_NEON       10
#define REPEATX_REPEATY_NOFILTER_AFFINE_NEON    11
#define REPEATX_REPEATY_FILTER_AFFINE_NEON      12
#define REPEATX_REPEATY_NOFILTER_PERSP_NEON     13
#define REPEATX_REPEATY_FILTER_PERSP_NEON       14

#define ShaderProc32_t(x) void (x)(const SkBitmapProcState&, int x, int y, \
                                   SkPMColor[], int count)

#define MatrixProc_t(x) void (x)(const SkBitmapProcState&,    \
                                 uint32_t bitmapXY[],         \
                                 int count,                   \
                                 int x, int y)

#define SampleProc32_t(x) void (x)(const SkBitmapProcState&,    \
                                   const uint32_t[],            \
                                   int count,                   \
                                   SkPMColor colors[])

/*
 * extern MatrixProc declaration
 */
extern MatrixProc_t(clampx_nofilter_trans);
extern MatrixProc_t(repeatx_nofilter_trans);
extern MatrixProc_t(mirrorx_nofilter_trans);
extern MatrixProc_t(ClampX_ClampY_nofilter_scale_neon);
extern MatrixProc_t(ClampX_ClampY_filter_scale_neon);
extern MatrixProc_t(ClampX_ClampY_nofilter_affine_neon);
extern MatrixProc_t(ClampX_ClampY_filter_affine_neon);
extern MatrixProc_t(ClampX_ClampY_nofilter_persp_neon);
extern MatrixProc_t(ClampX_ClampY_filter_persp_neon);
extern MatrixProc_t(RepeatX_RepeatY_nofilter_scale_neon);
extern MatrixProc_t(RepeatX_RepeatY_filter_scale_neon);
extern MatrixProc_t(RepeatX_RepeatY_nofilter_affine_neon);
extern MatrixProc_t(RepeatX_RepeatY_filter_affine_neon);
extern MatrixProc_t(RepeatX_RepeatY_nofilter_persp_neon);
extern MatrixProc_t(RepeatX_RepeatY_filter_persp_neon);

/*
 * extern SampleProc32 declaration
 */
extern SampleProc32_t(S32_opaque_D32_filter_DX_neon);
extern SampleProc32_t(S32_opaque_D32_nofilter_DX_t_neon);
extern SampleProc32_t(S16_opaque_D32_nofilter_DX_t_neon);
extern SampleProc32_t(SI8_opaque_D32_nofilter_DX_neon);
extern SampleProc32_t(S32_alpha_D32_nofilter_DX_neon);
extern SampleProc32_t(S32_opaque_D32_nofilter_DXDY_neon);
extern SampleProc32_t(S32_alpha_D32_nofilter_DXDY_neon);
extern SampleProc32_t(S32_alpha_D32_filter_DX_neon);
extern SampleProc32_t(S32_opaque_D32_filter_DXDY_neon);
extern SampleProc32_t(S32_alpha_D32_filter_DXDY_neon);
extern SampleProc32_t(SI8_opaque_D32_filter_DX_t_neon);

/*
 * extern ShaderProc32 declaration
 */
extern ShaderProc32_t(S32_Opaque_D32_filter_DX_shaderproc);
extern ShaderProc32_t(ClampX_S16_D32_nofilter_trans_t);
extern ShaderProc32_t(Repeatx_S32_D32_nofilter_trans_t);
extern ShaderProc32_t(ClampX_S32_D32_nofilter_trans_t);

int getMatrixProcName(void *pfunc)
{
    int i = -1;

    if (pfunc == clampx_nofilter_trans) {
        return CLAMPX_NOFILTER_TRANS;
    }
    if (pfunc == repeatx_nofilter_trans) {
        return REPEATX_NOFILTER_TRANS;
    }
    if (pfunc == mirrorx_nofilter_trans) {
        return MIRRORX_NOFILTER_TRANS;
    }
    // ClampX_ClampY ## xxx
    if (pfunc == ClampX_ClampY_nofilter_scale_neon) {
        return CLAMPX_CLAMPY_NOFILTER_SCALE_NEON;
    }
    if (pfunc == ClampX_ClampY_filter_scale_neon) {
        return CLAMPX_CLAMPY_FILTER_SCALE_NEON;
    }
    if (pfunc == ClampX_ClampY_nofilter_affine_neon) {
        return CLAMPX_CLAMPY_NOFILTER_AFFINE_NEON;
    }
    if (pfunc == ClampX_ClampY_filter_affine_neon) {
        return CLAMPX_CLAMPY_FILTER_AFFINE_NEON;
    }
    if (pfunc == ClampX_ClampY_nofilter_persp_neon) {
        return CLAMPX_CLAMPY_NOFILTER_PERSP_NEON;
    }
    if (pfunc == ClampX_ClampY_filter_persp_neon) {
        return CLAMPX_CLAMPY_FILTER_PERSP_NEON;
    }
    // RepeatX_RepeatY ## xxx
    if (pfunc == RepeatX_RepeatY_nofilter_scale_neon) {
        return REPEATX_REPEATY_NOFILTER_SCALE_NEON;
    }
    if (pfunc == RepeatX_RepeatY_filter_scale_neon) {
        return REPEATX_REPEATY_FILTER_SCALE_NEON;
    }
    if (pfunc == RepeatX_RepeatY_nofilter_affine_neon) {
        return REPEATX_REPEATY_NOFILTER_AFFINE_NEON;
    }
    if (pfunc == RepeatX_RepeatY_filter_affine_neon) {
        return REPEATX_REPEATY_FILTER_AFFINE_NEON;
    }
    if (pfunc == RepeatX_RepeatY_nofilter_persp_neon) {
        return REPEATX_REPEATY_NOFILTER_PERSP_NEON;
    }
    if (pfunc == RepeatX_RepeatY_filter_persp_neon) {
        return REPEATX_REPEATY_FILTER_PERSP_NEON;
    }
    return i;
}

/*
 * Merge operations for sampleProc and MatrixProc
 */
SkBitmapProcState::ShaderProc32 SkBitmap_find_merge_proc(SkBitmapProcState::SampleProc32 fSampleProc32,
                                                         bool clampClamp,
                                                         SkBitmapProcState::MatrixProc fMatrixProc)
{
    SkBitmapProcState::ShaderProc32 fShaderProc32 = NULL;

    int matrix_type = getMatrixProcName((void *)fMatrixProc);

    if (S32_opaque_D32_filter_DX_neon == fSampleProc32 && clampClamp) {
        fShaderProc32 = S32_Opaque_D32_filter_DX_shaderproc;
    }

    if ((S32_opaque_D32_nofilter_DX_t_neon == fSampleProc32) &&
        (CLAMPX_CLAMPY_NOFILTER_SCALE_NEON == matrix_type)) {
        fShaderProc32 = ClampX_S32_D32_nofilter_scale_t;
    }

    if ((S16_opaque_D32_nofilter_DX_t_neon == fSampleProc32) &&
        (CLAMPX_CLAMPY_NOFILTER_SCALE_NEON == matrix_type)) {
        fShaderProc32 = ClampX_S16_D32_nofilter_scale_t;
    }

    if ((S16_opaque_D32_nofilter_DX_t_neon == fSampleProc32) &&
        (CLAMPX_NOFILTER_TRANS == matrix_type)) {
        fShaderProc32 = ClampX_S16_D32_nofilter_trans_t;
    }

    // do something to optimize this case ? hardly appear
    if ((SI8_opaque_D32_nofilter_DX_neon == fSampleProc32) &&
        (CLAMPX_CLAMPY_NOFILTER_SCALE_NEON == matrix_type)) {
        fShaderProc32 = ClampX_SI8_D32_nofilter_scale_t;
    }

    if ((S32_alpha_D32_nofilter_DX_neon == fSampleProc32) &&
        (CLAMPX_CLAMPY_NOFILTER_SCALE_NEON == matrix_type)) {
        fShaderProc32 = ClampX_S32_Alpha_D32_nofilter_scale_t;
    }

    if ((S32_opaque_D32_nofilter_DXDY_neon == fSampleProc32) &&
        (CLAMPX_CLAMPY_NOFILTER_PERSP_NEON == matrix_type)) {
        fShaderProc32 = ClampXY_S32_D32_nofilter_persp_t;
    }

    if ((S32_alpha_D32_nofilter_DXDY_neon == fSampleProc32) &&
        (CLAMPX_CLAMPY_NOFILTER_PERSP_NEON == matrix_type)) {
        fShaderProc32 = ClampXY_S32A_D32_nofilter_persp_t;
    }

    if ((S32_alpha_D32_filter_DX_neon == fSampleProc32) &&
        (CLAMPX_CLAMPY_FILTER_SCALE_NEON == matrix_type)) {
        fShaderProc32 = Clamp_S32A_D32_filter_sale_t;
    }

    if ((S32_opaque_D32_filter_DXDY_neon == fSampleProc32) &&
        (CLAMPX_CLAMPY_FILTER_PERSP_NEON == matrix_type)) {
        fShaderProc32 = ClampXY_S32_D32_filter_persp_t;
    }

    if ((S32_alpha_D32_filter_DXDY_neon == fSampleProc32) &&
        (CLAMPX_CLAMPY_FILTER_PERSP_NEON == matrix_type)) {
        fShaderProc32 = ClampXY_S32A_D32_filter_persp_t;
    }

    if ((S32_opaque_D32_nofilter_DX_t_neon == fSampleProc32) &&
        (REPEATX_NOFILTER_TRANS == matrix_type)) {
        fShaderProc32 = Repeatx_S32_D32_nofilter_trans_t;
    }

    if ((S32_opaque_D32_nofilter_DX_t_neon == fSampleProc32) &&
        (CLAMPX_NOFILTER_TRANS == matrix_type)) {
        fShaderProc32 = ClampX_S32_D32_nofilter_trans_t;
    }

    if ((SI8_opaque_D32_filter_DX_t_neon == fSampleProc32) &&
        (REPEATX_REPEATY_FILTER_SCALE_NEON == matrix_type)) {
        fShaderProc32 = RepeatX_SI8_D32_filter_scale_t;
    }

    if ((SI8_opaque_D32_nofilter_DX_neon == fSampleProc32) &&
        (REPEATX_REPEATY_NOFILTER_SCALE_NEON == matrix_type)) {
        fShaderProc32 = RepeatX_SI8_D32_nofilter_scale_t;
    }
#if DEBUG_MERGE
    ALOGD("fShaderProc32:%p, fSampleProc32:%p, fMatrixProc:%p, matrix_type:%d\n",
          fShaderProc32, fSampleProc32, fMatrixProc, matrix_type);
#endif
    return fShaderProc32;
}

#endif

/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkShader.h"
#include "include/private/base/SkTo.h"
#include "src/base/SkArenaAlloc.h"
#include "src/base/SkUtils.h"
#include "src/core/SkBlendModePriv.h"
#include "src/core/SkBlitter.h"
#include "src/core/SkColorFilterBase.h"
#include "src/core/SkColorSpacePriv.h"
#include "src/core/SkColorSpaceXformSteps.h"
#include "src/core/SkMatrixProvider.h"
#include "src/core/SkOpts.h"
#include "src/core/SkRasterPipeline.h"
#include "src/shaders/SkShaderBase.h"
#include "src/utils/SkBlitterTrace.h"

#include <semaphore.h>
#include <assert.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#define SK_BLITTER_TRACE_IS_RASTER_PIPELINE

class SkRasterPipelineBlitter final : public SkBlitter {
public:
    // This is our common entrypoint for creating the blitter once we've sorted out shaders.
    static SkBlitter* Create(const SkPixmap&, const SkPaint&, SkArenaAlloc*,
                             const SkRasterPipeline& shaderPipeline,
                             bool is_opaque, bool is_constant,
                             sk_sp<SkShader> clipShader);

    SkRasterPipelineBlitter(SkPixmap dst,
                            SkBlendMode blend,
                            SkArenaAlloc* alloc)
        : fDst(dst)
        , fBlend(blend)
        , fAlloc(alloc)
        , fColorPipeline(alloc)
    {}

    void blitH     (int x, int y, int w)                            override;
    void blitAntiH (int x, int y, const SkAlpha[], const int16_t[]) override;
    void blitAntiH2(int x, int y, U8CPU a0, U8CPU a1)               override;
    void blitAntiV2(int x, int y, U8CPU a0, U8CPU a1)               override;
    void blitMask  (const SkMask&, const SkIRect& clip)             override;
    void blitRect  (int x, int y, int width, int height)            override;
    void blitV     (int x, int y, int height, SkAlpha alpha)        override;

private:
    void blitRectWithTrace(int x, int y, int w, int h, bool trace);
    void append_load_dst      (SkRasterPipeline*) const;
    void append_store         (SkRasterPipeline*) const;

    // these check internally, and only append if there was a native clipShader
    void append_clip_scale    (SkRasterPipeline*) const;
    void append_clip_lerp     (SkRasterPipeline*) const;

    SkPixmap               fDst;
    SkBlendMode            fBlend;
    SkArenaAlloc*          fAlloc;
    SkRasterPipeline       fColorPipeline;
    // set to pipeline storage (for alpha) if we have a clipShader
    void*                  fClipShaderBuffer = nullptr; // "native" : float or U16

    SkRasterPipeline_MemoryCtx
        fDstPtr       = {nullptr,0},  // Always points to the top-left of fDst.
        fMaskPtr      = {nullptr,0};  // Updated each call to blitMask().
    SkRasterPipeline_EmbossCtx fEmbossCtx;  // Used only for k3D_Format masks.

    // We may be able to specialize blitH() or blitRect() into a memset.
    void   (*fMemset2D)(SkPixmap*, int x,int y, int w,int h, uint64_t color) = nullptr;
    uint64_t fMemsetColor = 0;   // Big enough for largest memsettable dst format, F16.

    // Built lazily on first use.
    std::function<void(size_t, size_t, size_t, size_t)> fBlitRect,
                                                        fBlitAntiH,
                                                        fBlitMaskA8,
                                                        fBlitMaskLCD16,
                                                        fBlitMask3D;

    // These values are pointed to by the blit pipelines above,
    // which allows us to adjust them from call to call.
    float fCurrentCoverage = 0.0f;
    float fDitherRate      = 0.0f;

    using INHERITED = SkBlitter;
};

#define __ATOMIC_INLINE__ static __inline__ __attribute__((always_inline))
__ATOMIC_INLINE__ int __atomic_dec(volatile int *ptr) {
  return __sync_fetch_and_sub (ptr, 1);
}
__ATOMIC_INLINE__ int __atomic_inc(volatile int *ptr) {
  return __sync_fetch_and_add (ptr, 1);
}
static volatile int worker_thread_inited = 0;
static volatile int worker_thread_busy = 0;
#define MAX_WORKER_THREADS_NUM 3

static pthread_t worker_threads[MAX_WORKER_THREADS_NUM];
sem_t sem_todo[MAX_WORKER_THREADS_NUM];
sem_t sem_done[MAX_WORKER_THREADS_NUM];
static volatile void* work[MAX_WORKER_THREADS_NUM] = {0};
static int worker_thread_num = 0;

extern const char* __progname;

static int is_not_zygote(){

    const char* buf = __progname;
    const char* zygote = "zygote";
    /*
     * adb shell cat /proc/192/cmdline
     * zygote/bin/app_process-Xzygote/system/bin--zygote--start-system-server
     */
    return strncmp(buf, zygote, 6);
}

struct shadeSpanArg {
    int x;
    int y;
    int width;
    int height;
    std::function<void(size_t, size_t, size_t, size_t)> func;
};

void* worker_blitRect(void* arg) {
    long id = (long)arg;
    while(1) {
        sem_wait(&sem_todo[id]);
        if (work[id] != 0) {
            struct shadeSpanArg* arg = (struct shadeSpanArg*)work[id];
            int x = arg->x;
            int y = arg->y;
            int width = arg->width;
            int height = arg->height;
            if (arg->func) {
               arg->func(x, y, width, height);
            }
            work[id] = 0;
        }
        sem_post(&sem_done[id]);
    }
}

SkBlitter* SkCreateRasterPipelineBlitter(const SkPixmap& dst,
                                         const SkPaint& paint,
                                         const SkMatrix& ctm,
                                         SkArenaAlloc* alloc,
                                         sk_sp<SkShader> clipShader,
                                         const SkSurfaceProps& props) {
    if (!paint.asBlendMode()) {
        // The raster pipeline doesn't support SkBlender.
        return nullptr;
    }

    SkColorSpace* dstCS = dst.colorSpace();
    SkColorType dstCT = dst.colorType();
    SkColor4f paintColor = paint.getColor4f();
    SkColorSpaceXformSteps(sk_srgb_singleton(), kUnpremul_SkAlphaType,
                           dstCS,               kUnpremul_SkAlphaType).apply(paintColor.vec());

    auto shader = as_SB(paint.getShader());

    SkRasterPipeline_<256> shaderPipeline;
    if (!shader) {
        // Having no shader makes things nice and easy... just use the paint color.
        shaderPipeline.append_constant_color(alloc, paintColor.premul().vec());
        bool is_opaque    = paintColor.fA == 1.0f,
             is_constant  = true;
        return SkRasterPipelineBlitter::Create(dst, paint, alloc,
                                               shaderPipeline, is_opaque, is_constant,
                                               std::move(clipShader));
    }

    bool is_opaque    = shader->isOpaque() && paintColor.fA == 1.0f;
    bool is_constant  = shader->isConstant();

    if (shader->appendRootStages({&shaderPipeline, alloc, dstCT, dstCS, paint, props}, ctm)) {
        if (paintColor.fA != 1.0f) {
            shaderPipeline.append(SkRasterPipelineOp::scale_1_float,
                                  alloc->make<float>(paintColor.fA));
        }
        return SkRasterPipelineBlitter::Create(dst, paint, alloc,
                                               shaderPipeline, is_opaque, is_constant,
                                               std::move(clipShader));
    }

    // The shader can't draw with SkRasterPipeline.
    return nullptr;
}

SkBlitter* SkCreateRasterPipelineBlitter(const SkPixmap& dst,
                                         const SkPaint& paint,
                                         const SkRasterPipeline& shaderPipeline,
                                         bool is_opaque,
                                         SkArenaAlloc* alloc,
                                         sk_sp<SkShader> clipShader) {
    bool is_constant = false;  // If this were the case, it'd be better to just set a paint color.
    return SkRasterPipelineBlitter::Create(dst, paint, alloc,
                                           shaderPipeline, is_opaque, is_constant,
                                           clipShader);
}

SkBlitter* SkRasterPipelineBlitter::Create(const SkPixmap& dst,
                                           const SkPaint& paint,
                                           SkArenaAlloc* alloc,
                                           const SkRasterPipeline& shaderPipeline,
                                           bool is_opaque,
                                           bool is_constant,
                                           sk_sp<SkShader> clipShader) {
    const auto bm = paint.asBlendMode();
    if (!bm) {
        return nullptr;
    }

    auto blitter = alloc->make<SkRasterPipelineBlitter>(dst, bm.value(), alloc);

    // Our job in this factory is to fill out the blitter's color pipeline.
    // This is the common front of the full blit pipelines, each constructed lazily on first use.
    // The full blit pipelines handle reading and writing the dst, blending, coverage, dithering.
    auto colorPipeline = &blitter->fColorPipeline;

    if (clipShader) {
        auto clipP = colorPipeline;
        SkPaint clipPaint;  // just need default values
        SkColorType clipCT = kRGBA_8888_SkColorType;
        SkColorSpace* clipCS = nullptr;
        SkSurfaceProps props{}; // default OK; clipShader doesn't render text
        SkStageRec rec = {clipP, alloc, clipCT, clipCS, clipPaint, props};
        if (as_SB(clipShader)->appendRootStages(rec, SkMatrix::I())) {
            struct Storage {
                // large enough for highp (float) or lowp(U16)
                float   fA[SkRasterPipeline_kMaxStride];
            };
            auto storage = alloc->make<Storage>();
            clipP->append(SkRasterPipelineOp::store_src_a, storage->fA);
            blitter->fClipShaderBuffer = storage->fA;
            is_constant = false;
        } else {
            return nullptr;
        }
    }

    // Let's get the shader in first.
    colorPipeline->extend(shaderPipeline);

    // If there's a color filter it comes next.
    if (auto colorFilter = paint.getColorFilter()) {
        SkSurfaceProps props{}; // default OK; colorFilter doesn't render text
        SkStageRec rec = {colorPipeline, alloc, dst.colorType(), dst.colorSpace(), paint, props};
        if (!as_CFB(colorFilter)->appendStages(rec, is_opaque)) {
            return nullptr;
        }
        is_opaque = is_opaque && as_CFB(colorFilter)->isAlphaUnchanged();
    }

    // Not all formats make sense to dither (think, F16).  We set their dither rate
    // to zero.  We only dither non-constant shaders, so is_constant won't change here.
    if (paint.isDither() && !is_constant) {
        switch (dst.info().colorType()) {
            case kARGB_4444_SkColorType:
                blitter->fDitherRate = 1 / 15.0f;
                break;
            case kRGB_565_SkColorType:
                blitter->fDitherRate = 1 / 63.0f;
                break;
            case kGray_8_SkColorType:
            case kRGB_888x_SkColorType:
            case kRGBA_8888_SkColorType:
            case kBGRA_8888_SkColorType:
            case kSRGBA_8888_SkColorType:
            case kR8_unorm_SkColorType:
                blitter->fDitherRate = 1 / 255.0f;
                break;
            case kRGB_101010x_SkColorType:
            case kRGBA_1010102_SkColorType:
            case kBGR_101010x_SkColorType:
            case kBGRA_1010102_SkColorType:
                blitter->fDitherRate = 1 / 1023.0f;
                break;

            case kUnknown_SkColorType:
            case kAlpha_8_SkColorType:
            case kBGR_101010x_XR_SkColorType:
            case kRGBA_F16_SkColorType:
            case kRGBA_F16Norm_SkColorType:
            case kRGBA_F32_SkColorType:
            case kR8G8_unorm_SkColorType:
            case kA16_float_SkColorType:
            case kA16_unorm_SkColorType:
            case kR16G16_float_SkColorType:
            case kR16G16_unorm_SkColorType:
            case kR16G16B16A16_unorm_SkColorType:
                blitter->fDitherRate = 0.0f;
                break;
        }
        if (blitter->fDitherRate > 0.0f) {
            colorPipeline->append(SkRasterPipelineOp::dither, &blitter->fDitherRate);
        }
    }

    // We're logically done here.  The code between here and return blitter is all optimization.

    // A pipeline that's still constant here can collapse back into a constant color.
    if (is_constant) {
        SkColor4f constantColor;
        SkRasterPipeline_MemoryCtx constantColorPtr = { &constantColor, 0 };
        // We could remove this clamp entirely, but if the destination is 8888, doing the clamp
        // here allows the color pipeline to still run in lowp (we'll use uniform_color, rather than
        // unbounded_uniform_color).
        colorPipeline->append_clamp_if_normalized(dst.info());
        colorPipeline->append(SkRasterPipelineOp::store_f32, &constantColorPtr);
        colorPipeline->run(0,0,1,1);
        colorPipeline->reset();
        colorPipeline->append_constant_color(alloc, constantColor);

        is_opaque = constantColor.fA == 1.0f;
    }

    // We can strength-reduce SrcOver into Src when opaque.
    if (is_opaque && blitter->fBlend == SkBlendMode::kSrcOver) {
        blitter->fBlend = SkBlendMode::kSrc;
    }

    // When we're drawing a constant color in Src mode, we can sometimes just memset.
    // (The previous two optimizations help find more opportunities for this one.)
    if (is_constant && blitter->fBlend == SkBlendMode::kSrc) {
        // Run our color pipeline all the way through to produce what we'd memset when we can.
        // Not all blits can memset, so we need to keep colorPipeline too.
        SkRasterPipeline_<256> p;
        p.extend(*colorPipeline);
        blitter->fDstPtr = SkRasterPipeline_MemoryCtx{&blitter->fMemsetColor, 0};
        blitter->append_store(&p);
        p.run(0,0,1,1);

        switch (blitter->fDst.shiftPerPixel()) {
            case 0: blitter->fMemset2D = [](SkPixmap* dst, int x,int y, int w,int h, uint64_t c) {
                void* p = dst->writable_addr(x,y);
                while (h --> 0) {
                    memset(p, c, w);
                    p = SkTAddOffset<void>(p, dst->rowBytes());
                }
            }; break;

            case 1: blitter->fMemset2D = [](SkPixmap* dst, int x,int y, int w,int h, uint64_t c) {
                SkOpts::rect_memset16(dst->writable_addr16(x,y), c, w, dst->rowBytes(), h);
            }; break;

            case 2: blitter->fMemset2D = [](SkPixmap* dst, int x,int y, int w,int h, uint64_t c) {
                SkOpts::rect_memset32(dst->writable_addr32(x,y), c, w, dst->rowBytes(), h);
            }; break;

            case 3: blitter->fMemset2D = [](SkPixmap* dst, int x,int y, int w,int h, uint64_t c) {
                SkOpts::rect_memset64(dst->writable_addr64(x,y), c, w, dst->rowBytes(), h);
            }; break;

            // TODO(F32)?
        }
    }

    blitter->fDstPtr = SkRasterPipeline_MemoryCtx{
        blitter->fDst.writable_addr(),
        blitter->fDst.rowBytesAsPixels(),
    };

    return blitter;
}

void SkRasterPipelineBlitter::append_load_dst(SkRasterPipeline* p) const {
    p->append_load_dst(fDst.info().colorType(), &fDstPtr);
    if (fDst.info().alphaType() == kUnpremul_SkAlphaType) {
        p->append(SkRasterPipelineOp::premul_dst);
    }
}

void SkRasterPipelineBlitter::append_store(SkRasterPipeline* p) const {
    if (fDst.info().alphaType() == kUnpremul_SkAlphaType) {
        p->append(SkRasterPipelineOp::unpremul);
    }
    p->append_store(fDst.info().colorType(), &fDstPtr);
}

void SkRasterPipelineBlitter::append_clip_scale(SkRasterPipeline* p) const {
    if (fClipShaderBuffer) {
        p->append(SkRasterPipelineOp::scale_native, fClipShaderBuffer);
    }
}

void SkRasterPipelineBlitter::append_clip_lerp(SkRasterPipeline* p) const {
    if (fClipShaderBuffer) {
        p->append(SkRasterPipelineOp::lerp_native, fClipShaderBuffer);
    }
}

void SkRasterPipelineBlitter::blitH(int x, int y, int w) {
    this->blitRect(x,y,w,1);
}

void SkRasterPipelineBlitter::blitRect(int x, int y, int w, int h) {
    this->blitRectWithTrace(x, y, w, h, true);
}

void SkRasterPipelineBlitter::blitRectWithTrace(int x, int y, int w, int h, bool trace) {
    if (fMemset2D) {
        SK_BLITTER_TRACE_STEP(blitRectByMemset,
                           trace,
                           /*scanlines=*/h,
                           /*pixels=*/w * h);
        fMemset2D(&fDst, x,y, w,h, fMemsetColor);
        return;
    }

    if (!fBlitRect) {
        SkRasterPipeline p(fAlloc);
        p.extend(fColorPipeline);
        p.append_clamp_if_normalized(fDst.info());
        if (fBlend == SkBlendMode::kSrcOver
                && (fDst.info().colorType() == kRGBA_8888_SkColorType ||
                    fDst.info().colorType() == kBGRA_8888_SkColorType)
                && !fDst.colorSpace()
                && fDst.info().alphaType() != kUnpremul_SkAlphaType
                && fDitherRate == 0.0f) {
            if (fDst.info().colorType() == kBGRA_8888_SkColorType) {
                p.append(SkRasterPipelineOp::swap_rb);
            }
            this->append_clip_scale(&p);
            p.append(SkRasterPipelineOp::srcover_rgba_8888, &fDstPtr);
        } else {
            if (fBlend != SkBlendMode::kSrc) {
                this->append_load_dst(&p);
                SkBlendMode_AppendStages(fBlend, &p);
                this->append_clip_lerp(&p);
            } else if (fClipShaderBuffer) {
                this->append_load_dst(&p);
                this->append_clip_lerp(&p);
            }
            this->append_store(&p);
        }
        fBlitRect = p.compile();
    }

    //Disable this since some issue found
    if (false && (w * h) > (750 * 400) && h >= 4) {
        if (__atomic_inc(&worker_thread_busy) == 0) {
            if (!worker_thread_inited) {
                if (__atomic_inc(&worker_thread_inited) == 0) {
                //Only start the other thread in non-zygote mode.
                    if (!is_not_zygote()) {
                        __atomic_dec(&worker_thread_busy);
                        __atomic_dec(&worker_thread_inited);
                        goto fb;
                    }
                    long i;
                    worker_thread_num = (get_nprocs_conf()/2 -1);
                    if (worker_thread_num > MAX_WORKER_THREADS_NUM) {
                        worker_thread_num = MAX_WORKER_THREADS_NUM;
                    } else if (worker_thread_num <= 2) {
                        worker_thread_num = 1;
                    }
                    for (i = 0; i < worker_thread_num; i++) {
                        sem_init(&sem_todo[i], 0, 0);
                        sem_init(&sem_done[i], 0, 0);
                        pthread_create(&worker_threads[i], NULL, worker_blitRect, (void *)i);
                    }
                }
            }
            if (worker_thread_num > 0) {
                struct shadeSpanArg msg[worker_thread_num];

                if (worker_thread_num == 1) {
                    msg[0].y = y + (h >> 1);
                    msg[0].height = h - (h >> 1);

                    h = (h >> 1);
                } else if (worker_thread_num == 3) {
                    msg[0].y = (y + (h >> 2));
                    msg[0].height = ((h >> 1) - (h >> 2));

                    msg[1].y = (y + (h >> 1));
                    msg[1].height = (h - (h >> 1)) >> 1;

                    msg[2].y = (y + (h >> 1) + ((h - (h >> 1)) >> 1));
                    msg[2].height = h - (h >> 1) - ((h - (h >> 1)) >> 1);

                    h = (h >> 2);
                } else {
                    __atomic_dec(&worker_thread_busy);
                    goto fb;
                }

                for (int i = 0; i < worker_thread_num; i++) {
                    msg[i].x = x;
                    msg[i].width = w;
                    msg[i].func = fBlitRect;
                    assert( work[i] == 0);
                    work[i] = &msg[i];

                    sem_post(&sem_todo[i]);
                }

                fBlitRect(x,y,w,h);

                for (int i = 0; i < worker_thread_num; i++) {
                    sem_wait(&sem_done[i]);

                   /* Need to set to NULL value. No point in still storing addresso of local
                    * variable, after function returns
                    */
                    work[i] = NULL;
                }
                __atomic_dec(&worker_thread_busy);
            } else {
                __atomic_dec(&worker_thread_busy);
                goto fb;
            }
            return;
        }
  }

fb:
    SK_BLITTER_TRACE_STEP(blitRect, trace, /*scanlines=*/h, /*pixels=*/w * h);
    fBlitRect(x,y,w,h);
}

void SkRasterPipelineBlitter::blitAntiH(int x, int y, const SkAlpha aa[], const int16_t runs[]) {
    if (!fBlitAntiH) {
        SkRasterPipeline p(fAlloc);
        p.extend(fColorPipeline);
        p.append_clamp_if_normalized(fDst.info());
        if (SkBlendMode_ShouldPreScaleCoverage(fBlend, /*rgb_coverage=*/false)) {
            p.append(SkRasterPipelineOp::scale_1_float, &fCurrentCoverage);
            this->append_clip_scale(&p);
            this->append_load_dst(&p);
            SkBlendMode_AppendStages(fBlend, &p);
        } else {
            this->append_load_dst(&p);
            SkBlendMode_AppendStages(fBlend, &p);
            p.append(SkRasterPipelineOp::lerp_1_float, &fCurrentCoverage);
            this->append_clip_lerp(&p);
        }

        this->append_store(&p);
        fBlitAntiH = p.compile();
    }

    SK_BLITTER_TRACE_STEP(blitAntiH, true, /*scanlines=*/1ul, /*pixels=*/0ul);
    for (int16_t run = *runs; run > 0; run = *runs) {
        SK_BLITTER_TRACE_STEP_ACCUMULATE(blitAntiH, /*pixels=*/run);
        switch (*aa) {
            case 0x00:                                break;
            case 0xff:this->blitRectWithTrace(x,y,run, 1, false); break;
            default:
                fCurrentCoverage = *aa * (1/255.0f);
                fBlitAntiH(x,y,run,1);
        }
        x    += run;
        runs += run;
        aa   += run;
    }
}

void SkRasterPipelineBlitter::blitAntiH2(int x, int y, U8CPU a0, U8CPU a1) {
    SkIRect clip = {x,y, x+2,y+1};
    uint8_t coverage[] = { (uint8_t)a0, (uint8_t)a1 };

    SkMask mask;
    mask.fImage    = coverage;
    mask.fBounds   = clip;
    mask.fRowBytes = 2;
    mask.fFormat   = SkMask::kA8_Format;

    this->blitMask(mask, clip);
}

void SkRasterPipelineBlitter::blitAntiV2(int x, int y, U8CPU a0, U8CPU a1) {
    SkIRect clip = {x,y, x+1,y+2};
    uint8_t coverage[] = { (uint8_t)a0, (uint8_t)a1 };

    SkMask mask;
    mask.fImage    = coverage;
    mask.fBounds   = clip;
    mask.fRowBytes = 1;
    mask.fFormat   = SkMask::kA8_Format;

    this->blitMask(mask, clip);
}

void SkRasterPipelineBlitter::blitV(int x, int y, int height, SkAlpha alpha) {
    SkIRect clip = {x,y, x+1,y+height};

    SkMask mask;
    mask.fImage    = &alpha;
    mask.fBounds   = clip;
    mask.fRowBytes = 0;     // so we reuse the 1 "row" for all of height
    mask.fFormat   = SkMask::kA8_Format;

    this->blitMask(mask, clip);
}

void SkRasterPipelineBlitter::blitMask(const SkMask& mask, const SkIRect& clip) {
    if (mask.fFormat == SkMask::kBW_Format) {
        // TODO: native BW masks?
        return INHERITED::blitMask(mask, clip);
    }

    // ARGB and SDF masks shouldn't make it here.
    SkASSERT(mask.fFormat == SkMask::kA8_Format
          || mask.fFormat == SkMask::kLCD16_Format
          || mask.fFormat == SkMask::k3D_Format);

    auto extract_mask_plane = [&mask](int plane, SkRasterPipeline_MemoryCtx* ctx) {
        // LCD is 16-bit per pixel; A8 and 3D are 8-bit per pixel.
        size_t bpp = mask.fFormat == SkMask::kLCD16_Format ? 2 : 1;

        // Select the right mask plane.  Usually plane == 0 and this is just mask.fImage.
        auto ptr = (uintptr_t)mask.fImage
                 + plane * mask.computeImageSize();

        // Update ctx to point "into" this current mask, but lined up with fDstPtr at (0,0).
        // This sort of trickery upsets UBSAN (pointer-overflow) so our ptr must be a uintptr_t.
        // mask.fRowBytes is a uint32_t, which would break our addressing math on 64-bit builds.
        size_t rowBytes = mask.fRowBytes;
        ctx->stride = rowBytes / bpp;
        ctx->pixels = (void*)(ptr - mask.fBounds.left() * bpp
                                  - mask.fBounds.top()  * rowBytes);
    };

    extract_mask_plane(0, &fMaskPtr);
    if (mask.fFormat == SkMask::k3D_Format) {
        extract_mask_plane(1, &fEmbossCtx.mul);
        extract_mask_plane(2, &fEmbossCtx.add);
    }

    // Lazily build whichever pipeline we need, specialized for each mask format.
    if (mask.fFormat == SkMask::kA8_Format && !fBlitMaskA8) {
        SkRasterPipeline p(fAlloc);
        p.extend(fColorPipeline);
        p.append_clamp_if_normalized(fDst.info());
        if (SkBlendMode_ShouldPreScaleCoverage(fBlend, /*rgb_coverage=*/false)) {
            p.append(SkRasterPipelineOp::scale_u8, &fMaskPtr);
            this->append_clip_scale(&p);
            this->append_load_dst(&p);
            SkBlendMode_AppendStages(fBlend, &p);
        } else {
            this->append_load_dst(&p);
            SkBlendMode_AppendStages(fBlend, &p);
            p.append(SkRasterPipelineOp::lerp_u8, &fMaskPtr);
            this->append_clip_lerp(&p);
        }
        this->append_store(&p);
        fBlitMaskA8 = p.compile();
    }
    if (mask.fFormat == SkMask::kLCD16_Format && !fBlitMaskLCD16) {
        SkRasterPipeline p(fAlloc);
        p.extend(fColorPipeline);
        p.append_clamp_if_normalized(fDst.info());
        if (SkBlendMode_ShouldPreScaleCoverage(fBlend, /*rgb_coverage=*/true)) {
            // Somewhat unusually, scale_565 needs dst loaded first.
            this->append_load_dst(&p);
            p.append(SkRasterPipelineOp::scale_565, &fMaskPtr);
            this->append_clip_scale(&p);
            SkBlendMode_AppendStages(fBlend, &p);
        } else {
            this->append_load_dst(&p);
            SkBlendMode_AppendStages(fBlend, &p);
            p.append(SkRasterPipelineOp::lerp_565, &fMaskPtr);
            this->append_clip_lerp(&p);
        }
        this->append_store(&p);
        fBlitMaskLCD16 = p.compile();
    }
    if (mask.fFormat == SkMask::k3D_Format && !fBlitMask3D) {
        SkRasterPipeline p(fAlloc);
        p.extend(fColorPipeline);
        // This bit is where we differ from kA8_Format:
        p.append(SkRasterPipelineOp::emboss, &fEmbossCtx);
        // Now onward just as kA8.
        p.append_clamp_if_normalized(fDst.info());
        if (SkBlendMode_ShouldPreScaleCoverage(fBlend, /*rgb_coverage=*/false)) {
            p.append(SkRasterPipelineOp::scale_u8, &fMaskPtr);
            this->append_clip_scale(&p);
            this->append_load_dst(&p);
            SkBlendMode_AppendStages(fBlend, &p);
        } else {
            this->append_load_dst(&p);
            SkBlendMode_AppendStages(fBlend, &p);
            p.append(SkRasterPipelineOp::lerp_u8, &fMaskPtr);
            this->append_clip_lerp(&p);
        }
        this->append_store(&p);
        fBlitMask3D = p.compile();
    }

    std::function<void(size_t,size_t,size_t,size_t)>* blitter = nullptr;
    switch (mask.fFormat) {
        case SkMask::kA8_Format:    blitter = &fBlitMaskA8;    break;
        case SkMask::kLCD16_Format: blitter = &fBlitMaskLCD16; break;
        case SkMask::k3D_Format:    blitter = &fBlitMask3D;    break;
        default:
            SkASSERT(false);
            return;
    }

    SkASSERT(blitter);
    SK_BLITTER_TRACE_STEP(blitMask,
                       true,
                       /*scanlines=*/clip.height(),
                       /*pixels=*/clip.width() * clip.height());
    (*blitter)(clip.left(),clip.top(), clip.width(),clip.height());
}

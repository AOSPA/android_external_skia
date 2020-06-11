/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**************************************************************************************************
 *** This file was autogenerated from GrAlphaThresholdFragmentProcessor.fp; do not modify.
 **************************************************************************************************/
#ifndef GrAlphaThresholdFragmentProcessor_DEFINED
#define GrAlphaThresholdFragmentProcessor_DEFINED

#include "include/core/SkM44.h"
#include "include/core/SkTypes.h"

#include "src/gpu/GrCoordTransform.h"
#include "src/gpu/GrFragmentProcessor.h"

class GrAlphaThresholdFragmentProcessor : public GrFragmentProcessor {
public:
    static std::unique_ptr<GrFragmentProcessor> Make(std::unique_ptr<GrFragmentProcessor> inputFP,
                                                     GrSurfaceProxyView mask,
                                                     float innerThreshold,
                                                     float outerThreshold,
                                                     const SkIRect& bounds) {
        return std::unique_ptr<GrFragmentProcessor>(new GrAlphaThresholdFragmentProcessor(
                std::move(inputFP), std::move(mask), innerThreshold, outerThreshold, bounds));
    }
    GrAlphaThresholdFragmentProcessor(const GrAlphaThresholdFragmentProcessor& src);
    std::unique_ptr<GrFragmentProcessor> clone() const override;
    const char* name() const override { return "AlphaThresholdFragmentProcessor"; }
    GrCoordTransform maskCoordTransform;
    int inputFP_index = -1;
    TextureSampler mask;
    float innerThreshold;
    float outerThreshold;

private:
    GrAlphaThresholdFragmentProcessor(std::unique_ptr<GrFragmentProcessor> inputFP,
                                      GrSurfaceProxyView mask,
                                      float innerThreshold,
                                      float outerThreshold,
                                      const SkIRect& bounds)
            : INHERITED(kGrAlphaThresholdFragmentProcessor_ClassID,
                        (OptimizationFlags)(inputFP ? ProcessorOptimizationFlags(inputFP.get())
                                                    : kAll_OptimizationFlags) &
                                (kCompatibleWithCoverageAsAlpha_OptimizationFlag |
                                 ((outerThreshold >= 1.0) ? kPreservesOpaqueInput_OptimizationFlag
                                                          : kNone_OptimizationFlags)))
            , maskCoordTransform(
                      SkMatrix::Translate(SkIntToScalar(-bounds.x()), SkIntToScalar(-bounds.y())),
                      mask.proxy(),
                      mask.origin())
            , mask(std::move(mask))
            , innerThreshold(innerThreshold)
            , outerThreshold(outerThreshold) {
        if (inputFP) {
            inputFP_index = this->registerChildProcessor(std::move(inputFP));
        }
        this->setTextureSamplerCnt(1);
        this->addCoordTransform(&maskCoordTransform);
    }
    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override;
    void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override;
    bool onIsEqual(const GrFragmentProcessor&) const override;
    const TextureSampler& onTextureSampler(int) const override;
    GR_DECLARE_FRAGMENT_PROCESSOR_TEST
    typedef GrFragmentProcessor INHERITED;
};
#endif

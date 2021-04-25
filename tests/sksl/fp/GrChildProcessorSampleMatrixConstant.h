

/**************************************************************************************************
 *** This file was autogenerated from GrChildProcessorSampleMatrixConstant.fp; do not modify.
 **************************************************************************************************/
#ifndef GrChildProcessorSampleMatrixConstant_DEFINED
#define GrChildProcessorSampleMatrixConstant_DEFINED

#include "include/core/SkM44.h"
#include "include/core/SkTypes.h"


#include "src/gpu/GrFragmentProcessor.h"

class GrChildProcessorSampleMatrixConstant : public GrFragmentProcessor {
public:
    static std::unique_ptr<GrFragmentProcessor> Make(std::unique_ptr<GrFragmentProcessor> child) {
        return std::unique_ptr<GrFragmentProcessor>(new GrChildProcessorSampleMatrixConstant(std::move(child)));
    }
    GrChildProcessorSampleMatrixConstant(const GrChildProcessorSampleMatrixConstant& src);
    std::unique_ptr<GrFragmentProcessor> clone() const override;
    const char* name() const override { return "ChildProcessorSampleMatrixConstant"; }
private:
    GrChildProcessorSampleMatrixConstant(std::unique_ptr<GrFragmentProcessor> child)
    : INHERITED(kGrChildProcessorSampleMatrixConstant_ClassID, kNone_OptimizationFlags) {
        this->registerChild(std::move(child), SkSL::SampleUsage::UniformMatrix("float3x3(2.0)", true));
    }
    std::unique_ptr<GrGLSLFragmentProcessor> onMakeProgramImpl() const override;
    void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override;
    bool onIsEqual(const GrFragmentProcessor&) const override;
#if GR_TEST_UTILS
    SkString onDumpInfo() const override;
#endif
    GR_DECLARE_FRAGMENT_PROCESSOR_TEST
    using INHERITED = GrFragmentProcessor;
};
#endif

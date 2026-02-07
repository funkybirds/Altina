#pragma once

#include "ShaderCompiler/ShaderPermutation.h"
#include "ShaderCompiler/ShaderReflection.h"
#include "Container/String.h"
#include "Container/Vector.h"
#include "Rhi/RhiEnums.h"
#include "Types/Aliases.h"

namespace AltinaEngine::ShaderCompiler {
    using Core::Container::FString;
    using Core::Container::TVector;

    enum class EShaderSourceLanguage : u8 {
        Hlsl = 0,
        Slang
    };

    enum class EShaderStage : u8 {
        Vertex = 0,
        Pixel,
        Compute,
        Geometry,
        Hull,
        Domain,
        Mesh,
        Amplification,
        Library
    };

    enum class EShaderOptimization : u8 {
        Debug = 0,
        Default,
        Performance,
        Size
    };

    struct FVulkanBindingOptions {
        bool mEnableAutoShift = true;
        u32  mSpace           = 0U;
        u32  mConstantBufferShift = 0U;
        u32  mTextureShift        = 1000U;
        u32  mSamplerShift        = 2000U;
        u32  mStorageShift        = 3000U;
    };

    struct FShaderMacro {
        FString mName;
        FString mValue;
    };

    struct FShaderSourceDesc {
        FString              mPath;
        FString              mEntryPoint;
        EShaderStage         mStage     = EShaderStage::Vertex;
        EShaderSourceLanguage mLanguage = EShaderSourceLanguage::Hlsl;
        TVector<FString>     mIncludeDirs;
        TVector<FShaderMacro> mDefines;
    };

    struct FShaderCompileOptions {
        Rhi::ERhiBackend     mTargetBackend      = Rhi::ERhiBackend::Unknown;
        EShaderOptimization  mOptimization       = EShaderOptimization::Default;
        bool                 mDebugInfo          = false;
        bool                 mEnableBindless     = false;
        FVulkanBindingOptions mVulkanBinding;
        FString              mTargetProfile;
        FString              mCompilerPathOverride;
        FString              mShaderModelOverride;
    };

    struct FShaderCompileRequest {
        FShaderSourceDesc    mSource;
        FShaderCompileOptions mOptions;
        FShaderPermutationId mPermutationId;
    };

    struct FShaderCompileResult {
        bool                mSucceeded = false;
        TVector<u8>         mBytecode;
        FShaderReflection   mReflection;
        FString             mDiagnostics;
        FString             mOutputDebugPath;
    };

} // namespace AltinaEngine::ShaderCompiler

#pragma once

#include "Container/String.h"
#include "Container/Vector.h"
#include "Rhi/RhiEnums.h"
#include "Types/Aliases.h"

namespace AltinaEngine::ShaderCompiler::Detail {
    using Core::Container::FString;
    using Core::Container::FNativeString;
    using Core::Container::TVector;

    enum class EAutoBindingGroup : u8 {
        PerFrame = 0,
        PerDraw,
        PerMaterial,
        Count
    };

    enum class EAutoBindingResource : u8 {
        CBuffer = 0,
        SRV,
        UAV,
        Sampler,
        Count
    };

    struct FAutoBindingLayout {
        bool mGroupUsed[static_cast<u32>(EAutoBindingGroup::Count)] = {};
        u32  mCounts[static_cast<u32>(EAutoBindingGroup::Count)]
            [static_cast<u32>(EAutoBindingResource::Count)] = {};
    };

    struct FAutoBindingOutput {
        bool               mApplied = false;
        FString            mSourcePath;
        FAutoBindingLayout mLayout;
    };

    auto ApplyAutoBindings(const FString& sourcePath, Rhi::ERhiBackend backend,
        FAutoBindingOutput& outResult, FString& diagnostics) -> bool;
} // namespace AltinaEngine::ShaderCompiler::Detail

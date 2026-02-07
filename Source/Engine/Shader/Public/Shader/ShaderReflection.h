#pragma once

#include "Container/String.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Shader {
    using Core::Container::FString;
    using Core::Container::TVector;

    enum class EShaderResourceType : u8 {
        ConstantBuffer = 0,
        Texture,
        Sampler,
        StorageBuffer,
        StorageTexture,
        AccelerationStructure
    };

    enum class EShaderResourceAccess : u8 {
        ReadOnly = 0,
        ReadWrite
    };

    struct FShaderResourceBinding {
        FString               mName;
        EShaderResourceType   mType      = EShaderResourceType::Texture;
        EShaderResourceAccess mAccess    = EShaderResourceAccess::ReadOnly;
        u32                   mSet       = 0U;
        u32                   mBinding   = 0U;
        u32                   mRegister  = 0U;
        u32                   mSpace     = 0U;
    };

    struct FShaderReflection {
        TVector<FShaderResourceBinding> mResources;
        u32                             mPushConstantBytes = 0U;
        u32                             mThreadGroupSizeX  = 1U;
        u32                             mThreadGroupSizeY  = 1U;
        u32                             mThreadGroupSizeZ  = 1U;
    };

} // namespace AltinaEngine::Shader

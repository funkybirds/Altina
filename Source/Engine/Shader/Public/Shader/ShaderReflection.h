#pragma once

#include "Container/String.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Shader {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::TVector;

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

    struct FShaderConstantBufferMember {
        FString mName;
        u32     mOffset        = 0U;
        u32     mSize          = 0U;
        u32     mElementCount  = 0U;
        u32     mElementStride = 0U;
    };

    struct FShaderConstantBuffer {
        FString                              mName;
        u32                                  mSizeBytes = 0U;
        u32                                  mSet       = 0U;
        u32                                  mBinding   = 0U;
        u32                                  mRegister  = 0U;
        u32                                  mSpace     = 0U;
        TVector<FShaderConstantBufferMember> mMembers;
    };

    struct FShaderResourceBinding {
        FString               mName;
        EShaderResourceType   mType     = EShaderResourceType::Texture;
        EShaderResourceAccess mAccess   = EShaderResourceAccess::ReadOnly;
        u32                   mSet      = 0U;
        u32                   mBinding  = 0U;
        u32                   mRegister = 0U;
        u32                   mSpace    = 0U;
    };

    struct FShaderReflection {
        TVector<FShaderResourceBinding> mResources;
        TVector<FShaderConstantBuffer>  mConstantBuffers;
        u32                             mPushConstantBytes = 0U;
        u32                             mThreadGroupSizeX  = 1U;
        u32                             mThreadGroupSizeY  = 1U;
        u32                             mThreadGroupSizeZ  = 1U;
    };

} // namespace AltinaEngine::Shader

#pragma once

#include "RenderCoreAPI.h"

#include "Container/HashMap.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Rhi/RhiStructs.h"
#include "Shader/ShaderPermutation.h"
#include "Shader/ShaderPropertyBag.h"
#include "Shader/ShaderReflection.h"
#include "Shader/ShaderRegistry.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore {
    namespace Container = Core::Container;
    using Container::FStringView;
    using Container::THashMap;
    using Container::TVector;

    using Shader::FShaderPermutationId;

    using FMaterialParamId = u32; // NOLINT

    [[nodiscard]] AE_RENDER_CORE_API auto HashMaterialParamName(FStringView name) noexcept
        -> FMaterialParamId;
    [[nodiscard]] AE_RENDER_CORE_API auto HashMaterialParamName(const TChar* name) noexcept
        -> FMaterialParamId;

    enum class EMaterialPass : u8 {
        BasePass = 0,
        DepthPass,
        ShadowPass
    };

    inline constexpr u32 kMaterialInvalidBinding = 0xFFFFFFFFu;

    struct FMaterialPassHash {
        auto operator()(EMaterialPass pass) const noexcept -> usize {
            return static_cast<usize>(pass);
        }
    };

    struct FMaterialLayout {
        Shader::FShaderPropertyBag                                            mPropertyBag;

        TVector<FMaterialParamId>                                             mTextureNameHashes;
        TVector<u32>                                                          mTextureBindings;
        TVector<u32>                                                          mSamplerBindings;

        THashMap<FMaterialParamId, Shader::FShaderPropertyBag::FPropertyDesc> mPropertyMap;

        AE_RENDER_CORE_API void                                               Reset();
        AE_RENDER_CORE_API void InitFromConstantBuffer(
            const Shader::FShaderConstantBuffer& cbuffer);
        AE_RENDER_CORE_API void AddTextureBinding(FMaterialParamId nameHash, u32 textureBinding,
            u32 samplerBinding = kMaterialInvalidBinding);
        AE_RENDER_CORE_API void SortTextureBindings();

        [[nodiscard]] auto      FindProperty(FMaterialParamId id) const noexcept
            -> const Shader::FShaderPropertyBag::FPropertyDesc*;
        [[nodiscard]] auto HasProperty(FMaterialParamId id) const noexcept -> bool {
            return FindProperty(id) != nullptr;
        }
    };

    struct FMaterialPassShaders {
        FShaderRegistry::FShaderKey mVertex;
        FShaderRegistry::FShaderKey mPixel;
        FShaderRegistry::FShaderKey mCompute;
        FShaderPermutationId        mPermutation;

        [[nodiscard]] auto          IsValid() const noexcept -> bool {
            if (mCompute.IsValid()) {
                return true;
            }
            return mVertex.IsValid();
        }
    };

    struct FMaterialPassState {
        Rhi::FRhiRasterStateDesc mRaster;
        Rhi::FRhiDepthStateDesc  mDepth;
        Rhi::FRhiBlendStateDesc  mBlend;

        AE_RENDER_CORE_API void  ApplyRasterState(const Shader::FShaderRasterState& state) noexcept;
    };

    struct FMaterialPassDesc {
        FMaterialPassShaders mShaders;
        FMaterialPassState   mState;
        FMaterialLayout      mLayout;
    };

} // namespace AltinaEngine::RenderCore

namespace AltinaEngine::Core::Container {
    template <> struct THashFunc<RenderCore::FMaterialPassDesc> {
        [[nodiscard]] auto operator()(const RenderCore::FMaterialPassDesc& passDesc) const noexcept
            -> usize {

            u64 hash = 0ULL;
            hash     = InternalHashCombine(hash, GetInternalHash(passDesc.mShaders.mVertex));
            hash     = InternalHashCombine(hash, GetInternalHash(passDesc.mShaders.mPixel));
            hash     = InternalHashCombine(hash, GetInternalHash(passDesc.mShaders.mCompute));
            hash =
                InternalHashCombine(hash, static_cast<u64>(passDesc.mShaders.mPermutation.mHash));
            hash = InternalHashCombine(hash, GetInternalHash(passDesc.mState.mRaster));
            hash = InternalHashCombine(hash, GetInternalHash(passDesc.mState.mDepth));
            hash = InternalHashCombine(hash, GetInternalHash(passDesc.mState.mBlend));
            return hash;
        }
    };
} // namespace AltinaEngine::Core::Container
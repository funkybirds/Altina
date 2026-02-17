#pragma once

#include "RenderCoreAPI.h"

#include "Container/HashMap.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Math/Vector.h"
#include "Rhi/RhiStructs.h"
#include "Shader/ShaderPermutation.h"
#include "Shader/ShaderPropertyBag.h"
#include "Shader/ShaderReflection.h"
#include "Shader/ShaderTypes.h"
#include "Shader/ShaderRegistry.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore {
    namespace Container = Core::Container;
    using Container::FStringView;
    using Container::THashMap;
    using Container::TVector;

    using Shader::FShaderPermutationId;

    using FMaterialParamId = u32; // NameHash

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
        Shader::FShaderPropertyBag PropertyBag;

        TVector<FMaterialParamId>  TextureNameHashes;
        TVector<u32>               TextureBindings;
        TVector<u32>               SamplerBindings;

        THashMap<FMaterialParamId, Shader::FShaderPropertyBag::FPropertyDesc> PropertyMap;

        void                       Reset();
        void                       InitFromConstantBuffer(const Shader::FShaderConstantBuffer& cbuffer);
        void                       AddTextureBinding(FMaterialParamId nameHash, u32 textureBinding,
                                   u32 samplerBinding = kMaterialInvalidBinding);
        void                       SortTextureBindings();

        [[nodiscard]] auto         FindProperty(FMaterialParamId id) const noexcept
            -> const Shader::FShaderPropertyBag::FPropertyDesc*;
        [[nodiscard]] auto         HasProperty(FMaterialParamId id) const noexcept -> bool {
            return FindProperty(id) != nullptr;
        }
    };

    struct FMaterialPassShaders {
        RenderCore::FShaderRegistry::FShaderKey Vertex;
        RenderCore::FShaderRegistry::FShaderKey Pixel;
        RenderCore::FShaderRegistry::FShaderKey Compute;
        FShaderPermutationId                    Permutation;

        [[nodiscard]] auto IsValid() const noexcept -> bool {
            if (Compute.IsValid()) {
                return true;
            }
            return Vertex.IsValid();
        }
    };

    struct FMaterialPassState {
        Rhi::FRhiRasterStateDesc Raster;
        Rhi::FRhiDepthStateDesc  Depth;
        Rhi::FRhiBlendStateDesc  Blend;

        void                     ApplyRasterState(const Shader::FShaderRasterState& state) noexcept;
    };

    struct FMaterialPassDesc {
        FMaterialPassShaders Shaders;
        FMaterialPassState   State;
        FMaterialLayout      Layout;
    };

} // namespace AltinaEngine::RenderCore

#pragma once

#include "RenderCoreAPI.h"

#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Rhi/RhiStructs.h"
#include "Shader/ShaderRegistry.h"

namespace AltinaEngine::RenderCore::Geometry {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;

    struct FVertexSemanticKey {
        u32                   mNameHash      = 0U;
        u32                   mSemanticIndex = 0U;

        friend constexpr auto operator==(const FVertexSemanticKey& lhs,
            const FVertexSemanticKey&                              rhs) noexcept -> bool = default;
    };

    struct FShaderVertexInputElement {
        FVertexSemanticKey mSemantic{};
        FString            mSemanticName{};
        Rhi::ERhiFormat    mFormat = Rhi::ERhiFormat::Unknown;
    };

    struct FShaderVertexInputRequirement {
        TVector<FShaderVertexInputElement> mElements{};

        void                               Reset() { mElements.Clear(); }
    };

    struct FVertexFactoryInputElement {
        FVertexSemanticKey mSemantic{};
        FString            mSemanticName{};
        Rhi::ERhiFormat    mFormat            = Rhi::ERhiFormat::Unknown;
        u32                mInputSlot         = 0U;
        u32                mAlignedByteOffset = 0U;
        bool               mPerInstance       = false;
        u32                mInstanceStepRate  = 0U;
    };

    struct FVertexFactoryProvidedLayout {
        TVector<FVertexFactoryInputElement> mElements{};

        void                                Reset() { mElements.Clear(); }
    };

    struct FResolvedVertexLayout {
        Rhi::FRhiVertexLayoutDesc mVertexLayout{};
        u64                       mLayoutHash = 0ULL;
    };

    AE_RENDER_CORE_API auto HashVertexSemanticName(FStringView semanticName) noexcept -> u32;
    AE_RENDER_CORE_API auto MakeVertexSemanticKey(
        FStringView semanticName, u32 semanticIndex) noexcept -> FVertexSemanticKey;
    AE_RENDER_CORE_API auto EncodeVertexSemanticKey(const FVertexSemanticKey& key) noexcept -> u64;

    // Bootstrap helper for migration: build requirement from an existing manual vertex layout.
    AE_RENDER_CORE_API auto BuildShaderVertexInputRequirementFromVertexLayout(
        const Rhi::FRhiVertexLayoutDesc& layoutDesc, FShaderVertexInputRequirement& outRequirement)
        -> bool;
    AE_RENDER_CORE_API auto BuildShaderVertexInputRequirementFromShaderSet(
        const RenderCore::FShaderRegistry&                      registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys,
        FShaderVertexInputRequirement& outRequirement, FString* outError = nullptr) -> bool;

    AE_RENDER_CORE_API auto ValidateAndBuildVertexLayout(
        const FShaderVertexInputRequirement& requirement,
        const FVertexFactoryProvidedLayout& provided, FResolvedVertexLayout& outResolved,
        FString* outError = nullptr) -> bool;
} // namespace AltinaEngine::RenderCore::Geometry

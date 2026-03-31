#pragma once

#include "RenderCoreAPI.h"

#include "Container/StringView.h"
#include "Container/HashMap.h"
#include "Container/Vector.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiEnums.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiStructs.h"
#include "Shader/ShaderReflection.h"
#include "Shader/ShaderRegistry.h"
#include "Shader/ShaderTypes.h"

namespace AltinaEngine::RenderCore::ShaderBinding {
    namespace Container = Core::Container;
    using Container::FStringView;
    using Container::THashMap;
    using Container::TVector;

    inline constexpr u32    kInvalidBinding = ~0U;

    AE_RENDER_CORE_API auto ToRhiShaderStageFlags(Shader::EShaderStage stage) noexcept
        -> Rhi::ERhiShaderStageFlags;

    AE_RENDER_CORE_API auto OrShaderStageFlags(Rhi::ERhiShaderStageFlags lhs,
        Rhi::ERhiShaderStageFlags rhs) noexcept -> Rhi::ERhiShaderStageFlags;

    AE_RENDER_CORE_API auto ToRhiBindingType(
        const Shader::FShaderResourceBinding& resource) noexcept -> Rhi::ERhiBindingType;

    AE_RENDER_CORE_API auto BuildBindGroupLayoutFromShaderSet(
        const RenderCore::FShaderRegistry&                      registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys, u32 setIndex,
        Rhi::FRhiBindGroupLayoutDesc& outLayoutDesc) -> bool;
    AE_RENDER_CORE_API auto BuildBindGroupLayoutFromShaders(
        const TVector<Rhi::FRhiShader*>& shaders, u32 setIndex,
        Rhi::FRhiBindGroupLayoutDesc& outLayoutDesc) -> bool;

    AE_RENDER_CORE_API auto ResolveConstantBufferBindingByName(
        const RenderCore::FShaderRegistry&                      registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys, FStringView cbufferName,
        u32& outSetIndex, u32& outBinding, Rhi::ERhiShaderStageFlags& outVisibility) -> bool;

    AE_RENDER_CORE_API auto ResolveResourceBindingByName(
        const RenderCore::FShaderRegistry&                      registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys,
        FStringView resourceName, Rhi::ERhiBindingType expectedType, u32& outSetIndex,
        u32& outBinding, Rhi::ERhiShaderStageFlags& outVisibility) -> bool;

    AE_RENDER_CORE_API auto HashBindingName(FStringView name) noexcept -> u32;

    struct FBindingLookupTable {
        u32                             mSetIndex = 0U;
        const Rhi::FRhiBindGroupLayout* mLayout   = nullptr;
        THashMap<u64, u32>              mBindingByKey;

        void                            Reset() {
            mSetIndex = 0U;
            mLayout   = nullptr;
            mBindingByKey.Clear();
        }
    };

    AE_RENDER_CORE_API auto BuildBindingLookupTable(const RenderCore::FShaderRegistry& registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys, u32 setIndex,
        const Rhi::FRhiBindGroupLayout* layout, FBindingLookupTable& outTable) -> bool;
    AE_RENDER_CORE_API auto BuildBindingLookupTableFromShaders(
        const TVector<Rhi::FRhiShader*>& shaders, u32 setIndex,
        const Rhi::FRhiBindGroupLayout* layout, FBindingLookupTable& outTable) -> bool;

    AE_RENDER_CORE_API auto FindBindingByNameHash(const FBindingLookupTable& table, u32 nameHash,
        Rhi::ERhiBindingType type, u32& outBinding) -> bool;

    class AE_RENDER_CORE_API FBindGroupBuilder {
    public:
        explicit FBindGroupBuilder(const Rhi::FRhiBindGroupLayout* layout = nullptr) noexcept
            : mLayout(layout) {}

        void Reset(const Rhi::FRhiBindGroupLayout* layout) noexcept;

        auto AddBuffer(u32 binding, Rhi::FRhiBuffer* buffer, u64 offset, u64 size) -> bool;
        auto AddSampledBuffer(u32 binding, Rhi::FRhiBuffer* buffer, u64 offset, u64 size) -> bool;
        auto AddStorageBuffer(u32 binding, Rhi::FRhiBuffer* buffer, u64 offset, u64 size) -> bool;
        auto AddTexture(u32 binding, Rhi::FRhiTexture* texture) -> bool;
        auto AddStorageTexture(u32 binding, Rhi::FRhiTexture* texture) -> bool;
        auto AddSampler(u32 binding, Rhi::FRhiSampler* sampler) -> bool;

        auto Build(Rhi::FRhiBindGroupDesc& outDesc) const -> bool;

    private:
        [[nodiscard]] auto HasLayoutBinding(u32 binding, Rhi::ERhiBindingType type) const -> bool;
        [[nodiscard]] auto HasEntry(u32 binding, Rhi::ERhiBindingType type) const -> bool;

        const Rhi::FRhiBindGroupLayout*  mLayout = nullptr;
        TVector<Rhi::FRhiBindGroupEntry> mEntries;
    };
} // namespace AltinaEngine::RenderCore::ShaderBinding

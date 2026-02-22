#pragma once

#include "RenderCoreAPI.h"

#include "Material/MaterialParameters.h"
#include "Material/MaterialPass.h"
#include "Material/MaterialTemplate.h"
#include "RenderResource.h"

#include "Container/HashMap.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiSampler.h"

namespace AltinaEngine::RenderCore {
    namespace Container = Core::Container;
    namespace Math      = Core::Math;
    using Container::THashMap;
    using Container::TShared;
    using Container::TVector;

    struct FMaterialDesc {
        u32 ShadingModel = 0U;
        u32 BlendMode    = 0U;
        u32 Flags        = 0U;
        f32 AlphaCutoff  = 0.0f;
    };

    class AE_RENDER_CORE_API FMaterial final : public FRenderResource {
    public:
        void               SetDesc(const FMaterialDesc& desc) noexcept { mDesc = desc; }
        void               SetTemplate(TShared<FMaterialTemplate> templ) noexcept;
        void               SetSchema(TShared<FMaterialSchema> schema) noexcept;

        auto               SetScalar(FMaterialParamId id, f32 value) -> bool;
        auto               SetVector(FMaterialParamId id, const Math::FVector4f& value) -> bool;
        auto               SetMatrix(FMaterialParamId id, const Math::FMatrix4x4f& value) -> bool;
        auto               SetTexture(FMaterialParamId id, Rhi::FRhiShaderResourceViewRef srv,
                          Rhi::FRhiSamplerRef sampler, u32 samplerFlags) -> bool;

        [[nodiscard]] auto GetDesc() const noexcept -> const FMaterialDesc& { return mDesc; }
        [[nodiscard]] auto GetSchema() const noexcept -> const TShared<FMaterialSchema>& {
            return mSchema;
        }
        [[nodiscard]] auto GetParameters() noexcept -> FMaterialParameterBlock& {
            return mParameters;
        }
        [[nodiscard]] auto GetParameters() const noexcept -> const FMaterialParameterBlock& {
            return mParameters;
        }
        [[nodiscard]] auto FindPassDesc(EMaterialPass pass) const noexcept
            -> const FMaterialPassDesc*;
        [[nodiscard]] auto FindShaders(EMaterialPass pass) const noexcept
            -> const FMaterialPassShaders*;
        [[nodiscard]] auto FindState(EMaterialPass pass) const noexcept
            -> const FMaterialPassState*;
        [[nodiscard]] auto FindLayout(EMaterialPass pass) const noexcept -> const FMaterialLayout*;

        [[nodiscard]] auto GetBindGroup(EMaterialPass pass) const noexcept -> Rhi::FRhiBindGroupRef;

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;
        void UpdateRHI() override;

    private:
        [[nodiscard]] auto IsSchemaTypeMatch(
            FMaterialParamId id, EMaterialParamType type) const noexcept -> bool;
        void UpdateCBuffer(const FMaterialLayout& layout, bool& outRecreated, bool& outUploaded);
        void UpdateBindGroups(const FMaterialTemplate& templ, const FMaterialLayout& defaultLayout);

        FMaterialDesc                                                           mDesc{};
        TShared<FMaterialTemplate>                                              mTemplate;
        TShared<FMaterialSchema>                                                mSchema;
        FMaterialParameterBlock                                                 mParameters;

        TVector<u8>                                                             mCBufferData;

        Rhi::FRhiBufferRef                                                      mCBuffer;
        THashMap<EMaterialPass, Rhi::FRhiBindGroupRef, FMaterialPassHash>       mBindGroups;
        THashMap<EMaterialPass, Rhi::FRhiBindGroupLayoutRef, FMaterialPassHash> mBindGroupLayouts;

        bool mDirtyCBuffer  = false;
        bool mDirtyBindings = false;
    };

} // namespace AltinaEngine::RenderCore

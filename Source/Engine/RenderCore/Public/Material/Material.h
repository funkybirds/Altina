#pragma once

#include "RenderCoreAPI.h"

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

    struct FMaterialScalarParam {
        FMaterialParamId NameHash = 0U;
        f32              Value    = 0.0f;
    };

    struct FMaterialVectorParam {
        FMaterialParamId NameHash = 0U;
        Math::FVector4f  Value    = Math::FVector4f(0.0f);
    };

    struct FMaterialTextureParam {
        FMaterialParamId               NameHash     = 0U;
        Rhi::FRhiShaderResourceViewRef SRV;
        Rhi::FRhiSamplerRef            Sampler;
        u32                            SamplerFlags = 0U;
    };

    class AE_RENDER_CORE_API FMaterial final : public FRenderResource {
    public:
        void SetDesc(const FMaterialDesc& desc) noexcept { mDesc = desc; }
        void SetTemplate(TShared<FMaterialTemplate> templ) noexcept;

        auto SetScalar(FMaterialParamId id, f32 value) -> bool;
        auto SetVector(FMaterialParamId id, const Math::FVector4f& value) -> bool;
        auto SetTexture(FMaterialParamId id, Rhi::FRhiShaderResourceViewRef srv,
            Rhi::FRhiSamplerRef sampler, u32 samplerFlags) -> bool;

        [[nodiscard]] auto GetDesc() const noexcept -> const FMaterialDesc& { return mDesc; }
        [[nodiscard]] auto FindPassDesc(EMaterialPass pass) const noexcept
            -> const FMaterialPassDesc*;
        [[nodiscard]] auto FindShaders(EMaterialPass pass) const noexcept
            -> const FMaterialPassShaders*;
        [[nodiscard]] auto FindState(EMaterialPass pass) const noexcept
            -> const FMaterialPassState*;
        [[nodiscard]] auto FindLayout(EMaterialPass pass) const noexcept
            -> const FMaterialLayout*;

        [[nodiscard]] auto GetBindGroup(EMaterialPass pass) const noexcept
            -> Rhi::FRhiBindGroupRef;

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;
        void UpdateRHI() override;

    private:
        [[nodiscard]] auto FindTextureParam(FMaterialParamId id) const noexcept
            -> const FMaterialTextureParam*;
        void               UpdateCBuffer(const FMaterialLayout& layout,
                              bool& outRecreated, bool& outUploaded);
        void               UpdateBindGroups(const FMaterialTemplate& templ,
                              const FMaterialLayout& defaultLayout);

        FMaterialDesc              mDesc{};
        TShared<FMaterialTemplate> mTemplate;

        TVector<FMaterialScalarParam>  mScalars;
        TVector<FMaterialVectorParam>  mVectors;
        TVector<FMaterialTextureParam> mTextures;

        TVector<u8> mCBufferData;

        Rhi::FRhiBufferRef mCBuffer;
        THashMap<EMaterialPass, Rhi::FRhiBindGroupRef, FMaterialPassHash> mBindGroups;
        THashMap<EMaterialPass, Rhi::FRhiBindGroupLayoutRef, FMaterialPassHash> mBindGroupLayouts;

        bool mDirtyCBuffer  = false;
        bool mDirtyBindings = false;
    };

} // namespace AltinaEngine::RenderCore

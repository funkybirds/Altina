#pragma once

#include "RenderCoreAPI.h"

#include "Material/MaterialParameters.h"
#include "Material/MaterialPass.h"

namespace AltinaEngine::RenderCore {
    class AE_RENDER_CORE_API FMaterialTemplate {
    public:
        void               SetPassDesc(EMaterialPass pass, FMaterialPassDesc desc);
        void               SetPassOverrides(EMaterialPass pass, FMaterialParameterBlock overrides);

        [[nodiscard]] auto FindPassDesc(EMaterialPass pass) const noexcept
            -> const FMaterialPassDesc*;
        [[nodiscard]] auto FindLayout(EMaterialPass pass) const noexcept -> const FMaterialLayout*;
        [[nodiscard]] auto FindShaders(EMaterialPass pass) const noexcept
            -> const FMaterialPassShaders*;
        [[nodiscard]] auto FindState(EMaterialPass pass) const noexcept
            -> const FMaterialPassState*;
        [[nodiscard]] auto FindOverrides(EMaterialPass pass) const noexcept
            -> const FMaterialParameterBlock*;
        [[nodiscard]] auto FindAnyPassDesc() const noexcept -> const FMaterialPassDesc*;

        [[nodiscard]] auto GetPasses() const noexcept
            -> const THashMap<EMaterialPass, FMaterialPassDesc, FMaterialPassHash>& {
            return mPasses;
        }
        [[nodiscard]] auto GetOverrides() const noexcept
            -> const THashMap<EMaterialPass, FMaterialParameterBlock, FMaterialPassHash>& {
            return mOverrides;
        }

    private:
        THashMap<EMaterialPass, FMaterialPassDesc, FMaterialPassHash>       mPasses;
        THashMap<EMaterialPass, FMaterialParameterBlock, FMaterialPassHash> mOverrides;
    };
} // namespace AltinaEngine::RenderCore

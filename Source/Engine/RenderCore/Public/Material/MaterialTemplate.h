#pragma once

#include "RenderCoreAPI.h"

#include "Material/MaterialPass.h"

namespace AltinaEngine::RenderCore {
    class AE_RENDER_CORE_API FMaterialTemplate {
    public:
        void SetPassDesc(EMaterialPass pass, FMaterialPassDesc desc);

        [[nodiscard]] auto FindPassDesc(EMaterialPass pass) const noexcept
            -> const FMaterialPassDesc*;
        [[nodiscard]] auto FindLayout(EMaterialPass pass) const noexcept
            -> const FMaterialLayout*;
        [[nodiscard]] auto FindShaders(EMaterialPass pass) const noexcept
            -> const FMaterialPassShaders*;
        [[nodiscard]] auto FindState(EMaterialPass pass) const noexcept
            -> const FMaterialPassState*;
        [[nodiscard]] auto FindAnyPassDesc() const noexcept -> const FMaterialPassDesc*;

        [[nodiscard]] auto GetPasses() const noexcept
            -> const THashMap<EMaterialPass, FMaterialPassDesc, FMaterialPassHash>& {
            return mPasses;
        }

    private:
        THashMap<EMaterialPass, FMaterialPassDesc, FMaterialPassHash> mPasses;
    };
} // namespace AltinaEngine::RenderCore

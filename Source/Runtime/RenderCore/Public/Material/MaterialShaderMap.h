#pragma once

#include "RenderCoreAPI.h"

#include "Material/MaterialPass.h"

namespace AltinaEngine::RenderCore {
    class AE_RENDER_CORE_API FMaterialShaderMap {
    public:
        THashMap<EMaterialPass, FMaterialPassShaders, FMaterialPassHash> PassShaders;

        [[nodiscard]] auto Find(EMaterialPass pass) const noexcept -> const FMaterialPassShaders*;
        [[nodiscard]] auto Has(EMaterialPass pass) const noexcept -> bool;
    };
} // namespace AltinaEngine::RenderCore

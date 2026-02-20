#pragma once

#include "Engine/EngineAPI.h"

#include "Container/SmartPtr.h"
#include "Material/Material.h"
#include "Material/MaterialTemplate.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

namespace AltinaEngine::Engine {
    using AltinaEngine::Move;
    namespace Container = Core::Container;
    namespace Render    = AltinaEngine::RenderCore;

    /**
     * @brief Cache mapping material assets to render-core materials.
     */
    class AE_ENGINE_API FMaterialCache {
    public:
        void SetDefaultMaterial(Render::FMaterial* material) noexcept {
            mDefaultMaterial = material;
        }
        void SetDefaultTemplate(Container::TShared<Render::FMaterialTemplate> templ) noexcept {
            mDefaultTemplate = Move(templ);
        }

        [[nodiscard]] auto ResolveDefault() -> Render::FMaterial*;
        void               PrepareMaterialForRendering(Render::FMaterial& material);

        void               Clear();

    private:
        Container::TShared<Render::FMaterialTemplate>      mDefaultTemplate;
        Render::FMaterial*                                 mDefaultMaterial = nullptr;
        Container::TShared<Render::FMaterial>              mFallbackMaterial;
    };
} // namespace AltinaEngine::Engine

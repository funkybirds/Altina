#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Asset/AssetTypes.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    namespace Asset = AltinaEngine::Asset;

    /**
     * @brief Sky cube-map binding for skybox rendering.
     *
     * Stores only an asset handle. The actual GPU resource (cube texture / SRV) should be
     * resolved by render-side systems, following the pattern used by other components (e.g.
     * asset-to-render conversion owned outside the component).
     */
    class ACLASS() AE_ENGINE_API FSkyCubeComponent : public FComponent {
    public:
        void               SetCubeMapAsset(Asset::FAssetHandle handle) noexcept;
        [[nodiscard]] auto GetCubeMapAsset() const noexcept -> Asset::FAssetHandle {
            return mCubeMapAsset;
        }

    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

    public:
        // Expected to reference the cooked skybox cube output from HDRI import (tool-side).
        // Runtime only keeps the handle; render systems decide how to translate it to RHI.
        APROPERTY()
        Asset::FAssetHandle mCubeMapAsset{};
    };
} // namespace AltinaEngine::GameScene

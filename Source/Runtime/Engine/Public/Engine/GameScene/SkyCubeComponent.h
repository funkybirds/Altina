#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Asset/AssetTypes.h"
#include "Container/Function.h"
#include "Rhi/RhiResourceView.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    namespace Asset     = AltinaEngine::Asset;
    namespace Rhi       = AltinaEngine::Rhi;
    namespace Container = Core::Container;

    /**
     * @brief Sky cube-map binding for skybox rendering.
     *
     * Stores an asset handle and lazily resolves it to RHI resources via a pluggable converter,
     * following the pattern used by other asset-backed components (e.g. StaticMeshFilter).
     */
    class ACLASS() AE_ENGINE_API FSkyCubeComponent : public FComponent {
    public:
        struct FSkyCubeRhiResources {
            Rhi::FRhiTextureRef            Texture;
            Rhi::FRhiShaderResourceViewRef SRV;

            [[nodiscard]] auto IsValid() const noexcept -> bool { return Texture && SRV; }
        };

        using FAssetToSkyCubeConverter =
            Container::TFunction<FSkyCubeRhiResources(const Asset::FAssetHandle&)>;

        void               SetCubeMapAsset(Asset::FAssetHandle handle) noexcept;
        [[nodiscard]] auto GetCubeMapAsset() const noexcept -> Asset::FAssetHandle {
            return mCubeMapAsset;
        }

        [[nodiscard]] auto GetCubeMapRhi() const noexcept -> const FSkyCubeRhiResources&;

        static FAssetToSkyCubeConverter AssetToSkyCubeConverter;

    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

    public:
        // Expected to reference the cooked skybox cube output from HDRI import (tool-side).
        // Runtime only keeps the handle; render systems decide how to translate it to RHI.
        APROPERTY()
        Asset::FAssetHandle mCubeMapAsset{};

    private:
        void                         ResolveSkyCube() const noexcept;

        mutable FSkyCubeRhiResources mRhi{};
        mutable Asset::FAssetHandle  mResolvedAsset{};
        mutable bool                 mResolved = false;
    };
} // namespace AltinaEngine::GameScene

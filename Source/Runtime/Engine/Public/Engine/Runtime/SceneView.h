#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"
#include "Asset/AssetTypes.h"
#include "Container/Vector.h"
#include "Lighting/LightTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"
#include "View/ViewData.h"

namespace AltinaEngine::Rhi {
    class FRhiViewport;
}

namespace AltinaEngine::GameScene {
    class FWorld;
    class FMeshMaterialComponent;
} // namespace AltinaEngine::GameScene

namespace AltinaEngine::RenderCore::Geometry {
    struct FStaticMeshData;
}

namespace AltinaEngine::Engine {
    namespace Container = Core::Container;
    using Container::TVector;

    enum class ESkyProviderType : u8 {
        None = 0,
        SkyCube,
        PbrSky,
    };

    struct AE_ENGINE_API FPbrSkySceneParameters {
        Core::Math::FVector3f RayleighScattering    = Core::Math::FVector3f(0.0f);
        f32                   RayleighScaleHeightKm = 0.0f;

        Core::Math::FVector3f MieScattering    = Core::Math::FVector3f(0.0f);
        Core::Math::FVector3f MieAbsorption    = Core::Math::FVector3f(0.0f);
        f32                   MieScaleHeightKm = 0.0f;
        f32                   MieAnisotropy    = 0.0f;

        Core::Math::FVector3f OzoneAbsorption     = Core::Math::FVector3f(0.0f);
        f32                   OzoneCenterHeightKm = 0.0f;
        f32                   OzoneThicknessKm    = 0.0f;

        Core::Math::FVector3f GroundAlbedo     = Core::Math::FVector3f(0.0f);
        Core::Math::FVector3f SolarTint        = Core::Math::FVector3f(0.0f);
        f32                   SolarIlluminance = 0.0f;
        f32                   SunAngularRadius = 0.0f;

        f32                   PlanetRadiusKm     = 0.0f;
        f32                   AtmosphereHeightKm = 0.0f;
        f32                   ViewHeightKm       = 0.0f;
        f32                   Exposure           = 0.0f;
        u64                   Version            = 0ULL;
    };

    struct AE_ENGINE_API FSceneView {
        GameScene::FComponentId     CameraId{};
        RenderCore::View::FViewData View{};
        enum class ETargetType : u8 {
            None = 0,
            Viewport,
            TextureAsset
        };

        struct FTargetHandle {
            ETargetType                  Type     = ETargetType::None;
            Rhi::FRhiViewport*           Viewport = nullptr;
            Asset::FAssetHandle          Texture{};

            [[nodiscard]] constexpr auto IsValid() const noexcept -> bool {
                switch (Type) {
                    case ETargetType::Viewport:
                        return Viewport != nullptr;
                    case ETargetType::TextureAsset:
                        return Texture.IsValid();
                    default:
                        return false;
                }
            }

            constexpr void Reset() noexcept {
                Type     = ETargetType::None;
                Viewport = nullptr;
                Texture  = {};
            }
        } Target{};
    };

    struct AE_ENGINE_API FSceneStaticMesh {
        GameScene::FGameObjectId                     OwnerId{};
        GameScene::FComponentId                      MeshComponentId{};
        GameScene::FComponentId                      MaterialComponentId{};
        u64                                          MeshGeometryKey = 0ULL;
        const RenderCore::Geometry::FStaticMeshData* Mesh            = nullptr;
        const GameScene::FMeshMaterialComponent*     Materials       = nullptr;
        Core::Math::FMatrix4x4f                      WorldMatrix{};
        Core::Math::FMatrix4x4f                      PrevWorldMatrix{};
    };

    struct AE_ENGINE_API FRenderScene {
        TVector<FSceneView>                   Views{};
        TVector<FSceneStaticMesh>             StaticMeshes{};
        RenderCore::Lighting::FLightSceneData Lights{};

        ESkyProviderType                      SkyProvider = ESkyProviderType::None;
        Asset::FAssetHandle                   SkyCubeAsset{};
        bool                                  bHasSkyCube = false;
        FPbrSkySceneParameters                PbrSky{};
        bool                                  bHasPbrSky = false;
    };

    struct AE_ENGINE_API FSceneViewBuildParams {
        RenderCore::View::FViewRect             ViewRect{};
        RenderCore::View::FRenderTargetExtent2D RenderTargetExtent{};
        u64                                     FrameIndex          = 0ULL;
        u32                                     TemporalSampleIndex = 0U;
        f32                                     DeltaTimeSeconds    = 0.0f;
        bool                                    bReverseZ           = true;
        FSceneView::FTargetHandle               ViewTarget{};
        const RenderCore::View::FCameraData*    PrimaryCameraOverride = nullptr;
    };

    class AE_ENGINE_API FSceneViewBuilder {
    public:
        void Build(const GameScene::FWorld& world, const FSceneViewBuildParams& params,
            FRenderScene& outScene) const;
    };
} // namespace AltinaEngine::Engine

namespace AltinaEngine::Core::Container {
    template <> struct THashFunc<Engine::FSceneView> {
        auto operator()(const Engine::FSceneView& view) const noexcept -> usize {
            using ETargetType = Engine::FSceneView::ETargetType;

            u64 h = 0ULL;
            h     = InternalHashCombine(h, static_cast<u64>(view.Target.Type));

            // View target contributes to key.
            switch (view.Target.Type) {
                case ETargetType::Viewport:
                    h = InternalHashCombine(h, GetInternalHash(view.Target.Viewport));
                    break;
                case ETargetType::TextureAsset:
                    h = InternalHashCombine(h, GetInternalHash(view.Target.Texture.mUuid));
                    h = InternalHashCombine(h, static_cast<u64>(view.Target.Texture.mType));
                    break;
                default:
                    break;
            }

            // Camera id contributes to key.
            h = InternalHashCombine(h, static_cast<u64>(view.CameraId.Index));
            h = InternalHashCombine(h, static_cast<u64>(view.CameraId.Generation));
            h = InternalHashCombine(h, static_cast<u64>(view.CameraId.Type));
            return h;
        }
    };
} // namespace AltinaEngine::Core::Container

#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"
#include "Asset/AssetTypes.h"
#include "Container/Vector.h"
#include "Math/Matrix.h"
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
        const RenderCore::Geometry::FStaticMeshData* Mesh      = nullptr;
        const GameScene::FMeshMaterialComponent*     Materials = nullptr;
        Core::Math::FMatrix4x4f                      WorldMatrix{};
        Core::Math::FMatrix4x4f                      PrevWorldMatrix{};
    };

    struct AE_ENGINE_API FRenderScene {
        TVector<FSceneView>       Views{};
        TVector<FSceneStaticMesh> StaticMeshes{};
    };

    struct AE_ENGINE_API FSceneViewBuildParams {
        RenderCore::View::FViewRect             ViewRect{};
        RenderCore::View::FRenderTargetExtent2D RenderTargetExtent{};
        u64                                     FrameIndex          = 0ULL;
        u32                                     TemporalSampleIndex = 0U;
        f32                                     DeltaTimeSeconds    = 0.0f;
        bool                                    bReverseZ           = true;
        FSceneView::FTargetHandle               ViewTarget{};
    };

    class AE_ENGINE_API FSceneViewBuilder {
    public:
        void Build(const GameScene::FWorld& world, const FSceneViewBuildParams& params,
            FRenderScene& outScene) const;
    };
} // namespace AltinaEngine::Engine

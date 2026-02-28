#include "Base/AltinaBase.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/NativeScriptComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameSceneAsset/ModelAssetInstantiator.h"
#include "Launch/EngineLoop.h"
#include "Launch/GameClient.h"
#include "Logging/Log.h"
#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Rotation.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rendering/PostProcess/PostProcessSettings.h"
#include "DebugGui/DebugGui.h"

#include "NativeScripts/ShipCameraModesNative.h"
#include "NativeScripts/ShipOrbitControllerNative.h"
#include "NativeScripts/SpaceshipNativeContext.h"
#include "NativeScripts/SpaceshipNativeScripts.h"

#include <cmath>

using namespace AltinaEngine;

namespace {
    namespace Geometry  = RenderCore::Geometry;
    namespace Container = Core::Container;
    namespace Math      = Core::Math;

    using Container::TVector;
    using Math::FVector2f;
    using Math::FVector3f;
    using Math::FVector4f;

    namespace SpaceshipNative = Demo::SpaceshipGame::NativeScripts;

    [[nodiscard]] auto LengthXZ(const FVector3f& v) -> f32 {
        const f32 x = v.X();
        const f32 z = v.Z();
        return std::sqrt(x * x + z * z);
    }

    [[nodiscard]] auto NormalizeXZ(const FVector3f& v) -> FVector3f {
        const f32 len = LengthXZ(v);
        if (len <= 1e-6f) {
            return FVector3f(1.0f, 0.0f, 0.0f);
        }
        const f32 inv = 1.0f / len;
        return FVector3f(v.X() * inv, 0.0f, v.Z() * inv);
    }

    [[nodiscard]] auto PerpLeftXZ(const FVector3f& axisX) -> FVector3f {
        return FVector3f(-axisX.Z(), 0.0f, axisX.X());
    }

    [[nodiscard]] auto OrbitPointCircle(const FVector3f& center, const FVector3f& axisX,
        const FVector3f& axisZ, f32 radius, f32 phaseRad) -> FVector3f {
        const f32 c = std::cos(phaseRad);
        const f32 s = std::sin(phaseRad);
        return center + (axisX * FVector3f(radius * c)) + (axisZ * FVector3f(radius * s));
    }

    [[nodiscard]] auto OrbitPointTransferEllipse(const FVector3f& earthPos, const FVector3f& axisX,
        const FVector3f& axisZ, f32 thetaRad, f32 r1, f32 r2) -> FVector3f {
        const f32 a = 0.5f * (r1 + r2);
        const f32 e = (r2 - r1) / (r2 + r1);
        const f32 p = a * (1.0f - e * e);
        const f32 r = p / (1.0f + e * std::cos(thetaRad));

        const f32 phi = thetaRad + 3.14159265f;
        const f32 c   = std::cos(phi);
        const f32 s   = std::sin(phi);
        return earthPos + (axisX * FVector3f(r * c)) + (axisZ * FVector3f(r * s));
    }

    // Build a polyline mesh for OrbitLine.Billboard.hlsl:
    // - Vertex.Position = endpoint position
    // - Vertex.Normal   = other endpoint position (packed into the NORMAL buffer)
    // - Vertex.UV0      = (sideSign, endpointFlag)
    [[nodiscard]] auto BuildOrbitBillboardLineMesh(
        const TVector<FVector3f>& points, const TChar* debugName) -> Geometry::FStaticMeshData {
        Geometry::FStaticMeshData mesh{};
        if (points.Size() < 2U) {
            return mesh;
        }

        const u32          pointCount   = static_cast<u32>(points.Size());
        const u32          segmentCount = pointCount; // closed loop
        const u32          vertexCount  = segmentCount * 4U;
        const u32          indexCount   = segmentCount * 6U;

        TVector<FVector3f> positions;
        TVector<FVector4f> tangents;
        TVector<FVector2f> uv0;
        TVector<FVector2f> uv1;
        TVector<u32>       indices;

        positions.Reserve(vertexCount);
        tangents.Reserve(vertexCount);
        uv0.Reserve(vertexCount);
        uv1.Reserve(vertexCount);
        indices.Reserve(indexCount);

        FVector3f boundsMin(1e30f);
        FVector3f boundsMax(-1e30f);

        for (u32 i = 0U; i < segmentCount; ++i) {
            const FVector3f p0 = points[i];
            const FVector3f p1 = points[(i + 1U) % pointCount];

            // 4 vertices per segment:
            // (p0, -1), (p0, +1), (p1, -1), (p1, +1)
            const u32       base = i * 4U;

            const FVector2f uvP0L(-1.0f, 0.0f);
            const FVector2f uvP0R(+1.0f, 0.0f);
            const FVector2f uvP1L(-1.0f, 1.0f);
            const FVector2f uvP1R(+1.0f, 1.0f);

            positions.PushBack(p0);
            positions.PushBack(p0);
            positions.PushBack(p1);
            positions.PushBack(p1);

            // Pack the "other endpoint" into the NORMAL stream (slot1).
            tangents.PushBack(FVector4f(p1.X(), p1.Y(), p1.Z(), 1.0f));
            tangents.PushBack(FVector4f(p1.X(), p1.Y(), p1.Z(), 1.0f));
            tangents.PushBack(FVector4f(p0.X(), p0.Y(), p0.Z(), 1.0f));
            tangents.PushBack(FVector4f(p0.X(), p0.Y(), p0.Z(), 1.0f));

            uv0.PushBack(uvP0L);
            uv0.PushBack(uvP0R);
            uv0.PushBack(uvP1L);
            uv0.PushBack(uvP1R);

            uv1.PushBack(uvP0L);
            uv1.PushBack(uvP0R);
            uv1.PushBack(uvP1L);
            uv1.PushBack(uvP1R);

            indices.PushBack(base + 0U);
            indices.PushBack(base + 2U);
            indices.PushBack(base + 1U);

            indices.PushBack(base + 2U);
            indices.PushBack(base + 3U);
            indices.PushBack(base + 1U);

            const FVector3f verts[] = { p0, p1 };
            for (const auto& v : verts) {
                for (u32 c = 0U; c < 3U; ++c) {
                    if (v[c] < boundsMin[c])
                        boundsMin[c] = v[c];
                    if (v[c] > boundsMax[c])
                        boundsMax[c] = v[c];
                }
            }
        }

        Geometry::FStaticMeshLodData lod{};
        lod.PrimitiveTopology = Rhi::ERhiPrimitiveTopology::TriangleList;

        lod.SetPositions(positions.Data(), static_cast<u32>(positions.Size()));
        lod.SetTangents(tangents.Data(), static_cast<u32>(tangents.Size()));
        lod.SetUV0(uv0.Data(), static_cast<u32>(uv0.Size()));
        lod.SetUV1(uv1.Data(), static_cast<u32>(uv1.Size()));
        lod.SetIndices(
            indices.Data(), static_cast<u32>(indices.Size()), Rhi::ERhiIndexType::Uint32);

        Geometry::FStaticMeshSection section{};
        section.FirstIndex   = 0U;
        section.IndexCount   = static_cast<u32>(indices.Size());
        section.BaseVertex   = 0;
        section.MaterialSlot = 0U;
        lod.Sections.PushBack(section);

        lod.Bounds.Min = boundsMin;
        lod.Bounds.Max = boundsMax;

        mesh.Lods.PushBack(Move(lod));
        mesh.Bounds = lod.Bounds;

        LogInfo(
            TEXT(
                "[SpaceshipGame] Built orbit billboard line mesh '{}' (segments={}, verts={}, indices={})."),
            debugName, segmentCount, vertexCount, static_cast<u32>(indices.Size()));
        return mesh;
    }

    class FSpaceshipGameClient final : public Launch::FGameClient {
    public:
        auto OnInit(Launch::FEngineLoop& engineLoop) -> bool override {
            auto& assetManager = engineLoop.GetAssetManager();

            SpaceshipNative::SetNativeScriptContext(
                engineLoop.GetInputSystem(), engineLoop.GetMainWindow());
            SpaceshipNative::RegisterSpaceshipNativeScripts();

            if (auto* debugGui = engineLoop.GetDebugGui()) {
                debugGui->RegisterOverlay(TEXT("SpaceshipHelp"), [](DebugGui::IDebugGui& gui) {
                    constexpr f32 pad       = 10.0f;
                    constexpr f32 lineH     = 18.0f;
                    constexpr f32 boxW      = 420.0f;
                    constexpr u32 lineCount = 6U;
                    const auto    display   = gui.GetDisplaySize();

                    const f32     boxH = pad * 2.0f + static_cast<f32>(lineCount) * lineH;
                    f32           x    = pad;
                    f32           y    = display.Y() - pad - boxH;
                    if (y < 0.0f) {
                        y = 0.0f;
                    }

                    const DebugGui::FRect rect{
                        DebugGui::FVector2f(x, y),
                        DebugGui::FVector2f(x + boxW, y + boxH),
                    };

                    constexpr DebugGui::FColor32 bg     = DebugGui::MakeColor32(10, 10, 10, 150);
                    constexpr DebugGui::FColor32 border = DebugGui::MakeColor32(255, 255, 255, 80);
                    constexpr DebugGui::FColor32 text   = DebugGui::MakeColor32(235, 235, 235, 255);

                    gui.DrawRectFilled(rect, bg);
                    gui.DrawRect(rect, border, 1.0f);

                    f32 ty = y + pad;
                    gui.DrawText(DebugGui::FVector2f(x + pad, ty), text,
                        TEXT("C: Toggle camera (first/third person)"));
                    ty += lineH;
                    gui.DrawText(DebugGui::FVector2f(x + pad, ty), text,
                        TEXT("Space: Change orbit / transfer"));
                    ty += lineH;
                    gui.DrawText(DebugGui::FVector2f(x + pad, ty), text, TEXT("Q: Time scale up"));
                    ty += lineH;
                    gui.DrawText(
                        DebugGui::FVector2f(x + pad, ty), text, TEXT("E: Time scale down"));
                    ty += lineH;
                    gui.DrawText(
                        DebugGui::FVector2f(x + pad, ty), text, TEXT("F1: Toggle Debug GUI"));
                    ty += lineH;
                    gui.DrawText(
                        DebugGui::FVector2f(x + pad, ty), text, TEXT("Esc: Toggle mouse lock"));
                });
            }

            const auto sunMaterialHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/materials/sun"));
            const auto earthMaterialHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/materials/earth"));
            const auto moonMaterialHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/materials/moon"));
            const auto shipMaterialHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/materials/ship"));
            const auto orbitLineMaterialHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/materials/orbitline"));

            const auto sunModelHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/models/sun"));
            const auto earthModelHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/models/earthsimple"));
            const auto moonModelHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/models/moon"));
            const auto shipModelHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/models/apollo"));

            const auto skyCubeHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/skyboxes/nebulae"));

            if (!sunMaterialHandle.IsValid() || !earthMaterialHandle.IsValid()
                || !moonMaterialHandle.IsValid() || !shipMaterialHandle.IsValid()
                || !orbitLineMaterialHandle.IsValid() || !skyCubeHandle.IsValid()
                || !sunModelHandle.IsValid() || !earthModelHandle.IsValid()
                || !moonModelHandle.IsValid() || !shipModelHandle.IsValid()) {
                return false;
            }

            mMouseLocked = true;

            // Enable bloom post-process for the demo (defaults are defined in
            // PostProcessSettings.cpp).
            Rendering::rPostProcessBloom.Set(1);
            Rendering::rPostProcessBloomThreshold.Set(1.4f);
            Rendering::rPostProcessBloomKnee.Set(0.8f);
            Rendering::rPostProcessBloomIntensity.Set(0.16f);
            Rendering::rPostProcessBloomIterations.Set(6);
            Rendering::rPostProcessBloomKawaseOffset.Set(1.2f);
            LogInfo(TEXT("[SpaceshipGame] PostProcess: Bloom=ON (press B to toggle)."));

            // Thin world-space ribbons alias heavily at distance without AA. Enable FXAA so orbit
            // lines don't look "broken" when they become sub-pixel wide.
            Rendering::rPostProcessFxaa.Set(1);
            LogInfo(TEXT("[SpaceshipGame] PostProcess: FXAA=ON."));

            auto&      worldManager = engineLoop.GetWorldManager();
            const auto worldHandle  = worldManager.CreateWorld();
            worldManager.SetActiveWorld(worldHandle);

            auto* world = worldManager.GetWorld(worldHandle);
            if (world == nullptr) {
                return false;
            }

            // Skybox.
            {
                auto skyObject    = world->CreateGameObject(TEXT("SkyBox"));
                auto skyComponent = skyObject.AddComponent<GameScene::FSkyCubeComponent>();
                if (skyComponent.IsValid()) {
                    skyComponent.Get().SetCubeMapAsset(skyCubeHandle);
                }
            }

            // Ship root (logic + transform). Keep renderable mesh on a child so hiding the ship
            // does not accidentally break camera transforms by setting parent scale to zero.
            GameScene::FGameObjectView shipRootObject;
            {
                shipRootObject = world->CreateGameObject(TEXT("Ship"));
                auto t         = shipRootObject.GetWorldTransform();
                t.Translation  = Core::Math::FVector3f(401.2f, 0.0f, 0.0f);
                t.Scale        = Core::Math::FVector3f(1.0f);
                shipRootObject.SetWorldTransform(t);

                (void)shipRootObject.AddComponent<SpaceshipNative::FShipOrbitControllerNative>();
            }

            // Ship visual (model).
            {
                auto shipVisualObject = world->CreateGameObject(TEXT("ShipVisual"));
                shipVisualObject.SetParent(shipRootObject.GetId());

                auto t = shipVisualObject.GetLocalTransform();
                // Computed by AssetTool modelinfo so Apollo.usdz extent radius matches ShipRadius.
                t.Scale = Core::Math::FVector3f(0.000255973f);
                shipVisualObject.SetLocalTransform(t);

                auto shipModelResult = Engine::GameSceneAsset::FModelAssetInstantiator::Instantiate(
                    *world, assetManager, shipModelHandle);
                if (shipModelResult.Root.IsValid()) {
                    world->Object(shipModelResult.Root).SetParent(shipVisualObject.GetId());

                    for (const auto nodeId : shipModelResult.Nodes) {
                        if (!nodeId.IsValid()) {
                            continue;
                        }
                        auto nodeView = world->Object(nodeId);
                        if (!nodeView.HasComponent<GameScene::FStaticMeshFilterComponent>()) {
                            continue;
                        }

                        auto matComp = nodeView.GetComponent<GameScene::FMeshMaterialComponent>();
                        if (!matComp.IsValid()) {
                            continue;
                        }
                        const u32 slotCount = matComp.Get().GetMaterialCount();
                        const u32 setCount  = (slotCount > 0U) ? slotCount : 1U;
                        for (u32 slot = 0; slot < setCount; ++slot) {
                            matComp.Get().SetMaterialTemplate(slot, shipMaterialHandle);
                        }
                    }
                } else {
                    LogWarning(TEXT("[SpaceshipGame] Failed to instantiate ship model."));
                }
            }

            // Camera (first-person + third-person, toggled in script).
            {
                auto cameraObject = world->CreateGameObject(TEXT("Camera"));
                cameraObject.SetParent(shipRootObject.GetId());

                auto cameraComp = cameraObject.AddComponent<GameScene::FCameraComponent>();
                if (cameraComp.IsValid()) {
                    cameraComp.Get().SetNearPlane(0.1f);
                    cameraComp.Get().SetFarPlane(5000.0f);
                }

                (void)cameraObject.AddComponent<SpaceshipNative::FShipCameraModesNative>();
            }

            auto CreateBodyFromModel =
                [&](Core::Container::FStringView name, const Asset::FAssetHandle model,
                    const Asset::FAssetHandle overrideMaterial, const Core::Math::FVector3f& pos,
                    float uniformScale) -> GameScene::FGameObjectView {
                auto obj      = world->CreateGameObject(name);
                auto t        = obj.GetWorldTransform();
                t.Translation = pos;
                t.Scale       = Core::Math::FVector3f(uniformScale);
                obj.SetWorldTransform(t);

                auto modelResult = Engine::GameSceneAsset::FModelAssetInstantiator::Instantiate(
                    *world, assetManager, model);
                if (!modelResult.Root.IsValid()) {
                    LogWarning(TEXT("[SpaceshipGame] Failed to instantiate model for '{}'."), name);
                    return obj;
                }

                world->Object(modelResult.Root).SetParent(obj.GetId());

                // Force a single demo material so we can reuse the extracted textures/PBR preset
                // regardless of the model's embedded material slots.
                u32 meshNodeCount = 0;
                for (const auto nodeId : modelResult.Nodes) {
                    if (!nodeId.IsValid()) {
                        continue;
                    }
                    auto nodeView = world->Object(nodeId);
                    if (!nodeView.HasComponent<GameScene::FStaticMeshFilterComponent>()) {
                        continue;
                    }
                    ++meshNodeCount;

                    auto matComp = nodeView.GetComponent<GameScene::FMeshMaterialComponent>();
                    if (!matComp.IsValid()) {
                        continue;
                    }
                    const u32 slotCount = matComp.Get().GetMaterialCount();
                    const u32 setCount  = (slotCount > 0U) ? slotCount : 1U;
                    for (u32 slot = 0; slot < setCount; ++slot) {
                        matComp.Get().SetMaterialTemplate(slot, overrideMaterial);
                    }
                }

                LogInfo(TEXT("[SpaceshipGame] Instantiated '{}' model: nodes={}, meshNodes={}."),
                    name, static_cast<u32>(modelResult.Nodes.Size()), meshNodeCount);
                return obj;
            };

            // Suggested scales are computed by AssetTool modelinfo so imported USDZ extents match
            // Milestone 3 radii (world unit: 1 = 10,000 km).
            // NOTE: Keep Sun off the Earth->Moon line so the three bodies are not colinear in the
            // initial static scene (helps lighting/readability).
            const FVector3f kSunPos(400.0f, 0.0f, -400.0f);
            auto            sunObject = CreateBodyFromModel(
                TEXT("Sun"), sunModelHandle, sunMaterialHandle, kSunPos, 0.0464227f * 1.0f);

            auto earthObject = CreateBodyFromModel(TEXT("Earth"), earthModelHandle,
                earthMaterialHandle, Core::Math::FVector3f(400.0f, 0.0f, 0.0f), 0.00515107f * 1.0f);
            auto moonObject = CreateBodyFromModel(TEXT("Moon"), moonModelHandle, moonMaterialHandle,
                Core::Math::FVector3f(438.44f, 0.0f, 0.0f), 0.001158f * 1.0f);

            if (sunObject.IsValid()) {
                mSunObjectId = sunObject.GetId();
            }
            if (earthObject.IsValid()) {
                mEarthObjectId = earthObject.GetId();
            }

            // Celestial bodies are static in this iteration; positions are set directly above.

            // Orbit visualization (world-space billboard lines). These are depth-tested so they
            // can be occluded by Earth/Moon.
            {
                const FVector3f earthPos(400.0f, 0.0f, 0.0f);
                const FVector3f moonPos(438.44f, 0.0f, 0.0f);

                const FVector3f axisX = NormalizeXZ(moonPos - earthPos);
                const FVector3f axisZ = PerpLeftXZ(axisX);

                auto            CreateOrbitLineObject = [&](Container::FStringView      name,
                                                 Geometry::FStaticMeshData&& mesh) {
                    auto obj = world->CreateGameObject(name);

                    auto filterComp = obj.AddComponent<GameScene::FStaticMeshFilterComponent>();
                    if (filterComp.IsValid()) {
                        filterComp.Get().SetStaticMeshData(Move(mesh));
                    }

                    auto matComp = obj.AddComponent<GameScene::FMeshMaterialComponent>();
                    if (matComp.IsValid()) {
                        matComp.Get().SetMaterialTemplate(0U, orbitLineMaterialHandle);
                    }

                    return obj;
                };

                // Earth orbit circle.
                {
                    TVector<FVector3f> pts;
                    constexpr u32      kSamples = 512U;
                    pts.Reserve(kSamples);
                    for (u32 i = 0U; i < kSamples; ++i) {
                        const f32 phase =
                            (6.2831853f * static_cast<f32>(i)) / static_cast<f32>(kSamples);
                        pts.PushBack(OrbitPointCircle(earthPos, axisX, axisZ, 1.2f, phase));
                    }
                    auto mesh = BuildOrbitBillboardLineMesh(pts, TEXT("EarthOrbitLine"));
                    CreateOrbitLineObject(TEXT("EarthOrbitLine"), Move(mesh));
                }

                // Moon orbit circle.
                {
                    TVector<FVector3f> pts;
                    constexpr u32      kSamples = 512U;
                    pts.Reserve(kSamples);
                    for (u32 i = 0U; i < kSamples; ++i) {
                        const f32 phase =
                            (6.2831853f * static_cast<f32>(i)) / static_cast<f32>(kSamples);
                        pts.PushBack(OrbitPointCircle(moonPos, axisX, axisZ, 0.55f, phase));
                    }
                    auto mesh = BuildOrbitBillboardLineMesh(pts, TEXT("MoonOrbitLine"));
                    CreateOrbitLineObject(TEXT("MoonOrbitLine"), Move(mesh));
                }

                // Transfer ellipse (polar form around Earth).
                {
                    TVector<FVector3f> pts;
                    constexpr u32      kSamples = 1024U;
                    pts.Reserve(kSamples);

                    constexpr f32 r1 = 1.2f;
                    constexpr f32 r2 = 38.44f - 0.55f;
                    for (u32 i = 0U; i < kSamples; ++i) {
                        const f32 theta =
                            (6.2831853f * static_cast<f32>(i)) / static_cast<f32>(kSamples);
                        pts.PushBack(
                            OrbitPointTransferEllipse(earthPos, axisX, axisZ, theta, r1, r2));
                    }

                    auto mesh = BuildOrbitBillboardLineMesh(pts, TEXT("TransferOrbitLine"));
                    CreateOrbitLineObject(TEXT("TransferOrbitLine"), Move(mesh));
                }
            }

            // Directional light.
            {
                auto lightObject          = world->CreateGameObject(TEXT("DirectionalLight"));
                mDirectionalLightObjectId = lightObject.GetId();
                auto lightComp = lightObject.AddComponent<GameScene::FDirectionalLightComponent>();
                if (lightComp.IsValid()) {
                    auto& light        = lightComp.Get();
                    light.mColor       = Core::Math::FVector3f(1.0f, 1.0f, 1.0f);
                    light.mIntensity   = 5.0f;
                    light.mCastShadows = true;
                }

                // Initialize to "Sun -> Earth" direction.
                UpdateSunLightDirection(engineLoop);
            }

            return true;
        }

        auto OnTick(Launch::FEngineLoop& engineLoop, float deltaSeconds) -> bool override {
            engineLoop.Tick(deltaSeconds);

            UpdateSunLightDirection(engineLoop);

            auto* input  = engineLoop.GetInputSystem();
            auto* window = engineLoop.GetMainWindow();
            if (input != nullptr && window != nullptr) {
                // Demo-only FPS mouse lock toggle. Escape releases / re-captures the mouse.
                if (input->WasKeyPressed(Input::EKey::Escape)) {
                    mMouseLocked = !mMouseLocked;
                }

                if (input->WasKeyPressed(Input::EKey::B)) {
                    const i32 enabled = (Rendering::rPostProcessBloom.Get() != 0) ? 0 : 1;
                    Rendering::rPostProcessBloom.Set(enabled);
                    LogInfo(TEXT("[SpaceshipGame] PostProcess: Bloom={}."),
                        enabled ? TEXT("ON") : TEXT("OFF"));
                }

                if (mMouseLocked && input->HasFocus()) {
                    window->SetCursorVisible(false);
                    window->SetCursorClippedToClient(true);

                    const u32 width  = input->GetWindowWidth();
                    const u32 height = input->GetWindowHeight();
                    if (width > 0U && height > 0U) {
                        const i32 cx = static_cast<i32>(width / 2U);
                        const i32 cy = static_cast<i32>(height / 2U);

                        // Prevent warp-generated mouse move events from producing artificial
                        // deltas.
                        input->SetMousePositionNoDelta(cx, cy);
                        window->SetCursorPositionClient(cx, cy);
                    }
                } else {
                    window->SetCursorClippedToClient(false);
                    window->SetCursorVisible(true);
                }
            }

            Core::Platform::Generic::PlatformSleepMilliseconds(16);
            return engineLoop.IsRunning();
        }

        void OnShutdown(Launch::FEngineLoop& engineLoop) override {
            if (auto* window = engineLoop.GetMainWindow()) {
                window->SetCursorClippedToClient(false);
                window->SetCursorVisible(true);
            }
        }

    private:
        void UpdateSunLightDirection(Launch::FEngineLoop& engineLoop) {
            auto* world = engineLoop.GetWorldManager().GetActiveWorld();
            if (world == nullptr) {
                return;
            }

            if (!mDirectionalLightObjectId.IsValid() || !mSunObjectId.IsValid()
                || !mEarthObjectId.IsValid()) {
                return;
            }

            auto       lightObj = world->Object(mDirectionalLightObjectId);
            const auto sunObj   = world->Object(mSunObjectId);
            const auto earthObj = world->Object(mEarthObjectId);
            if (!lightObj.IsValid() || !sunObj.IsValid() || !earthObj.IsValid()) {
                return;
            }

            const auto sunPos   = sunObj.GetWorldTransform().Translation;
            const auto earthPos = earthObj.GetWorldTransform().Translation;

            auto       dir = earthPos - sunPos;
            dir.Y()        = 0.0f;

            const f32 len2 = dir.X() * dir.X() + dir.Z() * dir.Z();
            if (len2 <= 1e-6f) {
                return;
            }

            const f32 invLen = 1.0f / Core::Math::Sqrt(len2);
            dir              = dir * Core::Math::FVector3f(invLen, invLen, invLen);

            // Light direction is derived from the owner's +Z axis.
            const f32 yaw = Core::Math::Atan2(dir.X(), dir.Z());

            auto      t = lightObj.GetWorldTransform();
            t.Rotation  = Core::Math::FEulerRotator(0.0f, yaw, 0.0f).ToQuaternion();
            lightObj.SetWorldTransform(t);
        }

        bool                     mMouseLocked = false;
        GameScene::FGameObjectId mSunObjectId{};
        GameScene::FGameObjectId mEarthObjectId{};
        GameScene::FGameObjectId mDirectionalLightObjectId{};
    };
} // namespace

int main(int argc, char** argv) {
    FStartupParameters startupParams{};
    if (argc > 1) {
        startupParams.mCommandLine = argv[1];
    }

    FSpaceshipGameClient client;
    return Launch::RunGameClient(client, startupParams);
}

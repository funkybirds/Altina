#include "Base/AltinaBase.h"
#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/MaterialLoader.h"
#include "Asset/MeshAsset.h"
#include "Asset/MeshLoader.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Geometry/StaticMeshData.h"
#include "Launch/EngineLoop.h"
#include "Logging/Log.h"
#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Vector.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

#include <filesystem>

using namespace AltinaEngine;

namespace {
    auto ToFString(const std::filesystem::path& path) -> Core::Container::FString {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        const std::wstring wide = path.wstring();
        return Core::Container::FString(wide.c_str(), static_cast<usize>(wide.size()));
#else
        const std::string narrow = path.string();
        return Core::Container::FString(narrow.c_str(), static_cast<usize>(narrow.size()));
#endif
    }

    auto FindAssetRegistryPath() -> std::filesystem::path {
        std::filesystem::path probe = std::filesystem::current_path();
        for (int depth = 0; depth < 6; ++depth) {
            const auto candidate =
                probe / "build" / "Cooked" / "Win64" / "Registry" / "AssetRegistry.json";
            if (std::filesystem::exists(candidate)) {
                return candidate;
            }
            if (!probe.has_parent_path()) {
                break;
            }
            probe = probe.parent_path();
        }
        return {};
    }

    auto LoadDemoAssetRegistry(Launch::FEngineLoop& engineLoop) -> bool {
        const auto registryPath = FindAssetRegistryPath();
        if (registryPath.empty()) {
            LogWarning(TEXT("Demo asset registry not found."));
            return false;
        }
        if (!engineLoop.GetAssetRegistry().LoadFromJsonFile(ToFString(registryPath))) {
            LogWarning(TEXT("Demo asset registry load failed."));
            return false;
        }

        const auto assetRoot = registryPath.parent_path().parent_path();
        std::error_code ec;
        std::filesystem::current_path(assetRoot, ec);
        if (ec) {
            const auto rootText = ToFString(assetRoot);
            LogWarning(TEXT("Failed to set asset root to {}."), rootText.ToView());
        }
        return true;
    }

    auto FindPositionAttribute(const Asset::TVector<Asset::FMeshVertexAttributeDesc>& attributes,
        u32& outOffset, u32& outFormat) -> bool {
        for (const auto& attr : attributes) {
            if (attr.Semantic == Asset::kMeshSemanticPosition) {
                outOffset = attr.AlignedOffset;
                outFormat = attr.Format;
                return true;
            }
        }
        return false;
    }

    auto ReadPosition(const u8* vertexBase, u32 offset, u32 format) noexcept
        -> Core::Math::FVector3f {
        const auto* data = reinterpret_cast<const f32*>(vertexBase + offset);
        switch (format) {
            case Asset::kMeshVertexFormatR32Float:
                return Core::Math::FVector3f(data[0], 0.0f, 0.0f);
            case Asset::kMeshVertexFormatR32G32Float:
                return Core::Math::FVector3f(data[0], data[1], 0.0f);
            case Asset::kMeshVertexFormatR32G32B32Float:
                return Core::Math::FVector3f(data[0], data[1], data[2]);
            case Asset::kMeshVertexFormatR32G32B32A32Float:
                return Core::Math::FVector3f(data[0], data[1], data[2]);
            default:
                break;
        }
        return Core::Math::FVector3f(0.0f);
    }

    auto BuildStaticMeshFromAsset(const Asset::FMeshAsset& asset,
        RenderCore::Geometry::FStaticMeshData& outMesh) -> bool {
        const auto& desc       = asset.GetDesc();
        const auto& attributes = asset.GetAttributes();
        const auto& subMeshes  = asset.GetSubMeshes();
        const auto& vertices   = asset.GetVertexData();
        const auto& indices    = asset.GetIndexData();

        if (desc.VertexCount == 0U || desc.IndexCount == 0U || desc.VertexStride == 0U) {
            return false;
        }

        u32 positionOffset = 0U;
        u32 positionFormat = 0U;
        if (!FindPositionAttribute(attributes, positionOffset, positionFormat)) {
            return false;
        }

        const u32 positionBytes = [positionFormat]() -> u32 {
            switch (positionFormat) {
                case Asset::kMeshVertexFormatR32Float:
                    return 4U;
                case Asset::kMeshVertexFormatR32G32Float:
                    return 8U;
                case Asset::kMeshVertexFormatR32G32B32Float:
                    return 12U;
                case Asset::kMeshVertexFormatR32G32B32A32Float:
                    return 16U;
                default:
                    return 0U;
            }
        }();

        if (positionBytes == 0U) {
            return false;
        }

        const u64 vertexBytes = static_cast<u64>(desc.VertexStride)
            * static_cast<u64>(desc.VertexCount);
        if (vertices.Size() < vertexBytes) {
            return false;
        }

        const u64 posEnd = static_cast<u64>(positionOffset) + static_cast<u64>(positionBytes);
        if (posEnd > desc.VertexStride) {
            return false;
        }

        Core::Container::TVector<Core::Math::FVector3f> positions;
        positions.Reserve(static_cast<usize>(desc.VertexCount));
        for (u32 i = 0U; i < desc.VertexCount; ++i) {
            const u64 base = static_cast<u64>(i) * static_cast<u64>(desc.VertexStride);
            positions.PushBack(
                ReadPosition(vertices.Data() + base, positionOffset, positionFormat));
        }

        Rhi::ERhiIndexType indexType = Rhi::ERhiIndexType::Uint16;
        switch (desc.IndexType) {
            case Asset::kMeshIndexTypeUint16:
                indexType = Rhi::ERhiIndexType::Uint16;
                break;
            case Asset::kMeshIndexTypeUint32:
                indexType = Rhi::ERhiIndexType::Uint32;
                break;
            default:
                return false;
        }

        const u32 indexStride =
            RenderCore::Geometry::FStaticMeshLodData::GetIndexStrideBytes(indexType);
        if (indexStride == 0U) {
            return false;
        }

        const u64 indexBytes =
            static_cast<u64>(desc.IndexCount) * static_cast<u64>(indexStride);
        if (indices.Size() < indexBytes) {
            return false;
        }

        RenderCore::Geometry::FStaticMeshData mesh;
        mesh.Lods.Reserve(1);
        auto& lod = mesh.Lods.EmplaceBack();
        lod.PrimitiveTopology = Rhi::ERhiPrimitiveTopology::TriangleList;
        lod.SetPositions(positions.Data(), desc.VertexCount);
        lod.SetIndices(indices.Data(), desc.IndexCount, indexType);

        if (!subMeshes.IsEmpty()) {
            lod.Sections.Reserve(subMeshes.Size());
            for (const auto& subMesh : subMeshes) {
                RenderCore::Geometry::FStaticMeshSection section{};
                section.FirstIndex   = subMesh.IndexStart;
                section.IndexCount   = subMesh.IndexCount;
                section.BaseVertex   = subMesh.BaseVertex;
                section.MaterialSlot = subMesh.MaterialSlot;
                lod.Sections.PushBack(section);
            }
        } else {
            RenderCore::Geometry::FStaticMeshSection section{};
            section.FirstIndex   = 0U;
            section.IndexCount   = desc.IndexCount;
            section.BaseVertex   = 0;
            section.MaterialSlot = 0U;
            lod.Sections.PushBack(section);
        }

        lod.Bounds.Min = Core::Math::FVector3f(
            desc.BoundsMin[0], desc.BoundsMin[1], desc.BoundsMin[2]);
        lod.Bounds.Max = Core::Math::FVector3f(
            desc.BoundsMax[0], desc.BoundsMax[1], desc.BoundsMax[2]);

        mesh.Bounds = lod.Bounds;

        if (!mesh.IsValid()) {
            return false;
        }

        outMesh = AltinaEngine::Move(mesh);
        return true;
    }
} // namespace

int main(int argc, char** argv) {
    FStartupParameters startupParams{};
    if (argc > 1) {
        startupParams.mCommandLine = argv[1];
    }

    Launch::FEngineLoop engineLoop(startupParams);
    if (!engineLoop.PreInit()) {
        return 1;
    }
    if (!engineLoop.Init()) {
        engineLoop.Exit();
        return 1;
    }

    if (!LoadDemoAssetRegistry(engineLoop)) {
        engineLoop.Exit();
        return 1;
    }

    Asset::FAssetManager assetManager;
    Asset::FMeshLoader   meshLoader;
    Asset::FMaterialLoader materialLoader;
    assetManager.SetRegistry(&engineLoop.GetAssetRegistry());
    assetManager.RegisterLoader(&meshLoader);
    assetManager.RegisterLoader(&materialLoader);

    const auto meshHandle =
        engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/triangle"));
    const auto materialHandle =
        engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/materials/purpledeferred"));
    if (!meshHandle.IsValid() || !materialHandle.IsValid()) {
        LogError(TEXT("Demo assets missing (mesh or material)."));
        engineLoop.Exit();
        return 1;
    }

    auto meshAsset = assetManager.Load(meshHandle);
    auto* mesh = meshAsset ? static_cast<Asset::FMeshAsset*>(meshAsset.Get()) : nullptr;
    if (mesh == nullptr) {
        LogError(TEXT("Failed to load mesh asset."));
        engineLoop.Exit();
        return 1;
    }

    RenderCore::Geometry::FStaticMeshData meshData;
    if (!BuildStaticMeshFromAsset(*mesh, meshData)) {
        LogError(TEXT("Failed to build static mesh data from asset."));
        engineLoop.Exit();
        return 1;
    }
    for (auto& lod : meshData.Lods) {
        lod.PositionBuffer.InitResource();
        lod.IndexBuffer.InitResource();
        lod.TangentBuffer.InitResource();
        lod.UV0Buffer.InitResource();
        lod.UV1Buffer.InitResource();

        lod.PositionBuffer.WaitForInit();
        lod.IndexBuffer.WaitForInit();
        lod.TangentBuffer.WaitForInit();
        lod.UV0Buffer.WaitForInit();
        lod.UV1Buffer.WaitForInit();
    }

    auto& worldManager = engineLoop.GetWorldManager();
    const auto worldHandle = worldManager.CreateWorld();
    worldManager.SetActiveWorld(worldHandle);
    auto* world = worldManager.GetWorld(worldHandle);
    if (world == nullptr) {
        LogError(TEXT("Demo world creation failed."));
        engineLoop.Exit();
        return 1;
    }

    const auto cameraObject = world->CreateGameObject(TEXT("Camera"));
    const auto cameraComponentId =
        world->CreateComponent<GameScene::FCameraComponent>(cameraObject);
    if (cameraComponentId.IsValid()) {
        auto& camera = world->ResolveComponent<GameScene::FCameraComponent>(cameraComponentId);
        camera.SetNearPlane(0.1f);
        camera.SetFarPlane(1000.0f);

        auto cameraView = world->Object(cameraObject);
        auto transform  = cameraView.GetWorldTransform();
        transform.Translation = Core::Math::FVector3f(0.0f, 0.0f, -2.0f);
        cameraView.SetWorldTransform(transform);
    }

    const auto meshObject = world->CreateGameObject(TEXT("TriangleMesh"));
    const auto meshComponentId =
        world->CreateComponent<GameScene::FStaticMeshFilterComponent>(meshObject);
    const auto materialComponentId =
        world->CreateComponent<GameScene::FMeshMaterialComponent>(meshObject);

    if (meshComponentId.IsValid()) {
        auto& meshComponent =
            world->ResolveComponent<GameScene::FStaticMeshFilterComponent>(meshComponentId);
        meshComponent.SetStaticMesh(AltinaEngine::Move(meshData));
    }
    if (materialComponentId.IsValid()) {
        auto& materialComponent =
            world->ResolveComponent<GameScene::FMeshMaterialComponent>(materialComponentId);
        materialComponent.SetMaterial(0U, materialHandle);
    }

    constexpr f32 kFixedDeltaTime = 1.0f / 60.0f;
    for (i32 frameIndex = 0; frameIndex < 600; ++frameIndex) {
        engineLoop.Tick(kFixedDeltaTime);
        Core::Platform::Generic::PlatformSleepMilliseconds(16);
    }

    engineLoop.Exit();
    return 0;
}

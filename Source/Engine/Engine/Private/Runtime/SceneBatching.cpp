#include "Engine/Runtime/SceneBatching.h"

#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Material/Material.h"
#include "Material/MaterialPass.h"
#include "Shader/ShaderRegistry.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>

using AltinaEngine::Move;
namespace AltinaEngine::Engine {
    namespace {
        using RenderCore::Render::FDrawItem;
        using RenderCore::Render::FDrawKey;

        constexpr u64 kHashSeed = 0x9e3779b97f4a7c15ULL;

        auto          HashCombine(u64 seed, u64 value) noexcept -> u64 {
            return seed ^ (value + kHashSeed + (seed << 6U) + (seed >> 2U));
        }

        auto HashPointer(const void* ptr) noexcept -> u64 {
            return static_cast<u64>(reinterpret_cast<uintptr_t>(ptr));
        }

        auto HashFloat(f32 value) noexcept -> u64 {
            const auto bits = std::bit_cast<u32>(value);
            return static_cast<u64>(bits);
        }

        auto HashShaderKey(const RenderCore::FShaderRegistry::FShaderKey& key) noexcept -> u64 {
            if (!key.IsValid()) {
                return 0ULL;
            }
            u64 hash = std::hash<RenderCore::Container::FString>{}(key.Name);
            hash     = HashCombine(hash, static_cast<u64>(key.Stage));
            hash     = HashCombine(hash, static_cast<u64>(key.Permutation.mHash));
            return hash;
        }

        auto HashRasterState(const Rhi::FRhiRasterStateDesc& state) noexcept -> u64 {
            u64 hash = 0ULL;
            hash     = HashCombine(hash, static_cast<u64>(state.mFillMode));
            hash     = HashCombine(hash, static_cast<u64>(state.mCullMode));
            hash     = HashCombine(hash, static_cast<u64>(state.mFrontFace));
            hash     = HashCombine(hash, static_cast<u64>(state.mDepthBias));
            hash     = HashCombine(hash, HashFloat(state.mDepthBiasClamp));
            hash     = HashCombine(hash, HashFloat(state.mSlopeScaledDepthBias));
            hash     = HashCombine(hash, static_cast<u64>(state.mDepthClip ? 1U : 0U));
            hash     = HashCombine(hash, static_cast<u64>(state.mConservativeRaster ? 1U : 0U));
            return hash;
        }

        auto HashDepthState(const Rhi::FRhiDepthStateDesc& state) noexcept -> u64 {
            u64 hash = 0ULL;
            hash     = HashCombine(hash, static_cast<u64>(state.mDepthEnable ? 1U : 0U));
            hash     = HashCombine(hash, static_cast<u64>(state.mDepthWrite ? 1U : 0U));
            hash     = HashCombine(hash, static_cast<u64>(state.mDepthCompare));
            return hash;
        }

        auto HashBlendState(const Rhi::FRhiBlendStateDesc& state) noexcept -> u64 {
            u64 hash = 0ULL;
            hash     = HashCombine(hash, static_cast<u64>(state.mBlendEnable ? 1U : 0U));
            hash     = HashCombine(hash, static_cast<u64>(state.mSrcColor));
            hash     = HashCombine(hash, static_cast<u64>(state.mDstColor));
            hash     = HashCombine(hash, static_cast<u64>(state.mColorOp));
            hash     = HashCombine(hash, static_cast<u64>(state.mSrcAlpha));
            hash     = HashCombine(hash, static_cast<u64>(state.mDstAlpha));
            hash     = HashCombine(hash, static_cast<u64>(state.mAlphaOp));
            hash     = HashCombine(hash, static_cast<u64>(state.mColorWriteMask));
            return hash;
        }

        auto BuildPipelineKey(const RenderCore::FMaterialPassDesc* passDesc) noexcept -> u64 {
            if (passDesc == nullptr) {
                return 0ULL;
            }

            u64 hash = 0ULL;
            hash     = HashCombine(hash, HashShaderKey(passDesc->Shaders.Vertex));
            hash     = HashCombine(hash, HashShaderKey(passDesc->Shaders.Pixel));
            hash     = HashCombine(hash, HashShaderKey(passDesc->Shaders.Compute));
            hash     = HashCombine(hash, static_cast<u64>(passDesc->Shaders.Permutation.mHash));
            hash     = HashCombine(hash, HashRasterState(passDesc->State.Raster));
            hash     = HashCombine(hash, HashDepthState(passDesc->State.Depth));
            hash     = HashCombine(hash, HashBlendState(passDesc->State.Blend));
            return hash;
        }

        auto BuildGeometryKey(const RenderCore::Geometry::FStaticMeshData* mesh, u32 lodIndex,
            Rhi::ERhiPrimitiveTopology topology) noexcept -> u64 {
            u64 hash = HashPointer(mesh);
            hash     = HashCombine(hash, static_cast<u64>(lodIndex));
            hash     = HashCombine(hash, static_cast<u64>(topology));
            return hash;
        }

        auto BuildSectionKey(const RenderCore::Geometry::FStaticMeshSection& section) noexcept
            -> u64 {
            u64 hash = 0ULL;
            hash     = HashCombine(hash, static_cast<u64>(section.FirstIndex));
            hash     = HashCombine(hash, static_cast<u64>(section.IndexCount));
            hash     = HashCombine(hash, static_cast<u64>(section.BaseVertex));
            return hash;
        }
    } // namespace

    void FSceneBatchBuilder::Build(const FRenderScene& scene, const FSceneView& view,
        const FSceneBatchBuildParams& params, FMaterialCache& materialCache,
        RenderCore::Render::FDrawList& outDrawList) const {
        (void)view;
        outDrawList.Clear();

        if (scene.StaticMeshes.IsEmpty()) {
            return;
        }

        usize totalSections = 0;
        for (const auto& entry : scene.StaticMeshes) {
            if (entry.Mesh == nullptr) {
                continue;
            }
            const auto& lods = entry.Mesh->Lods;
            if (params.LodIndex >= lods.Size()) {
                continue;
            }
            totalSections += lods[params.LodIndex].Sections.Size();
        }

        Core::Container::TVector<FDrawItem> items;
        items.Reserve(totalSections);

        for (const auto& entry : scene.StaticMeshes) {
            if (entry.Mesh == nullptr) {
                continue;
            }
            if (params.LodIndex >= entry.Mesh->Lods.Size()) {
                continue;
            }

            const auto& lod = entry.Mesh->Lods[params.LodIndex];
            if (lod.Sections.IsEmpty()) {
                continue;
            }

            const u32 sectionCount = static_cast<u32>(lod.Sections.Size());
            for (u32 sectionIndex = 0U; sectionIndex < sectionCount; ++sectionIndex) {
                const auto& section  = lod.Sections[sectionIndex];
                const auto* material = (entry.Materials != nullptr)
                    ? entry.Materials->GetMaterial(section.MaterialSlot)
                    : nullptr;
                if (material == nullptr) {
                    material = materialCache.ResolveDefault();
                }

                FDrawItem   item{};
                item.MeshType            = RenderCore::Render::EDrawMeshType::StaticMesh;
                item.Pass                = params.Pass;
                item.Material            = material;
                item.Static.Mesh         = entry.Mesh;
                item.Static.LodIndex     = params.LodIndex;
                item.Static.SectionIndex = sectionIndex;
                item.Instance.World      = entry.WorldMatrix;
                item.Instance.PrevWorld  = entry.PrevWorldMatrix;
                item.Instance.ObjectId   = entry.OwnerId.IsValid() ? entry.OwnerId.Index : 0U;

                item.Key.PassKey     = static_cast<u64>(params.Pass);
                item.Key.PipelineKey = BuildPipelineKey(
                    material != nullptr ? material->FindPassDesc(params.Pass) : nullptr);
                item.Key.MaterialKey = HashPointer(material);
                item.Key.GeometryKey =
                    BuildGeometryKey(entry.Mesh, params.LodIndex, lod.PrimitiveTopology);
                item.Key.SectionKey = BuildSectionKey(section);

                items.PushBack(Move(item));
            }
        }

        if (items.IsEmpty()) {
            return;
        }

        std::sort(items.begin(), items.end(),
            [](const FDrawItem& lhs, const FDrawItem& rhs) { return lhs.Key < rhs.Key; });

        outDrawList.Batches.Reserve(items.Size());
        for (const auto& item : items) {
            if (outDrawList.Batches.IsEmpty() || !params.bAllowInstancing
                || !(item.Key == outDrawList.Batches.Back().BatchKey)) {
                RenderCore::Render::FDrawBatch batch{};
                batch.BatchKey = item.Key;
                batch.Pass     = item.Pass;
                batch.Material = item.Material;
                batch.Static   = item.Static;
                batch.Instances.PushBack(item.Instance);
                outDrawList.Batches.PushBack(Move(batch));
            } else {
                outDrawList.Batches.Back().Instances.PushBack(item.Instance);
            }
        }
    }
} // namespace AltinaEngine::Engine

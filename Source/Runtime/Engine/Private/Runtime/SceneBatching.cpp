#include "Engine/Runtime/SceneBatching.h"

#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Material/Material.h"
#include "Material/MaterialPass.h"
#include "Shader/ShaderRegistry.h"
#include "Types/Conversion.h"
#include "Algorithm/Sort.h"
#include "Container/HashUtility.h"

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
            const auto bits = BitCast<u32>(value);
            return static_cast<u64>(bits);
        }

        auto HashShaderKey(const RenderCore::FShaderRegistry::FShaderKey& key) noexcept -> u64 {
            if (!key.IsValid()) {
                return 0ULL;
            }
            auto hash = Core::Container::THashFunc<RenderCore::Container::FString>{}(key.mName);
            hash      = HashCombine(hash, static_cast<u64>(key.mStage));
            hash      = HashCombine(hash, static_cast<u64>(key.mPermutation.mHash));
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
            hash     = HashCombine(hash, HashShaderKey(passDesc->mShaders.mVertex));
            hash     = HashCombine(hash, HashShaderKey(passDesc->mShaders.mPixel));
            hash     = HashCombine(hash, HashShaderKey(passDesc->mShaders.mCompute));
            hash     = HashCombine(hash, static_cast<u64>(passDesc->mShaders.mPermutation.mHash));
            hash     = HashCombine(hash, HashRasterState(passDesc->mState.mRaster));
            hash     = HashCombine(hash, HashDepthState(passDesc->mState.mDepth));
            hash     = HashCombine(hash, HashBlendState(passDesc->mState.mBlend));
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

        auto BuildMaterialBucketKey(const FDrawKey& key)
            -> RenderCore::Render::FDrawMaterialBucketKey {
            RenderCore::Render::FDrawMaterialBucketKey bucketKey{};
            bucketKey.mPassKey     = key.mPassKey;
            bucketKey.mPipelineKey = key.mPipelineKey;
            bucketKey.mMaterialKey = key.mMaterialKey;
            return bucketKey;
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
            const auto& lods = entry.Mesh->mLods;
            if (params.LodIndex >= lods.Size()) {
                continue;
            }
            totalSections += lods[params.LodIndex].mSections.Size();
        }

        Core::Container::TVector<FDrawItem> items;
        items.Reserve(totalSections);

        for (const auto& entry : scene.StaticMeshes) {
            if (entry.Mesh == nullptr) {
                continue;
            }
            if (params.LodIndex >= entry.Mesh->mLods.Size()) {
                continue;
            }

            const auto& lod = entry.Mesh->mLods[params.LodIndex];
            if (lod.mSections.IsEmpty()) {
                continue;
            }

            const u32 sectionCount = static_cast<u32>(lod.mSections.Size());
            for (u32 sectionIndex = 0U; sectionIndex < sectionCount; ++sectionIndex) {
                const auto&            section  = lod.mSections[sectionIndex];
                RenderCore::FMaterial* material = nullptr;
                if (entry.Materials != nullptr) {
                    const auto* slot = entry.Materials->GetMaterialSlot(section.MaterialSlot);
                    if (slot != nullptr) {
                        material = materialCache.ResolveMaterial(slot->Template, slot->Parameters);
                    }
                }
                if (material == nullptr) {
                    material = materialCache.ResolveDefault();
                }

                FDrawItem item{};
                item.mMeshType             = RenderCore::Render::EDrawMeshType::StaticMesh;
                item.mPass                 = params.Pass;
                item.mMaterial             = material;
                item.mStatic.mMesh         = entry.Mesh;
                item.mStatic.mLodIndex     = params.LodIndex;
                item.mStatic.mSectionIndex = sectionIndex;
                item.mInstance.mWorld      = entry.WorldMatrix;
                item.mInstance.mPrevWorld  = entry.PrevWorldMatrix;
                item.mInstance.mObjectId   = entry.OwnerId.IsValid() ? entry.OwnerId.Index : 0U;

                item.mKey.mPassKey     = static_cast<u64>(params.Pass);
                item.mKey.mPipelineKey = BuildPipelineKey(
                    material != nullptr ? material->FindPassDesc(params.Pass) : nullptr);
                item.mKey.mMaterialKey = HashPointer(material);
                item.mKey.mGeometryKey =
                    BuildGeometryKey(entry.Mesh, params.LodIndex, lod.mPrimitiveTopology);
                item.mKey.mSectionKey = BuildSectionKey(section);

                items.PushBack(Move(item));
            }
        }

        if (items.IsEmpty()) {
            return;
        }

        Core::Algorithm::Sort(
            items, [](const FDrawItem& lhs, const FDrawItem& rhs) { return lhs.mKey < rhs.mKey; });

        outDrawList.mBuckets.Reserve(items.Size());
        for (const auto& item : items) {
            const auto bucketKey = BuildMaterialBucketKey(item.mKey);
            if (outDrawList.mBuckets.IsEmpty()
                || !(bucketKey == outDrawList.mBuckets.Back().mBucketKey)) {
                RenderCore::Render::FDrawMaterialBucket bucket{};
                bucket.mBucketKey = bucketKey;
                bucket.mPass      = item.mPass;
                bucket.mMaterial  = item.mMaterial;
                outDrawList.mBuckets.PushBack(Move(bucket));
            }

            auto& bucket = outDrawList.mBuckets.Back();
            if (bucket.mBatches.IsEmpty() || !params.bAllowInstancing
                || !(item.mKey == bucket.mBatches.Back().mBatchKey)) {
                RenderCore::Render::FDrawBatch batch{};
                batch.mBatchKey = item.mKey;
                batch.mPass     = item.mPass;
                batch.mMaterial = item.mMaterial;
                batch.mStatic   = item.mStatic;
                batch.mInstances.PushBack(item.mInstance);
                bucket.mBatches.PushBack(Move(batch));
            } else {
                bucket.mBatches.Back().mInstances.PushBack(item.mInstance);
            }
        }
    }
} // namespace AltinaEngine::Engine

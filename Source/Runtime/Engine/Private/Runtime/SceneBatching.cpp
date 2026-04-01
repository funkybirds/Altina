#include "Engine/Runtime/SceneBatching.h"

#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Material/Material.h"
#include "Material/MaterialPass.h"
#include "Shader/ShaderRegistry.h"
#include "Types/Conversion.h"
#include "Algorithm/Sort.h"
#include "Container/HashUtility.h"
#include "Logging/Log.h"
#include "Math/LinAlg/RenderingMath.h"

using AltinaEngine::Move;
namespace AltinaEngine::Engine {
    namespace {
        using Core::Math::FMatrix4x4f;
        using Core::Math::FVector3f;
        using Core::Math::FVector4f;
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

        struct FFrustumPlane {
            FVector3f mNormal   = FVector3f(0.0f);
            f32       mDistance = 0.0f;
        };

        struct FFrustumCullContext {
            FFrustumPlane mPlanes[6]{};
            bool          mEnabled = false;
        };

        [[nodiscard]] auto NormalizePlane(const FFrustumPlane& plane) noexcept -> FFrustumPlane {
            const f32 lengthSq = plane.mNormal[0] * plane.mNormal[0]
                + plane.mNormal[1] * plane.mNormal[1] + plane.mNormal[2] * plane.mNormal[2];
            if (lengthSq <= 1e-8f) {
                return plane;
            }

            const f32     invLength = 1.0f / Core::Math::Sqrt(lengthSq);
            FFrustumPlane out{};
            out.mNormal[0] = plane.mNormal[0] * invLength;
            out.mNormal[1] = plane.mNormal[1] * invLength;
            out.mNormal[2] = plane.mNormal[2] * invLength;
            out.mDistance  = plane.mDistance * invLength;
            return out;
        }

        void ExtractFrustumPlanes(
            const FMatrix4x4f& viewProj, FFrustumPlane (&outPlanes)[6]) noexcept {
            const FVector4f row0(viewProj(0, 0), viewProj(0, 1), viewProj(0, 2), viewProj(0, 3));
            const FVector4f row1(viewProj(1, 0), viewProj(1, 1), viewProj(1, 2), viewProj(1, 3));
            const FVector4f row2(viewProj(2, 0), viewProj(2, 1), viewProj(2, 2), viewProj(2, 3));
            const FVector4f row3(viewProj(3, 0), viewProj(3, 1), viewProj(3, 2), viewProj(3, 3));

            const auto      setPlane = [&outPlanes](u32 index, const FVector4f& coeffs) {
                FFrustumPlane plane{};
                plane.mNormal[0] = coeffs[0];
                plane.mNormal[1] = coeffs[1];
                plane.mNormal[2] = coeffs[2];
                plane.mDistance  = coeffs[3];
                outPlanes[index] = NormalizePlane(plane);
            };

            setPlane(0U, row3 + row0); // left
            setPlane(1U, row3 - row0); // right
            setPlane(2U, row3 + row1); // bottom
            setPlane(3U, row3 - row1); // top
            setPlane(4U, row2);        // near (D3D/Vulkan z >= 0)
            setPlane(5U, row3 - row2); // far
        }

        [[nodiscard]] auto IsOutsidePlane(const FFrustumPlane& plane, const FVector3f& minWS,
            const FVector3f& maxWS) noexcept -> bool {
            FVector3f support(0.0f);
            support[0] = (plane.mNormal[0] >= 0.0f) ? maxWS[0] : minWS[0];
            support[1] = (plane.mNormal[1] >= 0.0f) ? maxWS[1] : minWS[1];
            support[2] = (plane.mNormal[2] >= 0.0f) ? maxWS[2] : minWS[2];

            const f32 distance = plane.mNormal[0] * support[0] + plane.mNormal[1] * support[1]
                + plane.mNormal[2] * support[2] + plane.mDistance;
            return distance < 0.0f;
        }

        [[nodiscard]] auto TryBuildWorldBounds(const RenderCore::Geometry::FStaticMeshData* mesh,
            u32 lodIndex, const FMatrix4x4f& worldMatrix, FVector3f& outMinWS,
            FVector3f& outMaxWS) noexcept -> bool {
            if (mesh == nullptr || lodIndex >= mesh->mLods.Size()) {
                return false;
            }

            const auto& lodBounds = mesh->mLods[lodIndex].mBounds;
            if (!lodBounds.IsValid()) {
                return false;
            }

            return Core::Math::LinAlg::TransformAabbToWorld(
                worldMatrix, lodBounds.Max, lodBounds.Min, outMinWS, outMaxWS);
        }

        [[nodiscard]] auto BuildFrustumCullContext(const FSceneView& view,
            const FSceneBatchBuildParams& params) noexcept -> FFrustumCullContext {
            FFrustumCullContext context{};
            if (!params.bEnableFrustumCulling || !view.View.IsValid()) {
                return context;
            }

            const FMatrix4x4f& cullViewProj =
                params.bUseCustomCullMatrix ? params.mCullViewProj : view.View.Matrices.ViewProj;
            ExtractFrustumPlanes(cullViewProj, context.mPlanes);
            context.mEnabled = true;
            return context;
        }

        [[nodiscard]] auto IsVisibleInFrustum(const FVector3f& minWS, const FVector3f& maxWS,
            const FFrustumCullContext& context) noexcept -> bool {
            if (!context.mEnabled) {
                return true;
            }

            for (const auto& plane : context.mPlanes) {
                if (IsOutsidePlane(plane, minWS, maxWS)) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] auto IsBeyondShadowCullDistance(const FSceneView& view,
            const FVector3f& minWS, const FVector3f& maxWS,
            const FSceneBatchBuildParams& params) noexcept -> bool {
            if (!params.bEnableShadowDistanceCulling || params.mShadowCullMaxViewDepth <= 0.0f) {
                return false;
            }

            FVector3f minVS(0.0f);
            FVector3f maxVS(0.0f);
            if (!Core::Math::LinAlg::TransformAabbToWorld(
                    view.View.Matrices.View, maxWS, minWS, minVS, maxVS)) {
                return false;
            }

            const f32 maxAllowedDepth = params.mShadowCullMaxViewDepth
                + Core::Math::Max(params.mShadowCullViewDepthPadding, 0.0f);
            return minVS[2] > maxAllowedDepth;
        }

        [[nodiscard]] auto ComputeWorldBoundsRadius(
            const FVector3f& minWS, const FVector3f& maxWS) noexcept -> f32 {
            const FVector3f extents((maxWS[0] - minWS[0]) * 0.5f, (maxWS[1] - minWS[1]) * 0.5f,
                (maxWS[2] - minWS[2]) * 0.5f);
            return Core::Math::Sqrt(
                extents[0] * extents[0] + extents[1] * extents[1] + extents[2] * extents[2]);
        }

        [[nodiscard]] auto IsSmallShadowCaster(const FVector3f& minWS, const FVector3f& maxWS,
            const FSceneBatchBuildParams& params) noexcept -> bool {
            if (!params.bEnableShadowSmallCasterCulling
                || params.mShadowMinCasterRadiusWs <= 0.0f) {
                return false;
            }

            return ComputeWorldBoundsRadius(minWS, maxWS) < params.mShadowMinCasterRadiusWs;
        }
    } // namespace

    void FSceneBatchBuilder::Build(const FRenderScene& scene, const FSceneView& view,
        const FSceneBatchBuildParams& params, FMaterialCache& materialCache,
        RenderCore::Render::FDrawList& outDrawList) const {
        outDrawList.Clear();
        const TChar* debugName =
            params.mDebugName.IsEmptyString() ? TEXT("Unnamed") : params.mDebugName.CStr();

        const u32 totalStaticMeshes = static_cast<u32>(scene.StaticMeshes.Size());
        if (scene.StaticMeshes.IsEmpty()) {
            LogInfoCat(TEXT("Engine.SceneBatching"),
                "Build summary: name={}, pass={}, frustumEnabled={}, shadowDistanceEnabled={}, smallCasterEnabled={}, staticMeshes=0, frustumCandidates=0, frustumCulled=0, shadowDistanceCandidates=0, shadowDistanceCulled=0, smallCasterCandidates=0, smallCasterCulled=0, visibleMeshes=0, buckets=0, batches=0, instances=0.",
                debugName, static_cast<u32>(params.Pass), params.bEnableFrustumCulling ? 1U : 0U,
                params.bEnableShadowDistanceCulling ? 1U : 0U,
                params.bEnableShadowSmallCasterCulling ? 1U : 0U);
            return;
        }

        u32        visibleMeshCount          = 0U;
        u32        frustumCandidateCount     = 0U;
        u32        frustumCulledCount        = 0U;
        u32        shadowDistanceCandidates  = 0U;
        u32        shadowDistanceCulledCount = 0U;
        u32        smallCasterCandidates     = 0U;
        u32        smallCasterCulledCount    = 0U;

        const auto frustumContext = BuildFrustumCullContext(view, params);

        usize      totalSections = 0;
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

            FVector3f  minWS(0.0f);
            FVector3f  maxWS(0.0f);
            const bool bHasWorldBounds =
                TryBuildWorldBounds(entry.Mesh, params.LodIndex, entry.WorldMatrix, minWS, maxWS);

            if (params.bEnableFrustumCulling) {
                ++frustumCandidateCount;
                if (bHasWorldBounds && !IsVisibleInFrustum(minWS, maxWS, frustumContext)) {
                    ++frustumCulledCount;
                    continue;
                }
            }
            if (params.bEnableShadowDistanceCulling) {
                ++shadowDistanceCandidates;
                if (bHasWorldBounds && IsBeyondShadowCullDistance(view, minWS, maxWS, params)) {
                    ++shadowDistanceCulledCount;
                    continue;
                }
            }
            if (params.bEnableShadowSmallCasterCulling) {
                ++smallCasterCandidates;
                if (bHasWorldBounds && IsSmallShadowCaster(minWS, maxWS, params)) {
                    ++smallCasterCulledCount;
                    continue;
                }
            }
            ++visibleMeshCount;

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
            LogInfoCat(TEXT("Engine.SceneBatching"),
                "Build summary: name={}, pass={}, frustumEnabled={}, shadowDistanceEnabled={}, smallCasterEnabled={}, staticMeshes={}, frustumCandidates={}, frustumCulled={}, shadowDistanceCandidates={}, shadowDistanceCulled={}, smallCasterCandidates={}, smallCasterCulled={}, visibleMeshes={}, buckets=0, batches=0, instances=0.",
                debugName, static_cast<u32>(params.Pass), params.bEnableFrustumCulling ? 1U : 0U,
                params.bEnableShadowDistanceCulling ? 1U : 0U,
                params.bEnableShadowSmallCasterCulling ? 1U : 0U, totalStaticMeshes,
                frustumCandidateCount, frustumCulledCount, shadowDistanceCandidates,
                shadowDistanceCulledCount, smallCasterCandidates, smallCasterCulledCount,
                visibleMeshCount);
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

        u32 totalInstanceCount = 0U;
        outDrawList.ForEachBatch([&totalInstanceCount](const auto& batch) {
            totalInstanceCount += static_cast<u32>(batch.mInstances.Size());
        });

        LogInfoCat(TEXT("Engine.SceneBatching"),
            "Build summary: name={}, pass={}, frustumEnabled={}, shadowDistanceEnabled={}, smallCasterEnabled={}, staticMeshes={}, frustumCandidates={}, frustumCulled={}, shadowDistanceCandidates={}, shadowDistanceCulled={}, smallCasterCandidates={}, smallCasterCulled={}, visibleMeshes={}, buckets={}, batches={}, instances={}.",
            debugName, static_cast<u32>(params.Pass), params.bEnableFrustumCulling ? 1U : 0U,
            params.bEnableShadowDistanceCulling ? 1U : 0U,
            params.bEnableShadowSmallCasterCulling ? 1U : 0U, totalStaticMeshes,
            frustumCandidateCount, frustumCulledCount, shadowDistanceCandidates,
            shadowDistanceCulledCount, smallCasterCandidates, smallCasterCulledCount,
            visibleMeshCount, outDrawList.GetBucketCount(), outDrawList.GetBatchCount(),
            totalInstanceCount);
    }
} // namespace AltinaEngine::Engine

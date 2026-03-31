#pragma once

#include "Container/Vector.h"
#include "Geometry/StaticMeshData.h"
#include "Material/Material.h"
#include "Material/MaterialPass.h"
#include "Math/Matrix.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore::Render {
    namespace Container = Core::Container;
    namespace Math      = Core::Math;
    using Container::TVector;

    enum class EDrawMeshType : u8 {
        StaticMesh = 0,
        // DynamicMesh / SkinnedMesh reserved for future extension.
    };

    struct FDrawKey {
        u64                   mPassKey     = 0ULL; // EMaterialPass
        u64                   mPipelineKey = 0ULL; // ShaderKey + Raster/Depth/Blend
        u64                   mMaterialKey = 0ULL; // Material instance / BindGroup
        u64                   mGeometryKey = 0ULL; // Vertex/Index buffers + topology
        u64                   mSectionKey  = 0ULL; // FirstIndex/IndexCount/BaseVertex

        friend constexpr auto operator==(const FDrawKey& lhs, const FDrawKey& rhs) noexcept
            -> bool = default;
    };

    [[nodiscard]] inline constexpr auto operator<(const FDrawKey& lhs, const FDrawKey& rhs) noexcept
        -> bool {
        if (lhs.mPassKey != rhs.mPassKey) {
            return lhs.mPassKey < rhs.mPassKey;
        }
        if (lhs.mPipelineKey != rhs.mPipelineKey) {
            return lhs.mPipelineKey < rhs.mPipelineKey;
        }
        if (lhs.mMaterialKey != rhs.mMaterialKey) {
            return lhs.mMaterialKey < rhs.mMaterialKey;
        }
        if (lhs.mGeometryKey != rhs.mGeometryKey) {
            return lhs.mGeometryKey < rhs.mGeometryKey;
        }
        return lhs.mSectionKey < rhs.mSectionKey;
    }

    struct FStaticMeshDrawArgs {
        const Geometry::FStaticMeshData* mMesh         = nullptr;
        u32                              mLodIndex     = 0U;
        u32                              mSectionIndex = 0U;
    };

    struct FDrawInstanceData {
        Math::FMatrix4x4f mWorld;
        Math::FMatrix4x4f mPrevWorld; // Reserved for motion vectors / TAA.
        u32               mObjectId = 0U;
    };

    struct FDrawItem {
        EDrawMeshType       mMeshType = EDrawMeshType::StaticMesh;
        EMaterialPass       mPass     = EMaterialPass::BasePass;
        const FMaterial*    mMaterial = nullptr;
        FDrawKey            mKey;

        FStaticMeshDrawArgs mStatic;
        FDrawInstanceData   mInstance;
    };

    struct FDrawBatch {
        FDrawKey                   mBatchKey;
        EMaterialPass              mPass     = EMaterialPass::BasePass;
        const FMaterial*           mMaterial = nullptr;
        FStaticMeshDrawArgs        mStatic;
        TVector<FDrawInstanceData> mInstances; // Same Mesh+Material+Section can be instanced.
    };

    struct FDrawMaterialBucketKey {
        u64                   mPassKey     = 0ULL;
        u64                   mPipelineKey = 0ULL;
        u64                   mMaterialKey = 0ULL;

        friend constexpr auto operator==(const FDrawMaterialBucketKey& lhs,
            const FDrawMaterialBucketKey& rhs) noexcept -> bool = default;
    };

    struct FDrawMaterialBucket {
        FDrawMaterialBucketKey mBucketKey;
        EMaterialPass          mPass     = EMaterialPass::BasePass;
        const FMaterial*       mMaterial = nullptr;
        TVector<FDrawBatch> mBatches; // Same material bucket, but potentially different geometry.
    };

    struct FDrawList {
        TVector<FDrawMaterialBucket> mBuckets;

        void                         Clear() noexcept { mBuckets.Clear(); }
        [[nodiscard]] auto           IsEmpty() const noexcept -> bool {
            for (const auto& bucket : mBuckets) {
                if (!bucket.mBatches.IsEmpty()) {
                    return false;
                }
            }
            return true;
        }
        [[nodiscard]] auto GetBucketCount() const noexcept -> u32 {
            u32 count = 0U;
            for (const auto& bucket : mBuckets) {
                if (!bucket.mBatches.IsEmpty()) {
                    ++count;
                }
            }
            return count;
        }
        [[nodiscard]] auto GetBatchCount() const noexcept -> u32 {
            u32 count = 0U;
            for (const auto& bucket : mBuckets) {
                count += static_cast<u32>(bucket.mBatches.Size());
            }
            return count;
        }

        template <typename TFn> void ForEachBatch(TFn&& fn) {
            for (auto& bucket : mBuckets) {
                for (auto& batch : bucket.mBatches) {
                    fn(batch);
                }
            }
        }

        template <typename TFn> void ForEachBatch(TFn&& fn) const {
            for (const auto& bucket : mBuckets) {
                for (const auto& batch : bucket.mBatches) {
                    fn(batch);
                }
            }
        }
    };
} // namespace AltinaEngine::RenderCore::Render

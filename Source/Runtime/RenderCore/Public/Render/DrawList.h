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

    struct FDrawList {
        TVector<FDrawBatch> mBatches;

        void                Clear() noexcept { mBatches.Clear(); }
        [[nodiscard]] auto  IsEmpty() const noexcept -> bool { return mBatches.IsEmpty(); }
    };
} // namespace AltinaEngine::RenderCore::Render

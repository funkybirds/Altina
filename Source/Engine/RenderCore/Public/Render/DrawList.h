#pragma once

#include "RenderCoreAPI.h"

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
        u64 PassKey     = 0ULL; // EMaterialPass
        u64 PipelineKey = 0ULL; // ShaderKey + Raster/Depth/Blend
        u64 MaterialKey = 0ULL; // Material instance / BindGroup
        u64 GeometryKey = 0ULL; // Vertex/Index buffers + topology
        u64 SectionKey  = 0ULL; // FirstIndex/IndexCount/BaseVertex

        friend constexpr auto operator==(const FDrawKey& lhs, const FDrawKey& rhs) noexcept
            -> bool = default;
    };

    [[nodiscard]] inline constexpr auto operator<(const FDrawKey& lhs,
        const FDrawKey& rhs) noexcept -> bool {
        if (lhs.PassKey != rhs.PassKey) {
            return lhs.PassKey < rhs.PassKey;
        }
        if (lhs.PipelineKey != rhs.PipelineKey) {
            return lhs.PipelineKey < rhs.PipelineKey;
        }
        if (lhs.MaterialKey != rhs.MaterialKey) {
            return lhs.MaterialKey < rhs.MaterialKey;
        }
        if (lhs.GeometryKey != rhs.GeometryKey) {
            return lhs.GeometryKey < rhs.GeometryKey;
        }
        return lhs.SectionKey < rhs.SectionKey;
    }

    struct FStaticMeshDrawArgs {
        const Geometry::FStaticMeshData* Mesh         = nullptr;
        u32                              LodIndex     = 0U;
        u32                              SectionIndex = 0U;
    };

    struct FDrawInstanceData {
        Math::FMatrix4x4f World;
        Math::FMatrix4x4f PrevWorld; // Reserved for motion vectors / TAA.
        u32               ObjectId = 0U;
    };

    struct FDrawItem {
        EDrawMeshType          MeshType = EDrawMeshType::StaticMesh;
        EMaterialPass          Pass     = EMaterialPass::BasePass;
        const FMaterial*       Material = nullptr;
        FDrawKey               Key;

        FStaticMeshDrawArgs    Static;
        FDrawInstanceData      Instance; // 单实例（当前）
    };

    struct FDrawBatch {
        FDrawKey               BatchKey;
        EMaterialPass          Pass     = EMaterialPass::BasePass;
        const FMaterial*       Material = nullptr;
        FStaticMeshDrawArgs    Static;
        TVector<FDrawInstanceData> Instances; // Same Mesh+Material+Section can be instanced.
    };

    struct FDrawList {
        TVector<FDrawBatch> Batches;

        void                Clear() noexcept { Batches.Clear(); }
        [[nodiscard]] auto  IsEmpty() const noexcept -> bool { return Batches.IsEmpty(); }
    };
} // namespace AltinaEngine::RenderCore::Render

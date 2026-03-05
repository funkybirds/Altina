#include "Rendering/DrawListExecutor.h"

#include "Geometry/StaticMeshData.h"
#include "Material/Material.h"
#include "Material/MaterialPass.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiPipeline.h"
#include "Logging/Log.h"

namespace AltinaEngine::Rendering {
    namespace {
        auto GetSection(const RenderCore::Geometry::FStaticMeshLodData& lod, u32 sectionIndex)
            -> const RenderCore::Geometry::FStaticMeshSection* {
            if (sectionIndex >= lod.Sections.Size()) {
                return nullptr;
            }
            return &lod.Sections[sectionIndex];
        }

        void BindVertexBuffers(
            Rhi::FRhiCmdContext& ctx, const RenderCore::Geometry::FStaticMeshLodData& lod) {
            const auto posView = lod.PositionBuffer.GetView();
            if (posView.mBuffer != nullptr) {
                ctx.RHISetVertexBuffer(0U, posView);
            }

            const auto tangentView = lod.TangentBuffer.GetView();
            if (tangentView.mBuffer != nullptr) {
                ctx.RHISetVertexBuffer(1U, tangentView);
            }

            const auto uv0View = lod.UV0Buffer.GetView();
            if (uv0View.mBuffer != nullptr) {
                ctx.RHISetVertexBuffer(2U, uv0View);
            }

            const auto uv1View = lod.UV1Buffer.GetView();
            if (uv1View.mBuffer != nullptr) {
                ctx.RHISetVertexBuffer(3U, uv1View);
            }
        }
    } // namespace

    void FDrawListExecutor::ExecuteBasePass(Rhi::FRhiCmdContext& ctx,
        const RenderCore::Render::FDrawList& drawList, const FDrawListBindings& bindings,
        FDrawPipelineResolver pipelineResolver, void* pipelineUserData,
        FDrawBatchBinder batchBinder, void* batchUserData) {
        static u64 sBasePassExecCount = 0ULL;
        ++sBasePassExecCount;

        if (drawList.Batches.IsEmpty()) {
            return;
        }

        u32 batchCount          = 0U;
        u32 drawCallCount       = 0U;
        u32 skippedNullMesh     = 0U;
        u32 skippedInvalidLod   = 0U;
        u32 skippedNullSection  = 0U;
        u32 skippedNullPipeline = 0U;
        u32 skippedNullIndex    = 0U;
        u32 skippedZeroInst     = 0U;

        for (const auto& batch : drawList.Batches) {
            ++batchCount;
            const auto* mesh = batch.Static.Mesh;
            if (mesh == nullptr) {
                ++skippedNullMesh;
                continue;
            }
            if (batch.Static.LodIndex >= mesh->Lods.Size()) {
                ++skippedInvalidLod;
                continue;
            }

            const auto& lod     = mesh->Lods[batch.Static.LodIndex];
            const auto* section = GetSection(lod, batch.Static.SectionIndex);
            if (section == nullptr) {
                ++skippedNullSection;
                continue;
            }

            const auto* passDesc =
                (batch.Material != nullptr) ? batch.Material->FindPassDesc(batch.Pass) : nullptr;
            if (pipelineResolver != nullptr) {
                auto* pipeline = pipelineResolver(batch, passDesc, pipelineUserData);
                if (pipeline == nullptr) {
                    // Never draw with a stale pipeline from a previous pass/batch.
                    ++skippedNullPipeline;
                    continue;
                }
                ctx.RHISetGraphicsPipeline(pipeline);
            }

            if (bindings.PerFrame != nullptr) {
                ctx.RHISetBindGroup(bindings.PerFrameSetIndex, bindings.PerFrame, nullptr, 0U);
            }

            if (batch.Material != nullptr) {
                auto group = batch.Material->GetBindGroup(batch.Pass);
                if (group) {
                    ctx.RHISetBindGroup(bindings.PerMaterialSetIndex, group.Get(), nullptr, 0U);
                }
            }

            if (batchBinder != nullptr) {
                batchBinder(ctx, batch, batchUserData);
            }

            const auto indexView = lod.IndexBuffer.GetView();
            if (indexView.mBuffer == nullptr) {
                ++skippedNullIndex;
                continue;
            }

            ctx.RHISetPrimitiveTopology(lod.PrimitiveTopology);
            BindVertexBuffers(ctx, lod);
            ctx.RHISetIndexBuffer(indexView);

            const u32 instanceCount = static_cast<u32>(batch.Instances.Size());
            if (instanceCount == 0U) {
                ++skippedZeroInst;
                continue;
            }

            ctx.RHIDrawIndexed(
                section->IndexCount, instanceCount, section->FirstIndex, section->BaseVertex, 0U);
            ++drawCallCount;
        }

        if (drawCallCount == 0U) {
            LogWarningCat(TEXT("Rendering.DrawList"),
                "ExecuteBasePass: zero draw calls (batches={}, nullMesh={}, invalidLod={}, nullSection={}, nullPipeline={}, nullIndex={}, zeroInstance={}).",
                batchCount, skippedNullMesh, skippedInvalidLod, skippedNullSection,
                skippedNullPipeline, skippedNullIndex, skippedZeroInst);
            return;
        }

        if ((sBasePassExecCount % 120ULL) == 0ULL) {
            LogInfoCat(TEXT("Rendering.DrawList"),
                "ExecuteBasePass summary: draws={}, batches={}, skips(nullMesh={}, invalidLod={}, nullSection={}, nullPipeline={}, nullIndex={}, zeroInstance={}).",
                drawCallCount, batchCount, skippedNullMesh, skippedInvalidLod, skippedNullSection,
                skippedNullPipeline, skippedNullIndex, skippedZeroInst);
        }
    }
} // namespace AltinaEngine::Rendering

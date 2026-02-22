#include "Rendering/DrawListExecutor.h"

#include "Geometry/StaticMeshData.h"
#include "Material/Material.h"
#include "Material/MaterialPass.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiPipeline.h"

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
        if (drawList.Batches.IsEmpty()) {
            return;
        }

        for (const auto& batch : drawList.Batches) {
            const auto* mesh = batch.Static.Mesh;
            if (mesh == nullptr) {
                continue;
            }
            if (batch.Static.LodIndex >= mesh->Lods.Size()) {
                continue;
            }

            const auto& lod     = mesh->Lods[batch.Static.LodIndex];
            const auto* section = GetSection(lod, batch.Static.SectionIndex);
            if (section == nullptr) {
                continue;
            }

            const auto* passDesc =
                (batch.Material != nullptr) ? batch.Material->FindPassDesc(batch.Pass) : nullptr;
            if (pipelineResolver != nullptr) {
                if (auto* pipeline = pipelineResolver(batch, passDesc, pipelineUserData)) {
                    ctx.RHISetGraphicsPipeline(pipeline);
                }
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
                continue;
            }

            ctx.RHISetPrimitiveTopology(lod.PrimitiveTopology);
            BindVertexBuffers(ctx, lod);
            ctx.RHISetIndexBuffer(indexView);

            const u32 instanceCount = static_cast<u32>(batch.Instances.Size());
            if (instanceCount == 0U) {
                continue;
            }

            ctx.RHIDrawIndexed(
                section->IndexCount, instanceCount, section->FirstIndex, section->BaseVertex, 0U);
        }
    }
} // namespace AltinaEngine::Rendering

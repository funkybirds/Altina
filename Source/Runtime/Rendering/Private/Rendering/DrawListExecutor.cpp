#include "Rendering/DrawListExecutor.h"

#include "Geometry/StaticMeshData.h"
#include "Material/Material.h"
#include "Material/MaterialPass.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiPipeline.h"
#include "Utility/String/StringViewUtility.h"
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

        void BindVertexBuffersLegacy(
            Rhi::FRhiCmdContext& ctx, const RenderCore::Geometry::FStaticMeshLodData& lod) {
            const auto positionView = lod.PositionBuffer.GetView();
            if (positionView.mBuffer != nullptr) {
                ctx.RHISetVertexBuffer(0U, positionView);
            }

            const auto normalView = lod.TangentBuffer.GetView();
            if (normalView.mBuffer != nullptr) {
                ctx.RHISetVertexBuffer(1U, normalView);
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

        auto ResolveVertexStreamView(const RenderCore::Geometry::FStaticMeshLodData& lod,
            const Rhi::FRhiVertexAttributeDesc& attr, Rhi::FRhiVertexBufferView& outView) -> bool {
            const auto semantic = attr.mSemanticName.ToView();
            if (Core::Utility::String::EqualsIgnoreCase(semantic, TEXT("POSITION"))) {
                outView = lod.PositionBuffer.GetView();
                return outView.mBuffer != nullptr;
            }
            if (Core::Utility::String::EqualsIgnoreCase(semantic, TEXT("NORMAL"))
                || Core::Utility::String::EqualsIgnoreCase(semantic, TEXT("TANGENT"))) {
                // Static-mesh slot1 currently stores packed normal(float3).
                outView = lod.TangentBuffer.GetView();
                return outView.mBuffer != nullptr;
            }
            if (Core::Utility::String::EqualsIgnoreCase(semantic, TEXT("TEXCOORD"))) {
                if (attr.mSemanticIndex == 0U) {
                    outView = lod.UV0Buffer.GetView();
                } else if (attr.mSemanticIndex == 1U) {
                    outView = lod.UV1Buffer.GetView();
                } else {
                    return false;
                }
                return outView.mBuffer != nullptr;
            }
            return false;
        }

        void BindVertexBuffersResolved(Rhi::FRhiCmdContext& ctx,
            const RenderCore::Geometry::FStaticMeshLodData& lod,
            const Rhi::FRhiVertexLayoutDesc*                layout) {
            if (layout == nullptr || layout->mAttributes.IsEmpty()) {
                BindVertexBuffersLegacy(ctx, lod);
                return;
            }

            u32 boundCount = 0U;
            u32 missCount  = 0U;
            for (const auto& attr : layout->mAttributes) {
                Rhi::FRhiVertexBufferView view{};
                if (!ResolveVertexStreamView(lod, attr, view)) {
                    ++missCount;
                    continue;
                }
                ctx.RHISetVertexBuffer(attr.mInputSlot, view);
                ++boundCount;
            }

            if (boundCount == 0U) {
                BindVertexBuffersLegacy(ctx, lod);
                LogWarningCat(TEXT("Rendering.DrawList"),
                    "BindVertexBuffersResolved: no streams matched resolved layout, fallback to legacy binding.");
                return;
            }
            if (missCount > 0U) {
                LogWarningCat(TEXT("Rendering.DrawList"),
                    "BindVertexBuffersResolved: {} attributes not bound from resolved layout.",
                    missCount);
            }
        }
    } // namespace

    void FDrawListExecutor::ExecuteBasePass(Rhi::FRhiCmdContext& ctx,
        const RenderCore::Render::FDrawList& drawList, const FDrawListBindings& bindings,
        FDrawPipelineResolver pipelineResolver, void* pipelineUserData,
        FDrawBatchBinder batchBinder, void* batchUserData) {
        static u64 sBasePassExecCount = 0ULL;
        ++sBasePassExecCount;

        if (drawList.mBatches.IsEmpty()) {
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

        for (const auto& batch : drawList.mBatches) {
            ++batchCount;
            const auto* mesh = batch.mStatic.mMesh;
            if (mesh == nullptr) {
                ++skippedNullMesh;
                continue;
            }
            if (batch.mStatic.mLodIndex >= mesh->Lods.Size()) {
                ++skippedInvalidLod;
                continue;
            }

            const auto& lod     = mesh->Lods[batch.mStatic.mLodIndex];
            const auto* section = GetSection(lod, batch.mStatic.mSectionIndex);
            if (section == nullptr) {
                ++skippedNullSection;
                continue;
            }

            const auto* passDesc =
                (batch.mMaterial != nullptr) ? batch.mMaterial->FindPassDesc(batch.mPass) : nullptr;
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

            if (batch.mMaterial != nullptr) {
                auto group = batch.mMaterial->GetBindGroup(batch.mPass);
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
            BindVertexBuffersResolved(ctx, lod, bindings.ResolvedVertexLayout);
            ctx.RHISetIndexBuffer(indexView);

            const u32 instanceCount = static_cast<u32>(batch.mInstances.Size());
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

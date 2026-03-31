#include "Rendering/DrawListExecutor.h"

#include "Container/HashSet.h"
#include "Geometry/StaticMeshData.h"
#include "Material/Material.h"
#include "Material/MaterialPass.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiPipeline.h"
#include "Utility/String/StringViewUtility.h"
#include "Logging/Log.h"

namespace AltinaEngine::Rendering {
    namespace {
        void BindVertexBufferTracked(Rhi::FRhiCmdContext& ctx, u32 slot,
            const Rhi::FRhiVertexBufferView& view, u32& outBindCount) {
            if (view.mBuffer == nullptr) {
                return;
            }

            ctx.RHISetVertexBuffer(slot, view);
            ++outBindCount;
        }

        auto GetSection(const RenderCore::Geometry::FStaticMeshLodData& lod, u32 sectionIndex)
            -> const RenderCore::Geometry::FStaticMeshSection* {
            if (sectionIndex >= lod.mSections.Size()) {
                return nullptr;
            }
            return &lod.mSections[sectionIndex];
        }

        void BindVertexBuffersLegacy(Rhi::FRhiCmdContext&   ctx,
            const RenderCore::Geometry::FStaticMeshLodData& lod, u32& outBindCount) {
            const auto positionView = lod.mPositionBuffer.GetView();
            BindVertexBufferTracked(ctx, 0U, positionView, outBindCount);

            const auto normalView = lod.mTangentBuffer.GetView();
            BindVertexBufferTracked(ctx, 1U, normalView, outBindCount);

            const auto uv0View = lod.mUV0Buffer.GetView();
            BindVertexBufferTracked(ctx, 2U, uv0View, outBindCount);

            const auto uv1View = lod.mUV1Buffer.GetView();
            BindVertexBufferTracked(ctx, 3U, uv1View, outBindCount);
        }

        auto ResolveVertexStreamView(const RenderCore::Geometry::FStaticMeshLodData& lod,
            const Rhi::FRhiVertexAttributeDesc& attr, Rhi::FRhiVertexBufferView& outView) -> bool {
            const auto semantic = attr.mSemanticName.ToView();
            if (Core::Utility::String::EqualsIgnoreCase(semantic, TEXT("POSITION"))) {
                outView = lod.mPositionBuffer.GetView();
                return outView.mBuffer != nullptr;
            }
            if (Core::Utility::String::EqualsIgnoreCase(semantic, TEXT("NORMAL"))
                || Core::Utility::String::EqualsIgnoreCase(semantic, TEXT("TANGENT"))) {
                // Static-mesh slot1 currently stores packed normal(float3).
                outView = lod.mTangentBuffer.GetView();
                return outView.mBuffer != nullptr;
            }
            if (Core::Utility::String::EqualsIgnoreCase(semantic, TEXT("TEXCOORD"))) {
                if (attr.mSemanticIndex == 0U) {
                    outView = lod.mUV0Buffer.GetView();
                } else if (attr.mSemanticIndex == 1U) {
                    outView = lod.mUV1Buffer.GetView();
                } else {
                    return false;
                }
                return outView.mBuffer != nullptr;
            }
            return false;
        }

        void BindVertexBuffersResolved(Rhi::FRhiCmdContext& ctx,
            const RenderCore::Geometry::FStaticMeshLodData& lod,
            const Rhi::FRhiVertexLayoutDesc* layout, u32& outBindCount) {
            if (layout == nullptr || layout->mAttributes.IsEmpty()) {
                BindVertexBuffersLegacy(ctx, lod, outBindCount);
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
                BindVertexBufferTracked(ctx, attr.mInputSlot, view, outBindCount);
                ++boundCount;
            }

            if (boundCount == 0U) {
                BindVertexBuffersLegacy(ctx, lod, outBindCount);
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
        if (drawList.IsEmpty()) {
            return;
        }

        const auto rhiStatsBefore = Rhi::RHIGetFrameStats();
        u32        passId         = 0U;
        for (const auto& bucket : drawList.mBuckets) {
            if (!bucket.mBatches.IsEmpty()) {
                passId = static_cast<u32>(bucket.mPass);
                break;
            }
        }

        u32                            batchCount            = 0U;
        u32                            materialBucketCount   = 0U;
        u32                            drawCallCount         = 0U;
        u32                            totalInstanceCount    = 0U;
        u32                            maxBatchInstCount     = 0U;
        u32                            skippedNullMesh       = 0U;
        u32                            skippedInvalidLod     = 0U;
        u32                            skippedNullSection    = 0U;
        u32                            skippedNullPipeline   = 0U;
        u32                            skippedNullIndex      = 0U;
        u32                            skippedZeroInst       = 0U;
        u32                            vertexBufferBindCount = 0U;
        Core::Container::THashSet<u64> uniqueGeometryKeys{};
        uniqueGeometryKeys.Reserve(drawList.GetBatchCount());

        for (const auto& bucket : drawList.mBuckets) {
            if (bucket.mBatches.IsEmpty()) {
                continue;
            }
            ++materialBucketCount;
            batchCount += static_cast<u32>(bucket.mBatches.Size());

            const auto* passDesc = (bucket.mMaterial != nullptr)
                ? bucket.mMaterial->FindPassDesc(bucket.mPass)
                : nullptr;
            if (pipelineResolver != nullptr) {
                auto* pipeline = pipelineResolver(bucket.mBatches[0], passDesc, pipelineUserData);
                if (pipeline == nullptr) {
                    skippedNullPipeline += static_cast<u32>(bucket.mBatches.Size());
                    continue;
                }
                ctx.RHISetGraphicsPipeline(pipeline);
            }

            if (bindings.PerFrame != nullptr) {
                ctx.RHISetBindGroup(bindings.PerFrameSetIndex, bindings.PerFrame, nullptr, 0U);
            }

            if (bucket.mMaterial != nullptr) {
                auto group = bucket.mMaterial->GetBindGroup(bucket.mPass);
                if (group) {
                    ctx.RHISetBindGroup(bindings.PerMaterialSetIndex, group.Get(), nullptr, 0U);
                }
            }

            for (const auto& batch : bucket.mBatches) {
                const auto* mesh = batch.mStatic.mMesh;
                if (mesh == nullptr) {
                    ++skippedNullMesh;
                    continue;
                }
                if (batch.mStatic.mLodIndex >= mesh->mLods.Size()) {
                    ++skippedInvalidLod;
                    continue;
                }

                const auto& lod     = mesh->mLods[batch.mStatic.mLodIndex];
                const auto* section = GetSection(lod, batch.mStatic.mSectionIndex);
                if (section == nullptr) {
                    ++skippedNullSection;
                    continue;
                }
                uniqueGeometryKeys.Insert(batch.mBatchKey.mGeometryKey);

                if (batchBinder != nullptr) {
                    batchBinder(ctx, batch, batchUserData);
                }

                const auto indexView = lod.mIndexBuffer.GetView();
                if (indexView.mBuffer == nullptr) {
                    ++skippedNullIndex;
                    continue;
                }

                ctx.RHISetPrimitiveTopology(lod.mPrimitiveTopology);
                BindVertexBuffersResolved(
                    ctx, lod, bindings.ResolvedVertexLayout, vertexBufferBindCount);
                ctx.RHISetIndexBuffer(indexView);

                const u32 instanceCount = static_cast<u32>(batch.mInstances.Size());
                if (instanceCount == 0U) {
                    ++skippedZeroInst;
                    continue;
                }
                totalInstanceCount += instanceCount;
                if (instanceCount > maxBatchInstCount) {
                    maxBatchInstCount = instanceCount;
                }

                ctx.RHIDrawIndexed(section->IndexCount, instanceCount, section->FirstIndex,
                    section->BaseVertex, 0U);
                ++drawCallCount;
            }
        }

        if (drawCallCount == 0U) {
            LogWarningCat(TEXT("Rendering.DrawList"),
                "ExecuteBasePass: zero draw calls (batches={}, nullMesh={}, invalidLod={}, nullSection={}, nullPipeline={}, nullIndex={}, zeroInstance={}).",
                batchCount, skippedNullMesh, skippedInvalidLod, skippedNullSection,
                skippedNullPipeline, skippedNullIndex, skippedZeroInst);
            return;
        }

        const auto rhiStatsAfter = Rhi::RHIGetFrameStats();
        LogInfoCat(TEXT("Rendering.DrawList"),
            "ExecuteBasePass summary: pass={}, materialBuckets={}, draws={}, batches={}, uniqueGeometry={}, vbBinds={}, rhiVbBinds={}, instances={}, maxBatchInstances={}, skips(nullMesh={}, invalidLod={}, nullSection={}, nullPipeline={}, nullIndex={}, zeroInstance={}).",
            passId, materialBucketCount, drawCallCount, batchCount,
            static_cast<u32>(uniqueGeometryKeys.Num()), vertexBufferBindCount,
            static_cast<u32>(
                rhiStatsAfter.mSetVertexBufferCalls - rhiStatsBefore.mSetVertexBufferCalls),
            totalInstanceCount, maxBatchInstCount, skippedNullMesh, skippedInvalidLod,
            skippedNullSection, skippedNullPipeline, skippedNullIndex, skippedZeroInst);
    }
} // namespace AltinaEngine::Rendering

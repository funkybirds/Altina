#include "Deferred/DeferredCsm.h"

#include "Container/Vector.h"
#include "FrameGraph/FrameGraph.h"
#include "Material/Material.h"
#include "Rhi/RhiDebugMarker.h"
#include "Rhi/RhiInit.h"
#include "Utility/Assert.h"

#include <algorithm>

namespace AltinaEngine::Rendering::Deferred {
    namespace {
        using Core::Utility::Assert;

        struct FImportedExternalTexture {
            Rhi::FRhiTexture*                 Texture = nullptr;
            RenderCore::FFrameGraphTextureRef Ref;
        };

        auto RegisterMaterialTextureReads(RenderCore::FFrameGraph& graph,
            RenderCore::FFrameGraphPassBuilder& builder, const RenderCore::FMaterial* material,
            RenderCore::EMaterialPass                           pass,
            Core::Container::TVector<FImportedExternalTexture>& importedExternalTextures) -> void {
            if (material == nullptr) {
                return;
            }

            const auto* layout = material->FindLayout(pass);
            if (layout == nullptr) {
                return;
            }

            auto findOrImportExternalTextureRef =
                [&graph, &importedExternalTextures](
                    Rhi::FRhiTexture* texture) -> RenderCore::FFrameGraphTextureRef {
                if (texture == nullptr) {
                    return {};
                }
                for (const auto& imported : importedExternalTextures) {
                    if (imported.Texture == texture) {
                        return imported.Ref;
                    }
                }

                const auto ref = graph.ImportTexture(texture, Rhi::ERhiResourceState::Common);
                importedExternalTextures.PushBack({ texture, ref });
                return ref;
            };

            const auto& parameters = material->GetParameters();
            for (const auto paramId : layout->mTextureNameHashes) {
                const auto* textureParam = parameters.FindTextureParam(paramId);
                if (textureParam == nullptr || !textureParam->mSrv) {
                    continue;
                }
                auto* texture = textureParam->mSrv->GetTexture();
                if (texture == nullptr) {
                    continue;
                }

                const auto ref = findOrImportExternalTextureRef(texture);
                if (ref.IsValid()) {
                    builder.Read(ref, Rhi::ERhiResourceState::ShaderResource);
                }
            }
        }
    } // namespace

    [[nodiscard]] auto BuildCsm(const FCsmBuildInputs& inputs) -> FCsmBuildResult {
        FCsmBuildResult result{};

        // Defaults tuned for demo stability with large far planes.
        u32             cascades = RenderCore::Shadow::kMaxCascades;
        f32             lambda   = 0.65f;
        f32             maxDist  = 250.0f;
        u32             mapSize  = 2048U;
        f32             recvBias = 0.0015f;

        if (inputs.Lights != nullptr && inputs.Lights->mHasMainDirectionalLight) {
            const auto& dl = inputs.Lights->mMainDirectionalLight;
            cascades       = dl.mShadowCascadeCount;
            lambda         = dl.mShadowSplitLambda;
            maxDist        = dl.mShadowMaxDistance;
            mapSize        = dl.mShadowMapSize;
            recvBias       = dl.mShadowReceiverBias;
        }

        cascades = std::max(1U, std::min(cascades, RenderCore::Shadow::kMaxCascades));
        if (lambda < 0.0f) {
            lambda = 0.0f;
        } else if (lambda > 1.0f) {
            lambda = 1.0f;
        }
        if (maxDist < 0.0f) {
            maxDist = 0.0f;
        }
        if (mapSize == 0U) {
            mapSize = 2048U;
        }
        if (recvBias < 0.0f) {
            recvBias = 0.0f;
        }

        result.Settings.mCascadeCount  = cascades;
        result.Settings.mSplitLambda   = lambda;
        result.Settings.mMaxDistance   = maxDist;
        result.Settings.mShadowMapSize = mapSize;
        result.Settings.mReceiverBias  = recvBias;

        if (inputs.View != nullptr && inputs.Lights != nullptr
            && inputs.Lights->mHasMainDirectionalLight
            && inputs.Lights->mMainDirectionalLight.mCastShadows) {
            RenderCore::Shadow::BuildDirectionalCSM(
                *inputs.View, inputs.Lights->mMainDirectionalLight, result.Settings, result.Data);
        }

        return result;
    }

    void FillPerFrameCsmConstants(
        const FCsmBuildResult& csm, FPerFrameConstants& outPerFrameConstants) {
        outPerFrameConstants.CSMCascadeCount = csm.Data.mCascadeCount;
        outPerFrameConstants.ShadowBias      = csm.Settings.mReceiverBias;
        if (csm.Settings.mShadowMapSize > 0U) {
            const f32 inv = 1.0f / static_cast<f32>(csm.Settings.mShadowMapSize);
            outPerFrameConstants.ShadowMapInvSize[0] = inv;
            outPerFrameConstants.ShadowMapInvSize[1] = inv;
        }

        for (u32 i = 0U; i < RenderCore::Shadow::kMaxCascades; ++i) {
            outPerFrameConstants.CSM_SplitsVS[i][0] = 0.0f;
            outPerFrameConstants.CSM_SplitsVS[i][1] = 0.0f;
            outPerFrameConstants.CSM_SplitsVS[i][2] = 0.0f;
            outPerFrameConstants.CSM_SplitsVS[i][3] = 0.0f;
        }

        if (csm.Data.mCascadeCount == 0U) {
            return;
        }

        for (u32 i = 0U; i < csm.Data.mCascadeCount && i < RenderCore::Shadow::kMaxCascades; ++i) {
            outPerFrameConstants.CSM_SplitsVS[i][0] = csm.Data.mCascades[i].mSplitVs[0];
            outPerFrameConstants.CSM_SplitsVS[i][1] = csm.Data.mCascades[i].mSplitVs[1];
        }

        outPerFrameConstants.CSM_LightViewProj0 = csm.Data.mCascades[0].mLightViewProj;
        if (csm.Data.mCascadeCount > 1U) {
            outPerFrameConstants.CSM_LightViewProj1 = csm.Data.mCascades[1].mLightViewProj;
        }
        if (csm.Data.mCascadeCount > 2U) {
            outPerFrameConstants.CSM_LightViewProj2 = csm.Data.mCascades[2].mLightViewProj;
        }
        if (csm.Data.mCascadeCount > 3U) {
            outPerFrameConstants.CSM_LightViewProj3 = csm.Data.mCascades[3].mLightViewProj;
        }
    }

    void AddCsmShadowPasses(
        FCsmShadowPassInputs& inputs, RenderCore::FFrameGraphTextureRef& outShadowMap) {
        if (inputs.Graph == nullptr || inputs.View == nullptr || inputs.Csm == nullptr
            || inputs.PersistentShadowMap == nullptr || inputs.PersistentShadowMapSize == nullptr
            || inputs.PersistentShadowMapLayers == nullptr || inputs.ExecuteCascadeFn == nullptr) {
            return;
        }

        const auto& csmData     = inputs.Csm->Data;
        const auto& csmSettings = inputs.Csm->Settings;
        if (csmData.mCascadeCount == 0U) {
            return;
        }

        auto* device = Rhi::RHIGetDevice();
        Assert(device != nullptr, TEXT("BasicDeferredRenderer"),
            "Render failed: RHI device is null while preparing CSM shadow map.");

        const u32  shadowSize   = csmSettings.mShadowMapSize;
        const u32  shadowLayers = csmData.mCascadeCount;

        const bool bNeedRecreateShadowMap = (!(*inputs.PersistentShadowMap))
            || (*inputs.PersistentShadowMapSize != shadowSize)
            || (*inputs.PersistentShadowMapLayers != shadowLayers);
        if (bNeedRecreateShadowMap) {
            inputs.PersistentShadowMap->Reset();

            Rhi::FRhiTextureDesc shadowDesc{};
            shadowDesc.mDebugName.Assign(TEXT("ShadowMap.CSM"));
            shadowDesc.mWidth       = shadowSize;
            shadowDesc.mHeight      = shadowSize;
            shadowDesc.mArrayLayers = shadowLayers;
            // Keep ShadowMap SRV shape consistent with shaders (Texture2DArray), even when only
            // one cascade is active.
            shadowDesc.mDimension = Rhi::ERhiTextureDimension::Tex2DArray;
            shadowDesc.mFormat    = Rhi::ERhiFormat::D32Float;
            shadowDesc.mBindFlags =
                Rhi::ERhiTextureBindFlags::DepthStencil | Rhi::ERhiTextureBindFlags::ShaderResource;

            *inputs.PersistentShadowMap = device->CreateTexture(shadowDesc);
            Assert(static_cast<bool>(*inputs.PersistentShadowMap), TEXT("BasicDeferredRenderer"),
                "Render failed: CreateTexture(ShadowMap.CSM) returned null (size={}, layers={}).",
                shadowSize, shadowLayers);

            *inputs.PersistentShadowMapSize   = shadowSize;
            *inputs.PersistentShadowMapLayers = shadowLayers;
        }

        auto& graph = *inputs.Graph;
        outShadowMap =
            graph.ImportTexture(inputs.PersistentShadowMap->Get(), Rhi::ERhiResourceState::Common);
        Assert(outShadowMap.IsValid(), TEXT("BasicDeferredRenderer"),
            "Render failed: ImportTexture(ShadowMap.CSM) returned invalid ref.");

        RenderCore::FFrameGraphPassDesc shadowPassDesc{};
        shadowPassDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
        shadowPassDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

        Core::Container::TVector<FImportedExternalTexture> importedExternalTextures;

        for (u32 cascade = 0U; cascade < csmData.mCascadeCount; ++cascade) {
            struct FShadowPassData {
                RenderCore::FFrameGraphTextureRef Shadow;
                RenderCore::FFrameGraphDSVRef     ShadowDSV;
            };

            switch (cascade) {
                case 0U:
                    shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade0";
                    break;
                case 1U:
                    shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade1";
                    break;
                case 2U:
                    shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade2";
                    break;
                case 3U:
                    shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade3";
                    break;
                default:
                    shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade";
                    break;
            }

            graph.AddPass<FShadowPassData>(
                shadowPassDesc,
                [&](RenderCore::FFrameGraphPassBuilder& builder, FShadowPassData& data) {
                    data.Shadow = builder.Write(outShadowMap, Rhi::ERhiResourceState::DepthWrite);

                    const auto* shadowDrawList =
                        (cascade < 4U) ? inputs.ShadowDrawLists[cascade] : nullptr;
                    if (shadowDrawList != nullptr) {
                        for (const auto& bucket : shadowDrawList->mBuckets) {
                            RegisterMaterialTextureReads(graph, builder, bucket.mMaterial,
                                bucket.mPass, importedExternalTextures);
                        }
                    }

                    Rhi::FRhiTextureViewRange range{};
                    range.mBaseMip         = 0U;
                    range.mMipCount        = 1U;
                    range.mBaseArrayLayer  = cascade;
                    range.mLayerCount      = 1U;
                    range.mBaseDepthSlice  = 0U;
                    range.mDepthSliceCount = 1U;

                    Rhi::FRhiDepthStencilViewDesc dsvDesc{};
                    dsvDesc.mDebugName.Assign(TEXT("ShadowMap.CSM.DSV"));
                    dsvDesc.mFormat = Rhi::ERhiFormat::D32Float;
                    dsvDesc.mRange  = range;
                    data.ShadowDSV  = builder.CreateDSV(data.Shadow, dsvDesc);

                    RenderCore::FRdgDepthStencilBinding ds{};
                    ds.mDSV                      = data.ShadowDSV;
                    ds.mDepthLoadOp              = Rhi::ERhiLoadOp::Clear;
                    ds.mDepthStoreOp             = Rhi::ERhiStoreOp::Store;
                    ds.mClearDepthStencil.mDepth = inputs.View->bReverseZ ? 0.0f : 1.0f;
                    builder.SetRenderTargets(nullptr, 0U, &ds);
                },
                [executeFn = inputs.ExecuteCascadeFn, userData = inputs.ExecuteCascadeUserData,
                    cascade, lightViewProj = csmData.mCascades[cascade].mLightViewProj,
                    shadowSize](Rhi::FRhiCmdContext& ctx,
                    const RenderCore::FFrameGraphPassResources&, const FShadowPassData&) -> void {
                    const TChar* markerName = TEXT("Deferred.Shadow.Cascade");
                    switch (cascade) {
                        case 0U:
                            markerName = TEXT("Deferred.Shadow.Cascade0");
                            break;
                        case 1U:
                            markerName = TEXT("Deferred.Shadow.Cascade1");
                            break;
                        case 2U:
                            markerName = TEXT("Deferred.Shadow.Cascade2");
                            break;
                        case 3U:
                            markerName = TEXT("Deferred.Shadow.Cascade3");
                            break;
                        default:
                            break;
                    }
                    Rhi::FRhiDebugMarker marker(ctx, markerName);
                    executeFn(ctx, cascade, lightViewProj, shadowSize, userData);
                });
        }
    }
} // namespace AltinaEngine::Rendering::Deferred

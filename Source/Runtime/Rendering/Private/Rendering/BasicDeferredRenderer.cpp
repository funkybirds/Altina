
#include "Rendering/BasicDeferredRenderer.h"

#include "Rendering/DrawListExecutor.h"

#include "FrameGraph/FrameGraph.h"
#include "Material/MaterialPass.h"
#include "View/ViewData.h"

#include "Container/HashMap.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Logging/Log.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiTexture.h"

#include <algorithm>

using AltinaEngine::Move;
namespace AltinaEngine::Rendering {
    namespace {
        namespace Container = Core::Container;
        using Container::THashMap;
        using Container::TVector;
        using RenderCore::EMaterialPass;

        struct FPerFrameConstants {
            Core::Math::FMatrix4x4f ViewProjection;
        };

        struct FPerDrawConstants {
            Core::Math::FMatrix4x4f World;
        };

        struct FDeferredSharedResources {
            RenderCore::FShaderRegistry                       Registry;
            RenderCore::FShaderRegistry::FShaderKey           OutputVSKey;
            RenderCore::FShaderRegistry::FShaderKey           OutputPSKey;
            RenderCore::FMaterialPassDesc                     DefaultPassDesc;
            Container::TShared<RenderCore::FMaterialTemplate> DefaultTemplate;

            Rhi::FRhiBindGroupLayoutRef                       PerFrameLayout;
            Rhi::FRhiBindGroupLayoutRef                       PerDrawLayout;
            Rhi::FRhiBindGroupLayoutRef                       OutputLayout;
            Rhi::FRhiSamplerRef                               OutputSampler;
            Rhi::FRhiPipelineLayoutRef                        OutputPipelineLayout;
            Rhi::FRhiPipelineRef                              OutputPipeline;
            THashMap<u64, Rhi::FRhiPipelineRef>               BasePipelines;
            THashMap<u64, Rhi::FRhiBindGroupLayoutRef>        MaterialLayouts;
            THashMap<u64, Rhi::FRhiPipelineLayoutRef>         BasePipelineLayouts;
            Rhi::FRhiVertexLayoutDesc                         BaseVertexLayout;
            bool                                              bBaseVertexLayoutReady = false;
        };

        auto GetSharedResources() -> FDeferredSharedResources& {
            static FDeferredSharedResources resources;
            return resources;
        }

        auto BuildLayoutHash(const TVector<Rhi::FRhiBindGroupLayoutEntry>& entries, u32 setIndex)
            -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            constexpr u64 kPrime  = 1099511628211ULL;
            u64           hash    = kOffset;
            auto          mix     = [&](u64 value) { hash = (hash ^ value) * kPrime; };

            mix(setIndex);
            for (const auto& entry : entries) {
                mix(entry.mBinding);
                mix(static_cast<u64>(entry.mType));
                mix(static_cast<u64>(entry.mVisibility));
                mix(entry.mArrayCount);
                mix(entry.mHasDynamicOffset ? 1ULL : 0ULL);
            }
            return hash;
        }

        auto BuildMaterialBindGroupLayoutDesc(const RenderCore::FMaterialLayout& materialLayout,
            TVector<Rhi::FRhiBindGroupLayoutEntry>& outEntries) -> u64 {
            outEntries.Clear();

            if (materialLayout.PropertyBag.IsValid()) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding          = materialLayout.PropertyBag.GetBinding();
                entry.mType             = Rhi::ERhiBindingType::ConstantBuffer;
                entry.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entry.mArrayCount       = 1U;
                entry.mHasDynamicOffset = false;
                outEntries.PushBack(entry);
            }

            const usize textureCount = materialLayout.TextureBindings.Size();
            for (usize i = 0U; i < textureCount; ++i) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding          = materialLayout.TextureBindings[i];
                entry.mType             = Rhi::ERhiBindingType::SampledTexture;
                entry.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entry.mArrayCount       = 1U;
                entry.mHasDynamicOffset = false;
                outEntries.PushBack(entry);
            }

            const usize samplerCount = materialLayout.SamplerBindings.Size();
            for (usize i = 0U; i < samplerCount; ++i) {
                const u32 samplerBinding = materialLayout.SamplerBindings[i];
                if (samplerBinding == RenderCore::kMaterialInvalidBinding) {
                    continue;
                }
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding          = samplerBinding;
                entry.mType             = Rhi::ERhiBindingType::Sampler;
                entry.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entry.mArrayCount       = 1U;
                entry.mHasDynamicOffset = false;
                outEntries.PushBack(entry);
            }

            std::sort(outEntries.begin(), outEntries.end(), [](const auto& lhs, const auto& rhs) {
                if (lhs.mBinding != rhs.mBinding) {
                    return lhs.mBinding < rhs.mBinding;
                }
                return lhs.mType < rhs.mType;
            });

            return BuildLayoutHash(outEntries, 0U);
        }

        void UpdateDefaultPassDesc(FDeferredSharedResources& resources) {
            if (!resources.DefaultTemplate) {
                return;
            }
            const auto* baseDesc = resources.DefaultTemplate->FindPassDesc(EMaterialPass::BasePass);
            const auto* anyDesc =
                baseDesc ? baseDesc : resources.DefaultTemplate->FindAnyPassDesc();
            if (anyDesc != nullptr) {
                resources.DefaultPassDesc = *anyDesc;
            }
        }

        void EnsureDefaultTemplate(FDeferredSharedResources& resources) {
            UpdateDefaultPassDesc(resources);
        }

        auto EnsureOutputPipeline(Rhi::FRhiDevice& device, FDeferredSharedResources& resources)
            -> bool {
            if (resources.OutputPipeline) {
                return true;
            }

            if (!resources.OutputVSKey.IsValid() || !resources.OutputPSKey.IsValid()) {
                LogError(TEXT("Deferred output shaders are not configured."));
                return false;
            }

            auto outputVs = resources.Registry.FindShader(resources.OutputVSKey);
            auto outputPs = resources.Registry.FindShader(resources.OutputPSKey);
            if (!outputVs || !outputPs) {
                LogError(TEXT("Deferred output shaders are not registered."));
                return false;
            }

            Rhi::FRhiGraphicsPipelineDesc outputDesc{};
            outputDesc.mDebugName.Assign(TEXT("BasicDeferred.OutputPipeline"));
            outputDesc.mVertexShader   = outputVs.Get();
            outputDesc.mPixelShader    = outputPs.Get();
            outputDesc.mPipelineLayout = resources.OutputPipelineLayout.Get();
            outputDesc.mVertexLayout   = {};
            resources.OutputPipeline   = device.CreateGraphicsPipeline(outputDesc);

            return resources.OutputPipeline.Get() != nullptr;
        }

        void EnsureLayouts(Rhi::FRhiDevice& device, FDeferredSharedResources& resources) {
            if (!resources.PerFrameLayout) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding    = 0U;
                entry.mType       = Rhi::ERhiBindingType::ConstantBuffer;
                entry.mVisibility = Rhi::ERhiShaderStageFlags::All;

                Rhi::FRhiBindGroupLayoutEntry samplerEntry{};
                samplerEntry.mBinding    = 0U;
                samplerEntry.mType       = Rhi::ERhiBindingType::Sampler;
                samplerEntry.mVisibility = Rhi::ERhiShaderStageFlags::All;

                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex = 0U;
                layoutDesc.mEntries.PushBack(entry);
                layoutDesc.mEntries.PushBack(samplerEntry);
                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                resources.PerFrameLayout = device.CreateBindGroupLayout(layoutDesc);
            }

            if (!resources.PerDrawLayout) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding    = 4U;
                entry.mType       = Rhi::ERhiBindingType::ConstantBuffer;
                entry.mVisibility = Rhi::ERhiShaderStageFlags::All;

                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex = 0U;
                layoutDesc.mEntries.PushBack(entry);
                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                resources.PerDrawLayout = device.CreateBindGroupLayout(layoutDesc);
            }

            if (!resources.OutputLayout) {
                Rhi::FRhiBindGroupLayoutEntry textureEntry{};
                textureEntry.mBinding    = 0U;
                textureEntry.mType       = Rhi::ERhiBindingType::SampledTexture;
                textureEntry.mVisibility = Rhi::ERhiShaderStageFlags::All;

                Rhi::FRhiBindGroupLayoutEntry samplerEntry{};
                samplerEntry.mBinding    = 0U;
                samplerEntry.mType       = Rhi::ERhiBindingType::Sampler;
                samplerEntry.mVisibility = Rhi::ERhiShaderStageFlags::All;

                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex = 0U;
                layoutDesc.mEntries.PushBack(textureEntry);
                layoutDesc.mEntries.PushBack(samplerEntry);
                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                resources.OutputLayout = device.CreateBindGroupLayout(layoutDesc);
            }

            if (!resources.OutputSampler) {
                Rhi::FRhiSamplerDesc samplerDesc{};
                resources.OutputSampler = Rhi::RHICreateSampler(samplerDesc);
            }

            if (!resources.OutputPipelineLayout) {
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                if (resources.OutputLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.OutputLayout.Get());
                }
                resources.OutputPipelineLayout = device.CreatePipelineLayout(layoutDesc);
            }
        }

        void EnsureVertexLayout(FDeferredSharedResources& resources) {
            if (resources.bBaseVertexLayoutReady) {
                return;
            }

            Rhi::FRhiVertexAttributeDesc position{};
            position.mSemanticName.Assign(TEXT("POSITION"));
            position.mSemanticIndex     = 0U;
            position.mFormat            = Rhi::ERhiFormat::R32G32B32Float;
            position.mInputSlot         = 0U;
            position.mAlignedByteOffset = 0U;
            position.mPerInstance       = false;
            position.mInstanceStepRate  = 0U;

            resources.BaseVertexLayout.mAttributes.PushBack(position);
            resources.bBaseVertexLayoutReady = true;
        }

        void UpdateConstantBuffer(Rhi::FRhiBuffer* buffer, const void* data, u64 sizeBytes) {
            if (buffer == nullptr || data == nullptr || sizeBytes == 0ULL) {
                return;
            }

            auto lock = buffer->Lock(0ULL, sizeBytes, Rhi::ERhiBufferLockMode::WriteDiscard);
            if (!lock.IsValid()) {
                return;
            }
            Core::Platform::Generic::Memcpy(lock.mData, data, static_cast<usize>(sizeBytes));
            buffer->Unlock(lock);
        }
        struct FBasePassPipelineData {
            Rhi::FRhiDevice*                     Device          = nullptr;
            RenderCore::FShaderRegistry*         Registry        = nullptr;
            THashMap<u64, Rhi::FRhiPipelineRef>* PipelineCache   = nullptr;
            const RenderCore::FMaterialPassDesc* DefaultPassDesc = nullptr;
            Rhi::FRhiVertexLayoutDesc            VertexLayout;
        };

        auto ResolveBasePassPipeline(const RenderCore::Render::FDrawBatch& batch,
            const RenderCore::FMaterialPassDesc* passDesc, void* userData) -> Rhi::FRhiPipeline* {
            auto* data = static_cast<FBasePassPipelineData*>(userData);
            if (!data || !data->Device || !data->Registry || !data->PipelineCache) {
                return nullptr;
            }

            const auto* resolvedPass = passDesc ? passDesc : data->DefaultPassDesc;
            if (!resolvedPass) {
                return nullptr;
            }

            auto&       resources  = GetSharedResources();
            const auto& passLayout = resolvedPass->Layout;
            if (!passLayout.PropertyBag.IsValid() && passLayout.TextureBindings.IsEmpty()) {
                return nullptr;
            }

            TVector<Rhi::FRhiBindGroupLayoutEntry> layoutEntries;
            const u64                              materialLayoutHash =
                BuildMaterialBindGroupLayoutDesc(passLayout, layoutEntries);

            Rhi::FRhiBindGroupLayoutRef materialLayoutRef;
            if (const auto it = resources.MaterialLayouts.find(materialLayoutHash);
                it != resources.MaterialLayouts.end()) {
                materialLayoutRef = it->second;
            } else {
                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex   = 0U;
                layoutDesc.mEntries    = layoutEntries;
                layoutDesc.mLayoutHash = materialLayoutHash;
                materialLayoutRef      = data->Device->CreateBindGroupLayout(layoutDesc);
                if (!materialLayoutRef) {
                    return nullptr;
                }
                resources.MaterialLayouts[materialLayoutHash] = materialLayoutRef;
                LogInfo(TEXT("BasePass MaterialLayout entries={} hash={}"),
                    static_cast<u32>(layoutEntries.Size()), materialLayoutHash);
                for (const auto& entry : layoutEntries) {
                    LogInfo(TEXT("  MaterialLayout binding={} type={} vis={}"), entry.mBinding,
                        static_cast<u32>(entry.mType), static_cast<u32>(entry.mVisibility));
                }
            }

            Rhi::FRhiPipelineLayoutRef pipelineLayout;
            if (const auto it = resources.BasePipelineLayouts.find(materialLayoutHash);
                it != resources.BasePipelineLayouts.end()) {
                pipelineLayout = it->second;
            } else {
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                if (resources.PerFrameLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.PerFrameLayout.Get());
                }
                if (resources.PerDrawLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.PerDrawLayout.Get());
                }
                if (materialLayoutRef) {
                    layoutDesc.mBindGroupLayouts.PushBack(materialLayoutRef.Get());
                }
                pipelineLayout = data->Device->CreatePipelineLayout(layoutDesc);
                if (!pipelineLayout) {
                    return nullptr;
                }
                resources.BasePipelineLayouts[materialLayoutHash] = pipelineLayout;
                LogInfo(TEXT("BasePass PipelineLayout groups={} hash={}"),
                    static_cast<u32>(layoutDesc.mBindGroupLayouts.Size()), materialLayoutHash);
            }

            const u64 key =
                batch.BatchKey.PipelineKey ^ (materialLayoutHash * 0x9e3779b97f4a7c15ULL);
            if (const auto it = data->PipelineCache->find(key); it != data->PipelineCache->end()) {
                return it->second.Get();
            }

            const auto vs = data->Registry->FindShader(resolvedPass->Shaders.Vertex);
            const auto ps = data->Registry->FindShader(resolvedPass->Shaders.Pixel);
            if (!vs || !ps) {
                return nullptr;
            }

            Rhi::FRhiGraphicsPipelineDesc desc{};
            desc.mDebugName.Assign(TEXT("BasicDeferred.BasePassPipeline"));
            desc.mVertexShader   = vs.Get();
            desc.mPixelShader    = ps.Get();
            desc.mPipelineLayout = pipelineLayout.Get();
            desc.mVertexLayout   = data->VertexLayout;

            auto pipeline = data->Device->CreateGraphicsPipeline(desc);
            if (!pipeline) {
                return nullptr;
            }

            (*data->PipelineCache)[key] = pipeline;
            return pipeline.Get();
        }

        struct FBasePassBindingData {
            Rhi::FRhiBuffer*    PerDrawBuffer   = nullptr;
            Rhi::FRhiBindGroup* PerDrawGroup    = nullptr;
            u32                 PerDrawSetIndex = 1U;
        };

        void BindPerDraw(
            Rhi::FRhiCmdContext& ctx, const RenderCore::Render::FDrawBatch& batch, void* userData) {
            auto* data = static_cast<FBasePassBindingData*>(userData);
            if (!data || data->PerDrawBuffer == nullptr || data->PerDrawGroup == nullptr) {
                return;
            }

            if (batch.Instances.IsEmpty()) {
                return;
            }

            FPerDrawConstants constants{};
            constants.World = batch.Instances[0].World;
            UpdateConstantBuffer(data->PerDrawBuffer, &constants, sizeof(constants));
            ctx.RHISetBindGroup(data->PerDrawSetIndex, data->PerDrawGroup, nullptr, 0U);
        }
    } // namespace

    auto FBasicDeferredRenderer::GetDefaultMaterialTemplate()
        -> Core::Container::TShared<RenderCore::FMaterialTemplate> {
        auto& resources = GetSharedResources();
        EnsureDefaultTemplate(resources);
        return resources.DefaultTemplate;
    }

    void FBasicDeferredRenderer::SetDefaultMaterialTemplate(
        Core::Container::TShared<RenderCore::FMaterialTemplate> templ) noexcept {
        auto& resources           = GetSharedResources();
        resources.DefaultTemplate = Move(templ);
        resources.DefaultPassDesc = {};
        resources.MaterialLayouts.clear();
        resources.BasePipelineLayouts.clear();
        resources.BasePipelines.clear();
        EnsureDefaultTemplate(resources);
    }

    void FBasicDeferredRenderer::SetOutputShaderKeys(
        const RenderCore::FShaderRegistry::FShaderKey& vs,
        const RenderCore::FShaderRegistry::FShaderKey& ps) noexcept {
        auto& resources       = GetSharedResources();
        resources.OutputVSKey = vs;
        resources.OutputPSKey = ps;
        resources.OutputPipeline.Reset();
    }

    auto FBasicDeferredRenderer::RegisterShader(
        const RenderCore::FShaderRegistry::FShaderKey& key, Rhi::FRhiShaderRef shader) -> bool {
        auto& resources = GetSharedResources();
        return resources.Registry.RegisterShader(key, Move(shader));
    }

    void FBasicDeferredRenderer::PrepareForRendering(Rhi::FRhiDevice& device) {
        auto& resources = GetSharedResources();
        EnsureDefaultTemplate(resources);
        EnsureVertexLayout(resources);
        EnsureLayouts(device, resources);
        if (!EnsureOutputPipeline(device, resources)) {
            return;
        }

        if (!mPerFrameBuffer) {
            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.PerFrame"));
            desc.mSizeBytes = sizeof(FPerFrameConstants);
            desc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::Constant;
            desc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
            mPerFrameBuffer = device.CreateBuffer(desc);
        }

        if (!mPerFrameGroup && mPerFrameBuffer && resources.PerFrameLayout) {
            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mLayout = resources.PerFrameLayout.Get();

            Rhi::FRhiBindGroupEntry entry{};
            entry.mBinding = 0U;
            entry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
            entry.mBuffer  = mPerFrameBuffer.Get();
            entry.mOffset  = 0ULL;
            entry.mSize    = static_cast<u64>(sizeof(FPerFrameConstants));
            groupDesc.mEntries.PushBack(entry);

            if (resources.OutputSampler) {
                Rhi::FRhiBindGroupEntry samplerEntry{};
                samplerEntry.mBinding = 0U;
                samplerEntry.mType    = Rhi::ERhiBindingType::Sampler;
                samplerEntry.mSampler = resources.OutputSampler.Get();
                groupDesc.mEntries.PushBack(samplerEntry);
            }

            mPerFrameGroup = device.CreateBindGroup(groupDesc);
        }

        if (!mPerDrawBuffer) {
            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.PerDraw"));
            desc.mSizeBytes = sizeof(FPerDrawConstants);
            desc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::Constant;
            desc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
            mPerDrawBuffer  = device.CreateBuffer(desc);
        }

        if (!mPerDrawGroup && mPerDrawBuffer && resources.PerDrawLayout) {
            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mLayout = resources.PerDrawLayout.Get();

            Rhi::FRhiBindGroupEntry entry{};
            entry.mBinding = 4U;
            entry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
            entry.mBuffer  = mPerDrawBuffer.Get();
            entry.mOffset  = 0ULL;
            entry.mSize    = static_cast<u64>(sizeof(FPerDrawConstants));
            groupDesc.mEntries.PushBack(entry);

            mPerDrawGroup = device.CreateBindGroup(groupDesc);
        }
    }
    void FBasicDeferredRenderer::Render(RenderCore::FFrameGraph& graph) {
        const auto* view         = mViewContext.View;
        auto*       outputTarget = mViewContext.OutputTarget;
        const auto* drawList     = mViewContext.DrawList;
        if (view == nullptr || outputTarget == nullptr) {
            return;
        }

        if (!view->IsValid()) {
            return;
        }

        const u32 width  = view->RenderTargetExtent.Width;
        const u32 height = view->RenderTargetExtent.Height;
        if (width == 0U || height == 0U) {
            return;
        }

        if (mPerFrameBuffer) {
            FPerFrameConstants constants{};
            constants.ViewProjection = view->Matrices.ViewProj;
            UpdateConstantBuffer(mPerFrameBuffer.Get(), &constants, sizeof(constants));
        }

        constexpr Rhi::FRhiClearColor     kAlbedoClear{ 0.12f, 0.12f, 0.12f, 1.0f };
        constexpr Rhi::FRhiClearColor     kNormalClear{ 0.5f, 0.5f, 1.0f, 1.0f };

        RenderCore::FFrameGraphTextureRef gbufferA;
        RenderCore::FFrameGraphTextureRef gbufferB;

        struct FBasePassData {
            RenderCore::FFrameGraphTextureRef GBufferA;
            RenderCore::FFrameGraphTextureRef GBufferB;
            RenderCore::FFrameGraphTextureRef Depth;
            RenderCore::FFrameGraphRTVRef     GBufferARTV;
            RenderCore::FFrameGraphRTVRef     GBufferBRTV;
            RenderCore::FFrameGraphDSVRef     DepthDSV;
        };

        RenderCore::FFrameGraphPassDesc basePassDesc{};
        basePassDesc.mName  = "BasicDeferred.BasePass";
        basePassDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
        basePassDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

        auto&             resources = GetSharedResources();

        FDrawListBindings drawBindings{};
        drawBindings.PerFrame            = mPerFrameGroup.Get();
        drawBindings.PerFrameSetIndex    = 0U;
        drawBindings.PerDrawSetIndex     = 0U;
        drawBindings.PerMaterialSetIndex = 0U;

        FBasePassPipelineData pipelineData{};
        pipelineData.Device          = Rhi::RHIGetDevice();
        pipelineData.Registry        = &resources.Registry;
        pipelineData.PipelineCache   = &resources.BasePipelines;
        pipelineData.DefaultPassDesc = &resources.DefaultPassDesc;
        pipelineData.VertexLayout    = resources.BaseVertexLayout;

        FBasePassBindingData bindingData{};
        bindingData.PerDrawBuffer   = mPerDrawBuffer.Get();
        bindingData.PerDrawGroup    = mPerDrawGroup.Get();
        bindingData.PerDrawSetIndex = drawBindings.PerDrawSetIndex;

        const RenderCore::View::FViewRect viewRect = view->ViewRect;

        graph.AddPass<FBasePassData>(
            basePassDesc,
            [&](RenderCore::FFrameGraphPassBuilder& builder, FBasePassData& data) {
                RenderCore::FFrameGraphTextureDesc gbufferADesc{};
                gbufferADesc.mDesc.mDebugName.Assign(TEXT("GBufferA.Albedo"));
                gbufferADesc.mDesc.mWidth     = width;
                gbufferADesc.mDesc.mHeight    = height;
                gbufferADesc.mDesc.mFormat    = Rhi::ERhiFormat::R8G8B8A8Unorm;
                gbufferADesc.mDesc.mBindFlags = Rhi::ERhiTextureBindFlags::RenderTarget
                    | Rhi::ERhiTextureBindFlags::ShaderResource;

                RenderCore::FFrameGraphTextureDesc gbufferBDesc{};
                gbufferBDesc.mDesc.mDebugName.Assign(TEXT("GBufferB.Normal"));
                gbufferBDesc.mDesc.mWidth     = width;
                gbufferBDesc.mDesc.mHeight    = height;
                gbufferBDesc.mDesc.mFormat    = Rhi::ERhiFormat::R16G16B16A16Float;
                gbufferBDesc.mDesc.mBindFlags = Rhi::ERhiTextureBindFlags::RenderTarget
                    | Rhi::ERhiTextureBindFlags::ShaderResource;

                RenderCore::FFrameGraphTextureDesc depthDesc{};
                depthDesc.mDesc.mDebugName.Assign(TEXT("GBufferDepth"));
                depthDesc.mDesc.mWidth     = width;
                depthDesc.mDesc.mHeight    = height;
                depthDesc.mDesc.mFormat    = Rhi::ERhiFormat::D24UnormS8Uint;
                depthDesc.mDesc.mBindFlags = Rhi::ERhiTextureBindFlags::DepthStencil;

                data.GBufferA = builder.CreateTexture(gbufferADesc);
                data.GBufferB = builder.CreateTexture(gbufferBDesc);
                data.Depth    = builder.CreateTexture(depthDesc);

                data.GBufferA = builder.Write(data.GBufferA, Rhi::ERhiResourceState::RenderTarget);
                data.GBufferB = builder.Write(data.GBufferB, Rhi::ERhiResourceState::RenderTarget);
                data.Depth    = builder.Write(data.Depth, Rhi::ERhiResourceState::DepthWrite);

                Rhi::FRhiTextureViewRange viewRange{};
                viewRange.mMipCount        = 1U;
                viewRange.mLayerCount      = 1U;
                viewRange.mDepthSliceCount = 1U;

                Rhi::FRhiRenderTargetViewDesc rtvDescA{};
                rtvDescA.mDebugName.Assign(TEXT("GBufferA.RTV"));
                rtvDescA.mFormat = gbufferADesc.mDesc.mFormat;
                rtvDescA.mRange  = viewRange;
                data.GBufferARTV = builder.CreateRTV(data.GBufferA, rtvDescA);

                Rhi::FRhiRenderTargetViewDesc rtvDescB{};
                rtvDescB.mDebugName.Assign(TEXT("GBufferB.RTV"));
                rtvDescB.mFormat = gbufferBDesc.mDesc.mFormat;
                rtvDescB.mRange  = viewRange;
                data.GBufferBRTV = builder.CreateRTV(data.GBufferB, rtvDescB);

                Rhi::FRhiDepthStencilViewDesc dsvDesc{};
                dsvDesc.mDebugName.Assign(TEXT("GBufferDepth.DSV"));
                dsvDesc.mFormat = depthDesc.mDesc.mFormat;
                dsvDesc.mRange  = viewRange;
                data.DepthDSV   = builder.CreateDSV(data.Depth, dsvDesc);

                RenderCore::FRdgRenderTargetBinding rtvs[2]{};
                rtvs[0].mRTV        = data.GBufferARTV;
                rtvs[0].mLoadOp     = Rhi::ERhiLoadOp::Clear;
                rtvs[0].mStoreOp    = Rhi::ERhiStoreOp::Store;
                rtvs[0].mClearColor = kAlbedoClear;

                rtvs[1].mRTV        = data.GBufferBRTV;
                rtvs[1].mLoadOp     = Rhi::ERhiLoadOp::Clear;
                rtvs[1].mStoreOp    = Rhi::ERhiStoreOp::Store;
                rtvs[1].mClearColor = kNormalClear;

                RenderCore::FRdgDepthStencilBinding depthBinding{};
                depthBinding.mDSV                      = data.DepthDSV;
                depthBinding.mDepthLoadOp              = Rhi::ERhiLoadOp::Clear;
                depthBinding.mDepthStoreOp             = Rhi::ERhiStoreOp::Store;
                depthBinding.mClearDepthStencil.mDepth = 1.0f;

                builder.SetRenderTargets(rtvs, 2U, &depthBinding);

                gbufferA = data.GBufferA;
                gbufferB = data.GBufferB;
            },
            [drawList, drawBindings, pipelineData, bindingData, viewRect](Rhi::FRhiCmdContext& ctx,
                const RenderCore::FFrameGraphPassResources&, const FBasePassData&) {
                LogInfo(TEXT("FG Pass: BasicDeferred.BasePass"));
                Rhi::FRhiViewportRect viewport{};
                viewport.mX        = static_cast<f32>(viewRect.X);
                viewport.mY        = static_cast<f32>(viewRect.Y);
                viewport.mWidth    = static_cast<f32>(viewRect.Width);
                viewport.mHeight   = static_cast<f32>(viewRect.Height);
                viewport.mMinDepth = 0.0f;
                viewport.mMaxDepth = 1.0f;
                ctx.RHISetViewport(viewport);

                Rhi::FRhiScissorRect scissor{};
                scissor.mX      = viewRect.X;
                scissor.mY      = viewRect.Y;
                scissor.mWidth  = viewRect.Width;
                scissor.mHeight = viewRect.Height;
                ctx.RHISetScissor(scissor);

                if (drawList != nullptr) {
                    FDrawListExecutor::ExecuteBasePass(ctx, *drawList, drawBindings,
                        ResolveBasePassPipeline, const_cast<FBasePassPipelineData*>(&pipelineData),
                        BindPerDraw, const_cast<FBasePassBindingData*>(&bindingData));
                }
            });
        auto outputTexture = graph.ImportTexture(outputTarget, Rhi::ERhiResourceState::Present);

        struct FOutputPassData {
            RenderCore::FFrameGraphTextureRef Output;
            RenderCore::FFrameGraphRTVRef     OutputRTV;
            RenderCore::FFrameGraphTextureRef GBufferA;
        };

        RenderCore::FFrameGraphPassDesc outputPassDesc{};
        outputPassDesc.mName  = "BasicDeferred.Output";
        outputPassDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
        outputPassDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

        graph.AddPass<FOutputPassData>(
            outputPassDesc,
            [&](RenderCore::FFrameGraphPassBuilder& builder, FOutputPassData& data) {
                if (gbufferA.IsValid()) {
                    builder.Read(gbufferA, Rhi::ERhiResourceState::ShaderResource);
                }
                if (gbufferB.IsValid()) {
                    builder.Read(gbufferB, Rhi::ERhiResourceState::ShaderResource);
                }

                data.GBufferA = gbufferA;
                data.Output   = builder.Write(outputTexture, Rhi::ERhiResourceState::RenderTarget);

                Rhi::FRhiTextureViewRange viewRange{};
                viewRange.mMipCount        = 1U;
                viewRange.mLayerCount      = 1U;
                viewRange.mDepthSliceCount = 1U;

                Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                rtvDesc.mDebugName.Assign(TEXT("BackBuffer.RTV"));
                rtvDesc.mFormat = outputTarget->GetDesc().mFormat;
                rtvDesc.mRange  = viewRange;
                data.OutputRTV  = builder.CreateRTV(data.Output, rtvDesc);

                RenderCore::FRdgRenderTargetBinding rtvBinding{};
                rtvBinding.mRTV        = data.OutputRTV;
                rtvBinding.mLoadOp     = Rhi::ERhiLoadOp::Clear;
                rtvBinding.mStoreOp    = Rhi::ERhiStoreOp::Store;
                rtvBinding.mClearColor = kAlbedoClear;

                builder.SetRenderTargets(&rtvBinding, 1U, nullptr);
                builder.SetExternalOutput(data.Output, Rhi::ERhiResourceState::Present);
            },
            [viewRect](Rhi::FRhiCmdContext& ctx, const RenderCore::FFrameGraphPassResources& res,
                const FOutputPassData& data) {
                LogInfo(TEXT("FG Pass: BasicDeferred.Output"));
                auto& shared = GetSharedResources();
                if (!shared.OutputPipeline) {
                    return;
                }

                auto* texture = res.GetTexture(data.GBufferA);
                if (!texture) {
                    return;
                }

                auto* device = Rhi::RHIGetDevice();
                if (!device || !shared.OutputLayout || !shared.OutputSampler) {
                    return;
                }

                Rhi::FRhiBindGroupDesc groupDesc{};
                groupDesc.mLayout = shared.OutputLayout.Get();

                Rhi::FRhiBindGroupEntry texEntry{};
                texEntry.mBinding = 0U;
                texEntry.mType    = Rhi::ERhiBindingType::SampledTexture;
                texEntry.mTexture = texture;
                groupDesc.mEntries.PushBack(texEntry);

                Rhi::FRhiBindGroupEntry samplerEntry{};
                samplerEntry.mBinding = 0U;
                samplerEntry.mType    = Rhi::ERhiBindingType::Sampler;
                samplerEntry.mSampler = shared.OutputSampler.Get();
                groupDesc.mEntries.PushBack(samplerEntry);

                auto bindGroup = device->CreateBindGroup(groupDesc);
                if (!bindGroup) {
                    return;
                }

                ctx.RHISetGraphicsPipeline(shared.OutputPipeline.Get());

                Rhi::FRhiViewportRect viewport{};
                viewport.mX        = static_cast<f32>(viewRect.X);
                viewport.mY        = static_cast<f32>(viewRect.Y);
                viewport.mWidth    = static_cast<f32>(viewRect.Width);
                viewport.mHeight   = static_cast<f32>(viewRect.Height);
                viewport.mMinDepth = 0.0f;
                viewport.mMaxDepth = 1.0f;
                ctx.RHISetViewport(viewport);

                Rhi::FRhiScissorRect scissor{};
                scissor.mX      = viewRect.X;
                scissor.mY      = viewRect.Y;
                scissor.mWidth  = viewRect.Width;
                scissor.mHeight = viewRect.Height;
                ctx.RHISetScissor(scissor);

                ctx.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
                ctx.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);
                ctx.RHIDraw(3U, 1U, 0U, 0U);
            });
    }

    void FBasicDeferredRenderer::FinalizeRendering() {}
} // namespace AltinaEngine::Rendering

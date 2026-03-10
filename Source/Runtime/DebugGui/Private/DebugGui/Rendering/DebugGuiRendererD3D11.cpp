#include "DebugGui/Rendering/DebugGuiRendererD3D11.h"

#include "CoreMinimal.h"

#include "Container/String.h"
#include "Container/Vector.h"
#include "Logging/Log.h"
#include "Platform/PlatformFileSystem.h"
#include "Platform/Generic/GenericPlatformDecl.h"

#include "Rhi/Command/RhiCmdContextAdapter.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiViewport.h"

#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Shader/ShaderTypes.h"

#include "Utility/Filesystem/Path.h"
#include "Utility/Filesystem/PathUtils.h"

namespace AltinaEngine::DebugGui::Private {
    namespace {
        using Container::FStringView;
        using Container::TVector;
        using Core::Logging::LogError;
        using Core::Logging::LogInfo;
        using Core::Utility::Filesystem::FPath;

        void UploadDynamicBuffer(Rhi::FRhiBuffer* buffer, const void* data, u64 sizeBytes) {
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

        [[nodiscard]] auto BuildIncludeDir(const FPath& shaderPath) -> FPath {
            auto includeDir = shaderPath.ParentPath().ParentPath().ParentPath();
            if (!includeDir.IsEmpty()) {
                return includeDir;
            }
            return shaderPath.ParentPath();
        }

        [[nodiscard]] auto FindDebugGuiShaderPath() -> FPath {
            constexpr FStringView kAssetsRelPath = TEXT("Assets/Shader/Debug/DebugGui.hlsl");
            constexpr FStringView kLegacyRelPath = TEXT("Shader/Debug/DebugGui.hlsl");
            constexpr FStringView kSourceRelPath = TEXT("Source/Shader/Debug/DebugGui.hlsl");

            auto                  TrySourceInParents = [&](const FPath& base) -> FPath {
                if (base.IsEmpty()) {
                    return {};
                }

                FPath probe = base;
                for (u32 i = 0U; i < 8U && !probe.IsEmpty(); ++i) {
                    const auto pSource = probe / kSourceRelPath;
                    if (pSource.Exists()) {
                        return pSource;
                    }
                    const auto parent = probe.ParentPath();
                    if (parent == probe) {
                        break;
                    }
                    probe = parent;
                }
                return {};
            };

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto pSource = cwd / kSourceRelPath;
                if (pSource.Exists()) {
                    return pSource;
                }
                const auto sourceInParents = TrySourceInParents(cwd);
                if (!sourceInParents.IsEmpty()) {
                    return sourceInParents;
                }
            }

            const FPath exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto sourceInParents = TrySourceInParents(exeDir);
                if (!sourceInParents.IsEmpty()) {
                    return sourceInParents;
                }

                const auto pAssets = exeDir / kAssetsRelPath;
                if (pAssets.Exists()) {
                    return pAssets;
                }
                const auto pLegacy = exeDir / kLegacyRelPath;
                if (pLegacy.Exists()) {
                    return pLegacy;
                }
                const auto exeParent = exeDir.ParentPath();
                if (!exeParent.IsEmpty() && exeParent != exeDir) {
                    const auto ppAssets = exeParent / kAssetsRelPath;
                    if (ppAssets.Exists()) {
                        return ppAssets;
                    }
                    const auto ppLegacy = exeParent / kLegacyRelPath;
                    if (ppLegacy.Exists()) {
                        return ppLegacy;
                    }
                }
            }

            if (!cwd.IsEmpty()) {
                const auto pAssets = cwd / kAssetsRelPath;
                if (pAssets.Exists()) {
                    return pAssets;
                }
                const auto pLegacy = cwd / kLegacyRelPath;
                if (pLegacy.Exists()) {
                    return pLegacy;
                }
            }

            return {};
        }
    } // namespace

    void FDebugGuiRendererD3D11::SetExternalTextures(const FImageTextureMap& textures) {
        mExternalTextures = textures;
        PruneExternalTextureCache();
    }

    void FDebugGuiRendererD3D11::Render(Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport,
        const FDrawData& drawData, const FFontAtlas& atlas) {
        auto* backBuffer = viewport.GetBackBuffer();
        if (backBuffer == nullptr) {
            return;
        }

        Rhi::FRhiCommandContextDesc ctxDesc{};
        ctxDesc.mQueueType = Rhi::ERhiQueueType::Graphics;
        ctxDesc.mDebugName.Assign(TEXT("DebugGui.Overlay"));
        auto commandContext = device.CreateCommandContext(ctxDesc);
        if (!commandContext) {
            return;
        }

        auto* ops = dynamic_cast<Rhi::IRhiCmdContextOps*>(commandContext.Get());
        if (ops == nullptr) {
            return;
        }

        Rhi::FRhiCmdContextAdapter ctx(*commandContext.Get(), *ops);

        const bool hasDrawData = !drawData.mCmds.IsEmpty() && !drawData.mVertices.IsEmpty()
            && !drawData.mIndices.IsEmpty();
        if (!hasDrawData) {
            Rhi::FRhiTransitionInfo toPresent{};
            toPresent.mResource = backBuffer;
            toPresent.mBefore   = Rhi::ERhiResourceState::RenderTarget;
            toPresent.mAfter    = Rhi::ERhiResourceState::Present;
            Rhi::FRhiTransitionCreateInfo toPresentBatch{};
            toPresentBatch.mTransitions     = &toPresent;
            toPresentBatch.mTransitionCount = 1U;
            toPresentBatch.mSrcQueue        = Rhi::ERhiQueueType::Graphics;
            toPresentBatch.mDstQueue        = Rhi::ERhiQueueType::Graphics;
            ctx.RHIBeginTransition(toPresentBatch);
            commandContext->RHIFlushContextDevice({});
            return;
        }

        if (!EnsureResources(device, atlas)) {
            return;
        }

        if (!EnsureBackBufferRtv(device, backBuffer)) {
            return;
        }

        if (!EnsureGeometryBuffers(device, drawData)) {
            return;
        }

        const auto& bbDesc = backBuffer->GetDesc();
        const u32   w      = bbDesc.mWidth;
        const u32   h      = bbDesc.mHeight;

        UpdateConstants(w, h);

        ctx.RHISetGraphicsPipeline(mPipeline.Get());
        ctx.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);

        Rhi::FRhiViewportRect vp{};
        vp.mX      = 0.0f;
        vp.mY      = 0.0f;
        vp.mWidth  = static_cast<f32>(w);
        vp.mHeight = static_cast<f32>(h);
        ctx.RHISetViewport(vp);

        Rhi::FRhiTransitionInfo toRenderTarget{};
        toRenderTarget.mResource = backBuffer;
        toRenderTarget.mBefore   = Rhi::ERhiResourceState::Present;
        toRenderTarget.mAfter    = Rhi::ERhiResourceState::RenderTarget;
        Rhi::FRhiTransitionCreateInfo toRenderTargetBatch{};
        toRenderTargetBatch.mTransitions     = &toRenderTarget;
        toRenderTargetBatch.mTransitionCount = 1U;
        toRenderTargetBatch.mSrcQueue        = Rhi::ERhiQueueType::Graphics;
        toRenderTargetBatch.mDstQueue        = Rhi::ERhiQueueType::Graphics;
        ctx.RHIBeginTransition(toRenderTargetBatch);

        Rhi::FRhiRenderPassColorAttachment colorAtt[3]{};
        colorAtt[0].mView        = mBackBufferRtv.Get();
        colorAtt[0].mLoadOp      = Rhi::ERhiLoadOp::Load;
        colorAtt[0].mStoreOp     = Rhi::ERhiStoreOp::Store;
        u32 colorAttachmentCount = 1U;
        if (Rhi::RHIGetBackend() == Rhi::ERhiBackend::Vulkan) {
            if (!EnsureAuxiliaryRtvs(device, w, h, bbDesc.mFormat)) {
                return;
            }
            colorAtt[1].mView    = mAuxColorRtv1.Get();
            colorAtt[1].mLoadOp  = Rhi::ERhiLoadOp::DontCare;
            colorAtt[1].mStoreOp = Rhi::ERhiStoreOp::DontCare;
            colorAtt[2].mView    = mAuxColorRtv2.Get();
            colorAtt[2].mLoadOp  = Rhi::ERhiLoadOp::DontCare;
            colorAtt[2].mStoreOp = Rhi::ERhiStoreOp::DontCare;
            colorAttachmentCount = 3U;
        }
        Rhi::FRhiRenderPassDesc rp{};
        rp.mDebugName.Assign(TEXT("DebugGui.RenderPass"));
        rp.mColorAttachmentCount = colorAttachmentCount;
        rp.mColorAttachments     = colorAtt;
        ctx.RHIBeginRenderPass(rp);

        Rhi::FRhiVertexBufferView vb{};
        vb.mBuffer      = mVertexBuffer.Get();
        vb.mStrideBytes = sizeof(FDrawVertex);
        vb.mOffsetBytes = 0U;
        ctx.RHISetVertexBuffer(0U, vb);

        Rhi::FRhiIndexBufferView ib{};
        ib.mBuffer      = mIndexBuffer.Get();
        ib.mIndexType   = Rhi::ERhiIndexType::Uint32;
        ib.mOffsetBytes = 0U;
        ctx.RHISetIndexBuffer(ib);

        Rhi::FRhiBindGroup* currentBindGroup = nullptr;

        u32                 idxOffset = 0U;
        for (const auto& cmd : drawData.mCmds) {
            Rhi::FRhiBindGroupRef imageBindGroup;
            if (cmd.mTextureId == 0ULL) {
                imageBindGroup = mBindGroup;
            } else {
                auto* texturePtr = mExternalTextures.Find(cmd.mTextureId);
                if (texturePtr == nullptr || *texturePtr == nullptr) {
                    idxOffset += cmd.mIndexCount;
                    continue;
                }
                if (!EnsureBindGroupForTexture(
                        device, cmd.mTextureId, *texturePtr, imageBindGroup)) {
                    idxOffset += cmd.mIndexCount;
                    continue;
                }
            }

            if (!imageBindGroup) {
                idxOffset += cmd.mIndexCount;
                continue;
            }
            if (currentBindGroup != imageBindGroup.Get()) {
                currentBindGroup = imageBindGroup.Get();
                ctx.RHISetBindGroup(0U, currentBindGroup, nullptr, 0U);
            }

            const i32 sx = static_cast<i32>(cmd.mClipRect.Min.X());
            const i32 sy = static_cast<i32>(cmd.mClipRect.Min.Y());
            const i32 ex = static_cast<i32>(cmd.mClipRect.Max.X());
            const i32 ey = static_cast<i32>(cmd.mClipRect.Max.Y());
            const u32 sw = (ex > sx) ? static_cast<u32>(ex - sx) : 0U;
            const u32 sh = (ey > sy) ? static_cast<u32>(ey - sy) : 0U;
            if (sw == 0U || sh == 0U) {
                idxOffset += cmd.mIndexCount;
                continue;
            }

            Rhi::FRhiScissorRect sc{};
            sc.mX      = sx;
            sc.mY      = sy;
            sc.mWidth  = sw;
            sc.mHeight = sh;
            ctx.RHISetScissor(sc);
            ctx.RHIDrawIndexed(cmd.mIndexCount, 1U, idxOffset, 0, 0U);
            idxOffset += cmd.mIndexCount;
        }

        ctx.RHIEndRenderPass();

        Rhi::FRhiTransitionInfo toPresent{};
        toPresent.mResource = backBuffer;
        toPresent.mBefore   = Rhi::ERhiResourceState::RenderTarget;
        toPresent.mAfter    = Rhi::ERhiResourceState::Present;
        Rhi::FRhiTransitionCreateInfo toPresentBatch{};
        toPresentBatch.mTransitions     = &toPresent;
        toPresentBatch.mTransitionCount = 1U;
        toPresentBatch.mSrcQueue        = Rhi::ERhiQueueType::Graphics;
        toPresentBatch.mDstQueue        = Rhi::ERhiQueueType::Graphics;
        ctx.RHIBeginTransition(toPresentBatch);

        commandContext->RHIFlushContextDevice({});
    }
    bool FDebugGuiRendererD3D11::EnsureResources(Rhi::FRhiDevice& device, const FFontAtlas& atlas) {
        if (!mVertexShader || !mPixelShader) {
            if (!CompileShaders(device)) {
                return false;
            }
        }

        if (!mFontTexture) {
            Rhi::FRhiTextureDesc desc{};
            desc.mDebugName.Assign(TEXT("DebugGui.FontAtlas"));
            desc.mWidth       = FFontAtlas::kAtlasW;
            desc.mHeight      = FFontAtlas::kAtlasH;
            desc.mDepth       = 1U;
            desc.mMipLevels   = 1U;
            desc.mArrayLayers = 1U;
            desc.mFormat      = Rhi::ERhiFormat::R8G8B8A8Unorm;
            desc.mBindFlags   = Rhi::ERhiTextureBindFlags::ShaderResource;
            mFontTexture      = device.CreateTexture(desc);
            if (!mFontTexture) {
                LogError(TEXT("DebugGui: Failed to create font texture."));
                return false;
            }

            Rhi::FRhiTextureSubresource sub{};
            const u32                   rowPitch   = FFontAtlas::kAtlasW * 4U;
            const u32                   slicePitch = rowPitch * FFontAtlas::kAtlasH;
            device.UpdateTextureSubresource(
                mFontTexture.Get(), sub, atlas.mPixels.Data(), rowPitch, slicePitch);

            Rhi::FRhiShaderResourceViewDesc srv{};
            srv.mDebugName.Assign(TEXT("DebugGui.FontAtlas.SRV"));
            srv.mTexture = mFontTexture.Get();
            srv.mFormat  = desc.mFormat;
            mFontSrv     = device.CreateShaderResourceView(srv);
            if (!mFontSrv) {
                LogError(TEXT("DebugGui: Failed to create font SRV."));
                return false;
            }
        }

        if (!mSampler) {
            Rhi::FRhiSamplerDesc s{};
            s.mDebugName.Assign(TEXT("DebugGui.Sampler"));
            // Debug text is frequently rendered at small sizes; use point sampling to avoid
            // blur.
            s.mFilter = Rhi::ERhiSamplerFilter::Nearest;
            mSampler  = device.CreateSampler(s);
            if (!mSampler) {
                LogError(TEXT("DebugGui: Failed to create sampler."));
                return false;
            }
        }

        if (!mLayout) {
            Rhi::FRhiBindGroupLayoutDesc layout{};
            layout.mDebugName.Assign(TEXT("DebugGui.Layout"));
            TVector<Rhi::FRhiShader*> shaders;
            shaders.PushBack(mVertexShader.Get());
            shaders.PushBack(mPixelShader.Get());
            if (!RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaders(shaders, 0U, layout)) {
                LogError(
                    TEXT("DebugGui: Failed to build bind-group layout from shader reflection."));
                return false;
            }
            mLayout = device.CreateBindGroupLayout(layout);
            if (!mLayout) {
                LogError(TEXT("DebugGui: Failed to create bind group layout."));
                return false;
            }
            if (!RenderCore::ShaderBinding::BuildBindingLookupTableFromShaders(
                    shaders, layout.mSetIndex, mLayout.Get(), mBindingLookupTable)) {
                LogError(TEXT("DebugGui: Failed to build binding lookup table."));
                return false;
            }

            if (!RenderCore::ShaderBinding::FindBindingByNameHash(mBindingLookupTable,
                    RenderCore::ShaderBinding::HashBindingName(TEXT("DebugGuiConstants")),
                    Rhi::ERhiBindingType::ConstantBuffer, mConstantsBinding)
                || !RenderCore::ShaderBinding::FindBindingByNameHash(mBindingLookupTable,
                    RenderCore::ShaderBinding::HashBindingName(TEXT("gFontAtlas")),
                    Rhi::ERhiBindingType::SampledTexture, mTextureBinding)
                || !RenderCore::ShaderBinding::FindBindingByNameHash(mBindingLookupTable,
                    RenderCore::ShaderBinding::HashBindingName(TEXT("gSampler")),
                    Rhi::ERhiBindingType::Sampler, mSamplerBinding)) {
                LogError(TEXT("DebugGui: Failed to resolve bind-group bindings from reflection."));
                return false;
            }
        }

        if (!mPipelineLayout) {
            Rhi::FRhiPipelineLayoutDesc p{};
            p.mDebugName.Assign(TEXT("DebugGui.PipelineLayout"));
            p.mBindGroupLayouts.PushBack(mLayout.Get());
            mPipelineLayout = device.CreatePipelineLayout(p);
            if (!mPipelineLayout) {
                LogError(TEXT("DebugGui: Failed to create pipeline layout."));
                return false;
            }
        }

        if (!mConstantsBuffer) {
            Rhi::FRhiBufferDesc cb{};
            cb.mDebugName.Assign(TEXT("DebugGui.Constants"));
            cb.mSizeBytes    = sizeof(FConstants);
            cb.mUsage        = Rhi::ERhiResourceUsage::Dynamic;
            cb.mBindFlags    = Rhi::ERhiBufferBindFlags::Constant;
            cb.mCpuAccess    = Rhi::ERhiCpuAccess::Write;
            mConstantsBuffer = device.CreateBuffer(cb);
            if (!mConstantsBuffer) {
                LogError(TEXT("DebugGui: Failed to create constants buffer."));
                return false;
            }
        }

        if (!mBindGroup) {
            if (mConstantsBinding == RenderCore::ShaderBinding::kInvalidBinding
                || mTextureBinding == RenderCore::ShaderBinding::kInvalidBinding
                || mSamplerBinding == RenderCore::ShaderBinding::kInvalidBinding) {
                LogError(TEXT("DebugGui: Invalid bind-group binding lookup state."));
                return false;
            }

            RenderCore::ShaderBinding::FBindGroupBuilder builder(mLayout.Get());
            if (!builder.AddBuffer(mConstantsBinding, mConstantsBuffer.Get(), 0ULL,
                    static_cast<u64>(sizeof(FConstants)))) {
                LogError(
                    TEXT("DebugGui: Failed to add constant-buffer bind-group entry (binding={})."),
                    mConstantsBinding);
                return false;
            }
            if (!builder.AddTexture(mTextureBinding, mFontTexture.Get())) {
                LogError(
                    TEXT("DebugGui: Failed to add font-texture bind-group entry (binding={})."),
                    mTextureBinding);
                return false;
            }
            if (!builder.AddSampler(mSamplerBinding, mSampler.Get())) {
                LogError(TEXT("DebugGui: Failed to add sampler bind-group entry (binding={})."),
                    mSamplerBinding);
                return false;
            }

            Rhi::FRhiBindGroupDesc bg{};
            bg.mDebugName.Assign(TEXT("DebugGui.BindGroup"));
            if (!builder.Build(bg)) {
                LogError(
                    TEXT("DebugGui: Failed to build bind group desc (layout/entry mismatch)."));
                return false;
            }
            mBindGroup = device.CreateBindGroup(bg);
            if (!mBindGroup) {
                LogError(TEXT("DebugGui: Failed to create bind group."));
                return false;
            }
        }

        if (!mPipeline) {
            Rhi::FRhiGraphicsPipelineDesc gp{};
            gp.mDebugName.Assign(TEXT("DebugGui.PSO"));
            gp.mPipelineLayout = mPipelineLayout.Get();
            gp.mVertexShader   = mVertexShader.Get();
            gp.mPixelShader    = mPixelShader.Get();

            {
                Rhi::FRhiVertexAttributeDesc a{};
                a.mSemanticName.Assign(TEXT("POSITION"));
                a.mFormat            = Rhi::ERhiFormat::R32G32Float;
                a.mInputSlot         = 0U;
                a.mAlignedByteOffset = 0U;
                gp.mVertexLayout.mAttributes.PushBack(a);
            }
            {
                Rhi::FRhiVertexAttributeDesc a{};
                a.mSemanticName.Assign(TEXT("TEXCOORD"));
                a.mFormat            = Rhi::ERhiFormat::R32G32Float;
                a.mInputSlot         = 0U;
                a.mAlignedByteOffset = 8U;
                gp.mVertexLayout.mAttributes.PushBack(a);
            }
            {
                Rhi::FRhiVertexAttributeDesc a{};
                a.mSemanticName.Assign(TEXT("COLOR"));
                a.mFormat            = Rhi::ERhiFormat::R8G8B8A8Unorm;
                a.mInputSlot         = 0U;
                a.mAlignedByteOffset = 16U;
                gp.mVertexLayout.mAttributes.PushBack(a);
            }

            gp.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
            gp.mDepthState.mDepthEnable = false;
            gp.mDepthState.mDepthWrite  = false;
            gp.mBlendState.mBlendEnable = true;
            gp.mBlendState.mSrcColor    = Rhi::ERhiBlendFactor::SrcAlpha;
            gp.mBlendState.mDstColor    = Rhi::ERhiBlendFactor::InvSrcAlpha;
            gp.mBlendState.mColorOp     = Rhi::ERhiBlendOp::Add;
            gp.mBlendState.mSrcAlpha    = Rhi::ERhiBlendFactor::One;
            gp.mBlendState.mDstAlpha    = Rhi::ERhiBlendFactor::InvSrcAlpha;
            gp.mBlendState.mAlphaOp     = Rhi::ERhiBlendOp::Add;

            mPipeline = device.CreateGraphicsPipeline(gp);
            if (!mPipeline) {
                LogError(TEXT("DebugGui: Failed to create graphics pipeline."));
                return false;
            }
        }

        return true;
    }

    bool FDebugGuiRendererD3D11::EnsureBindGroupForTexture(Rhi::FRhiDevice& device, u64 imageId,
        Rhi::FRhiTexture* texture, Rhi::FRhiBindGroupRef& out) {
        if (imageId == 0ULL || texture == nullptr || !mLayout || !mSampler || !mConstantsBuffer) {
            return false;
        }
        if (mConstantsBinding == RenderCore::ShaderBinding::kInvalidBinding
            || mTextureBinding == RenderCore::ShaderBinding::kInvalidBinding
            || mSamplerBinding == RenderCore::ShaderBinding::kInvalidBinding) {
            return false;
        }

        auto it = mExternalTextureCache.FindIt(imageId);
        if (it != mExternalTextureCache.end()) {
            if (it->second.Texture == texture && it->second.BindGroup) {
                out = it->second.BindGroup;
                return true;
            }
            mExternalTextureCache.Erase(it);
        }

        Rhi::FRhiShaderResourceViewDesc srvDesc{};
        srvDesc.mDebugName.Assign(TEXT("DebugGui.ExternalImage.SRV"));
        srvDesc.mTexture               = texture;
        srvDesc.mFormat                = texture->GetDesc().mFormat;
        srvDesc.mTextureRange.mBaseMip = 0U;
        srvDesc.mTextureRange.mMipCount =
            (texture->GetDesc().mMipLevels > 0U) ? texture->GetDesc().mMipLevels : 1U;
        srvDesc.mTextureRange.mBaseArrayLayer = 0U;
        srvDesc.mTextureRange.mLayerCount =
            (texture->GetDesc().mArrayLayers > 0U) ? texture->GetDesc().mArrayLayers : 1U;
        auto srv = device.CreateShaderResourceView(srvDesc);
        if (!srv) {
            return false;
        }

        RenderCore::ShaderBinding::FBindGroupBuilder builder(mLayout.Get());
        if (!builder.AddBuffer(mConstantsBinding, mConstantsBuffer.Get(), 0ULL,
                static_cast<u64>(sizeof(FConstants)))) {
            return false;
        }
        if (!builder.AddTexture(mTextureBinding, texture)) {
            return false;
        }
        if (!builder.AddSampler(mSamplerBinding, mSampler.Get())) {
            return false;
        }

        Rhi::FRhiBindGroupDesc bindGroupDesc{};
        bindGroupDesc.mDebugName.Assign(TEXT("DebugGui.ExternalImage.BindGroup"));
        if (!builder.Build(bindGroupDesc)) {
            return false;
        }

        auto bindGroup = device.CreateBindGroup(bindGroupDesc);
        if (!bindGroup) {
            return false;
        }

        FExternalTextureBinding entry{};
        entry.Texture                  = texture;
        entry.Srv                      = srv;
        entry.BindGroup                = bindGroup;
        mExternalTextureCache[imageId] = entry;
        out                            = bindGroup;
        return true;
    }

    void FDebugGuiRendererD3D11::PruneExternalTextureCache() {
        TVector<u64> removeKeys{};
        for (const auto& entry : mExternalTextureCache) {
            auto* texture = mExternalTextures.Find(entry.first);
            if (texture == nullptr || *texture != entry.second.Texture || *texture == nullptr) {
                removeKeys.PushBack(entry.first);
            }
        }

        for (const u64 key : removeKeys) {
            mExternalTextureCache.Erase(key);
        }
    }

    bool FDebugGuiRendererD3D11::EnsureBackBufferRtv(
        Rhi::FRhiDevice& device, Rhi::FRhiTexture* backBuffer) {
        if (mBackBufferTex == backBuffer && mBackBufferRtv) {
            return true;
        }
        mBackBufferTex = backBuffer;
        Rhi::FRhiRenderTargetViewDesc rtv{};
        rtv.mDebugName.Assign(TEXT("DebugGui.BackBuffer.RTV"));
        rtv.mTexture   = backBuffer;
        rtv.mFormat    = backBuffer->GetDesc().mFormat;
        mBackBufferRtv = device.CreateRenderTargetView(rtv);
        return static_cast<bool>(mBackBufferRtv);
    }

    bool FDebugGuiRendererD3D11::EnsureAuxiliaryRtvs(
        Rhi::FRhiDevice& device, u32 width, u32 height, Rhi::ERhiFormat format) {
        if (width == 0U || height == 0U || format == Rhi::ERhiFormat::Unknown) {
            return false;
        }
        const bool recreate =
            (mAuxWidth != width) || (mAuxHeight != height) || (mAuxFormat != format);
        if (recreate) {
            mAuxColorRtv1.Reset();
            mAuxColorRtv2.Reset();
            mAuxColorTex1.Reset();
            mAuxColorTex2.Reset();
            mAuxWidth  = width;
            mAuxHeight = height;
            mAuxFormat = format;
        }
        if (mAuxColorRtv1 && mAuxColorRtv2) {
            return true;
        }

        Rhi::FRhiTextureDesc texDesc{};
        texDesc.mDebugName.Assign(TEXT("DebugGui.AuxColor"));
        texDesc.mDimension   = Rhi::ERhiTextureDimension::Tex2D;
        texDesc.mWidth       = width;
        texDesc.mHeight      = height;
        texDesc.mDepth       = 1U;
        texDesc.mMipLevels   = 1U;
        texDesc.mArrayLayers = 1U;
        texDesc.mFormat      = format;
        texDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::RenderTarget;
        mAuxColorTex1        = device.CreateTexture(texDesc);
        mAuxColorTex2        = device.CreateTexture(texDesc);
        if (!mAuxColorTex1 || !mAuxColorTex2) {
            return false;
        }

        Rhi::FRhiRenderTargetViewDesc rtvDesc{};
        rtvDesc.mDebugName.Assign(TEXT("DebugGui.AuxColor.RTV"));
        rtvDesc.mFormat = format;

        rtvDesc.mTexture = mAuxColorTex1.Get();
        mAuxColorRtv1    = device.CreateRenderTargetView(rtvDesc);
        rtvDesc.mTexture = mAuxColorTex2.Get();
        mAuxColorRtv2    = device.CreateRenderTargetView(rtvDesc);
        return static_cast<bool>(mAuxColorRtv1) && static_cast<bool>(mAuxColorRtv2);
    }

    bool FDebugGuiRendererD3D11::EnsureGeometryBuffers(
        Rhi::FRhiDevice& device, const FDrawData& drawData) {
        const u64 vbBytes = static_cast<u64>(drawData.mVertices.Size()) * sizeof(FDrawVertex);
        const u64 ibBytes = static_cast<u64>(drawData.mIndices.Size()) * sizeof(u32);
        if (vbBytes == 0ULL || ibBytes == 0ULL) {
            return false;
        }

        if (!mVertexBuffer || mVertexBufferSize < vbBytes) {
            Rhi::FRhiBufferDesc d{};
            d.mDebugName.Assign(TEXT("DebugGui.VB"));
            d.mSizeBytes      = vbBytes;
            d.mUsage          = Rhi::ERhiResourceUsage::Dynamic;
            d.mBindFlags      = Rhi::ERhiBufferBindFlags::Vertex;
            d.mCpuAccess      = Rhi::ERhiCpuAccess::Write;
            mVertexBuffer     = device.CreateBuffer(d);
            mVertexBufferSize = vbBytes;
        }
        if (!mIndexBuffer || mIndexBufferSize < ibBytes) {
            Rhi::FRhiBufferDesc d{};
            d.mDebugName.Assign(TEXT("DebugGui.IB"));
            d.mSizeBytes     = ibBytes;
            d.mUsage         = Rhi::ERhiResourceUsage::Dynamic;
            d.mBindFlags     = Rhi::ERhiBufferBindFlags::Index;
            d.mCpuAccess     = Rhi::ERhiCpuAccess::Write;
            mIndexBuffer     = device.CreateBuffer(d);
            mIndexBufferSize = ibBytes;
        }
        if (!mVertexBuffer || !mIndexBuffer) {
            LogError(TEXT("DebugGui: Failed to allocate geometry buffers."));
            return false;
        }

        UploadDynamicBuffer(mVertexBuffer.Get(), drawData.mVertices.Data(), vbBytes);
        UploadDynamicBuffer(mIndexBuffer.Get(), drawData.mIndices.Data(), ibBytes);
        return true;
    }

    void FDebugGuiRendererD3D11::UpdateConstants(u32 w, u32 h) {
        if (!mConstantsBuffer || w == 0U || h == 0U) {
            return;
        }
        FConstants c{};
        c.ScaleX     = 2.0f / static_cast<f32>(w);
        c.ScaleY     = 2.0f / static_cast<f32>(h);
        c.TranslateX = -1.0f;
        c.TranslateY = -1.0f;
        UploadDynamicBuffer(mConstantsBuffer.Get(), &c, sizeof(FConstants));
    }

    bool FDebugGuiRendererD3D11::CompileShaders(Rhi::FRhiDevice& device) {
        using ShaderCompiler::EShaderOptimization;
        using ShaderCompiler::EShaderSourceLanguage;
        using ShaderCompiler::FShaderCompileRequest;
        using ShaderCompiler::FShaderCompileResult;
        using ShaderCompiler::GetShaderCompiler;

        const auto shaderPath = FindDebugGuiShaderPath();
        if (shaderPath.IsEmpty() || !shaderPath.Exists()) {
            LogError(
                TEXT("DebugGui shader source not found: '{}'"), shaderPath.GetString().ToView());
            return false;
        }
        LogInfo(TEXT("DebugGui shader source: '{}'"), shaderPath.GetString().ToView());

        auto CompileStage = [&](FStringView entry, Shader::EShaderStage stage,
                                Rhi::FRhiShaderRef& out) -> bool {
            FShaderCompileRequest request{};
            request.mSource.mPath.Assign(shaderPath.GetString().ToView());
            request.mSource.mEntryPoint.Assign(entry);
            request.mSource.mStage    = stage;
            request.mSource.mLanguage = EShaderSourceLanguage::Hlsl;

            const auto includeDir = BuildIncludeDir(shaderPath);
            if (!includeDir.IsEmpty()) {
                request.mSource.mIncludeDirs.PushBack(includeDir.GetString());
            }

            const auto backend = Rhi::RHIGetBackend();
            request.mOptions.mTargetBackend =
                (backend != Rhi::ERhiBackend::Unknown) ? backend : Rhi::ERhiBackend::DirectX11;
            request.mOptions.mOptimization = EShaderOptimization::Default;
            request.mOptions.mDebugInfo    = false;

            FShaderCompileResult result = GetShaderCompiler().Compile(request);
            if (!result.mSucceeded) {
                LogError(TEXT("DebugGui shader compile failed: entry='{}' stage={} diag={}"), entry,
                    static_cast<u32>(stage), result.mDiagnostics.ToView());
                return false;
            }

            auto shaderDesc = ShaderCompiler::BuildRhiShaderDesc(result);
            shaderDesc.mDebugName.Assign(entry);
            out = device.CreateShader(shaderDesc);
            if (!out) {
                LogError(TEXT("DebugGui: Failed to create RHI shader: '{}'"), entry);
                return false;
            }
            return true;
        };

        if (!CompileStage(TEXT("DebugGuiVSMain"), Shader::EShaderStage::Vertex, mVertexShader)
            || !CompileStage(TEXT("DebugGuiPSMain"), Shader::EShaderStage::Pixel, mPixelShader)) {
            return false;
        }
        return true;
    }

} // namespace AltinaEngine::DebugGui::Private

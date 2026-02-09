#include "RhiD3D11/RhiD3D11Device.h"
#include "RhiD3D11/RhiD3D11CommandContext.h"
#include "RhiD3D11/RhiD3D11CommandList.h"
#include "RhiD3D11/RhiD3D11Pipeline.h"
#include "RhiD3D11/RhiD3D11Resources.h"
#include "RhiD3D11/RhiD3D11Shader.h"
#include "RhiD3D11/RhiD3D11StagingBufferManager.h"
#include "RhiD3D11/RhiD3D11UploadBufferManager.h"
#include "RhiD3D11/RhiD3D11Viewport.h"

#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiSemaphore.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiViewport.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

#if AE_PLATFORM_WIN
    #ifdef TEXT
        #undef TEXT
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
    #ifdef CreateSemaphore
        #undef CreateSemaphore
    #endif
    #include <d3d11.h>
    #include <d3d11_1.h>
    #include <wrl/client.h>
#endif

#include <string>
#include <vector>
#include <type_traits>
#include <limits>

namespace AltinaEngine::Rhi {
    using Core::Container::TVector;
#if AE_PLATFORM_WIN
    using Microsoft::WRL::ComPtr;

    struct FRhiD3D11Device::FState {
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11DeviceContext> mImmediateContext;
        D3D_FEATURE_LEVEL           mFeatureLevel = D3D_FEATURE_LEVEL_11_0;
        FD3D11UploadBufferManager   mUploadManager;
        FD3D11StagingBufferManager  mStagingManager;
        u64                         mFrameIndex = 0ULL;
        u64                         mCompletedSerial = 0ULL;
        u32                         mFrameQueryIndex = 0U;
        TVector<ComPtr<ID3D11Query>> mFrameQueries;
        TVector<u64>                mFrameQuerySerials;
    };

    struct FRhiD3D11CommandList::FState {
        ComPtr<ID3D11CommandList> mCommandList;
    };

    struct FRhiD3D11CommandContext::FState {
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11DeviceContext> mDeferredContext;
        ComPtr<ID3D11DeviceContext1> mDeferredContext1;
        FRhiD3D11GraphicsPipeline*  mCurrentGraphicsPipeline = nullptr;
        FRhiD3D11ComputePipeline*   mCurrentComputePipeline = nullptr;
        bool                        mUseComputeBindings = false;
        ID3D11RenderTargetView*     mCurrentRtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
        UINT                        mCurrentRtvCount = 0U;
        ID3D11DepthStencilView*     mCurrentDsv = nullptr;
    };
#else
    struct FRhiD3D11Device::FState {};
    struct FRhiD3D11CommandList::FState {};
    struct FRhiD3D11CommandContext::FState {};
#endif

    FRhiD3D11CommandList::FRhiD3D11CommandList(const FRhiCommandListDesc& desc)
        : FRhiCommandList(desc) {
        mState = new FState{};
    }

    FRhiD3D11CommandList::~FRhiD3D11CommandList() {
        delete mState;
        mState = nullptr;
    }

    auto FRhiD3D11CommandList::GetNativeCommandList() const noexcept -> ID3D11CommandList* {
#if AE_PLATFORM_WIN
        return (mState != nullptr) ? mState->mCommandList.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    void FRhiD3D11CommandList::SetNativeCommandList(ID3D11CommandList* list) {
#if AE_PLATFORM_WIN
        if (mState == nullptr) {
            return;
        }
        mState->mCommandList.Reset();
        if (list != nullptr) {
            mState->mCommandList.Attach(list);
        }
#else
        (void)list;
#endif
    }

    void FRhiD3D11CommandList::Reset(FRhiCommandPool* /*pool*/) {
#if AE_PLATFORM_WIN
        if (mState != nullptr) {
            mState->mCommandList.Reset();
        }
#endif
    }

    void FRhiD3D11CommandList::Close() {}

    FRhiD3D11CommandContext::FRhiD3D11CommandContext(
        const FRhiCommandContextDesc& desc, ID3D11Device* device, FRhiCommandListRef commandList)
        : FRhiCommandContext(desc), mCommandList(AltinaEngine::Move(commandList)) {
        mState = new FState{};
#if AE_PLATFORM_WIN
        if (mState && (device != nullptr)) {
            mState->mDevice = device;
        }
#else
        (void)device;
#endif
    }

    FRhiD3D11CommandContext::~FRhiD3D11CommandContext() {
        delete mState;
        mState = nullptr;
    }

    void FRhiD3D11CommandContext::Begin() {
#if AE_PLATFORM_WIN
        if (!mState || !mState->mDevice) {
            return;
        }

        if (!mState->mDeferredContext) {
            ComPtr<ID3D11DeviceContext> deferred;
            if (SUCCEEDED(mState->mDevice->CreateDeferredContext(0, &deferred))) {
                mState->mDeferredContext = AltinaEngine::Move(deferred);
            }
        }

        if (mState->mDeferredContext) {
            mState->mDeferredContext->ClearState();
            mState->mDeferredContext1.Reset();
            mState->mDeferredContext.As(&mState->mDeferredContext1);
        }

        mState->mCurrentGraphicsPipeline = nullptr;
        mState->mCurrentComputePipeline  = nullptr;
        mState->mUseComputeBindings      = false;
        mState->mCurrentRtvCount         = 0U;
        mState->mCurrentDsv              = nullptr;
        for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
            mState->mCurrentRtvs[i] = nullptr;
        }

        auto* commandList = static_cast<FRhiD3D11CommandList*>(mCommandList.Get());
        if (commandList) {
            commandList->Reset(nullptr);
        }
#endif
    }

    void FRhiD3D11CommandContext::End() {
#if AE_PLATFORM_WIN
        if (!mState || !mState->mDeferredContext) {
            return;
        }

        ComPtr<ID3D11CommandList> commandList;
        const HRESULT hr = mState->mDeferredContext->FinishCommandList(TRUE, &commandList);
        if (FAILED(hr)) {
            return;
        }

        auto* rhiCommandList = static_cast<FRhiD3D11CommandList*>(mCommandList.Get());
        if (rhiCommandList) {
            rhiCommandList->SetNativeCommandList(commandList.Detach());
        }
#endif
    }

    auto FRhiD3D11CommandContext::GetCommandList() const noexcept -> FRhiCommandList* {
        return mCommandList.Get();
    }

    namespace {
#if AE_PLATFORM_WIN
        auto IsComputeStage(EShaderStage stage) noexcept -> bool {
            return stage == EShaderStage::Compute;
        }

        void BindConstantBuffer(ID3D11DeviceContext* context, EShaderStage stage, UINT slot,
            ID3D11Buffer* buffer) {
            switch (stage) {
                case EShaderStage::Vertex:
                    context->VSSetConstantBuffers(slot, 1, &buffer);
                    break;
                case EShaderStage::Pixel:
                    context->PSSetConstantBuffers(slot, 1, &buffer);
                    break;
                case EShaderStage::Geometry:
                    context->GSSetConstantBuffers(slot, 1, &buffer);
                    break;
                case EShaderStage::Hull:
                    context->HSSetConstantBuffers(slot, 1, &buffer);
                    break;
                case EShaderStage::Domain:
                    context->DSSetConstantBuffers(slot, 1, &buffer);
                    break;
                case EShaderStage::Compute:
                    context->CSSetConstantBuffers(slot, 1, &buffer);
                    break;
                default:
                    break;
            }
        }

        void BindConstantBufferWithOffset(ID3D11DeviceContext1* context, EShaderStage stage,
            UINT slot, ID3D11Buffer* buffer, UINT firstConstant, UINT numConstants) {
            switch (stage) {
                case EShaderStage::Vertex:
                    context->VSSetConstantBuffers1(slot, 1, &buffer, &firstConstant, &numConstants);
                    break;
                case EShaderStage::Pixel:
                    context->PSSetConstantBuffers1(slot, 1, &buffer, &firstConstant, &numConstants);
                    break;
                case EShaderStage::Geometry:
                    context->GSSetConstantBuffers1(slot, 1, &buffer, &firstConstant, &numConstants);
                    break;
                case EShaderStage::Hull:
                    context->HSSetConstantBuffers1(slot, 1, &buffer, &firstConstant, &numConstants);
                    break;
                case EShaderStage::Domain:
                    context->DSSetConstantBuffers1(slot, 1, &buffer, &firstConstant, &numConstants);
                    break;
                case EShaderStage::Compute:
                    context->CSSetConstantBuffers1(slot, 1, &buffer, &firstConstant, &numConstants);
                    break;
                default:
                    break;
            }
        }

        void BindShaderResource(ID3D11DeviceContext* context, EShaderStage stage, UINT slot,
            ID3D11ShaderResourceView* view) {
            switch (stage) {
                case EShaderStage::Vertex:
                    context->VSSetShaderResources(slot, 1, &view);
                    break;
                case EShaderStage::Pixel:
                    context->PSSetShaderResources(slot, 1, &view);
                    break;
                case EShaderStage::Geometry:
                    context->GSSetShaderResources(slot, 1, &view);
                    break;
                case EShaderStage::Hull:
                    context->HSSetShaderResources(slot, 1, &view);
                    break;
                case EShaderStage::Domain:
                    context->DSSetShaderResources(slot, 1, &view);
                    break;
                case EShaderStage::Compute:
                    context->CSSetShaderResources(slot, 1, &view);
                    break;
                default:
                    break;
            }
        }

        void BindSampler(ID3D11DeviceContext* context, EShaderStage stage, UINT slot,
            ID3D11SamplerState* sampler) {
            switch (stage) {
                case EShaderStage::Vertex:
                    context->VSSetSamplers(slot, 1, &sampler);
                    break;
                case EShaderStage::Pixel:
                    context->PSSetSamplers(slot, 1, &sampler);
                    break;
                case EShaderStage::Geometry:
                    context->GSSetSamplers(slot, 1, &sampler);
                    break;
                case EShaderStage::Hull:
                    context->HSSetSamplers(slot, 1, &sampler);
                    break;
                case EShaderStage::Domain:
                    context->DSSetSamplers(slot, 1, &sampler);
                    break;
                case EShaderStage::Compute:
                    context->CSSetSamplers(slot, 1, &sampler);
                    break;
                default:
                    break;
            }
        }

        void BindUnorderedAccess(ID3D11DeviceContext* context, EShaderStage stage, UINT slot,
            ID3D11UnorderedAccessView* view) {
            if (!IsComputeStage(stage)) {
                return;
            }
            context->CSSetUnorderedAccessViews(slot, 1, &view, nullptr);
        }

        void BindGraphicsUnorderedAccess(ID3D11DeviceContext* context,
            ID3D11RenderTargetView* const* rtvs, UINT rtvCount, ID3D11DepthStencilView* dsv,
            UINT slot, ID3D11UnorderedAccessView* view) {
            if (context == nullptr) {
                return;
            }
            if (slot >= D3D11_PS_CS_UAV_REGISTER_COUNT) {
                return;
            }
            if (slot < rtvCount) {
                return;
            }
            ID3D11UnorderedAccessView* uavs[1] = { view };
            context->OMSetRenderTargetsAndUnorderedAccessViews(
                rtvCount, (rtvCount > 0U) ? rtvs : nullptr, dsv, slot, 1, uavs, nullptr);
        }
#endif
    } // namespace

    void FRhiD3D11CommandContext::RHISetGraphicsPipeline(FRhiPipeline* pipeline) {
#if AE_PLATFORM_WIN
        ID3D11DeviceContext* context = GetDeferredContext();
        if (!context || !mState) {
            return;
        }

        FRhiD3D11GraphicsPipeline* graphicsPipeline = nullptr;
        if (pipeline != nullptr && pipeline->IsGraphics()) {
            graphicsPipeline = static_cast<FRhiD3D11GraphicsPipeline*>(pipeline);
        }

        mState->mCurrentGraphicsPipeline = graphicsPipeline;
        mState->mUseComputeBindings      = false;

        ID3D11InputLayout*  inputLayout = nullptr;
        ID3D11VertexShader* vertexShader = nullptr;
        ID3D11PixelShader*  pixelShader  = nullptr;
        ID3D11GeometryShader* geometryShader = nullptr;
        ID3D11HullShader*   hullShader   = nullptr;
        ID3D11DomainShader* domainShader = nullptr;

        if (graphicsPipeline != nullptr) {
            inputLayout = graphicsPipeline->GetInputLayout();
            const auto& desc = pipeline->GetGraphicsDesc();
            auto* vs = static_cast<FRhiD3D11Shader*>(desc.mVertexShader);
            auto* ps = static_cast<FRhiD3D11Shader*>(desc.mPixelShader);
            auto* gs = static_cast<FRhiD3D11Shader*>(desc.mGeometryShader);
            auto* hs = static_cast<FRhiD3D11Shader*>(desc.mHullShader);
            auto* ds = static_cast<FRhiD3D11Shader*>(desc.mDomainShader);

            vertexShader   = vs ? vs->GetVertexShader() : nullptr;
            pixelShader    = ps ? ps->GetPixelShader() : nullptr;
            geometryShader = gs ? gs->GetGeometryShader() : nullptr;
            hullShader     = hs ? hs->GetHullShader() : nullptr;
            domainShader   = ds ? ds->GetDomainShader() : nullptr;
        }

        context->IASetInputLayout(inputLayout);
        context->VSSetShader(vertexShader, nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);
        context->GSSetShader(geometryShader, nullptr, 0);
        context->HSSetShader(hullShader, nullptr, 0);
        context->DSSetShader(domainShader, nullptr, 0);
#else
        (void)pipeline;
#endif
    }

    void FRhiD3D11CommandContext::RHISetComputePipeline(FRhiPipeline* pipeline) {
#if AE_PLATFORM_WIN
        ID3D11DeviceContext* context = GetDeferredContext();
        if (!context || !mState) {
            return;
        }

        FRhiD3D11ComputePipeline* computePipeline = nullptr;
        if (pipeline != nullptr && !pipeline->IsGraphics()) {
            computePipeline = static_cast<FRhiD3D11ComputePipeline*>(pipeline);
        }

        mState->mCurrentComputePipeline = computePipeline;
        mState->mUseComputeBindings     = true;

        ID3D11ComputeShader* computeShader = nullptr;
        if (computePipeline != nullptr) {
            const auto& desc = pipeline->GetComputeDesc();
            auto* cs = static_cast<FRhiD3D11Shader*>(desc.mComputeShader);
            computeShader = cs ? cs->GetComputeShader() : nullptr;
        }
        context->CSSetShader(computeShader, nullptr, 0);
#else
        (void)pipeline;
#endif
    }

    void FRhiD3D11CommandContext::RHISetRenderTargets(u32 colorTargetCount,
        FRhiTexture* const* colorTargets, FRhiTexture* depthTarget) {
#if AE_PLATFORM_WIN
        ID3D11DeviceContext* context = GetDeferredContext();
        if (!context || !mState) {
            return;
        }

        const UINT maxTargets = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        const UINT rtvCount   = (colorTargetCount > maxTargets)
            ? maxTargets
            : static_cast<UINT>(colorTargetCount);

        ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
        for (UINT i = 0; i < rtvCount; ++i) {
            auto* texture = colorTargets ? static_cast<FRhiD3D11Texture*>(colorTargets[i]) : nullptr;
            rtvs[i] = texture ? texture->GetRenderTargetView() : nullptr;
        }

        ID3D11DepthStencilView* dsv = nullptr;
        if (depthTarget != nullptr) {
            auto* depthTexture = static_cast<FRhiD3D11Texture*>(depthTarget);
            dsv = depthTexture ? depthTexture->GetDepthStencilView() : nullptr;
        }

        context->OMSetRenderTargets(rtvCount, rtvCount ? rtvs : nullptr, dsv);

        mState->mCurrentRtvCount = rtvCount;
        mState->mCurrentDsv      = dsv;
        for (UINT i = 0; i < maxTargets; ++i) {
            mState->mCurrentRtvs[i] = (i < rtvCount) ? rtvs[i] : nullptr;
        }
#else
        (void)colorTargetCount;
        (void)colorTargets;
        (void)depthTarget;
#endif
    }

    void FRhiD3D11CommandContext::RHISetBindGroup(u32 setIndex, FRhiBindGroup* group,
        const u32* dynamicOffsets, u32 dynamicOffsetCount) {
#if AE_PLATFORM_WIN
        ID3D11DeviceContext* context = GetDeferredContext();
        if (!context || !mState || group == nullptr) {
            return;
        }

        const auto& groupDesc = group->GetDesc();
        if (groupDesc.mEntries.IsEmpty()) {
            return;
        }

        const FRhiBindGroupLayout* groupLayout  = groupDesc.mLayout;
        const auto*                layoutEntries =
            groupLayout ? &groupLayout->GetDesc().mEntries : nullptr;

        const FRhiD3D11GraphicsPipeline* graphicsPipeline = mState->mCurrentGraphicsPipeline;
        const FRhiD3D11ComputePipeline* computePipeline   = mState->mCurrentComputePipeline;

        auto getDynamicOffsetBytes = [&](const FRhiBindGroupEntry& entry,
                                         bool& hasDynamicOffset) -> u64 {
            hasDynamicOffset = false;
            if (!groupLayout || layoutEntries == nullptr || dynamicOffsets == nullptr
                || dynamicOffsetCount == 0U) {
                return 0ULL;
            }

            u32 dynIndex = 0U;
            for (const auto& layoutEntry : *layoutEntries) {
                if (!layoutEntry.mHasDynamicOffset) {
                    continue;
                }
                if (layoutEntry.mBinding == entry.mBinding && layoutEntry.mType == entry.mType) {
                    hasDynamicOffset = true;
                    if (dynIndex < dynamicOffsetCount) {
                        return static_cast<u64>(dynamicOffsets[dynIndex]);
                    }
                    return 0ULL;
                }
                ++dynIndex;
            }
            return 0ULL;
        };

        auto applyMappings = [&](const TVector<FD3D11BindingMappingEntry>& mappings) {
            for (const auto& entry : groupDesc.mEntries) {
                for (const auto& mapping : mappings) {
                    if (mapping.mSet != setIndex || mapping.mBinding != entry.mBinding) {
                        continue;
                    }
                    if (mapping.mType != entry.mType) {
                        continue;
                    }

                    const UINT slot = static_cast<UINT>(mapping.mRegister + entry.mArrayIndex);

                    switch (mapping.mType) {
                        case ERhiBindingType::ConstantBuffer:
                        {
                            auto* buffer = static_cast<FRhiD3D11Buffer*>(entry.mBuffer);
                            ID3D11Buffer* nativeBuffer =
                                buffer ? buffer->GetNativeBuffer() : nullptr;
                            ID3D11DeviceContext1* context1 = mState->mDeferredContext1.Get();
                            bool                 hasDynamicOffset = false;
                            const u64            dynamicOffsetBytes =
                                getDynamicOffsetBytes(entry, hasDynamicOffset);

                            const bool wantsRange =
                                hasDynamicOffset || entry.mOffset != 0ULL || entry.mSize != 0ULL;

                            if (context1 && wantsRange && buffer != nullptr) {
                                const u64 bufferSizeBytes = buffer->GetDesc().mSizeBytes;
                                u64       offsetBytes = entry.mOffset + dynamicOffsetBytes;
                                u64       sizeBytes   = entry.mSize;

                                if (sizeBytes == 0ULL) {
                                    sizeBytes = (offsetBytes <= bufferSizeBytes)
                                        ? (bufferSizeBytes - offsetBytes)
                                        : 0ULL;
                                }

                                const bool validRange =
                                    (offsetBytes <= bufferSizeBytes)
                                    && (sizeBytes != 0ULL)
                                    && (sizeBytes <= (bufferSizeBytes - offsetBytes))
                                    && (offsetBytes % 16ULL == 0ULL)
                                    && (sizeBytes % 16ULL == 0ULL);

                                if (validRange) {
                                    const u64 firstConstant64 = offsetBytes / 16ULL;
                                    const u64 numConstants64  = sizeBytes / 16ULL;
                                    const u64 maxUint =
                                        static_cast<u64>(std::numeric_limits<UINT>::max());
                                    if (firstConstant64 <= maxUint && numConstants64 <= maxUint) {
                                        const UINT firstConstant = static_cast<UINT>(firstConstant64);
                                        const UINT numConstants  = static_cast<UINT>(numConstants64);
                                        BindConstantBufferWithOffset(
                                            context1, mapping.mStage, slot, nativeBuffer,
                                            firstConstant, numConstants);
                                        break;
                                    }
                                }
                            }

                            BindConstantBuffer(context, mapping.mStage, slot, nativeBuffer);
                            break;
                        }
                        case ERhiBindingType::SampledTexture:
                        {
                            auto* texture = static_cast<FRhiD3D11Texture*>(entry.mTexture);
                            ID3D11ShaderResourceView* view =
                                texture ? texture->GetShaderResourceView() : nullptr;
                            BindShaderResource(context, mapping.mStage, slot, view);
                            break;
                        }
                        case ERhiBindingType::SampledBuffer:
                        {
                            auto* buffer = static_cast<FRhiD3D11Buffer*>(entry.mBuffer);
                            ID3D11ShaderResourceView* view =
                                buffer ? buffer->GetShaderResourceView() : nullptr;
                            BindShaderResource(context, mapping.mStage, slot, view);
                            break;
                        }
                        case ERhiBindingType::StorageTexture:
                        {
                            auto* texture = static_cast<FRhiD3D11Texture*>(entry.mTexture);
                            ID3D11UnorderedAccessView* view =
                                texture ? texture->GetUnorderedAccessView() : nullptr;
                            if (IsComputeStage(mapping.mStage)) {
                                BindUnorderedAccess(context, mapping.mStage, slot, view);
                            } else if (mapping.mStage == EShaderStage::Pixel) {
                                BindGraphicsUnorderedAccess(context, mState->mCurrentRtvs,
                                    mState->mCurrentRtvCount, mState->mCurrentDsv, slot, view);
                            }
                            break;
                        }
                        case ERhiBindingType::StorageBuffer:
                        {
                            auto* buffer = static_cast<FRhiD3D11Buffer*>(entry.mBuffer);
                            ID3D11UnorderedAccessView* view =
                                buffer ? buffer->GetUnorderedAccessView() : nullptr;
                            if (IsComputeStage(mapping.mStage)) {
                                BindUnorderedAccess(context, mapping.mStage, slot, view);
                            } else if (mapping.mStage == EShaderStage::Pixel) {
                                BindGraphicsUnorderedAccess(context, mState->mCurrentRtvs,
                                    mState->mCurrentRtvCount, mState->mCurrentDsv, slot, view);
                            }
                            break;
                        }
                        case ERhiBindingType::Sampler:
                        {
                            auto* sampler = static_cast<FRhiD3D11Sampler*>(entry.mSampler);
                            ID3D11SamplerState* nativeSampler =
                                sampler ? sampler->GetNativeSampler() : nullptr;
                            BindSampler(context, mapping.mStage, slot, nativeSampler);
                            break;
                        }
                        case ERhiBindingType::AccelerationStructure:
                        default:
                            break;
                    }
                }
            }
        };

        if (mState->mUseComputeBindings) {
            if (computePipeline) {
                applyMappings(computePipeline->GetBindingMappings());
            }
        } else {
            if (graphicsPipeline) {
                applyMappings(graphicsPipeline->GetBindingMappings());
            }
        }
#else
        (void)setIndex;
        (void)group;
        (void)dynamicOffsets;
        (void)dynamicOffsetCount;
#endif
    }

    void FRhiD3D11CommandContext::RHIDrawIndexed(
        u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset, u32 firstInstance) {
#if AE_PLATFORM_WIN
        ID3D11DeviceContext* context = GetDeferredContext();
        if (!context) {
            return;
        }

        const UINT idxCount      = static_cast<UINT>(indexCount);
        const UINT instCount     = static_cast<UINT>(instanceCount);
        const UINT startIdx      = static_cast<UINT>(firstIndex);
        const INT  baseVertex    = static_cast<INT>(vertexOffset);
        const UINT startInstance = static_cast<UINT>(firstInstance);

        if (instCount <= 1U && startInstance == 0U) {
            context->DrawIndexed(idxCount, startIdx, baseVertex);
        } else {
            context->DrawIndexedInstanced(idxCount, instCount, startIdx, baseVertex, startInstance);
        }
#else
        (void)indexCount;
        (void)instanceCount;
        (void)firstIndex;
        (void)vertexOffset;
        (void)firstInstance;
#endif
    }

    void FRhiD3D11CommandContext::RHIDispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) {
#if AE_PLATFORM_WIN
        ID3D11DeviceContext* context = GetDeferredContext();
        if (!context) {
            return;
        }
        context->Dispatch(static_cast<UINT>(groupCountX), static_cast<UINT>(groupCountY),
            static_cast<UINT>(groupCountZ));
#else
        (void)groupCountX;
        (void)groupCountY;
        (void)groupCountZ;
#endif
    }

    auto FRhiD3D11CommandContext::GetDeferredContext() const noexcept -> ID3D11DeviceContext* {
#if AE_PLATFORM_WIN
        return mState ? mState->mDeferredContext.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    namespace {
#if AE_PLATFORM_WIN
        using Core::Container::FString;
        using Core::Container::TVector;

        auto ToD3D11Format(ERhiFormat format) noexcept -> DXGI_FORMAT {
            switch (format) {
                case ERhiFormat::R8G8B8A8Unorm:
                    return DXGI_FORMAT_R8G8B8A8_UNORM;
                case ERhiFormat::R8G8B8A8UnormSrgb:
                    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                case ERhiFormat::B8G8R8A8Unorm:
                    return DXGI_FORMAT_B8G8R8A8_UNORM;
                case ERhiFormat::B8G8R8A8UnormSrgb:
                    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                case ERhiFormat::R16G16B16A16Float:
                    return DXGI_FORMAT_R16G16B16A16_FLOAT;
                case ERhiFormat::R32Float:
                    return DXGI_FORMAT_R32_FLOAT;
                case ERhiFormat::D24UnormS8Uint:
                    return DXGI_FORMAT_D24_UNORM_S8_UINT;
                case ERhiFormat::D32Float:
                    return DXGI_FORMAT_D32_FLOAT;
                case ERhiFormat::Unknown:
                default:
                    return DXGI_FORMAT_UNKNOWN;
            }
        }

        auto ToBindingType(const Shader::FShaderResourceBinding& resource) -> Rhi::ERhiBindingType {
            using Rhi::ERhiBindingType;
            switch (resource.mType) {
                case Shader::EShaderResourceType::ConstantBuffer:
                    return ERhiBindingType::ConstantBuffer;
                case Shader::EShaderResourceType::Texture:
                    return ERhiBindingType::SampledTexture;
                case Shader::EShaderResourceType::Sampler:
                    return ERhiBindingType::Sampler;
                case Shader::EShaderResourceType::StorageBuffer:
                    return (resource.mAccess == Shader::EShaderResourceAccess::ReadWrite)
                        ? ERhiBindingType::StorageBuffer
                        : ERhiBindingType::SampledBuffer;
                case Shader::EShaderResourceType::StorageTexture:
                    return (resource.mAccess == Shader::EShaderResourceAccess::ReadWrite)
                        ? ERhiBindingType::StorageTexture
                        : ERhiBindingType::SampledTexture;
                case Shader::EShaderResourceType::AccelerationStructure:
                    return ERhiBindingType::AccelerationStructure;
                default:
                    return ERhiBindingType::SampledTexture;
            }
        }

        auto HasLayoutBinding(const FRhiPipelineLayout* layout, u32 setIndex, u32 binding,
            Rhi::ERhiBindingType type) -> bool {
            if (layout == nullptr) {
                return true;
            }

            const auto& pipelineDesc = layout->GetDesc();
            for (const auto* group : pipelineDesc.mBindGroupLayouts) {
                if (group == nullptr) {
                    continue;
                }
                const auto& groupDesc = group->GetDesc();
                if (groupDesc.mSetIndex != setIndex) {
                    continue;
                }
                for (const auto& entry : groupDesc.mEntries) {
                    if (entry.mBinding == binding) {
                        return entry.mType == type;
                    }
                }
            }
            return false;
        }

        auto AppendReflectionBindings(const Shader::FShaderReflection& reflection,
            EShaderStage stage, const FRhiPipelineLayout* layout,
            TVector<FD3D11BindingMappingEntry>& outBindings) -> void {
            for (const auto& resource : reflection.mResources) {
                const auto bindingType = ToBindingType(resource);
                if (!HasLayoutBinding(layout, resource.mSet, resource.mBinding, bindingType)) {
                    continue;
                }

                FD3D11BindingMappingEntry entry{};
                entry.mStage    = stage;
                entry.mType     = bindingType;
                entry.mSet      = resource.mSet;
                entry.mBinding  = resource.mBinding;
                entry.mRegister = resource.mRegister;
                entry.mSpace    = resource.mSpace;
                outBindings.PushBack(entry);
            }
        }

        auto ToAnsiString(const FString& text) -> std::string {
            std::string out;
            const auto  length = static_cast<size_t>(text.Length());
            out.reserve(length);
            const auto* data = text.GetData();
            for (size_t i = 0; i < length; ++i) {
                const auto ch = data[i];
                out.push_back((ch <= 0x7f) ? static_cast<char>(ch) : '?');
            }
            return out;
        }

        auto BuildInputLayout(const FRhiGraphicsPipelineDesc& desc, ID3D11Device* device)
            -> ComPtr<ID3D11InputLayout> {
            ComPtr<ID3D11InputLayout> layout;
            if (device == nullptr || desc.mVertexShader == nullptr) {
                return layout;
            }

            const auto& shaderDesc = desc.mVertexShader->GetDesc();
            if (shaderDesc.mBytecode.IsEmpty() || desc.mVertexLayout.mAttributes.IsEmpty()) {
                return layout;
            }

            TVector<D3D11_INPUT_ELEMENT_DESC> elements;
            elements.Reserve(desc.mVertexLayout.mAttributes.Size());

            std::vector<std::string> semanticStorage;
            semanticStorage.reserve(static_cast<size_t>(desc.mVertexLayout.mAttributes.Size()));

            for (const auto& attribute : desc.mVertexLayout.mAttributes) {
                const DXGI_FORMAT format = ToD3D11Format(attribute.mFormat);
                if (format == DXGI_FORMAT_UNKNOWN) {
                    return layout;
                }

                semanticStorage.push_back(ToAnsiString(attribute.mSemanticName));

                D3D11_INPUT_ELEMENT_DESC element{};
                element.SemanticName      = semanticStorage.back().c_str();
                element.SemanticIndex     = attribute.mSemanticIndex;
                element.Format            = format;
                element.InputSlot         = attribute.mInputSlot;
                element.AlignedByteOffset = attribute.mAlignedByteOffset;
                element.InputSlotClass    = attribute.mPerInstance ? D3D11_INPUT_PER_INSTANCE_DATA
                                                                   : D3D11_INPUT_PER_VERTEX_DATA;
                element.InstanceDataStepRate = attribute.mPerInstance
                    ? (attribute.mInstanceStepRate == 0U ? 1U : attribute.mInstanceStepRate)
                    : 0U;
                elements.PushBack(element);
            }

            const void*  data = shaderDesc.mBytecode.Data();
            const SIZE_T size = static_cast<SIZE_T>(shaderDesc.mBytecode.Size());
            if (data == nullptr || size == 0U) {
                return layout;
            }

            const HRESULT hr = device->CreateInputLayout(
                elements.Data(), static_cast<UINT>(elements.Size()), data, size, &layout);
            if (FAILED(hr)) {
                layout.Reset();
            }
            return layout;
        }
#endif

        class FRhiD3D11PipelineLayout final : public FRhiPipelineLayout {
        public:
            explicit FRhiD3D11PipelineLayout(const FRhiPipelineLayoutDesc& desc)
                : FRhiPipelineLayout(desc) {}
        };

        class FRhiD3D11BindGroupLayout final : public FRhiBindGroupLayout {
        public:
            explicit FRhiD3D11BindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
                : FRhiBindGroupLayout(desc) {}
        };

        class FRhiD3D11BindGroup final : public FRhiBindGroup {
        public:
            explicit FRhiD3D11BindGroup(const FRhiBindGroupDesc& desc) : FRhiBindGroup(desc) {}
        };

        class FRhiD3D11Fence final : public FRhiFence {
        public:
            explicit FRhiD3D11Fence(u64 initialValue) : FRhiFence(), mValue(initialValue) {}

            [[nodiscard]] auto GetCompletedValue() const noexcept -> u64 override { return mValue; }
            void               SignalCPU(u64 value) override { mValue = value; }
            void               WaitCPU(u64 value) override { mValue = value; }
            void               Reset(u64 value) override { mValue = value; }

        private:
            u64 mValue = 0ULL;
        };

        class FRhiD3D11Semaphore final : public FRhiSemaphore {
        public:
            FRhiD3D11Semaphore(bool timeline, u64 initialValue)
                : FRhiSemaphore(), mIsTimeline(timeline), mValue(initialValue) {}

            [[nodiscard]] auto IsTimeline() const noexcept -> bool override { return mIsTimeline; }
            [[nodiscard]] auto GetCurrentValue() const noexcept -> u64 override { return mValue; }
            void               Signal(u64 value) {
                if (mIsTimeline) {
                    mValue = value;
                }
            }

        private:
            bool mIsTimeline = false;
            u64  mValue      = 0ULL;
        };

        class FRhiD3D11CommandPool final : public FRhiCommandPool {
        public:
            explicit FRhiD3D11CommandPool(const FRhiCommandPoolDesc& desc)
                : FRhiCommandPool(desc) {}

            void Reset() override {}
        };

        class FRhiD3D11Queue final : public FRhiQueue {
        public:
            FRhiD3D11Queue(ERhiQueueType type, ID3D11DeviceContext* immediateContext)
                : FRhiQueue(type) {
#if AE_PLATFORM_WIN
                mImmediateContext = immediateContext;
#else
                (void)immediateContext;
#endif
            }

            void Submit(const FRhiSubmitInfo& info) override {
#if AE_PLATFORM_WIN
                if (mImmediateContext && info.mCommandLists) {
                    for (u32 i = 0; i < info.mCommandListCount; ++i) {
                        auto* rhiList = info.mCommandLists[i];
                        if (rhiList == nullptr) {
                            continue;
                        }
                        auto* d3dList    = static_cast<FRhiD3D11CommandList*>(rhiList);
                        auto* nativeList = d3dList ? d3dList->GetNativeCommandList() : nullptr;
                        if (nativeList) {
                            mImmediateContext->ExecuteCommandList(nativeList, TRUE);
                        }
                    }
                }
#endif

                if (info.mSignals) {
                    for (u32 i = 0; i < info.mSignalCount; ++i) {
                        const auto& signal = info.mSignals[i];
                        if (signal.mSemaphore == nullptr) {
                            continue;
                        }
                        if (!signal.mSemaphore->IsTimeline()) {
                            continue;
                        }
                        auto* semaphore = static_cast<FRhiD3D11Semaphore*>(signal.mSemaphore);
                        semaphore->Signal(signal.mValue);
                    }
                }

                if (info.mFence) {
                    info.mFence->SignalCPU(info.mFenceValue);
                }
            }
            void Signal(FRhiFence* fence, u64 value) override {
                if (fence) {
                    fence->SignalCPU(value);
                }
            }
            void Wait(FRhiFence* fence, u64 value) override {
                if (fence) {
                    fence->WaitCPU(value);
                }
            }
            void WaitIdle() override {
#if AE_PLATFORM_WIN
                if (mImmediateContext) {
                    mImmediateContext->Flush();
                }
#endif
            }
            void Present(const FRhiPresentInfo& info) override {
                if (info.mViewport) {
                    info.mViewport->Present(info);
                }
            }

        private:
#if AE_PLATFORM_WIN
            ComPtr<ID3D11DeviceContext> mImmediateContext;
#else
            ID3D11DeviceContext* mImmediateContext = nullptr;
#endif
        };
    } // namespace

#if AE_PLATFORM_WIN
    struct FRhiD3D11GraphicsPipeline::FState {
        ComPtr<ID3D11InputLayout> mInputLayout;
    };
#else
    struct FRhiD3D11GraphicsPipeline::FState {};
#endif

    FRhiD3D11GraphicsPipeline::FRhiD3D11GraphicsPipeline(
        const FRhiGraphicsPipelineDesc& desc, ID3D11Device* device)
        : FRhiPipeline(desc)
        , mPipelineLayout(desc.mPipelineLayout)
        , mVertexShader(desc.mVertexShader)
        , mPixelShader(desc.mPixelShader)
        , mGeometryShader(desc.mGeometryShader)
        , mHullShader(desc.mHullShader)
        , mDomainShader(desc.mDomainShader) {
#if AE_PLATFORM_WIN
        mState = new FState{};
        if (mState) {
            mState->mInputLayout = BuildInputLayout(desc, device);
        }
        const FRhiPipelineLayout* layout = desc.mPipelineLayout;
        if (desc.mVertexShader) {
            AppendReflectionBindings(
                desc.mVertexShader->GetDesc().mReflection, EShaderStage::Vertex, layout, mBindings);
        }
        if (desc.mPixelShader) {
            AppendReflectionBindings(
                desc.mPixelShader->GetDesc().mReflection, EShaderStage::Pixel, layout, mBindings);
        }
        if (desc.mGeometryShader) {
            AppendReflectionBindings(desc.mGeometryShader->GetDesc().mReflection,
                EShaderStage::Geometry, layout, mBindings);
        }
        if (desc.mHullShader) {
            AppendReflectionBindings(
                desc.mHullShader->GetDesc().mReflection, EShaderStage::Hull, layout, mBindings);
        }
        if (desc.mDomainShader) {
            AppendReflectionBindings(
                desc.mDomainShader->GetDesc().mReflection, EShaderStage::Domain, layout, mBindings);
        }
#else
        (void)desc;
        (void)device;
#endif
    }

    FRhiD3D11GraphicsPipeline::~FRhiD3D11GraphicsPipeline() {
#if AE_PLATFORM_WIN
        delete mState;
        mState = nullptr;
#endif
    }

    auto FRhiD3D11GraphicsPipeline::GetInputLayout() const noexcept -> ID3D11InputLayout* {
#if AE_PLATFORM_WIN
        return mState ? mState->mInputLayout.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11GraphicsPipeline::GetBindingMappings() const noexcept
        -> const TVector<FD3D11BindingMappingEntry>& {
        return mBindings;
    }

    FRhiD3D11ComputePipeline::FRhiD3D11ComputePipeline(const FRhiComputePipelineDesc& desc)
        : FRhiPipeline(desc)
        , mPipelineLayout(desc.mPipelineLayout)
        , mComputeShader(desc.mComputeShader) {
#if AE_PLATFORM_WIN
        if (desc.mComputeShader) {
            AppendReflectionBindings(desc.mComputeShader->GetDesc().mReflection,
                EShaderStage::Compute, desc.mPipelineLayout, mBindings);
        }
#else
        (void)desc;
#endif
    }

    FRhiD3D11ComputePipeline::~FRhiD3D11ComputePipeline() = default;

    auto FRhiD3D11ComputePipeline::GetBindingMappings() const noexcept
        -> const TVector<FD3D11BindingMappingEntry>& {
        return mBindings;
    }

    FRhiD3D11Device::FRhiD3D11Device(const FRhiDeviceDesc& desc, const FRhiAdapterDesc& adapterDesc,
        ID3D11Device* device, ID3D11DeviceContext* context, u32 featureLevel)
        : FRhiDevice(desc, adapterDesc) {
        mState = new FState{};

#if AE_PLATFORM_WIN
        if (mState) {
            mState->mDevice.Attach(device);
            mState->mImmediateContext.Attach(context);
            mState->mFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(featureLevel);
        }

        FRhiSupportedLimits limits;
        limits.mMaxTextureDimension1D = D3D11_REQ_TEXTURE1D_U_DIMENSION;
        limits.mMaxTextureDimension2D = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        limits.mMaxTextureDimension3D = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        limits.mMaxTextureArrayLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        limits.mMaxSamplers           = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
        limits.mMaxColorAttachments   = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        SetSupportedLimits(limits);

        FRhiQueueCapabilities queueCaps;
        queueCaps.mSupportsGraphics     = true;
        queueCaps.mSupportsCompute      = true;
        queueCaps.mSupportsCopy         = true;
        queueCaps.mSupportsAsyncCompute = false;
        queueCaps.mSupportsAsyncCopy    = false;
        SetQueueCapabilities(queueCaps);

        if (mState) {
            FD3D11UploadBufferManagerDesc uploadDesc{};
            uploadDesc.mAllowConstantBufferSuballocation = false;
            mState->mUploadManager.Init(this, uploadDesc);
            mState->mStagingManager.Init(this);

            if (mState->mDevice && mState->mImmediateContext) {
                constexpr u32 kQueryCount = 3U;
                mState->mFrameQueries.Resize(kQueryCount);
                mState->mFrameQuerySerials.Resize(kQueryCount);
                for (u32 i = 0U; i < kQueryCount; ++i) {
                    mState->mFrameQuerySerials[i] = 0ULL;
                }

                D3D11_QUERY_DESC queryDesc{};
                queryDesc.Query = D3D11_QUERY_EVENT;
                queryDesc.MiscFlags = 0U;

                bool queryOk = true;
                for (u32 i = 0U; i < kQueryCount; ++i) {
                    ComPtr<ID3D11Query> query;
                    if (FAILED(mState->mDevice->CreateQuery(&queryDesc, &query))) {
                        queryOk = false;
                        break;
                    }
                    mState->mFrameQueries[i] = AltinaEngine::Move(query);
                }

                if (!queryOk) {
                    mState->mFrameQueries.Clear();
                    mState->mFrameQuerySerials.Clear();
                }
            }
        }
#else
        (void)device;
        (void)context;
        (void)featureLevel;
#endif

        ID3D11DeviceContext* immediateContext = GetImmediateContext();
        RegisterQueue(ERhiQueueType::Graphics,
            MakeResource<FRhiD3D11Queue>(ERhiQueueType::Graphics, immediateContext));
        RegisterQueue(ERhiQueueType::Compute,
            MakeResource<FRhiD3D11Queue>(ERhiQueueType::Compute, immediateContext));
        RegisterQueue(ERhiQueueType::Copy,
            MakeResource<FRhiD3D11Queue>(ERhiQueueType::Copy, immediateContext));
    }

    FRhiD3D11Device::~FRhiD3D11Device() {
        delete mState;
        mState = nullptr;
    }

    auto FRhiD3D11Device::GetNativeDevice() const noexcept -> ID3D11Device* {
#if AE_PLATFORM_WIN
        return mState ? mState->mDevice.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::GetImmediateContext() const noexcept -> ID3D11DeviceContext* {
#if AE_PLATFORM_WIN
        return mState ? mState->mImmediateContext.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::GetFeatureLevel() const noexcept -> u32 {
#if AE_PLATFORM_WIN
        return mState ? static_cast<u32>(mState->mFeatureLevel) : 0U;
#else
        return 0U;
#endif
    }

    auto FRhiD3D11Device::GetUploadBufferManager() noexcept -> FD3D11UploadBufferManager* {
#if AE_PLATFORM_WIN
        return mState ? &mState->mUploadManager : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::GetStagingBufferManager() noexcept -> FD3D11StagingBufferManager* {
#if AE_PLATFORM_WIN
        return mState ? &mState->mStagingManager : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::CreateViewport(const FRhiViewportDesc& desc) -> FRhiViewportRef {
#if AE_PLATFORM_WIN
        auto viewport = MakeResource<FRhiD3D11Viewport>(
            desc, GetNativeDevice(), GetImmediateContext());
        if (viewport && !viewport->IsValid()) {
            viewport->SetDeleteQueue(nullptr);
            viewport.Reset();
        }
        return viewport;
#else
        (void)desc;
        return {};
#endif
    }

    auto FRhiD3D11Device::CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef {
#if AE_PLATFORM_WIN
        ID3D11Device* device = GetNativeDevice();
        if (!device || desc.mBytecode.IsEmpty()) {
            return {};
        }

        const void*  data = desc.mBytecode.Data();
        const SIZE_T size = static_cast<SIZE_T>(desc.mBytecode.Size());
        if (!data || size == 0U) {
            return {};
        }

        HRESULT                   hr = E_FAIL;
        ComPtr<ID3D11DeviceChild> shader;

        switch (desc.mStage) {
            case EShaderStage::Vertex:
            {
                ComPtr<ID3D11VertexShader> vertexShader;
                hr     = device->CreateVertexShader(data, size, nullptr, &vertexShader);
                shader = vertexShader;
                break;
            }
            case EShaderStage::Pixel:
            {
                ComPtr<ID3D11PixelShader> pixelShader;
                hr     = device->CreatePixelShader(data, size, nullptr, &pixelShader);
                shader = pixelShader;
                break;
            }
            case EShaderStage::Compute:
            {
                ComPtr<ID3D11ComputeShader> computeShader;
                hr     = device->CreateComputeShader(data, size, nullptr, &computeShader);
                shader = computeShader;
                break;
            }
            case EShaderStage::Geometry:
            {
                ComPtr<ID3D11GeometryShader> geometryShader;
                hr     = device->CreateGeometryShader(data, size, nullptr, &geometryShader);
                shader = geometryShader;
                break;
            }
            case EShaderStage::Hull:
            {
                ComPtr<ID3D11HullShader> hullShader;
                hr     = device->CreateHullShader(data, size, nullptr, &hullShader);
                shader = hullShader;
                break;
            }
            case EShaderStage::Domain:
            {
                ComPtr<ID3D11DomainShader> domainShader;
                hr     = device->CreateDomainShader(data, size, nullptr, &domainShader);
                shader = domainShader;
                break;
            }
            case EShaderStage::Mesh:
            case EShaderStage::Amplification:
            case EShaderStage::Library:
            default:
                return {};
        }

        if (FAILED(hr) || shader == nullptr) {
            return {};
        }

        return MakeResource<FRhiD3D11Shader>(desc, shader.Detach());
#else
        return MakeResource<FRhiD3D11Shader>(desc);
#endif
    }

    auto FRhiD3D11Device::CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
        -> FRhiPipelineRef {
#if AE_PLATFORM_WIN
        return MakeResource<FRhiD3D11GraphicsPipeline>(desc, GetNativeDevice());
#else
        return MakeResource<FRhiD3D11GraphicsPipeline>(desc);
#endif
    }

    auto FRhiD3D11Device::CreateComputePipeline(const FRhiComputePipelineDesc& desc)
        -> FRhiPipelineRef {
        return MakeResource<FRhiD3D11ComputePipeline>(desc);
    }

    auto FRhiD3D11Device::CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
        -> FRhiPipelineLayoutRef {
        return MakeResource<FRhiD3D11PipelineLayout>(desc);
    }

    auto FRhiD3D11Device::CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
        -> FRhiBindGroupLayoutRef {
        return MakeResource<FRhiD3D11BindGroupLayout>(desc);
    }

    auto FRhiD3D11Device::CreateBindGroup(const FRhiBindGroupDesc& desc) -> FRhiBindGroupRef {
        return MakeResource<FRhiD3D11BindGroup>(desc);
    }

    auto FRhiD3D11Device::CreateFence(u64 initialValue) -> FRhiFenceRef {
        return MakeResource<FRhiD3D11Fence>(initialValue);
    }

    auto FRhiD3D11Device::CreateSemaphore(bool timeline, u64 initialValue) -> FRhiSemaphoreRef {
        return MakeResource<FRhiD3D11Semaphore>(timeline, initialValue);
    }

    auto FRhiD3D11Device::CreateCommandPool(const FRhiCommandPoolDesc& desc) -> FRhiCommandPoolRef {
        return MakeResource<FRhiD3D11CommandPool>(desc);
    }

    auto FRhiD3D11Device::CreateCommandList(const FRhiCommandListDesc& desc) -> FRhiCommandListRef {
        return MakeResource<FRhiD3D11CommandList>(desc);
    }

    auto FRhiD3D11Device::CreateCommandContext(const FRhiCommandContextDesc& desc)
        -> FRhiCommandContextRef {
        FRhiCommandListDesc listDesc;
        listDesc.mDebugName = desc.mDebugName;
        listDesc.mQueueType = desc.mQueueType;
        listDesc.mListType  = desc.mListType;
        auto commandList    = MakeResource<FRhiD3D11CommandList>(listDesc);
        return MakeResource<FRhiD3D11CommandContext>(
            desc, GetNativeDevice(), AltinaEngine::Move(commandList));
    }

    void FRhiD3D11Device::BeginFrame(u64 frameIndex) {
#if AE_PLATFORM_WIN
        if (!mState) {
            return;
        }

        mState->mFrameIndex = frameIndex;
        mState->mUploadManager.BeginFrame(frameIndex);
        mState->mStagingManager.Reset();

        if (mState->mImmediateContext && !mState->mFrameQueries.IsEmpty()) {
            for (usize i = 0; i < mState->mFrameQueries.Size(); ++i) {
                auto& query = mState->mFrameQueries[i];
                if (!query) {
                    continue;
                }
                const HRESULT hr = mState->mImmediateContext->GetData(query.Get(), nullptr, 0, 0);
                if (hr == S_OK) {
                    const u64 serial = mState->mFrameQuerySerials[i];
                    if (serial > mState->mCompletedSerial) {
                        mState->mCompletedSerial = serial;
                    }
                }
            }
        }
        ProcessResourceDeleteQueue(mState->mCompletedSerial);
#else
        (void)frameIndex;
#endif
    }

    void FRhiD3D11Device::EndFrame() {
#if AE_PLATFORM_WIN
        if (!mState) {
            return;
        }

        mState->mUploadManager.EndFrame();

        if (mState->mImmediateContext && !mState->mFrameQueries.IsEmpty()) {
            const u32 queryCount = static_cast<u32>(mState->mFrameQueries.Size());
            const u32 index = (queryCount > 0U) ? (mState->mFrameQueryIndex % queryCount) : 0U;
            auto& query = mState->mFrameQueries[index];
            if (query) {
                mState->mFrameQuerySerials[index] = mState->mFrameIndex;
                mState->mImmediateContext->End(query.Get());
            }
            mState->mFrameQueryIndex = index + 1U;
        }
#endif
    }

} // namespace AltinaEngine::Rhi

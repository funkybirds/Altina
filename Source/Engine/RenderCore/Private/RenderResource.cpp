#include "RenderResource.h"

#include "Threading/RenderingThread.h"

#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"

using AltinaEngine::Core::Container::FString;
using AltinaEngine::Core::Container::TVector;
namespace AltinaEngine::RenderCore {
    namespace {
        using Core::Platform::Generic::Memcpy;

        void UploadBufferData(const Rhi::FRhiBufferRef& buffer, const TVector<u8>& data) {
            if (!buffer || data.IsEmpty()) {
                return;
            }

            auto lock = buffer->Lock(0ULL, static_cast<u64>(data.Size()),
                Rhi::ERhiBufferLockMode::WriteDiscard);
            if (!lock.IsValid()) {
                return;
            }

            Memcpy(lock.mData, data.Data(), static_cast<usize>(data.Size()));
            buffer->Unlock(lock);
        }

        void FillTextureRange(Rhi::FRhiTextureViewRange& range,
            const Rhi::FRhiTextureDesc& desc) {
            if (range.mMipCount == 0U) {
                range.mMipCount = desc.mMipLevels;
            }
            if (range.mLayerCount == 0U) {
                range.mLayerCount = desc.mArrayLayers;
            }
            if (range.mDepthSliceCount == 0U) {
                range.mDepthSliceCount = desc.mDepth;
            }
        }
    } // namespace

    FRenderResource::~FRenderResource() = default;

    void FRenderResource::InitResource() noexcept {
        i32 expected = static_cast<i32>(EState::Uninitialized);
        if (!mState.CompareExchangeStrong(expected, static_cast<i32>(EState::InitPending))) {
            return;
        }

        mInitHandle = EnqueueRenderTask(FString(TEXT("RenderResource.Init")),
            [this]() -> void {
                InitRHI();
                mState.Store(static_cast<i32>(EState::Initialized));
                OnInitComplete();
            });
    }

    void FRenderResource::ReleaseResource() noexcept {
        const i32 current = mState.Load();
        if (current == static_cast<i32>(EState::Uninitialized)
            || current == static_cast<i32>(EState::ReleasePending)) {
            return;
        }

        mState.Store(static_cast<i32>(EState::ReleasePending));
        mReleaseHandle = EnqueueRenderTask(FString(TEXT("RenderResource.Release")),
            [this]() -> void {
                ReleaseRHI();
                mState.Store(static_cast<i32>(EState::Uninitialized));
            });
    }

    void FRenderResource::UpdateResource() noexcept {
        if (mState.Load() != static_cast<i32>(EState::Initialized)) {
            return;
        }

        EnqueueRenderTask(FString(TEXT("RenderResource.Update")),
            [this]() -> void { UpdateRHI(); });
    }

    void FRenderResource::WaitForInit() noexcept {
        if (mInitHandle.IsValid()) {
            Core::Jobs::FJobSystem::Wait(mInitHandle);
        }
    }

    void FRenderResource::WaitForRelease() noexcept {
        if (mReleaseHandle.IsValid()) {
            Core::Jobs::FJobSystem::Wait(mReleaseHandle);
        }
    }

    auto FRenderResource::IsInitialized() const noexcept -> bool {
        return mState.Load() == static_cast<i32>(EState::Initialized);
    }

    FPositionBuffer::FPositionBuffer() noexcept {
        mDesc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
        mDesc.mBindFlags = Rhi::ERhiBufferBindFlags::Vertex;
        mDesc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
    }

    FPositionBuffer::FPositionBuffer(const Rhi::FRhiBufferDesc& desc) noexcept : mDesc(desc) {
        if (mDesc.mBindFlags == Rhi::ERhiBufferBindFlags::None) {
            mDesc.mBindFlags = Rhi::ERhiBufferBindFlags::Vertex;
        }
    }

    void FPositionBuffer::SetData(const void* data, u32 sizeBytes, u32 strideBytes) {
        mDesc.mSizeBytes = sizeBytes;
        mStrideBytes     = strideBytes;

        mStagingData.Clear();
        if (data != nullptr && sizeBytes > 0U) {
            mStagingData.Resize(sizeBytes);
            Memcpy(mStagingData.Data(), data, static_cast<usize>(sizeBytes));
        }

        if (IsInitialized()) {
            UpdateResource();
        }
    }

    auto FPositionBuffer::GetView() const noexcept -> Rhi::FRhiVertexBufferView {
        Rhi::FRhiVertexBufferView view{};
        view.mBuffer      = mBuffer.Get();
        view.mStrideBytes = mStrideBytes;
        view.mOffsetBytes = 0U;
        return view;
    }

    void FPositionBuffer::InitRHI() {
        if (mDesc.mSizeBytes == 0ULL) {
            return;
        }
        mBuffer = Rhi::RHICreateBuffer(mDesc);
        UploadBufferData(mBuffer, mStagingData);
    }

    void FPositionBuffer::ReleaseRHI() { mBuffer.Reset(); }

    void FPositionBuffer::UpdateRHI() { UploadBufferData(mBuffer, mStagingData); }

    FVertexTangentBuffer::FVertexTangentBuffer() noexcept {
        mDesc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
        mDesc.mBindFlags = Rhi::ERhiBufferBindFlags::Vertex;
        mDesc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
    }

    FVertexTangentBuffer::FVertexTangentBuffer(const Rhi::FRhiBufferDesc& desc) noexcept
        : mDesc(desc) {
        if (mDesc.mBindFlags == Rhi::ERhiBufferBindFlags::None) {
            mDesc.mBindFlags = Rhi::ERhiBufferBindFlags::Vertex;
        }
    }

    void FVertexTangentBuffer::SetData(const void* data, u32 sizeBytes, u32 strideBytes) {
        mDesc.mSizeBytes = sizeBytes;
        mStrideBytes     = strideBytes;

        mStagingData.Clear();
        if (data != nullptr && sizeBytes > 0U) {
            mStagingData.Resize(sizeBytes);
            Memcpy(mStagingData.Data(), data, static_cast<usize>(sizeBytes));
        }

        if (IsInitialized()) {
            UpdateResource();
        }
    }

    auto FVertexTangentBuffer::GetView() const noexcept -> Rhi::FRhiVertexBufferView {
        Rhi::FRhiVertexBufferView view{};
        view.mBuffer      = mBuffer.Get();
        view.mStrideBytes = mStrideBytes;
        view.mOffsetBytes = 0U;
        return view;
    }

    void FVertexTangentBuffer::InitRHI() {
        if (mDesc.mSizeBytes == 0ULL) {
            return;
        }
        mBuffer = Rhi::RHICreateBuffer(mDesc);
        UploadBufferData(mBuffer, mStagingData);
    }

    void FVertexTangentBuffer::ReleaseRHI() { mBuffer.Reset(); }

    void FVertexTangentBuffer::UpdateRHI() { UploadBufferData(mBuffer, mStagingData); }

    FVertexUVBuffer::FVertexUVBuffer() noexcept {
        mDesc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
        mDesc.mBindFlags = Rhi::ERhiBufferBindFlags::Vertex;
        mDesc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
    }

    FVertexUVBuffer::FVertexUVBuffer(const Rhi::FRhiBufferDesc& desc) noexcept : mDesc(desc) {
        if (mDesc.mBindFlags == Rhi::ERhiBufferBindFlags::None) {
            mDesc.mBindFlags = Rhi::ERhiBufferBindFlags::Vertex;
        }
    }

    void FVertexUVBuffer::SetData(const void* data, u32 sizeBytes, u32 strideBytes) {
        mDesc.mSizeBytes = sizeBytes;
        mStrideBytes     = strideBytes;

        mStagingData.Clear();
        if (data != nullptr && sizeBytes > 0U) {
            mStagingData.Resize(sizeBytes);
            Memcpy(mStagingData.Data(), data, static_cast<usize>(sizeBytes));
        }

        if (IsInitialized()) {
            UpdateResource();
        }
    }

    auto FVertexUVBuffer::GetView() const noexcept -> Rhi::FRhiVertexBufferView {
        Rhi::FRhiVertexBufferView view{};
        view.mBuffer      = mBuffer.Get();
        view.mStrideBytes = mStrideBytes;
        view.mOffsetBytes = 0U;
        return view;
    }

    void FVertexUVBuffer::InitRHI() {
        if (mDesc.mSizeBytes == 0ULL) {
            return;
        }
        mBuffer = Rhi::RHICreateBuffer(mDesc);
        UploadBufferData(mBuffer, mStagingData);
    }

    void FVertexUVBuffer::ReleaseRHI() { mBuffer.Reset(); }

    void FVertexUVBuffer::UpdateRHI() { UploadBufferData(mBuffer, mStagingData); }

    FIndexBuffer::FIndexBuffer() noexcept {
        mDesc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
        mDesc.mBindFlags = Rhi::ERhiBufferBindFlags::Index;
        mDesc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
    }

    FIndexBuffer::FIndexBuffer(const Rhi::FRhiBufferDesc& desc,
        Rhi::ERhiIndexType indexType) noexcept
        : mDesc(desc), mIndexType(indexType) {
        if (mDesc.mBindFlags == Rhi::ERhiBufferBindFlags::None) {
            mDesc.mBindFlags = Rhi::ERhiBufferBindFlags::Index;
        }
    }

    void FIndexBuffer::SetData(const void* data, u32 sizeBytes, Rhi::ERhiIndexType indexType) {
        mDesc.mSizeBytes = sizeBytes;
        mIndexType       = indexType;

        mStagingData.Clear();
        if (data != nullptr && sizeBytes > 0U) {
            mStagingData.Resize(sizeBytes);
            Memcpy(mStagingData.Data(), data, static_cast<usize>(sizeBytes));
        }

        if (IsInitialized()) {
            UpdateResource();
        }
    }

    auto FIndexBuffer::GetView() const noexcept -> Rhi::FRhiIndexBufferView {
        Rhi::FRhiIndexBufferView view{};
        view.mBuffer      = mBuffer.Get();
        view.mIndexType   = mIndexType;
        view.mOffsetBytes = 0U;
        return view;
    }

    void FIndexBuffer::InitRHI() {
        if (mDesc.mSizeBytes == 0ULL) {
            return;
        }
        mBuffer = Rhi::RHICreateBuffer(mDesc);
        UploadBufferData(mBuffer, mStagingData);
    }

    void FIndexBuffer::ReleaseRHI() { mBuffer.Reset(); }

    void FIndexBuffer::UpdateRHI() { UploadBufferData(mBuffer, mStagingData); }

    FTexture::FTexture(const Rhi::FRhiTextureDesc& desc) noexcept : mDesc(desc) {}

    void FTexture::SetDesc(const Rhi::FRhiTextureDesc& desc) noexcept { mDesc = desc; }

    auto FTexture::GetDesc() const noexcept -> const Rhi::FRhiTextureDesc& { return mDesc; }

    void FTexture::InitRHI() { mTexture = Rhi::RHICreateTexture(mDesc); }

    void FTexture::ReleaseRHI() { mTexture.Reset(); }

    FTextureWithSRV::FTextureWithSRV(const Rhi::FRhiTextureDesc& desc) noexcept : FTexture(desc) {
        mDesc.mBindFlags |= Rhi::ERhiTextureBindFlags::ShaderResource;
    }

    void FTextureWithSRV::SetSRVDesc(const Rhi::FRhiShaderResourceViewDesc& desc) noexcept {
        mSRVDesc = desc;
    }

    void FTextureWithSRV::InitRHI() {
        FTexture::InitRHI();
        auto* device = Rhi::RHIGetDevice();
        if (!device || !mTexture) {
            return;
        }

        Rhi::FRhiShaderResourceViewDesc desc = mSRVDesc;
        desc.mTexture = mTexture.Get();
        if (desc.mFormat == Rhi::ERhiFormat::Unknown) {
            desc.mFormat = mDesc.mFormat;
        }
        FillTextureRange(desc.mTextureRange, mDesc);

        mSRV = device->CreateShaderResourceView(desc);
    }

    void FTextureWithSRV::ReleaseRHI() {
        mSRV.Reset();
        FTexture::ReleaseRHI();
    }

    FTextureWithUAV::FTextureWithUAV() noexcept {
        mDesc.mBindFlags |= Rhi::ERhiTextureBindFlags::UnorderedAccess;
    }

    FTextureWithUAV::FTextureWithUAV(const Rhi::FRhiTextureDesc& desc) noexcept : FTexture(desc) {
        mDesc.mBindFlags |= Rhi::ERhiTextureBindFlags::UnorderedAccess;
    }

    void FTextureWithUAV::SetUAVDesc(const Rhi::FRhiUnorderedAccessViewDesc& desc) noexcept {
        mUAVDesc = desc;
    }

    void FTextureWithUAV::InitRHI() {
        FTexture::InitRHI();
        auto* device = Rhi::RHIGetDevice();
        if (!device || !mTexture) {
            return;
        }

        Rhi::FRhiUnorderedAccessViewDesc desc = mUAVDesc;
        desc.mTexture = mTexture.Get();
        if (desc.mFormat == Rhi::ERhiFormat::Unknown) {
            desc.mFormat = mDesc.mFormat;
        }
        FillTextureRange(desc.mTextureRange, mDesc);

        mUAV = device->CreateUnorderedAccessView(desc);
    }

    void FTextureWithUAV::ReleaseRHI() {
        mUAV.Reset();
        FTexture::ReleaseRHI();
    }

    FTextureWithSRVUAV::FTextureWithSRVUAV() noexcept {
        mDesc.mBindFlags |= Rhi::ERhiTextureBindFlags::ShaderResource;
        mDesc.mBindFlags |= Rhi::ERhiTextureBindFlags::UnorderedAccess;
    }

    FTextureWithSRVUAV::FTextureWithSRVUAV(const Rhi::FRhiTextureDesc& desc) noexcept
        : FTexture(desc) {
        mDesc.mBindFlags |= Rhi::ERhiTextureBindFlags::ShaderResource;
        mDesc.mBindFlags |= Rhi::ERhiTextureBindFlags::UnorderedAccess;
    }

    void FTextureWithSRVUAV::SetSRVDesc(const Rhi::FRhiShaderResourceViewDesc& desc) noexcept {
        mSRVDesc = desc;
    }

    void FTextureWithSRVUAV::SetUAVDesc(const Rhi::FRhiUnorderedAccessViewDesc& desc) noexcept {
        mUAVDesc = desc;
    }

    void FTextureWithSRVUAV::InitRHI() {
        FTexture::InitRHI();
        auto* device = Rhi::RHIGetDevice();
        if (!device || !mTexture) {
            return;
        }

        Rhi::FRhiShaderResourceViewDesc srvDesc = mSRVDesc;
        srvDesc.mTexture = mTexture.Get();
        if (srvDesc.mFormat == Rhi::ERhiFormat::Unknown) {
            srvDesc.mFormat = mDesc.mFormat;
        }
        FillTextureRange(srvDesc.mTextureRange, mDesc);
        mSRV = device->CreateShaderResourceView(srvDesc);

        Rhi::FRhiUnorderedAccessViewDesc uavDesc = mUAVDesc;
        uavDesc.mTexture = mTexture.Get();
        if (uavDesc.mFormat == Rhi::ERhiFormat::Unknown) {
            uavDesc.mFormat = mDesc.mFormat;
        }
        FillTextureRange(uavDesc.mTextureRange, mDesc);
        mUAV = device->CreateUnorderedAccessView(uavDesc);
    }

    void FTextureWithSRVUAV::ReleaseRHI() {
        mSRV.Reset();
        mUAV.Reset();
        FTexture::ReleaseRHI();
    }

} // namespace AltinaEngine::RenderCore











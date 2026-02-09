#include "Rhi/RhiDevice.h"
#include "Rhi/RhiQueue.h"

#include "Types/Traits.h"

namespace AltinaEngine::Rhi {

    FRhiDevice::FRhiDevice(const FRhiDeviceDesc& desc, const FRhiAdapterDesc& adapterDesc)
        : mDesc(desc), mAdapterDesc(adapterDesc) {
        NormalizeDebugName();
        mQueueCaps.mSupportsGraphics = true;
    }

    auto FRhiDevice::GetDesc() const noexcept -> const FRhiDeviceDesc& { return mDesc; }

    auto FRhiDevice::GetAdapterDesc() const noexcept -> const FRhiAdapterDesc& {
        return mAdapterDesc;
    }

    auto FRhiDevice::GetDebugName() const noexcept -> FStringView {
        return mDesc.mDebugName.ToView();
    }

    void FRhiDevice::SetDebugName(FStringView name) {
        mDesc.mDebugName.Clear();
        if (!name.IsEmpty()) {
            mDesc.mDebugName.Append(name.Data(), name.Length());
        }
    }

    auto FRhiDevice::GetSupportedFeatures() const noexcept -> const FRhiSupportedFeatures& {
        return mSupportedFeatures;
    }

    auto FRhiDevice::GetSupportedLimits() const noexcept -> const FRhiSupportedLimits& {
        return mSupportedLimits;
    }

    auto FRhiDevice::GetQueueCapabilities() const noexcept -> const FRhiQueueCapabilities& {
        return mQueueCaps;
    }

    auto FRhiDevice::IsFeatureSupported(ERhiFeature feature) const noexcept -> bool {
        return mSupportedFeatures.IsSupported(feature);
    }

    auto FRhiDevice::GetQueue(ERhiQueueType type) const noexcept -> FRhiQueueRef {
        for (const auto& entry : mQueues) {
            if (entry.mType == type) {
                return entry.mQueue;
            }
        }

        return {};
    }

    void FRhiDevice::BeginFrame(u64 /*frameIndex*/) {}

    void FRhiDevice::EndFrame() {}

    void FRhiDevice::SetSupportedFeatures(const FRhiSupportedFeatures& features) noexcept {
        mSupportedFeatures = features;
    }

    void FRhiDevice::SetSupportedLimits(const FRhiSupportedLimits& limits) noexcept {
        mSupportedLimits = limits;
    }

    void FRhiDevice::SetQueueCapabilities(const FRhiQueueCapabilities& caps) noexcept {
        mQueueCaps = caps;
    }

    void FRhiDevice::RegisterQueue(ERhiQueueType type, FRhiQueueRef queue) {
        for (auto& entry : mQueues) {
            if (entry.mType == type) {
                entry.mQueue = AltinaEngine::Move(queue);
                return;
            }
        }

        FRhiQueueEntry entry;
        entry.mType  = type;
        entry.mQueue = AltinaEngine::Move(queue);
        mQueues.PushBack(AltinaEngine::Move(entry));
    }

    void FRhiDevice::ProcessResourceDeleteQueue(u64 completedSerial) {
        mResourceDeleteQueue.Process(completedSerial);
    }

    void FRhiDevice::FlushResourceDeleteQueue() {
        mResourceDeleteQueue.Flush();
    }

    void FRhiDevice::NormalizeDebugName() {
        if (!mDesc.mDebugName.IsEmptyString()) {
            return;
        }

        if (!mAdapterDesc.mName.IsEmptyString()) {
            mDesc.mDebugName = mAdapterDesc.mName;
            mDesc.mDebugName.Append(TEXT(" Device"));
        }
    }

} // namespace AltinaEngine::Rhi

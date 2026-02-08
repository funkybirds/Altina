#include "Rhi/RhiContext.h"

#include "Types/Traits.h"

namespace AltinaEngine::Rhi {
    namespace {
        auto NormalizeInitDesc(FRhiInitDesc desc) -> FRhiInitDesc {
            if (desc.mAppName.IsEmptyString()) {
                desc.mAppName.Assign(TEXT("AltinaEngine"));
            }

            if (desc.mEnableGpuValidation) {
                desc.mEnableValidation = true;
            }

            if (desc.mEnableValidation) {
                desc.mEnableDebugLayer = true;
            }

            return desc;
        }
    } // namespace

    auto FRhiContext::Init(const FRhiInitDesc& desc) -> bool {
        if (mIsInitialized) {
            return true;
        }

        mInitDesc = NormalizeInitDesc(desc);
        if (!InitializeBackend(mInitDesc)) {
            mIsInitialized = false;
            return false;
        }

        mIsInitialized = true;
        RefreshAdapters();
        return true;
    }

    void FRhiContext::Shutdown() {
        if (!mIsInitialized) {
            return;
        }

        mAdapters.Clear();
        mAdaptersDirty = true;

        ShutdownBackend();
        mIsInitialized = false;
    }

    auto FRhiContext::EnumerateAdapters() -> TVector<FRhiAdapterDesc> {
        TVector<FRhiAdapterDesc> result;
        if (!mIsInitialized) {
            return result;
        }

        if (mAdaptersDirty) {
            RefreshAdapters();
        }

        result.Reserve(mAdapters.Size());
        for (const auto& adapter : mAdapters) {
            if (adapter) {
                result.PushBack(adapter->GetDesc());
            }
        }

        return result;
    }

    auto FRhiContext::GetAdapterCount() const noexcept -> u32 {
        return static_cast<u32>(mAdapters.Size());
    }

    auto FRhiContext::GetAdapterDesc(u32 index) const noexcept -> const FRhiAdapterDesc* {
        const auto adapterIndex = static_cast<usize>(index);
        if (adapterIndex >= mAdapters.Size()) {
            return nullptr;
        }

        const auto& adapter = mAdapters[adapterIndex];
        return adapter ? &adapter->GetDesc() : nullptr;
    }

    auto FRhiContext::GetPreferredAdapterIndex() const noexcept -> u32 {
        return SelectAdapterIndex(mInitDesc.mAdapterPreference);
    }

    auto FRhiContext::CreateDevice(u32 adapterIndex, const FRhiDeviceDesc& deviceDesc)
        -> TShared<FRhiDevice> {
        if (!mIsInitialized) {
            return {};
        }

        if (mAdaptersDirty) {
            RefreshAdapters();
        }

        if (mAdapters.IsEmpty()) {
            return {};
        }

        u32 selectedIndex = adapterIndex;
        if ((selectedIndex == kRhiInvalidAdapterIndex)
            || (static_cast<usize>(selectedIndex) >= mAdapters.Size())) {
            selectedIndex = SelectAdapterIndex(mInitDesc.mAdapterPreference);
        }

        if (selectedIndex == kRhiInvalidAdapterIndex) {
            return {};
        }

        const auto resolvedIndex = static_cast<usize>(selectedIndex);
        if (resolvedIndex >= mAdapters.Size()) {
            return {};
        }

        const auto& adapter = mAdapters[resolvedIndex];
        if (!adapter) {
            return {};
        }

        return CreateDeviceInternal(adapter, deviceDesc);
    }

    void FRhiContext::RefreshAdapters() {
        TVector<TShared<FRhiAdapter>> adapters;
        EnumerateAdaptersInternal(adapters);
        mAdapters      = AltinaEngine::Move(adapters);
        mAdaptersDirty = false;
    }

    auto FRhiContext::SelectAdapterIndex(ERhiGpuPreference preference) const noexcept -> u32 {
        if (mAdapters.IsEmpty()) {
            return kRhiInvalidAdapterIndex;
        }

        u64 bestScore = 0ULL;
        u32 bestIndex = kRhiInvalidAdapterIndex;

        for (usize index = 0; index < mAdapters.Size(); ++index) {
            const auto& adapter = mAdapters[index];
            if (!adapter) {
                continue;
            }

            const u64 score = adapter->GetPreferenceScore(preference);
            if ((bestIndex == kRhiInvalidAdapterIndex) || (score > bestScore)) {
                bestScore = score;
                bestIndex = static_cast<u32>(index);
            }
        }

        return bestIndex;
    }

} // namespace AltinaEngine::Rhi

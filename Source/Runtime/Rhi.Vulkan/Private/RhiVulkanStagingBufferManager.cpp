#include "RhiVulkan/RhiVulkanStagingBufferManager.h"

#include "RhiVulkan/RhiVulkanDevice.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiStructs.h"
#include "Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Rhi {
    namespace {
        [[nodiscard]] auto ToLockMode(EVulkanStagingMapMode mode) noexcept -> ERhiBufferLockMode {
            switch (mode) {
                case EVulkanStagingMapMode::Write:
                    return ERhiBufferLockMode::Write;
                case EVulkanStagingMapMode::ReadWrite:
                    return ERhiBufferLockMode::ReadWrite;
                case EVulkanStagingMapMode::Read:
                default:
                    return ERhiBufferLockMode::Read;
            }
        }

        [[nodiscard]] auto HasAllAccess(ERhiCpuAccess existing, ERhiCpuAccess requested) noexcept
            -> bool {
            const u8 e = static_cast<u8>(existing);
            const u8 r = static_cast<u8>(requested);
            return (e & r) == r;
        }
    } // namespace

    void FVulkanStagingBufferManager::Init(FRhiVulkanDevice* device) {
        mEntries.Clear();
        mDevice = device;
    }

    void FVulkanStagingBufferManager::Reset() {
        // Keep entries for reuse; just mark them available again.
        for (auto& entry : mEntries) {
            entry.mInUse = false;
            if (entry.mLock.IsValid() && entry.mBuffer) {
                entry.mBuffer->Unlock(entry.mLock);
                entry.mLock = {};
            }
        }
    }

    auto FVulkanStagingBufferManager::Acquire(u64 sizeBytes, ERhiCpuAccess access)
        -> FVulkanStagingAllocation {
        if (mDevice == nullptr || sizeBytes == 0ULL || access == ERhiCpuAccess::None) {
            return {};
        }

        for (u32 i = 0; i < static_cast<u32>(mEntries.Size()); ++i) {
            auto& entry = mEntries[i];
            if (entry.mInUse) {
                continue;
            }
            if (entry.mSizeBytes < sizeBytes) {
                continue;
            }
            if (!HasAllAccess(entry.mCpuAccess, access)) {
                continue;
            }
            entry.mInUse = true;
            return { entry.mBuffer.Get(), entry.mSizeBytes, i };
        }

        FRhiBufferDesc desc{};
        desc.mSizeBytes = sizeBytes;
        desc.mUsage     = ERhiResourceUsage::Staging;
        desc.mCpuAccess = access;
        desc.mBindFlags = ERhiBufferBindFlags::None;

        FRhiBufferRef buffer = mDevice->CreateBuffer(desc);
        if (!buffer) {
            return {};
        }

        FStagingEntry entry{};
        entry.mBuffer    = buffer;
        entry.mSizeBytes = sizeBytes;
        entry.mCpuAccess = access;
        entry.mInUse     = true;

        const u32 index = static_cast<u32>(mEntries.Size());
        mEntries.PushBack(entry);
        return { buffer.Get(), sizeBytes, index };
    }

    void FVulkanStagingBufferManager::Release(const FVulkanStagingAllocation& allocation) {
        if (!allocation.IsValid()) {
            return;
        }
        if (allocation.mPoolIndex >= static_cast<u32>(mEntries.Size())) {
            return;
        }
        auto& entry = mEntries[allocation.mPoolIndex];
        if (entry.mLock.IsValid() && entry.mBuffer) {
            entry.mBuffer->Unlock(entry.mLock);
            entry.mLock = {};
        }
        entry.mInUse = false;
    }

    auto FVulkanStagingBufferManager::Map(
        const FVulkanStagingAllocation& allocation, EVulkanStagingMapMode mode) -> void* {
        if (!allocation.IsValid()) {
            return nullptr;
        }
        if (allocation.mPoolIndex >= static_cast<u32>(mEntries.Size())) {
            return nullptr;
        }

        auto& entry = mEntries[allocation.mPoolIndex];
        if (!entry.mInUse || entry.mBuffer.Get() != allocation.mBuffer) {
            return nullptr;
        }

        if (!entry.mLock.IsValid()) {
            const auto lockMode = ToLockMode(mode);
            entry.mLock         = entry.mBuffer->Lock(0ULL, entry.mSizeBytes, lockMode);
        }
        return entry.mLock.IsValid() ? entry.mLock.mData : nullptr;
    }

    void FVulkanStagingBufferManager::Unmap(const FVulkanStagingAllocation& allocation) {
        if (!allocation.IsValid()) {
            return;
        }
        if (allocation.mPoolIndex >= static_cast<u32>(mEntries.Size())) {
            return;
        }
        auto& entry = mEntries[allocation.mPoolIndex];
        if (entry.mLock.IsValid() && entry.mBuffer) {
            entry.mBuffer->Unlock(entry.mLock);
            entry.mLock = {};
        }
    }
} // namespace AltinaEngine::Rhi

#include "RhiVulkanBackend.h"
#include "RhiVulkan/RhiVulkanCommandList.h"
#include "RhiVulkan/RhiVulkanViewport.h"
#include "RhiVulkan/RhiVulkanDevice.h"

#include "Jobs/JobSystem.h"

using AltinaEngine::Move;
namespace AltinaEngine::Rhi {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    void FRhiVulkanCommandSubmitter::FSubmitQueue::Push(FSubmitWork&& work) {
        {
            FScopedLock lock(mMutex);
            mQueue.PushBack(Move(work));
        }
        mCond.NotifyOne();
    }

    auto FRhiVulkanCommandSubmitter::FSubmitQueue::WaitPop(FSubmitWork& out) -> bool {
        FScopedLock lock(mMutex);
        while (mQueue.IsEmpty()) {
            if (!mCond.Wait(mMutex, Core::Threading::kInfiniteWait)) {
                return false;
            }
        }
        out = Move(mQueue.Front());
        mQueue.PopFront();
        return true;
    }

    void FRhiVulkanCommandSubmitter::Start() {
        if (mRunning) {
            return;
        }
        mRunning = true;
        mThread = std::thread([this]() { ThreadMain(); });
    }

    void FRhiVulkanCommandSubmitter::Stop() {
        if (!mRunning) {
            return;
        }
        FSubmitWork work;
        work.mType = FSubmitWork::EType::Shutdown;
        mQueue.Push(Move(work));
        if (mThread.joinable()) {
            mThread.join();
        }
        mRunning = false;
    }

    void FRhiVulkanCommandSubmitter::Enqueue(FSubmitWork&& work) { mQueue.Push(Move(work)); }

    void FRhiVulkanCommandSubmitter::ThreadMain() {
        Core::Jobs::RegisterNamedThread(Core::Jobs::ENamedThread::RHI,
            "RhiCommandSubmitThread");
        while (true) {
            FSubmitWork work;
            if (!mQueue.WaitPop(work)) {
                continue;
            }
            if (work.mType == FSubmitWork::EType::Shutdown) {
                break;
            }
            if (work.mType == FSubmitWork::EType::WaitIdle) {
                if (work.mQueue) {
                    vkQueueWaitIdle(work.mQueue);
                }
                continue;
            }

            if (work.mType == FSubmitWork::EType::Present) {
                VkPresentInfoKHR present{};
                present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                present.swapchainCount     = 1;
                present.pSwapchains        = &work.mSwapchain;
                present.pImageIndices      = &work.mImageIndex;
                present.waitSemaphoreCount =
                    static_cast<u32>(work.mPresentWaitSemaphores.Size());
                present.pWaitSemaphores = work.mPresentWaitSemaphores.Data();
                vkQueuePresentKHR(work.mQueue, &present);
                continue;
            }

            VkTimelineSemaphoreSubmitInfo timelineInfo{};
            timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            if (!work.mWaitValues.IsEmpty()) {
                timelineInfo.waitSemaphoreValueCount =
                    static_cast<u32>(work.mWaitValues.Size());
                timelineInfo.pWaitSemaphoreValues = work.mWaitValues.Data();
            }
            if (!work.mSignalValues.IsEmpty()) {
                timelineInfo.signalSemaphoreValueCount =
                    static_cast<u32>(work.mSignalValues.Size());
                timelineInfo.pSignalSemaphoreValues = work.mSignalValues.Data();
            }

            VkSubmitInfo submit{};
            submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.pNext                = (timelineInfo.waitSemaphoreValueCount > 0
                            || timelineInfo.signalSemaphoreValueCount > 0)
                ? &timelineInfo
                : nullptr;
            submit.commandBufferCount   = static_cast<u32>(work.mCommandBuffers.Size());
            submit.pCommandBuffers      = work.mCommandBuffers.Data();
            submit.waitSemaphoreCount   = static_cast<u32>(work.mWaitSemaphores.Size());
            submit.pWaitSemaphores      = work.mWaitSemaphores.Data();
            submit.pWaitDstStageMask    = work.mWaitStages.Data();
            submit.signalSemaphoreCount = static_cast<u32>(work.mSignalSemaphores.Size());
            submit.pSignalSemaphores    = work.mSignalSemaphores.Data();

            vkQueueSubmit(work.mQueue, 1, &submit, work.mFence);
        }
        Core::Jobs::UnregisterNamedThread(Core::Jobs::ENamedThread::RHI);
    }

    FRhiVulkanFence::FRhiVulkanFence(u64 initialValue) : mValue(initialValue) {}

    auto FRhiVulkanFence::GetCompletedValue() const noexcept -> u64 { return mValue; }
    void FRhiVulkanFence::SignalCPU(u64 value) { mValue = value; }
    void FRhiVulkanFence::WaitCPU(u64 value) { mValue = value; }
    void FRhiVulkanFence::Reset(u64 value) { mValue = value; }

    FRhiVulkanSemaphore::FRhiVulkanSemaphore(VkDevice device, bool timeline, u64 initialValue)
        : mDevice(device), mIsTimeline(timeline) {
        VkSemaphoreTypeCreateInfo typeInfo{};
        typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeInfo.semaphoreType =
            timeline ? VK_SEMAPHORE_TYPE_TIMELINE : VK_SEMAPHORE_TYPE_BINARY;
        typeInfo.initialValue = initialValue;

        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = timeline ? &typeInfo : nullptr;

        vkCreateSemaphore(device, &info, nullptr, &mSemaphore);
    }

    FRhiVulkanSemaphore::~FRhiVulkanSemaphore() {
        if (mDevice && mSemaphore) {
            vkDestroySemaphore(mDevice, mSemaphore, nullptr);
        }
    }

    auto FRhiVulkanSemaphore::IsTimeline() const noexcept -> bool { return mIsTimeline; }
    auto FRhiVulkanSemaphore::GetCurrentValue() const noexcept -> u64 {
        if (!mIsTimeline || !mDevice || !mSemaphore) {
            return 0ULL;
        }
        u64 value = 0ULL;
        vkGetSemaphoreCounterValue(mDevice, mSemaphore, &value);
        return value;
    }

    auto FRhiVulkanSemaphore::GetNativeSemaphore() const noexcept -> VkSemaphore {
        return mSemaphore;
    }

    FRhiVulkanQueue::FRhiVulkanQueue(ERhiQueueType type, VkQueue queue,
        FRhiVulkanCommandSubmitter* submitter, FRhiVulkanDevice* device)
        : FRhiQueue(type), mQueue(queue), mSubmitter(submitter), mDevice(device) {}

    void FRhiVulkanQueue::Submit(const FRhiSubmitInfo& info) {
        if (!mSubmitter || !mQueue) {
            return;
        }

        FSubmitWork work;
        work.mType  = FSubmitWork::EType::Submit;
        work.mQueue = mQueue;

        if (info.mCommandLists && info.mCommandListCount > 0U) {
            work.mCommandBuffers.Reserve(info.mCommandListCount);
            for (u32 i = 0; i < info.mCommandListCount; ++i) {
                auto* list = info.mCommandLists[i];
                auto* vkList = static_cast<FRhiVulkanCommandList*>(list);
                if (vkList && vkList->GetNativeCommandBuffer()) {
                    work.mCommandBuffers.PushBack(vkList->GetNativeCommandBuffer());
                }
            }
        }

        VkSemaphore pendingAcquire = VK_NULL_HANDLE;
        VkSemaphore pendingRenderComplete = VK_NULL_HANDLE;
        if (mDevice) {
            pendingAcquire = mDevice->ConsumePendingAcquireSemaphore();
            pendingRenderComplete = mDevice->ConsumePendingRenderCompleteSemaphore();
        }

        if (pendingAcquire != VK_NULL_HANDLE) {
            work.mWaitSemaphores.PushBack(pendingAcquire);
            work.mWaitStages.PushBack(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        }

        if (info.mWaits && info.mWaitCount > 0U) {
            work.mWaitSemaphores.Reserve(work.mWaitSemaphores.Size() + info.mWaitCount);
            work.mWaitStages.Reserve(work.mWaitStages.Size() + info.mWaitCount);
            work.mWaitValues.Reserve(work.mWaitValues.Size() + info.mWaitCount);
            for (u32 i = 0; i < info.mWaitCount; ++i) {
                const auto& wait = info.mWaits[i];
                if (!wait.mSemaphore) {
                    continue;
                }
                auto* vkSem = static_cast<FRhiVulkanSemaphore*>(wait.mSemaphore);
                if (!vkSem) {
                    continue;
                }
                work.mWaitSemaphores.PushBack(vkSem->GetNativeSemaphore());
                work.mWaitStages.PushBack(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                if (vkSem->IsTimeline()) {
                    work.mWaitValues.PushBack(wait.mValue);
                }
            }
        }

        if (info.mSignals && info.mSignalCount > 0U) {
            work.mSignalSemaphores.Reserve(info.mSignalCount + (pendingRenderComplete ? 1 : 0));
            work.mSignalValues.Reserve(info.mSignalCount);
            for (u32 i = 0; i < info.mSignalCount; ++i) {
                const auto& signal = info.mSignals[i];
                if (!signal.mSemaphore) {
                    continue;
                }
                auto* vkSem = static_cast<FRhiVulkanSemaphore*>(signal.mSemaphore);
                if (!vkSem) {
                    continue;
                }
                work.mSignalSemaphores.PushBack(vkSem->GetNativeSemaphore());
                if (vkSem->IsTimeline()) {
                    work.mSignalValues.PushBack(signal.mValue);
                }
            }
        }

        if (pendingRenderComplete != VK_NULL_HANDLE) {
            work.mSignalSemaphores.PushBack(pendingRenderComplete);
        }

        if (info.mFence) {
            info.mFence->SignalCPU(info.mFenceValue);
        }

        mSubmitter->Enqueue(Move(work));
    }

    void FRhiVulkanQueue::Signal(FRhiFence* fence, u64 value) {
        if (fence) {
            fence->SignalCPU(value);
        }
    }

    void FRhiVulkanQueue::Wait(FRhiFence* fence, u64 value) {
        if (fence) {
            fence->WaitCPU(value);
        }
    }

    void FRhiVulkanQueue::WaitIdle() {
        if (!mSubmitter || !mQueue) {
            return;
        }
        FSubmitWork work;
        work.mType  = FSubmitWork::EType::WaitIdle;
        work.mQueue = mQueue;
        mSubmitter->Enqueue(Move(work));
    }

    void FRhiVulkanQueue::Present(const FRhiPresentInfo& info) {
        if (!info.mViewport || !mSubmitter) {
            return;
        }
        auto* viewport = static_cast<FRhiVulkanViewport*>(info.mViewport);
        if (!viewport) {
            return;
        }

        FSubmitWork work;
        work.mType = FSubmitWork::EType::Present;
        work.mQueue = mQueue;
        work.mSwapchain = viewport->GetNativeSwapchain();
        work.mImageIndex = viewport->GetCurrentImageIndex();
        VkSemaphore waitSem = viewport->GetRenderCompleteSemaphore();
        if (waitSem) {
            work.mPresentWaitSemaphores.PushBack(waitSem);
        }
        mSubmitter->Enqueue(Move(work));
    }
#endif
} // namespace AltinaEngine::Rhi

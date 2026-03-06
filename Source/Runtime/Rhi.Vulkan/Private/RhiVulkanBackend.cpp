#include "RhiVulkanBackend.h"
#include "RhiVulkan/RhiVulkanCommandList.h"
#include "RhiVulkan/RhiVulkanViewport.h"
#include "RhiVulkan/RhiVulkanDevice.h"
#include "RhiVulkan/RhiVulkanResources.h"

#include "Jobs/JobSystem.h"
#if AE_PLATFORM_WIN
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif

using AltinaEngine::Move;
namespace AltinaEngine::Rhi {
    namespace {
        class FQueueApiLock final {
        public:
            void Lock() noexcept {
#if AE_PLATFORM_WIN
                if (mMutexHandle == nullptr) {
                    mMutexHandle =
                        ::CreateMutexW(nullptr, FALSE, L"AltinaEngine.Rhi.Vulkan.QueueApiMutex");
                }
                if (mMutexHandle != nullptr) {
                    (void)::WaitForSingleObject(mMutexHandle, INFINITE);
                }
#else
                mMutex.Lock();
#endif
            }

            void Unlock() noexcept {
#if AE_PLATFORM_WIN
                if (mMutexHandle != nullptr) {
                    (void)::ReleaseMutex(mMutexHandle);
                }
#else
                mMutex.Unlock();
#endif
            }

            ~FQueueApiLock() noexcept {
#if AE_PLATFORM_WIN
                if (mMutexHandle != nullptr) {
                    (void)::CloseHandle(mMutexHandle);
                    mMutexHandle = nullptr;
                }
#endif
            }

        private:
#if AE_PLATFORM_WIN
            HANDLE mMutexHandle = nullptr;
#else
            Core::Threading::FMutex mMutex;
#endif
        };

        class FQueueApiScopedLock final {
        public:
            explicit FQueueApiScopedLock(FQueueApiLock& lock) noexcept : mLock(lock) {
                mLock.Lock();
            }
            ~FQueueApiScopedLock() noexcept { mLock.Unlock(); }

            FQueueApiScopedLock(const FQueueApiScopedLock&)                    = delete;
            auto operator=(const FQueueApiScopedLock&) -> FQueueApiScopedLock& = delete;

        private:
            FQueueApiLock& mLock;
        };

        FQueueApiLock gQueueSubmitMutex;
    } // namespace

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
        mThread  = std::thread([this]() { ThreadMain(); });
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
        Core::Jobs::RegisterNamedThread(Core::Jobs::ENamedThread::RHI, "RhiCommandSubmitThread");
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
                    FQueueApiScopedLock lock(gQueueSubmitMutex);
                    vkQueueWaitIdle(work.mQueue);
                }
                if (work.mCompletion) {
                    work.mCompletion->Signal();
                }
                continue;
            }

            if (work.mType == FSubmitWork::EType::Present) {
                VkPresentInfoKHR present{};
                present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                present.swapchainCount     = 1;
                present.pSwapchains        = &work.mSwapchain;
                present.pImageIndices      = &work.mImageIndex;
                present.waitSemaphoreCount = static_cast<u32>(work.mPresentWaitSemaphores.Size());
                present.pWaitSemaphores    = work.mPresentWaitSemaphores.Data();
                FQueueApiScopedLock lock(gQueueSubmitMutex);
                vkQueuePresentKHR(work.mQueue, &present);
                if (work.mCompletion) {
                    work.mCompletion->Signal();
                }
                continue;
            }

            VkTimelineSemaphoreSubmitInfo timelineInfo{};
            timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            if (!work.mWaitValues.IsEmpty()) {
                timelineInfo.waitSemaphoreValueCount = static_cast<u32>(work.mWaitValues.Size());
                timelineInfo.pWaitSemaphoreValues    = work.mWaitValues.Data();
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

            {
                FQueueApiScopedLock lock(gQueueSubmitMutex);
                vkQueueSubmit(work.mQueue, 1, &submit, work.mFence);
            }

            if (work.mCompletion) {
                work.mCompletion->Signal();
            }
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
        typeInfo.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeInfo.semaphoreType = timeline ? VK_SEMAPHORE_TYPE_TIMELINE : VK_SEMAPHORE_TYPE_BINARY;
        typeInfo.initialValue  = initialValue;

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

        auto addWait = [&](FRhiSemaphore* semaphore, u64 value) {
            if (!semaphore) {
                return;
            }
            auto* vkSem = static_cast<FRhiVulkanSemaphore*>(semaphore);
            if (!vkSem) {
                return;
            }
            VkSemaphore native = vkSem->GetNativeSemaphore();
            if (native == VK_NULL_HANDLE) {
                return;
            }
            const bool isTimeline = vkSem->IsTimeline();
            for (usize i = 0; i < work.mWaitSemaphores.Size(); ++i) {
                if (work.mWaitSemaphores[i] != native) {
                    continue;
                }
                if (!isTimeline) {
                    return;
                }
                const u64 existingValue =
                    (i < work.mWaitValues.Size()) ? work.mWaitValues[i] : 0ULL;
                if (existingValue >= value) {
                    return;
                }
                if (i < work.mWaitValues.Size()) {
                    work.mWaitValues[i] = value;
                }
                return;
            }
            work.mWaitSemaphores.PushBack(native);
            work.mWaitStages.PushBack(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            if (isTimeline) {
                work.mWaitValues.PushBack(value);
            }
        };

        if (info.mCommandLists && info.mCommandListCount > 0U) {
            work.mCommandBuffers.Reserve(info.mCommandListCount);
            for (u32 i = 0; i < info.mCommandListCount; ++i) {
                auto* list   = info.mCommandLists[i];
                auto* vkList = static_cast<FRhiVulkanCommandList*>(list);
                if (vkList && vkList->GetNativeCommandBuffer()) {
                    work.mCommandBuffers.PushBack(vkList->GetNativeCommandBuffer());
                }
                if (vkList) {
                    for (auto* texture : vkList->GetTouchedTextures()) {
                        if (!texture || !texture->HasPendingUpload()) {
                            continue;
                        }
                        FRhiSemaphore* pendingSemaphore = nullptr;
                        u64            pendingValue     = 0ULL;
                        if (texture->GetPendingUpload(pendingSemaphore, pendingValue)) {
                            addWait(pendingSemaphore, pendingValue);
                            texture->ClearPendingUpload();
                        }
                    }
                }
            }
        }

        VkSemaphore pendingAcquire = VK_NULL_HANDLE;
        if (mDevice) {
            pendingAcquire = mDevice->ConsumePendingAcquireSemaphore();
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
            work.mSignalSemaphores.Reserve(info.mSignalCount);
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

        if (!work.mWaitValues.IsEmpty() && work.mWaitValues.Size() < work.mWaitSemaphores.Size()) {
            const usize missing = work.mWaitSemaphores.Size() - work.mWaitValues.Size();
            for (usize i = 0; i < missing; ++i) {
                work.mWaitValues.PushBack(0ULL);
            }
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
        FSubmitWork::FCompletion completion{};
        work.mCompletion = &completion;
        mSubmitter->Enqueue(Move(work));
        completion.Wait();
    }

    void FRhiVulkanQueue::Present(const FRhiPresentInfo& info) {
        if (!info.mViewport || !mSubmitter) {
            return;
        }
        auto* viewport = static_cast<FRhiVulkanViewport*>(info.mViewport);
        if (!viewport) {
            return;
        }
        if (!viewport->IsImageAcquired()) {
            // Skip invalid presents when swapchain image acquisition failed this frame.
            return;
        }

        // Make present wait on a signal that is submitted after all prior queue work in this
        // frame, so multi-submit frames (scene + overlays) don't present too early.
        VkSemaphore renderComplete = VK_NULL_HANDLE;
        if (mDevice) {
            renderComplete = mDevice->ConsumePendingRenderCompleteSemaphore();
        }
        if (renderComplete != VK_NULL_HANDLE) {
            FSubmitWork signalWork;
            signalWork.mType  = FSubmitWork::EType::Submit;
            signalWork.mQueue = mQueue;
            signalWork.mSignalSemaphores.PushBack(renderComplete);
            mSubmitter->Enqueue(Move(signalWork));
        }

        FSubmitWork work;
        work.mType       = FSubmitWork::EType::Present;
        work.mQueue      = mQueue;
        work.mSwapchain  = viewport->GetNativeSwapchain();
        work.mImageIndex = viewport->GetCurrentImageIndex();
        if (renderComplete != VK_NULL_HANDLE) {
            work.mPresentWaitSemaphores.PushBack(renderComplete);
        }
        mSubmitter->Enqueue(Move(work));
        viewport->Present(info);
    }
} // namespace AltinaEngine::Rhi

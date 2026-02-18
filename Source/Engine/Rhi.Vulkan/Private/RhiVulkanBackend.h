#pragma once

#include "RhiVulkanInternal.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiSemaphore.h"
#include "Container/Vector.h"
#include "Container/Deque.h"
#include "Threading/ConditionVariable.h"
#include "Threading/Mutex.h"

#include <thread>

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::TVector;
    using Container::TDeque;
    using Core::Threading::FMutex;
    using Core::Threading::FScopedLock;
    using Core::Threading::FConditionVariable;

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    class FRhiVulkanCommandList;
    class FRhiVulkanViewport;
    class FRhiVulkanDevice;

    struct FSubmitWork {
        enum class EType {
            Submit,
            Present,
            WaitIdle,
            Shutdown
        };

        EType mType = EType::Submit;
        VkQueue mQueue = VK_NULL_HANDLE;

        TVector<VkCommandBuffer>      mCommandBuffers;
        TVector<VkSemaphore>          mWaitSemaphores;
        TVector<VkPipelineStageFlags> mWaitStages;
        TVector<u64>                  mWaitValues;
        TVector<VkSemaphore>          mSignalSemaphores;
        TVector<u64>                  mSignalValues;
        VkFence                       mFence = VK_NULL_HANDLE;

        VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
        u32            mImageIndex = 0U;
        TVector<VkSemaphore> mPresentWaitSemaphores;
    };

    class FRhiVulkanCommandSubmitter {
    public:
        void Start();
        void Stop();
        void Enqueue(FSubmitWork&& work);

    private:
        class FSubmitQueue {
        public:
            void Push(FSubmitWork&& work);
            auto WaitPop(FSubmitWork& out) -> bool;

        private:
            TDeque<FSubmitWork> mQueue;
            FMutex mMutex;
            FConditionVariable mCond;
        };

        void ThreadMain();

        bool mRunning = false;
        std::thread mThread;
        FSubmitQueue mQueue;
    };

    class FRhiVulkanFence final : public FRhiFence {
    public:
        explicit FRhiVulkanFence(u64 initialValue);

        [[nodiscard]] auto GetCompletedValue() const noexcept -> u64 override;
        void               SignalCPU(u64 value) override;
        void               WaitCPU(u64 value) override;
        void               Reset(u64 value) override;

    private:
        u64 mValue = 0ULL;
    };

    class FRhiVulkanSemaphore final : public FRhiSemaphore {
    public:
        FRhiVulkanSemaphore(VkDevice device, bool timeline, u64 initialValue);
        ~FRhiVulkanSemaphore() override;

        [[nodiscard]] auto IsTimeline() const noexcept -> bool override;
        [[nodiscard]] auto GetCurrentValue() const noexcept -> u64 override;
        [[nodiscard]] auto GetNativeSemaphore() const noexcept -> VkSemaphore;

    private:
        VkDevice    mDevice = VK_NULL_HANDLE;
        VkSemaphore mSemaphore = VK_NULL_HANDLE;
        bool        mIsTimeline = false;
    };

    class FRhiVulkanQueue final : public FRhiQueue {
    public:
        FRhiVulkanQueue(ERhiQueueType type, VkQueue queue, FRhiVulkanCommandSubmitter* submitter,
            FRhiVulkanDevice* device);

        void Submit(const FRhiSubmitInfo& info) override;
        void Signal(FRhiFence* fence, u64 value) override;
        void Wait(FRhiFence* fence, u64 value) override;
        void WaitIdle() override;
        void Present(const FRhiPresentInfo& info) override;

    private:
        VkQueue mQueue = VK_NULL_HANDLE;
        FRhiVulkanCommandSubmitter* mSubmitter = nullptr;
        FRhiVulkanDevice* mDevice = nullptr;
    };
#endif
} // namespace AltinaEngine::Rhi

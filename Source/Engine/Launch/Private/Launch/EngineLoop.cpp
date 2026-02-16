#include "Launch/EngineLoop.h"

#include "Input/InputMessageHandler.h"
#include "Input/InputSystem.h"

#include "Console/ConsoleVariable.h"
#include "Logging/Log.h"
#include "Threading/RenderingThread.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiStructs.h"

#if AE_PLATFORM_WIN
    #include "Application/Windows/WindowsApplication.h"
    #include "RhiD3D11/RhiD3D11Context.h"
#else
    #include "RhiMock/RhiMockContext.h"
#endif

using AltinaEngine::Move;
using AltinaEngine::Core::Container::MakeUnique;
using AltinaEngine::Core::Container::MakeUniqueAs;
namespace AltinaEngine::Launch {
    namespace Container = Core::Container;
    namespace {
        auto GetRenderThreadLagFrames() noexcept -> u32 {
            int value = RenderCore::gRenderingThreadLagFrames.Get();
            if (value < 0)
                value = 0;
            return static_cast<u32>(value);
        }
    } // namespace

    FEngineLoop::FEngineLoop(const FStartupParameters& InStartupParameters)
        : mStartupParameters(InStartupParameters) {}

    FEngineLoop::~FEngineLoop() = default;

    auto FEngineLoop::PreInit() -> bool {
        Core::Jobs::FJobSystem::RegisterGameThread();
        if (mApplication) {
            return true;
        }

        if (!mInputSystem) {
            mInputSystem = MakeUnique<Input::FInputSystem>();
        }

        if (!mAppMessageHandler && mInputSystem) {
            mAppMessageHandler = MakeUnique<Input::FInputMessageHandler>(*mInputSystem);
        }

#if AE_PLATFORM_WIN
        mApplication = MakeUniqueAs<Application::FApplication, Application::FWindowsApplication>(
            mStartupParameters);
#else
        LogError(TEXT("FEngineLoop PreInit failed: no platform application available."));
        return false;
#endif

        if (!mApplication) {
            LogError(TEXT("FEngineLoop PreInit failed: application allocation failed."));
            return false;
        }

        if (mAppMessageHandler) {
            mApplication->RegisterMessageHandler(mAppMessageHandler.Get());
        }

        mApplication->Initialize();
        if (!mApplication->IsRunning()) {
            LogError(TEXT("FEngineLoop PreInit failed: application did not start."));
            return false;
        }

        mIsRunning = true;
        return true;
    }

    auto FEngineLoop::Init() -> bool {
        if (!mApplication) {
            LogError(TEXT("FEngineLoop Init failed: application is not initialized."));
            return false;
        }

        if (mRhiContext) {
            return true;
        }

        if (!mAssetReady) {
            mAssetManager.SetRegistry(&mAssetRegistry);
            mAssetManager.RegisterLoader(&mAudioLoader);
            mAssetManager.RegisterLoader(&mMaterialLoader);
            mAssetManager.RegisterLoader(&mMeshLoader);
            mAssetManager.RegisterLoader(&mTexture2DLoader);
            mAssetReady = true;
        }

#if AE_PLATFORM_WIN
        mRhiContext = MakeUniqueAs<Rhi::FRhiContext, Rhi::FRhiD3D11Context>();
#else
        mRhiContext = MakeUniqueAs<Rhi::FRhiContext, Rhi::FRhiMockContext>();
#endif

        if (!mRhiContext) {
            LogError(TEXT("FEngineLoop Init failed: RHI context allocation failed."));
            return false;
        }

        Rhi::FRhiInitDesc initDesc{};
        initDesc.mAppName.Assign(TEXT("AltinaEngine"));
#if AE_PLATFORM_WIN
        initDesc.mBackend = Rhi::ERhiBackend::DirectX11;
#endif

        mRhiDevice = Rhi::RHIInit(*mRhiContext, initDesc);
        if (!mRhiDevice) {
            LogError(TEXT("FEngineLoop Init failed: RHIInit failed."));
            return false;
        }

        auto* window = mApplication->GetMainWindow();
        if (!window) {
            LogError(TEXT("FEngineLoop Init failed: main window is missing."));
            return false;
        }

        const auto            extent = window->GetSize();
        Rhi::FRhiViewportDesc viewportDesc{};
        viewportDesc.mDebugName.Assign(TEXT("MainViewport"));
        viewportDesc.mWidth        = extent.mWidth;
        viewportDesc.mHeight       = extent.mHeight;
        viewportDesc.mNativeHandle = window->GetNativeHandle();
        mMainViewport              = Rhi::RHICreateViewport(viewportDesc);
        if (!mMainViewport) {
            LogError(TEXT("FEngineLoop Init failed: viewport creation failed."));
            return false;
        }

        mViewportWidth  = extent.mWidth;
        mViewportHeight = extent.mHeight;

        if (!mRenderingThread) {
            mRenderingThread = MakeUnique<RenderCore::FRenderingThread>();
        }
        if (mRenderingThread && !mRenderingThread->IsRunning()) {
            mRenderingThread->Start();
        }

        return true;
    }

    void FEngineLoop::Tick(float InDeltaTime) {
        if (!mIsRunning) {
            return;
        }

        Core::Jobs::FJobSystem::ProcessGameThreadJobs();

        if (mInputSystem) {
            mInputSystem->ClearFrameState();
        }

        if (mApplication) {
            mApplication->Tick(InDeltaTime);
            if (!mApplication->IsRunning()) {
                mIsRunning = false;
            }
        }

        if (!mIsRunning || !mRhiDevice) {
            return;
        }

        u32  width        = 0U;
        u32  height       = 0U;
        bool shouldResize = false;

        if (mApplication) {
            auto* window = mApplication->GetMainWindow();
            if (window != nullptr) {
                const auto extent = window->GetSize();
                width             = extent.mWidth;
                height            = extent.mHeight;
                if (width > 0U && height > 0U) {
                    if (width != mViewportWidth || height != mViewportHeight) {
                        mViewportWidth  = width;
                        mViewportHeight = height;
                        shouldResize    = true;
                    }
                }
            }
        }

        const u64 frameIndex = ++mFrameIndex;
        auto      device     = mRhiDevice;
        auto      viewport   = mMainViewport;
        auto      callback   = mRenderCallback;

        LogInfo(TEXT("GameThread Frame {}"), frameIndex);

        auto handle = RenderCore::EnqueueRenderTask(Container::FString(TEXT("RenderFrame")),
            [device, viewport, callback, frameIndex, width, height, shouldResize]() mutable -> void {
                if (!device)
                    return;

                device->BeginFrame(frameIndex);

                if (viewport && width > 0U && height > 0U) {
                    if (shouldResize) {
                        viewport->Resize(width, height);
                    }

                    if (callback) {
                        callback(*device, *viewport, width, height);
                    }

                    const auto queue = device->GetQueue(Rhi::ERhiQueueType::Graphics);
                    if (queue) {
                        Rhi::FRhiPresentInfo presentInfo{};
                        presentInfo.mViewport     = viewport.Get();
                        presentInfo.mSyncInterval = 1U;
                        queue->Present(presentInfo);
                    }
                }

                device->EndFrame();

                LogInfo(TEXT("RenderThread Frame {}"), frameIndex);
            });
        if (handle.IsValid()) {
            mPendingRenderFrames.Push(handle);
            EnforceRenderLag(GetRenderThreadLagFrames());
        }

    }

    void FEngineLoop::Exit() {
        mIsRunning = false;

        FlushRenderFrames();
        if (mRenderingThread) {
            mRenderingThread->Stop();
            mRenderingThread.Reset();
        }

        if (mAssetReady) {
            mAssetManager.ClearCache();
            mAssetManager.UnregisterLoader(&mTexture2DLoader);
            mAssetManager.UnregisterLoader(&mMeshLoader);
            mAssetManager.UnregisterLoader(&mMaterialLoader);
            mAssetManager.UnregisterLoader(&mAudioLoader);
            mAssetManager.SetRegistry(nullptr);
            mAssetReady = false;
        }

        if (mMainViewport) {
            mMainViewport->SetDeleteQueue(nullptr);
            mMainViewport.Reset();
        }
        if (mRhiDevice) {
            mRhiDevice->FlushResourceDeleteQueue();
        }
        mRhiDevice.Reset();
        if (mRhiContext) {
            Rhi::RHIExit(*mRhiContext);
            mRhiContext.Reset();
        }

        if (mApplication && mAppMessageHandler) {
            mApplication->UnregisterMessageHandler(mAppMessageHandler.Get());
        }

        if (mApplication) {
            mApplication->Shutdown();
            mApplication.Reset();
        }

        if (mAppMessageHandler) {
            mAppMessageHandler.Reset();
        }

        if (mInputSystem) {
            mInputSystem.Reset();
        }
    }

    void FEngineLoop::SetRenderCallback(FRenderCallback callback) {
        FlushRenderFrames();
        mRenderCallback = Move(callback);
    }

    auto FEngineLoop::GetInputSystem() const noexcept -> const Input::FInputSystem* {
        return mInputSystem.Get();
    }

    void FEngineLoop::FlushRenderFrames() {
        while (!mPendingRenderFrames.IsEmpty()) {
            auto handle = mPendingRenderFrames.Front();
            mPendingRenderFrames.Pop();
            Core::Jobs::FJobSystem::Wait(handle);
        }
    }

    void FEngineLoop::EnforceRenderLag(u32 maxLagFrames) {
        while (mPendingRenderFrames.Size() > maxLagFrames) {
            auto handle = mPendingRenderFrames.Front();
            mPendingRenderFrames.Pop();
            Core::Jobs::FJobSystem::Wait(handle);
        }
    }
} // namespace AltinaEngine::Launch

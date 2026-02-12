#include "Launch/EngineLoop.h"

#include "Logging/Log.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiStructs.h"

#if AE_PLATFORM_WIN
    #include "Application/Windows/WindowsApplication.h"
    #include "RhiD3D11/RhiD3D11Context.h"
#else
    #include "RhiMock/RhiMockContext.h"
#endif

namespace AltinaEngine::Launch {
    FEngineLoop::FEngineLoop(const FStartupParameters& InStartupParameters)
        : mStartupParameters(InStartupParameters) {}

    auto FEngineLoop::PreInit() -> bool {
        if (mApplication) {
            return true;
        }

#if AE_PLATFORM_WIN
        mApplication = Core::Container::MakeUniqueAs<Application::FApplication,
            Application::FWindowsApplication>(mStartupParameters);
#else
        LogError(TEXT("FEngineLoop PreInit failed: no platform application available."));
        return false;
#endif

        if (!mApplication) {
            LogError(TEXT("FEngineLoop PreInit failed: application allocation failed."));
            return false;
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
            mAssetManager.RegisterLoader(&mMeshLoader);
            mAssetManager.RegisterLoader(&mTexture2DLoader);
            mAssetReady = true;
        }

#if AE_PLATFORM_WIN
        mRhiContext = Core::Container::MakeUniqueAs<Rhi::FRhiContext, Rhi::FRhiD3D11Context>();
#else
        mRhiContext = Core::Container::MakeUniqueAs<Rhi::FRhiContext, Rhi::FRhiMockContext>();
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

        return true;
    }

    void FEngineLoop::Tick(float InDeltaTime) {
        if (!mIsRunning) {
            return;
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

        mRhiDevice->BeginFrame(++mFrameIndex);

        if (mMainViewport && mApplication) {
            auto* window = mApplication->GetMainWindow();
            if (window != nullptr) {
                const auto extent = window->GetSize();
                if (extent.mWidth > 0U && extent.mHeight > 0U) {
                    if (extent.mWidth != mViewportWidth || extent.mHeight != mViewportHeight) {
                        mMainViewport->Resize(extent.mWidth, extent.mHeight);
                        mViewportWidth  = extent.mWidth;
                        mViewportHeight = extent.mHeight;
                    }

                    if (mRenderCallback && mMainViewport && mRhiDevice) {
                        mRenderCallback(
                            *mRhiDevice, *mMainViewport, mViewportWidth, mViewportHeight);
                    }

                    const auto queue = mRhiDevice->GetQueue(Rhi::ERhiQueueType::Graphics);
                    if (queue) {
                        Rhi::FRhiPresentInfo presentInfo{};
                        presentInfo.mViewport     = mMainViewport.Get();
                        presentInfo.mSyncInterval = 1U;
                        queue->Present(presentInfo);
                    }
                }
            }
        }

        mRhiDevice->EndFrame();
    }

    void FEngineLoop::Exit() {
        mIsRunning = false;

        if (mAssetReady) {
            mAssetManager.ClearCache();
            mAssetManager.UnregisterLoader(&mTexture2DLoader);
            mAssetManager.UnregisterLoader(&mMeshLoader);
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

        if (mApplication) {
            mApplication->Shutdown();
            mApplication.Reset();
        }
    }

    void FEngineLoop::SetRenderCallback(FRenderCallback callback) {
        mRenderCallback = AltinaEngine::Move(callback);
    }
} // namespace AltinaEngine::Launch

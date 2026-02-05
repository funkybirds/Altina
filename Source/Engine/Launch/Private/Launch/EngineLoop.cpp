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
        mApplication.reset(new Application::FWindowsApplication(mStartupParameters));
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

#if AE_PLATFORM_WIN
        mRhiContext.reset(new Rhi::FRhiD3D11Context());
#else
        mRhiContext.reset(new Rhi::FRhiMockContext());
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
    }

    void FEngineLoop::Exit() {
        mIsRunning = false;

        mRhiDevice.Reset();
        if (mRhiContext) {
            Rhi::RHIExit(*mRhiContext);
            mRhiContext.reset();
        }

        if (mApplication) {
            mApplication->Shutdown();
            mApplication.reset();
        }
    }
} // namespace AltinaEngine::Launch

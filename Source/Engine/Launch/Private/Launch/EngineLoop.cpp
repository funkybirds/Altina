#include "Launch/EngineLoop.h"

#include "Input/InputMessageHandler.h"
#include "Input/InputSystem.h"
#include "Engine/GameScene/ScriptComponent.h"

#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
    #include "Scripting/ScriptSystemCoreCLR.h"
#endif

#include "Console/ConsoleVariable.h"
#include "Logging/Log.h"
#include "Threading/RenderingThread.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiStructs.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#if AE_PLATFORM_WIN
#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
    #ifdef TEXT
        #undef TEXT
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #ifdef TEXT
        #undef TEXT
    #endif
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        #define TEXT(str) L##str
    #else
        #define TEXT(str) str
    #endif
#endif
    #include "Application/Windows/WindowsApplication.h"
    #include "RhiD3D11/RhiD3D11Context.h"
#elif AE_PLATFORM_MACOS
#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
    #include <mach-o/dyld.h>
    #include <unistd.h>
#endif
#else
    #include "RhiMock/RhiMockContext.h"
#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
    #include <unistd.h>
#endif
#endif

using AltinaEngine::Move;
using AltinaEngine::Core::Container::MakeUnique;
using AltinaEngine::Core::Container::MakeUniqueAs;
using AltinaEngine::Core::Logging::LogWarningCat;
namespace AltinaEngine::Launch {
    namespace Container = Core::Container;
    namespace {
        auto GetRenderThreadLagFrames() noexcept -> u32 {
            int value = RenderCore::gRenderingThreadLagFrames.Get();
            if (value < 0)
                value = 0;
            return static_cast<u32>(value);
        }

#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        constexpr auto kScriptingCategory = TEXT("Scripting.CoreCLR");
        constexpr auto kManagedRuntimeConfig = TEXT("AltinaEngine.Managed.runtimeconfig.json");
        constexpr auto kManagedAssembly = TEXT("AltinaEngine.Managed.dll");
        constexpr auto kManagedType =
            TEXT("AltinaEngine.Managed.ManagedBootstrap, AltinaEngine.Managed");
        constexpr auto kManagedStartupMethod = TEXT("Startup");
        constexpr auto kManagedStartupDelegate =
            TEXT("AltinaEngine.Managed.ManagedStartupDelegate, AltinaEngine.Managed");

        struct FManagedPathResolve {
            std::filesystem::path mPath;
            bool                  mExists = false;
        };

        auto GetExecutableDir() -> std::filesystem::path {
#if AE_PLATFORM_WIN
            std::wstring buffer(260, L'\0');
            DWORD        length = 0;
            while (true) {
                length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (length == 0) {
                    return {};
                }
                if (length < buffer.size() - 1) {
                    buffer.resize(length);
                    break;
                }
                buffer.resize(buffer.size() * 2);
            }
            return std::filesystem::path(buffer).parent_path();
#elif AE_PLATFORM_MACOS
            uint32_t size = 0;
            if (_NSGetExecutablePath(nullptr, &size) != -1 || size == 0) {
                return {};
            }
            std::vector<char> buffer(size, '\0');
            if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
                return {};
            }
            return std::filesystem::path(buffer.data()).parent_path();
#else
            std::vector<char> buffer(1024, '\0');
            while (true) {
                const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
                if (length <= 0) {
                    return {};
                }
                if (static_cast<size_t>(length) < buffer.size() - 1) {
                    buffer[static_cast<size_t>(length)] = '\0';
                    return std::filesystem::path(buffer.data()).parent_path();
                }
                buffer.resize(buffer.size() * 2);
            }
#endif
        }

        auto ToFString(const std::filesystem::path& path) -> Container::FString {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            const std::wstring wide = path.wstring();
            return Container::FString(wide.c_str(), static_cast<usize>(wide.size()));
#else
            const std::string narrow = path.string();
            return Container::FString(narrow.c_str(), static_cast<usize>(narrow.size()));
#endif
        }

        auto ResolveManagedPath(const std::filesystem::path& exeDir, const TChar* fileName)
            -> FManagedPathResolve {
            FManagedPathResolve result{};
            if (fileName == nullptr || fileName[0] == static_cast<TChar>(0)) {
                return result;
            }

            const std::filesystem::path filePart(fileName);
            std::error_code             ec;

            auto TryCandidate = [&](const std::filesystem::path& root) -> bool {
                if (root.empty()) {
                    return false;
                }
                const auto candidate = root / filePart;
                if (std::filesystem::exists(candidate, ec)) {
                    result.mPath = candidate;
                    result.mExists = true;
                    return true;
                }
                return false;
            };

            if (TryCandidate(exeDir)) {
                return result;
            }

            if (!exeDir.empty()) {
                const auto parent = exeDir.parent_path();
                if (TryCandidate(parent)) {
                    return result;
                }
            }

            ec = {};
            const auto cwd = std::filesystem::current_path(ec);
            if (!ec && TryCandidate(cwd)) {
                return result;
            }

            result.mPath = exeDir.empty() ? filePart : (exeDir / filePart);
            return result;
        }
#endif
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
            mAssetManager.RegisterLoader(&mScriptLoader);
            mAssetManager.RegisterLoader(&mTexture2DLoader);
            GameScene::FScriptComponent::SetAssetManager(&mAssetManager);
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

#if defined(AE_ENABLE_SCRIPTING_CORECLR) && AE_ENABLE_SCRIPTING_CORECLR
        if (!mScriptSystem) {
            mScriptSystem = MakeUnique<Scripting::CoreCLR::FScriptSystem>();
        }
        if (mScriptSystem) {
            Scripting::FScriptRuntimeConfig runtimeConfig{};
            const auto exeDir = GetExecutableDir();
            const auto runtimePath = ResolveManagedPath(exeDir, kManagedRuntimeConfig);
            if (runtimePath.mPath.empty()) {
                runtimeConfig.mRuntimeConfigPath.Assign(kManagedRuntimeConfig);
            } else {
                runtimeConfig.mRuntimeConfigPath = ToFString(runtimePath.mPath);
            }
            if (!runtimePath.mExists) {
                LogWarningCat(kScriptingCategory,
                    TEXT("Managed runtime config not found at {}."),
                    runtimeConfig.mRuntimeConfigPath.ToView());
            }

            Scripting::CoreCLR::FManagedRuntimeConfig managedConfig{};
            const auto assemblyPath = ResolveManagedPath(exeDir, kManagedAssembly);
            if (assemblyPath.mPath.empty()) {
                managedConfig.mAssemblyPath.Assign(kManagedAssembly);
            } else {
                managedConfig.mAssemblyPath = ToFString(assemblyPath.mPath);
            }
            if (!assemblyPath.mExists) {
                LogWarningCat(kScriptingCategory,
                    TEXT("Managed assembly not found at {}."), managedConfig.mAssemblyPath.ToView());
            }
            managedConfig.mTypeName.Assign(kManagedType);
            managedConfig.mMethodName.Assign(kManagedStartupMethod);
            managedConfig.mDelegateTypeName.Assign(kManagedStartupDelegate);

            const bool scriptingReady =
                mScriptSystem->Initialize(runtimeConfig, managedConfig, mInputSystem.Get());
            if (!scriptingReady) {
                LogWarningCat(kScriptingCategory, TEXT("Managed scripting runtime init failed."));
            } else {
                LogInfoCat(kScriptingCategory, TEXT("Managed scripting runtime initialized."));
            }
        }
#endif

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

        if (auto* world = mEngineRuntime.GetWorldManager().GetActiveWorld()) {
            world->Tick(InDeltaTime);
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
            mAssetManager.UnregisterLoader(&mScriptLoader);
            mAssetManager.UnregisterLoader(&mMeshLoader);
            mAssetManager.UnregisterLoader(&mMaterialLoader);
            mAssetManager.UnregisterLoader(&mAudioLoader);
            mAssetManager.SetRegistry(nullptr);
            GameScene::FScriptComponent::SetAssetManager(nullptr);
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

        if (mScriptSystem) {
            mScriptSystem->Shutdown();
            mScriptSystem.Reset();
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

    auto FEngineLoop::GetWorldManager() noexcept -> GameScene::FWorldManager& {
        return mEngineRuntime.GetWorldManager();
    }

    auto FEngineLoop::GetWorldManager() const noexcept -> const GameScene::FWorldManager& {
        return mEngineRuntime.GetWorldManager();
    }

    auto FEngineLoop::GetAssetRegistry() noexcept -> Asset::FAssetRegistry& {
        return mAssetRegistry;
    }

    auto FEngineLoop::GetAssetRegistry() const noexcept -> const Asset::FAssetRegistry& {
        return mAssetRegistry;
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

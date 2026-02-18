#include "TestHarness.h"

#include "Scripting/ScriptRuntime.h"
#include "Scripting/ScriptRuntimeCoreCLR.h"

#include "Container/String.h"
#include "Types/Aliases.h"

#include <filesystem>
#include <string>
#include <vector>

#if AE_PLATFORM_WIN
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
#elif AE_PLATFORM_MACOS
    #include <mach-o/dyld.h>
    #include <unistd.h>
#else
    #include <unistd.h>
#endif

using AltinaEngine::i32;
using AltinaEngine::usize;
using AltinaEngine::Core::Container::FString;
using AltinaEngine::Scripting::CoreCLR::CreateCoreCLRRuntime;
using AltinaEngine::Scripting::FScriptHandle;
using AltinaEngine::Scripting::FScriptInvocation;
using AltinaEngine::Scripting::FScriptLoadRequest;
using AltinaEngine::Scripting::FScriptRuntimeConfig;

namespace {
#if AE_PLATFORM_WIN
    #define AE_SCRIPT_TEST_CALL __cdecl
#else
    #define AE_SCRIPT_TEST_CALL
#endif

    extern "C" int AE_SCRIPT_TEST_CALL NativeAdd(int a, int b) { return a + b; }

    struct FInteropPayload {
        void* mCallback = nullptr;
        i32   mA = 0;
        i32   mB = 0;
        i32   mResult = 0;
        i32   mCallbackHit = 0;
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

    auto ToFString(const std::filesystem::path& path) -> FString {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        const std::wstring wide = path.wstring();
        return FString(wide.c_str(), static_cast<usize>(wide.size()));
#else
        const std::string narrow = path.string();
        return FString(narrow.c_str(), static_cast<usize>(narrow.size()));
#endif
    }
} // namespace

TEST_CASE("Scripting.CoreCLR.Interop") {
    const auto exeDir = GetExecutableDir();
    REQUIRE(!exeDir.empty());
    if (exeDir.empty()) {
        return;
    }

    const auto assemblyPath = exeDir / "AltinaEngine.Scripting.Tests.dll";
    const auto runtimeConfigPath = exeDir / "AltinaEngine.Scripting.Tests.runtimeconfig.json";

    REQUIRE(std::filesystem::exists(assemblyPath));
    REQUIRE(std::filesystem::exists(runtimeConfigPath));
    if (!std::filesystem::exists(assemblyPath) || !std::filesystem::exists(runtimeConfigPath)) {
        return;
    }

    FScriptRuntimeConfig config{};
    config.mRuntimeConfigPath = ToFString(runtimeConfigPath);

    auto runtime = CreateCoreCLRRuntime();
    REQUIRE(runtime);
    if (!runtime) {
        return;
    }
    const bool initialized = runtime->Initialize(config);
    REQUIRE(initialized);
    if (!initialized) {
        return;
    }

    FScriptLoadRequest request{};
    request.mAssemblyPath = ToFString(assemblyPath);
    request.mTypeName = FString(TEXT("AltinaEngine.Scripting.Tests.InteropEntry, AltinaEngine.Scripting.Tests"));
    request.mMethodName = FString(TEXT("ManagedEntryPoint"));
    request.mDelegateTypeName = FString(
        TEXT("AltinaEngine.Scripting.Tests.ManagedEntryPointDelegate, AltinaEngine.Scripting.Tests"));

    FScriptHandle handle{};
    const bool loaded = runtime->Load(request, handle);
    REQUIRE(loaded);
    if (!loaded) {
        runtime->Shutdown();
        return;
    }

    FInteropPayload payload{};
    payload.mCallback = reinterpret_cast<void*>(&NativeAdd);
    payload.mA = 7;
    payload.mB = 5;

    FScriptInvocation invocation{};
    invocation.mArgs = &payload;
    invocation.mSize = static_cast<i32>(sizeof(payload));

    const bool invoked = runtime->Invoke(handle, invocation);
    REQUIRE(invoked);
    if (!invoked) {
        runtime->Shutdown();
        return;
    }
    REQUIRE_EQ(payload.mCallbackHit, 1);
    REQUIRE_EQ(payload.mResult, payload.mA + payload.mB);

    runtime->Shutdown();
}

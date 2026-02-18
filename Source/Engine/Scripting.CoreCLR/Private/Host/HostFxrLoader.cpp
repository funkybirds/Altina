#include "Host/HostFxrLoader.h"

#include "Logging/Log.h"
#include "Types/Aliases.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <type_traits>
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
#else
    #include <dlfcn.h>
#endif

using AltinaEngine::TChar;
using AltinaEngine::Core::Container::FString;
using AltinaEngine::Core::Container::FStringView;
using AltinaEngine::Core::Logging::LogErrorCat;

namespace AltinaEngine::Scripting::CoreCLR::Host {
    namespace {
        constexpr auto kLogCategory = TEXT("Scripting.CoreCLR");

        auto Utf8ToWide(const char* data, usize length) -> std::wstring;
        auto WideToUtf8(const wchar_t* data, usize length) -> std::string;

        template <typename SrcChar, typename DstChar>
        auto ConvertStringView(AltinaEngine::Core::Container::TBasicStringView<SrcChar> value)
            -> std::basic_string<DstChar> {
            if (value.Length() == 0) {
                return {};
            }

            if constexpr (std::is_same_v<SrcChar, DstChar>) {
                return std::basic_string<DstChar>(value.Data(), value.Length());
            } else if constexpr (std::is_same_v<SrcChar, char>
                && std::is_same_v<DstChar, wchar_t>) {
                return Utf8ToWide(value.Data(), value.Length());
            } else if constexpr (std::is_same_v<SrcChar, wchar_t>
                && std::is_same_v<DstChar, char>) {
                return WideToUtf8(value.Data(), value.Length());
            } else {
                static_assert(std::is_same_v<SrcChar, DstChar>,
                    "Unsupported string conversion.");
            }
        }

        auto ToStdPath(FStringView value) -> std::filesystem::path {
            return std::filesystem::path(ConvertStringView<TChar, TChar>(value));
        }

        auto HostFxrLibraryName() -> const char* {
#if AE_PLATFORM_WIN
            return "hostfxr.dll";
#elif AE_PLATFORM_MACOS
            return "libhostfxr.dylib";
#else
            return "libhostfxr.so";
#endif
        }

        auto NethostLibraryName() -> const char* {
#if AE_PLATFORM_WIN
            return "nethost.dll";
#elif AE_PLATFORM_MACOS
            return "libnethost.dylib";
#else
            return "libnethost.so";
#endif
        }

        auto ParseVersion(const std::string& value, std::vector<int>& out) -> bool {
            out.clear();
            int current = 0;
            bool hasDigit = false;
            for (char ch : value) {
                if (ch >= '0' && ch <= '9') {
                    current = (current * 10) + (ch - '0');
                    hasDigit = true;
                } else if (ch == '.') {
                    if (!hasDigit) {
                        return false;
                    }
                    out.push_back(current);
                    current = 0;
                    hasDigit = false;
                } else {
                    return false;
                }
            }
            if (!hasDigit) {
                return false;
            }
            out.push_back(current);
            return !out.empty();
        }

        auto IsVersionGreater(const std::vector<int>& a, const std::vector<int>& b) -> bool {
            const size_t maxCount = std::max(a.size(), b.size());
            for (size_t i = 0; i < maxCount; ++i) {
                const int left = (i < a.size()) ? a[i] : 0;
                const int right = (i < b.size()) ? b[i] : 0;
                if (left != right) {
                    return left > right;
                }
            }
            return false;
        }

        auto FindLibraryInRoot(const std::filesystem::path& root, const char* fileName)
            -> std::filesystem::path {
            if (root.empty()) {
                return {};
            }

            std::error_code ec;
            auto direct = root / fileName;
            if (std::filesystem::exists(direct, ec)) {
                return direct;
            }

            auto fxrRoot = root / "host" / "fxr";
            if (!std::filesystem::exists(fxrRoot, ec)) {
                return {};
            }

            std::filesystem::path bestPath;
            std::vector<int>      bestVersion;
            for (const auto& entry : std::filesystem::directory_iterator(fxrRoot, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_directory()) {
                    continue;
                }

                const std::string versionName = entry.path().filename().string();
                std::vector<int>  parsed;
                if (!ParseVersion(versionName, parsed)) {
                    continue;
                }

                auto candidate = entry.path() / fileName;
                if (!std::filesystem::exists(candidate, ec)) {
                    continue;
                }

                if (bestPath.empty() || IsVersionGreater(parsed, bestVersion)) {
                    bestVersion = parsed;
                    bestPath = candidate;
                }
            }

            return bestPath;
        }

        auto GetDotnetRootFromEnv() -> std::filesystem::path {
#if AE_PLATFORM_WIN
            auto ReadEnv = [](const wchar_t* name) -> std::wstring {
                DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
                if (size == 0) {
                    return {};
                }
                std::wstring value(size, L'\0');
                DWORD written = GetEnvironmentVariableW(name, value.data(), size);
                if (written == 0 || written >= size) {
                    return {};
                }
                value.resize(written);
                return value;
            };

            std::wstring value = ReadEnv(L"DOTNET_ROOT");
            if (!value.empty()) {
                return std::filesystem::path(value);
            }
            value = ReadEnv(L"DOTNET_ROOT(x86)");
            if (!value.empty()) {
                return std::filesystem::path(value);
            }
#else
            if (const char* value = std::getenv("DOTNET_ROOT")) {
                return std::filesystem::path(value);
            }
#endif
            return {};
        }

        auto FindHostFxrWithNethost(const std::vector<std::filesystem::path>& roots,
            const std::filesystem::path& dotnetRoot) -> std::filesystem::path {
            std::filesystem::path nethostPath;
            for (const auto& root : roots) {
                nethostPath = FindLibraryInRoot(root, NethostLibraryName());
                if (!nethostPath.empty()) {
                    break;
                }
            }
            if (nethostPath.empty() && !dotnetRoot.empty()) {
                nethostPath = FindLibraryInRoot(dotnetRoot, NethostLibraryName());
            }

            FDynamicLibrary nethostLibrary;
            if (!nethostPath.empty()) {
                if (!nethostLibrary.Load(nethostPath)) {
                    return {};
                }
            } else {
                nethostLibrary.Load(std::filesystem::path(NethostLibraryName()));
            }

            if (!nethostLibrary.IsLoaded()) {
                return {};
            }

            auto getHostFxr = reinterpret_cast<get_hostfxr_path_fn>(
                nethostLibrary.GetSymbol("get_hostfxr_path"));
            if (!getHostFxr) {
                return {};
            }

            usize bufferSize = 0;
            if (getHostFxr(nullptr, &bufferSize, nullptr) != 0 || bufferSize == 0) {
                return {};
            }

            std::vector<FHostFxrChar> buffer(bufferSize);
            if (getHostFxr(buffer.data(), &bufferSize, nullptr) != 0) {
                return {};
            }

            return std::filesystem::path(buffer.data());
        }

#if AE_PLATFORM_WIN
        auto Utf8ToWide(const char* data, usize length) -> std::wstring {
            if (data == nullptr || length == 0) {
                return {};
            }
            const int wideCount = MultiByteToWideChar(
                CP_UTF8, 0, data, static_cast<int>(length), nullptr, 0);
            if (wideCount <= 0) {
                return {};
            }
            std::wstring wide(static_cast<size_t>(wideCount), L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0, data, static_cast<int>(length), wide.data(), wideCount);
            return wide;
        }

        auto WideToUtf8(const wchar_t* data, usize length) -> std::string {
            if (data == nullptr || length == 0) {
                return {};
            }
            const int narrowCount = WideCharToMultiByte(
                CP_UTF8, 0, data, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
            if (narrowCount <= 0) {
                return {};
            }
            std::string narrow(static_cast<size_t>(narrowCount), '\0');
            WideCharToMultiByte(CP_UTF8, 0, data, static_cast<int>(length), narrow.data(),
                narrowCount, nullptr, nullptr);
            return narrow;
        }
#else
        auto Utf8ToWide(const char* data, usize length) -> std::wstring {
            std::wstring wide;
            if (data == nullptr || length == 0) {
                return wide;
            }
            wide.reserve(length);
            for (usize i = 0; i < length; ++i) {
                wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(data[i])));
            }
            return wide;
        }

        auto WideToUtf8(const wchar_t* data, usize length) -> std::string {
            std::string narrow;
            if (data == nullptr || length == 0) {
                return narrow;
            }
            narrow.reserve(length);
            for (usize i = 0; i < length; ++i) {
                wchar_t ch = data[i];
                if (ch <= 0x7f) {
                    narrow.push_back(static_cast<char>(ch));
                } else {
                    narrow.push_back('?');
                }
            }
            return narrow;
        }
#endif
    } // namespace

    auto FDynamicLibrary::Load(const std::filesystem::path& path) -> bool {
        Unload();
        if (path.empty()) {
            return false;
        }
#if AE_PLATFORM_WIN
        mHandle = static_cast<void*>(LoadLibraryW(path.c_str()));
#else
        mHandle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
        return mHandle != nullptr;
    }

    void FDynamicLibrary::Unload() {
        if (!mHandle) {
            return;
        }
#if AE_PLATFORM_WIN
        FreeLibrary(static_cast<HMODULE>(mHandle));
#else
        dlclose(mHandle);
#endif
        mHandle = nullptr;
    }

    auto FDynamicLibrary::GetSymbol(const char* name) const -> void* {
        if (!mHandle || name == nullptr) {
            return nullptr;
        }
#if AE_PLATFORM_WIN
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(mHandle), name));
#else
        return dlsym(mHandle, name);
#endif
    }

    auto ToHostFxrString(FStringView value) -> FHostFxrString {
        return ConvertStringView<TChar, FHostFxrChar>(value);
    }

    auto FHostFxrLibrary::Load(
        const FString& runtimeConfigPath, const FString& runtimeRoot, const FString& dotnetRoot)
        -> bool {
        Unload();

        std::vector<std::filesystem::path> localRoots;
        if (!runtimeRoot.IsEmptyString()) {
            localRoots.push_back(ToStdPath(runtimeRoot.ToView()));
        }
        if (!runtimeConfigPath.IsEmptyString()) {
            auto configPath = ToStdPath(runtimeConfigPath.ToView());
            if (!configPath.empty()) {
                localRoots.push_back(configPath.parent_path());
            }
        }

        std::filesystem::path hostFxrPath;
        for (const auto& root : localRoots) {
            hostFxrPath = FindLibraryInRoot(root, HostFxrLibraryName());
            if (!hostFxrPath.empty()) {
                break;
            }
        }

        std::filesystem::path dotnetRootPath;
        if (!dotnetRoot.IsEmptyString()) {
            dotnetRootPath = ToStdPath(dotnetRoot.ToView());
        } else {
            dotnetRootPath = GetDotnetRootFromEnv();
        }
        if (dotnetRootPath.empty()) {
            if (!runtimeRoot.IsEmptyString()) {
                dotnetRootPath = ToStdPath(runtimeRoot.ToView());
            } else if (!runtimeConfigPath.IsEmptyString()) {
                auto configPath = ToStdPath(runtimeConfigPath.ToView());
                if (!configPath.empty()) {
                    dotnetRootPath = configPath.parent_path();
                }
            }
        }

        if (hostFxrPath.empty() && !dotnetRootPath.empty()) {
            hostFxrPath = FindLibraryInRoot(dotnetRootPath, HostFxrLibraryName());
        }

        if (hostFxrPath.empty()) {
            hostFxrPath = FindHostFxrWithNethost(localRoots, dotnetRootPath);
        }

        if (hostFxrPath.empty()) {
            hostFxrPath = std::filesystem::path(HostFxrLibraryName());
        }

        if (!mLibrary.Load(hostFxrPath)) {
            LogErrorCat(kLogCategory, TEXT("Failed to load hostfxr library."));
            return false;
        }

        mFunctions.InitializeForRuntimeConfig =
            reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
                mLibrary.GetSymbol("hostfxr_initialize_for_runtime_config"));
        mFunctions.GetRuntimeDelegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
            mLibrary.GetSymbol("hostfxr_get_runtime_delegate"));
        mFunctions.Close = reinterpret_cast<hostfxr_close_fn>(mLibrary.GetSymbol("hostfxr_close"));
        mFunctions.SetErrorWriter = reinterpret_cast<hostfxr_set_error_writer_fn>(
            mLibrary.GetSymbol("hostfxr_set_error_writer"));

        if (!mFunctions.InitializeForRuntimeConfig || !mFunctions.GetRuntimeDelegate
            || !mFunctions.Close) {
            LogErrorCat(kLogCategory, TEXT("hostfxr exports are missing required functions."));
            Unload();
            return false;
        }

        if (!dotnetRootPath.empty()) {
            mDotnetRoot = dotnetRootPath.native();
        }
        mHostFxrPath = hostFxrPath.native();

        return true;
    }

    void FHostFxrLibrary::Unload() {
        mLibrary.Unload();
        mFunctions = {};
        mDotnetRoot.clear();
        mHostFxrPath.clear();
    }
} // namespace AltinaEngine::Scripting::CoreCLR::Host

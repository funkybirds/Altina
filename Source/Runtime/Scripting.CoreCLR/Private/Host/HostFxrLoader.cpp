#include "Host/HostFxrLoader.h"

#include "Logging/Log.h"
#include "Types/Aliases.h"
#include "Utility/Filesystem/FileSystem.h"
#include "Utility/String/CodeConvert.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
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
using AltinaEngine::Core::Container::TVector;
using AltinaEngine::Core::Logging::LogErrorCat;
using AltinaEngine::Core::Utility::Filesystem::EnumerateDirectory;
using AltinaEngine::Core::Utility::Filesystem::FDirectoryEntry;
using AltinaEngine::Core::Utility::Filesystem::FPath;
using AltinaEngine::Core::Utility::String::FromUtf8Bytes;

namespace AltinaEngine::Scripting::CoreCLR::Host {
    namespace {
        constexpr auto kLogCategory = TEXT("Scripting.CoreCLR");

        auto           Utf8ToWide(const char* data, usize length) -> std::wstring;
        auto           WideToUtf8(const wchar_t* data, usize length) -> std::string;

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
                static_assert(std::is_same_v<SrcChar, DstChar>, "Unsupported string conversion.");
            }
        }

        auto ToPath(FStringView value) -> FPath { return FPath(value); }

        auto MakeFStringFromWide(const wchar_t* data, usize length) -> FString {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            return FString(data, length);
#else
            const auto utf8 = WideToUtf8(data, length);
            return FromUtf8Bytes(utf8.c_str(), utf8.size());
#endif
        }

        auto MakeFStringFromHostFxr(const FHostFxrChar* data, usize length) -> FString {
#if AE_PLATFORM_WIN
            return MakeFStringFromWide(data, length);
#else
            return FromUtf8Bytes(data, length);
#endif
        }

        auto HostFxrLibraryName() -> const TChar* {
#if AE_PLATFORM_WIN
            return TEXT("hostfxr.dll");
#elif AE_PLATFORM_MACOS
            return TEXT("libhostfxr.dylib");
#else
            return TEXT("libhostfxr.so");
#endif
        }

        auto NethostLibraryName() -> const TChar* {
#if AE_PLATFORM_WIN
            return TEXT("nethost.dll");
#elif AE_PLATFORM_MACOS
            return TEXT("libnethost.dylib");
#else
            return TEXT("libnethost.so");
#endif
        }

        auto ParseVersion(FStringView value, TVector<i32>& out) -> bool {
            out.Clear();
            i32  current  = 0;
            bool hasDigit = false;
            for (usize i = 0; i < value.Length(); ++i) {
                const TChar ch = value[i];
                if (ch >= static_cast<TChar>('0') && ch <= static_cast<TChar>('9')) {
                    current  = (current * 10) + (ch - static_cast<TChar>('0'));
                    hasDigit = true;
                } else if (ch == static_cast<TChar>('.')) {
                    if (!hasDigit) {
                        return false;
                    }
                    out.PushBack(current);
                    current  = 0;
                    hasDigit = false;
                } else {
                    return false;
                }
            }
            if (!hasDigit) {
                return false;
            }
            out.PushBack(current);
            return !out.IsEmpty();
        }

        auto IsVersionGreater(const TVector<i32>& a, const TVector<i32>& b) -> bool {
            const usize maxCount = (a.Size() > b.Size()) ? a.Size() : b.Size();
            for (usize i = 0; i < maxCount; ++i) {
                const i32 left  = (i < a.Size()) ? a[i] : 0;
                const i32 right = (i < b.Size()) ? b[i] : 0;
                if (left != right) {
                    return left > right;
                }
            }
            return false;
        }

        auto FindLibraryInRoot(const FPath& root, const TChar* fileName) -> FPath {
            if (root.IsEmpty() || fileName == nullptr || fileName[0] == static_cast<TChar>(0)) {
                return {};
            }

            const FPath direct = root / fileName;
            if (direct.Exists()) {
                return direct;
            }

            const FPath fxrRoot = root / TEXT("host") / TEXT("fxr");
            if (!Core::Utility::Filesystem::IsDirectory(fxrRoot)) {
                return {};
            }

            TVector<FDirectoryEntry> entries;
            if (!EnumerateDirectory(fxrRoot, false, entries)) {
                return {};
            }

            FPath        bestPath;
            TVector<i32> bestVersion;
            for (const auto& entry : entries) {
                if (!entry.IsDirectory) {
                    continue;
                }

                const auto   versionName = entry.Path.Filename();
                TVector<i32> parsed;
                if (!ParseVersion(versionName, parsed)) {
                    continue;
                }

                const FPath candidate = entry.Path / fileName;
                if (!candidate.Exists()) {
                    continue;
                }

                if (bestPath.IsEmpty() || IsVersionGreater(parsed, bestVersion)) {
                    bestVersion = parsed;
                    bestPath    = candidate;
                }
            }

            return bestPath;
        }

        auto GetDotnetRootFromEnv() -> FPath {
#if AE_PLATFORM_WIN
            auto ReadEnv = [](const wchar_t* name) -> std::wstring {
                DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
                if (size == 0) {
                    return {};
                }
                std::wstring value(size, L'\0');
                DWORD        written = GetEnvironmentVariableW(name, value.data(), size);
                if (written == 0 || written >= size) {
                    return {};
                }
                value.resize(written);
                return value;
            };

            std::wstring value = ReadEnv(L"DOTNET_ROOT");
            if (!value.empty()) {
                return FPath(MakeFStringFromWide(value.c_str(), static_cast<usize>(value.size())));
            }
            value = ReadEnv(L"DOTNET_ROOT(x86)");
            if (!value.empty()) {
                return FPath(MakeFStringFromWide(value.c_str(), static_cast<usize>(value.size())));
            }
#else
            if (const char* value = std::getenv("DOTNET_ROOT")) {
                return FPath(Core::Utility::String::FromUtf8Bytes(value, std::strlen(value)));
            }
#endif
            return {};
        }

        auto FindHostFxrWithNethost(const TVector<FPath>& roots, const FPath& dotnetRoot) -> FPath {
            FPath nethostPath;
            for (const auto& root : roots) {
                nethostPath = FindLibraryInRoot(root, NethostLibraryName());
                if (!nethostPath.IsEmpty()) {
                    break;
                }
            }
            if (nethostPath.IsEmpty() && !dotnetRoot.IsEmpty()) {
                nethostPath = FindLibraryInRoot(dotnetRoot, NethostLibraryName());
            }

            FDynamicLibrary nethostLibrary;
            if (!nethostPath.IsEmpty()) {
                if (!nethostLibrary.Load(nethostPath)) {
                    return {};
                }
            } else {
                nethostLibrary.Load(FPath(NethostLibraryName()));
            }

            if (!nethostLibrary.IsLoaded()) {
                return {};
            }

            auto getHostFxr =
                reinterpret_cast<get_hostfxr_path_fn>(nethostLibrary.GetSymbol("get_hostfxr_path"));
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

            usize length = bufferSize;
            if (length > 0 && buffer[length - 1] == static_cast<FHostFxrChar>(0)) {
                --length;
            }
            return FPath(MakeFStringFromHostFxr(buffer.data(), length));
        }

#if AE_PLATFORM_WIN
        auto Utf8ToWide(const char* data, usize length) -> std::wstring {
            if (data == nullptr || length == 0) {
                return {};
            }
            const int wideCount =
                MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(length), nullptr, 0);
            if (wideCount <= 0) {
                return {};
            }
            std::wstring wide(static_cast<size_t>(wideCount), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(length), wide.data(), wideCount);
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

    auto FDynamicLibrary::Load(const FPath& path) -> bool {
        Unload();
        if (path.IsEmpty()) {
            return false;
        }
#if AE_PLATFORM_WIN
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        mHandle = static_cast<void*>(LoadLibraryW(path.GetString().CStr()));
    #else
        const auto utf8Path = Core::Utility::String::ToUtf8Bytes(path.GetString());
        mHandle             = static_cast<void*>(LoadLibraryA(utf8Path.CStr()));
    #endif
#else
        const auto utf8Path = Core::Utility::String::ToUtf8Bytes(path.GetString());
        mHandle             = dlopen(utf8Path.CStr(), RTLD_LAZY | RTLD_LOCAL);
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

    auto FHostFxrLibrary::Load(const FString& runtimeConfigPath, const FString& runtimeRoot,
        const FString& dotnetRoot) -> bool {
        Unload();

        TVector<FPath> localRoots;
        if (!runtimeRoot.IsEmptyString()) {
            localRoots.PushBack(ToPath(runtimeRoot.ToView()));
        }
        if (!runtimeConfigPath.IsEmptyString()) {
            const auto configPath = ToPath(runtimeConfigPath.ToView());
            if (!configPath.IsEmpty()) {
                localRoots.PushBack(configPath.ParentPath());
            }
        }

        FPath hostFxrPath;
        for (const auto& root : localRoots) {
            hostFxrPath = FindLibraryInRoot(root, HostFxrLibraryName());
            if (!hostFxrPath.IsEmpty()) {
                break;
            }
        }

        FPath dotnetRootPath;
        if (!dotnetRoot.IsEmptyString()) {
            dotnetRootPath = ToPath(dotnetRoot.ToView());
        } else {
            dotnetRootPath = GetDotnetRootFromEnv();
        }
        if (dotnetRootPath.IsEmpty()) {
            if (!runtimeRoot.IsEmptyString()) {
                dotnetRootPath = ToPath(runtimeRoot.ToView());
            } else if (!runtimeConfigPath.IsEmptyString()) {
                const auto configPath = ToPath(runtimeConfigPath.ToView());
                if (!configPath.IsEmpty()) {
                    dotnetRootPath = configPath.ParentPath();
                }
            }
        }

        if (hostFxrPath.IsEmpty() && !dotnetRootPath.IsEmpty()) {
            hostFxrPath = FindLibraryInRoot(dotnetRootPath, HostFxrLibraryName());
        }

        if (hostFxrPath.IsEmpty()) {
            hostFxrPath = FindHostFxrWithNethost(localRoots, dotnetRootPath);
        }

        if (hostFxrPath.IsEmpty()) {
            hostFxrPath = FPath(HostFxrLibraryName());
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

        if (!dotnetRootPath.IsEmpty()) {
            mDotnetRoot = ToHostFxrString(dotnetRootPath.GetString().ToView());
        }
        mHostFxrPath = ToHostFxrString(hostFxrPath.GetString().ToView());

        return true;
    }

    void FHostFxrLibrary::Unload() {
        mLibrary.Unload();
        mFunctions = {};
        mDotnetRoot.clear();
        mHostFxrPath.clear();
    }
} // namespace AltinaEngine::Scripting::CoreCLR::Host

#include "Platform/PlatformFileSystem.h"
#include "Utility/String/CodeConvert.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
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
#else
    #include <unistd.h>
#endif

using AltinaEngine::Core::Container::TBasicString;
namespace AltinaEngine::Core::Platform {
    namespace Container = Core::Container;
    namespace {
        template <typename CharT>
        auto ToPathImpl(const TBasicString<CharT>& value) -> std::filesystem::path {
            return std::filesystem::path(value.CStr());
        }

        auto ToPath(const FString& value) -> std::filesystem::path { return ToPathImpl(value); }

        auto FromUtf8(Container::FNativeStringView value) -> FString {
            FString out;
            if (value.Length() == 0) {
                return out;
            }
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
    #if AE_PLATFORM_WIN
            int wideCount = MultiByteToWideChar(
                CP_UTF8, 0, value.Data(), static_cast<int>(value.Length()), nullptr, 0);
            if (wideCount <= 0) {
                return out;
            }
            std::wstring wide(static_cast<size_t>(wideCount), L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0, value.Data(), static_cast<int>(value.Length()), wide.data(), wideCount);
            out.Append(wide.c_str(), wide.size());
    #else
            for (usize i = 0; i < value.Length(); ++i) {
                out.Append(static_cast<wchar_t>(static_cast<unsigned char>(value.Data()[i])));
            }
    #endif
#else
            out.Append(value.Data(), value.Length());
#endif
            return out;
        }

        auto FromPath(const std::filesystem::path& value) -> FString {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
    #if AE_PLATFORM_WIN
            const auto wide = value.wstring();
            return FString(wide.c_str(), static_cast<usize>(wide.size()));
    #else
            const auto utf8 = value.string();
            return FromUtf8(Container::FNativeStringView(utf8.c_str(), utf8.size()));
    #endif
#else
            const auto utf8 = value.string();
            return FString(utf8.c_str(), static_cast<usize>(utf8.size()));
#endif
        }

#if AE_PLATFORM_WIN
        auto ToUtf8(const std::wstring& value) -> std::string {
            if (value.empty()) {
                return {};
            }
            const int utf8Count = WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (utf8Count <= 0) {
                return {};
            }
            std::string utf8(static_cast<size_t>(utf8Count), '\0');
            WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                utf8.data(), utf8Count, nullptr, nullptr);
            return utf8;
        }
#endif
    } // namespace

    auto ReadFileBytes(const FString& path, TVector<u8>& outBytes) -> bool {
        outBytes.Clear();
        std::error_code ec;
        const auto      fsPath = ToPath(path);
        if (!std::filesystem::exists(fsPath, ec)) {
            return false;
        }

        std::ifstream file(fsPath, std::ios::binary);
        if (!file) {
            return false;
        }

        file.seekg(0, std::ios::end);
        const auto endPos = file.tellg();
        if (endPos < 0) {
            return false;
        }
        const auto size = static_cast<usize>(endPos);
        file.seekg(0, std::ios::beg);
        outBytes.Resize(size);
        if (size > 0) {
            file.read(reinterpret_cast<char*>(outBytes.Data()), static_cast<std::streamsize>(size));
        }
        return file.good() || file.eof();
    }

    auto ReadFileTextUtf8(const FString& path, FNativeString& outText) -> bool {
        outText.Clear();
        std::error_code ec;
        const auto      fsPath = ToPath(path);
        if (!std::filesystem::exists(fsPath, ec)) {
            return false;
        }

        std::ifstream file(fsPath, std::ios::binary);
        if (!file) {
            return false;
        }

        file.seekg(0, std::ios::end);
        const auto endPos = file.tellg();
        if (endPos < 0) {
            return false;
        }
        const auto size = static_cast<usize>(endPos);
        file.seekg(0, std::ios::beg);

        if (size > 0) {
            outText.Resize(size);
            file.read(outText.GetData(), static_cast<std::streamsize>(size));
            if (!file.good() && !file.eof()) {
                outText.Clear();
                return false;
            }
        }
        return file.good() || file.eof();
    }

    void RemoveFileIfExists(const FString& path) {
        std::error_code ec;
        std::filesystem::remove(ToPath(path), ec);
    }

    auto IsPathExist(const FString& path) -> bool {
        std::error_code ec;
        return std::filesystem::exists(ToPath(path), ec);
    }

    auto GetExecutableDir() -> FString {
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
        const auto dir = std::filesystem::path(buffer).parent_path().wstring();
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        return FString(dir.c_str(), static_cast<usize>(dir.size()));
    #else
        const auto utf8 = ToUtf8(dir);
        return FromUtf8(Container::FNativeStringView(utf8.c_str(), utf8.size()));
    #endif
#elif AE_PLATFORM_MACOS
        uint32_t size = 0;
        if (_NSGetExecutablePath(nullptr, &size) != -1 || size == 0) {
            return {};
        }
        std::vector<char> buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return {};
        }
        const auto dir = std::filesystem::path(buffer.data()).parent_path().string();
        return FromUtf8(Container::FNativeStringView(dir.c_str(), dir.size()));
#else
        std::vector<char> buffer(1024, '\0');
        while (true) {
            const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
            if (length <= 0) {
                return {};
            }
            if (static_cast<size_t>(length) < buffer.size() - 1) {
                buffer[static_cast<size_t>(length)] = '\0';
                const auto dir = std::filesystem::path(buffer.data()).parent_path().string();
                return FromUtf8(Container::FNativeStringView(dir.c_str(), dir.size()));
            }
            buffer.resize(buffer.size() * 2);
        }
#endif
    }

    auto GetCurrentWorkingDir() -> FString {
#if AE_PLATFORM_WIN
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        std::wstring buffer(260, L'\0');
        DWORD        length = 0;
        while (true) {
            length = GetCurrentDirectoryW(static_cast<DWORD>(buffer.size()), buffer.data());
            if (length == 0) {
                return {};
            }
            if (length < buffer.size()) {
                buffer.resize(length);
                break;
            }
            buffer.resize(buffer.size() * 2);
        }
        return FString(buffer.c_str(), static_cast<usize>(buffer.size()));
    #else
        Container::TVector<char> buffer;
        buffer.Resize(260);
        DWORD length = 0;
        while (true) {
            length = GetCurrentDirectoryA(static_cast<DWORD>(buffer.Size()), buffer.Data());
            if (length == 0) {
                return {};
            }
            if (length < buffer.Size()) {
                buffer.Resize(length);
                break;
            }
            buffer.Resize(buffer.Size() * 2);
        }
        return FString(buffer.Data(), static_cast<usize>(buffer.Size()));
    #endif
#else
        Container::TVector<char> buffer;
        buffer.Resize(256);
        while (true) {
            if (getcwd(buffer.Data(), buffer.Size()) != nullptr) {
                const auto length = std::strlen(buffer.Data());
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
                return Utility::String::FromUtf8Bytes(buffer.Data(), static_cast<usize>(length));
    #else
                return FString(buffer.Data(), static_cast<usize>(length));
    #endif
            }
            if (errno == ERANGE) {
                buffer.Resize(buffer.Size() * 2);
                continue;
            }
            return {};
        }
#endif
    }

    auto SetCurrentWorkingDir(const FString& path) -> bool {
        if (path.IsEmptyString()) {
            return false;
        }
#if AE_PLATFORM_WIN
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        return SetCurrentDirectoryW(path.CStr()) != 0;
    #else
        return SetCurrentDirectoryA(path.CStr()) != 0;
    #endif
#else
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        const auto utf8 = Utility::String::ToUtf8Bytes(path);
        return chdir(utf8.CStr()) == 0;
    #else
        return chdir(path.CStr()) == 0;
    #endif
#endif
    }

    auto GetTempDirectory() -> FString {
        std::error_code       ec;
        std::filesystem::path tempPath;
        try {
            tempPath = std::filesystem::temp_directory_path();
        } catch (...) {
            tempPath = std::filesystem::current_path(ec);
        }
        if (tempPath.empty()) {
            return {};
        }
        return FromPath(tempPath);
    }

    auto CreateDirectories(const FString& path) -> bool {
        if (path.IsEmptyString()) {
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(ToPath(path), ec);
        return !ec;
    }

    auto GetPathSeparator() -> TChar {
#if AE_PLATFORM_WIN
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        return static_cast<TChar>(L'\\');
    #else
        return static_cast<TChar>('\\');
    #endif
#else
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        return static_cast<TChar>(L'/');
    #else
        return static_cast<TChar>('/');
    #endif
#endif
    }

    auto IsPathSeparator(TChar value) -> bool {
#if AE_PLATFORM_WIN
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        return value == static_cast<TChar>(L'\\') || value == static_cast<TChar>(L'/');
    #else
        return value == static_cast<TChar>('\\') || value == static_cast<TChar>('/');
    #endif
#else
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        return value == static_cast<TChar>(L'/');
    #else
        return value == static_cast<TChar>('/');
    #endif
#endif
    }

    auto IsAbsolutePath(FStringView path) -> bool {
        if (path.IsEmpty()) {
            return false;
        }
        const FString               temp(path);
        const std::filesystem::path fsPath = ToPath(temp);
        return fsPath.is_absolute();
    }

    auto NormalizePath(FStringView path) -> FString {
        if (path.IsEmpty()) {
            return {};
        }
        const FString temp(path);
        auto          fsPath = ToPath(temp);
        fsPath               = fsPath.lexically_normal();
        fsPath.make_preferred();
        return FromPath(fsPath);
    }

    auto GetRootLength(FStringView path) -> usize {
        if (path.IsEmpty()) {
            return 0;
        }
        const FString temp(path);
        const auto    fsPath = ToPath(temp);
        const auto    root   = fsPath.root_path();
        if (root.empty()) {
            return 0;
        }
        const auto rootText = FromPath(root);
        return rootText.Length();
    }

} // namespace AltinaEngine::Core::Platform

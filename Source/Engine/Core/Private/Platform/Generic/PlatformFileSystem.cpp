#include "Platform/PlatformFileSystem.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

using AltinaEngine::Core::Container::TBasicString;
namespace AltinaEngine::Core::Platform {
    namespace Container = Core::Container;
    namespace {
        template <typename CharT>
        auto ToPathImpl(const TBasicString<CharT>& value) -> std::filesystem::path {
            if constexpr (std::is_same_v<CharT, wchar_t>) {
                return std::filesystem::path(std::wstring(value.GetData(), value.Length()));
            } else {
                return std::filesystem::path(std::string(value.GetData(), value.Length()));
            }
        }

        auto ToPath(const FString& value) -> std::filesystem::path { return ToPathImpl(value); }
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

        std::string content(
            (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (!content.empty()) {
            outText.Append(content.c_str(), content.size());
        }
        return file.good() || file.eof();
    }

    void RemoveFileIfExists(const FString& path) {
        std::error_code ec;
        std::filesystem::remove(ToPath(path), ec);
    }

} // namespace AltinaEngine::Core::Platform

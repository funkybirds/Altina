#include "Utility/Filesystem/FileSystem.h"

#include "Utility/String/CodeConvert.h"

#include <filesystem>

namespace AltinaEngine::Core::Utility::Filesystem {
    namespace {
        auto ToStdPath(const FPath& path) -> std::filesystem::path {
            const auto& text = path.GetString();
            return std::filesystem::path(text.CStr());
        }

        auto FromStdPath(const std::filesystem::path& value) -> FPath {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
    #if AE_PLATFORM_WIN
            const auto wide = value.wstring();
            return FPath(Container::FString(wide.c_str(), static_cast<usize>(wide.size())));
    #else
            const auto utf8 = value.string();
            return FPath(Utility::String::FromUtf8Bytes(utf8.c_str(), utf8.size()));
    #endif
#else
            const auto utf8 = value.string();
            return FPath(Container::FString(utf8.c_str(), static_cast<usize>(utf8.size())));
#endif
        }
    } // namespace

    auto Absolute(const FPath& path) -> FPath {
        if (path.IsEmpty()) {
            return path;
        }
        std::error_code ec;
        auto            abs = std::filesystem::absolute(ToStdPath(path), ec);
        if (ec) {
            return path;
        }
        return FromStdPath(abs);
    }

    auto Relative(const FPath& path, const FPath& base) -> FPath {
        if (path.IsEmpty()) {
            return path;
        }
        std::error_code ec;
        auto            rel = std::filesystem::relative(ToStdPath(path), ToStdPath(base), ec);
        if (ec) {
            return path;
        }
        return FromStdPath(rel);
    }

    auto IsDirectory(const FPath& path) -> bool {
        if (path.IsEmpty()) {
            return false;
        }
        std::error_code ec;
        return std::filesystem::is_directory(ToStdPath(path), ec);
    }

    auto EnumerateDirectory(const FPath& root, bool recursive, TVector<FDirectoryEntry>& outEntries)
        -> bool {
        outEntries.Clear();
        if (root.IsEmpty()) {
            return false;
        }

        std::error_code ec;
        const auto      rootPath = ToStdPath(root);
        if (!std::filesystem::exists(rootPath, ec)) {
            return false;
        }

        if (recursive) {
            std::filesystem::recursive_directory_iterator it(
                rootPath, std::filesystem::directory_options::skip_permission_denied, ec);
            const std::filesystem::recursive_directory_iterator end;
            for (; it != end && !ec; it.increment(ec)) {
                FDirectoryEntry entry{};
                entry.Path        = FromStdPath(it->path());
                entry.IsDirectory = it->is_directory(ec);
                outEntries.PushBack(Move(entry));
            }
        } else {
            std::filesystem::directory_iterator it(
                rootPath, std::filesystem::directory_options::skip_permission_denied, ec);
            const std::filesystem::directory_iterator end;
            for (; it != end && !ec; it.increment(ec)) {
                FDirectoryEntry entry{};
                entry.Path        = FromStdPath(it->path());
                entry.IsDirectory = it->is_directory(ec);
                outEntries.PushBack(Move(entry));
            }
        }

        return !ec;
    }
} // namespace AltinaEngine::Core::Utility::Filesystem

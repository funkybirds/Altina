#include "ShaderCompilerUtils.h"

#include <atomic>
#include <filesystem>
#include <string>
#include <type_traits>

using AltinaEngine::Core::Container::TBasicString;
namespace AltinaEngine::ShaderCompiler::Detail {
    namespace Container = Core::Container;
    namespace {
        template <typename CharT>
        auto FromPathImpl(const std::filesystem::path& path) -> TBasicString<CharT> {
            TBasicString<CharT> out;
            if constexpr (std::is_same_v<CharT, wchar_t>) {
                const auto native = path.wstring();
                out.Append(native.c_str(), native.size());
            } else {
                const auto native = path.string();
                out.Append(native.c_str(), native.size());
            }
            return out;
        }

        auto FromPath(const std::filesystem::path& path) -> FString {
            return FromPathImpl<TChar>(path);
        }

        template <typename CharT>
        auto ToPathImpl(const TBasicString<CharT>& value) -> std::filesystem::path {
            return std::filesystem::path(value.CStr());
        }

        auto ToPath(const FString& value) -> std::filesystem::path { return ToPathImpl(value); }
    } // namespace

    void AppendDiagnosticLine(FString& diagnostics, const TChar* line) {
        if ((line == nullptr) || (line[0] == static_cast<TChar>(0))) {
            return;
        }
        if (!diagnostics.IsEmptyString()) {
            diagnostics.Append(TEXT("\n"));
        }
        diagnostics.Append(line);
    }

    void AppendDiagnostic(FString& diagnostics, const FString& text) {
        if (text.IsEmptyString()) {
            return;
        }
        if (!diagnostics.IsEmptyString()) {
            diagnostics.Append(TEXT("\n"));
        }
        diagnostics.Append(text.GetData(), text.Length());
    }

    auto BuildTempOutputPath(
        const FString& sourcePath, const FString& suffix, const FString& extension) -> FString {
        static std::atomic<u32> counter{ 0 };
        std::error_code         ec;

        std::filesystem::path   dir;
        try {
            dir = std::filesystem::temp_directory_path();
        } catch (...) {
            dir = std::filesystem::current_path(ec);
        }

        dir /= "AltinaEngine";
        dir /= "ShaderCompile";
        std::filesystem::create_directories(dir, ec);

        auto basePath = ToPath(sourcePath);
        auto stem     = basePath.stem();
        if (stem.empty()) {
            stem = std::filesystem::path("shader");
        }

        const auto            uniqueId = counter.fetch_add(1, std::memory_order_relaxed);
        std::filesystem::path filename = stem;
        filename += std::filesystem::path(TEXT("_"));
        filename += ToPath(FString::ToString(uniqueId));
        filename += std::filesystem::path(TEXT("_"));
        filename += ToPath(suffix);
        filename += ToPath(extension);

        return FromPath(dir / filename);
    }

} // namespace AltinaEngine::ShaderCompiler::Detail

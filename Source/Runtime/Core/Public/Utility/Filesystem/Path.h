#pragma once

#include "Base/CoreAPI.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Platform/PlatformFileSystem.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

namespace AltinaEngine::Core::Utility::Filesystem {
    using Container::FString;
    using Container::FStringView;

    class FPath {
    public:
        FPath() = default;
        explicit FPath(const FString& path) : mPath(path) {}
        explicit FPath(FString&& path) noexcept : mPath(AltinaEngine::Move(path)) {}
        explicit FPath(FStringView path) : mPath(path) {}
        explicit FPath(const TChar* path) : mPath(path) {}

        auto operator=(FStringView path) -> FPath& {
            mPath.Assign(path);
            return *this;
        }

        [[nodiscard]] auto IsEmpty() const noexcept -> bool { return mPath.IsEmptyString(); }
        void               Clear() { mPath.Clear(); }

        [[nodiscard]] auto GetString() const noexcept -> const FString& { return mPath; }
        [[nodiscard]] auto ToView() const noexcept -> FStringView { return mPath.ToView(); }

        [[nodiscard]] auto IsAbsolute() const noexcept -> bool {
            return Platform::IsAbsolutePath(mPath.ToView());
        }
        [[nodiscard]] auto IsRelative() const noexcept -> bool { return !IsAbsolute(); }
        [[nodiscard]] auto Exists() const noexcept -> bool { return Platform::IsPathExist(mPath); }

        [[nodiscard]] auto ParentPath() const -> FPath { return FPath(ExtractParent()); }
        [[nodiscard]] auto Filename() const noexcept -> FStringView { return ExtractFilename(); }
        [[nodiscard]] auto Extension() const noexcept -> FStringView { return ExtractExtension(); }
        [[nodiscard]] auto Stem() const noexcept -> FStringView { return ExtractStem(); }
        [[nodiscard]] auto HasExtension() const noexcept -> bool {
            return Extension().Length() > 0;
        }

        [[nodiscard]] auto ReplaceExtension(FStringView extension) const -> FPath {
            FPath out(*this);
            out.ReplaceExtensionInPlace(extension);
            return out;
        }

        auto Normalize() -> FPath& {
            mPath = Platform::NormalizePath(mPath.ToView());
            return *this;
        }
        [[nodiscard]] auto Normalized() const -> FPath {
            FPath out(*this);
            out.Normalize();
            return out;
        }

        auto MakePreferred() -> FPath& {
            ReplaceSeparators(Platform::GetPathSeparator());
            return *this;
        }
        [[nodiscard]] auto Preferred() const -> FPath {
            FPath out(*this);
            out.MakePreferred();
            return out;
        }

        auto Append(FStringView component) -> FPath& {
            AppendImpl(component);
            return *this;
        }

        auto operator/=(FStringView component) -> FPath& {
            AppendImpl(component);
            return *this;
        }

        [[nodiscard]] friend auto operator/(const FPath& lhs, FStringView rhs) -> FPath {
            FPath out(lhs);
            out.Append(rhs);
            return out;
        }

        [[nodiscard]] auto operator==(const FPath& other) const noexcept -> bool {
            return mPath == other.mPath;
        }
        [[nodiscard]] auto operator!=(const FPath& other) const noexcept -> bool {
            return !(*this == other);
        }

    private:
        static auto IsSeparator(TChar value) noexcept -> bool {
            return Platform::IsPathSeparator(value);
        }

        static auto TrimTrailingSeparators(FStringView view, usize rootLength) noexcept -> usize {
            usize end = view.Length();
            while (end > rootLength && IsSeparator(view[end - 1])) {
                --end;
            }
            return end;
        }

        [[nodiscard]] auto ExtractParent() const -> FString {
            const auto view = mPath.ToView();
            if (view.IsEmpty()) {
                return {};
            }

            const usize rootLength = Platform::GetRootLength(view);
            const usize end        = TrimTrailingSeparators(view, rootLength);
            if (end <= rootLength) {
                return FString(view.Substr(0, rootLength));
            }

            if (end < view.Length()) {
                return FString(view.Substr(0, end));
            }

            usize pos = end;
            while (pos > rootLength && !IsSeparator(view[pos - 1])) {
                --pos;
            }

            if (pos <= rootLength) {
                return FString(view.Substr(0, rootLength));
            }

            const usize parentLength = (pos > 0) ? (pos - 1) : 0;
            if (parentLength == 0) {
                return {};
            }
            return FString(view.Substr(0, parentLength));
        }

        [[nodiscard]] auto ExtractFilename() const noexcept -> FStringView {
            const auto view = mPath.ToView();
            if (view.IsEmpty()) {
                return {};
            }

            const usize rootLength = Platform::GetRootLength(view);
            const usize end        = TrimTrailingSeparators(view, rootLength);
            if (end <= rootLength) {
                return {};
            }
            if (end < view.Length()) {
                return {};
            }

            usize start = end;
            while (start > rootLength && !IsSeparator(view[start - 1])) {
                --start;
            }

            return view.Substr(start, end - start);
        }

        [[nodiscard]] auto ExtractExtension() const noexcept -> FStringView {
            const auto filename = ExtractFilename();
            if (filename.IsEmpty()) {
                return {};
            }

            for (usize i = filename.Length(); i > 0; --i) {
                if (filename[i - 1] == static_cast<TChar>('.')) {
                    if (i == 1) {
                        return {};
                    }
                    return filename.Substr(i - 1, filename.Length() - (i - 1));
                }
            }
            return {};
        }

        [[nodiscard]] auto ExtractStem() const noexcept -> FStringView {
            const auto filename = ExtractFilename();
            if (filename.IsEmpty()) {
                return {};
            }

            for (usize i = filename.Length(); i > 0; --i) {
                if (filename[i - 1] == static_cast<TChar>('.')) {
                    if (i == 1) {
                        return filename;
                    }
                    return filename.Substr(0, i - 1);
                }
            }
            return filename;
        }

        void ReplaceExtensionInPlace(FStringView extension) {
            const auto view = mPath.ToView();
            if (view.IsEmpty()) {
                return;
            }

            const usize rootLength = Platform::GetRootLength(view);
            const usize end        = TrimTrailingSeparators(view, rootLength);
            if (end <= rootLength) {
                return;
            }
            if (end < view.Length()) {
                return;
            }

            usize filenameStart = end;
            while (filenameStart > rootLength && !IsSeparator(view[filenameStart - 1])) {
                --filenameStart;
            }
            if (filenameStart == end) {
                return;
            }

            usize dotPos = end;
            for (usize i = end; i > filenameStart; --i) {
                if (view[i - 1] == static_cast<TChar>('.')) {
                    if (i - 1 == filenameStart) {
                        dotPos = end;
                    } else {
                        dotPos = i - 1;
                    }
                    break;
                }
            }

            if (dotPos == end) {
                dotPos = end;
            }

            FString out;
            out.Append(view.Substr(0, dotPos));

            if (!extension.IsEmpty()) {
                if (extension[0] != static_cast<TChar>('.')) {
                    out.Append(static_cast<TChar>('.'));
                }
                out.Append(extension);
            }

            mPath = AltinaEngine::Move(out);
        }

        void ReplaceSeparators(TChar preferred) {
            const auto length = mPath.Length();
            for (usize i = 0; i < length; ++i) {
                if (IsSeparator(mPath[i]) && mPath[i] != preferred) {
                    mPath[i] = preferred;
                }
            }
        }

        void AppendImpl(FStringView component) {
            if (component.IsEmpty()) {
                return;
            }

            if (mPath.IsEmptyString()) {
                mPath.Assign(component);
                return;
            }

            if (Platform::IsAbsolutePath(component)) {
                mPath.Assign(component);
                return;
            }

            const TChar separator  = Platform::GetPathSeparator();
            const bool  leftHasSep = IsSeparator(mPath[mPath.Length() - 1]);
            usize       start      = 0;
            while (start < component.Length() && IsSeparator(component[start])) {
                ++start;
            }

            if (!leftHasSep) {
                mPath.Append(separator);
            }
            mPath.Append(component.Substr(start, component.Length() - start));
        }

    private:
        FString mPath;
    };
} // namespace AltinaEngine::Core::Utility::Filesystem

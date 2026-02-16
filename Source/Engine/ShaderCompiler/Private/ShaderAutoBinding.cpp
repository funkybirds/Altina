#include "ShaderAutoBinding.h"

#include "Platform/PlatformFileSystem.h"
#include "ShaderCompilerUtils.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <type_traits>

using AltinaEngine::Core::Container::TBasicString;
namespace AltinaEngine::ShaderCompiler::Detail {
    namespace Container = Core::Container;
    using Container::FNativeString;
    using Container::FNativeStringView;
    namespace {
        constexpr u32 kGroupCount = static_cast<u32>(EAutoBindingGroup::Count);

        constexpr u32 kDx11CbvBase[kGroupCount]     = { 0U, 4U, 8U };
        constexpr u32 kDx11SrvBase[kGroupCount]     = { 0U, 16U, 32U };
        constexpr u32 kDx11SamplerBase[kGroupCount] = { 0U, 4U, 8U };
        constexpr u32 kDx11UavBase[kGroupCount]     = { 0U, 4U, 8U };

        template <typename CharT>
        auto ToPathImpl(const TBasicString<CharT>& value) -> std::filesystem::path {
            return std::filesystem::path(value.CStr());
        }

        auto ToPath(const FString& value) -> std::filesystem::path { return ToPathImpl(value); }

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

        auto Trim(FNativeStringView text) -> FNativeString {
            const FNativeStringView whitespace(" \t\r\n");
            const auto              first = text.FindFirstNotOf(whitespace);
            if (first == FNativeStringView::npos) {
                return {};
            }
            const auto last = text.FindLastNotOf(whitespace);
            if (last == FNativeStringView::npos || last < first) {
                return {};
            }
            return FNativeString(text.Substr(first, last - first + 1));
        }

        auto SplitArgs(FNativeStringView text, FNativeString& a, FNativeString& b) -> bool {
            int depthAngle   = 0;
            int depthParen   = 0;
            int depthBracket = 0;
            for (usize i = 0; i < text.Length(); ++i) {
                const char ch = text[i];
                if (ch == '<') {
                    ++depthAngle;
                } else if (ch == '>') {
                    depthAngle = (depthAngle > 0) ? depthAngle - 1 : 0;
                } else if (ch == '(') {
                    ++depthParen;
                } else if (ch == ')') {
                    depthParen = (depthParen > 0) ? depthParen - 1 : 0;
                } else if (ch == '[') {
                    ++depthBracket;
                } else if (ch == ']') {
                    depthBracket = (depthBracket > 0) ? depthBracket - 1 : 0;
                } else if (ch == ',' && depthAngle == 0 && depthParen == 0 && depthBracket == 0) {
                    a = Trim(text.Substr(0, i));
                    b = Trim(text.Substr(i + 1));
                    return !a.IsEmptyString() && !b.IsEmptyString();
                }
            }
            return false;
        }

        auto MatchToken(FNativeStringView text, usize pos, const char* token) -> bool {
            if (token == nullptr) {
                return false;
            }
            const FNativeStringView tokenView(token);
            if (pos + tokenView.Length() > text.Length()) {
                return false;
            }
            return text.Substr(pos, tokenView.Length()) == tokenView;
        }

        auto GetRegisterChar(EAutoBindingResource resource) -> char {
            switch (resource) {
                case EAutoBindingResource::CBuffer:
                    return 'b';
                case EAutoBindingResource::SRV:
                    return 't';
                case EAutoBindingResource::UAV:
                    return 'u';
                case EAutoBindingResource::Sampler:
                    return 's';
                default:
                    return 't';
            }
        }

        auto GetDx11Base(EAutoBindingResource resource, u32 group) -> u32 {
            switch (resource) {
                case EAutoBindingResource::CBuffer:
                    return kDx11CbvBase[group];
                case EAutoBindingResource::SRV:
                    return kDx11SrvBase[group];
                case EAutoBindingResource::UAV:
                    return kDx11UavBase[group];
                case EAutoBindingResource::Sampler:
                    return kDx11SamplerBase[group];
                default:
                    return 0U;
            }
        }

        auto BuildRegisterSuffix(Rhi::ERhiBackend backend, EAutoBindingResource resource, u32 index,
            u32 group) -> FNativeString {
            const char reg = GetRegisterChar(resource);
            if (backend == Rhi::ERhiBackend::DirectX11) {
                const u32 dx11Index = GetDx11Base(resource, group) + index;
                FNativeString out("register(");
                out.Append(reg);
                out.AppendNumber(dx11Index);
                out.Append(")");
                return out;
            }

            FNativeString out("register(");
            out.Append(reg);
            out.AppendNumber(index);
            out.Append(", space");
            out.AppendNumber(group);
            out.Append(")");
            return out;
        }

        auto ResolveGroupToken(FNativeStringView token, EAutoBindingGroup& outGroup) -> bool {
            if (token == FNativeStringView("FRAME")) {
                outGroup = EAutoBindingGroup::PerFrame;
                return true;
            }
            if (token == FNativeStringView("DRAW")) {
                outGroup = EAutoBindingGroup::PerDraw;
                return true;
            }
            if (token == FNativeStringView("MATERIAL")) {
                outGroup = EAutoBindingGroup::PerMaterial;
                return true;
            }
            return false;
        }

        auto ResolveResourceToken(FNativeStringView token, EAutoBindingResource& outResource)
            -> bool {
            if (token == FNativeStringView("CBUFFER")) {
                outResource = EAutoBindingResource::CBuffer;
                return true;
            }
            if (token == FNativeStringView("SRV")) {
                outResource = EAutoBindingResource::SRV;
                return true;
            }
            if (token == FNativeStringView("UAV")) {
                outResource = EAutoBindingResource::UAV;
                return true;
            }
            if (token == FNativeStringView("SAMPLER")) {
                outResource = EAutoBindingResource::Sampler;
                return true;
            }
            return false;
        }

        struct FMarker {
            size_t               mStart    = 0;
            size_t               mEnd      = 0;
            EAutoBindingGroup    mGroup    = EAutoBindingGroup::PerFrame;
            EAutoBindingResource mResource = EAutoBindingResource::SRV;
            FNativeString        mArgs;
        };

        auto TryParseMarker(FNativeStringView text, usize pos, FMarker& outMarker) -> bool {
            if (!MatchToken(text, pos, "AE_PER_")) {
                return false;
            }

            usize       cursor   = pos + 7;
            const usize groupEnd = text.Find('_', cursor);
            if (groupEnd == FNativeStringView::npos) {
                return false;
            }

            const FNativeStringView groupToken = text.Substr(cursor, groupEnd - cursor);
            EAutoBindingGroup group{};
            if (!ResolveGroupToken(groupToken, group)) {
                return false;
            }

            cursor                   = groupEnd + 1;
            const auto resourceEnd =
                text.FindFirstOf(FNativeStringView(" \t\r\n("), cursor);
            if (resourceEnd == FNativeStringView::npos) {
                return false;
            }

            const FNativeStringView resourceToken = text.Substr(cursor, resourceEnd - cursor);
            EAutoBindingResource resource{};
            if (!ResolveResourceToken(resourceToken, resource)) {
                return false;
            }

            cursor = resourceEnd;
            while (cursor < text.Length()
                && std::isspace(static_cast<unsigned char>(text[cursor]))) {
                ++cursor;
            }
            if (cursor >= text.Length() || text[cursor] != '(') {
                return false;
            }

            const usize parenStart = cursor;
            int    depth      = 0;
            usize parenEnd   = FNativeStringView::npos;
            for (usize i = parenStart + 1; i < text.Length(); ++i) {
                const char ch = text[i];
                if (ch == '(') {
                    ++depth;
                } else if (ch == ')') {
                    if (depth == 0) {
                        parenEnd = i;
                        break;
                    }
                    --depth;
                }
            }
            if (parenEnd == FNativeStringView::npos) {
                return false;
            }

            outMarker.mStart    = pos;
            outMarker.mEnd      = parenEnd + 1;
            outMarker.mGroup    = group;
            outMarker.mResource = resource;
            outMarker.mArgs =
                FNativeString(text.Substr(parenStart + 1, parenEnd - parenStart - 1));
            return true;
        }

        auto BuildReplacement(const FMarker& marker, Rhi::ERhiBackend backend,
            FAutoBindingLayout& layout, FString& diagnostics) -> FNativeString {
            const u32 groupIndex          = static_cast<u32>(marker.mGroup);
            const u32 resourceIndex       = static_cast<u32>(marker.mResource);
            const u32 bindingIndex        = layout.mCounts[groupIndex][resourceIndex]++;
            layout.mGroupUsed[groupIndex] = true;

            const FNativeString registerSuffix =
                BuildRegisterSuffix(backend, marker.mResource, bindingIndex, groupIndex);

            const FNativeString args = Trim(marker.mArgs.ToView());

            if (marker.mResource == EAutoBindingResource::CBuffer) {
                if (args.IsEmptyString()) {
                    AppendDiagnosticLine(diagnostics, TEXT("AutoBinding: CBUFFER missing name."));
                    return {};
                }
                FNativeString out("cbuffer ");
                out.Append(args);
                out.Append(" : ");
                out.Append(registerSuffix);
                return out;
            }

            if (marker.mResource == EAutoBindingResource::Sampler) {
                FNativeString typeName;
                FNativeString name;
                if (SplitArgs(args.ToView(), typeName, name)) {
                    FNativeString out;
                    out.Append(typeName);
                    out.Append(" ");
                    out.Append(name);
                    out.Append(" : ");
                    out.Append(registerSuffix);
                    return out;
                }
                if (args.IsEmptyString()) {
                    AppendDiagnosticLine(diagnostics, TEXT("AutoBinding: SAMPLER missing name."));
                    return {};
                }
                FNativeString out("SamplerState ");
                out.Append(args);
                out.Append(" : ");
                out.Append(registerSuffix);
                return out;
            }

            FNativeString typeName;
            FNativeString name;
            if (!SplitArgs(args.ToView(), typeName, name)) {
                AppendDiagnosticLine(
                    diagnostics, TEXT("AutoBinding: SRV/UAV requires Type, Name."));
                return {};
            }

            FNativeString out;
            out.Append(typeName);
            out.Append(" ");
            out.Append(name);
            out.Append(" : ");
            out.Append(registerSuffix);
            return out;
        }
    } // namespace

    auto ApplyAutoBindings(const FString& sourcePath, Rhi::ERhiBackend backend,
        FAutoBindingOutput& outResult, FString& diagnostics) -> bool {
        outResult.mApplied    = false;
        outResult.mSourcePath = sourcePath;
        outResult.mLayout     = {};

        FNativeString sourceText;
        if (!Core::Platform::ReadFileTextUtf8(sourcePath, sourceText)) {
            AppendDiagnosticLine(diagnostics, TEXT("AutoBinding: failed to read shader source."));
            return false;
        }

        const FNativeStringView input = sourceText.ToView();
        FNativeString           output;
        output.Reserve(input.Length() + 256);

        usize cursor  = 0;
        bool  applied = false;
        while (cursor < input.Length()) {
            const auto found = input.Find(FNativeStringView("AE_PER_"), cursor);
            if (found == FNativeStringView::npos) {
                output.Append(input.Data() + cursor, input.Length() - cursor);
                break;
            }

            output.Append(input.Data() + cursor, found - cursor);

            FMarker marker{};
            if (!TryParseMarker(input, found, marker)) {
                output.Append(input[found]);
                cursor = found + 1;
                continue;
            }

            const FNativeString replacement =
                BuildReplacement(marker, backend, outResult.mLayout, diagnostics);
            if (!replacement.IsEmptyString()) {
                output.Append(replacement);
                applied = true;
            } else {
                output.Append(input.Data() + marker.mStart, marker.mEnd - marker.mStart);
            }

            cursor = marker.mEnd;
        }

        if (!applied) {
            return true;
        }

        std::filesystem::path       originalPath = ToPath(sourcePath);
        const std::filesystem::path extension    = originalPath.extension();

        FString                     extensionText;
        if (!extension.empty()) {
            extensionText = FromPath(extension);
        } else {
            extensionText = FString(TEXT(".hlsl"));
        }

        const FString tempPath =
            BuildTempOutputPath(sourcePath, FString(TEXT("autobind")), extensionText);

        std::ofstream file(ToPath(tempPath), std::ios::binary | std::ios::trunc);
        if (!file) {
            AppendDiagnosticLine(
                diagnostics, TEXT("AutoBinding: failed to write preprocessed shader."));
            return false;
        }

        FNativeString header;
        header.Append("// AutoBinding generated\n#line 1 \"");
        const auto generic = originalPath.generic_string();
        if (!generic.empty()) {
            header.Append(generic.c_str(), generic.size());
        }
        header.Append("\"\n");
        file.write(header.GetData(), static_cast<std::streamsize>(header.Length()));
        file.write(output.GetData(), static_cast<std::streamsize>(output.Length()));
        file.close();

        outResult.mApplied    = true;
        outResult.mSourcePath = tempPath;
        return true;
    }
} // namespace AltinaEngine::ShaderCompiler::Detail

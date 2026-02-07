#include "ShaderAutoBinding.h"

#include "Platform/PlatformFileSystem.h"
#include "ShaderCompilerUtils.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

namespace AltinaEngine::ShaderCompiler::Detail {
    namespace {
        constexpr u32 kGroupCount = static_cast<u32>(EAutoBindingGroup::Count);

        constexpr u32 kDx11CbvBase[kGroupCount] = { 0U, 4U, 8U };
        constexpr u32 kDx11SrvBase[kGroupCount] = { 0U, 16U, 32U };
        constexpr u32 kDx11SamplerBase[kGroupCount] = { 0U, 4U, 8U };
        constexpr u32 kDx11UavBase[kGroupCount] = { 0U, 4U, 8U };

        template <typename CharT>
        auto ToPathImpl(const Core::Container::TBasicString<CharT>& value) -> std::filesystem::path {
            if constexpr (std::is_same_v<CharT, wchar_t>) {
                return std::filesystem::path(std::wstring(value.GetData(), value.Length()));
            } else {
                return std::filesystem::path(std::string(value.GetData(), value.Length()));
            }
        }

        auto ToPath(const FString& value) -> std::filesystem::path {
            return ToPathImpl(value);
        }

        template <typename CharT>
        auto FromPathImpl(const std::filesystem::path& path) -> Core::Container::TBasicString<CharT> {
            Core::Container::TBasicString<CharT> out;
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

        auto Trim(const std::string& text) -> std::string {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return {};
            }
            const auto last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        auto SplitArgs(const std::string& text, std::string& a, std::string& b) -> bool {
            int depthAngle = 0;
            int depthParen = 0;
            int depthBracket = 0;
            for (size_t i = 0; i < text.size(); ++i) {
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
                    a = Trim(text.substr(0, i));
                    b = Trim(text.substr(i + 1));
                    return !a.empty() && !b.empty();
                }
            }
            return false;
        }

        auto MatchToken(const std::string& text, size_t pos, const char* token) -> bool {
            const size_t len = std::strlen(token);
            return text.compare(pos, len, token) == 0;
        }

        auto GetRegisterChar(EAutoBindingResource resource) -> char {
            switch (resource) {
            case EAutoBindingResource::CBuffer: return 'b';
            case EAutoBindingResource::SRV: return 't';
            case EAutoBindingResource::UAV: return 'u';
            case EAutoBindingResource::Sampler: return 's';
            default: return 't';
            }
        }

        auto GetDx11Base(EAutoBindingResource resource, u32 group) -> u32 {
            switch (resource) {
            case EAutoBindingResource::CBuffer: return kDx11CbvBase[group];
            case EAutoBindingResource::SRV: return kDx11SrvBase[group];
            case EAutoBindingResource::UAV: return kDx11UavBase[group];
            case EAutoBindingResource::Sampler: return kDx11SamplerBase[group];
            default: return 0U;
            }
        }

        auto BuildRegisterSuffix(Rhi::ERhiBackend backend, EAutoBindingResource resource,
            u32 index, u32 group) -> std::string {
            const char reg = GetRegisterChar(resource);
            if (backend == Rhi::ERhiBackend::DirectX11) {
                const u32 dx11Index = GetDx11Base(resource, group) + index;
                return std::string("register(") + reg + std::to_string(dx11Index) + ")";
            }

            return std::string("register(") + reg + std::to_string(index)
                + ", space" + std::to_string(group) + ")";
        }

        auto ResolveGroupToken(const std::string& token, EAutoBindingGroup& outGroup) -> bool {
            if (token == "FRAME") {
                outGroup = EAutoBindingGroup::PerFrame;
                return true;
            }
            if (token == "DRAW") {
                outGroup = EAutoBindingGroup::PerDraw;
                return true;
            }
            if (token == "MATERIAL") {
                outGroup = EAutoBindingGroup::PerMaterial;
                return true;
            }
            return false;
        }

        auto ResolveResourceToken(const std::string& token, EAutoBindingResource& outResource)
            -> bool {
            if (token == "CBUFFER") {
                outResource = EAutoBindingResource::CBuffer;
                return true;
            }
            if (token == "SRV") {
                outResource = EAutoBindingResource::SRV;
                return true;
            }
            if (token == "UAV") {
                outResource = EAutoBindingResource::UAV;
                return true;
            }
            if (token == "SAMPLER") {
                outResource = EAutoBindingResource::Sampler;
                return true;
            }
            return false;
        }

        struct FMarker {
            size_t              mStart = 0;
            size_t              mEnd = 0;
            EAutoBindingGroup   mGroup = EAutoBindingGroup::PerFrame;
            EAutoBindingResource mResource = EAutoBindingResource::SRV;
            std::string         mArgs;
        };

        auto TryParseMarker(const std::string& text, size_t pos, FMarker& outMarker) -> bool {
            if (!MatchToken(text, pos, "AE_PER_")) {
                return false;
            }

            size_t cursor = pos + 7;
            const size_t groupEnd = text.find('_', cursor);
            if (groupEnd == std::string::npos) {
                return false;
            }

            const std::string groupToken = text.substr(cursor, groupEnd - cursor);
            EAutoBindingGroup group{};
            if (!ResolveGroupToken(groupToken, group)) {
                return false;
            }

            cursor = groupEnd + 1;
            const size_t resourceEnd = text.find_first_of(" \t\r\n(", cursor);
            if (resourceEnd == std::string::npos) {
                return false;
            }

            const std::string resourceToken = text.substr(cursor, resourceEnd - cursor);
            EAutoBindingResource resource{};
            if (!ResolveResourceToken(resourceToken, resource)) {
                return false;
            }

            cursor = resourceEnd;
            while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor]))) {
                ++cursor;
            }
            if (cursor >= text.size() || text[cursor] != '(') {
                return false;
            }

            size_t parenStart = cursor;
            int    depth = 0;
            size_t parenEnd = std::string::npos;
            for (size_t i = parenStart + 1; i < text.size(); ++i) {
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
            if (parenEnd == std::string::npos) {
                return false;
            }

            outMarker.mStart    = pos;
            outMarker.mEnd      = parenEnd + 1;
            outMarker.mGroup    = group;
            outMarker.mResource = resource;
            outMarker.mArgs     = text.substr(parenStart + 1, parenEnd - parenStart - 1);
            return true;
        }

        auto BuildReplacement(const FMarker& marker, Rhi::ERhiBackend backend,
            FAutoBindingLayout& layout, FString& diagnostics) -> std::string {
            const u32 groupIndex = static_cast<u32>(marker.mGroup);
            const u32 resourceIndex = static_cast<u32>(marker.mResource);
            const u32 bindingIndex = layout.mCounts[groupIndex][resourceIndex]++;
            layout.mGroupUsed[groupIndex] = true;

            const std::string registerSuffix =
                BuildRegisterSuffix(backend, marker.mResource, bindingIndex, groupIndex);

            const std::string args = Trim(marker.mArgs);

            if (marker.mResource == EAutoBindingResource::CBuffer) {
                if (args.empty()) {
                    AppendDiagnosticLine(diagnostics, TEXT("AutoBinding: CBUFFER missing name."));
                    return {};
                }
                return "cbuffer " + args + " : " + registerSuffix;
            }

            if (marker.mResource == EAutoBindingResource::Sampler) {
                std::string typeName;
                std::string name;
                if (SplitArgs(args, typeName, name)) {
                    return typeName + " " + name + " : " + registerSuffix;
                }
                if (args.empty()) {
                    AppendDiagnosticLine(diagnostics, TEXT("AutoBinding: SAMPLER missing name."));
                    return {};
                }
                return std::string("SamplerState ") + args + " : " + registerSuffix;
            }

            std::string typeName;
            std::string name;
            if (!SplitArgs(args, typeName, name)) {
                AppendDiagnosticLine(diagnostics,
                    TEXT("AutoBinding: SRV/UAV requires Type, Name."));
                return {};
            }

            return typeName + " " + name + " : " + registerSuffix;
        }
    } // namespace

    auto ApplyAutoBindings(const FString& sourcePath, Rhi::ERhiBackend backend,
        FAutoBindingOutput& outResult, FString& diagnostics) -> bool {
        outResult.mApplied   = false;
        outResult.mSourcePath = sourcePath;
        outResult.mLayout    = {};

        FNativeString sourceText;
        if (!Core::Platform::ReadFileTextUtf8(sourcePath, sourceText)) {
            AppendDiagnosticLine(diagnostics, TEXT("AutoBinding: failed to read shader source."));
            return false;
        }

        const std::string input(sourceText.GetData(), sourceText.Length());
        std::string       output;
        output.reserve(input.size() + 256);

        size_t cursor = 0;
        bool   applied = false;
        while (cursor < input.size()) {
            const size_t found = input.find("AE_PER_", cursor);
            if (found == std::string::npos) {
                output.append(input, cursor, std::string::npos);
                break;
            }

            output.append(input, cursor, found - cursor);

            FMarker marker{};
            if (!TryParseMarker(input, found, marker)) {
                output.push_back(input[found]);
                cursor = found + 1;
                continue;
            }

            const std::string replacement =
                BuildReplacement(marker, backend, outResult.mLayout, diagnostics);
            if (!replacement.empty()) {
                output.append(replacement);
                applied = true;
            } else {
                output.append(input, marker.mStart, marker.mEnd - marker.mStart);
            }

            cursor = marker.mEnd;
        }

        if (!applied) {
            return true;
        }

        std::filesystem::path originalPath = ToPath(sourcePath);
        const std::filesystem::path extension = originalPath.extension();

        FString extensionText;
        if (!extension.empty()) {
            extensionText = FromPath(extension);
        } else {
            extensionText = FString(TEXT(".hlsl"));
        }

        const FString tempPath =
            BuildTempOutputPath(sourcePath, FString(TEXT("autobind")), extensionText);

        std::ofstream file(ToPath(tempPath), std::ios::binary | std::ios::trunc);
        if (!file) {
            AppendDiagnosticLine(diagnostics,
                TEXT("AutoBinding: failed to write preprocessed shader."));
            return false;
        }

        const std::string header =
            std::string("// AutoBinding generated\n#line 1 \"")
            + originalPath.generic_string() + "\"\n";
        file.write(header.data(), static_cast<std::streamsize>(header.size()));
        file.write(output.data(), static_cast<std::streamsize>(output.size()));
        file.close();

        outResult.mApplied    = true;
        outResult.mSourcePath = tempPath;
        return true;
    }
} // namespace AltinaEngine::ShaderCompiler::Detail

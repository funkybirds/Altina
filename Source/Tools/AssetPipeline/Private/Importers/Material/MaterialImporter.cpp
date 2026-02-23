#include "Importers/Material/MaterialImporter.h"

#include "Container/String.h"
#include "Container/StringView.h"
#include "Utility/Json.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>

using AltinaEngine::Move;
namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        using Container::FNativeString;
        using Container::FNativeStringView;
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetBoolValue;
        using Core::Utility::Json::GetNumberValue;
        using Core::Utility::Json::GetStringValue;

        auto ToLowerAscii(char value) -> char {
            if (value >= 'A' && value <= 'Z') {
                return static_cast<char>(value - 'A' + 'a');
            }
            return value;
        }

        void ToLowerAscii(std::string& value) {
            for (char& character : value) {
                character = ToLowerAscii(character);
            }
        }

        auto EscapeJson(const std::string& value) -> std::string {
            std::string out;
            out.reserve(value.size() + 8U);
            for (char character : value) {
                switch (character) {
                    case '\\':
                        out += "\\\\";
                        break;
                    case '"':
                        out += "\\\"";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if (static_cast<unsigned char>(character) < 0x20U) {
                            std::ostringstream stream;
                            stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                                   << static_cast<int>(static_cast<unsigned char>(character));
                            out += stream.str();
                        } else {
                            out += character;
                        }
                        break;
                }
            }
            return out;
        }

        auto NormalizeVirtualPath(std::string value) -> std::string {
            for (auto& ch : value) {
                if (ch == '\\') {
                    ch = '/';
                }
            }
            ToLowerAscii(value);
            if (value.rfind("./", 0) == 0) {
                value.erase(0, 2U);
            }
            return value;
        }

        auto ToStdString(const FNativeString& value) -> std::string {
            if (value.IsEmptyString()) {
                return {};
            }
            return std::string(value.GetData(), value.Length());
        }

        auto ToStdString(FNativeStringView value) -> std::string {
            if (value.IsEmpty()) {
                return {};
            }
            return std::string(value.Data(), value.Length());
        }

        auto AssetTypeToString(Asset::EAssetType type) -> const char* {
            switch (type) {
                case Asset::EAssetType::Texture2D:
                    return "Texture2D";
                case Asset::EAssetType::Mesh:
                    return "Mesh";
                case Asset::EAssetType::MaterialTemplate:
                    return "MaterialTemplate";
                case Asset::EAssetType::Shader:
                    return "Shader";
                case Asset::EAssetType::Audio:
                    return "Audio";
                case Asset::EAssetType::Model:
                    return "Model";
                case Asset::EAssetType::Script:
                    return "Script";
                case Asset::EAssetType::Redirector:
                    return "Redirector";
                case Asset::EAssetType::MaterialInstance:
                    return "MaterialInstance";
                default:
                    return "Unknown";
            }
        }

        struct FMaterialShaderRef {
            std::string         AssetPath;
            std::string         Entry;
            Asset::FAssetHandle Handle{};
        };

        struct FMaterialOverrideParam {
            std::string         Name;
            std::string         Type;
            std::vector<double> Values;
            bool                IsScalar  = false;
            bool                IsTexture = false;
            std::string         TextureAssetPath;
            Asset::FAssetHandle TextureHandle{};
        };

        enum class EMaterialRasterFillMode : u8 {
            Solid = 0,
            Wireframe
        };

        enum class EMaterialRasterCullMode : u8 {
            None = 0,
            Front,
            Back
        };

        enum class EMaterialRasterFrontFace : u8 {
            CCW = 0,
            CW
        };

        struct FMaterialRasterOverrides {
            bool                     HasFillMode             = false;
            bool                     HasCullMode             = false;
            bool                     HasFrontFace            = false;
            bool                     HasDepthBias            = false;
            bool                     HasDepthBiasClamp       = false;
            bool                     HasSlopeScaledDepthBias = false;
            bool                     HasDepthClip            = false;
            bool                     HasConservativeRaster   = false;

            EMaterialRasterFillMode  FillMode             = EMaterialRasterFillMode::Solid;
            EMaterialRasterCullMode  CullMode             = EMaterialRasterCullMode::Back;
            EMaterialRasterFrontFace FrontFace            = EMaterialRasterFrontFace::CCW;
            int                      DepthBias            = 0;
            double                   DepthBiasClamp       = 0.0;
            double                   SlopeScaledDepthBias = 0.0;
            bool                     DepthClip            = true;
            bool                     ConservativeRaster   = false;

            [[nodiscard]] auto       HasAny() const noexcept -> bool {
                return HasFillMode || HasCullMode || HasFrontFace || HasDepthBias
                    || HasDepthBiasClamp || HasSlopeScaledDepthBias || HasDepthClip
                    || HasConservativeRaster;
            }
        };

        struct FMaterialPassSource {
            std::string                         Name;
            std::string                         Preset;
            bool                                HasVertex  = false;
            bool                                HasPixel   = false;
            bool                                HasCompute = false;
            FMaterialShaderRef                  Vertex;
            FMaterialShaderRef                  Pixel;
            FMaterialShaderRef                  Compute;
            std::vector<FMaterialOverrideParam> Overrides;
            FMaterialRasterOverrides            RasterOverrides;
        };

        struct FMaterialTemplateSource {
            std::string                           Name;
            std::vector<FMaterialPassSource>      Passes;
            std::vector<std::vector<std::string>> Variants;
        };

        auto ParseShaderStageRef(const FJsonValue& value, FMaterialShaderRef& out) -> bool {
            if (value.Type != EJsonType::Object) {
                return false;
            }

            FNativeString assetText;
            if (!GetStringValue(FindObjectValueInsensitive(value, "Asset"), assetText)) {
                return false;
            }

            FNativeString entryText;
            if (!GetStringValue(FindObjectValueInsensitive(value, "Entry"), entryText)) {
                return false;
            }

            out.AssetPath = NormalizeVirtualPath(ToStdString(assetText));
            out.Entry     = ToStdString(entryText);
            return !out.AssetPath.empty() && !out.Entry.empty();
        }

        auto FindRasterOverridesValue(const FJsonValue& passObject) -> const FJsonValue* {
            if (const auto* value = FindObjectValueInsensitive(passObject, "RasterOverrides")) {
                return value;
            }
            if (const auto* value = FindObjectValueInsensitive(passObject, "Raster_Overrides")) {
                return value;
            }
            if (const auto* value =
                    FindObjectValueInsensitive(passObject, "RasterStateOverrides")) {
                return value;
            }
            if (const auto* value =
                    FindObjectValueInsensitive(passObject, "Raster_State_Overrides")) {
                return value;
            }
            return nullptr;
        }

        auto NormalizeKey(std::string value) -> std::string {
            std::string out;
            out.reserve(value.size());
            for (const char ch : value) {
                if (ch == '_' || ch == '-') {
                    continue;
                }
                out.push_back(ToLowerAscii(ch));
            }
            return out;
        }

        auto TryParseRasterFillMode(const std::string& value, EMaterialRasterFillMode& out)
            -> bool {
            const auto token = NormalizeKey(value);
            if (token == "solid") {
                out = EMaterialRasterFillMode::Solid;
                return true;
            }
            if (token == "wireframe") {
                out = EMaterialRasterFillMode::Wireframe;
                return true;
            }
            return false;
        }

        auto TryParseRasterCullMode(const std::string& value, EMaterialRasterCullMode& out)
            -> bool {
            const auto token = NormalizeKey(value);
            if (token == "none") {
                out = EMaterialRasterCullMode::None;
                return true;
            }
            if (token == "front") {
                out = EMaterialRasterCullMode::Front;
                return true;
            }
            if (token == "back") {
                out = EMaterialRasterCullMode::Back;
                return true;
            }
            return false;
        }

        auto TryParseRasterFrontFace(const std::string& value, EMaterialRasterFrontFace& out)
            -> bool {
            const auto token = NormalizeKey(value);
            if (token == "ccw") {
                out = EMaterialRasterFrontFace::CCW;
                return true;
            }
            if (token == "cw") {
                out = EMaterialRasterFrontFace::CW;
                return true;
            }
            return false;
        }

        auto RasterFillModeToString(EMaterialRasterFillMode mode) -> const char* {
            switch (mode) {
                case EMaterialRasterFillMode::Wireframe:
                    return "Wireframe";
                case EMaterialRasterFillMode::Solid:
                default:
                    return "Solid";
            }
        }

        auto RasterCullModeToString(EMaterialRasterCullMode mode) -> const char* {
            switch (mode) {
                case EMaterialRasterCullMode::None:
                    return "None";
                case EMaterialRasterCullMode::Front:
                    return "Front";
                case EMaterialRasterCullMode::Back:
                default:
                    return "Back";
            }
        }

        auto RasterFrontFaceToString(EMaterialRasterFrontFace mode) -> const char* {
            switch (mode) {
                case EMaterialRasterFrontFace::CW:
                    return "CW";
                case EMaterialRasterFrontFace::CCW:
                default:
                    return "CCW";
            }
        }

        auto ParseMaterialSource(const std::vector<u8>& sourceBytes, FMaterialTemplateSource& out,
            std::string& outError) -> bool {
            out = {};
            if (sourceBytes.empty()) {
                outError = "Material source empty.";
                return false;
            }

            std::string   text(sourceBytes.begin(), sourceBytes.end());
            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                outError = "Material JSON parse failed.";
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                outError = "Material JSON root invalid.";
                return false;
            }

            FNativeString nameText;
            if (GetStringValue(FindObjectValueInsensitive(*root, "Name"), nameText)) {
                out.Name = ToStdString(nameText);
            }

            const FJsonValue* passesValue = FindObjectValueInsensitive(*root, "Passes");
            if (passesValue == nullptr || passesValue->Type != EJsonType::Object) {
                outError = "Material Passes missing.";
                return false;
            }

            for (const auto& pair : passesValue->Object) {
                if (pair.Value == nullptr || pair.Value->Type != EJsonType::Object) {
                    continue;
                }

                FMaterialPassSource pass{};
                pass.Name = ToStdString(pair.Key);
                if (pass.Name.empty()) {
                    continue;
                }

                FNativeString presetText;
                if (GetStringValue(FindObjectValueInsensitive(*pair.Value, "Preset"), presetText)) {
                    pass.Preset = ToStdString(presetText);
                }

                const bool        hasPreset    = !pass.Preset.empty();
                const FJsonValue* shadersValue = FindObjectValueInsensitive(*pair.Value, "Shaders");
                if (!hasPreset) {
                    if (shadersValue == nullptr || shadersValue->Type != EJsonType::Object) {
                        outError = "Material Pass shaders missing.";
                        return false;
                    }
                }

                if (shadersValue != nullptr && shadersValue->Type == EJsonType::Object) {
                    if (const FJsonValue* vsValue =
                            FindObjectValueInsensitive(*shadersValue, "vs")) {
                        pass.HasVertex = ParseShaderStageRef(*vsValue, pass.Vertex);
                    }
                    if (const FJsonValue* psValue =
                            FindObjectValueInsensitive(*shadersValue, "ps")) {
                        pass.HasPixel = ParseShaderStageRef(*psValue, pass.Pixel);
                    }
                    if (const FJsonValue* csValue =
                            FindObjectValueInsensitive(*shadersValue, "cs")) {
                        pass.HasCompute = ParseShaderStageRef(*csValue, pass.Compute);
                    }
                }

                if (!hasPreset && !pass.HasVertex && !pass.HasCompute) {
                    outError = "Material Pass requires at least VS or CS.";
                    return false;
                }

                if (const FJsonValue* overridesValue =
                        FindObjectValueInsensitive(*pair.Value, "Overrides")) {
                    if (overridesValue->Type == EJsonType::Object) {
                        for (const auto& overridePair : overridesValue->Object) {
                            if (overridePair.Value == nullptr
                                || overridePair.Value->Type != EJsonType::Object) {
                                continue;
                            }

                            FNativeString typeText;
                            if (!GetStringValue(
                                    FindObjectValueInsensitive(*overridePair.Value, "Type"),
                                    typeText)) {
                                continue;
                            }

                            const FJsonValue* valueNode =
                                FindObjectValueInsensitive(*overridePair.Value, "Value");
                            if (valueNode == nullptr) {
                                continue;
                            }

                            FMaterialOverrideParam overrideParam{};
                            overrideParam.Name = ToStdString(overridePair.Key);
                            overrideParam.Type = ToStdString(typeText);
                            if (overrideParam.Name.empty() || overrideParam.Type.empty()) {
                                continue;
                            }

                            const auto typeLower = NormalizeVirtualPath(overrideParam.Type);
                            const bool isTextureType =
                                (typeLower == "texture2d" || typeLower == "texture");

                            if (isTextureType && valueNode->Type == EJsonType::String) {
                                const std::string assetPath =
                                    NormalizeVirtualPath(ToStdString(valueNode->String));
                                if (!assetPath.empty()) {
                                    overrideParam.IsTexture        = true;
                                    overrideParam.TextureAssetPath = assetPath;
                                }
                            } else if (valueNode->Type == EJsonType::Number) {
                                overrideParam.IsScalar = true;
                                overrideParam.Values.push_back(valueNode->Number);
                            } else if (valueNode->Type == EJsonType::Array) {
                                bool valid = true;
                                for (const auto* entry : valueNode->Array) {
                                    if (entry == nullptr || entry->Type != EJsonType::Number) {
                                        valid = false;
                                        break;
                                    }
                                    overrideParam.Values.push_back(entry->Number);
                                }
                                if (!valid) {
                                    continue;
                                }
                            } else if (!isTextureType) {
                                continue;
                            }

                            pass.Overrides.push_back(Move(overrideParam));
                        }
                    }
                }

                if (const FJsonValue* rasterValue = FindRasterOverridesValue(*pair.Value)) {
                    if (rasterValue->Type == EJsonType::Object) {
                        for (const auto& rasterPair : rasterValue->Object) {
                            if (rasterPair.Value == nullptr) {
                                continue;
                            }

                            const std::string keyNorm = NormalizeKey(ToStdString(rasterPair.Key));
                            if (keyNorm.empty()) {
                                continue;
                            }

                            if (rasterPair.Value->Type == EJsonType::String) {
                                const std::string valueText = ToStdString(rasterPair.Value->String);

                                if (keyNorm == "fillmode" || keyNorm == "fill") {
                                    EMaterialRasterFillMode mode{};
                                    if (TryParseRasterFillMode(valueText, mode)) {
                                        pass.RasterOverrides.HasFillMode = true;
                                        pass.RasterOverrides.FillMode    = mode;
                                    }
                                    continue;
                                }

                                if (keyNorm == "cullmode" || keyNorm == "cull") {
                                    EMaterialRasterCullMode mode{};
                                    if (TryParseRasterCullMode(valueText, mode)) {
                                        pass.RasterOverrides.HasCullMode = true;
                                        pass.RasterOverrides.CullMode    = mode;
                                    }
                                    continue;
                                }

                                if (keyNorm == "frontface" || keyNorm == "front") {
                                    EMaterialRasterFrontFace mode{};
                                    if (TryParseRasterFrontFace(valueText, mode)) {
                                        pass.RasterOverrides.HasFrontFace = true;
                                        pass.RasterOverrides.FrontFace    = mode;
                                    }
                                    continue;
                                }
                            }

                            if (rasterPair.Value->Type == EJsonType::Number) {
                                double number = 0.0;
                                if (!GetNumberValue(rasterPair.Value, number)) {
                                    continue;
                                }

                                if (keyNorm == "depthbias") {
                                    pass.RasterOverrides.HasDepthBias = true;
                                    pass.RasterOverrides.DepthBias    = static_cast<int>(number);
                                    continue;
                                }

                                if (keyNorm == "depthbiasclamp") {
                                    pass.RasterOverrides.HasDepthBiasClamp = true;
                                    pass.RasterOverrides.DepthBiasClamp    = number;
                                    continue;
                                }

                                if (keyNorm == "slopescaleddepthbias") {
                                    pass.RasterOverrides.HasSlopeScaledDepthBias = true;
                                    pass.RasterOverrides.SlopeScaledDepthBias    = number;
                                    continue;
                                }
                            }

                            if (rasterPair.Value->Type == EJsonType::Bool) {
                                bool flag = false;
                                if (!GetBoolValue(rasterPair.Value, flag)) {
                                    continue;
                                }

                                if (keyNorm == "depthclip") {
                                    pass.RasterOverrides.HasDepthClip = true;
                                    pass.RasterOverrides.DepthClip    = flag;
                                    continue;
                                }

                                if (keyNorm == "conservativeraster") {
                                    pass.RasterOverrides.HasConservativeRaster = true;
                                    pass.RasterOverrides.ConservativeRaster    = flag;
                                    continue;
                                }
                            }
                        }
                    }
                }

                out.Passes.push_back(Move(pass));
            }

            const FJsonValue* variantsValue =
                FindObjectValueInsensitive(*root, "Precompile_Variants");
            if (variantsValue != nullptr && variantsValue->Type == EJsonType::Array) {
                for (const auto* variantValue : variantsValue->Array) {
                    if (variantValue == nullptr || variantValue->Type != EJsonType::Array) {
                        continue;
                    }
                    std::vector<std::string> variant;
                    for (const auto* item : variantValue->Array) {
                        if (item == nullptr || item->Type != EJsonType::String) {
                            continue;
                        }
                        const std::string name = ToStdString(item->String);
                        if (!name.empty()) {
                            variant.push_back(name);
                        }
                    }
                    out.Variants.push_back(Move(variant));
                }
            }

            return !out.Passes.empty();
        }

        auto ResolveMaterialDependencies(FMaterialTemplateSource&       material,
            const std::unordered_map<std::string, const FAssetRecord*>& assetsByPath,
            std::vector<Asset::FAssetHandle>& outDeps, Asset::FMaterialDesc& outDesc,
            std::string& outError) -> bool {
            outDeps.clear();
            outDesc = {};

            std::unordered_set<std::string> seen;

            u32                             shaderCount = 0U;
            for (auto& pass : material.Passes) {
                auto resolve = [&](FMaterialShaderRef& shaderRef) -> bool {
                    const auto it = assetsByPath.find(shaderRef.AssetPath);
                    if (it == assetsByPath.end()) {
                        outError = "Material shader asset not found: " + shaderRef.AssetPath;
                        return false;
                    }
                    const auto* record = it->second;
                    if (record == nullptr || record->Type != Asset::EAssetType::Shader) {
                        outError = "Material shader asset invalid: " + shaderRef.AssetPath;
                        return false;
                    }

                    shaderRef.Handle.Uuid = record->Uuid;
                    shaderRef.Handle.Type = record->Type;
                    ++shaderCount;

                    const std::string uuidText = ToStdString(record->Uuid.ToNativeString());
                    if (seen.insert(uuidText).second) {
                        outDeps.push_back(shaderRef.Handle);
                    }
                    return true;
                };

                if (!pass.Preset.empty()) {
                    for (auto& param : pass.Overrides) {
                        if (!param.IsTexture || param.TextureAssetPath.empty()) {
                            continue;
                        }
                        const auto it = assetsByPath.find(param.TextureAssetPath);
                        if (it == assetsByPath.end()) {
                            outError =
                                "Material texture asset not found: " + param.TextureAssetPath;
                            return false;
                        }
                        const auto* record = it->second;
                        if (record == nullptr || record->Type != Asset::EAssetType::Texture2D) {
                            outError = "Material texture asset invalid: " + param.TextureAssetPath;
                            return false;
                        }
                        param.TextureHandle.Uuid   = record->Uuid;
                        param.TextureHandle.Type   = record->Type;
                        const std::string uuidText = ToStdString(record->Uuid.ToNativeString());
                        if (seen.insert(uuidText).second) {
                            outDeps.push_back(param.TextureHandle);
                        }
                    }
                } else {
                    if (pass.HasVertex && !resolve(pass.Vertex)) {
                        return false;
                    }
                    if (pass.HasPixel && !resolve(pass.Pixel)) {
                        return false;
                    }
                    if (pass.HasCompute && !resolve(pass.Compute)) {
                        return false;
                    }
                }
            }

            outDesc.PassCount    = static_cast<u32>(material.Passes.size());
            outDesc.ShaderCount  = shaderCount;
            outDesc.VariantCount = static_cast<u32>(material.Variants.size());
            return true;
        }

        auto WriteMaterialCookedJson(const FMaterialTemplateSource& material, std::string& outJson)
            -> bool {
            std::ostringstream stream;
            stream << "{\n";
            if (!material.Name.empty()) {
                stream << "  \"Name\": \"" << EscapeJson(material.Name) << "\",\n";
            }
            stream << "  \"Passes\": {\n";
            for (size_t passIndex = 0; passIndex < material.Passes.size(); ++passIndex) {
                const auto& pass = material.Passes[passIndex];
                stream << "    \"" << EscapeJson(pass.Name) << "\": {\n";
                if (!pass.Preset.empty()) {
                    stream << "      \"Preset\": \"" << EscapeJson(pass.Preset) << "\"";
                } else {
                    stream << "      \"Shaders\": {\n";

                    auto WriteShader = [&](const char* key, const FMaterialShaderRef& ref,
                                           bool emit, bool isLast) {
                        if (!emit) {
                            return;
                        }
                        const std::string uuidText = ToStdString(ref.Handle.Uuid.ToNativeString());
                        stream << "        \"" << key << "\": { \"Uuid\": \""
                               << EscapeJson(uuidText) << "\", \"Type\": \""
                               << AssetTypeToString(ref.Handle.Type) << "\", \"Entry\": \""
                               << EscapeJson(ref.Entry) << "\" }";
                        if (!isLast) {
                            stream << ",";
                        }
                        stream << "\n";
                    };

                    const bool hasVs  = pass.HasVertex;
                    const bool hasPs  = pass.HasPixel;
                    const bool hasCs  = pass.HasCompute;
                    const bool vsLast = !hasPs && !hasCs;
                    const bool psLast = !hasCs;

                    if (hasVs) {
                        WriteShader("vs", pass.Vertex, true, vsLast);
                    }
                    if (hasPs) {
                        WriteShader("ps", pass.Pixel, true, psLast);
                    }
                    if (hasCs) {
                        WriteShader("cs", pass.Compute, true, true);
                    }

                    stream << "      }";
                }

                if (pass.RasterOverrides.HasAny()) {
                    stream << ",\n";
                    stream << "      \"RasterOverrides\": {\n";

                    // Write only overridden fields; keep deterministic ordering.
                    std::vector<std::string> lines;
                    lines.reserve(8);
                    if (pass.RasterOverrides.HasFillMode) {
                        lines.push_back(std::string("        \"FillMode\": \"")
                            + RasterFillModeToString(pass.RasterOverrides.FillMode) + "\"");
                    }
                    if (pass.RasterOverrides.HasCullMode) {
                        lines.push_back(std::string("        \"CullMode\": \"")
                            + RasterCullModeToString(pass.RasterOverrides.CullMode) + "\"");
                    }
                    if (pass.RasterOverrides.HasFrontFace) {
                        lines.push_back(std::string("        \"FrontFace\": \"")
                            + RasterFrontFaceToString(pass.RasterOverrides.FrontFace) + "\"");
                    }
                    if (pass.RasterOverrides.HasDepthBias) {
                        lines.push_back("        \"DepthBias\": "
                            + std::to_string(pass.RasterOverrides.DepthBias));
                    }
                    if (pass.RasterOverrides.HasDepthBiasClamp) {
                        std::ostringstream s;
                        s << "        \"DepthBiasClamp\": " << pass.RasterOverrides.DepthBiasClamp;
                        lines.push_back(s.str());
                    }
                    if (pass.RasterOverrides.HasSlopeScaledDepthBias) {
                        std::ostringstream s;
                        s << "        \"SlopeScaledDepthBias\": "
                          << pass.RasterOverrides.SlopeScaledDepthBias;
                        lines.push_back(s.str());
                    }
                    if (pass.RasterOverrides.HasDepthClip) {
                        lines.push_back(std::string("        \"DepthClip\": ")
                            + (pass.RasterOverrides.DepthClip ? "true" : "false"));
                    }
                    if (pass.RasterOverrides.HasConservativeRaster) {
                        lines.push_back(std::string("        \"ConservativeRaster\": ")
                            + (pass.RasterOverrides.ConservativeRaster ? "true" : "false"));
                    }

                    for (size_t i = 0; i < lines.size(); ++i) {
                        stream << lines[i];
                        if (i + 1 < lines.size()) {
                            stream << ",";
                        }
                        stream << "\n";
                    }

                    stream << "      }";
                }

                if (!pass.Overrides.empty()) {
                    stream << ",\n";
                    stream << "      \"Overrides\": {\n";
                    for (size_t overrideIndex = 0; overrideIndex < pass.Overrides.size();
                        ++overrideIndex) {
                        const auto& overrideParam = pass.Overrides[overrideIndex];
                        stream << "        \"" << EscapeJson(overrideParam.Name) << "\": { ";
                        stream << "\"Type\": \"" << EscapeJson(overrideParam.Type) << "\", ";
                        stream << "\"Value\": ";
                        if (overrideParam.IsTexture) {
                            const std::string uuidText =
                                ToStdString(overrideParam.TextureHandle.Uuid.ToNativeString());
                            stream << "{ \"Uuid\": \"" << EscapeJson(uuidText) << "\", \"Type\": \""
                                   << AssetTypeToString(overrideParam.TextureHandle.Type) << "\" }";
                        } else if (overrideParam.IsScalar && !overrideParam.Values.empty()) {
                            stream << overrideParam.Values[0];
                        } else {
                            stream << "[";
                            for (size_t valueIndex = 0; valueIndex < overrideParam.Values.size();
                                ++valueIndex) {
                                stream << overrideParam.Values[valueIndex];
                                if (valueIndex + 1 < overrideParam.Values.size()) {
                                    stream << ", ";
                                }
                            }
                            stream << "]";
                        }
                        stream << " }";
                        if (overrideIndex + 1 < pass.Overrides.size()) {
                            stream << ",";
                        }
                        stream << "\n";
                    }
                    stream << "      }";
                } else {
                    // No parameter overrides.
                }
                stream << "\n";
                stream << "    }";
                if (passIndex + 1 < material.Passes.size()) {
                    stream << ",";
                }
                stream << "\n";
            }
            stream << "  },\n";
            stream << "  \"Precompile_Variants\": [\n";
            for (size_t variantIndex = 0; variantIndex < material.Variants.size(); ++variantIndex) {
                const auto& variant = material.Variants[variantIndex];
                stream << "    [";
                for (size_t valueIndex = 0; valueIndex < variant.size(); ++valueIndex) {
                    stream << "\"" << EscapeJson(variant[valueIndex]) << "\"";
                    if (valueIndex + 1 < variant.size()) {
                        stream << ", ";
                    }
                }
                stream << "]";
                if (variantIndex + 1 < material.Variants.size()) {
                    stream << ",";
                }
                stream << "\n";
            }
            stream << "  ]\n";
            stream << "}\n";

            outJson = stream.str();
            return true;
        }
    } // namespace

    auto CookMaterial(const std::vector<u8>&                        sourceBytes,
        const std::unordered_map<std::string, const FAssetRecord*>& assetsByPath,
        std::vector<Asset::FAssetHandle>& outDeps, Asset::FMaterialDesc& outDesc,
        std::vector<u8>& outCooked) -> bool {
        std::string             error;
        FMaterialTemplateSource material;
        if (!ParseMaterialSource(sourceBytes, material, error)) {
            return false;
        }

        if (!ResolveMaterialDependencies(material, assetsByPath, outDeps, outDesc, error)) {
            return false;
        }

        std::string cookedJson;
        if (!WriteMaterialCookedJson(material, cookedJson)) {
            return false;
        }

        outCooked.assign(cookedJson.begin(), cookedJson.end());
        return true;
    }
} // namespace AltinaEngine::Tools::AssetPipeline

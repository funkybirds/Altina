#include "Asset/AssetBinary.h"
#include "Asset/AssetBundle.h"
#include "Asset/AssetTypes.h"
#include "AssetToolIO.h"
#include "AssetToolTypes.h"
#include "Importers/Audio/AudioImporter.h"
#include "Importers/Material/MaterialImporter.h"
#include "Importers/Mesh/MeshImporter.h"
#include "Importers/Model/ModelImporter.h"
#include "Importers/Shader/ShaderImporter.h"
#include "Importers/Texture/TextureImporter.h"
#include "Types/Aliases.h"
#include "Utility/Json.h"
#include "Utility/Uuid.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using AltinaEngine::Move;
namespace AltinaEngine::Tools::AssetPipeline {
    namespace Container = Core::Container;
    namespace {
        using Container::FNativeString;
        using Container::FNativeStringView;
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetNumberValue;
        using Core::Utility::Json::GetStringValue;

        struct FToolPaths {
            std::filesystem::path Root;
            std::filesystem::path BuildRoot;
            std::filesystem::path CookedRoot;
            std::filesystem::path CacheRoot;
            std::filesystem::path CookCachePath;
        };

        struct FCommandLine {
            std::string                                  Command;
            std::unordered_map<std::string, std::string> Options;
        };

        struct FRegistryEntry {
            std::string                      Uuid;
            Asset::EAssetType                Type = Asset::EAssetType::Unknown;
            std::string                      VirtualPath;
            std::string                      CookedPath;
            std::vector<Asset::FAssetHandle> Dependencies;
            Asset::FTexture2DDesc            TextureDesc{};
            bool                             HasTextureDesc = false;
            Asset::FMeshDesc                 MeshDesc{};
            bool                             HasMeshDesc = false;
            Asset::FMaterialDesc             MaterialDesc{};
            bool                             HasMaterialDesc = false;
            Asset::FShaderDesc               ShaderDesc{};
            bool                             HasShaderDesc = false;
            Asset::FModelDesc                ModelDesc{};
            bool                             HasModelDesc = false;
            Asset::FAudioDesc                AudioDesc{};
            bool                             HasAudioDesc = false;
            std::string                      ScriptAssemblyPath;
            std::string                      ScriptTypeName;
            bool                             HasScriptDesc = false;
        };

        struct FCookCacheEntry {
            std::string Uuid;
            std::string CookKey;
            std::string SourcePath;
            std::string CookedPath;
            std::string LastCooked;
        };

        enum class EMetaWriteMode {
            MissingOnly,
            Always
        };

        constexpr u32 kCookPipelineVersion = 4;
        constexpr u64 kFnvOffsetBasis      = 14695981039346656037ULL;
        constexpr u64 kFnvPrime            = 1099511628211ULL;
        auto          ToLowerAscii(char value) -> char {
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

        auto NormalizePath(const std::filesystem::path& path) -> std::string {
            std::string out = path.generic_string();
            if (out.size() >= 2U && out[0] == '.' && out[1] == '/') {
                out.erase(0, 2U);
            }
            return out;
        }

        auto MakeRelativePath(const std::filesystem::path& root, const std::filesystem::path& path)
            -> std::string {
            std::error_code       ec;
            std::filesystem::path rel = std::filesystem::relative(path, root, ec);
            if (ec) {
                return NormalizePath(path);
            }
            return NormalizePath(rel);
        }

        auto ToStdString(const FNativeString& value) -> std::string {
            if (value.IsEmptyString()) {
                return {};
            }
            return std::string(value.GetData(), value.Length());
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

        auto ParseAssetType(std::string value) -> Asset::EAssetType {
            ToLowerAscii(value);
            if (value == "texture2d") {
                return Asset::EAssetType::Texture2D;
            }
            if (value == "mesh") {
                return Asset::EAssetType::Mesh;
            }
            if (value == "material" || value == "materialtemplate") {
                return Asset::EAssetType::MaterialTemplate;
            }
            if (value == "materialinstance") {
                return Asset::EAssetType::MaterialInstance;
            }
            if (value == "shader") {
                return Asset::EAssetType::Shader;
            }
            if (value == "model") {
                return Asset::EAssetType::Model;
            }
            if (value == "audio") {
                return Asset::EAssetType::Audio;
            }
            if (value == "script") {
                return Asset::EAssetType::Script;
            }
            if (value == "redirector") {
                return Asset::EAssetType::Redirector;
            }
            return Asset::EAssetType::Unknown;
        }

        auto GetImporterName(Asset::EAssetType type) -> std::string {
            switch (type) {
                case Asset::EAssetType::Texture2D:
                    return "TextureImporter";
                case Asset::EAssetType::Mesh:
                    return "MeshImporter";
                case Asset::EAssetType::MaterialTemplate:
                    return "MaterialImporter";
                case Asset::EAssetType::Shader:
                    return "ShaderImporter";
                case Asset::EAssetType::Audio:
                    return "AudioImporter";
                case Asset::EAssetType::Model:
                    return "ModelImporter";
                case Asset::EAssetType::Script:
                    return "ScriptImporter";
                case Asset::EAssetType::MaterialInstance:
                    return "MaterialInstanceImporter";
                default:
                    return "UnknownImporter";
            }
        }

        auto IsTextureExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
        }

        auto IsMeshExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb";
        }

        auto IsModelExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".model";
        }

        auto IsMaterialExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".material";
        }

        auto IsShaderExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".hlsl" || ext == ".slang";
        }

        auto IsAudioExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".wav" || ext == ".ogg";
        }

        auto IsScriptExtension(const std::filesystem::path& path) -> bool {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext == ".script";
        }

        auto GuessAssetType(const std::filesystem::path& path) -> Asset::EAssetType {
            if (IsTextureExtension(path)) {
                return Asset::EAssetType::Texture2D;
            }
            if (IsMeshExtension(path)) {
                return Asset::EAssetType::Mesh;
            }
            if (IsModelExtension(path)) {
                return Asset::EAssetType::Model;
            }
            if (IsMaterialExtension(path)) {
                return Asset::EAssetType::MaterialTemplate;
            }
            if (IsShaderExtension(path)) {
                return Asset::EAssetType::Shader;
            }
            if (IsAudioExtension(path)) {
                return Asset::EAssetType::Audio;
            }
            if (IsScriptExtension(path)) {
                return Asset::EAssetType::Script;
            }
            return Asset::EAssetType::Unknown;
        }

        auto ParseScriptDescriptor(const std::vector<u8>& bytes, std::string& outAssemblyPath,
            std::string& outTypeName) -> bool {
            outAssemblyPath.clear();
            outTypeName.clear();
            if (bytes.empty()) {
                return false;
            }

            std::string   text(bytes.begin(), bytes.end());
            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                return false;
            }

            FNativeString assemblyText;
            FNativeString typeText;
            (void)GetStringValue(FindObjectValueInsensitive(*root, "AssemblyPath"), assemblyText);
            (void)GetStringValue(FindObjectValueInsensitive(*root, "TypeName"), typeText);

            if (typeText.IsEmptyString()) {
                return false;
            }

            outAssemblyPath = ToStdString(assemblyText);
            outTypeName     = ToStdString(typeText);
            return true;
        }
        void HashBytes(u64& hash, const void* data, size_t size) {
            if (data == nullptr || size == 0U) {
                return;
            }

            const auto* bytes = static_cast<const u8*>(data);
            for (size_t i = 0; i < size; ++i) {
                hash ^= static_cast<u64>(bytes[i]);
                hash *= kFnvPrime;
            }
        }

        void HashString(u64& hash, const std::string& value) {
            HashBytes(hash, value.data(), value.size());
        }

        auto FormatHex64(u64 value) -> std::string {
            std::ostringstream stream;
            stream << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << value;
            return stream.str();
        }

        auto BuildCookKey(const std::vector<u8>& sourceBytes, const FAssetRecord& asset,
            const std::string& platform) -> std::string {
            u64 hash = kFnvOffsetBasis;
            HashBytes(hash, &kCookPipelineVersion, sizeof(kCookPipelineVersion));
            HashBytes(hash, sourceBytes.data(), sourceBytes.size());
            HashString(hash, asset.ImporterName);
            HashBytes(hash, &asset.ImporterVersion, sizeof(asset.ImporterVersion));
            const u8 typeValue = static_cast<u8>(asset.Type);
            HashBytes(hash, &typeValue, sizeof(typeValue));
            HashString(hash, platform);
            return "fnv1a64:" + FormatHex64(hash);
        }

        auto BuildCookKeyWithExtras(const std::vector<u8>& sourceBytes,
            const std::vector<u8>& extraBytes, const FAssetRecord& asset,
            const std::string& platform) -> std::string {
            u64 hash = kFnvOffsetBasis;
            HashBytes(hash, &kCookPipelineVersion, sizeof(kCookPipelineVersion));
            HashBytes(hash, sourceBytes.data(), sourceBytes.size());
            if (!extraBytes.empty()) {
                HashBytes(hash, extraBytes.data(), extraBytes.size());
            }
            HashString(hash, asset.ImporterName);
            HashBytes(hash, &asset.ImporterVersion, sizeof(asset.ImporterVersion));
            const u8 typeValue = static_cast<u8>(asset.Type);
            HashBytes(hash, &typeValue, sizeof(typeValue));
            HashString(hash, platform);
            return "fnv1a64:" + FormatHex64(hash);
        }

        auto GetUtcTimestamp() -> std::string {
            using clock                 = std::chrono::system_clock;
            const auto        now       = clock::now();
            const std::time_t timeValue = clock::to_time_t(now);
            std::tm           utc{};
#if defined(_WIN32)
            gmtime_s(&utc, &timeValue);
#else
            gmtime_r(&timeValue, &utc);
#endif
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min,
                utc.tm_sec);
            return std::string(buffer);
        }

        struct FBundledAsset {
            FUuid             Uuid;
            std::string       UuidText;
            Asset::EAssetType Type = Asset::EAssetType::Unknown;
            std::string       CookedPath;
            std::vector<u8>   Data;
        };

        void WriteBundleUuid(Asset::FBundleIndexEntry& entry, const FUuid& uuid) {
            const auto& bytes = uuid.GetBytes();
            for (usize index = 0; index < FUuid::kByteCount; ++index) {
                entry.Uuid[index] = bytes[index];
            }
        }

        auto LoadRegistryAssets(const std::filesystem::path& registryPath,
            const std::filesystem::path& cookedRoot, std::vector<FBundledAsset>& outAssets)
            -> bool {
            outAssets.clear();

            std::string text;
            if (!ReadFileText(registryPath, text)) {
                return false;
            }

            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                return false;
            }

            const FJsonValue* assetsValue = FindObjectValueInsensitive(*root, "Assets");
            if (assetsValue == nullptr || assetsValue->Type != EJsonType::Array) {
                return false;
            }

            for (const auto* assetValue : assetsValue->Array) {
                if (assetValue == nullptr || assetValue->Type != EJsonType::Object) {
                    continue;
                }

                FNativeString uuidText;
                FNativeString typeText;
                FNativeString cookedText;

                if (!GetStringValue(FindObjectValueInsensitive(*assetValue, "Uuid"), uuidText)) {
                    continue;
                }
                if (!GetStringValue(FindObjectValueInsensitive(*assetValue, "Type"), typeText)) {
                    continue;
                }
                if (!GetStringValue(
                        FindObjectValueInsensitive(*assetValue, "CookedPath"), cookedText)) {
                    continue;
                }

                FUuid uuid;
                if (!FUuid::TryParse(
                        FNativeStringView(uuidText.GetData(), uuidText.Length()), uuid)) {
                    continue;
                }

                FBundledAsset asset{};
                asset.Uuid     = uuid;
                asset.UuidText = ToStdString(uuidText);
                asset.Type     = ParseAssetType(ToStdString(typeText));
                if (asset.Type == Asset::EAssetType::Unknown) {
                    continue;
                }
                asset.CookedPath = ToStdString(cookedText);

                const std::filesystem::path sourcePath = cookedRoot / asset.CookedPath;
                if (!ReadFileBytes(sourcePath, asset.Data)) {
                    std::cerr << "Failed to read cooked asset: " << sourcePath.string() << "\n";
                    continue;
                }

                outAssets.push_back(Move(asset));
            }

            return !outAssets.empty();
        }
        auto LoadMeta(const std::filesystem::path& metaPath, FUuid& outUuid,
            Asset::EAssetType& outType, std::string& outVirtualPath) -> bool {
            std::string text;
            if (!ReadFileText(metaPath, text)) {
                return false;
            }

            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                return false;
            }

            FNativeString uuidText;
            if (!GetStringValue(FindObjectValueInsensitive(*root, "Uuid"), uuidText)) {
                return false;
            }

            const FNativeStringView uuidView(uuidText.GetData(), uuidText.Length());
            if (!FUuid::TryParse(uuidView, outUuid)) {
                return false;
            }

            FNativeString typeText;
            if (GetStringValue(FindObjectValueInsensitive(*root, "Type"), typeText)) {
                outType = ParseAssetType(ToStdString(typeText));
            }

            FNativeString pathText;
            if (GetStringValue(FindObjectValueInsensitive(*root, "VirtualPath"), pathText)) {
                outVirtualPath = ToStdString(pathText);
                ToLowerAscii(outVirtualPath);
            }

            return true;
        }

        auto WriteMetaFile(const FAssetRecord& asset) -> bool {
            const std::string  uuid = ToStdString(asset.Uuid.ToNativeString());

            std::ostringstream stream;
            stream << "{\n";
            stream << "  \"Uuid\": \"" << EscapeJson(uuid) << "\",\n";
            stream << "  \"Type\": \"" << AssetTypeToString(asset.Type) << "\",\n";
            stream << "  \"VirtualPath\": \"" << EscapeJson(asset.VirtualPath) << "\",\n";
            stream << "  \"SourcePath\": \"" << EscapeJson(asset.SourcePathRel) << "\",\n";
            stream << "  \"Importer\": \"" << EscapeJson(asset.ImporterName) << "\",\n";
            stream << "  \"ImporterVersion\": " << asset.ImporterVersion << ",\n";
            stream << "  \"Dependencies\": []\n";
            stream << "}\n";

            return WriteTextFile(asset.MetaPath, stream.str());
        }
        void CollectAssetsInDirectory(const std::filesystem::path& assetsRoot,
            const std::string& virtualPrefix, const std::filesystem::path& repoRoot,
            std::vector<FAssetRecord>& outAssets) {
            std::error_code ec;
            if (!std::filesystem::exists(assetsRoot, ec)) {
                return;
            }

            std::filesystem::recursive_directory_iterator it(
                assetsRoot, std::filesystem::directory_options::skip_permission_denied, ec);
            const std::filesystem::recursive_directory_iterator end;
            for (; it != end; it.increment(ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }

                if (!it->is_regular_file(ec)) {
                    ec.clear();
                    continue;
                }

                const std::filesystem::path sourcePath = it->path();
                if (sourcePath.extension() == ".meta") {
                    continue;
                }

                const Asset::EAssetType type = GuessAssetType(sourcePath);
                if (type == Asset::EAssetType::Unknown) {
                    continue;
                }

                std::string           sourceRel = MakeRelativePath(repoRoot, sourcePath);

                std::filesystem::path relVirtual =
                    std::filesystem::relative(sourcePath, assetsRoot, ec);
                if (ec) {
                    relVirtual = sourcePath.filename();
                    ec.clear();
                }
                relVirtual.replace_extension();

                std::string virtualPath = virtualPrefix;
                if (!virtualPath.empty() && virtualPath.back() != '/') {
                    virtualPath.push_back('/');
                }
                virtualPath += NormalizePath(relVirtual);
                ToLowerAscii(virtualPath);

                FAssetRecord record{};
                record.SourcePath = sourcePath;
                record.MetaPath   = sourcePath;
                record.MetaPath += ".meta";
                record.SourcePathRel   = sourceRel;
                record.VirtualPath     = virtualPath;
                record.Type            = type;
                record.ImporterName    = GetImporterName(type);
                record.ImporterVersion = 1U;

                outAssets.push_back(record);
            }
        }
        auto CollectAssets(const std::filesystem::path& repoRoot, const std::string& demoFilter,
            std::vector<FAssetRecord>& outAssets) -> bool {
            outAssets.clear();

            CollectAssetsInDirectory(repoRoot / "Assets", "Engine", repoRoot, outAssets);

            std::error_code             ec;
            const std::filesystem::path demoRoot = repoRoot / "Demo";
            if (!std::filesystem::exists(demoRoot, ec)) {
                return true;
            }

            for (const auto& entry : std::filesystem::directory_iterator(demoRoot, ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }

                if (!entry.is_directory(ec)) {
                    ec.clear();
                    continue;
                }

                const std::string demoName = entry.path().filename().string();
                if (!demoFilter.empty() && demoName != demoFilter) {
                    continue;
                }

                const std::filesystem::path demoAssets = entry.path() / "Assets";
                CollectAssetsInDirectory(demoAssets, "Demo/" + demoName, repoRoot, outAssets);
            }

            return true;
        }

        auto EnsureMeta(FAssetRecord& asset, EMetaWriteMode mode) -> bool {
            Asset::EAssetType metaType = Asset::EAssetType::Unknown;
            std::string       metaVirtualPath;
            if (LoadMeta(asset.MetaPath, asset.Uuid, metaType, metaVirtualPath)) {
                if (metaType != Asset::EAssetType::Unknown) {
                    asset.Type = metaType;
                }
                if (!metaVirtualPath.empty()) {
                    asset.VirtualPath = metaVirtualPath;
                }
                asset.ImporterName = GetImporterName(asset.Type);

                if (mode == EMetaWriteMode::MissingOnly) {
                    return true;
                }
                return WriteMetaFile(asset);
            }

            asset.Uuid         = FUuid::New();
            asset.ImporterName = GetImporterName(asset.Type);
            return WriteMetaFile(asset);
        }
        auto ParseCommandLine(
            int argc, char** argv, FCommandLine& outCommand, std::string& outError) -> bool {
            if (argc < 2) {
                outError = "Missing command.";
                return false;
            }

            outCommand.Command = argv[1];
            for (int index = 2; index < argc; ++index) {
                std::string arg = argv[index];
                if (arg.rfind("--", 0) == 0) {
                    std::string key   = arg.substr(2);
                    std::string value = "true";
                    if (index + 1 < argc && std::string(argv[index + 1]).rfind("--", 0) != 0) {
                        value = argv[index + 1];
                        ++index;
                    }
                    outCommand.Options[key] = value;
                }
            }

            return true;
        }

        void PrintUsage() {
            std::cout << "AssetTool commands:\n";
            std::cout << "  import   --root <repoRoot> [--demo <DemoName>]\n";
            std::cout << "  cook     --root <repoRoot> --platform <Platform> [--demo <DemoName>]";
            std::cout << " [--build-root <BuildRoot>] [--cook-root <CookRoot>]\n";
            std::cout << "  bundle   --root <repoRoot> --platform <Platform> [--demo <DemoName>]";
            std::cout << " [--build-root <BuildRoot>] [--cook-root <CookRoot>]\n";
            std::cout << "  validate --registry <PathToAssetRegistry.json>\n";
            std::cout << "  clean    --root <repoRoot> [--build-root <BuildRoot>] --cache\n";
        }

        auto BuildPaths(const FCommandLine& command, const std::string& platform) -> FToolPaths {
            std::filesystem::path root;
            auto                  rootIt = command.Options.find("root");
            if (rootIt != command.Options.end()) {
                root = std::filesystem::path(rootIt->second);
            } else {
                root = std::filesystem::current_path();
            }

            std::filesystem::path buildRoot = root / "build";
            auto                  buildIt   = command.Options.find("build-root");
            if (buildIt != command.Options.end()) {
                buildRoot = std::filesystem::path(buildIt->second);
            }

            FToolPaths paths{};
            paths.Root      = std::filesystem::absolute(root);
            paths.BuildRoot = std::filesystem::absolute(buildRoot);
            auto cookedIt   = command.Options.find("cook-root");
            if (cookedIt != command.Options.end()) {
                paths.CookedRoot =
                    std::filesystem::absolute(std::filesystem::path(cookedIt->second));
            } else {
                paths.CookedRoot = paths.BuildRoot / "Cooked" / platform;
            }
            paths.CacheRoot     = paths.BuildRoot / "Cache";
            paths.CookCachePath = paths.CacheRoot / "CookKeys.json";
            return paths;
        }
        auto LoadCookCache(const std::filesystem::path&       cachePath,
            std::unordered_map<std::string, FCookCacheEntry>& outEntries) -> bool {
            outEntries.clear();
            if (!std::filesystem::exists(cachePath)) {
                return true;
            }

            std::string text;
            if (!ReadFileText(cachePath, text)) {
                return false;
            }

            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                return false;
            }

            const FJsonValue* entriesValue = FindObjectValueInsensitive(*root, "Entries");
            if (entriesValue == nullptr || entriesValue->Type != EJsonType::Array) {
                return true;
            }

            for (const auto* entry : entriesValue->Array) {
                if (entry == nullptr || entry->Type != EJsonType::Object) {
                    continue;
                }

                FNativeString uuidText;
                FNativeString cookKeyText;
                if (!GetStringValue(FindObjectValueInsensitive(*entry, "Uuid"), uuidText)
                    || !GetStringValue(
                        FindObjectValueInsensitive(*entry, "CookKey"), cookKeyText)) {
                    continue;
                }

                FCookCacheEntry cacheEntry{};
                cacheEntry.Uuid    = ToStdString(uuidText);
                cacheEntry.CookKey = ToStdString(cookKeyText);

                FNativeString sourceText;
                if (GetStringValue(FindObjectValueInsensitive(*entry, "SourcePath"), sourceText)) {
                    cacheEntry.SourcePath = ToStdString(sourceText);
                }

                FNativeString cookedText;
                if (GetStringValue(FindObjectValueInsensitive(*entry, "CookedPath"), cookedText)) {
                    cacheEntry.CookedPath = ToStdString(cookedText);
                }

                FNativeString lastCookedText;
                if (GetStringValue(
                        FindObjectValueInsensitive(*entry, "LastCooked"), lastCookedText)) {
                    cacheEntry.LastCooked = ToStdString(lastCookedText);
                }

                if (!cacheEntry.Uuid.empty()) {
                    outEntries[cacheEntry.Uuid] = cacheEntry;
                }
            }

            return true;
        }
        auto SaveCookCache(const std::filesystem::path&             cachePath,
            const std::unordered_map<std::string, FCookCacheEntry>& entries) -> bool {
            std::vector<std::string> keys;
            keys.reserve(entries.size());
            for (const auto& pair : entries) {
                keys.push_back(pair.first);
            }
            std::sort(keys.begin(), keys.end());

            std::ostringstream stream;
            stream << "{\n";
            stream << "  \"Version\": 1,\n";
            stream << "  \"Entries\": [\n";

            bool first = true;
            for (const auto& key : keys) {
                const auto& entry = entries.at(key);
                if (!first) {
                    stream << ",\n";
                }
                first = false;

                stream << "    {\n";
                stream << "      \"Uuid\": \"" << EscapeJson(entry.Uuid) << "\",\n";
                stream << "      \"CookKey\": \"" << EscapeJson(entry.CookKey) << "\",\n";
                stream << "      \"SourcePath\": \"" << EscapeJson(entry.SourcePath) << "\",\n";
                stream << "      \"CookedPath\": \"" << EscapeJson(entry.CookedPath) << "\"";
                if (!entry.LastCooked.empty()) {
                    stream << ",\n";
                    stream << "      \"LastCooked\": \"" << EscapeJson(entry.LastCooked) << "\"\n";
                } else {
                    stream << "\n";
                }
                stream << "    }";
            }

            stream << "\n  ]\n";
            stream << "}\n";

            std::error_code ec;
            std::filesystem::create_directories(cachePath.parent_path(), ec);

            return WriteTextFile(cachePath, stream.str());
        }
        auto WriteRegistry(const std::filesystem::path& registryPath,
            const std::vector<FRegistryEntry>&          assets) -> bool {
            std::ostringstream stream;
            stream << "{\n";
            stream << "  \"SchemaVersion\": 1,\n";
            stream << "  \"Assets\": [\n";

            auto WriteDependencies = [&](const std::vector<Asset::FAssetHandle>& deps) {
                stream << "      \"Dependencies\": [";
                if (!deps.empty()) {
                    stream << "\n";
                    for (size_t depIndex = 0; depIndex < deps.size(); ++depIndex) {
                        const auto& dep = deps[depIndex];
                        stream << "        ";
                        const std::string depUuid = ToStdString(dep.Uuid.ToNativeString());
                        if (dep.Type == Asset::EAssetType::Unknown) {
                            stream << "\"" << EscapeJson(depUuid) << "\"";
                        } else {
                            stream << "{ \"Uuid\": \"" << EscapeJson(depUuid) << "\", \"Type\": \""
                                   << AssetTypeToString(dep.Type) << "\" }";
                        }
                        if (depIndex + 1 < deps.size()) {
                            stream << ",";
                        }
                        stream << "\n";
                    }
                    stream << "      ";
                }
                stream << "],\n";
            };

            for (size_t index = 0; index < assets.size(); ++index) {
                const auto& entry = assets[index];
                stream << "    {\n";
                stream << "      \"Uuid\": \"" << EscapeJson(entry.Uuid) << "\",\n";
                stream << "      \"Type\": \"" << AssetTypeToString(entry.Type) << "\",\n";
                stream << "      \"VirtualPath\": \"" << EscapeJson(entry.VirtualPath) << "\",\n";
                stream << "      \"CookedPath\": \"" << EscapeJson(entry.CookedPath) << "\",\n";
                WriteDependencies(entry.Dependencies);
                stream << "      \"Desc\": {";

                switch (entry.Type) {
                    case Asset::EAssetType::Texture2D:
                        if (entry.HasTextureDesc) {
                            stream << "\"Width\": " << entry.TextureDesc.Width
                                   << ", \"Height\": " << entry.TextureDesc.Height
                                   << ", \"Format\": " << entry.TextureDesc.Format
                                   << ", \"MipCount\": " << entry.TextureDesc.MipCount
                                   << ", \"SRGB\": " << (entry.TextureDesc.SRGB ? "true" : "false");
                        } else {
                            stream
                                << "\"Width\": 0, \"Height\": 0, \"Format\": 0, \"MipCount\": 0, \"SRGB\": true";
                        }
                        break;
                    case Asset::EAssetType::Mesh:
                        if (entry.HasMeshDesc) {
                            stream << "\"VertexFormat\": " << entry.MeshDesc.VertexFormat
                                   << ", \"IndexFormat\": " << entry.MeshDesc.IndexFormat
                                   << ", \"SubMeshCount\": " << entry.MeshDesc.SubMeshCount;
                        } else {
                            stream
                                << "\"VertexFormat\": 0, \"IndexFormat\": 0, \"SubMeshCount\": 0";
                        }
                        break;
                    case Asset::EAssetType::MaterialTemplate:
                        if (entry.HasMaterialDesc) {
                            stream << "\"PassCount\": " << entry.MaterialDesc.PassCount
                                   << ", \"ShaderCount\": " << entry.MaterialDesc.ShaderCount
                                   << ", \"VariantCount\": " << entry.MaterialDesc.VariantCount;
                        } else {
                            stream << "\"PassCount\": 0, \"ShaderCount\": 0, \"VariantCount\": 0";
                        }
                        break;
                    case Asset::EAssetType::Shader:
                        if (entry.HasShaderDesc) {
                            stream << "\"Language\": " << entry.ShaderDesc.Language;
                        } else {
                            stream << "\"Language\": 0";
                        }
                        break;
                    case Asset::EAssetType::Model:
                        if (entry.HasModelDesc) {
                            stream
                                << "\"NodeCount\": " << entry.ModelDesc.NodeCount
                                << ", \"MeshRefCount\": " << entry.ModelDesc.MeshRefCount
                                << ", \"MaterialSlotCount\": " << entry.ModelDesc.MaterialSlotCount;
                        } else {
                            stream
                                << "\"NodeCount\": 0, \"MeshRefCount\": 0, \"MaterialSlotCount\": 0";
                        }
                        break;
                    case Asset::EAssetType::Audio:
                        if (entry.HasAudioDesc) {
                            stream << "\"Codec\": " << entry.AudioDesc.Codec
                                   << ", \"Channels\": " << entry.AudioDesc.Channels
                                   << ", \"SampleRate\": " << entry.AudioDesc.SampleRate
                                   << ", \"Duration\": " << entry.AudioDesc.DurationSeconds;
                        } else {
                            stream
                                << "\"Codec\": 0, \"Channels\": 0, \"SampleRate\": 0, \"Duration\": 0";
                        }
                        break;
                    case Asset::EAssetType::Script:
                        if (entry.HasScriptDesc) {
                            stream << "\"AssemblyPath\": \"" << EscapeJson(entry.ScriptAssemblyPath)
                                   << "\", \"TypeName\": \"" << EscapeJson(entry.ScriptTypeName)
                                   << "\"";
                        } else {
                            stream << "\"AssemblyPath\": \"\", \"TypeName\": \"\"";
                        }
                        break;
                    default:
                        break;
                }

                stream << "}\n";
                stream << "    }";
                if (index + 1 < assets.size()) {
                    stream << ",";
                }
                stream << "\n";
            }

            stream << "  ],\n";
            stream << "  \"Redirectors\": []\n";
            stream << "}\n";

            std::error_code ec;
            std::filesystem::create_directories(registryPath.parent_path(), ec);

            return WriteTextFile(registryPath, stream.str());
        }
        auto ImportAssets(const FCommandLine& command) -> int {
            const std::string demoFilter =
                command.Options.contains("demo") ? command.Options.at("demo") : std::string();

            FToolPaths                paths = BuildPaths(command, "Win64");

            std::vector<FAssetRecord> assets;
            if (!CollectAssets(paths.Root, demoFilter, assets)) {
                std::cerr << "Failed to collect assets.\n";
                return 1;
            }

            for (auto& asset : assets) {
                if (!EnsureMeta(asset, EMetaWriteMode::MissingOnly)) {
                    std::cerr << "Failed to ensure meta: " << asset.MetaPath.string() << "\n";
                    asset.Type = Asset::EAssetType::Unknown;
                }
            }

            std::unordered_map<std::string, const FAssetRecord*> assetsByVirtualPath;
            assetsByVirtualPath.reserve(assets.size());
            for (const auto& asset : assets) {
                if (asset.Type != Asset::EAssetType::Unknown && !asset.VirtualPath.empty()) {
                    assetsByVirtualPath[asset.VirtualPath] = &asset;
                }
            }

            size_t written = 0U;
            for (auto& asset : assets) {
                if (!EnsureMeta(asset, EMetaWriteMode::Always)) {
                    std::cerr << "Failed to write meta: " << asset.MetaPath.string() << "\n";
                    continue;
                }
                ++written;
            }

            std::cout << "Imported assets: " << written << "\n";
            return 0;
        }
        auto CookAssets(const FCommandLine& command) -> int {
            const std::string platform = command.Options.contains("platform")
                ? command.Options.at("platform")
                : std::string("Win64");
            const std::string demoFilter =
                command.Options.contains("demo") ? command.Options.at("demo") : std::string();

            FToolPaths                paths = BuildPaths(command, platform);

            std::vector<FAssetRecord> assets;
            if (!CollectAssets(paths.Root, demoFilter, assets)) {
                std::cerr << "Failed to collect assets.\n";
                return 1;
            }

            for (auto& asset : assets) {
                if (!EnsureMeta(asset, EMetaWriteMode::MissingOnly)) {
                    std::cerr << "Failed to ensure meta: " << asset.MetaPath.string() << "\n";
                    asset.Type = Asset::EAssetType::Unknown;
                }
            }

            std::unordered_map<std::string, const FAssetRecord*> assetsByVirtualPath;
            assetsByVirtualPath.reserve(assets.size());
            for (const auto& asset : assets) {
                if (asset.Type != Asset::EAssetType::Unknown && !asset.VirtualPath.empty()) {
                    assetsByVirtualPath[asset.VirtualPath] = &asset;
                }
            }

            std::unordered_map<std::string, FCookCacheEntry> cacheEntries;
            if (!LoadCookCache(paths.CookCachePath, cacheEntries)) {
                std::cerr << "Failed to read cook cache: " << paths.CookCachePath.string() << "\n";
                return 1;
            }

            std::vector<FRegistryEntry> registryAssets;
            registryAssets.reserve(assets.size());

            size_t cookedCount = 0U;
            for (auto& asset : assets) {
                if (asset.Type == Asset::EAssetType::Unknown) {
                    continue;
                }

                std::vector<u8> bytes;
                if (!ReadFileBytes(asset.SourcePath, bytes)) {
                    std::cerr << "Failed to read source: " << asset.SourcePath.string() << "\n";
                    continue;
                }

                std::vector<u8>       cookedBytes;
                std::vector<u8>       cookKeyExtras;
                Asset::FTexture2DDesc textureDesc{};
                Asset::FMeshDesc      meshDesc{};
                Asset::FMaterialDesc  materialDesc{};
                Asset::FShaderDesc    shaderDesc{};
                Asset::FModelDesc     modelDesc{};
                Asset::FAudioDesc     audioDesc{};
                const bool            isTexture = asset.Type == Asset::EAssetType::Texture2D;
                const bool            isMesh    = asset.Type == Asset::EAssetType::Mesh;
                const bool  isMaterial          = asset.Type == Asset::EAssetType::MaterialTemplate;
                const bool  isAudio             = asset.Type == Asset::EAssetType::Audio;
                const bool  isScript            = asset.Type == Asset::EAssetType::Script;
                const bool  isShader            = asset.Type == Asset::EAssetType::Shader;
                const bool  isModel             = asset.Type == Asset::EAssetType::Model;
                std::string scriptAssemblyPath;
                std::string scriptTypeName;
                bool        hasScriptDesc = false;
                std::vector<Asset::FAssetHandle> materialDeps;
                if (isScript) {
                    if (!ParseScriptDescriptor(bytes, scriptAssemblyPath, scriptTypeName)) {
                        std::cerr << "Failed to read script descriptor: "
                                  << asset.SourcePath.string() << "\n";
                        continue;
                    }
                    hasScriptDesc = true;
                }
                if (isTexture) {
                    constexpr bool kDefaultSrgb = true;
                    if (!CookTexture2D(bytes, kDefaultSrgb, cookedBytes, textureDesc)) {
                        std::cerr << "Failed to cook texture: " << asset.SourcePath.string()
                                  << "\n";
                        continue;
                    }
                } else if (isMesh) {
                    if (!CookMesh(asset.SourcePath, cookedBytes, meshDesc, cookKeyExtras)) {
                        std::cerr << "Failed to cook mesh: " << asset.SourcePath.string() << "\n";
                        continue;
                    }
                } else if (isModel) {
                    if (!CookModel(bytes, cookedBytes, modelDesc)) {
                        std::cerr << "Failed to cook model: " << asset.SourcePath.string() << "\n";
                        continue;
                    }
                    cookKeyExtras = cookedBytes;
                } else if (isMaterial) {
                    if (!CookMaterial(
                            bytes, assetsByVirtualPath, materialDeps, materialDesc, cookedBytes)) {
                        std::cerr << "Failed to cook material: " << asset.SourcePath.string()
                                  << "\n";
                        continue;
                    }
                    cookKeyExtras = cookedBytes;
                } else if (isShader) {
                    if (!CookShader(asset.SourcePath, bytes, paths.Root, cookedBytes, shaderDesc)) {
                        std::cerr << "Failed to cook shader: " << asset.SourcePath.string() << "\n";
                        continue;
                    }
                    cookKeyExtras = cookedBytes;
                } else if (isAudio) {
                    if (!CookAudio(asset.SourcePath, bytes, cookedBytes, audioDesc)) {
                        std::cerr << "Failed to cook audio: " << asset.SourcePath.string() << "\n";
                        continue;
                    }
                } else {
                    cookedBytes = bytes;
                }

                const std::string           uuid      = ToStdString(asset.Uuid.ToNativeString());
                const std::string           cookedRel = "Assets/" + uuid + ".bin";
                const std::filesystem::path cookedPath =
                    paths.CookedRoot / "Assets" / (uuid + ".bin");

                const std::string cookKey = (!cookKeyExtras.empty() || isMesh)
                    ? BuildCookKeyWithExtras(bytes, cookKeyExtras, asset, platform)
                    : BuildCookKey(bytes, asset, platform);

                bool              needsCook = true;
                auto              cacheIt   = cacheEntries.find(uuid);
                if (cacheIt != cacheEntries.end()) {
                    if (cacheIt->second.CookKey == cookKey && std::filesystem::exists(cookedPath)) {
                        needsCook = false;
                    }
                }

                if (needsCook) {
                    std::error_code ec;
                    std::filesystem::create_directories(cookedPath.parent_path(), ec);
                    if (!WriteBytesFile(cookedPath, cookedBytes)) {
                        std::cerr << "Failed to write cooked asset: " << cookedPath.string()
                                  << "\n";
                        continue;
                    }
                    ++cookedCount;
                }

                FCookCacheEntry cacheEntry{};
                cacheEntry.Uuid       = uuid;
                cacheEntry.CookKey    = cookKey;
                cacheEntry.SourcePath = asset.SourcePathRel;
                cacheEntry.CookedPath = cookedRel;
                cacheEntry.LastCooked = GetUtcTimestamp();
                cacheEntries[uuid]    = cacheEntry;

                FRegistryEntry registryEntry{};
                registryEntry.Uuid        = uuid;
                registryEntry.Type        = asset.Type;
                registryEntry.VirtualPath = asset.VirtualPath;
                registryEntry.CookedPath  = cookedRel;
                if (isTexture) {
                    registryEntry.TextureDesc    = textureDesc;
                    registryEntry.HasTextureDesc = true;
                } else if (isMesh) {
                    registryEntry.MeshDesc    = meshDesc;
                    registryEntry.HasMeshDesc = true;
                } else if (isModel) {
                    registryEntry.ModelDesc    = modelDesc;
                    registryEntry.HasModelDesc = true;
                } else if (isMaterial) {
                    registryEntry.MaterialDesc    = materialDesc;
                    registryEntry.HasMaterialDesc = true;
                    registryEntry.Dependencies    = Move(materialDeps);
                } else if (isShader) {
                    registryEntry.ShaderDesc    = shaderDesc;
                    registryEntry.HasShaderDesc = true;
                } else if (isAudio) {
                    registryEntry.AudioDesc    = audioDesc;
                    registryEntry.HasAudioDesc = true;
                } else if (isScript) {
                    registryEntry.ScriptAssemblyPath = scriptAssemblyPath;
                    registryEntry.ScriptTypeName     = scriptTypeName;
                    registryEntry.HasScriptDesc      = hasScriptDesc;
                }
                registryAssets.push_back(registryEntry);
            }

            const std::filesystem::path registryPath =
                paths.CookedRoot / "Registry" / "AssetRegistry.json";
            if (!WriteRegistry(registryPath, registryAssets)) {
                std::cerr << "Failed to write registry: " << registryPath.string() << "\n";
                return 1;
            }

            if (!SaveCookCache(paths.CookCachePath, cacheEntries)) {
                std::cerr << "Failed to write cook cache: " << paths.CookCachePath.string() << "\n";
                return 1;
            }

            std::cout << "Cooked assets: " << cookedCount << "\n";
            std::cout << "Registry: " << registryPath.string() << "\n";
            return 0;
        }

        auto BundleAssets(const FCommandLine& command) -> int {
            const std::string platform = command.Options.contains("platform")
                ? command.Options.at("platform")
                : std::string("Win64");
            const std::string demoFilter =
                command.Options.contains("demo") ? command.Options.at("demo") : std::string();

            FToolPaths                  paths = BuildPaths(command, platform);
            const std::filesystem::path registryPath =
                paths.CookedRoot / "Registry" / "AssetRegistry.json";

            std::vector<FBundledAsset> assets;
            if (!LoadRegistryAssets(registryPath, paths.CookedRoot, assets)) {
                std::cerr << "Failed to load registry assets: " << registryPath.string() << "\n";
                return 1;
            }

            std::sort(assets.begin(), assets.end(),
                [](const FBundledAsset& left, const FBundledAsset& right) {
                    return left.UuidText < right.UuidText;
                });

            const std::string bundleName = demoFilter.empty() ? std::string("All") : demoFilter;
            const std::filesystem::path bundlePath =
                paths.CookedRoot / "Bundles" / (bundleName + ".pak");

            std::error_code ec;
            std::filesystem::create_directories(bundlePath.parent_path(), ec);

            std::ofstream file(bundlePath, std::ios::binary);
            if (!file) {
                std::cerr << "Failed to open bundle for writing: " << bundlePath.string() << "\n";
                return 1;
            }

            Asset::FBundleHeader header{};
            header.Magic   = Asset::kBundleMagic;
            header.Version = Asset::kBundleVersion;

            file.write(reinterpret_cast<const char*>(&header),
                static_cast<std::streamsize>(sizeof(header)));

            u64                                   offset = sizeof(header);
            std::vector<Asset::FBundleIndexEntry> entries;
            entries.reserve(assets.size());

            for (const auto& asset : assets) {
                if (asset.Data.empty()) {
                    std::cerr << "Skipping empty asset: " << asset.CookedPath << "\n";
                    continue;
                }

                Asset::FBundleIndexEntry entry{};
                WriteBundleUuid(entry, asset.Uuid);
                entry.Type             = static_cast<u32>(asset.Type);
                entry.Compression      = static_cast<u32>(Asset::EBundleCompression::None);
                entry.Offset           = offset;
                entry.Size             = static_cast<u64>(asset.Data.size());
                entry.RawSize          = static_cast<u64>(asset.Data.size());
                entry.ChunkCount       = 0;
                entry.ChunkTableOffset = 0;

                file.write(reinterpret_cast<const char*>(asset.Data.data()),
                    static_cast<std::streamsize>(asset.Data.size()));
                offset += entry.Size;
                entries.push_back(entry);
            }

            const u64                 indexOffset = offset;
            Asset::FBundleIndexHeader indexHeader{};
            indexHeader.EntryCount      = static_cast<u32>(entries.size());
            indexHeader.StringTableSize = 0;

            file.write(reinterpret_cast<const char*>(&indexHeader),
                static_cast<std::streamsize>(sizeof(indexHeader)));

            if (!entries.empty()) {
                file.write(reinterpret_cast<const char*>(entries.data()),
                    static_cast<std::streamsize>(
                        entries.size() * sizeof(Asset::FBundleIndexEntry)));
            }

            const u64 indexSize = sizeof(indexHeader)
                + static_cast<u64>(entries.size()) * sizeof(Asset::FBundleIndexEntry);
            const u64 bundleSize = indexOffset + indexSize;

            header.IndexOffset = indexOffset;
            header.IndexSize   = indexSize;
            header.BundleSize  = bundleSize;

            file.seekp(0, std::ios::beg);
            file.write(reinterpret_cast<const char*>(&header),
                static_cast<std::streamsize>(sizeof(header)));

            if (!file.good()) {
                std::cerr << "Failed to write bundle: " << bundlePath.string() << "\n";
                return 1;
            }

            std::cout << "Bundle: " << bundlePath.string() << "\n";
            std::cout << "Bundle assets: " << entries.size() << "\n";
            return 0;
        }
        auto ValidateRegistry(const std::filesystem::path& registryPath) -> int {
            std::string text;
            if (!ReadFileText(registryPath, text)) {
                std::cerr << "Failed to read registry: " << registryPath.string() << "\n";
                return 1;
            }

            FNativeString native;
            native.Append(text.c_str(), text.size());
            const FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument           document;
            if (!document.Parse(view)) {
                std::cerr << "Registry JSON parse failed.\n";
                return 1;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                std::cerr << "Registry root is invalid.\n";
                return 1;
            }

            const FJsonValue* schemaValue  = FindObjectValueInsensitive(*root, "SchemaVersion");
            double            schemaNumber = 0.0;
            if (!GetNumberValue(schemaValue, schemaNumber)) {
                std::cerr << "SchemaVersion missing or invalid.\n";
                return 1;
            }

            const FJsonValue* assetsValue = FindObjectValueInsensitive(*root, "Assets");
            if (assetsValue == nullptr || assetsValue->Type != EJsonType::Array) {
                std::cerr << "Assets array missing.\n";
                return 1;
            }

            std::unordered_set<std::string> uuidSet;
            std::unordered_set<std::string> pathSet;
            bool                            ok = true;

            for (const auto* assetValue : assetsValue->Array) {
                if (assetValue == nullptr || assetValue->Type != EJsonType::Object) {
                    std::cerr << "Asset entry is not an object.\n";
                    ok = false;
                    continue;
                }

                FNativeString uuidText;
                FNativeString typeText;
                FNativeString pathText;

                if (!GetStringValue(FindObjectValueInsensitive(*assetValue, "Uuid"), uuidText)
                    || !GetStringValue(FindObjectValueInsensitive(*assetValue, "Type"), typeText)
                    || !GetStringValue(
                        FindObjectValueInsensitive(*assetValue, "VirtualPath"), pathText)) {
                    std::cerr << "Asset missing required fields.\n";
                    ok = false;
                    continue;
                }

                std::string uuid  = ToStdString(uuidText);
                std::string type  = ToStdString(typeText);
                std::string vpath = ToStdString(pathText);
                ToLowerAscii(uuid);
                ToLowerAscii(vpath);

                if (ParseAssetType(type) == Asset::EAssetType::Unknown) {
                    std::cerr << "Unknown asset type: " << type << "\n";
                    ok = false;
                }

                if (!uuidSet.insert(uuid).second) {
                    std::cerr << "Duplicate UUID: " << uuid << "\n";
                    ok = false;
                }

                if (!pathSet.insert(vpath).second) {
                    std::cerr << "Duplicate VirtualPath: " << vpath << "\n";
                    ok = false;
                }
            }

            if (!ok) {
                return 1;
            }

            std::cout << "Registry validated. SchemaVersion=" << schemaNumber << "\n";
            return 0;
        }
        auto CleanCache(const FCommandLine& command) -> int {
            FToolPaths paths   = BuildPaths(command, "Win64");
            auto       cacheIt = command.Options.find("cache");
            if (cacheIt == command.Options.end()) {
                std::cerr << "Specify --cache to remove cook cache.\n";
                return 1;
            }

            std::error_code ec;
            if (std::filesystem::exists(paths.CookCachePath, ec)) {
                std::filesystem::remove(paths.CookCachePath, ec);
                if (ec) {
                    std::cerr << "Failed to remove cache: " << paths.CookCachePath.string() << "\n";
                    return 1;
                }
            }

            std::cout << "Cook cache removed.\n";
            return 0;
        }

    } // namespace

    auto RunTool(int argc, char** argv) -> int {
        FCommandLine command;
        std::string  error;
        if (!ParseCommandLine(argc, argv, command, error)) {
            std::cerr << error << "\n";
            PrintUsage();
            return 1;
        }

        const std::string cmdLower = [&command]() {
            std::string out = command.Command;
            ToLowerAscii(out);
            return out;
        }();

        if (cmdLower == "import") {
            return ImportAssets(command);
        }
        if (cmdLower == "cook") {
            return CookAssets(command);
        }
        if (cmdLower == "bundle") {
            return BundleAssets(command);
        }
        if (cmdLower == "validate") {
            auto registryIt = command.Options.find("registry");
            if (registryIt == command.Options.end()) {
                std::cerr << "Missing --registry.\n";
                return 1;
            }
            return ValidateRegistry(std::filesystem::path(registryIt->second));
        }
        if (cmdLower == "clean") {
            return CleanCache(command);
        }

        PrintUsage();
        return 1;
    }

} // namespace AltinaEngine::Tools::AssetPipeline

auto main(int argc, char** argv) -> int {
    return AltinaEngine::Tools::AssetPipeline::RunTool(argc, argv);
}

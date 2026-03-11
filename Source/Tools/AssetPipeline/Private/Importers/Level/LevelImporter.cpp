#include "Importers/Level/LevelImporter.h"

#include "Asset/AssetBinary.h"
#include "Utility/Json.h"
#include "Utility/String/UuidParser.h"

#include <cstring>
#include <string>

namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetBoolValue;
        using Core::Utility::Json::GetNumberValue;
        using Core::Utility::Json::GetStringValue;

        auto ParseAssetHandleJson(const FJsonValue& value, Asset::FAssetHandle& outHandle) -> bool {
            if (value.Type != EJsonType::Object) {
                return false;
            }

            bool valid = false;
            (void)GetBoolValue(FindObjectValueInsensitive(value, "valid"), valid);
            if (!valid) {
                outHandle = {};
                return true;
            }

            double typeNumber = 0.0;
            if (!GetNumberValue(FindObjectValueInsensitive(value, "type"), typeNumber)) {
                return false;
            }

            Core::Container::FNativeString uuidText;
            if (!GetStringValue(FindObjectValueInsensitive(value, "uuid"), uuidText)) {
                return false;
            }

            FUuid uuid{};
            if (!Core::Utility::String::ParseUuid(uuidText.ToView(), uuid)) {
                return false;
            }

            outHandle.mType = static_cast<Asset::EAssetType>(static_cast<u8>(typeNumber));
            outHandle.mUuid = uuid;
            return outHandle.IsValid();
        }

        void AddDependency(
            const Asset::FAssetHandle& handle, std::vector<Asset::FAssetHandle>& outDependencies) {
            if (!handle.IsValid()) {
                return;
            }
            for (const auto& existing : outDependencies) {
                if (existing == handle) {
                    return;
                }
            }
            outDependencies.push_back(handle);
        }

        void ScanDependencies(
            const FJsonValue& value, std::vector<Asset::FAssetHandle>& outDependencies) {
            if (value.Type == EJsonType::Object) {
                Asset::FAssetHandle handle{};
                if (ParseAssetHandleJson(value, handle)) {
                    AddDependency(handle, outDependencies);
                }

                for (const auto& pair : value.Object) {
                    if (pair.Value != nullptr) {
                        ScanDependencies(*pair.Value, outDependencies);
                    }
                }
                return;
            }

            if (value.Type == EJsonType::Array) {
                for (const auto* item : value.Array) {
                    if (item != nullptr) {
                        ScanDependencies(*item, outDependencies);
                    }
                }
            }
        }
    } // namespace

    auto CookLevel(const std::filesystem::path& sourcePath, const std::vector<u8>& sourceBytes,
        std::vector<u8>& outCooked, std::vector<Asset::FAssetHandle>& outDependencies,
        Asset::FLevelDesc& outDesc) -> bool {
        (void)sourcePath;

        outCooked.clear();
        outDependencies.clear();
        outDesc = {};

        const std::string              text(sourceBytes.begin(), sourceBytes.end());
        Core::Container::FNativeString nativeText{};
        nativeText.Append(text.c_str(), text.size());

        FJsonDocument document{};
        if (!document.Parse(nativeText.ToView())) {
            return false;
        }

        const auto* root = document.GetRoot();
        if (root == nullptr || root->Type != EJsonType::Object) {
            return false;
        }

        double versionNumber = 0.0;
        if (!GetNumberValue(FindObjectValueInsensitive(*root, "version"), versionNumber)) {
            return false;
        }
        const auto* objectsValue = FindObjectValueInsensitive(*root, "objects");
        if (objectsValue == nullptr || objectsValue->Type != EJsonType::Array) {
            return false;
        }
        (void)versionNumber;

        ScanDependencies(*root, outDependencies);

        Asset::FAssetBlobHeader header{};
        header.mType     = static_cast<u8>(Asset::EAssetType::Level);
        header.mDescSize = static_cast<u32>(sizeof(Asset::FLevelBlobDesc));
        header.mDataSize = static_cast<u32>(sourceBytes.size());

        Asset::FLevelBlobDesc blobDesc{};
        blobDesc.mEncoding = Asset::kLevelEncodingWorldJson;

        const usize totalSize =
            sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FLevelBlobDesc) + sourceBytes.size();
        outCooked.resize(totalSize);

        u8* writePtr = outCooked.data();
        std::memcpy(writePtr, &header, sizeof(header));
        writePtr += sizeof(header);
        std::memcpy(writePtr, &blobDesc, sizeof(blobDesc));
        writePtr += sizeof(blobDesc);
        if (!sourceBytes.empty()) {
            std::memcpy(writePtr, sourceBytes.data(), sourceBytes.size());
        }

        outDesc.Encoding = blobDesc.mEncoding;
        outDesc.ByteSize = static_cast<u32>(sourceBytes.size());
        return true;
    }
} // namespace AltinaEngine::Tools::AssetPipeline

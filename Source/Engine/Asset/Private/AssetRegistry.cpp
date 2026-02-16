#include "Asset/AssetRegistry.h"

#include "Algorithm/CStringUtils.h"
#include "Platform/PlatformFileSystem.h"
#include "Types/Traits.h"
#include "Utility/Json.h"

#include <cstring>
#include <limits>
#include <string>

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
#endif

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    namespace {
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetBoolValue;
        using Core::Utility::Json::GetNumberValue;
        using Core::Utility::Json::GetStringValue;

        auto EqualPathI(FStringView left, FStringView right) -> bool {
            if (left.Length() != right.Length()) {
                return false;
            }

            for (usize i = 0; i < left.Length(); ++i) {
                if (Core::Algorithm::ToLowerChar(left[i])
                    != Core::Algorithm::ToLowerChar(right[i])) {
                    return false;
                }
            }

            return true;
        }

        auto EqualLiteralI(FNativeStringView text, const char* literal) -> bool {
            if (literal == nullptr) {
                return false;
            }
            const usize length = static_cast<usize>(std::strlen(literal));
            if (text.Length() != length) {
                return false;
            }
            for (usize i = 0; i < length; ++i) {
                if (Core::Algorithm::ToLowerChar(text[i])
                    != Core::Algorithm::ToLowerChar(literal[i])) {
                    return false;
                }
            }
            return true;
        }

        auto ParseUuid(const FNativeString& text, FUuid& out) -> bool {
            if (text.IsEmptyString()) {
                return false;
            }
            FNativeStringView view(text.GetData(), text.Length());
            return FUuid::TryParse(view, out);
        }

        auto ParseAssetType(FNativeStringView text) -> EAssetType {
            if (EqualLiteralI(text, "texture2d")) {
                return EAssetType::Texture2D;
            }
            if (EqualLiteralI(text, "mesh")) {
                return EAssetType::Mesh;
            }
            if (EqualLiteralI(text, "material")) {
                return EAssetType::Material;
            }
            if (EqualLiteralI(text, "audio")) {
                return EAssetType::Audio;
            }
            if (EqualLiteralI(text, "redirector")) {
                return EAssetType::Redirector;
            }
            return EAssetType::Unknown;
        }

        auto FromUtf8(const FNativeString& value) -> FString {
            FString out;
            if (value.IsEmptyString()) {
                return out;
            }
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            std::string utf8(value.GetData(), value.GetData() + value.Length());
    #if AE_PLATFORM_WIN
            int wideCount = MultiByteToWideChar(
                CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            if (wideCount <= 0) {
                return out;
            }
            std::wstring wide(static_cast<size_t>(wideCount), L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), wideCount);
            out.Append(wide.c_str(), wide.size());
    #else
            out.Append(utf8.c_str(), utf8.size());
    #endif
#else
            out.Append(value.GetData(), value.Length());
#endif
            return out;
        }

        void ReadU32Field(const FJsonValue& object, const char* key, u32& out) {
            const FJsonValue* value  = FindObjectValueInsensitive(object, key);
            double            number = 0.0;
            if (!GetNumberValue(value, number)) {
                return;
            }
            if (number < 0.0) {
                return;
            }
            if (number > static_cast<double>(std::numeric_limits<u32>::max())) {
                return;
            }
            out = static_cast<u32>(number);
        }

        void ReadFloatField(const FJsonValue& object, const char* key, f32& out) {
            const FJsonValue* value  = FindObjectValueInsensitive(object, key);
            double            number = 0.0;
            if (!GetNumberValue(value, number)) {
                return;
            }
            out = static_cast<f32>(number);
        }

        void ReadBoolField(const FJsonValue& object, const char* key, bool& out) {
            const FJsonValue* value = FindObjectValueInsensitive(object, key);
            bool              flag  = false;
            if (!GetBoolValue(value, flag)) {
                return;
            }
            out = flag;
        }

        void ReadTextureBindings(const FJsonValue& object, TVector<FAssetHandle>& outBindings) {
            const FJsonValue* bindings = FindObjectValueInsensitive(object, "TextureBindings");
            if (bindings == nullptr || bindings->Type != EJsonType::Array) {
                return;
            }

            for (const auto* item : bindings->Array) {
                if (item == nullptr) {
                    continue;
                }

                if (item->Type == EJsonType::String) {
                    FUuid uuid;
                    if (ParseUuid(item->String, uuid)) {
                        outBindings.PushBack({ uuid, EAssetType::Texture2D });
                    }
                    continue;
                }

                if (item->Type == EJsonType::Object) {
                    FNativeString uuidText;
                    if (!GetStringValue(FindObjectValueInsensitive(*item, "Uuid"), uuidText)) {
                        continue;
                    }
                    FUuid uuid;
                    if (!ParseUuid(uuidText, uuid)) {
                        continue;
                    }
                    EAssetType    type = EAssetType::Texture2D;
                    FNativeString typeText;
                    if (GetStringValue(FindObjectValueInsensitive(*item, "Type"), typeText)) {
                        type = ParseAssetType(
                            FNativeStringView(typeText.GetData(), typeText.Length()));
                    }
                    outBindings.PushBack({ uuid, type });
                }
            }
        }

        void ReadDescFields(const FJsonValue& descObject, FAssetDesc& desc) {
            switch (desc.Handle.Type) {
                case EAssetType::Texture2D:
                    ReadU32Field(descObject, "Width", desc.Texture.Width);
                    ReadU32Field(descObject, "Height", desc.Texture.Height);
                    ReadU32Field(descObject, "MipCount", desc.Texture.MipCount);
                    ReadU32Field(descObject, "Format", desc.Texture.Format);
                    ReadBoolField(descObject, "SRGB", desc.Texture.SRGB);
                    break;
                case EAssetType::Mesh:
                    ReadU32Field(descObject, "VertexFormat", desc.Mesh.VertexFormat);
                    ReadU32Field(descObject, "IndexFormat", desc.Mesh.IndexFormat);
                    ReadU32Field(descObject, "SubMeshCount", desc.Mesh.SubMeshCount);
                    break;
                case EAssetType::Material:
                    ReadU32Field(descObject, "ShadingModel", desc.Material.ShadingModel);
                    ReadU32Field(descObject, "BlendMode", desc.Material.BlendMode);
                    ReadU32Field(descObject, "Flags", desc.Material.Flags);
                    ReadFloatField(descObject, "AlphaCutoff", desc.Material.AlphaCutoff);
                    ReadTextureBindings(descObject, desc.Material.TextureBindings);
                    break;
                case EAssetType::Audio:
                    ReadU32Field(descObject, "Codec", desc.Audio.Codec);
                    ReadU32Field(descObject, "Channels", desc.Audio.Channels);
                    ReadU32Field(descObject, "SampleRate", desc.Audio.SampleRate);
                    ReadFloatField(descObject, "Duration", desc.Audio.DurationSeconds);
                    break;
                default:
                    break;
            }
        }

        auto ValidateSchema(const FJsonValue& root, FNativeString& outError) -> bool {
            if (root.Type != EJsonType::Object) {
                outError = "Root must be a JSON object.";
                return false;
            }

            const FJsonValue* schemaVersion = FindObjectValueInsensitive(root, "SchemaVersion");
            double            versionNumber = 0.0;
            if (!GetNumberValue(schemaVersion, versionNumber)) {
                outError = "SchemaVersion is missing or not a number.";
                return false;
            }

            const FJsonValue* assets = FindObjectValueInsensitive(root, "Assets");
            if (assets == nullptr || assets->Type != EJsonType::Array) {
                outError = "Assets array is missing.";
                return false;
            }

            (void)versionNumber;
            return true;
        }

        auto ParseDependencies(const FJsonValue& object, TVector<FAssetHandle>& outDependencies)
            -> bool {
            const FJsonValue* deps = FindObjectValueInsensitive(object, "Dependencies");
            if (deps == nullptr) {
                return true;
            }
            if (deps->Type != EJsonType::Array) {
                return false;
            }

            for (const auto* item : deps->Array) {
                if (item == nullptr) {
                    continue;
                }

                if (item->Type == EJsonType::String) {
                    FUuid uuid;
                    if (ParseUuid(item->String, uuid)) {
                        outDependencies.PushBack({ uuid, EAssetType::Unknown });
                    }
                    continue;
                }

                if (item->Type == EJsonType::Object) {
                    FNativeString uuidText;
                    if (!GetStringValue(FindObjectValueInsensitive(*item, "Uuid"), uuidText)) {
                        continue;
                    }
                    FUuid uuid;
                    if (!ParseUuid(uuidText, uuid)) {
                        continue;
                    }

                    EAssetType    type = EAssetType::Unknown;
                    FNativeString typeText;
                    if (GetStringValue(FindObjectValueInsensitive(*item, "Type"), typeText)) {
                        type = ParseAssetType(
                            FNativeStringView(typeText.GetData(), typeText.Length()));
                    }
                    outDependencies.PushBack({ uuid, type });
                }
            }

            return true;
        }

        auto ParseAssets(const FJsonValue& root, TVector<FAssetDesc>& outAssets,
            TVector<FAssetRedirector>& outRedirectors, FNativeString& outError) -> bool {
            const FJsonValue* assetsValue = FindObjectValueInsensitive(root, "Assets");
            if (assetsValue == nullptr || assetsValue->Type != EJsonType::Array) {
                outError = "Assets array missing.";
                return false;
            }

            for (const auto* assetValue : assetsValue->Array) {
                if (assetValue == nullptr || assetValue->Type != EJsonType::Object) {
                    outError = "Asset entry must be an object.";
                    return false;
                }

                FNativeString uuidText;
                FNativeString typeText;
                FNativeString virtualPathText;
                FNativeString cookedPathText;

                if (!GetStringValue(FindObjectValueInsensitive(*assetValue, "Uuid"), uuidText)) {
                    outError = "Asset missing Uuid.";
                    return false;
                }
                if (!GetStringValue(FindObjectValueInsensitive(*assetValue, "Type"), typeText)) {
                    outError = "Asset missing Type.";
                    return false;
                }
                if (!GetStringValue(
                        FindObjectValueInsensitive(*assetValue, "VirtualPath"), virtualPathText)) {
                    outError = "Asset missing VirtualPath.";
                    return false;
                }

                FUuid uuid;
                if (!ParseUuid(uuidText, uuid)) {
                    outError = "Asset Uuid invalid.";
                    return false;
                }

                const EAssetType type =
                    ParseAssetType(FNativeStringView(typeText.GetData(), typeText.Length()));
                if (type == EAssetType::Unknown) {
                    outError = "Asset Type invalid.";
                    return false;
                }

                FAssetDesc desc;
                desc.Handle.Uuid = uuid;
                desc.Handle.Type = type;
                desc.VirtualPath = FromUtf8(virtualPathText);

                if (GetStringValue(
                        FindObjectValueInsensitive(*assetValue, "CookedPath"), cookedPathText)) {
                    desc.CookedPath = FromUtf8(cookedPathText);
                }

                if (!ParseDependencies(*assetValue, desc.Dependencies)) {
                    outError = "Asset Dependencies invalid.";
                    return false;
                }

                const FJsonValue* descValue = FindObjectValueInsensitive(*assetValue, "Desc");
                if (descValue != nullptr && descValue->Type == EJsonType::Object) {
                    ReadDescFields(*descValue, desc);
                }

                desc.VirtualPath.ToLower();
                outAssets.PushBack(Move(desc));
            }

            const FJsonValue* redirectorsValue = FindObjectValueInsensitive(root, "Redirectors");
            if (redirectorsValue != nullptr) {
                if (redirectorsValue->Type != EJsonType::Array) {
                    outError = "Redirectors must be an array.";
                    return false;
                }

                for (const auto* entry : redirectorsValue->Array) {
                    if (entry == nullptr || entry->Type != EJsonType::Object) {
                        outError = "Redirector entry must be an object.";
                        return false;
                    }

                    FNativeString oldUuidText;
                    FNativeString newUuidText;
                    FNativeString oldPathText;

                    if (!GetStringValue(FindObjectValueInsensitive(*entry, "OldUuid"), oldUuidText)
                        || !GetStringValue(
                            FindObjectValueInsensitive(*entry, "NewUuid"), newUuidText)
                        || !GetStringValue(
                            FindObjectValueInsensitive(*entry, "OldVirtualPath"), oldPathText)) {
                        outError = "Redirector missing required fields.";
                        return false;
                    }

                    FUuid oldUuid;
                    FUuid newUuid;
                    if (!ParseUuid(oldUuidText, oldUuid) || !ParseUuid(newUuidText, newUuid)) {
                        outError = "Redirector UUID invalid.";
                        return false;
                    }

                    FAssetRedirector redirector;
                    redirector.OldUuid        = oldUuid;
                    redirector.NewUuid        = newUuid;
                    redirector.OldVirtualPath = FromUtf8(oldPathText);
                    redirector.OldVirtualPath.ToLower();
                    outRedirectors.PushBack(Move(redirector));
                }
            }

            return true;
        }
    } // namespace

    void FAssetRegistry::Clear() {
        mAssets.Clear();
        mRedirectors.Clear();
        mLastError.Clear();
    }

    auto FAssetRegistry::LoadFromJsonFile(const FString& path) -> bool {
        mLastError.Clear();

        FNativeString text;
        if (!Core::Platform::ReadFileTextUtf8(path, text)) {
            mLastError = "Failed to read registry JSON.";
            return false;
        }

        return LoadFromJsonText(FNativeStringView(text.GetData(), text.Length()));
    }

    auto FAssetRegistry::LoadFromJsonText(FNativeStringView text) -> bool {
        mLastError.Clear();

        FJsonDocument document;
        if (!document.Parse(text)) {
            mLastError = document.GetError();
            return false;
        }

        const FJsonValue* root = document.GetRoot();
        if (root == nullptr) {
            mLastError = "Registry JSON missing root.";
            return false;
        }

        if (!ValidateSchema(*root, mLastError)) {
            return false;
        }

        TVector<FAssetDesc>       assets;
        TVector<FAssetRedirector> redirectors;
        if (!ParseAssets(*root, assets, redirectors, mLastError)) {
            return false;
        }

        mAssets      = Move(assets);
        mRedirectors = Move(redirectors);
        return true;
    }

    auto FAssetRegistry::GetLastError() const noexcept -> FNativeStringView {
        return { mLastError.GetData(), mLastError.Length() };
    }

    void FAssetRegistry::AddAsset(FAssetDesc desc) {
        desc.VirtualPath.ToLower();
        mAssets.PushBack(Move(desc));
    }

    void FAssetRegistry::AddRedirector(FAssetRedirector redirector) {
        redirector.OldVirtualPath.ToLower();
        mRedirectors.PushBack(Move(redirector));
    }

    auto FAssetRegistry::FindByPath(FStringView path) const noexcept -> FAssetHandle {
        for (const auto& asset : mAssets) {
            if (EqualPathI(path, asset.VirtualPath.ToView())) {
                return asset.Handle;
            }
        }

        for (const auto& redirector : mRedirectors) {
            if (EqualPathI(path, redirector.OldVirtualPath.ToView())) {
                return FindByUuid(redirector.NewUuid);
            }
        }

        return {};
    }

    auto FAssetRegistry::FindByUuid(const FUuid& uuid) const noexcept -> FAssetHandle {
        if (uuid.IsNil()) {
            return {};
        }

        for (const auto& asset : mAssets) {
            if (asset.Handle.Uuid == uuid) {
                return asset.Handle;
            }
        }

        return {};
    }

    auto FAssetRegistry::GetDesc(const FAssetHandle& handle) const noexcept -> const FAssetDesc* {
        if (!handle.IsValid()) {
            return nullptr;
        }

        for (const auto& asset : mAssets) {
            if (asset.Handle.Uuid == handle.Uuid) {
                if (handle.Type == EAssetType::Unknown || asset.Handle.Type == handle.Type) {
                    return &asset;
                }
            }
        }

        return nullptr;
    }

    auto FAssetRegistry::GetDependencies(const FAssetHandle& handle) const noexcept
        -> const TVector<FAssetHandle>* {
        const FAssetDesc* desc = GetDesc(handle);
        if (desc == nullptr) {
            return nullptr;
        }

        return &desc->Dependencies;
    }

    auto FAssetRegistry::ResolveRedirector(const FAssetHandle& handle) const noexcept
        -> FAssetHandle {
        if (!handle.IsValid()) {
            return handle;
        }

        for (const auto& redirector : mRedirectors) {
            if (redirector.OldUuid == handle.Uuid) {
                FAssetHandle resolved = FindByUuid(redirector.NewUuid);
                if (resolved.IsValid()) {
                    return resolved;
                }

                return { redirector.NewUuid, handle.Type };
            }
        }

        return handle;
    }

} // namespace AltinaEngine::Asset

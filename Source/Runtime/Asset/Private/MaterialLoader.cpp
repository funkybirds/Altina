#include "Asset/MaterialLoader.h"

#include "Asset/MaterialAsset.h"

#include "Algorithm/CStringUtils.h"
#include "Container/StringView.h"
#include "Types/Traits.h"
#include "Utility/Json.h"
#include "Utility/String/CodeConvert.h"
#include "Utility/String/StringViewUtility.h"
#include "Utility/String/UuidParser.h"
#include "Utility/Uuid.h"

using AltinaEngine::Forward;
using AltinaEngine::Move;
using AltinaEngine::Core::Container::DestroyPolymorphic;
namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::FStringView;
    using Container::TVector;
    namespace {
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetBoolValue;
        using Core::Utility::Json::GetNumberValue;
        using Core::Utility::Json::GetStringValue;

        auto ReadExact(IAssetStream& stream, void* outBuffer, usize size) -> bool {
            if (outBuffer == nullptr || size == 0U) {
                return false;
            }

            auto*       out       = static_cast<u8*>(outBuffer);
            usize       totalRead = 0;
            const usize target    = size;
            while (totalRead < target) {
                const usize read = stream.Read(out + totalRead, target - totalRead);
                if (read == 0U) {
                    return false;
                }
                totalRead += read;
            }
            return true;
        }

        auto ReadAllBytes(IAssetStream& stream, TVector<u8>& outBytes) -> bool {
            const usize size = stream.Size();
            if (size == 0U) {
                return false;
            }

            outBytes.Resize(size);
            stream.Seek(0U);
            return ReadExact(stream, outBytes.Data(), size);
        }

        template <typename TDerived, typename... Args>
        auto MakeSharedAsset(Args&&... args) -> TShared<IAsset> {
            return Container::MakeSharedAs<IAsset, TDerived>(Forward<Args>(args)...);
        }

        auto ParseAssetTypeText(const FJsonValue* value, EAssetType& outType) -> bool {
            Container::FNativeString typeText;
            if (!GetStringValue(value, typeText)) {
                return false;
            }

            Container::FNativeStringView view(typeText.GetData(), typeText.Length());
            if (Core::Utility::String::EqualLiteralI(view, "texture2d")
                || Core::Utility::String::EqualLiteralI(view, "texture")
                || Core::Utility::String::EqualLiteralI(view, "tex2d")) {
                outType = EAssetType::Texture2D;
                return true;
            }
            if (Core::Utility::String::EqualLiteralI(view, "shader")) {
                outType = EAssetType::Shader;
                return true;
            }
            if (Core::Utility::String::EqualLiteralI(view, "materialtemplate")
                || Core::Utility::String::EqualLiteralI(view, "material")) {
                outType = EAssetType::MaterialTemplate;
                return true;
            }
            if (Core::Utility::String::EqualLiteralI(view, "materialinstance")) {
                outType = EAssetType::MaterialInstance;
                return true;
            }
            return false;
        }

        auto ParseAssetHandle(const FJsonValue& value, FAssetHandle& outHandle) -> bool {
            if (value.Type != EJsonType::Object) {
                return false;
            }

            FNativeString uuidText;
            if (!GetStringValue(FindObjectValueInsensitive(value, "Uuid"), uuidText)) {
                return false;
            }

            FUuid uuid;
            if (!Core::Utility::String::ParseUuid(uuidText, uuid)) {
                return false;
            }

            EAssetType        type      = EAssetType::Unknown;
            const FJsonValue* typeValue = FindObjectValueInsensitive(value, "Type");
            if (typeValue != nullptr) {
                (void)ParseAssetTypeText(typeValue, type);
            }
            if (type == EAssetType::Unknown) {
                return false;
            }

            outHandle.mUuid = uuid;
            outHandle.mType = type;
            return outHandle.IsValid();
        }

        auto ParseShaderSource(const FJsonValue& value, FMaterialShaderSource& out) -> bool {
            if (value.Type != EJsonType::Object) {
                return false;
            }

            FNativeString uuidText;
            if (!GetStringValue(FindObjectValueInsensitive(value, "Uuid"), uuidText)) {
                return false;
            }

            FUuid uuid;
            if (!Core::Utility::String::ParseUuid(uuidText, uuid)) {
                return false;
            }

            EAssetType        type      = EAssetType::Shader;
            const FJsonValue* typeValue = FindObjectValueInsensitive(value, "Type");
            if (typeValue != nullptr) {
                (void)ParseAssetTypeText(typeValue, type);
            }

            FNativeString entryText;
            if (!GetStringValue(FindObjectValueInsensitive(value, "Entry"), entryText)) {
                return false;
            }

            out.mAsset.mUuid = uuid;
            out.mAsset.mType = type;
            out.mEntry       = Core::Utility::String::FromUtf8(entryText);
            return out.mAsset.IsValid() && !out.mEntry.IsEmptyString();
        }

        constexpr u32 kFnvOffset32 = 2166136261u;
        constexpr u32 kFnvPrime32  = 16777619u;

        auto          HashMaterialParamName(FStringView name) noexcept -> FMaterialParamId {
            if (name.IsEmpty()) {
                return 0U;
            }

            u32         hash   = kFnvOffset32;
            const auto* data   = name.Data();
            const auto  length = name.Length();
            for (usize i = 0U; i < length; ++i) {
                hash ^= static_cast<u32>(static_cast<FStringView::TUnsigned>(data[i]));
                hash *= kFnvPrime32;
            }
            return hash;
        }

        auto ReadFloatArray(const FJsonValue& value, TVector<f32>& out) -> bool {
            if (value.Type != EJsonType::Array) {
                return false;
            }

            out.Clear();
            out.Reserve(value.Array.Size());
            for (const auto* entry : value.Array) {
                if (entry == nullptr || entry->Type != EJsonType::Number) {
                    return false;
                }
                out.PushBack(static_cast<f32>(entry->Number));
            }
            return true;
        }

        auto ParseOverrideEntry(const Container::FNativeString& nameText, const FJsonValue& value,
            FMeshMaterialParameterBlock& outOverrides) -> bool {
            if (value.Type != EJsonType::Object) {
                return false;
            }

            FNativeString typeText;
            if (!GetStringValue(FindObjectValueInsensitive(value, "Type"), typeText)) {
                return false;
            }

            const FJsonValue* valueNode = FindObjectValueInsensitive(value, "Value");
            if (valueNode == nullptr) {
                return false;
            }

            const Container::FString paramName = Core::Utility::String::FromUtf8(nameText);
            const auto               nameHash  = HashMaterialParamName(paramName.ToView());
            if (nameHash == 0U) {
                return false;
            }

            const Container::FNativeStringView typeView(typeText.GetData(), typeText.Length());
            if (Core::Utility::String::EqualLiteralI(typeView, "float")
                || Core::Utility::String::EqualLiteralI(typeView, "scalar")) {
                double number = 0.0;
                if (!GetNumberValue(valueNode, number)) {
                    return false;
                }
                outOverrides.SetScalar(nameHash, static_cast<f32>(number));
                return true;
            }

            if (Core::Utility::String::EqualLiteralI(typeView, "float2")
                || Core::Utility::String::EqualLiteralI(typeView, "float3")
                || Core::Utility::String::EqualLiteralI(typeView, "float4")
                || Core::Utility::String::EqualLiteralI(typeView, "vector")) {
                TVector<f32> values;
                if (!ReadFloatArray(*valueNode, values)) {
                    return false;
                }

                Core::Math::FVector4f vec(0.0f);
                if (values.Size() > 0U) {
                    vec.X() = values[0];
                }
                if (values.Size() > 1U) {
                    vec.Y() = values[1];
                }
                if (values.Size() > 2U) {
                    vec.Z() = values[2];
                }
                if (values.Size() > 3U) {
                    vec.W() = values[3];
                }
                outOverrides.SetVector(nameHash, vec);
                return true;
            }

            if (Core::Utility::String::EqualLiteralI(typeView, "float4x4")
                || Core::Utility::String::EqualLiteralI(typeView, "matrix")) {
                TVector<f32> values;
                if (!ReadFloatArray(*valueNode, values)) {
                    return false;
                }
                if (values.Size() != 16U) {
                    return false;
                }

                Core::Math::FMatrix4x4f matrix(0.0f);
                for (u32 row = 0U; row < 4U; ++row) {
                    for (u32 col = 0U; col < 4U; ++col) {
                        matrix.mElements[row][col] = values[row * 4U + col];
                    }
                }
                outOverrides.SetMatrix(nameHash, matrix);
                return true;
            }

            if (Core::Utility::String::EqualLiteralI(typeView, "texture2d")
                || Core::Utility::String::EqualLiteralI(typeView, "texture")) {
                FAssetHandle handle{};
                if (valueNode->Type == EJsonType::Object) {
                    if (!ParseAssetHandle(*valueNode, handle)) {
                        return false;
                    }
                } else if (valueNode->Type == EJsonType::String) {
                    FUuid uuid;
                    if (!Core::Utility::String::ParseUuid(valueNode->String, uuid)) {
                        return false;
                    }
                    handle.mUuid = uuid;
                    handle.mType = EAssetType::Texture2D;
                } else {
                    return false;
                }

                return outOverrides.SetTexture(
                    nameHash, EMeshMaterialTextureType::Texture2D, handle, 0U);
            }

            return false;
        }

        void ParseOverridesObject(
            const FJsonValue& overridesValue, FMeshMaterialParameterBlock& outOverrides) {
            if (overridesValue.Type != EJsonType::Object) {
                return;
            }

            for (const auto& pair : overridesValue.Object) {
                if (pair.Value == nullptr) {
                    continue;
                }
                ParseOverrideEntry(pair.Key, *pair.Value, outOverrides);
            }
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

        auto TryParseRasterFillMode(
            Container::FNativeStringView value, EMaterialRasterFillMode& out) noexcept -> bool {
            if (Core::Utility::String::EqualLiteralI(value, "solid")) {
                out = EMaterialRasterFillMode::Solid;
                return true;
            }
            if (Core::Utility::String::EqualLiteralI(value, "wireframe")) {
                out = EMaterialRasterFillMode::Wireframe;
                return true;
            }
            return false;
        }

        auto TryParseRasterCullMode(
            Container::FNativeStringView value, EMaterialRasterCullMode& out) noexcept -> bool {
            if (Core::Utility::String::EqualLiteralI(value, "none")) {
                out = EMaterialRasterCullMode::None;
                return true;
            }
            if (Core::Utility::String::EqualLiteralI(value, "front")) {
                out = EMaterialRasterCullMode::Front;
                return true;
            }
            if (Core::Utility::String::EqualLiteralI(value, "back")) {
                out = EMaterialRasterCullMode::Back;
                return true;
            }
            return false;
        }

        auto TryParseRasterFrontFace(
            Container::FNativeStringView value, EMaterialRasterFrontFace& out) noexcept -> bool {
            if (Core::Utility::String::EqualLiteralI(value, "ccw")) {
                out = EMaterialRasterFrontFace::CCW;
                return true;
            }
            if (Core::Utility::String::EqualLiteralI(value, "cw")) {
                out = EMaterialRasterFrontFace::CW;
                return true;
            }
            return false;
        }

        void ParseRasterOverridesObject(
            const FJsonValue& value, FMaterialRasterStateOverrides& outOverrides) {
            if (value.Type != EJsonType::Object) {
                return;
            }

            auto KeyEquals = [](Container::FNativeStringView key, const char* literalLower) {
                if (literalLower == nullptr) {
                    return false;
                }

                usize j = 0U;
                for (usize i = 0U; i < key.Length(); ++i) {
                    const char ch = key[i];
                    if (ch == '_' || ch == '-') {
                        continue;
                    }

                    const char expected = literalLower[j];
                    if (expected == '\0') {
                        return false;
                    }

                    if (Core::Algorithm::ToLowerChar(ch) != expected) {
                        return false;
                    }
                    ++j;
                }
                return literalLower[j] == '\0';
            };

            for (const auto& pair : value.Object) {
                if (pair.Value == nullptr) {
                    continue;
                }

                const Container::FNativeStringView key(pair.Key.GetData(), pair.Key.Length());

                if (pair.Value->Type == EJsonType::String) {
                    const Container::FNativeStringView text(
                        pair.Value->String.GetData(), pair.Value->String.Length());

                    if (KeyEquals(key, "fillmode")) {
                        EMaterialRasterFillMode mode{};
                        if (TryParseRasterFillMode(text, mode)) {
                            outOverrides.mHasFillMode = true;
                            outOverrides.mFillMode    = mode;
                        }
                        continue;
                    }

                    if (KeyEquals(key, "cullmode") || KeyEquals(key, "cull")) {
                        EMaterialRasterCullMode mode{};
                        if (TryParseRasterCullMode(text, mode)) {
                            outOverrides.mHasCullMode = true;
                            outOverrides.mCullMode    = mode;
                        }
                        continue;
                    }

                    if (KeyEquals(key, "frontface") || KeyEquals(key, "front")) {
                        EMaterialRasterFrontFace mode{};
                        if (TryParseRasterFrontFace(text, mode)) {
                            outOverrides.mHasFrontFace = true;
                            outOverrides.mFrontFace    = mode;
                        }
                        continue;
                    }
                }

                if (pair.Value->Type == EJsonType::Number) {
                    double number = 0.0;
                    if (!GetNumberValue(pair.Value, number)) {
                        continue;
                    }

                    if (KeyEquals(key, "depthbias")) {
                        outOverrides.mHasDepthBias = true;
                        outOverrides.mDepthBias    = static_cast<i32>(number);
                        continue;
                    }

                    if (KeyEquals(key, "depthbiasclamp")) {
                        outOverrides.mHasDepthBiasClamp = true;
                        outOverrides.mDepthBiasClamp    = static_cast<f32>(number);
                        continue;
                    }

                    if (KeyEquals(key, "slopescaleddepthbias")) {
                        outOverrides.mHasSlopeScaledDepthBias = true;
                        outOverrides.mSlopeScaledDepthBias    = static_cast<f32>(number);
                        continue;
                    }
                }

                if (pair.Value->Type == EJsonType::Bool) {
                    bool flag = false;
                    if (!GetBoolValue(pair.Value, flag)) {
                        continue;
                    }

                    if (KeyEquals(key, "depthclip")) {
                        outOverrides.mHasDepthClip = true;
                        outOverrides.mDepthClip    = flag;
                        continue;
                    }

                    if (KeyEquals(key, "conservativeraster")) {
                        outOverrides.mHasConservativeRaster = true;
                        outOverrides.mConservativeRaster    = flag;
                        continue;
                    }
                }
            }
        }

        auto ParseMaterialTemplate(const FJsonValue& root, Container::FString& outName,
            TVector<FMaterialPassTemplate>&       outPasses,
            TVector<TVector<Container::FString>>& outVariants) -> bool {
            const FJsonValue* nameValue = FindObjectValueInsensitive(root, "Name");
            if (nameValue != nullptr && nameValue->Type == EJsonType::String) {
                outName = Core::Utility::String::FromUtf8(nameValue->String);
            }

            const FJsonValue* passesValue = FindObjectValueInsensitive(root, "Passes");
            if (passesValue == nullptr || passesValue->Type != EJsonType::Object) {
                return false;
            }

            for (const auto& pair : passesValue->Object) {
                if (pair.Value == nullptr || pair.Value->Type != EJsonType::Object) {
                    continue;
                }

                FMaterialPassTemplate pass{};
                pass.mName = Core::Utility::String::FromUtf8(pair.Key);
                if (pass.mName.IsEmptyString()) {
                    continue;
                }

                const FJsonValue* presetValue = FindObjectValueInsensitive(*pair.Value, "Preset");
                if (presetValue != nullptr && presetValue->Type == EJsonType::String) {
                    pass.mPreset = Core::Utility::String::FromUtf8(presetValue->String);
                }

                const bool        hasPreset    = !pass.mPreset.IsEmptyString();
                const FJsonValue* shadersValue = FindObjectValueInsensitive(*pair.Value, "Shaders");
                if (!hasPreset) {
                    if (shadersValue == nullptr || shadersValue->Type != EJsonType::Object) {
                        return false;
                    }
                }

                if (shadersValue != nullptr && shadersValue->Type == EJsonType::Object) {
                    if (const FJsonValue* vsValue =
                            FindObjectValueInsensitive(*shadersValue, "vs")) {
                        pass.mHasVertex = ParseShaderSource(*vsValue, pass.mVertex);
                    }
                    if (const FJsonValue* psValue =
                            FindObjectValueInsensitive(*shadersValue, "ps")) {
                        pass.mHasPixel = ParseShaderSource(*psValue, pass.mPixel);
                    }
                    if (const FJsonValue* csValue =
                            FindObjectValueInsensitive(*shadersValue, "cs")) {
                        pass.mHasCompute = ParseShaderSource(*csValue, pass.mCompute);
                    }
                }

                if (!hasPreset && !pass.mHasVertex && !pass.mHasCompute) {
                    return false;
                }

                if (const FJsonValue* overridesValue =
                        FindObjectValueInsensitive(*pair.Value, "Overrides")) {
                    ParseOverridesObject(*overridesValue, pass.mOverrides);
                }

                if (const FJsonValue* rasterValue = FindRasterOverridesValue(*pair.Value)) {
                    ParseRasterOverridesObject(*rasterValue, pass.mRasterOverrides);
                }

                outPasses.PushBack(Move(pass));
            }

            const FJsonValue* variantsValue =
                FindObjectValueInsensitive(root, "Precompile_Variants");
            if (variantsValue != nullptr && variantsValue->Type == EJsonType::Array) {
                for (const auto* variantValue : variantsValue->Array) {
                    if (variantValue == nullptr || variantValue->Type != EJsonType::Array) {
                        continue;
                    }
                    TVector<Container::FString> variant;
                    for (const auto* item : variantValue->Array) {
                        if (item == nullptr || item->Type != EJsonType::String) {
                            continue;
                        }
                        variant.PushBack(Core::Utility::String::FromUtf8(item->String));
                    }
                    outVariants.PushBack(Move(variant));
                }
            }

            return !outPasses.IsEmpty();
        }
    } // namespace

    auto FMaterialLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::MaterialTemplate;
    }

    auto FMaterialLoader::Load(const FAssetDesc&, IAssetStream& stream) -> TShared<IAsset> {
        TVector<u8> bytes;
        if (!ReadAllBytes(stream, bytes)) {
            return {};
        }

        Container::FNativeString native;
        if (!bytes.IsEmpty()) {
            native.Append(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
        }

        const Container::FNativeStringView view(native.GetData(), native.Length());
        FJsonDocument                      document;
        if (!document.Parse(view)) {
            return {};
        }

        const FJsonValue* root = document.GetRoot();
        if (root == nullptr || root->Type != EJsonType::Object) {
            return {};
        }

        Container::FString                   name;
        TVector<FMaterialPassTemplate>       passes;
        TVector<TVector<Container::FString>> variants;
        if (!ParseMaterialTemplate(*root, name, passes, variants)) {
            return {};
        }

        return MakeSharedAsset<FMaterialAsset>(Move(name), Move(passes), Move(variants));
    }

} // namespace AltinaEngine::Asset

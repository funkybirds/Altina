#include "Asset/MaterialLoader.h"

#include "Asset/MaterialAsset.h"
#include "Algorithm/CStringUtils.h"
#include "Types/Traits.h"
#include "Utility/Json.h"
#include "Utility/Uuid.h"

#include <cstring>
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

using AltinaEngine::Forward;
using AltinaEngine::Move;
using AltinaEngine::Core::Container::DestroyPolymorphic;
namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;
    namespace {
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
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
            using Container::kSmartPtrUseManagedAllocator;
            using Container::TAllocator;
            using Container::TAllocatorTraits;
            using Container::TPolymorphicDeleter;

            TDerived* ptr = nullptr;
            if constexpr (kSmartPtrUseManagedAllocator) {
                TAllocator<TDerived> allocator;
                ptr = TAllocatorTraits<TAllocator<TDerived>>::Allocate(allocator, 1);
                if (ptr == nullptr) {
                    return {};
                }

                try {
                    TAllocatorTraits<TAllocator<TDerived>>::Construct(
                        allocator, ptr, Forward<Args>(args)...);
                } catch (...) {
                    TAllocatorTraits<TAllocator<TDerived>>::Deallocate(allocator, ptr, 1);
                    return {};
                }
            } else {
                ptr = new TDerived(Forward<Args>(args)...); // NOLINT
            }

            return TShared<IAsset>(
                ptr, TPolymorphicDeleter<IAsset>(&DestroyPolymorphic<IAsset, TDerived>));
        }

        auto FromUtf8(const Container::FNativeString& value) -> Container::FString {
            Container::FString out;
            if (value.IsEmptyString()) {
                return out;
            }
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
    #if AE_PLATFORM_WIN
            int wideCount = MultiByteToWideChar(
                CP_UTF8, 0, value.GetData(), static_cast<int>(value.Length()), nullptr, 0);
            if (wideCount <= 0) {
                return out;
            }
            std::wstring wide(static_cast<size_t>(wideCount), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, value.GetData(), static_cast<int>(value.Length()),
                wide.data(), wideCount);
            out.Append(wide.c_str(), wide.size());
    #else
            out.Append(value.GetData(), value.Length());
    #endif
#else
            out.Append(value.GetData(), value.Length());
#endif
            return out;
        }

        auto ParseUuid(const Container::FNativeString& text, FUuid& out) -> bool {
            if (text.IsEmptyString()) {
                return false;
            }
            return FUuid::TryParse(
                Container::FNativeStringView(text.GetData(), text.Length()), out);
        }

        auto ParseAssetTypeText(const FJsonValue* value, EAssetType& outType) -> bool {
            Container::FNativeString typeText;
            if (!GetStringValue(value, typeText)) {
                return false;
            }

            auto EqualLiteralI = [](Container::FNativeStringView text, const char* literal) {
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
            };

            Container::FNativeStringView view(typeText.GetData(), typeText.Length());
            if (EqualLiteralI(view, "shader")) {
                outType = EAssetType::Shader;
                return true;
            }
            if (EqualLiteralI(view, "materialtemplate") || EqualLiteralI(view, "material")) {
                outType = EAssetType::MaterialTemplate;
                return true;
            }
            if (EqualLiteralI(view, "materialinstance")) {
                outType = EAssetType::MaterialInstance;
                return true;
            }
            return false;
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
            if (!ParseUuid(uuidText, uuid)) {
                return false;
            }

            EAssetType type = EAssetType::Shader;
            const FJsonValue* typeValue = FindObjectValueInsensitive(value, "Type");
            if (typeValue != nullptr) {
                (void)ParseAssetTypeText(typeValue, type);
            }

            FNativeString entryText;
            if (!GetStringValue(FindObjectValueInsensitive(value, "Entry"), entryText)) {
                return false;
            }

            out.Asset.Uuid = uuid;
            out.Asset.Type = type;
            out.Entry      = FromUtf8(entryText);
            return out.Asset.IsValid() && !out.Entry.IsEmptyString();
        }

        auto ParseMaterialTemplate(const FJsonValue& root, Container::FString& outName,
            TVector<FMaterialPassTemplate>& outPasses,
            TVector<TVector<Container::FString>>& outVariants) -> bool {
            const FJsonValue* nameValue = FindObjectValueInsensitive(root, "Name");
            if (nameValue != nullptr && nameValue->Type == EJsonType::String) {
                outName = FromUtf8(nameValue->String);
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
                pass.Name = FromUtf8(pair.Key);
                if (pass.Name.IsEmptyString()) {
                    continue;
                }

                const FJsonValue* shadersValue =
                    FindObjectValueInsensitive(*pair.Value, "Shaders");
                if (shadersValue == nullptr || shadersValue->Type != EJsonType::Object) {
                    return false;
                }

                if (const FJsonValue* vsValue =
                        FindObjectValueInsensitive(*shadersValue, "vs")) {
                    pass.HasVertex = ParseShaderSource(*vsValue, pass.Vertex);
                }
                if (const FJsonValue* psValue =
                        FindObjectValueInsensitive(*shadersValue, "ps")) {
                    pass.HasPixel = ParseShaderSource(*psValue, pass.Pixel);
                }
                if (const FJsonValue* csValue =
                        FindObjectValueInsensitive(*shadersValue, "cs")) {
                    pass.HasCompute = ParseShaderSource(*csValue, pass.Compute);
                }

                if (!pass.HasVertex && !pass.HasCompute) {
                    return false;
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
                        variant.PushBack(FromUtf8(item->String));
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

        Container::FString name;
        TVector<FMaterialPassTemplate> passes;
        TVector<TVector<Container::FString>> variants;
        if (!ParseMaterialTemplate(*root, name, passes, variants)) {
            return {};
        }

        return MakeSharedAsset<FMaterialAsset>(Move(name), Move(passes), Move(variants));
    }

} // namespace AltinaEngine::Asset

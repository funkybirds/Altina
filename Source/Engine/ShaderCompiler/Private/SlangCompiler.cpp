#include "ShaderAutoBinding.h"
#include "ShaderCompilerBackend.h"
#include "ShaderCompilerUtils.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Platform/PlatformFileSystem.h"
#include "Platform/PlatformProcess.h"

#include <cctype>
#include <cstdlib>
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

namespace AltinaEngine::ShaderCompiler::Detail {
    using Core::Container::FString;
    using Core::Platform::ReadFileBytes;
    using Core::Platform::ReadFileTextUtf8;
    using Core::Platform::RemoveFileIfExists;
    using Core::Platform::RunProcess;

    namespace {
        using Core::Container::FNativeString;
        using Core::Container::FStringView;

        constexpr const TChar* kSlangName = TEXT("Slang");
        constexpr const TChar* kSlangDisabledMessage =
            TEXT("Slang backend disabled. Define AE_SHADER_COMPILER_ENABLE_SLANG=1 to enable.");

        enum class EJsonType : u8 {
            Null = 0,
            Bool,
            Number,
            String,
            Object,
            Array
        };

        struct FJsonValue;

        struct FJsonPair {
            FNativeString mKey;
            FJsonValue*   mValue = nullptr;
        };

        struct FJsonValue {
            EJsonType              mType = EJsonType::Null;
            bool                   mBool = false;
            double                 mNumber = 0.0;
            FNativeString          mString;
            TVector<FJsonValue*>   mArray;
            TVector<FJsonPair>     mObject;
        };

        class FJsonReader {
        public:
            explicit FJsonReader(const FNativeString& text)
                : mPtr(text.GetData()), mEnd(text.GetData() + text.Length()) {}

            ~FJsonReader() {
                for (auto* value : mOwnedValues) {
                    delete value;
                }
            }

            auto Parse(FJsonValue& outValue) -> bool {
                SkipWhitespace();
                if (!ParseValue(outValue)) {
                    return false;
                }
                SkipWhitespace();
                return true;
            }

            auto GetError() const -> const char* { return mError; }

        private:
            void SkipWhitespace() {
                while (mPtr < mEnd && std::isspace(static_cast<unsigned char>(*mPtr))) {
                    ++mPtr;
                }
            }

            auto ParseValue(FJsonValue& outValue) -> bool {
                SkipWhitespace();
                if (mPtr >= mEnd) {
                    mError = "Unexpected end of JSON.";
                    return false;
                }

                const char ch = *mPtr;
                if (ch == '"') {
                    outValue.mType = EJsonType::String;
                    return ParseString(outValue.mString);
                }
                if (ch == '{') {
                    outValue.mType = EJsonType::Object;
                    return ParseObject(outValue);
                }
                if (ch == '[') {
                    outValue.mType = EJsonType::Array;
                    return ParseArray(outValue);
                }
                if (ch == 't' || ch == 'f') {
                    outValue.mType = EJsonType::Bool;
                    return ParseBool(outValue.mBool);
                }
                if (ch == 'n') {
                    outValue.mType = EJsonType::Null;
                    return ParseNull();
                }
                if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
                    outValue.mType = EJsonType::Number;
                    return ParseNumber(outValue.mNumber);
                }

                mError = "Invalid JSON token.";
                return false;
            }

            auto ParseString(FNativeString& out) -> bool {
                if (*mPtr != '"') {
                    mError = "Expected string.";
                    return false;
                }
                ++mPtr;
                out.Clear();

                while (mPtr < mEnd) {
                    char ch = *mPtr++;
                    if (ch == '"') {
                        return true;
                    }
                    if (ch == '\\') {
                        if (mPtr >= mEnd) {
                            mError = "Invalid escape.";
                            return false;
                        }
                        const char esc = *mPtr++;
                        switch (esc) {
                        case '"': out.Append("\"", 1); break;
                        case '\\': out.Append("\\", 1); break;
                        case '/': out.Append("/", 1); break;
                        case 'b': out.Append("\b", 1); break;
                        case 'f': out.Append("\f", 1); break;
                        case 'n': out.Append("\n", 1); break;
                        case 'r': out.Append("\r", 1); break;
                        case 't': out.Append("\t", 1); break;
                        case 'u':
                            // Skip unicode escape for now; emit placeholder.
                            if (mEnd - mPtr >= 4) {
                                mPtr += 4;
                            } else {
                                mPtr = mEnd;
                            }
                            out.Append("?", 1);
                            break;
                        default:
                            out.Append(&esc, 1);
                            break;
                        }
                    } else {
                        out.Append(&ch, 1);
                    }
                }

                mError = "Unterminated string.";
                return false;
            }

            auto ParseObject(FJsonValue& out) -> bool {
                if (*mPtr != '{') {
                    mError = "Expected object.";
                    return false;
                }
                ++mPtr;
                SkipWhitespace();

                if (mPtr < mEnd && *mPtr == '}') {
                    ++mPtr;
                    return true;
                }

                while (mPtr < mEnd) {
                    FNativeString key;
                    if (!ParseString(key)) {
                        return false;
                    }
                    SkipWhitespace();
                    if (mPtr >= mEnd || *mPtr != ':') {
                        mError = "Expected ':' in object.";
                        return false;
                    }
                    ++mPtr;
                    SkipWhitespace();
                    auto* value = new FJsonValue();
                    mOwnedValues.PushBack(value);
                    if (!ParseValue(*value)) {
                        return false;
                    }
                    FJsonPair pair;
                    pair.mKey   = key;
                    pair.mValue = value;
                    out.mObject.PushBack(pair);
                    SkipWhitespace();

                    if (mPtr < mEnd && *mPtr == ',') {
                        ++mPtr;
                        SkipWhitespace();
                        continue;
                    }
                    if (mPtr < mEnd && *mPtr == '}') {
                        ++mPtr;
                        return true;
                    }
                    mError = "Expected ',' or '}' in object.";
                    return false;
                }

                mError = "Unterminated object.";
                return false;
            }

            auto ParseArray(FJsonValue& out) -> bool {
                if (*mPtr != '[') {
                    mError = "Expected array.";
                    return false;
                }
                ++mPtr;
                SkipWhitespace();

                if (mPtr < mEnd && *mPtr == ']') {
                    ++mPtr;
                    return true;
                }

                while (mPtr < mEnd) {
                    auto* value = new FJsonValue();
                    mOwnedValues.PushBack(value);
                    if (!ParseValue(*value)) {
                        return false;
                    }
                    out.mArray.PushBack(value);
                    SkipWhitespace();

                    if (mPtr < mEnd && *mPtr == ',') {
                        ++mPtr;
                        SkipWhitespace();
                        continue;
                    }
                    if (mPtr < mEnd && *mPtr == ']') {
                        ++mPtr;
                        return true;
                    }
                    mError = "Expected ',' or ']' in array.";
                    return false;
                }

                mError = "Unterminated array.";
                return false;
            }

            auto ParseBool(bool& out) -> bool {
                if ((mEnd - mPtr) >= 4 && std::strncmp(mPtr, "true", 4) == 0) {
                    out  = true;
                    mPtr += 4;
                    return true;
                }
                if ((mEnd - mPtr) >= 5 && std::strncmp(mPtr, "false", 5) == 0) {
                    out  = false;
                    mPtr += 5;
                    return true;
                }
                mError = "Invalid boolean.";
                return false;
            }

            auto ParseNull() -> bool {
                if ((mEnd - mPtr) >= 4 && std::strncmp(mPtr, "null", 4) == 0) {
                    mPtr += 4;
                    return true;
                }
                mError = "Invalid null.";
                return false;
            }

            auto ParseNumber(double& out) -> bool {
                char* endPtr = nullptr;
                out          = std::strtod(mPtr, &endPtr);
                if (endPtr == mPtr) {
                    mError = "Invalid number.";
                    return false;
                }
                mPtr = endPtr;
                return true;
            }

        private:
            const char*           mPtr   = nullptr;
            const char*           mEnd   = nullptr;
            const char*           mError = nullptr;
            TVector<FJsonValue*>  mOwnedValues;
        };

        auto FindObjectValue(const FJsonValue& object, const char* key) -> const FJsonValue* {
            for (const auto& pair : object.mObject) {
                if (pair.mKey.Length() == std::strlen(key)
                    && std::strncmp(pair.mKey.GetData(), key, pair.mKey.Length()) == 0) {
                    return pair.mValue;
                }
            }
            return nullptr;
        }

        auto GetStringValue(const FJsonValue* value, FNativeString& out) -> bool {
            if (value == nullptr || value->mType != EJsonType::String) {
                return false;
            }
            out = value->mString;
            return true;
        }

        auto GetNumberValue(const FJsonValue* value, double& out) -> bool {
            if (value == nullptr || value->mType != EJsonType::Number) {
                return false;
            }
            out = value->mNumber;
            return true;
        }

        auto GetNumberAsU32(const FJsonValue* value, u32& out) -> bool {
            double number = 0.0;
            if (!GetNumberValue(value, number) || number < 0.0) {
                return false;
            }
            out = static_cast<u32>(number);
            return true;
        }

        auto GetLayoutOffsetBytes(const FJsonValue* value, u32& out) -> bool {
            if (GetNumberAsU32(value, out)) {
                return true;
            }
            if (value == nullptr || value->mType != EJsonType::Object) {
                return false;
            }
            if (GetNumberAsU32(FindObjectValue(*value, "uniform"), out)) {
                return true;
            }
            if (GetNumberAsU32(FindObjectValue(*value, "constantBuffer"), out)) {
                return true;
            }
            if (GetNumberAsU32(FindObjectValue(*value, "byteOffset"), out)) {
                return true;
            }
            if (GetNumberAsU32(FindObjectValue(*value, "offset"), out)) {
                return true;
            }
            return false;
        }

        auto GetLayoutSizeBytes(const FJsonValue* layout, u32& out) -> bool {
            if (layout == nullptr || layout->mType != EJsonType::Object) {
                return false;
            }
            if (GetNumberAsU32(FindObjectValue(*layout, "size"), out)) {
                return true;
            }
            if (GetNumberAsU32(FindObjectValue(*layout, "uniformSize"), out)) {
                return true;
            }
            const auto* sizeObj = FindObjectValue(*layout, "size");
            if (sizeObj != nullptr && sizeObj->mType == EJsonType::Object) {
                if (GetNumberAsU32(FindObjectValue(*sizeObj, "uniform"), out)) {
                    return true;
                }
                if (GetNumberAsU32(FindObjectValue(*sizeObj, "constantBuffer"), out)) {
                    return true;
                }
                if (GetNumberAsU32(FindObjectValue(*sizeObj, "byteSize"), out)) {
                    return true;
                }
            }
            return false;
        }

        auto NativeToFString(const FNativeString& value) -> FString;

        auto ParseSlangTypeLayoutFields(const FJsonValue* layout, const FString& prefix,
            u32 baseOffset, FShaderConstantBuffer& outCb) -> void {
            if (layout == nullptr || layout->mType != EJsonType::Object) {
                return;
            }
            const auto* fields = FindObjectValue(*layout, "fields");
            if (fields == nullptr || fields->mType != EJsonType::Array) {
                return;
            }

            for (const auto* field : fields->mArray) {
                if (field == nullptr || field->mType != EJsonType::Object) {
                    continue;
                }

                FNativeString name;
                if (!GetStringValue(FindObjectValue(*field, "name"), name)) {
                    continue;
                }

                u32 offsetBytes = 0U;
                const auto* offsetValue = FindObjectValue(*field, "offset");
                if (!GetLayoutOffsetBytes(offsetValue, offsetBytes)) {
                    GetNumberAsU32(FindObjectValue(*field, "uniformOffset"), offsetBytes);
                }

                const auto* fieldTypeLayout = FindObjectValue(*field, "typeLayout");
                u32         sizeBytes = 0U;
                if (!GetLayoutSizeBytes(fieldTypeLayout, sizeBytes)) {
                    GetNumberAsU32(FindObjectValue(*field, "size"), sizeBytes);
                }

                const auto* fieldType = FindObjectValue(*field, "type");
                FNativeString kind;
                if (fieldType != nullptr && fieldType->mType == EJsonType::Object) {
                    GetStringValue(FindObjectValue(*fieldType, "kind"), kind);
                }

                u32 elementCount = 0U;
                GetNumberAsU32(FindObjectValue(*field, "elementCount"), elementCount);
                if (elementCount == 0U && fieldType != nullptr && fieldType->mType == EJsonType::Object) {
                    GetNumberAsU32(FindObjectValue(*fieldType, "elementCount"), elementCount);
                }

                const u32 elementStride =
                    (elementCount > 0U && sizeBytes > 0U) ? (sizeBytes / elementCount) : 0U;

                FString fullName = prefix;
                if (!fullName.IsEmptyString()) {
                    fullName.Append(TEXT("."));
                }
                const FString nameText = NativeToFString(name);
                if (!nameText.IsEmptyString()) {
                    fullName.Append(nameText.GetData(), nameText.Length());
                }

                FShaderConstantBufferMember member{};
                member.mName          = fullName;
                member.mOffset        = baseOffset + offsetBytes;
                member.mSize          = sizeBytes;
                member.mElementCount  = elementCount;
                member.mElementStride = elementStride;
                outCb.mMembers.PushBack(member);

                const std::string kindStr(kind.GetData(), kind.Length());
                const bool isArray = (kindStr == "array");
                if (!isArray && kindStr == "struct") {
                    ParseSlangTypeLayoutFields(fieldTypeLayout, fullName,
                        baseOffset + offsetBytes, outCb);
                }
            }
        }

        auto NativeToFString(const FNativeString& value) -> FString {
            FString out;
            if (value.IsEmptyString()) {
                return out;
            }
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
#if AE_PLATFORM_WIN
            const int wideCount = MultiByteToWideChar(CP_UTF8, 0, value.GetData(),
                static_cast<int>(value.Length()), nullptr, 0);
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

        auto MapResourceKind(const FNativeString& kind, const FNativeString& baseShape,
            const FNativeString& access, EShaderResourceAccess& outAccess)
            -> EShaderResourceType {
            outAccess = EShaderResourceAccess::ReadOnly;
            const std::string accessStr(access.GetData(), access.Length());
            if (accessStr == "readWrite") {
                outAccess = EShaderResourceAccess::ReadWrite;
            }

            const std::string kindStr(kind.GetData(), kind.Length());
            if (kindStr.empty()) {
                return EShaderResourceType::Texture;
            }

            if (kindStr == "constantBuffer") {
                return EShaderResourceType::ConstantBuffer;
            }
            if (kindStr == "samplerState") {
                return EShaderResourceType::Sampler;
            }
            if (kindStr == "resource") {
                const std::string shapeStr(baseShape.GetData(), baseShape.Length());
                if (!shapeStr.empty()) {
                    if (shapeStr.find("texture") != std::string::npos) {
                        return (outAccess == EShaderResourceAccess::ReadWrite)
                            ? EShaderResourceType::StorageTexture
                            : EShaderResourceType::Texture;
                    }
                    if (shapeStr.find("buffer") != std::string::npos) {
                        return EShaderResourceType::StorageBuffer;
                    }
                }
                return EShaderResourceType::Texture;
            }

            return EShaderResourceType::Texture;
        }

        auto ParseSlangReflectionJson(const FNativeString& text, FShaderReflection& outReflection,
            FString& diagnostics) -> bool {
            FJsonValue root;
            FJsonReader reader(text);
            if (!reader.Parse(root) || root.mType != EJsonType::Object) {
                AppendDiagnosticLine(diagnostics, TEXT("Failed to parse Slang reflection JSON."));
                return false;
            }

            outReflection.mResources.Clear();
            outReflection.mConstantBuffers.Clear();

            const auto* params = FindObjectValue(root, "parameters");
            if (params != nullptr && params->mType == EJsonType::Array) {
                outReflection.mResources.Reserve(params->mArray.Size());
                outReflection.mConstantBuffers.Reserve(params->mArray.Size());

                for (const auto* param : params->mArray) {
                    if (param == nullptr || param->mType != EJsonType::Object) {
                        continue;
                    }

                    FNativeString name;
                    if (!GetStringValue(FindObjectValue(*param, "name"), name)) {
                        continue;
                    }

                    const auto* bindingObj = FindObjectValue(*param, "binding");
                    u32 bindingIndex       = 0;
                    u32 bindingSet         = 0;
                    if (bindingObj != nullptr && bindingObj->mType == EJsonType::Object) {
                        double indexValue = 0.0;
                        if (GetNumberValue(FindObjectValue(*bindingObj, "index"), indexValue)) {
                            bindingIndex = static_cast<u32>(indexValue);
                        }
                        double spaceValue = 0.0;
                        if (GetNumberValue(FindObjectValue(*bindingObj, "space"), spaceValue)) {
                            bindingSet = static_cast<u32>(spaceValue);
                        }
                    }

                    const auto* typeObj = FindObjectValue(*param, "type");
                    FNativeString kind;
                    FNativeString baseShape;
                    FNativeString access;
                    if (typeObj != nullptr && typeObj->mType == EJsonType::Object) {
                        GetStringValue(FindObjectValue(*typeObj, "kind"), kind);
                        GetStringValue(FindObjectValue(*typeObj, "baseShape"), baseShape);
                        GetStringValue(FindObjectValue(*typeObj, "access"), access);
                    }

                    FShaderResourceBinding binding{};
                    binding.mName     = NativeToFString(name);
                    binding.mBinding  = bindingIndex;
                    binding.mSet      = bindingSet;
                    binding.mRegister = bindingIndex;
                    binding.mSpace    = bindingSet;
                    binding.mType     = MapResourceKind(kind, baseShape, access, binding.mAccess);
                    outReflection.mResources.PushBack(binding);

                    const std::string kindStr(kind.GetData(), kind.Length());
                    if (kindStr == "constantBuffer") {
                        FShaderConstantBuffer cbInfo{};
                        cbInfo.mName      = binding.mName;
                        cbInfo.mBinding   = binding.mBinding;
                        cbInfo.mSet       = binding.mSet;
                        cbInfo.mRegister  = binding.mRegister;
                        cbInfo.mSpace     = binding.mSpace;

                        const auto* typeLayout = FindObjectValue(*param, "typeLayout");
                        const auto* layout = typeLayout;
                        if (typeLayout != nullptr && typeLayout->mType == EJsonType::Object) {
                            const auto* elementLayout =
                                FindObjectValue(*typeLayout, "elementTypeLayout");
                            if (elementLayout != nullptr && elementLayout->mType == EJsonType::Object) {
                                layout = elementLayout;
                            }
                        }

                        if (layout != nullptr && layout->mType == EJsonType::Object) {
                            GetLayoutSizeBytes(layout, cbInfo.mSizeBytes);
                            ParseSlangTypeLayoutFields(layout, FString{}, 0U, cbInfo);
                        }

                        outReflection.mConstantBuffers.PushBack(cbInfo);
                    }
                }
            }

            const auto* entryPoints = FindObjectValue(root, "entryPoints");
            if (entryPoints != nullptr && entryPoints->mType == EJsonType::Array
                && entryPoints->mArray.Size() > 0) {
                const auto* entry = entryPoints->mArray[0];
                if (entry != nullptr && entry->mType == EJsonType::Object) {
                    const auto* threadGroup = FindObjectValue(*entry, "threadGroupSize");
                    if (threadGroup != nullptr && threadGroup->mType == EJsonType::Array
                        && threadGroup->mArray.Size() >= 3) {
                        double tgx = 1.0;
                        double tgy = 1.0;
                        double tgz = 1.0;
                        GetNumberValue(threadGroup->mArray[0], tgx);
                        GetNumberValue(threadGroup->mArray[1], tgy);
                        GetNumberValue(threadGroup->mArray[2], tgz);
                        outReflection.mThreadGroupSizeX = static_cast<u32>(tgx);
                        outReflection.mThreadGroupSizeY = static_cast<u32>(tgy);
                        outReflection.mThreadGroupSizeZ = static_cast<u32>(tgz);
                    }
                }
            }

            return true;
        }

        auto GetStageName(EShaderStage stage) -> const TChar* {
            switch (stage) {
            case EShaderStage::Vertex:
                return TEXT("vertex");
            case EShaderStage::Pixel:
                return TEXT("fragment");
            case EShaderStage::Compute:
                return TEXT("compute");
            case EShaderStage::Geometry:
                return TEXT("geometry");
            case EShaderStage::Hull:
                return TEXT("hull");
            case EShaderStage::Domain:
                return TEXT("domain");
            case EShaderStage::Mesh:
                return TEXT("mesh");
            case EShaderStage::Amplification:
                return TEXT("amplification");
            case EShaderStage::Library:
                return TEXT("library");
            default:
                return TEXT("fragment");
            }
        }

        auto GetProfilePrefix(EShaderStage stage) -> const TChar* {
            switch (stage) {
            case EShaderStage::Vertex:
                return TEXT("vs");
            case EShaderStage::Pixel:
                return TEXT("ps");
            case EShaderStage::Compute:
                return TEXT("cs");
            case EShaderStage::Geometry:
                return TEXT("gs");
            case EShaderStage::Hull:
                return TEXT("hs");
            case EShaderStage::Domain:
                return TEXT("ds");
            case EShaderStage::Mesh:
                return TEXT("ms");
            case EShaderStage::Amplification:
                return TEXT("as");
            case EShaderStage::Library:
                return TEXT("lib");
            default:
                return TEXT("ps");
            }
        }

        auto BuildDefaultProfile(EShaderStage stage, Rhi::ERhiBackend backend) -> FString {
            FString profile;
            profile.Append(GetProfilePrefix(stage));
            profile.Append(TEXT("_"));
            switch (backend) {
            case Rhi::ERhiBackend::DirectX11:
                profile.Append(TEXT("5_0"));
                break;
            case Rhi::ERhiBackend::DirectX12:
            case Rhi::ERhiBackend::Vulkan:
            default:
                profile.Append(TEXT("6_6"));
                break;
            }
            return profile;
        }

        auto GetTargetForBackend(Rhi::ERhiBackend backend) -> const TChar* {
            switch (backend) {
            case Rhi::ERhiBackend::DirectX11:
                return TEXT("dxbc");
            case Rhi::ERhiBackend::DirectX12:
                return TEXT("dxil");
            case Rhi::ERhiBackend::Vulkan:
                return TEXT("spirv");
            default:
                return TEXT("dxil");
            }
        }

        auto GetOptimizationFlag(EShaderOptimization optimization) -> FString {
            switch (optimization) {
            case EShaderOptimization::Debug:
                return FString(TEXT("-O0"));
            case EShaderOptimization::Performance:
                return FString(TEXT("-O3"));
            case EShaderOptimization::Size:
                return FString(TEXT("-O2"));
            case EShaderOptimization::Default:
            default:
                return FString(TEXT("-O1"));
            }
        }

        auto BuildOutputExtension(Rhi::ERhiBackend backend) -> FString {
            switch (backend) {
            case Rhi::ERhiBackend::Vulkan:
                return FString(TEXT(".spv"));
            case Rhi::ERhiBackend::DirectX11:
                return FString(TEXT(".dxbc"));
            case Rhi::ERhiBackend::DirectX12:
            default:
                return FString(TEXT(".dxil"));
            }
        }

        auto GetLanguageFlag(EShaderSourceLanguage language) -> const TChar* {
            switch (language) {
            case EShaderSourceLanguage::Slang:
                return TEXT("slang");
            case EShaderSourceLanguage::Hlsl:
            default:
                return TEXT("hlsl");
            }
        }

        auto ToFString(u32 value) -> FString {
            const auto text = std::to_string(value);
            FString    out;
            for (char ch : text) {
                out.Append(static_cast<TChar>(ch));
            }
            return out;
        }

        void AddArg(TVector<FString>& args, const TChar* text) {
            args.EmplaceBack(text);
        }

        void AppendVulkanBindingArgs(const FVulkanBindingOptions& options,
            const TVector<u32>* spaces, TVector<FString>& args) {
            if (!options.mEnableAutoShift) {
                return;
            }

            auto AppendShiftForSpace = [&](u32 space) {
                AddArg(args, TEXT("-fvk-b-shift"));
                args.PushBack(ToFString(options.mConstantBufferShift));
                args.PushBack(ToFString(space));

                AddArg(args, TEXT("-fvk-t-shift"));
                args.PushBack(ToFString(options.mTextureShift));
                args.PushBack(ToFString(space));

                AddArg(args, TEXT("-fvk-s-shift"));
                args.PushBack(ToFString(options.mSamplerShift));
                args.PushBack(ToFString(space));

                AddArg(args, TEXT("-fvk-u-shift"));
                args.PushBack(ToFString(options.mStorageShift));
                args.PushBack(ToFString(space));
            };

            if (spaces != nullptr && !spaces->IsEmpty()) {
                for (u32 space : *spaces) {
                    AppendShiftForSpace(space);
                }
            } else {
                AppendShiftForSpace(options.mSpace);
            }
        }
    } // namespace

    auto FSlangCompilerBackend::GetDisplayName() const noexcept -> FStringView {
        return FStringView(kSlangName);
    }

    auto FSlangCompilerBackend::IsAvailable() const noexcept -> bool {
        return AE_SHADER_COMPILER_ENABLE_SLANG != 0;
    }

    auto FSlangCompilerBackend::Compile(const FShaderCompileRequest& request)
        -> FShaderCompileResult {
        FShaderCompileResult result;
        result.mStage = request.mSource.mStage;
        if (!IsAvailable()) {
            result.mSucceeded   = false;
            result.mDiagnostics = FString(kSlangDisabledMessage);
            return result;
        }

        FAutoBindingOutput autoBinding;
        if (!ApplyAutoBindings(request.mSource.mPath, request.mOptions.mTargetBackend,
                autoBinding, result.mDiagnostics)) {
            result.mSucceeded = false;
            return result;
        }

        const FString sourcePath =
            autoBinding.mApplied ? autoBinding.mSourcePath : request.mSource.mPath;

        const FString outputPath =
            BuildTempOutputPath(sourcePath, FString(TEXT("slang")),
                BuildOutputExtension(request.mOptions.mTargetBackend));
        const FString reflectionPath =
            BuildTempOutputPath(sourcePath, FString(TEXT("slang")),
                FString(TEXT(".json")));

        TVector<FString> args;
        AddArg(args, TEXT("-lang"));
        args.PushBack(FString(GetLanguageFlag(request.mSource.mLanguage)));
        if (!request.mSource.mEntryPoint.IsEmptyString()) {
            AddArg(args, TEXT("-entry"));
            args.PushBack(request.mSource.mEntryPoint);
            AddArg(args, TEXT("-stage"));
            args.PushBack(FString(GetStageName(request.mSource.mStage)));
        }
        AddArg(args, TEXT("-target"));
        args.PushBack(FString(GetTargetForBackend(request.mOptions.mTargetBackend)));

        if (!request.mOptions.mTargetProfile.IsEmptyString()) {
            AddArg(args, TEXT("-profile"));
            args.PushBack(request.mOptions.mTargetProfile);
        } else {
            AddArg(args, TEXT("-profile"));
            args.PushBack(BuildDefaultProfile(request.mSource.mStage,
                request.mOptions.mTargetBackend));
        }

        if (request.mOptions.mDebugInfo) {
            AddArg(args, TEXT("-g"));
        }

        args.PushBack(GetOptimizationFlag(request.mOptions.mOptimization));

        for (const auto& includeDir : request.mSource.mIncludeDirs) {
            AddArg(args, TEXT("-I"));
            args.PushBack(includeDir);
        }

        for (const auto& macro : request.mSource.mDefines) {
            AddArg(args, TEXT("-D"));
            if (macro.mValue.IsEmptyString()) {
                args.PushBack(macro.mName);
            } else {
                FString define = macro.mName;
                define.Append(TEXT("="));
                define.Append(macro.mValue.GetData(), macro.mValue.Length());
                args.PushBack(define);
            }
        }

        AddArg(args, TEXT("-o"));
        args.PushBack(outputPath);
        AddArg(args, TEXT("-reflection-json"));
        args.PushBack(reflectionPath);

        TVector<u32> autoSpaces;
        if (autoBinding.mApplied && request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
            for (u32 i = 0; i < static_cast<u32>(EAutoBindingGroup::Count); ++i) {
                if (autoBinding.mLayout.mGroupUsed[i]) {
                    autoSpaces.PushBack(i);
                }
            }
        }

        if (request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
            AppendVulkanBindingArgs(request.mOptions.mVulkanBinding,
                autoSpaces.IsEmpty() ? nullptr : &autoSpaces, args);
        }

        args.PushBack(sourcePath);

        FString compilerPath = request.mOptions.mCompilerPathOverride;
        if (compilerPath.IsEmptyString()) {
            compilerPath = FString(TEXT("slangc.exe"));
        }

        const auto procResult = RunProcess(compilerPath, args);
        result.mDiagnostics   = procResult.mOutput;

        if (!procResult.mSucceeded) {
            result.mSucceeded = false;
            RemoveFileIfExists(outputPath);
            RemoveFileIfExists(reflectionPath);
            return result;
        }

        if (!ReadFileBytes(outputPath, result.mBytecode)) {
            AppendDiagnosticLine(result.mDiagnostics, TEXT("Failed to read Slang output file."));
            result.mSucceeded = false;
            RemoveFileIfExists(outputPath);
            RemoveFileIfExists(reflectionPath);
            return result;
        }

        FNativeString reflectionJson;
        if (!ReadFileTextUtf8(reflectionPath, reflectionJson)) {
            AppendDiagnosticLine(result.mDiagnostics,
                TEXT("Failed to read Slang reflection JSON."));
        } else if (!ParseSlangReflectionJson(reflectionJson, result.mReflection,
                       result.mDiagnostics)) {
            AppendDiagnosticLine(result.mDiagnostics,
                TEXT("Failed to parse Slang reflection JSON."));
        }

        RemoveFileIfExists(reflectionPath);

        result.mOutputDebugPath = outputPath;
        result.mSucceeded       = true;
        result.mRhiLayout       = BuildRhiBindingLayout(result.mReflection, request.mSource.mStage);
        return result;
    }

} // namespace AltinaEngine::ShaderCompiler::Detail

#include "ShaderAutoBinding.h"
#include "ShaderCompilerBackend.h"
#include "ShaderCompilerUtils.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Platform/PlatformFileSystem.h"
#include "Platform/PlatformProcess.h"
#include "Logging/Log.h"
#include "Utility/Json.h"

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
    namespace Container = Core::Container;
    using Container::FString;
    using Core::Platform::ReadFileBytes;
    using Core::Platform::ReadFileTextUtf8;
    using Core::Platform::RemoveFileIfExists;
    using Core::Platform::RunProcess;

    namespace {
        using Container::FNativeString;
        using Container::FNativeStringView;
        using Container::FStringView;

        constexpr const TChar* kSlangName = TEXT("Slang");
        constexpr const TChar* kSlangDisabledMessage =
            TEXT("Slang backend disabled. Define AE_SHADER_COMPILER_ENABLE_SLANG=1 to enable.");

        namespace Json = Core::Utility::Json;
        using Json::EJsonType;
        using Json::FindObjectValue;
        using Json::FJsonDocument;
        using Json::FJsonValue;
        using Json::GetNumberAsU32;
        using Json::GetNumberValue;
        using Json::GetStringValue;

        auto GetLayoutOffsetBytes(const FJsonValue* value, u32& out) -> bool {
            if (GetNumberAsU32(value, out)) {
                return true;
            }
            if (value == nullptr || value->Type != EJsonType::Object) {
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
            if (layout == nullptr || layout->Type != EJsonType::Object) {
                return false;
            }
            if (GetNumberAsU32(FindObjectValue(*layout, "size"), out)) {
                return true;
            }
            if (GetNumberAsU32(FindObjectValue(*layout, "uniformSize"), out)) {
                return true;
            }
            const auto* sizeObj = FindObjectValue(*layout, "size");
            if (sizeObj != nullptr && sizeObj->Type == EJsonType::Object) {
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

        auto GetBindingSizeBytes(const FJsonValue* binding, u32& out) -> bool {
            if (binding == nullptr || binding->Type != EJsonType::Object) {
                return false;
            }
            if (GetNumberAsU32(FindObjectValue(*binding, "size"), out)) {
                return true;
            }
            if (GetNumberAsU32(FindObjectValue(*binding, "byteSize"), out)) {
                return true;
            }
            if (GetNumberAsU32(FindObjectValue(*binding, "uniformSize"), out)) {
                return true;
            }
            return false;
        }

        auto NativeToFString(const FNativeString& value) -> FString;
        auto ParseSlangTypeLayoutFields(const FJsonValue* layout, const FString& prefix,
            u32 baseOffset, FShaderConstantBuffer& outCb) -> void;

        auto ParseSlangFieldsArray(const FJsonValue* fields, const FString& prefix, u32 baseOffset,
            FShaderConstantBuffer& outCb) -> void {
            if (fields == nullptr || fields->Type != EJsonType::Array) {
                return;
            }

            for (const auto* field : fields->Array) {
                if (field == nullptr || field->Type != EJsonType::Object) {
                    continue;
                }

                FNativeString name;
                if (!GetStringValue(FindObjectValue(*field, "name"), name)) {
                    continue;
                }

                const auto* fieldBinding = FindObjectValue(*field, "binding");

                u32         offsetBytes = 0U;
                if (!GetLayoutOffsetBytes(fieldBinding, offsetBytes)) {
                    const auto* offsetValue = FindObjectValue(*field, "offset");
                    if (!GetLayoutOffsetBytes(offsetValue, offsetBytes)) {
                        GetNumberAsU32(FindObjectValue(*field, "uniformOffset"), offsetBytes);
                    }
                }

                const auto* fieldTypeLayout = FindObjectValue(*field, "typeLayout");
                u32         sizeBytes       = 0U;
                if (!GetLayoutSizeBytes(fieldTypeLayout, sizeBytes)) {
                    if (!GetNumberAsU32(FindObjectValue(*field, "size"), sizeBytes)) {
                        GetBindingSizeBytes(fieldBinding, sizeBytes);
                    }
                }

                const auto*   fieldType = FindObjectValue(*field, "type");
                FNativeString kind;
                if (fieldType != nullptr && fieldType->Type == EJsonType::Object) {
                    GetStringValue(FindObjectValue(*fieldType, "kind"), kind);
                }

                u32 elementCount = 0U;
                GetNumberAsU32(FindObjectValue(*field, "elementCount"), elementCount);
                if (elementCount == 0U && fieldType != nullptr
                    && fieldType->Type == EJsonType::Object) {
                    GetNumberAsU32(FindObjectValue(*fieldType, "elementCount"), elementCount);
                }

                u32 elementStride =
                    (elementCount > 0U && sizeBytes > 0U) ? (sizeBytes / elementCount) : 0U;
                if (elementStride == 0U) {
                    GetNumberAsU32(FindObjectValue(*fieldBinding, "elementStride"), elementStride);
                }

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

                const FNativeStringView kindView(kind.GetData(), kind.Length());
                const bool              isArray = (kindView == FNativeStringView("array"));
                if (!isArray && kindView == FNativeStringView("struct")) {
                    ParseSlangTypeLayoutFields(
                        fieldTypeLayout, fullName, baseOffset + offsetBytes, outCb);
                }
            }
        }

        auto ParseSlangTypeFieldsFromTypeObject(const FJsonValue* typeObj, const FString& prefix,
            u32 baseOffset, FShaderConstantBuffer& outCb) -> void {
            if (typeObj == nullptr || typeObj->Type != EJsonType::Object) {
                return;
            }
            const auto* fields = FindObjectValue(*typeObj, "fields");
            ParseSlangFieldsArray(fields, prefix, baseOffset, outCb);
        }

        auto ParseSlangTypeLayoutFields(const FJsonValue* layout, const FString& prefix,
            u32 baseOffset, FShaderConstantBuffer& outCb) -> void {
            if (layout == nullptr || layout->Type != EJsonType::Object) {
                return;
            }
            const auto* fields = FindObjectValue(*layout, "fields");
            ParseSlangFieldsArray(fields, prefix, baseOffset, outCb);
        }

        auto NativeToFString(const FNativeString& value) -> FString {
            FString out;
            if (value.IsEmptyString()) {
                return out;
            }
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
    #if AE_PLATFORM_WIN
            const int wideCount = MultiByteToWideChar(
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

        auto MapResourceKind(const FNativeString& kind, const FNativeString& baseShape,
            const FNativeString& access, EShaderResourceAccess& outAccess) -> EShaderResourceType {
            outAccess = EShaderResourceAccess::ReadOnly;
            const FNativeStringView accessView(access.GetData(), access.Length());
            if (accessView == FNativeStringView("readWrite")) {
                outAccess = EShaderResourceAccess::ReadWrite;
            }

            auto contains = [](FNativeStringView  haystack,
                                FNativeStringView needle) noexcept -> bool {
                return (!haystack.IsEmpty()) && (haystack.Find(needle) != FNativeStringView::npos);
            };

            const FNativeStringView kindView(kind.GetData(), kind.Length());
            const FNativeStringView shapeView(baseShape.GetData(), baseShape.Length());

            if (kindView == FNativeStringView("constantBuffer")) {
                return EShaderResourceType::ConstantBuffer;
            }
            if (kindView == FNativeStringView("samplerState")) {
                return EShaderResourceType::Sampler;
            }

            // Slang reflection JSON isn't stable across versions for resource kinds. Handle the
            // expected "resource" kind, but also fall back to shape/name heuristics so RW buffers
            // don't get misclassified as textures.
            if (kindView == FNativeStringView("resource") || kindView.IsEmpty()
                || contains(kindView, FNativeStringView("resource"))
                || contains(kindView, FNativeStringView("buffer"))
                || contains(kindView, FNativeStringView("texture"))) {
                if (contains(shapeView, FNativeStringView("texture"))
                    || contains(shapeView, FNativeStringView("Texture"))) {
                    return (outAccess == EShaderResourceAccess::ReadWrite)
                        ? EShaderResourceType::StorageTexture
                        : EShaderResourceType::Texture;
                }
                if (contains(shapeView, FNativeStringView("buffer"))
                    || contains(shapeView, FNativeStringView("Buffer"))
                    || contains(kindView, FNativeStringView("buffer"))
                    || contains(kindView, FNativeStringView("Buffer"))) {
                    return EShaderResourceType::StorageBuffer;
                }
                return EShaderResourceType::Texture;
            }

            return EShaderResourceType::Texture;
        }

        auto IsSystemValueSemantic(const FNativeString& semantic) -> bool {
            if (semantic.Length() < 3U) {
                return false;
            }
            const auto* data = semantic.GetData();
            return ((data[0] == 'S' || data[0] == 's') && (data[1] == 'V' || data[1] == 'v')
                && data[2] == '_');
        }

        auto ParseSemanticIndexFromName(FNativeString& semanticInOut, u32& outSemanticIndex)
            -> void {
            outSemanticIndex = 0U;
            const usize len  = semanticInOut.Length();
            if (len == 0U) {
                return;
            }

            usize digitStart = len;
            while (digitStart > 0U) {
                const char c = semanticInOut[digitStart - 1U];
                if (c < '0' || c > '9') {
                    break;
                }
                --digitStart;
            }

            if (digitStart == len) {
                return;
            }

            u32 value = 0U;
            for (usize i = digitStart; i < len; ++i) {
                value = value * 10U + static_cast<u32>(semanticInOut[i] - '0');
            }
            outSemanticIndex = value;

            FNativeString base;
            base.Append(semanticInOut.GetData(), digitStart);
            semanticInOut = base;
        }

        auto GetVertexValueTypeFromTypeObject(const FJsonValue* typeObj) -> EShaderVertexValueType {
            if (typeObj == nullptr || typeObj->Type != EJsonType::Object) {
                return EShaderVertexValueType::Unknown;
            }

            const auto*   scalarSource = typeObj;
            FNativeString kind;
            if (GetStringValue(FindObjectValue(*typeObj, "kind"), kind)) {
                const FNativeStringView kindView(kind.GetData(), kind.Length());
                if (kindView == FNativeStringView("vector")
                    || kindView == FNativeStringView("matrix")
                    || kindView == FNativeStringView("array")) {
                    const auto* elementType = FindObjectValue(*typeObj, "elementType");
                    if (elementType != nullptr && elementType->Type == EJsonType::Object) {
                        scalarSource = elementType;
                    }
                }
            }

            FNativeString scalarType;
            GetStringValue(FindObjectValue(*scalarSource, "scalarType"), scalarType);
            const FNativeStringView scalarView(scalarType.GetData(), scalarType.Length());
            const bool              isFloat =
                (scalarView == FNativeStringView("float") || scalarView == FNativeStringView("half")
                    || scalarView == FNativeStringView("double")
                    || scalarView == FNativeStringView("float16")
                    || scalarView == FNativeStringView("float32")
                    || scalarView == FNativeStringView("float64"));
            const bool isInt  = (scalarView == FNativeStringView("int")
                || scalarView == FNativeStringView("int32_t")
                || scalarView == FNativeStringView("int32"));
            const bool isUInt = (scalarView == FNativeStringView("uint")
                || scalarView == FNativeStringView("uint32_t")
                || scalarView == FNativeStringView("uint32"));
            if (!isFloat && !isInt && !isUInt) {
                return EShaderVertexValueType::Unknown;
            }

            u32 laneCount = 0U;
            GetNumberAsU32(FindObjectValue(*typeObj, "elementCount"), laneCount);
            if (laneCount == 0U) {
                GetNumberAsU32(FindObjectValue(*typeObj, "columnCount"), laneCount);
            }
            if (laneCount == 0U) {
                GetNumberAsU32(FindObjectValue(*typeObj, "vectorSize"), laneCount);
            }
            if (laneCount == 0U) {
                laneCount = 1U;
            }

            switch (laneCount) {
                case 1U:
                    if (isFloat) {
                        return EShaderVertexValueType::Float1;
                    }
                    if (isInt) {
                        return EShaderVertexValueType::Int1;
                    }
                    return EShaderVertexValueType::UInt1;
                case 2U:
                    if (isFloat) {
                        return EShaderVertexValueType::Float2;
                    }
                    if (isInt) {
                        return EShaderVertexValueType::Int2;
                    }
                    return EShaderVertexValueType::UInt2;
                case 3U:
                    if (isFloat) {
                        return EShaderVertexValueType::Float3;
                    }
                    if (isInt) {
                        return EShaderVertexValueType::Int3;
                    }
                    return EShaderVertexValueType::UInt3;
                case 4U:
                    if (isFloat) {
                        return EShaderVertexValueType::Float4;
                    }
                    if (isInt) {
                        return EShaderVertexValueType::Int4;
                    }
                    return EShaderVertexValueType::UInt4;
                default:
                    return EShaderVertexValueType::Unknown;
            }
        }

        auto AddVertexInputFromJsonParam(
            const FJsonValue& paramObj, TVector<FShaderVertexInput>& outInputs) -> void {
            if (paramObj.Type != EJsonType::Object) {
                return;
            }

            FNativeString semantic;
            if (!GetStringValue(FindObjectValue(paramObj, "semanticName"), semantic)) {
                if (!GetStringValue(FindObjectValue(paramObj, "semantic"), semantic)) {
                    return;
                }
            }
            if (semantic.IsEmptyString() || IsSystemValueSemantic(semantic)) {
                return;
            }

            u32 semanticIndex = 0U;
            if (!GetNumberAsU32(FindObjectValue(paramObj, "semanticIndex"), semanticIndex)) {
                ParseSemanticIndexFromName(semantic, semanticIndex);
            }

            const auto*        typeObj = FindObjectValue(paramObj, "type");
            FShaderVertexInput input{};
            input.mSemanticName  = NativeToFString(semantic);
            input.mSemanticIndex = semanticIndex;
            input.mValueType     = GetVertexValueTypeFromTypeObject(typeObj);

            for (const auto& existing : outInputs) {
                if (existing.mSemanticIndex == input.mSemanticIndex
                    && existing.mSemanticName == input.mSemanticName) {
                    return;
                }
            }
            outInputs.PushBack(input);
        }

        auto ParseSlangReflectionJson(const FNativeString& text, FShaderReflection& outReflection,
            FString& diagnostics) -> bool {
            FJsonDocument document{};
            if (!document.Parse(FNativeStringView(text.GetData(), text.Length()))) {
                AppendDiagnosticLine(diagnostics, TEXT("Failed to parse Slang reflection JSON."));
                return false;
            }
            const auto* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                AppendDiagnosticLine(
                    diagnostics, TEXT("Failed to parse Slang reflection JSON root object."));
                return false;
            }

            outReflection.mResources.Clear();
            outReflection.mConstantBuffers.Clear();
            outReflection.mVertexInputs.Clear();

            const auto* params = FindObjectValue(*root, "parameters");
            if (params != nullptr && params->Type == EJsonType::Array) {
                outReflection.mResources.Reserve(params->Array.Size());
                outReflection.mConstantBuffers.Reserve(params->Array.Size());

                for (const auto* param : params->Array) {
                    if (param == nullptr || param->Type != EJsonType::Object) {
                        continue;
                    }

                    FNativeString name;
                    if (!GetStringValue(FindObjectValue(*param, "name"), name)) {
                        continue;
                    }

                    const auto* bindingObj   = FindObjectValue(*param, "binding");
                    u32         bindingIndex = 0;
                    u32         bindingSet   = 0;
                    if (bindingObj != nullptr && bindingObj->Type == EJsonType::Object) {
                        double indexValue = 0.0;
                        if (GetNumberValue(FindObjectValue(*bindingObj, "index"), indexValue)) {
                            bindingIndex = static_cast<u32>(indexValue);
                        }
                        double spaceValue = 0.0;
                        if (GetNumberValue(FindObjectValue(*bindingObj, "space"), spaceValue)) {
                            bindingSet = static_cast<u32>(spaceValue);
                        }
                    }

                    const auto*   typeObj = FindObjectValue(*param, "type");
                    FNativeString kind;
                    FNativeString baseShape;
                    FNativeString access;
                    if (typeObj != nullptr && typeObj->Type == EJsonType::Object) {
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

                    const FNativeStringView kindView(kind.GetData(), kind.Length());
                    if (kindView == FNativeStringView("constantBuffer")) {
                        FShaderConstantBuffer cbInfo{};
                        cbInfo.mName     = binding.mName;
                        cbInfo.mBinding  = binding.mBinding;
                        cbInfo.mSet      = binding.mSet;
                        cbInfo.mRegister = binding.mRegister;
                        cbInfo.mSpace    = binding.mSpace;

                        const auto* typeLayout = FindObjectValue(*param, "typeLayout");
                        const auto* layout     = typeLayout;
                        if (typeLayout != nullptr && typeLayout->Type == EJsonType::Object) {
                            const auto* elementLayout =
                                FindObjectValue(*typeLayout, "elementTypeLayout");
                            if (elementLayout != nullptr
                                && elementLayout->Type == EJsonType::Object) {
                                layout = elementLayout;
                            }
                        }

                        if (layout != nullptr && layout->Type == EJsonType::Object) {
                            GetLayoutSizeBytes(layout, cbInfo.mSizeBytes);
                            ParseSlangTypeLayoutFields(layout, FString{}, 0U, cbInfo);
                        } else if (typeObj != nullptr && typeObj->Type == EJsonType::Object) {
                            const auto* elementVarLayout =
                                FindObjectValue(*typeObj, "elementVarLayout");
                            if (elementVarLayout != nullptr
                                && elementVarLayout->Type == EJsonType::Object) {
                                const auto* elementBinding =
                                    FindObjectValue(*elementVarLayout, "binding");
                                GetBindingSizeBytes(elementBinding, cbInfo.mSizeBytes);

                                const auto* elementType =
                                    FindObjectValue(*elementVarLayout, "type");
                                ParseSlangTypeFieldsFromTypeObject(
                                    elementType, FString{}, 0U, cbInfo);
                            }

                            if (cbInfo.mMembers.IsEmpty()) {
                                const auto* elementType = FindObjectValue(*typeObj, "elementType");
                                ParseSlangTypeFieldsFromTypeObject(
                                    elementType, FString{}, 0U, cbInfo);
                            }

                            if (cbInfo.mMembers.IsEmpty()) {
                                ParseSlangTypeFieldsFromTypeObject(typeObj, FString{}, 0U, cbInfo);
                            }
                        }
                        if (cbInfo.mSizeBytes == 0U && !cbInfo.mMembers.IsEmpty()) {
                            u32 maxEnd = 0U;
                            for (const auto& member : cbInfo.mMembers) {
                                const u32 end = member.mOffset + member.mSize;
                                if (end > maxEnd) {
                                    maxEnd = end;
                                }
                            }
                            cbInfo.mSizeBytes = maxEnd;
                        }

                        outReflection.mConstantBuffers.PushBack(cbInfo);
                    }
                }
            }

            const auto* entryPoints = FindObjectValue(*root, "entryPoints");
            if (entryPoints != nullptr && entryPoints->Type == EJsonType::Array
                && entryPoints->Array.Size() > 0) {
                const auto* entry = entryPoints->Array[0];
                if (entry != nullptr && entry->Type == EJsonType::Object) {
                    const auto* entryParams = FindObjectValue(*entry, "parameters");
                    if (entryParams != nullptr && entryParams->Type == EJsonType::Array) {
                        for (const auto* param : entryParams->Array) {
                            if (param == nullptr || param->Type != EJsonType::Object) {
                                continue;
                            }
                            AddVertexInputFromJsonParam(*param, outReflection.mVertexInputs);

                            const auto* paramType = FindObjectValue(*param, "type");
                            if (paramType != nullptr && paramType->Type == EJsonType::Object) {
                                FNativeString kind;
                                if (GetStringValue(FindObjectValue(*paramType, "kind"), kind)) {
                                    if (FNativeStringView(kind.GetData(), kind.Length())
                                        == FNativeStringView("struct")) {
                                        const auto* fields = FindObjectValue(*paramType, "fields");
                                        if (fields != nullptr && fields->Type == EJsonType::Array) {
                                            for (const auto* field : fields->Array) {
                                                if (field == nullptr
                                                    || field->Type != EJsonType::Object) {
                                                    continue;
                                                }
                                                AddVertexInputFromJsonParam(
                                                    *field, outReflection.mVertexInputs);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    const auto* threadGroup = FindObjectValue(*entry, "threadGroupSize");
                    if (threadGroup != nullptr && threadGroup->Type == EJsonType::Array
                        && threadGroup->Array.Size() >= 3) {
                        double tgx = 1.0;
                        double tgy = 1.0;
                        double tgz = 1.0;
                        GetNumberValue(threadGroup->Array[0], tgx);
                        GetNumberValue(threadGroup->Array[1], tgy);
                        GetNumberValue(threadGroup->Array[2], tgz);
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

        auto ToFString(u32 value) -> FString { return FString::ToString(value); }

        void AddArg(TVector<FString>& args, const TChar* text) { args.EmplaceBack(text); }

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
        if (!ApplyAutoBindings(request.mSource.mPath, request.mOptions.mTargetBackend, autoBinding,
                result.mDiagnostics)) {
            result.mSucceeded = false;
            return result;
        }

        const FString sourcePath =
            autoBinding.mApplied ? autoBinding.mSourcePath : request.mSource.mPath;

        const FString outputPath = BuildTempOutputPath(sourcePath, FString(TEXT("slang")),
            BuildOutputExtension(request.mOptions.mTargetBackend));
        const FString reflectionPath =
            BuildTempOutputPath(sourcePath, FString(TEXT("slang")), FString(TEXT(".json")));

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

        // Slang defaults to column-major matrix layout when invoked via `slangc` (legacy behavior).
        // The engine uploads matrices from host code in row-major order (e.g.
        // Core::Math::FMatrix4x4f is stored as rows), so force a consistent packing to avoid
        // implicit transposes at the shader/host boundary (notably for matrix arrays like `float4x4
        // M[4]`).
        AddArg(args, TEXT("-matrix-layout-row-major"));

        if (!request.mOptions.mTargetProfile.IsEmptyString()) {
            AddArg(args, TEXT("-profile"));
            args.PushBack(request.mOptions.mTargetProfile);
        } else {
            AddArg(args, TEXT("-profile"));
            args.PushBack(
                BuildDefaultProfile(request.mSource.mStage, request.mOptions.mTargetBackend));
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

        if (request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
            AddArg(args, TEXT("-D"));
            args.PushBack(FString(TEXT("AE_SHADER_TARGET_VULKAN=1")));
        }

        AddArg(args, TEXT("-o"));
        args.PushBack(outputPath);
        AddArg(args, TEXT("-reflection-json"));
        args.PushBack(reflectionPath);

        TVector<u32> autoSpaces;
        if (request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
            if (!autoBinding.mApplied) {
                for (u32 i = 0; i < static_cast<u32>(EAutoBindingGroup::Count); ++i) {
                    autoSpaces.PushBack(i);
                }
            }
        }
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
            AppendDiagnosticLine(
                result.mDiagnostics, TEXT("Failed to read Slang reflection JSON."));
        } else if (!ParseSlangReflectionJson(
                       reflectionJson, result.mReflection, result.mDiagnostics)) {
            AppendDiagnosticLine(
                result.mDiagnostics, TEXT("Failed to parse Slang reflection JSON."));
        }

        RemoveFileIfExists(reflectionPath);

        result.mOutputDebugPath = outputPath;
        result.mSucceeded       = true;
        result.mRhiLayout       = BuildRhiBindingLayout(result.mReflection, request.mSource.mStage);
        return result;
    }

} // namespace AltinaEngine::ShaderCompiler::Detail

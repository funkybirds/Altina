#include "Geometry/VertexLayoutBuilder.h"

#include "Container/HashMap.h"

namespace AltinaEngine::RenderCore::Geometry {
    namespace {
        using Container::THashMap;

        constexpr u32 kFnvOffset32  = 2166136261u;
        constexpr u32 kFnvPrime32   = 16777619u;
        constexpr u64 kHashOffset64 = 1469598103934665603ULL;
        constexpr u64 kHashPrime64  = 1099511628211ULL;

        void          SetError(FString* outError, const TChar* message) {
            if (outError != nullptr) {
                outError->Assign(message);
            }
        }
        void SetError(FString* outError, FStringView message) {
            if (outError != nullptr) {
                outError->Assign(message);
            }
        }

        auto IsSystemSemantic(FStringView semanticName) -> bool {
            if (semanticName.Length() < 3U) {
                return false;
            }
            const auto s0 = semanticName[0];
            const auto s1 = semanticName[1];
            return ((s0 == TEXT('S') || s0 == TEXT('s')) && (s1 == TEXT('V') || s1 == TEXT('v'))
                && semanticName[2] == TEXT('_'));
        }

        auto IsFloatValueType(Shader::EShaderVertexValueType valueType) -> bool {
            switch (valueType) {
                case Shader::EShaderVertexValueType::Float1:
                case Shader::EShaderVertexValueType::Float2:
                case Shader::EShaderVertexValueType::Float3:
                case Shader::EShaderVertexValueType::Float4:
                    return true;
                default:
                    return false;
            }
        }

        auto GetValueTypeComponentCount(Shader::EShaderVertexValueType valueType) -> u32 {
            switch (valueType) {
                case Shader::EShaderVertexValueType::Float1:
                case Shader::EShaderVertexValueType::Int1:
                case Shader::EShaderVertexValueType::UInt1:
                    return 1U;
                case Shader::EShaderVertexValueType::Float2:
                case Shader::EShaderVertexValueType::Int2:
                case Shader::EShaderVertexValueType::UInt2:
                    return 2U;
                case Shader::EShaderVertexValueType::Float3:
                case Shader::EShaderVertexValueType::Int3:
                case Shader::EShaderVertexValueType::UInt3:
                    return 3U;
                case Shader::EShaderVertexValueType::Float4:
                case Shader::EShaderVertexValueType::Int4:
                case Shader::EShaderVertexValueType::UInt4:
                    return 4U;
                default:
                    return 0U;
            }
        }

        auto GetFormatComponentCount(Rhi::ERhiFormat format) -> u32 {
            switch (format) {
                case Rhi::ERhiFormat::R32Float:
                case Rhi::ERhiFormat::D32Float:
                    return 1U;
                case Rhi::ERhiFormat::R32G32Float:
                    return 2U;
                case Rhi::ERhiFormat::R32G32B32Float:
                    return 3U;
                case Rhi::ERhiFormat::R16G16B16A16Float:
                case Rhi::ERhiFormat::R8G8B8A8Unorm:
                case Rhi::ERhiFormat::R8G8B8A8UnormSrgb:
                case Rhi::ERhiFormat::B8G8R8A8Unorm:
                case Rhi::ERhiFormat::B8G8R8A8UnormSrgb:
                    return 4U;
                default:
                    return 0U;
            }
        }

        auto IsFloatFormat(Rhi::ERhiFormat format) -> bool {
            switch (format) {
                case Rhi::ERhiFormat::R32Float:
                case Rhi::ERhiFormat::R32G32Float:
                case Rhi::ERhiFormat::R32G32B32Float:
                case Rhi::ERhiFormat::R16G16B16A16Float:
                case Rhi::ERhiFormat::R8G8B8A8Unorm:
                case Rhi::ERhiFormat::R8G8B8A8UnormSrgb:
                case Rhi::ERhiFormat::B8G8R8A8Unorm:
                case Rhi::ERhiFormat::B8G8R8A8UnormSrgb:
                    return true;
                default:
                    return false;
            }
        }

        auto IsVertexFormatCompatible(
            Shader::EShaderVertexValueType valueType, Rhi::ERhiFormat format) -> bool {
            if (format == Rhi::ERhiFormat::Unknown) {
                return false;
            }
            if (valueType == Shader::EShaderVertexValueType::Unknown) {
                return false;
            }

            const u32 requiredComponents = GetValueTypeComponentCount(valueType);
            const u32 providedComponents = GetFormatComponentCount(format);
            if (requiredComponents == 0U || providedComponents == 0U) {
                return false;
            }
            if (IsFloatValueType(valueType)) {
                if (!IsFloatFormat(format)) {
                    return false;
                }
                if (requiredComponents == providedComponents) {
                    return true;
                }
                // Common shader pattern: float4 input fed by RGB vertex stream.
                if (requiredComponents == 4U && providedComponents == 3U) {
                    return true;
                }
                return false;
            }
            // Integer-valued vertex inputs are not representable with current runtime formats.
            return false;
        }

        auto BuildVertexLayoutHash(const Rhi::FRhiVertexLayoutDesc& layout) -> u64 {
            u64  hash = kHashOffset64;
            auto mix  = [&hash](u64 value) { hash = (hash ^ value) * kHashPrime64; };

            mix(static_cast<u64>(layout.mAttributes.Size()));
            for (const auto& attr : layout.mAttributes) {
                mix(HashVertexSemanticName(attr.mSemanticName.ToView()));
                mix(static_cast<u64>(attr.mSemanticIndex));
                mix(static_cast<u64>(attr.mFormat));
                mix(static_cast<u64>(attr.mInputSlot));
                mix(static_cast<u64>(attr.mAlignedByteOffset));
                mix(attr.mPerInstance ? 1ULL : 0ULL);
                mix(static_cast<u64>(attr.mInstanceStepRate));
            }
            return hash;
        }
    } // namespace

    auto HashVertexSemanticName(FStringView semanticName) noexcept -> u32 {
        if (semanticName.IsEmpty()) {
            return 0U;
        }

        u32 hash = kFnvOffset32;
        for (usize i = 0U; i < semanticName.Length(); ++i) {
            const auto c     = semanticName[i];
            const auto upper = (c >= TEXT('a') && c <= TEXT('z'))
                ? static_cast<TChar>(c - (TEXT('a') - TEXT('A')))
                : c;
            hash ^= static_cast<u32>(static_cast<FStringView::TUnsigned>(upper));
            hash *= kFnvPrime32;
        }
        return hash;
    }

    auto MakeVertexSemanticKey(FStringView semanticName, u32 semanticIndex) noexcept
        -> FVertexSemanticKey {
        return FVertexSemanticKey{
            .mNameHash      = HashVertexSemanticName(semanticName),
            .mSemanticIndex = semanticIndex,
        };
    }

    auto EncodeVertexSemanticKey(const FVertexSemanticKey& key) noexcept -> u64 {
        return (static_cast<u64>(key.mNameHash) << 32ULL) | static_cast<u64>(key.mSemanticIndex);
    }

    auto BuildShaderVertexInputRequirementFromVertexLayout(
        const Rhi::FRhiVertexLayoutDesc& layoutDesc, FShaderVertexInputRequirement& outRequirement)
        -> bool {
        outRequirement.Reset();
        outRequirement.mElements.Reserve(layoutDesc.mAttributes.Size());

        for (const auto& attr : layoutDesc.mAttributes) {
            FShaderVertexInputElement input{};
            input.mSemantic =
                MakeVertexSemanticKey(attr.mSemanticName.ToView(), attr.mSemanticIndex);
            input.mSemanticName.Assign(attr.mSemanticName.ToView());
            switch (attr.mFormat) {
                case Rhi::ERhiFormat::R32Float:
                    input.mValueType = Shader::EShaderVertexValueType::Float1;
                    break;
                case Rhi::ERhiFormat::R32G32Float:
                    input.mValueType = Shader::EShaderVertexValueType::Float2;
                    break;
                case Rhi::ERhiFormat::R32G32B32Float:
                    input.mValueType = Shader::EShaderVertexValueType::Float3;
                    break;
                case Rhi::ERhiFormat::R16G16B16A16Float:
                case Rhi::ERhiFormat::R8G8B8A8Unorm:
                case Rhi::ERhiFormat::R8G8B8A8UnormSrgb:
                case Rhi::ERhiFormat::B8G8R8A8Unorm:
                case Rhi::ERhiFormat::B8G8R8A8UnormSrgb:
                    input.mValueType = Shader::EShaderVertexValueType::Float4;
                    break;
                default:
                    input.mValueType = Shader::EShaderVertexValueType::Unknown;
                    break;
            }
            if (input.mValueType == Shader::EShaderVertexValueType::Unknown) {
                return false;
            }
            outRequirement.mElements.PushBack(input);
        }
        return true;
    }

    auto BuildShaderVertexInputRequirementFromShaderSet(const RenderCore::FShaderRegistry& registry,
        const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys,
        FShaderVertexInputRequirement& outRequirement, FString* outError) -> bool {
        outRequirement.Reset();

        THashMap<u64, usize> semanticToIndex{};
        for (const auto& shaderKey : shaderKeys) {
            if (!shaderKey.IsValid()) {
                continue;
            }

            const auto shader = registry.FindShader(shaderKey);
            if (!shader) {
                continue;
            }
            if (shader->GetDesc().mStage != Shader::EShaderStage::Vertex) {
                continue;
            }

            for (const auto& input : shader->GetDesc().mReflection.mVertexInputs) {
                const auto semanticName = input.mSemanticName.ToView();
                if (semanticName.IsEmpty() || IsSystemSemantic(semanticName)) {
                    continue;
                }

                const auto semanticKey = MakeVertexSemanticKey(semanticName, input.mSemanticIndex);
                const auto encodedKey  = EncodeVertexSemanticKey(semanticKey);
                const auto valueType   = input.mValueType;
                if (valueType == Shader::EShaderVertexValueType::Unknown) {
                    FString message{};
                    message.Append(
                        TEXT("Shader reflection vertex input has unknown value type: semantic='"));
                    message.Append(semanticName);
                    message.Append(TEXT("' index="));
                    message.Append(FString::ToString(input.mSemanticIndex));
                    message.Append(TEXT(" shader='"));
                    message.Append(shaderKey.Name.ToView());
                    message.Append(TEXT("'."));
                    SetError(outError, message.ToView());
                    return false;
                }
                const auto it = semanticToIndex.find(encodedKey);
                if (it == semanticToIndex.end()) {
                    FShaderVertexInputElement element{};
                    element.mSemantic = semanticKey;
                    element.mSemanticName.Assign(semanticName);
                    element.mValueType          = valueType;
                    semanticToIndex[encodedKey] = outRequirement.mElements.Size();
                    outRequirement.mElements.PushBack(element);
                    continue;
                }

                auto& existing = outRequirement.mElements[it->second];
                if (existing.mValueType != valueType) {
                    SetError(outError,
                        TEXT("Vertex input semantic value type mismatch across shaders."));
                    return false;
                }
            }
        }

        if (outRequirement.mElements.IsEmpty()) {
            SetError(outError, TEXT("No vertex input semantics found from shader reflection."));
            return false;
        }
        return true;
    }

    auto ValidateAndBuildVertexLayout(const FShaderVertexInputRequirement& requirement,
        const FVertexFactoryProvidedLayout& provided, FResolvedVertexLayout& outResolved,
        FString* outError) -> bool {
        outResolved.mVertexLayout.mAttributes.Clear();
        outResolved.mLayoutHash = 0ULL;

        THashMap<u64, usize> providedIndexBySemantic{};
        THashMap<u32, u32>   inputSlotByFactorySlot{};
        u32                  nextInputSlot = 0U;
        for (usize i = 0U; i < provided.mElements.Size(); ++i) {
            const auto key = EncodeVertexSemanticKey(provided.mElements[i].mSemantic);
            if (providedIndexBySemantic.find(key) != providedIndexBySemantic.end()) {
                SetError(outError, TEXT("VertexFactory layout has duplicated semantic key."));
                return false;
            }
            providedIndexBySemantic[key] = i;
            const u32 slotKey            = static_cast<u32>(provided.mElements[i].mSlot);
            if (inputSlotByFactorySlot.find(slotKey) == inputSlotByFactorySlot.end()) {
                inputSlotByFactorySlot[slotKey] = nextInputSlot++;
            }
        }

        outResolved.mVertexLayout.mAttributes.Reserve(requirement.mElements.Size());
        for (const auto& required : requirement.mElements) {
            const auto key = EncodeVertexSemanticKey(required.mSemantic);
            const auto it  = providedIndexBySemantic.find(key);
            if (it == providedIndexBySemantic.end()) {
                SetError(outError, TEXT("VertexFactory layout misses a required semantic."));
                return false;
            }

            const auto& source = provided.mElements[it->second];
            if (!IsVertexFormatCompatible(required.mValueType, source.mFormat)) {
                SetError(outError,
                    TEXT("VertexFactory semantic format is incompatible with shader value type."));
                return false;
            }
            if (source.mFormat == Rhi::ERhiFormat::Unknown) {
                SetError(outError, TEXT("VertexFactory semantic format is unknown."));
                return false;
            }

            Rhi::FRhiVertexAttributeDesc outAttr{};
            if (!source.mSemanticName.IsEmptyString()) {
                outAttr.mSemanticName.Assign(source.mSemanticName.ToView());
            } else if (!required.mSemanticName.IsEmptyString()) {
                outAttr.mSemanticName.Assign(required.mSemanticName.ToView());
            } else {
                outAttr.mSemanticName.Assign(TEXT("AUTO"));
            }
            outAttr.mSemanticIndex = source.mSemantic.mSemanticIndex;
            outAttr.mFormat        = source.mFormat;
            const auto slotIt      = inputSlotByFactorySlot.find(static_cast<u32>(source.mSlot));
            if (slotIt == inputSlotByFactorySlot.end()) {
                SetError(outError, TEXT("VertexFactory slot map resolve failed."));
                return false;
            }
            outAttr.mInputSlot         = slotIt->second;
            outAttr.mAlignedByteOffset = source.mAlignedByteOffset;
            outAttr.mPerInstance       = source.mPerInstance;
            outAttr.mInstanceStepRate  = source.mInstanceStepRate;
            outResolved.mVertexLayout.mAttributes.PushBack(outAttr);
        }

        outResolved.mLayoutHash = BuildVertexLayoutHash(outResolved.mVertexLayout);
        return true;
    }
} // namespace AltinaEngine::RenderCore::Geometry

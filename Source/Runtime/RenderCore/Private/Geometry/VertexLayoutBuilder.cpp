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

        auto IsSystemSemantic(FStringView semanticName) -> bool {
            if (semanticName.Length() < 3U) {
                return false;
            }
            const auto s0 = semanticName[0];
            const auto s1 = semanticName[1];
            return ((s0 == TEXT('S') || s0 == TEXT('s')) && (s1 == TEXT('V') || s1 == TEXT('v'))
                && semanticName[2] == TEXT('_'));
        }

        auto ToRhiFormat(Shader::EShaderVertexValueType valueType) -> Rhi::ERhiFormat {
            switch (valueType) {
                case Shader::EShaderVertexValueType::Float1:
                    return Rhi::ERhiFormat::R32Float;
                case Shader::EShaderVertexValueType::Float2:
                    return Rhi::ERhiFormat::R32G32Float;
                case Shader::EShaderVertexValueType::Float3:
                    return Rhi::ERhiFormat::R32G32B32Float;
                case Shader::EShaderVertexValueType::Float4:
                case Shader::EShaderVertexValueType::Unknown:
                default:
                    return Rhi::ERhiFormat::Unknown;
            }
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
            input.mFormat = attr.mFormat;
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
                const auto format      = ToRhiFormat(input.mValueType);
                const auto it          = semanticToIndex.find(encodedKey);
                if (it == semanticToIndex.end()) {
                    FShaderVertexInputElement element{};
                    element.mSemantic = semanticKey;
                    element.mSemanticName.Assign(semanticName);
                    element.mFormat             = format;
                    semanticToIndex[encodedKey] = outRequirement.mElements.Size();
                    outRequirement.mElements.PushBack(element);
                    continue;
                }

                auto& existing = outRequirement.mElements[it->second];
                if (existing.mFormat == Rhi::ERhiFormat::Unknown) {
                    existing.mFormat = format;
                    continue;
                }
                if (format == Rhi::ERhiFormat::Unknown) {
                    continue;
                }
                if (existing.mFormat != format) {
                    SetError(
                        outError, TEXT("Vertex input semantic format mismatch across shaders."));
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
        for (usize i = 0U; i < provided.mElements.Size(); ++i) {
            const auto key = EncodeVertexSemanticKey(provided.mElements[i].mSemantic);
            if (providedIndexBySemantic.find(key) != providedIndexBySemantic.end()) {
                SetError(outError, TEXT("VertexFactory layout has duplicated semantic key."));
                return false;
            }
            providedIndexBySemantic[key] = i;
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
            if (required.mFormat != Rhi::ERhiFormat::Unknown
                && source.mFormat != required.mFormat) {
                SetError(outError, TEXT("VertexFactory semantic format mismatch."));
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
            outAttr.mSemanticIndex     = source.mSemantic.mSemanticIndex;
            outAttr.mFormat            = source.mFormat;
            outAttr.mInputSlot         = source.mInputSlot;
            outAttr.mAlignedByteOffset = source.mAlignedByteOffset;
            outAttr.mPerInstance       = source.mPerInstance;
            outAttr.mInstanceStepRate  = source.mInstanceStepRate;
            outResolved.mVertexLayout.mAttributes.PushBack(outAttr);
        }

        outResolved.mLayoutHash = BuildVertexLayoutHash(outResolved.mVertexLayout);
        return true;
    }
} // namespace AltinaEngine::RenderCore::Geometry

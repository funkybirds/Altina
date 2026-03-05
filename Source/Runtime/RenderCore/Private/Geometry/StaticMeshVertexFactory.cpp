#include "Geometry/StaticMeshVertexFactory.h"

namespace AltinaEngine::RenderCore::Geometry {
    auto BuildStaticMeshProvidedLayout(FVertexFactoryProvidedLayout& outLayout) -> bool {
        outLayout.Reset();
        outLayout.mElements.Reserve(3U);

        FVertexFactoryInputElement position{};
        position.mSemantic = MakeVertexSemanticKey(TEXT("POSITION"), 0U);
        position.mSemanticName.Assign(TEXT("POSITION"));
        position.mFormat            = Rhi::ERhiFormat::R32G32B32Float;
        position.mSlot              = EVertexFactorySlot::Position;
        position.mAlignedByteOffset = 0U;
        position.mPerInstance       = false;
        position.mInstanceStepRate  = 0U;
        outLayout.mElements.PushBack(position);

        FVertexFactoryInputElement normal{};
        normal.mSemantic = MakeVertexSemanticKey(TEXT("NORMAL"), 0U);
        normal.mSemanticName.Assign(TEXT("NORMAL"));
        normal.mFormat            = Rhi::ERhiFormat::R32G32B32Float;
        normal.mSlot              = EVertexFactorySlot::Normal;
        normal.mAlignedByteOffset = 0U;
        normal.mPerInstance       = false;
        normal.mInstanceStepRate  = 0U;
        outLayout.mElements.PushBack(normal);

        FVertexFactoryInputElement texcoord{};
        texcoord.mSemantic = MakeVertexSemanticKey(TEXT("TEXCOORD"), 0U);
        texcoord.mSemanticName.Assign(TEXT("TEXCOORD"));
        texcoord.mFormat            = Rhi::ERhiFormat::R32G32Float;
        texcoord.mSlot              = EVertexFactorySlot::UV0;
        texcoord.mAlignedByteOffset = 0U;
        texcoord.mPerInstance       = false;
        texcoord.mInstanceStepRate  = 0U;
        outLayout.mElements.PushBack(texcoord);
        return true;
    }

    auto BuildStaticMeshLegacyVertexLayout(Rhi::FRhiVertexLayoutDesc& outLayout) -> bool {
        outLayout.mAttributes.Clear();

        FVertexFactoryProvidedLayout provided{};
        if (!BuildStaticMeshProvidedLayout(provided)) {
            return false;
        }

        auto mapSlotToInputSlot = [](EVertexFactorySlot slot, u32& outInputSlot) -> bool {
            switch (slot) {
                case EVertexFactorySlot::Position:
                    outInputSlot = 0U;
                    return true;
                case EVertexFactorySlot::Normal:
                    outInputSlot = 1U;
                    return true;
                case EVertexFactorySlot::UV0:
                    outInputSlot = 2U;
                    return true;
                case EVertexFactorySlot::UV1:
                    outInputSlot = 3U;
                    return true;
                default:
                    return false;
            }
        };

        outLayout.mAttributes.Reserve(provided.mElements.Size());
        for (const auto& element : provided.mElements) {
            Rhi::FRhiVertexAttributeDesc attr{};
            if (element.mSemantic == MakeVertexSemanticKey(TEXT("POSITION"), 0U)) {
                attr.mSemanticName.Assign(TEXT("POSITION"));
            } else if (element.mSemantic == MakeVertexSemanticKey(TEXT("NORMAL"), 0U)) {
                attr.mSemanticName.Assign(TEXT("NORMAL"));
            } else if (element.mSemantic == MakeVertexSemanticKey(TEXT("TEXCOORD"), 0U)) {
                attr.mSemanticName.Assign(TEXT("TEXCOORD"));
            } else {
                return false;
            }
            attr.mSemanticIndex = element.mSemantic.mSemanticIndex;
            attr.mFormat        = element.mFormat;
            u32 inputSlot       = 0U;
            if (!mapSlotToInputSlot(element.mSlot, inputSlot)) {
                return false;
            }
            attr.mInputSlot         = inputSlot;
            attr.mAlignedByteOffset = element.mAlignedByteOffset;
            attr.mPerInstance       = element.mPerInstance;
            attr.mInstanceStepRate  = element.mInstanceStepRate;
            outLayout.mAttributes.PushBack(attr);
        }
        return true;
    }
} // namespace AltinaEngine::RenderCore::Geometry

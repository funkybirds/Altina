#include "TestHarness.h"

#include "Geometry/VertexLayoutBuilder.h"
#include "Rhi/RhiDevice.h"
#include "RhiMock/RhiMockContext.h"
#include "Shader/ShaderRegistry.h"

namespace {
    using AltinaEngine::RenderCore::FShaderRegistry;
    using AltinaEngine::RenderCore::Geometry::BuildShaderVertexInputRequirementFromShaderSet;
    using AltinaEngine::RenderCore::Geometry::BuildShaderVertexInputRequirementFromVertexLayout;
    using AltinaEngine::RenderCore::Geometry::EVertexFactorySlot;
    using AltinaEngine::RenderCore::Geometry::FResolvedVertexLayout;
    using AltinaEngine::RenderCore::Geometry::FShaderVertexInputRequirement;
    using AltinaEngine::RenderCore::Geometry::FVertexFactoryInputElement;
    using AltinaEngine::RenderCore::Geometry::FVertexFactoryProvidedLayout;
    using AltinaEngine::RenderCore::Geometry::MakeVertexSemanticKey;
    using AltinaEngine::RenderCore::Geometry::ValidateAndBuildVertexLayout;
    using AltinaEngine::Rhi::ERhiAdapterType;
    using AltinaEngine::Rhi::ERhiFormat;
    using AltinaEngine::Rhi::ERhiGpuPreference;
    using AltinaEngine::Rhi::ERhiVendorId;
    using AltinaEngine::Rhi::FRhiAdapterDesc;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiMockContext;
    using AltinaEngine::Rhi::FRhiShaderDesc;
    using AltinaEngine::Rhi::FRhiVertexLayoutDesc;
    using AltinaEngine::Shader::EShaderStage;
    using AltinaEngine::Shader::EShaderVertexValueType;

    auto MakeAdapterDesc(const AltinaEngine::TChar* name, ERhiAdapterType type, ERhiVendorId vendor)
        -> FRhiAdapterDesc {
        FRhiAdapterDesc desc{};
        desc.mName.Assign(name);
        desc.mType     = type;
        desc.mVendorId = vendor;
        return desc;
    }

    auto CreateMockDevice(FRhiMockContext& context) {
        context.AddAdapter(MakeAdapterDesc(
            TEXT("Mock Discrete"), ERhiAdapterType::Discrete, ERhiVendorId::Nvidia));
        FRhiInitDesc initDesc{};
        initDesc.mAdapterPreference = ERhiGpuPreference::HighPerformance;
        REQUIRE(context.Init(initDesc));
        auto device = context.CreateDevice(0U);
        REQUIRE(device);
        return device;
    }
} // namespace

TEST_CASE("RenderCore.VertexLayoutBuilder.ValidateAndBuild.Success") {
    auto makeAttr = [](const AltinaEngine::TChar* semantic, AltinaEngine::u32 index,
                        ERhiFormat format) {
        AltinaEngine::Rhi::FRhiVertexAttributeDesc attr{};
        attr.mSemanticName.Assign(semantic);
        attr.mSemanticIndex = index;
        attr.mFormat        = format;
        return attr;
    };

    FRhiVertexLayoutDesc shaderLayout{};
    shaderLayout.mAttributes.PushBack(makeAttr(TEXT("POSITION"), 0U, ERhiFormat::R32G32B32Float));
    shaderLayout.mAttributes.PushBack(makeAttr(TEXT("NORMAL"), 0U, ERhiFormat::R32G32B32Float));
    shaderLayout.mAttributes.PushBack(makeAttr(TEXT("TEXCOORD"), 0U, ERhiFormat::R32G32Float));

    FShaderVertexInputRequirement requirement{};
    REQUIRE(BuildShaderVertexInputRequirementFromVertexLayout(shaderLayout, requirement));

    FVertexFactoryProvidedLayout provided{};
    provided.mElements.PushBack(FVertexFactoryInputElement{
        .mSemantic          = MakeVertexSemanticKey(TEXT("POSITION"), 0U),
        .mFormat            = ERhiFormat::R32G32B32Float,
        .mSlot              = EVertexFactorySlot::Position,
        .mAlignedByteOffset = 0U,
    });
    provided.mElements.PushBack(FVertexFactoryInputElement{
        .mSemantic          = MakeVertexSemanticKey(TEXT("NORMAL"), 0U),
        .mFormat            = ERhiFormat::R32G32B32Float,
        .mSlot              = EVertexFactorySlot::Normal,
        .mAlignedByteOffset = 12U,
    });
    provided.mElements.PushBack(FVertexFactoryInputElement{
        .mSemantic          = MakeVertexSemanticKey(TEXT("TEXCOORD"), 0U),
        .mFormat            = ERhiFormat::R32G32Float,
        .mSlot              = EVertexFactorySlot::UV0,
        .mAlignedByteOffset = 24U,
    });

    FResolvedVertexLayout resolved{};
    REQUIRE(ValidateAndBuildVertexLayout(requirement, provided, resolved, nullptr));
    REQUIRE_EQ(resolved.mVertexLayout.mAttributes.Size(), 3U);
    REQUIRE(resolved.mLayoutHash != 0ULL);
}

TEST_CASE("RenderCore.VertexLayoutBuilder.ValidateAndBuild.MissingSemantic") {
    FShaderVertexInputRequirement requirement{};
    requirement.mElements.PushBack({
        .mSemantic  = MakeVertexSemanticKey(TEXT("POSITION"), 0U),
        .mValueType = EShaderVertexValueType::Float3,
    });
    requirement.mElements.PushBack({
        .mSemantic  = MakeVertexSemanticKey(TEXT("NORMAL"), 0U),
        .mValueType = EShaderVertexValueType::Float3,
    });

    FVertexFactoryProvidedLayout provided{};
    provided.mElements.PushBack({
        .mSemantic          = MakeVertexSemanticKey(TEXT("POSITION"), 0U),
        .mFormat            = ERhiFormat::R32G32B32Float,
        .mSlot              = EVertexFactorySlot::Position,
        .mAlignedByteOffset = 0U,
    });

    FResolvedVertexLayout resolved{};
    REQUIRE(!ValidateAndBuildVertexLayout(requirement, provided, resolved, nullptr));
}

TEST_CASE("RenderCore.VertexLayoutBuilder.ValidateAndBuild.FormatMismatch") {
    FShaderVertexInputRequirement requirement{};
    requirement.mElements.PushBack({
        .mSemantic  = MakeVertexSemanticKey(TEXT("TEXCOORD"), 0U),
        .mValueType = EShaderVertexValueType::Float2,
    });

    FVertexFactoryProvidedLayout provided{};
    provided.mElements.PushBack({
        .mSemantic          = MakeVertexSemanticKey(TEXT("TEXCOORD"), 0U),
        .mFormat            = ERhiFormat::R32G32B32Float,
        .mSlot              = EVertexFactorySlot::UV0,
        .mAlignedByteOffset = 0U,
    });

    FResolvedVertexLayout resolved{};
    REQUIRE(!ValidateAndBuildVertexLayout(requirement, provided, resolved, nullptr));
}

TEST_CASE("RenderCore.VertexLayoutBuilder.ValidateAndBuild.DuplicateProvidedSemantic") {
    FShaderVertexInputRequirement requirement{};
    requirement.mElements.PushBack({
        .mSemantic  = MakeVertexSemanticKey(TEXT("POSITION"), 0U),
        .mValueType = EShaderVertexValueType::Float3,
    });

    FVertexFactoryProvidedLayout provided{};
    provided.mElements.PushBack({
        .mSemantic          = MakeVertexSemanticKey(TEXT("POSITION"), 0U),
        .mFormat            = ERhiFormat::R32G32B32Float,
        .mSlot              = EVertexFactorySlot::Position,
        .mAlignedByteOffset = 0U,
    });
    provided.mElements.PushBack({
        .mSemantic          = MakeVertexSemanticKey(TEXT("POSITION"), 0U),
        .mFormat            = ERhiFormat::R32G32B32Float,
        .mSlot              = EVertexFactorySlot::Normal,
        .mAlignedByteOffset = 0U,
    });

    FResolvedVertexLayout resolved{};
    REQUIRE(!ValidateAndBuildVertexLayout(requirement, provided, resolved, nullptr));
}

TEST_CASE("RenderCore.VertexLayoutBuilder.BuildRequirementFromShaderSet") {
    FRhiMockContext context;
    auto            device = CreateMockDevice(context);

    FShaderRegistry registry{};

    FRhiShaderDesc  vsDesc{};
    vsDesc.mDebugName.Assign(TEXT("VertexLayoutBuilder.Test.VS"));
    vsDesc.mStage = EShaderStage::Vertex;
    {
        AltinaEngine::Shader::FShaderVertexInput input{};
        input.mSemanticName.Assign(TEXT("POSITION"));
        input.mSemanticIndex = 0U;
        input.mValueType     = EShaderVertexValueType::Float3;
        vsDesc.mReflection.mVertexInputs.PushBack(input);
    }
    {
        AltinaEngine::Shader::FShaderVertexInput input{};
        input.mSemanticName.Assign(TEXT("NORMAL"));
        input.mSemanticIndex = 0U;
        input.mValueType     = EShaderVertexValueType::Float3;
        vsDesc.mReflection.mVertexInputs.PushBack(input);
    }
    {
        AltinaEngine::Shader::FShaderVertexInput input{};
        input.mSemanticName.Assign(TEXT("TEXCOORD"));
        input.mSemanticIndex = 0U;
        input.mValueType     = EShaderVertexValueType::Float2;
        vsDesc.mReflection.mVertexInputs.PushBack(input);
    }
    {
        AltinaEngine::Shader::FShaderVertexInput input{};
        input.mSemanticName.Assign(TEXT("SV_VertexID"));
        input.mSemanticIndex = 0U;
        input.mValueType     = EShaderVertexValueType::Float1;
        vsDesc.mReflection.mVertexInputs.PushBack(input);
    }

    AltinaEngine::Shader::FShaderPermutationId permutation{};
    permutation.mHash = 123ULL;
    const auto vsKey  = FShaderRegistry::MakeKey(
        TEXT("VertexLayoutBuilder.Test"), EShaderStage::Vertex, permutation);
    auto vs = device->CreateShader(vsDesc);
    REQUIRE(vs);
    REQUIRE(registry.RegisterShader(vsKey, vs));

    AltinaEngine::Core::Container::TVector<FShaderRegistry::FShaderKey> shaderKeys{};
    shaderKeys.PushBack(vsKey);

    FShaderVertexInputRequirement requirement{};
    REQUIRE(
        BuildShaderVertexInputRequirementFromShaderSet(registry, shaderKeys, requirement, nullptr));
    REQUIRE_EQ(requirement.mElements.Size(), 3U);
}

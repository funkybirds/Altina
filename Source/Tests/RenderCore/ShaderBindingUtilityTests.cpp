#include "TestHarness.h"

#include "Shader/ShaderBindingUtility.h"

#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiStructs.h"
#include "RhiMock/RhiMockContext.h"
#include "Shader/ShaderRegistry.h"

namespace {
    using AltinaEngine::TChar;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::u8;
    using AltinaEngine::RenderCore::FShaderRegistry;
    using AltinaEngine::RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaderSet;
    using AltinaEngine::RenderCore::ShaderBinding::BuildBindingLookupTable;
    using AltinaEngine::RenderCore::ShaderBinding::FBindGroupBuilder;
    using AltinaEngine::RenderCore::ShaderBinding::FBindingLookupTable;
    using AltinaEngine::RenderCore::ShaderBinding::FindBindingByNameHash;
    using AltinaEngine::RenderCore::ShaderBinding::HashBindingName;
    using AltinaEngine::RenderCore::ShaderBinding::OrShaderStageFlags;
    using AltinaEngine::RenderCore::ShaderBinding::ResolveConstantBufferBindingByName;
    using AltinaEngine::RenderCore::ShaderBinding::ResolveResourceBindingByName;
    using AltinaEngine::RenderCore::ShaderBinding::ToRhiBindingType;
    using AltinaEngine::RenderCore::ShaderBinding::ToRhiShaderStageFlags;
    using AltinaEngine::Rhi::ERhiAdapterType;
    using AltinaEngine::Rhi::ERhiBindingType;
    using AltinaEngine::Rhi::ERhiGpuPreference;
    using AltinaEngine::Rhi::ERhiShaderStageFlags;
    using AltinaEngine::Rhi::ERhiVendorId;
    using AltinaEngine::Rhi::FRhiAdapterDesc;
    using AltinaEngine::Rhi::FRhiBindGroupDesc;
    using AltinaEngine::Rhi::FRhiBindGroupLayout;
    using AltinaEngine::Rhi::FRhiBindGroupLayoutDesc;
    using AltinaEngine::Rhi::FRhiBuffer;
    using AltinaEngine::Rhi::FRhiDevice;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiMockContext;
    using AltinaEngine::Rhi::FRhiSampler;
    using AltinaEngine::Rhi::FRhiShaderDesc;
    using AltinaEngine::Rhi::FRhiTexture;
    using AltinaEngine::Shader::EShaderResourceAccess;
    using AltinaEngine::Shader::EShaderResourceType;
    using AltinaEngine::Shader::EShaderStage;
    using AltinaEngine::Shader::FShaderConstantBuffer;
    using AltinaEngine::Shader::FShaderPermutationId;
    using AltinaEngine::Shader::FShaderResourceBinding;

    auto MakeAdapterDesc(const TChar* name, ERhiAdapterType type, ERhiVendorId vendor)
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

    void AddCBufferReflection(
        FRhiShaderDesc& desc, const TChar* name, u32 setIndex, u32 bindingIndex) {
        FShaderConstantBuffer cbuffer{};
        cbuffer.mName.Assign(name);
        cbuffer.mSet     = setIndex;
        cbuffer.mBinding = bindingIndex;
        desc.mReflection.mConstantBuffers.PushBack(cbuffer);

        // Runtime compiler reflection reports cbuffers in both constant buffer and
        // generic resource lists.
        FShaderResourceBinding resource{};
        resource.mName.Assign(name);
        resource.mType    = EShaderResourceType::ConstantBuffer;
        resource.mAccess  = EShaderResourceAccess::ReadOnly;
        resource.mSet     = setIndex;
        resource.mBinding = bindingIndex;
        desc.mReflection.mResources.PushBack(resource);
    }

    void AddResourceReflection(FRhiShaderDesc& desc, const TChar* name, EShaderResourceType type,
        EShaderResourceAccess access, u32 setIndex, u32 bindingIndex) {
        FShaderResourceBinding resource{};
        resource.mName.Assign(name);
        resource.mType    = type;
        resource.mAccess  = access;
        resource.mSet     = setIndex;
        resource.mBinding = bindingIndex;
        desc.mReflection.mResources.PushBack(resource);
    }

    auto RegisterShader(FShaderRegistry& registry, FRhiDevice& device, const TChar* shaderName,
        EShaderStage stage, const FRhiShaderDesc& desc, u64 hashSeed)
        -> FShaderRegistry::FShaderKey {
        FShaderPermutationId permutation{};
        permutation.mHash = hashSeed;
        const auto key    = FShaderRegistry::MakeKey(shaderName, stage, permutation);
        auto       shader = device.CreateShader(desc);
        REQUIRE(shader);
        REQUIRE(registry.RegisterShader(key, shader));
        return key;
    }
} // namespace

TEST_CASE("RenderCore.ShaderBindingUtility.StageAndTypeMapping") {
    REQUIRE_EQ(ToRhiShaderStageFlags(EShaderStage::Vertex), ERhiShaderStageFlags::Vertex);
    REQUIRE_EQ(ToRhiShaderStageFlags(EShaderStage::Pixel), ERhiShaderStageFlags::Pixel);

    const auto merged =
        OrShaderStageFlags(ERhiShaderStageFlags::Vertex, ERhiShaderStageFlags::Pixel);
    REQUIRE(static_cast<u8>(merged)
        == (static_cast<u8>(ERhiShaderStageFlags::Vertex)
            | static_cast<u8>(ERhiShaderStageFlags::Pixel)));

    FShaderResourceBinding sampledTexture{};
    sampledTexture.mType = EShaderResourceType::Texture;
    REQUIRE_EQ(ToRhiBindingType(sampledTexture), ERhiBindingType::SampledTexture);

    FShaderResourceBinding storageBufferRw{};
    storageBufferRw.mType   = EShaderResourceType::StorageBuffer;
    storageBufferRw.mAccess = EShaderResourceAccess::ReadWrite;
    REQUIRE_EQ(ToRhiBindingType(storageBufferRw), ERhiBindingType::StorageBuffer);
}

TEST_CASE("RenderCore.ShaderBindingUtility.ReflectionBuildAndResolve") {
    FRhiMockContext context;
    auto            device = CreateMockDevice(context);

    FShaderRegistry registry;

    FRhiShaderDesc  vsDesc{};
    vsDesc.mDebugName.Assign(TEXT("BindUtil.Test.VS"));
    vsDesc.mStage = EShaderStage::Vertex;
    AddCBufferReflection(vsDesc, TEXT("PerFrame"), 0U, 0U);

    FRhiShaderDesc psDesc{};
    psDesc.mDebugName.Assign(TEXT("BindUtil.Test.PS"));
    psDesc.mStage = EShaderStage::Pixel;
    AddCBufferReflection(psDesc, TEXT("PerFrame"), 0U, 0U);
    AddResourceReflection(psDesc, TEXT("SceneColor"), EShaderResourceType::Texture,
        EShaderResourceAccess::ReadOnly, 0U, 3U);
    AddResourceReflection(psDesc, TEXT("LinearSampler"), EShaderResourceType::Sampler,
        EShaderResourceAccess::ReadOnly, 0U, 4U);

    const auto vsKey = RegisterShader(
        registry, *device, TEXT("BindUtil.Test"), EShaderStage::Vertex, vsDesc, 1ULL);
    const auto psKey =
        RegisterShader(registry, *device, TEXT("BindUtil.Test"), EShaderStage::Pixel, psDesc, 2ULL);

    AltinaEngine::Core::Container::TVector<FShaderRegistry::FShaderKey> shaderKeys{};
    shaderKeys.PushBack(vsKey);
    shaderKeys.PushBack(psKey);

    FRhiBindGroupLayoutDesc layoutDesc{};
    REQUIRE(BuildBindGroupLayoutFromShaderSet(registry, shaderKeys, 0U, layoutDesc));
    REQUIRE_EQ(layoutDesc.mEntries.Size(), 3U);

    u32                  outSet       = 0U;
    u32                  outBinding   = 0U;
    ERhiShaderStageFlags outStageMask = ERhiShaderStageFlags::None;
    REQUIRE(ResolveConstantBufferBindingByName(
        registry, shaderKeys, TEXT("PerFrame"), outSet, outBinding, outStageMask));
    REQUIRE_EQ(outSet, 0U);
    REQUIRE_EQ(outBinding, 0U);
    const auto expectedCbVisibility = static_cast<u8>(ERhiShaderStageFlags::Vertex)
        | static_cast<u8>(ERhiShaderStageFlags::Pixel);
    REQUIRE(static_cast<u8>(outStageMask) == expectedCbVisibility);

    REQUIRE(ResolveResourceBindingByName(registry, shaderKeys, TEXT("SceneColor"),
        ERhiBindingType::SampledTexture, outSet, outBinding, outStageMask));
    REQUIRE_EQ(outSet, 0U);
    REQUIRE_EQ(outBinding, 3U);
    REQUIRE_EQ(outStageMask, ERhiShaderStageFlags::Pixel);

    REQUIRE(!ResolveResourceBindingByName(registry, shaderKeys, TEXT("SceneColor"),
        ERhiBindingType::Sampler, outSet, outBinding, outStageMask));

    FRhiBindGroupLayout layout(layoutDesc);
    FBindingLookupTable table{};
    REQUIRE(BuildBindingLookupTable(registry, shaderKeys, 0U, &layout, table));

    u32 sceneColorBinding = 0U;
    REQUIRE(FindBindingByNameHash(table, HashBindingName(TEXT("SceneColor")),
        ERhiBindingType::SampledTexture, sceneColorBinding));
    REQUIRE_EQ(sceneColorBinding, 3U);
}

TEST_CASE("RenderCore.ShaderBindingUtility.BindGroupBuilderValidatesEntries") {
    FRhiBindGroupLayoutDesc layoutDesc{};
    layoutDesc.mSetIndex = 0U;
    layoutDesc.mEntries.PushBack({ .mBinding = 0U,
        .mType                               = ERhiBindingType::ConstantBuffer,
        .mVisibility                         = ERhiShaderStageFlags::Vertex });
    layoutDesc.mEntries.PushBack({ .mBinding = 1U,
        .mType                               = ERhiBindingType::SampledTexture,
        .mVisibility                         = ERhiShaderStageFlags::Pixel });
    layoutDesc.mEntries.PushBack({ .mBinding = 2U,
        .mType                               = ERhiBindingType::Sampler,
        .mVisibility                         = ERhiShaderStageFlags::Pixel });

    FRhiBindGroupLayout layout(layoutDesc);

    FBindGroupBuilder   builder(&layout);
    FRhiBindGroupDesc   outDesc{};

    REQUIRE(builder.AddBuffer(0U, reinterpret_cast<FRhiBuffer*>(1ULL), 0ULL, 256ULL));
    REQUIRE(builder.AddTexture(1U, reinterpret_cast<FRhiTexture*>(2ULL)));
    REQUIRE(!builder.AddTexture(1U, reinterpret_cast<FRhiTexture*>(3ULL)));
    REQUIRE(!builder.Build(outDesc));

    REQUIRE(builder.AddSampler(2U, reinterpret_cast<FRhiSampler*>(4ULL)));
    REQUIRE(builder.Build(outDesc));
    REQUIRE_EQ(outDesc.mEntries.Size(), 3U);
}

#include "TestHarness.h"

#include "Shader/ShaderRegistry.h"
#include "RhiMock/RhiMockContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiStructs.h"

namespace {
    using AltinaEngine::TChar;
    using AltinaEngine::u64;
    using AltinaEngine::RenderCore::FShaderRegistry;
    using AltinaEngine::Rhi::ERhiAdapterType;
    using AltinaEngine::Rhi::ERhiGpuPreference;
    using AltinaEngine::Rhi::ERhiVendorId;
    using AltinaEngine::Rhi::FRhiAdapterDesc;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiMockContext;
    using AltinaEngine::Rhi::FRhiShaderDesc;
    using AltinaEngine::Shader::EShaderStage;
    using AltinaEngine::Shader::FShaderPermutationId;

    auto MakeAdapterDesc(const TChar* name, ERhiAdapterType type, ERhiVendorId vendor)
        -> FRhiAdapterDesc {
        FRhiAdapterDesc desc;
        desc.mName.Assign(name);
        desc.mType     = type;
        desc.mVendorId = vendor;
        return desc;
    }

    auto CreateMockDevice(FRhiMockContext& context) {
        context.AddAdapter(MakeAdapterDesc(
            TEXT("Mock Discrete"), ERhiAdapterType::Discrete, ERhiVendorId::Nvidia));
        FRhiInitDesc initDesc;
        initDesc.mAdapterPreference = ERhiGpuPreference::HighPerformance;
        REQUIRE(context.Init(initDesc));
        auto device = context.CreateDevice(0);
        REQUIRE(device);
        return device;
    }

    auto CreateMockShader(AltinaEngine::Rhi::FRhiDevice& device, const TChar* debugName,
        EShaderStage stage) {
        FRhiShaderDesc desc;
        desc.mDebugName.Assign(debugName);
        desc.mStage = stage;
        return device.CreateShader(desc);
    }
} // namespace

TEST_CASE("RenderCore.ShaderRegistry.BasicOps") {
    FRhiMockContext context;
    auto            device = CreateMockDevice(context);

    FShaderRegistry registry;

    FShaderPermutationId permutation{};
    permutation.mHash = 42ULL;
    const auto key = FShaderRegistry::MakeKey(TEXT("TestShader"), EShaderStage::Vertex,
        permutation);

    auto shader = CreateMockShader(*device, TEXT("TestShader.VS"), EShaderStage::Vertex);
    REQUIRE(shader);

    REQUIRE_EQ(registry.GetEntryCount(), 0U);
    REQUIRE(!registry.Contains(key));
    REQUIRE(!registry.FindShader(key));

    REQUIRE(registry.RegisterShader(key, shader));
    REQUIRE_EQ(registry.GetEntryCount(), 1U);
    REQUIRE(registry.Contains(key));
    REQUIRE(registry.FindShader(key).Get() == shader.Get());

    REQUIRE(registry.RemoveShader(key));
    REQUIRE_EQ(registry.GetEntryCount(), 0U);
    REQUIRE(!registry.Contains(key));
    REQUIRE(!registry.FindShader(key));

    FShaderRegistry::FShaderKey invalidKey{};
    REQUIRE(!registry.RegisterShader(invalidKey, shader));
    REQUIRE_EQ(registry.GetEntryCount(), 0U);
}

TEST_CASE("RenderCore.ShaderRegistry.Overwrite") {
    FRhiMockContext context;
    auto            device = CreateMockDevice(context);

    FShaderRegistry registry;

    FShaderPermutationId permutation{};
    permutation.mHash = 7ULL;
    const auto key = FShaderRegistry::MakeKey(TEXT("TestShader"), EShaderStage::Pixel,
        permutation);

    auto shaderA = CreateMockShader(*device, TEXT("TestShader.PS.A"), EShaderStage::Pixel);
    auto shaderB = CreateMockShader(*device, TEXT("TestShader.PS.B"), EShaderStage::Pixel);
    REQUIRE(shaderA);
    REQUIRE(shaderB);

    REQUIRE(registry.RegisterShader(key, shaderA));
    REQUIRE_EQ(registry.GetEntryCount(), 1U);
    REQUIRE(registry.FindShader(key).Get() == shaderA.Get());

    REQUIRE(registry.RegisterShader(key, shaderB));
    REQUIRE_EQ(registry.GetEntryCount(), 1U);
    REQUIRE(registry.FindShader(key).Get() == shaderB.Get());
}

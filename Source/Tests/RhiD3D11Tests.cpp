#include "TestHarness.h"

#include "RhiD3D11/RhiD3D11Context.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiTexture.h"
#include "Types/Traits.h"

TEST_CASE("RhiD3D11.DeviceCreation") {
#if AE_PLATFORM_WIN
    using AltinaEngine::Rhi::ERhiBufferBindFlags;
    using AltinaEngine::Rhi::FRhiBufferDesc;
    using AltinaEngine::Rhi::FRhiD3D11Context;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiSamplerDesc;
    using AltinaEngine::Rhi::FRhiTextureDesc;
    using AltinaEngine::Rhi::kRhiInvalidAdapterIndex;

    FRhiD3D11Context context;
    FRhiInitDesc     initDesc;
    initDesc.mEnableDebugLayer = false;

    REQUIRE(context.Init(initDesc));

    const auto adapters = context.EnumerateAdapters();
    if (adapters.IsEmpty()) {
        return;
    }

    const auto device = context.CreateDevice(kRhiInvalidAdapterIndex);
    REQUIRE(device);
    REQUIRE(device->GetAdapterDesc().IsValid());

    FRhiBufferDesc bufferDesc;
    bufferDesc.mSizeBytes = 256;
    bufferDesc.mBindFlags = ERhiBufferBindFlags::Vertex;
    const auto buffer = device->CreateBuffer(bufferDesc);
    REQUIRE(buffer);

    FRhiTextureDesc textureDesc;
    textureDesc.mWidth  = 4;
    textureDesc.mHeight = 4;
    const auto texture = device->CreateTexture(textureDesc);
    REQUIRE(texture);

    FRhiSamplerDesc samplerDesc;
    const auto sampler = device->CreateSampler(samplerDesc);
    REQUIRE(sampler);
#else
    // Non-Windows platforms do not support D3D11.
#endif
}

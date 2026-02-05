#include "TestHarness.h"

#include "RhiMock/RhiMockContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiBuffer.h"

namespace {
    using AltinaEngine::TChar;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::Rhi::ERhiAdapterType;
    using AltinaEngine::Rhi::ERhiGpuPreference;
    using AltinaEngine::Rhi::ERhiVendorId;
    using AltinaEngine::Rhi::FRhiAdapterDesc;
    using AltinaEngine::Rhi::FRhiBufferDesc;
    using AltinaEngine::Rhi::FRhiBufferRef;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiMockContext;
    using AltinaEngine::Rhi::kRhiInvalidAdapterIndex;

    auto MakeAdapterDesc(const TChar* name, ERhiAdapterType type, ERhiVendorId vendor,
        u64 dedicatedVideoMemoryBytes) -> FRhiAdapterDesc {
        FRhiAdapterDesc desc;
        desc.mName.Assign(name);
        desc.mType                      = type;
        desc.mVendorId                  = vendor;
        desc.mDedicatedVideoMemoryBytes = dedicatedVideoMemoryBytes;
        return desc;
    }
} // namespace

TEST_CASE("RhiMock.ContextCachesAdapters") {
    FRhiMockContext context;

    context.AddAdapter(MakeAdapterDesc(TEXT("Mock Integrated"), ERhiAdapterType::Integrated,
        ERhiVendorId::Intel, 256ULL * 1024ULL * 1024ULL));
    context.AddAdapter(MakeAdapterDesc(
        TEXT("Mock Discrete"), ERhiAdapterType::Discrete, ERhiVendorId::Nvidia, 4ULL << 30));

    FRhiInitDesc initDesc;
    initDesc.mAdapterPreference = ERhiGpuPreference::HighPerformance;
    REQUIRE(context.Init(initDesc));

    REQUIRE_EQ(context.GetEnumerateAdapterCallCount(), 1U);

    const auto adapters = context.EnumerateAdapters();
    REQUIRE_EQ(context.GetEnumerateAdapterCallCount(), 1U);
    REQUIRE_EQ(static_cast<u32>(adapters.Size()), 2U);

    context.MarkAdaptersDirty();
    const auto refreshedAdapters = context.EnumerateAdapters();
    REQUIRE_EQ(context.GetEnumerateAdapterCallCount(), 2U);
    REQUIRE_EQ(static_cast<u32>(refreshedAdapters.Size()), 2U);

    auto device = context.CreateDevice(kRhiInvalidAdapterIndex);
    REQUIRE(device);
    REQUIRE(device->GetAdapterDesc().IsDiscrete());

    REQUIRE_EQ(context.GetEnumerateAdapterCallCount(), 2U);
}

TEST_CASE("RhiMock.DeviceLifecycle") {
    FRhiMockContext context;
    context.AddAdapter(MakeAdapterDesc(
        TEXT("Mock Discrete"), ERhiAdapterType::Discrete, ERhiVendorId::Nvidia, 2ULL << 30));

    REQUIRE(context.Init(FRhiInitDesc{}));
    REQUIRE_EQ(context.GetDeviceCreatedCount(), 0U);
    REQUIRE_EQ(context.GetDeviceDestroyedCount(), 0U);
    REQUIRE_EQ(context.GetDeviceLiveCount(), 0U);

    {
        auto device = context.CreateDevice(0);
        REQUIRE(device);
        REQUIRE_EQ(context.GetDeviceCreatedCount(), 1U);
        REQUIRE_EQ(context.GetDeviceLiveCount(), 1U);
    }

    REQUIRE_EQ(context.GetDeviceDestroyedCount(), 1U);
    REQUIRE_EQ(context.GetDeviceLiveCount(), 0U);

    context.Shutdown();
    REQUIRE_EQ(context.GetShutdownCallCount(), 1U);
}

TEST_CASE("RhiMock.ResourceDeleteQueueDelays") {
    FRhiMockContext context;
    context.AddAdapter(MakeAdapterDesc(
        TEXT("Mock Discrete"), ERhiAdapterType::Discrete, ERhiVendorId::Nvidia, 2ULL << 30));

    REQUIRE(context.Init(FRhiInitDesc{}));
    auto device = context.CreateDevice(0);
    REQUIRE(device);

    FRhiBufferDesc bufferDesc;
    bufferDesc.mDebugName.Assign(TEXT("Mock Buffer"));
    bufferDesc.mSizeBytes = 256U;

    REQUIRE_EQ(context.GetResourceCreatedCount(), 0U);
    REQUIRE_EQ(context.GetResourceDestroyedCount(), 0U);

    {
        FRhiBufferRef buffer = device->CreateBuffer(bufferDesc);
        REQUIRE(buffer);
        buffer->SetRetireSerial(5U);
        REQUIRE_EQ(context.GetResourceCreatedCount(), 1U);
        REQUIRE_EQ(context.GetResourceDestroyedCount(), 0U);
    }

    REQUIRE_EQ(context.GetResourceDestroyedCount(), 0U);
    device->ProcessResourceDeleteQueue(4U);
    REQUIRE_EQ(context.GetResourceDestroyedCount(), 0U);
    device->ProcessResourceDeleteQueue(5U);
    REQUIRE_EQ(context.GetResourceDestroyedCount(), 1U);
}

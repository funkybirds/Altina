#include "TestHarness.h"

#include "RhiD3D11/RhiD3D11Context.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiStructs.h"
#include "Types/Traits.h"

TEST_CASE("RhiD3D11.DeviceCreation") {
#if AE_PLATFORM_WIN
    using AltinaEngine::Rhi::FRhiD3D11Context;
    using AltinaEngine::Rhi::FRhiInitDesc;
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
#else
    // Non-Windows platforms do not support D3D11.
#endif
}

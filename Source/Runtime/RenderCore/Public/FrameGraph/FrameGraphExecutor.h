#pragma once

#include "RenderCoreAPI.h"

#include "FrameGraph/FrameGraph.h"
#include "Rhi/RhiDevice.h"

namespace AltinaEngine::RenderCore {
    class AE_RENDER_CORE_API FFrameGraphExecutor {
    public:
        explicit FFrameGraphExecutor(Rhi::FRhiDevice& device);

        void Execute(FFrameGraph& graph);

    private:
        Rhi::FRhiDevice* mDevice = nullptr;
    };
} // namespace AltinaEngine::RenderCore

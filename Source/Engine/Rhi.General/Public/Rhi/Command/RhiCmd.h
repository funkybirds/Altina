#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/Command/RhiCmdContext.h"

namespace AltinaEngine::Rhi {
    class FRhiCmd {
    public:
        virtual ~FRhiCmd() = default;
        virtual void Execute(FRhiCmdContext& context) = 0;
    };

} // namespace AltinaEngine::Rhi

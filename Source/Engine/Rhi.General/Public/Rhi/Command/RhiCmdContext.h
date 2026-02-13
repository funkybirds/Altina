#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/Command/RhiCmdContextOps.h"

namespace AltinaEngine::Rhi {
    class FRhiCmdContext : public IRhiCmdContextOps {
    public:
        virtual ~FRhiCmdContext() = default;

        virtual void Begin() = 0;
        virtual void End()   = 0;
    };

} // namespace AltinaEngine::Rhi

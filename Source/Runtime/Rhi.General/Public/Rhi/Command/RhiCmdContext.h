#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/Command/RhiCmdContextOps.h"

namespace AltinaEngine::Rhi {
    class FRhiCmdContext : public IRhiCmdContextOps {
    public:
        virtual ~FRhiCmdContext() = default;
    };

} // namespace AltinaEngine::Rhi

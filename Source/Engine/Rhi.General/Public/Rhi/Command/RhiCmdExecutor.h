#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/Command/RhiCmdList.h"

namespace AltinaEngine::Rhi {
    class FRhiCmdExecutor {
    public:
        static void Execute(FRhiCmdList& list, FRhiCmdContext& context) { list.Execute(context); }
    };

} // namespace AltinaEngine::Rhi

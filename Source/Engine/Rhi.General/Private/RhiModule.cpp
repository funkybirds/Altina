#include "RhiModule.h"
#include <Logging/Log.h>

namespace AltinaEngine::Rhi
{
    void FRhiModule::LogHelloWorld()
    {
        LogInfo(TEXT("Hello from Rhi.General!"));
    }
}

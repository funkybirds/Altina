#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_ENGINE_BUILD)
    #define AE_ENGINE_API AE_DLLEXPORT
#else
    #define AE_ENGINE_API AE_DLLIMPORT
#endif






#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_GAMEPLAY_BUILD)
    #define AE_GAMEPLAY_API AE_DLLEXPORT
#else
    #define AE_GAMEPLAY_API AE_DLLIMPORT
#endif

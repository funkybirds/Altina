#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_RENDERING_BUILD)
    #define AE_RENDERING_API AE_DLLEXPORT
#else
    #define AE_RENDERING_API AE_DLLIMPORT
#endif

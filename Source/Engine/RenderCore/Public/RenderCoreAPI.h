#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_RENDER_CORE_BUILD)
    #define AE_RENDER_CORE_API AE_DLLEXPORT
#else
    #define AE_RENDER_CORE_API AE_DLLIMPORT
#endif

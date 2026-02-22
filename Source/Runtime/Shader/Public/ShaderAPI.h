#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_SHADER_BUILD)
    #define AE_SHADER_API AE_DLLEXPORT
#else
    #define AE_SHADER_API AE_DLLIMPORT
#endif

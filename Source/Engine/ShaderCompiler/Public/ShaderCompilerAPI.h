#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_SHADER_COMPILER_BUILD)
    #define AE_SHADER_COMPILER_API AE_DLLEXPORT
#else
    #define AE_SHADER_COMPILER_API AE_DLLIMPORT
#endif

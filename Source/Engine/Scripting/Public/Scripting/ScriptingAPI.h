#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_SCRIPTING_BUILD)
    #define AE_SCRIPTING_API AE_DLLEXPORT
#else
    #define AE_SCRIPTING_API AE_DLLIMPORT
#endif

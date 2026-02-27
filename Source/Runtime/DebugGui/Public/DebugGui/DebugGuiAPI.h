#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_DEBUGGUI_BUILD)
    #define AE_DEBUGGUI_API AE_DLLEXPORT
#else
    #define AE_DEBUGGUI_API AE_DLLIMPORT
#endif

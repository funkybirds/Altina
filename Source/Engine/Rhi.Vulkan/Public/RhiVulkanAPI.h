#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_RHI_VULKAN_BUILD)
    #define AE_RHI_VULKAN_API AE_DLLEXPORT
#else
    #define AE_RHI_VULKAN_API AE_DLLIMPORT
#endif

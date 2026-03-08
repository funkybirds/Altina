#pragma once

#include "DebugGui/DebugGuiAPI.h"
#include "Container/SmartPtr.h"
#include "DebugGui/Interfaces/IDebugGuiSystem.h"

namespace AltinaEngine::DebugGui {
    using Core::Container::TOwner;
    using Core::Container::TPolymorphicDeleter;

    using FDebugGuiSystemOwner = TOwner<IDebugGuiSystem, TPolymorphicDeleter<IDebugGuiSystem>>;

    AE_DEBUGGUI_API auto CreateDebugGuiSystemOwner() -> FDebugGuiSystemOwner;
    AE_DEBUGGUI_API auto CreateDebugGuiSystem() -> IDebugGuiSystem*;
    AE_DEBUGGUI_API void DestroyDebugGuiSystem(IDebugGuiSystem* sys);
} // namespace AltinaEngine::DebugGui

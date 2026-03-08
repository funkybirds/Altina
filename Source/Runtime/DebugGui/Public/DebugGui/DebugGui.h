#pragma once

#include "DebugGui/DebugGuiAPI.h"
#include "DebugGui/Core/Theme.h"
#include "DebugGui/Factory.h"
#include "DebugGui/Interfaces/IDebugGui.h"
#include "DebugGui/Interfaces/IDebugGuiSystem.h"

namespace AltinaEngine::DebugGui {
    // Backward compatibility aliases for existing users of DebugGui.h.
    namespace Container = Core::Container;
    using Container::FStringView;
    using Container::TFunction;
    using Container::TOwner;
    using Container::TPolymorphicDeleter;
    using Core::Math::FVector2f;
} // namespace AltinaEngine::DebugGui

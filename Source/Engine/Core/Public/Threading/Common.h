// Common threading utilities
#pragma once
#include "../Base/CoreAPI.h"

namespace AltinaEngine::Core::Threading {

// Use a portable infinite-wait sentinel instead of exposing platform headers.
inline constexpr unsigned long kInfiniteWait = static_cast<unsigned long>(-1);

} // namespace

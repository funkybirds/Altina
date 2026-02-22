#include "Scripting/ManagedInterop.h"

namespace AltinaEngine::Scripting {
    namespace {
        const FManagedApi* gManagedApi = nullptr;
    }

    void SetManagedApi(const FManagedApi* api) { gManagedApi = api; }

    void ClearManagedApi() { gManagedApi = nullptr; }

    auto GetManagedApi() -> const FManagedApi* { return gManagedApi; }
} // namespace AltinaEngine::Scripting

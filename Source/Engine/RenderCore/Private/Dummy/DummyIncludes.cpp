#include "Geometry/StaticMeshData.h"
#include "View/CameraData.h"
#include "View/ViewData.h"

namespace AltinaEngine::RenderCore {
    void DummyIncludeValidation() {
        Geometry::FStaticMeshData mesh{};
        (void)mesh;

        View::FCameraData camera{};
        (void)camera;

        View::FViewData view{};
        (void)view;
    }
} // namespace AltinaEngine::RenderCore

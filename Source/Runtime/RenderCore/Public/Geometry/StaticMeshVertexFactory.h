#pragma once

#include "RenderCoreAPI.h"

#include "Geometry/VertexLayoutBuilder.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::RenderCore::Geometry {
    // CPU stream mapping for static mesh base pass.
    AE_RENDER_CORE_API auto BuildStaticMeshProvidedLayout(FVertexFactoryProvidedLayout& outLayout)
        -> bool;

    // Transitional layout (keeps renderer behavior unchanged before full reflection path).
    AE_RENDER_CORE_API auto BuildStaticMeshLegacyVertexLayout(Rhi::FRhiVertexLayoutDesc& outLayout)
        -> bool;
} // namespace AltinaEngine::RenderCore::Geometry

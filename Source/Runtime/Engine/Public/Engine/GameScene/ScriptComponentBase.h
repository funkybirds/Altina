#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Reflection/ReflectionAnnotations.h"

namespace AltinaEngine::GameScene {
    /**
     * @brief Common base class for both managed and native script components.
     *
     * This is primarily a semantic tag/type root for "script-like" components.
     * It intentionally contains no serialized fields.
     */
    class ACLASS(Abstract) AE_ENGINE_API FScriptComponentBase : public FComponent {
    public:
        ~FScriptComponentBase() override = default;

    protected:
        FScriptComponentBase() = default;
    };
} // namespace AltinaEngine::GameScene

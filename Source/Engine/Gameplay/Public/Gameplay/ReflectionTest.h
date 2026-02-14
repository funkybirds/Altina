#pragma once

#include "Gameplay/GameplayAPI.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Gameplay {
    class ACLASS(DisplayName="Gameplay Reflection Test", Category="Test") AE_GAMEPLAY_API
        FGameplayReflectionTest {
    public:
        APROPERTY(Editable, Category="Test", ClampMin=0, ClampMax=200)
        i32 mHealth = 100;

        APROPERTY(Editable, Category="Test", ClampMin=0.1, ClampMax=10.0)
        f32 mSpeed = 1.0f;

        AFUNCTION(BlueprintCallable)
        i32 Add(i32 lhs, i32 rhs) const {
            return lhs + rhs;
        }

        AFUNCTION()
        void Reset() {
            mHealth = 100;
            mSpeed = 1.0f;
        }
    };
} // namespace AltinaEngine::Gameplay

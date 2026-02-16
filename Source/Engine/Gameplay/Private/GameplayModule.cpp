#include "Gameplay/GameplayModule.h"
#include "Gameplay/ReflectionTest.h"

#include "Logging/Log.h"
#include "Reflection/Reflection.h"

using AltinaEngine::Core::Container::TRef;
namespace AltinaEngine::Core::Reflection {
    void RegisterReflection_AltinaEngineGameplay();
}

namespace AltinaEngine::Gameplay {
    void FGameplayModule::LogHelloWorld() { LogInfo(TEXT("Hello from Gameplay!")); }

    void FGameplayModule::ValidateReflection() {
        Core::Reflection::RegisterReflection_AltinaEngineGameplay();

        auto obj = Core::Reflection::ConstructObject(
            Core::TypeMeta::FMetaTypeInfo::Create<FGameplayReflectionTest>());
        auto healthMeta =
            Core::TypeMeta::FMetaPropertyInfo::Create<&FGameplayReflectionTest::mHealth>();
        auto speedMeta =
            Core::TypeMeta::FMetaPropertyInfo::Create<&FGameplayReflectionTest::mSpeed>();

        auto  healthProp = Core::Reflection::GetProperty(obj, healthMeta);
        auto  speedProp  = Core::Reflection::GetProperty(obj, speedMeta);

        auto& healthRef = healthProp.As<TRef<i32>>().Get();
        auto& speedRef  = speedProp.As<TRef<f32>>().Get();
        healthRef       = 123;
        speedRef        = 2.5f;

        const auto& data = obj.As<FGameplayReflectionTest>();
        LogInfo(
            TEXT("Gameplay reflection check: mHealth={}, mSpeed={}"), data.mHealth, data.mSpeed);
    }
} // namespace AltinaEngine::Gameplay

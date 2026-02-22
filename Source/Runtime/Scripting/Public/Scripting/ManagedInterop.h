#pragma once

#include "Scripting/ScriptingAPI.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Scripting {
    struct AE_SCRIPTING_API FNativeApi {
        void (*LogInfo)(const char* message)  = nullptr;
        void (*LogError)(const char* message) = nullptr;

        bool (*IsKeyDown)(u16 key)      = nullptr;
        bool (*WasKeyPressed)(u16 key)  = nullptr;
        bool (*WasKeyReleased)(u16 key) = nullptr;

        bool (*IsMouseButtonDown)(u32 button)      = nullptr;
        bool (*WasMouseButtonPressed)(u32 button)  = nullptr;
        bool (*WasMouseButtonReleased)(u32 button) = nullptr;

        i32 (*GetMouseX)()          = nullptr;
        i32 (*GetMouseY)()          = nullptr;
        i32 (*GetMouseDeltaX)()     = nullptr;
        i32 (*GetMouseDeltaY)()     = nullptr;
        f32 (*GetMouseWheelDelta)() = nullptr;

        u32 (*GetWindowWidth)()  = nullptr;
        u32 (*GetWindowHeight)() = nullptr;
        bool (*HasFocus)()       = nullptr;

        u32 (*GetCharInputCount)()       = nullptr;
        u32 (*GetCharInputAt)(u32 index) = nullptr;
    };

    struct AE_SCRIPTING_API FManagedCreateArgs {
        const char* mAssemblyPathUtf8 = nullptr;
        const char* mTypeNameUtf8     = nullptr;
        u32         mOwnerIndex       = 0;
        u32         mOwnerGeneration  = 0;
        u32         mWorldId          = 0;
    };

    struct AE_SCRIPTING_API FManagedApi {
        u64 (*CreateInstance)(const FManagedCreateArgs* args) = nullptr;
        void (*DestroyInstance)(u64 handle)                   = nullptr;
        void (*OnCreate)(u64 handle)                          = nullptr;
        void (*OnDestroy)(u64 handle)                         = nullptr;
        void (*OnEnable)(u64 handle)                          = nullptr;
        void (*OnDisable)(u64 handle)                         = nullptr;
        void (*Tick)(u64 handle, f32 dt)                      = nullptr;
    };

    AE_SCRIPTING_API void               SetManagedApi(const FManagedApi* api);
    AE_SCRIPTING_API void               ClearManagedApi();
    [[nodiscard]] AE_SCRIPTING_API auto GetManagedApi() -> const FManagedApi*;
} // namespace AltinaEngine::Scripting

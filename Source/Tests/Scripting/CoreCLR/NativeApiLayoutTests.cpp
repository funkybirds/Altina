#include "TestHarness.h"

#include "Scripting/ManagedInterop.h"

#include <cstddef>

using AltinaEngine::Scripting::FNativeApi;

TEST_CASE("Scripting.CoreCLR.NativeApiLayout") {
    // Keep these checks simple: they guard against accidental field reordering/removal.
    STATIC_REQUIRE(
        offsetof(FNativeApi, GetWorldTranslation) < offsetof(FNativeApi, SetWorldTranslation));
    STATIC_REQUIRE(
        offsetof(FNativeApi, SetWorldTranslation) < offsetof(FNativeApi, GetLocalTranslation));
    STATIC_REQUIRE(
        offsetof(FNativeApi, GetLocalTranslation) < offsetof(FNativeApi, SetLocalTranslation));
    STATIC_REQUIRE(
        offsetof(FNativeApi, SetLocalTranslation) < offsetof(FNativeApi, GetWorldRotation));
    STATIC_REQUIRE(offsetof(FNativeApi, GetWorldRotation) < offsetof(FNativeApi, SetWorldRotation));
    STATIC_REQUIRE(offsetof(FNativeApi, SetWorldRotation) < offsetof(FNativeApi, GetLocalRotation));
    STATIC_REQUIRE(offsetof(FNativeApi, GetLocalRotation) < offsetof(FNativeApi, SetLocalRotation));

    REQUIRE(sizeof(FNativeApi) >= sizeof(void*) * 10U);
}

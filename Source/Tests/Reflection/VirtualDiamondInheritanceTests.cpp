#include "TestHarness.h"

#include "Reflection/Reflection.h"

#include <mutex>
#include <cstdint>

using namespace AltinaEngine::Core::Reflection;

// Virtual (diamond) inheritance test types
struct VBase {
    virtual ~VBase() = default;
    int mBase        = 1;
};

struct VLeft : virtual VBase {
    virtual ~VLeft() = default;
    int mLeft        = 2;
};

struct VRight : virtual VBase {
    virtual ~VRight() = default;
    int mRight        = 3;
};

struct VDerived : VLeft, VRight {
    int mDerived = 4;
    VDerived()   = default;
};

TEST_CASE("Reflection.VirtualDiamond.AsVirtualBases") {
    // Register types once per process to avoid duplicate registration crashes
    static std::once_flag sReg;
    std::call_once(sReg, []() {
        RegisterType<VBase>();
        RegisterPropertyField<&VBase::mBase>("mBase");

        RegisterType<VLeft>();
        RegisterPropertyField<&VLeft::mLeft>("mLeft");

        RegisterType<VRight>();
        RegisterPropertyField<&VRight::mRight>("mRight");

        RegisterType<VDerived>();
        RegisterPropertyField<&VDerived::mDerived>("mDerived");

        // Register polymorphic relations: connect each accessible base to the concrete derived
        RegisterPolymorphicRelation<VBase, VDerived>();
        RegisterPolymorphicRelation<VLeft, VDerived>();
        RegisterPolymorphicRelation<VRight, VDerived>();
    });

    // Create via reflection
    auto  obj = FObject::Create<VDerived>();

    // Exact type
    auto& dref = obj.As<VDerived>();
    REQUIRE_EQ(dref.mDerived, 4);

    // Access each base via As<T>
    auto& lref = obj.As<VLeft>();
    auto& rref = obj.As<VRight>();
    auto& bref = obj.As<VBase>();
    REQUIRE_EQ(lref.mLeft, 2);
    REQUIRE_EQ(rref.mRight, 3);
    REQUIRE_EQ(bref.mBase, 1);

    // Verify address adjustments match C++ static_cast for virtual inheritance
    auto*   derivedPtr = &obj.As<VDerived>();
    VLeft*  expectedL  = static_cast<VLeft*>(derivedPtr);
    VRight* expectedR  = static_cast<VRight*>(derivedPtr);
    VBase*  expectedB  = static_cast<VBase*>(derivedPtr);

    REQUIRE_EQ(
        reinterpret_cast<uintptr_t>(&obj.As<VLeft>()), reinterpret_cast<uintptr_t>(expectedL));
    REQUIRE_EQ(
        reinterpret_cast<uintptr_t>(&obj.As<VRight>()), reinterpret_cast<uintptr_t>(expectedR));
    REQUIRE_EQ(
        reinterpret_cast<uintptr_t>(&obj.As<VBase>()), reinterpret_cast<uintptr_t>(expectedB));
}

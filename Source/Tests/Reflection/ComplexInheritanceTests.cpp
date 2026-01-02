#include "TestHarness.h"

#include "Reflection/Reflection.h"

#include <cstdint>

using namespace AltinaEngine::Core::Reflection;

// Complex multiple inheritance types for testing
struct CBaseA
{
    virtual ~CBaseA() = default;
    int mA            = 11;
};

struct CBaseB
{
    virtual ~CBaseB() = default;
    double mB         = 22.5;
};

struct CDerived : CBaseA, CBaseB
{
    int64_t mD = 33;
    CDerived() = default;
};

TEST_CASE("Reflection.ComplexInheritance.AsMultipleBases")
{
    // Register types and relations locally for this test
    RegisterType<CBaseA>();
    RegisterPropertyField<&CBaseA::mA>("mA");

    RegisterType<CBaseB>();
    RegisterPropertyField<&CBaseB::mB>("mB");

    RegisterType<CDerived>();
    RegisterPropertyField<&CDerived::mD>("mD");

    RegisterPolymorphicRelation<CBaseA, CDerived>();
    RegisterPolymorphicRelation<CBaseB, CDerived>();

    // Create object via reflection factory
    auto  obj = FObject::Create<CDerived>();

    // Exact type access
    auto& dr = obj.As<CDerived>();
    REQUIRE_EQ(dr.mD, 33);

    // Upcast to each base and verify member values
    auto& aref = obj.As<CBaseA>();
    auto& bref = obj.As<CBaseB>();
    REQUIRE_EQ(aref.mA, 11);
    REQUIRE_CLOSE(bref.mB, 22.5, 0.0001);

    // Verify addresses match C++ static_cast adjustments
    auto*   derivedPtr = &obj.As<CDerived>();
    CBaseA* expectedA  = static_cast<CBaseA*>(derivedPtr);
    CBaseB* expectedB  = static_cast<CBaseB*>(derivedPtr);

    REQUIRE_EQ(reinterpret_cast<uintptr_t>(&obj.As<CBaseA>()), reinterpret_cast<uintptr_t>(expectedA));
    REQUIRE_EQ(reinterpret_cast<uintptr_t>(&obj.As<CBaseB>()), reinterpret_cast<uintptr_t>(expectedB));

    // Const correctness: const FObject should give const reference
    const auto  cobj  = obj;
    const auto& crefA = cobj.As<CBaseA>();
    (void)crefA; // just ensure compiles and returns const ref
}

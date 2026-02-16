#include "TestHarness.h"
#include "ReflectionTestCommon.h"

// All test types are defined in ReflectionTestCommon.h

// ============================================================================
// Test: Multiple Property Registration and Access
// ============================================================================

TEST_CASE("Reflection.Advanced.MultiplePropertyAccess") {
    ReflectionTestHelpers::EnsureTypesRegistered();

    auto obj = ConstructObject(FMetaTypeInfo::Create<FComplexStruct>());

    // Access and modify all properties
    {
        auto propMeta                 = FMetaPropertyInfo::Create<&FComplexStruct::mA>();
        auto propObj                  = GetProperty(obj, propMeta);
        propObj.As<TRef<int>>().Get() = 10;
    }
    {
        auto propMeta                 = FMetaPropertyInfo::Create<&FComplexStruct::mB>();
        auto propObj                  = GetProperty(obj, propMeta);
        propObj.As<TRef<int>>().Get() = 20;
    }
    {
        auto propMeta                 = FMetaPropertyInfo::Create<&FComplexStruct::mC>();
        auto propObj                  = GetProperty(obj, propMeta);
        propObj.As<TRef<int>>().Get() = 30;
    }
    {
        auto propMeta                   = FMetaPropertyInfo::Create<&FComplexStruct::mX>();
        auto propObj                    = GetProperty(obj, propMeta);
        propObj.As<TRef<float>>().Get() = 100.0f;
    }
    {
        auto propMeta                   = FMetaPropertyInfo::Create<&FComplexStruct::mY>();
        auto propObj                    = GetProperty(obj, propMeta);
        propObj.As<TRef<float>>().Get() = 200.0f;
    }
    {
        auto propMeta                    = FMetaPropertyInfo::Create<&FComplexStruct::mZ>();
        auto propObj                     = GetProperty(obj, propMeta);
        propObj.As<TRef<double>>().Get() = 300.0;
    }

    // Verify all modifications
    auto& s = obj.As<FComplexStruct>();
    REQUIRE_EQ(s.mA, 10);
    REQUIRE_EQ(s.mB, 20);
    REQUIRE_EQ(s.mC, 30);
    REQUIRE_CLOSE(s.mX, 100.0f, 0.001f);
    REQUIRE_CLOSE(s.mY, 200.0f, 0.001f);
    REQUIRE_CLOSE(s.mZ, 300.0, 0.001);
}

// ============================================================================
// Test: Memory Layout Validation
// ============================================================================

TEST_CASE("Reflection.Advanced.MemoryLayout") {
    ReflectionTestHelpers::EnsureTypesRegistered();
    auto  obj = ConstructObject(FMetaTypeInfo::Create<FComplexStruct>());
    auto& s   = obj.As<FComplexStruct>();

    // Get addresses through reflection
    auto  propMetaA = FMetaPropertyInfo::Create<&FComplexStruct::mA>();
    auto  propObjA  = GetProperty(obj, propMetaA);
    auto& refA      = propObjA.As<TRef<int>>().Get();

    auto  propMetaB = FMetaPropertyInfo::Create<&FComplexStruct::mB>();
    auto  propObjB  = GetProperty(obj, propMetaB);
    auto& refB      = propObjB.As<TRef<int>>().Get();

    auto  propMetaC = FMetaPropertyInfo::Create<&FComplexStruct::mC>();
    auto  propObjC  = GetProperty(obj, propMetaC);
    auto& refC      = propObjC.As<TRef<int>>().Get();

    // Verify addresses match struct layout
    REQUIRE_EQ((u64)&s.mA, (u64)&refA);
    REQUIRE_EQ((u64)&s.mB, (u64)&refB);
    REQUIRE_EQ((u64)&s.mC, (u64)&refC);

    // Verify sequential layout (assuming no padding issues)
    u64 addrA = (u64)&refA;
    u64 addrB = (u64)&refB;
    u64 addrC = (u64)&refC;

    REQUIRE_EQ(addrB - addrA, sizeof(int));
    REQUIRE_EQ(addrC - addrB, sizeof(int));
}

// ============================================================================
// Test: Large Structure Handling
// ============================================================================

TEST_CASE("Reflection.Advanced.LargeStructure") {
    auto  obj = ConstructObject(FMetaTypeInfo::Create<FLargeStruct>());

    auto& s = obj.As<FLargeStruct>();

    // Verify constructor was called
    REQUIRE_EQ(s.mValues[0], 0);
    REQUIRE_EQ(s.mValues[50], 50);
    REQUIRE_EQ(s.mValues[99], 99);
    REQUIRE_CLOSE(s.mDoubles[0], 0.0, 0.001);
    REQUIRE_CLOSE(s.mDoubles[25], 12.5, 0.001);
    REQUIRE_CLOSE(s.mDoubles[49], 24.5, 0.001);

    // Modify and verify
    s.mValues[42] = 12345;
    REQUIRE_EQ(s.mValues[42], 12345);
}

// ============================================================================
// Test: Empty Structure
// ============================================================================

TEST_CASE("Reflection.Advanced.EmptyStructure") {
    auto  obj = ConstructObject(FMetaTypeInfo::Create<FEmptyStruct>());

    // Should be able to cast to empty struct
    auto& s = obj.As<FEmptyStruct>();
    (void)s; // Suppress unused variable warning
}

// ============================================================================
// Test: Type Hash Uniqueness
// ============================================================================

TEST_CASE("Reflection.Advanced.TypeHashUniqueness") {
    auto hash1 = FMetaTypeInfo::Create<FComplexStruct>().GetHash();
    auto hash2 = FMetaTypeInfo::Create<FLargeStruct>().GetHash();
    auto hash3 = FMetaTypeInfo::Create<FEmptyStruct>().GetHash();
    auto hash4 = FMetaTypeInfo::Create<int>().GetHash();
    auto hash5 = FMetaTypeInfo::Create<float>().GetHash();
    auto hash6 = FMetaTypeInfo::Create<double>().GetHash();

    // All hashes should be unique
    REQUIRE(hash1 != hash2);
    REQUIRE(hash1 != hash3);
    REQUIRE(hash1 != hash4);
    REQUIRE(hash2 != hash3);
    REQUIRE(hash2 != hash4);
    REQUIRE(hash3 != hash4);
    REQUIRE(hash4 != hash5);
    REQUIRE(hash5 != hash6);

    // Same type should produce same hash
    auto hash1Copy = FMetaTypeInfo::Create<FComplexStruct>().GetHash();
    REQUIRE_EQ(hash1, hash1Copy);
}

// ============================================================================
// Test: Property Hash Uniqueness
// ============================================================================

TEST_CASE("Reflection.Advanced.PropertyHashUniqueness") {
    auto hashA = FMetaPropertyInfo::Create<&FComplexStruct::mA>().GetHash();
    auto hashB = FMetaPropertyInfo::Create<&FComplexStruct::mB>().GetHash();
    auto hashC = FMetaPropertyInfo::Create<&FComplexStruct::mC>().GetHash();
    auto hashX = FMetaPropertyInfo::Create<&FComplexStruct::mX>().GetHash();
    auto hashY = FMetaPropertyInfo::Create<&FComplexStruct::mY>().GetHash();
    auto hashZ = FMetaPropertyInfo::Create<&FComplexStruct::mZ>().GetHash();

    // All property hashes should be unique
    REQUIRE(hashA != hashB);
    REQUIRE(hashA != hashC);
    REQUIRE(hashB != hashC);
    REQUIRE(hashX != hashY);
    REQUIRE(hashX != hashZ);
    REQUIRE(hashY != hashZ);
    REQUIRE(hashA != hashX);

    // Same property should produce same hash
    auto hashACopy = FMetaPropertyInfo::Create<&FComplexStruct::mA>().GetHash();
    REQUIRE_EQ(hashA, hashACopy);
}

// ============================================================================
// Test: Object Copy Semantics
// ============================================================================

TEST_CASE("Reflection.Advanced.ObjectCopySemantics") {
    ReflectionTestHelpers::EnsureTypesRegistered();

    auto  obj1 = ConstructObject(FMetaTypeInfo::Create<FComplexStruct>());
    auto& s1   = obj1.As<FComplexStruct>();
    s1.mA      = 123;

    // Copy object - FObject copy constructor creates a new copy
    auto  obj2 = obj1;
    auto& s2   = obj2.As<FComplexStruct>();

    // Copy should have the same initial value
    REQUIRE_EQ(s2.mA, 123);

    // Copy creates new instance, so addresses should differ
    REQUIRE((u64)&s1 != (u64)&s2);

    // Modifying one should NOT affect the other (independent copies)
    s1.mA = 456;
    REQUIRE_EQ(s1.mA, 456);
    REQUIRE_EQ(s2.mA, 123); // s2 should still have original value
}

// ============================================================================
// Test: Multiple Object Instances
// ============================================================================

TEST_CASE("Reflection.Advanced.MultipleInstances") {
    ReflectionTestHelpers::EnsureTypesRegistered(); // Create multiple instances
    auto obj1 = ConstructObject(FMetaTypeInfo::Create<FComplexStruct>());
    auto obj2 = ConstructObject(FMetaTypeInfo::Create<FComplexStruct>());
    auto obj3 = ConstructObject(FMetaTypeInfo::Create<FComplexStruct>());

    // Modify each independently through reflection
    auto propMeta = FMetaPropertyInfo::Create<&FComplexStruct::mA>();

    GetProperty(obj1, propMeta).As<TRef<int>>().Get() = 100;
    GetProperty(obj2, propMeta).As<TRef<int>>().Get() = 200;
    GetProperty(obj3, propMeta).As<TRef<int>>().Get() = 300;

    // Verify independence
    REQUIRE_EQ(obj1.As<FComplexStruct>().mA, 100);
    REQUIRE_EQ(obj2.As<FComplexStruct>().mA, 200);
    REQUIRE_EQ(obj3.As<FComplexStruct>().mA, 300);
}

// ============================================================================
// Test: Property Type Information
// ============================================================================

TEST_CASE("Reflection.Advanced.PropertyTypeInformation") {
    auto  propMetaInt    = FMetaPropertyInfo::Create<&FComplexStruct::mA>();
    auto  propMetaFloat  = FMetaPropertyInfo::Create<&FComplexStruct::mX>();
    auto  propMetaDouble = FMetaPropertyInfo::Create<&FComplexStruct::mZ>();

    // Verify property type metadata
    auto& intTypeMeta    = propMetaInt.GetPropertyTypeMetadata();
    auto& floatTypeMeta  = propMetaFloat.GetPropertyTypeMetadata();
    auto& doubleTypeMeta = propMetaDouble.GetPropertyTypeMetadata();

    REQUIRE_EQ(intTypeMeta.GetHash(), FMetaTypeInfo::Create<int>().GetHash());
    REQUIRE_EQ(floatTypeMeta.GetHash(), FMetaTypeInfo::Create<float>().GetHash());
    REQUIRE_EQ(doubleTypeMeta.GetHash(), FMetaTypeInfo::Create<double>().GetHash());

    // Verify class type metadata
    auto& classTypeMeta1 = propMetaInt.GetClassTypeMetadata();
    auto& classTypeMeta2 = propMetaFloat.GetClassTypeMetadata();
    auto& classTypeMeta3 = propMetaDouble.GetClassTypeMetadata();

    auto  expectedClassHash = FMetaTypeInfo::Create<FComplexStruct>().GetHash();
    REQUIRE_EQ(classTypeMeta1.GetHash(), expectedClassHash);
    REQUIRE_EQ(classTypeMeta2.GetHash(), expectedClassHash);
    REQUIRE_EQ(classTypeMeta3.GetHash(), expectedClassHash);
}

// ============================================================================
// Test: Const Correctness
// ============================================================================

TEST_CASE("Reflection.Advanced.ConstCorrectness") {
    ReflectionTestHelpers::EnsureTypesRegistered();
    const auto obj      = ConstructObject(FMetaTypeInfo::Create<FComplexStruct>());
    const auto typeMeta = FMetaTypeInfo::Create<FComplexStruct>();
    const auto propMeta = FMetaPropertyInfo::Create<&FComplexStruct::mA>();

    // Should be able to query const metadata
    REQUIRE(typeMeta.GetHash() != 0);
    REQUIRE(propMeta.GetHash() != 0);
    REQUIRE(propMeta.GetName().Length() > 0);

    // Note: GetProperty takes non-const FObject&, so we can't test property access here
    // This test primarily validates const metadata queries
}

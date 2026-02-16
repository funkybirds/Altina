#include "TestHarness.h"
#include "ReflectionTestCommon.h"

// All test types are defined in ReflectionTestCommon.h

// ============================================================================
// Test: Type Registration
// ============================================================================

TEST_CASE("Reflection.TypeRegistration") {
    ReflectionTestHelpers::EnsureTypesRegistered();

    // Verify we can construct an object
    auto  obj = ConstructObject(FMetaTypeInfo::Create<FSimpleTestStruct>());

    // Verify the object has the correct default values
    auto& s = obj.As<FSimpleTestStruct>();
    REQUIRE_EQ(s.mIntValue, 42);
    REQUIRE_CLOSE(s.mFloatValue, 3.14f, 0.001f);
    REQUIRE_CLOSE(s.mDoubleValue, 2.718, 0.001);
}

// ============================================================================
// Test: Property Field Registration - Integer
// ============================================================================

TEST_CASE("Reflection.PropertyField.Integer") {
    ReflectionTestHelpers::EnsureTypesRegistered();
    // Create object
    auto  obj = ConstructObject(FMetaTypeInfo::Create<FSimpleTestStruct>());

    // Get property metadata
    auto  propMeta = FMetaPropertyInfo::Create<&FSimpleTestStruct::mIntValue>();

    // Access property through reflection
    auto  propObj = GetProperty(obj, propMeta);

    // Verify initial value
    auto& intRef = propObj.As<TRef<int>>().Get();
    REQUIRE_EQ(intRef, 42);

    // Modify through reflection
    intRef = 999;

    // Verify modification
    auto& s = obj.As<FSimpleTestStruct>();
    REQUIRE_EQ(s.mIntValue, 999);

    // Verify addresses match (reference semantics)
    REQUIRE_EQ((u64)&s.mIntValue, (u64)&intRef);
}

// ============================================================================
// Test: Property Field Registration - Float
// ============================================================================

TEST_CASE("Reflection.PropertyField.Float") {
    ReflectionTestHelpers::EnsureTypesRegistered();
    auto  obj      = ConstructObject(FMetaTypeInfo::Create<FSimpleTestStruct>());
    auto  propMeta = FMetaPropertyInfo::Create<&FSimpleTestStruct::mFloatValue>();
    auto  propObj  = GetProperty(obj, propMeta);

    auto& floatRef = propObj.As<TRef<float>>().Get();
    REQUIRE_CLOSE(floatRef, 3.14f, 0.001f);

    floatRef = 123.456f;

    auto& s = obj.As<FSimpleTestStruct>();
    REQUIRE_CLOSE(s.mFloatValue, 123.456f, 0.001f);
    REQUIRE_EQ((u64)&s.mFloatValue, (u64)&floatRef);
}

// ============================================================================
// Test: Property Field Registration - Double
// ============================================================================

TEST_CASE("Reflection.PropertyField.Double") {
    ReflectionTestHelpers::EnsureTypesRegistered();
    auto  obj      = ConstructObject(FMetaTypeInfo::Create<FSimpleTestStruct>());
    auto  propMeta = FMetaPropertyInfo::Create<&FSimpleTestStruct::mDoubleValue>();
    auto  propObj  = GetProperty(obj, propMeta);

    auto& doubleRef = propObj.As<TRef<double>>().Get();
    REQUIRE_CLOSE(doubleRef, 2.718, 0.001);

    doubleRef = 9.87654321;

    auto& s = obj.As<FSimpleTestStruct>();
    REQUIRE_CLOSE(s.mDoubleValue, 9.87654321, 0.001);
    REQUIRE_EQ((u64)&s.mDoubleValue, (u64)&doubleRef);
}

// ============================================================================
// Test: Multiple Properties
// ============================================================================

TEST_CASE("Reflection.MultipleProperties") {
    ReflectionTestHelpers::EnsureTypesRegistered();
    auto obj = ConstructObject(FMetaTypeInfo::Create<FSimpleTestStruct>());

    // Modify int
    auto intPropMeta                 = FMetaPropertyInfo::Create<&FSimpleTestStruct::mIntValue>();
    auto intPropObj                  = GetProperty(obj, intPropMeta);
    intPropObj.As<TRef<int>>().Get() = 100;

    // Modify float
    auto floatPropMeta = FMetaPropertyInfo::Create<&FSimpleTestStruct::mFloatValue>();
    auto floatPropObj  = GetProperty(obj, floatPropMeta);
    floatPropObj.As<TRef<float>>().Get() = 200.0f;

    // Modify double
    auto doublePropMeta = FMetaPropertyInfo::Create<&FSimpleTestStruct::mDoubleValue>();
    auto doublePropObj  = GetProperty(obj, doublePropMeta);
    doublePropObj.As<TRef<double>>().Get() = 300.0;

    // Verify all modifications
    auto& s = obj.As<FSimpleTestStruct>();
    REQUIRE_EQ(s.mIntValue, 100);
    REQUIRE_CLOSE(s.mFloatValue, 200.0f, 0.001f);
    REQUIRE_CLOSE(s.mDoubleValue, 300.0, 0.001);
}

// ============================================================================
// Test: Nested Structures
// ============================================================================

TEST_CASE("Reflection.NestedStructures") {
    ReflectionTestHelpers::EnsureTypesRegistered();
    auto obj = ConstructObject(FMetaTypeInfo::Create<FNestedTestStruct>());

    // Access nested struct's ID
    auto idPropMeta                 = FMetaPropertyInfo::Create<&FNestedTestStruct::mId>();
    auto idPropObj                  = GetProperty(obj, idPropMeta);
    idPropObj.As<TRef<int>>().Get() = 500;

    // Access nested struct
    auto  nestedPropMeta = FMetaPropertyInfo::Create<&FNestedTestStruct::mNested>();
    auto  nestedPropObj  = GetProperty(obj, nestedPropMeta);

    // Verify
    auto& nested = obj.As<FNestedTestStruct>();
    REQUIRE_EQ(nested.mId, 500);
    REQUIRE_EQ(nested.mNested.mIntValue, 42); // Default value
}

// ============================================================================
// Test: Type Metadata Query
// ============================================================================

TEST_CASE("Reflection.TypeMetadata.Query") {
    auto intMeta = FMetaTypeInfo::Create<int>();
    REQUIRE(intMeta.GetHash() != 0);

    auto floatMeta = FMetaTypeInfo::Create<float>();
    REQUIRE(floatMeta.GetHash() != 0);
    REQUIRE(intMeta.GetHash() != floatMeta.GetHash());

    auto structMeta = FMetaTypeInfo::Create<FSimpleTestStruct>();
    REQUIRE(structMeta.GetHash() != 0);
    REQUIRE(structMeta.GetHash() != intMeta.GetHash());
}

// ============================================================================
// Test: Property Metadata Query
// ============================================================================

TEST_CASE("Reflection.PropertyMetadata.Query") {
    auto propMeta = FMetaPropertyInfo::Create<&FSimpleTestStruct::mIntValue>();

    REQUIRE(propMeta.GetHash() != 0);
    REQUIRE(propMeta.GetName().Length() > 0);

    auto& propTypeMeta = propMeta.GetPropertyTypeMetadata();
    REQUIRE(propTypeMeta.GetHash() == FMetaTypeInfo::Create<int>().GetHash());

    auto& classTypeMeta = propMeta.GetClassTypeMetadata();
    REQUIRE(classTypeMeta.GetHash() == FMetaTypeInfo::Create<FSimpleTestStruct>().GetHash());
}

// ============================================================================
// Test: Object Validity
// ============================================================================

TEST_CASE("Reflection.Object.Validity") {
    ReflectionTestHelpers::EnsureTypesRegistered();

    auto  obj = ConstructObject(FMetaTypeInfo::Create<FSimpleTestStruct>());

    // Copy creates an independent copy
    auto  objCopy = obj;

    // Both should be different objects
    auto& s1 = obj.As<FSimpleTestStruct>();
    auto& s2 = objCopy.As<FSimpleTestStruct>();

    // Initially they have the same values
    REQUIRE_EQ(s1.mIntValue, s2.mIntValue);

    // Modifying one doesn't affect the other (independent copies)
    s1.mIntValue = 12345;
    REQUIRE_EQ(s1.mIntValue, 12345);
    REQUIRE(s2.mIntValue != 12345); // s2 should still have the original value (42)
}

// ============================================================================
// Test: Reference Semantics
// ============================================================================

TEST_CASE("Reflection.ReferenceSematics") {
    ReflectionTestHelpers::EnsureTypesRegistered();
    auto  obj = ConstructObject(FMetaTypeInfo::Create<FSimpleTestStruct>());

    // Get property twice
    auto  propMeta = FMetaPropertyInfo::Create<&FSimpleTestStruct::mIntValue>();
    auto  propObj1 = GetProperty(obj, propMeta);
    auto  propObj2 = GetProperty(obj, propMeta);

    // Both should reference the same underlying data
    auto& ref1 = propObj1.As<TRef<int>>().Get();
    auto& ref2 = propObj2.As<TRef<int>>().Get();

    REQUIRE_EQ((u64)&ref1, (u64)&ref2);

    ref1 = 999;
    REQUIRE_EQ(ref2, 999);
}

#include "ReflectionTestCommon.h"
#include <mutex>

namespace ReflectionTestHelpers {
    void EnsureTypesRegistered() {
        static std::once_flag sRegistrationFlag;
        std::call_once(sRegistrationFlag, []() {
            // Register basic types
            RegisterType<FSimpleTestStruct>();
            RegisterPropertyField<&FSimpleTestStruct::mIntValue>("IntValue");
            RegisterPropertyField<&FSimpleTestStruct::mFloatValue>("FloatValue");
            RegisterPropertyField<&FSimpleTestStruct::mDoubleValue>("DoubleValue");

            // Register nested types
            RegisterType<FNestedTestStruct>();
            RegisterPropertyField<&FNestedTestStruct::mId>("Id");
            RegisterPropertyField<&FNestedTestStruct::mNested>("Nested");

            // Register polymorphic types
            RegisterType<FPolymorphicBase>();
            RegisterPropertyField<&FPolymorphicBase::mBaseValue>("BaseValue");

            RegisterType<FPolymorphicDerived>();
            RegisterPropertyField<&FPolymorphicDerived::mDerivedValue>("DerivedValue");
            RegisterPolymorphicRelation<FPolymorphicBase, FPolymorphicDerived>();

            // Register complex types
            RegisterType<FComplexStruct>();
            RegisterPropertyField<&FComplexStruct::mA>("A");
            RegisterPropertyField<&FComplexStruct::mB>("B");
            RegisterPropertyField<&FComplexStruct::mC>("C");
            RegisterPropertyField<&FComplexStruct::mX>("X");
            RegisterPropertyField<&FComplexStruct::mY>("Y");
            RegisterPropertyField<&FComplexStruct::mZ>("Z");

            // Register large struct
            RegisterType<FLargeStruct>();

            // Register empty struct
            RegisterType<FEmptyStruct>();
        });
    }
} // namespace ReflectionTestHelpers

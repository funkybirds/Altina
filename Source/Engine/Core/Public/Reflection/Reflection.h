#pragma once

#include "Container/Span.h"
#include "Reflection/Object.h"
#include "Container/IndexSequence.h"
#include "Reflection/ReflectionBase.h"
#include "Utility/CompilerHint.h"
#include "Container/Ref.h"
namespace AltinaEngine::Core::Reflection
{
    using Container::FNativeStringView;
    using Container::MakeRef;
    using Container::TIndexSequence;
    using Container::TSpan;

    namespace Detail
    {
        using TFnMemberFunctionInvoker  = FObject (*)(FObject&, TSpan<FObject>);
        using TFnMemberPropertyAccessor = FObject (*)(FObject&);

        template <typename T, typename R, typename... Args, usize... I>
        auto MemberFunctorInvokerWrapperImpl(R (T::*f)(Args...), T& obj, TSpan<FObject> vec, TIndexSequence<I...>) -> R
        {
            return (obj.*f)(vec[I].template As<Args>()...); // NOLINT
        }

        template <typename T, typename R, typename... Args>
        auto MemberFunctorInvokerWrapper(R (T::*f)(Args...), T& obj, TSpan<FObject> vec) -> R
        {
            if (ReflectionAssert(vec.Size() == sizeof...(Args), EReflectionErrorCode::MismatchedArgumentNumber,
                    FReflectionDumpData{})) [[likely]]
            {
                return MemberFunctorInvokerWrapperImpl(f, obj, vec, Container::TIndexSequenceFor<Args...>{});
            }
            Utility::CompilerHint::Unreachable();
        }

        // Accessors
        template <auto Member>
            requires IMemberPointer<decltype(Member)>
        struct TAutoMemberAccessor : TMemberType<decltype(Member)>
        {
            using TSuper = TMemberType<decltype(Member)>;
            static auto Get(typename TSuper::TClassType& object) -> TSuper::TBaseType& { return object.*Member; }
            static auto GetAccessor() -> TFnMemberPropertyAccessor
            {
                return [](FObject& obj) -> FObject {
                    auto& classObject = obj.As<typename TSuper::TClassType>();
                    return FObject::CreateClone(Container::MakeRef(Get(classObject)));
                };
            }
        };
        template <auto Member>
            requires IMemberFunctionPointer<decltype(Member)>
        struct TAutoMemberFunctionAccessor : TMemberFunctionTrait<decltype(Member)>
        {
            using TSuper = TMemberFunctionTrait<decltype(Member)>;
            static auto GetInvoker() -> TFnMemberFunctionInvoker
            {
                return [](FObject& classObject, TSpan<FObject> args) -> FObject {
                    auto& obj = classObject.As<typename TSuper::TClassType>();
                    if constexpr (TTypeIsVoid_v<typename TSuper::TReturnType>)
                    {
                        MemberFunctorInvokerWrapper(Member, obj, args);
                        return FObject::Create<void>();
                    }
                    else
                    {
                        return FObject::CreateClone(MemberFunctorInvokerWrapper(Member, obj, args));
                    }
                };
            };
        };

        AE_CORE_API void RegisterType(const FTypeInfo& stdTypeInfo, const FMetaTypeInfo& meta);
        AE_CORE_API void RegisterPolymorphicRelation(FTypeMetaHash baseType, FTypeMetaHash derivedType);
        AE_CORE_API void RegisterPropertyField(
            const FMetaPropertyInfo& propMeta, FNativeStringView name, TFnMemberPropertyAccessor accessor);
        AE_CORE_API auto ConstructObject(FTypeMetaHash classHash) -> FObject;
        AE_CORE_API auto GetProperty(FObject& object, FTypeMetaHash propHash, FTypeMetaHash classHash) -> FObject;

    } // namespace Detail

    template <typename T> void RegisterType()
    {
        Detail::RegisterType(GetRttiTypeInfo<T>(), FMetaTypeInfo::Create<T>());
    }

    template <typename TBase, typename TDerived>
        requires IClassBaseOf<TBase, TDerived>
    void RegisterPolymorphicRelation()
    {
        Detail::RegisterPolymorphicRelation(
            FMetaTypeInfo::Create<TBase>().GetHash(), FMetaTypeInfo::Create<TDerived>().GetHash());
    }

    template <auto Member>
        requires IMemberPointer<decltype(Member)>
    void RegisterPropertyField(FNativeStringView name)
    {
        using TAccessor = Detail::TAutoMemberAccessor<Member>;
        auto propField  = FMetaPropertyInfo::Create<Member>();
        Detail::RegisterPropertyField(propField, name, TAccessor::GetAccessor());
    }

    inline auto ConstructObject(const FMetaTypeInfo& valueMeta) -> FObject
    {
        return Detail::ConstructObject(valueMeta.GetHash());
    }
    inline auto GetProperty(FObject& object, const FMetaPropertyInfo& propMeta) -> FObject
    {
        return Detail::GetProperty(object, propMeta.GetHash(), propMeta.GetClassTypeMetadata().GetHash());
    }

} // namespace AltinaEngine::Core::Reflection
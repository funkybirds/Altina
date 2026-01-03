#pragma once

#include "Reflection/Object.h"
#include "Reflection/ReflectionBase.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::Core::Reflection {

    namespace Detail {
        template <typename T, typename R, typename... Args, usize... I>
        auto MemberFunctorInvokerWrapperImpl(
            R (T::*f)(Args...), T& obj, TSpan<FObject> vec, TIndexSequence<I...>) -> R {
            return (obj.*f)(vec[I].template As<Args>()...); // NOLINT
        }

        template <typename T, typename R, typename... Args>
        auto MemberFunctorInvokerWrapper(R (T::*f)(Args...), T& obj, TSpan<FObject> vec) -> R {
            if (ReflectionAssert(vec.Size() == sizeof...(Args),
                    EReflectionErrorCode::MismatchedArgumentNumber, FReflectionDumpData{}))
                [[likely]] {
                return MemberFunctorInvokerWrapperImpl(
                    f, obj, vec, Container::TIndexSequenceFor<Args...>{});
            }
            Utility::CompilerHint::Unreachable();
        }

        // Accessors
        template <auto Member>
            requires CMemberPointer<decltype(Member)>
        struct TAutoMemberAccessor : TMemberType<decltype(Member)> {
            using TSuper = TMemberType<decltype(Member)>;
            static auto Get(TSuper::TClassType& object) -> TSuper::TBaseType& {
                return object.*Member;
            }
            static auto GetAccessor() -> TFnMemberPropertyAccessor {
                return [](FObject& obj) -> FObject {
                    auto& classObject = obj.As<typename TSuper::TClassType>();
                    return FObject::CreateClone(Container::MakeRef(Get(classObject)));
                };
            }
        };
        template <auto Member>
            requires CMemberFunctionPointer<decltype(Member)>
        struct TAutoMemberFunctionAccessor : TMemberFunctionTrait<decltype(Member)> {
            using TSuper = TMemberFunctionTrait<decltype(Member)>;
            static auto GetInvoker() -> TFnMemberFunctionInvoker {
                return [](FObject& classObject, TSpan<FObject> args) -> FObject {
                    auto& obj = classObject.As<typename TSuper::TClassType>();
                    if constexpr (TTypeIsVoid_v<typename TSuper::TReturnType>) {
                        MemberFunctorInvokerWrapper(Member, obj, args);
                        return FObject::Create<void>();
                    } else {
                        return FObject::CreateClone(MemberFunctorInvokerWrapper(Member, obj, args));
                    }
                };
            };
        };

        template <typename TBase, typename TDerived>
            requires(CClassBaseOf<TBase, TDerived>)
        struct TPolymorphismInfo {
            static constexpr auto GetStaticUpCastWrapper() -> void* (*)(void*) {
                return [](void* ptr) -> void* {
                    return static_cast<void*>(static_cast<TBase*>(static_cast<TDerived*>(ptr)));
                };
            }
        };

    } // namespace Detail

    template <CDecayed T> void RegisterType() {
        Detail::RegisterType(GetRttiTypeInfo<T>(), FMetaTypeInfo::Create<T>());
    }

    template <CDecayed TBase, CDecayed TDerived>
        requires CClassBaseOf<TBase, TDerived>
    void RegisterPolymorphicRelation() {
        using TInheritanceInfo = Detail::TPolymorphismInfo<TBase, TDerived>;
        Detail::RegisterPolymorphicRelation(FMetaTypeInfo::Create<TBase>().GetHash(),
            FMetaTypeInfo::Create<TDerived>().GetHash(),
            TInheritanceInfo::GetStaticUpCastWrapper());
    }

    template <auto Member>
        requires CMemberPointer<decltype(Member)>
    void RegisterPropertyField(FNativeStringView name) {
        using TAccessor = Detail::TAutoMemberAccessor<Member>;
        auto propField  = FMetaPropertyInfo::Create<Member>();
        Detail::RegisterPropertyField(propField, name, TAccessor::GetAccessor());
    }

    template <auto Member>
        requires CMemberFunctionPointer<decltype(Member)>
    void RegisterMethodField(FNativeStringView name) {
        using TInvoker   = Detail::TAutoMemberFunctionAccessor<Member>;
        auto methodField = FMetaMethodInfo::Create<Member>();
        Detail::RegisterMethodField(methodField, name, TInvoker::GetInvoker());
    }

    inline auto ConstructObject(const FMetaTypeInfo& valueMeta) -> FObject {
        return Detail::ConstructObject(valueMeta.GetHash());
    }
    inline auto GetProperty(FObject& object, const FMetaPropertyInfo& propMeta) -> FObject {
        return Detail::GetProperty(
            object, propMeta.GetHash(), propMeta.GetClassTypeMetadata().GetHash());
    }
    inline auto InvokeMethod(
        FObject& object, const FMetaPropertyInfo& propMeta, TSpan<FObject> args) -> FObject {
        return Detail::InvokeMethod(object, propMeta.GetHash(), args);
    }

} // namespace AltinaEngine::Core::Reflection
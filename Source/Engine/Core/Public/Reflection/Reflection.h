#pragma once

#include "Container/Span.h"
#include "Reflection/Object.h"
#include "Container/IndexSequence.h"
#include "Reflection/ReflectionBase.h"
#include "Utility/CompilerHint.h"
#include "Container/Ref.h"

namespace AltinaEngine::Core::Reflection
{
    using Container::TIndexSequence;
    using Container::TSpan;

    namespace Detail
    {
        using TFnMemberFunctionInvoker  = FObject (*)(FObject&, TSpan<FObject>);
        using TFnMemberPropertyAccessor = FObject (*)(FObject&);

        template <typename T, typename R, typename... Args, usize... I>
        auto MemberFunctorInvokerWrapperImpl(R (T::*f)(Args...), T& obj, TSpan<FObject> vec, TIndexSequence<I...>) -> R
        {
            return (obj.*f)(
                vec[I].template As<Args>()...); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
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
            static auto Get(typename TSuper::TClassType& object) -> TSuper::TClassType& { return object.*Member; }
            static auto GetAccessor() -> TFnMemberPropertyAccessor
            {
                return [](FObject& obj) -> FObject {
                    auto& classObject = obj.As<typename TSuper::TClassType>();
                    return FObject::CreateClone(MakeRef(Get(classObject)));
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

    } // namespace Detail

} // namespace AltinaEngine::Core::Reflection
#pragma once
#include "Container/Span.h"
#include "Container/Ref.h"
#include "Container/IndexSequence.h"
#include "Container/StringView.h"
#include "Types/Meta.h"
namespace AltinaEngine::Core::Reflection
{
    using Container::FNativeStringView;
    using Container::MakeRef;
    using Container::TIndexSequence;
    using Container::TSpan;

    class FObject;
    namespace Detail
    {
        using namespace TypeMeta;
        using TFnMemberFunctionInvoker  = FObject (*)(FObject&, TSpan<FObject>);
        using TFnMemberPropertyAccessor = FObject (*)(FObject&);
        using TFnPolymorphismUpCaster   = void* (*)(void*);

        AE_CORE_API void RegisterType(const FTypeInfo& stdTypeInfo, const FMetaTypeInfo& meta);
        AE_CORE_API void RegisterPolymorphicRelation(
            FTypeMetaHash baseType, FTypeMetaHash derivedType, TFnPolymorphismUpCaster upCaster);
        AE_CORE_API void RegisterPropertyField(
            const FMetaPropertyInfo& propMeta, FNativeStringView name, TFnMemberPropertyAccessor accessor);
        AE_CORE_API void RegisterMethodField(
            const FMetaMethodInfo& methodMeta, FNativeStringView name, TFnMemberFunctionInvoker invoker);
        AE_CORE_API auto ConstructObject(FTypeMetaHash classHash) -> FObject;
        AE_CORE_API auto GetProperty(FObject& object, FTypeMetaHash propHash, FTypeMetaHash classHash) -> FObject;
        AE_CORE_API auto InvokeMethod(FObject& object, FTypeMetaHash methodHash, TSpan<FObject> args) -> FObject;
        AE_CORE_API auto TryChainedUpcast(void* ptr, FTypeMetaHash srcType, FTypeMetaHash dstType) -> void*;
    } // namespace Detail

} // namespace AltinaEngine::Core::Reflection
#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include "Base/AltinaBase.h"
#include "Types/Aliases.h"
#include "Container/Array.h"
#include "Algorithm/CompileTimeUtils/ArrayUtils.h"
#include "Container/StringView.h"
#include "Types/Traits.h"
#include "Types/RTTI.h"

namespace AltinaEngine::Core::TypeMeta
{
    namespace Detail
    {
        constexpr u64 kHashingMultiplier = 257;
        using Container::TArray;

        template <class T> consteval static auto GetFuncNameRaw() -> const char*
        {
#ifdef AE_COMPILER_MSVC
            return __FUNCSIG__;
#else
    #ifdef __PRETTY_FUNCTION__
            return __PRETTY_FUNCTION__;
    #else
            static_assert(false, "Unsupported compiler");
    #endif
#endif
        }
        template <auto T> consteval static auto GetVarNameRaw() -> const char*
        {
#ifdef AE_COMPILER_MSVC
            return __FUNCSIG__;
#else
    #ifdef __PRETTY_FUNCTION__
            return __PRETTY_FUNCTION__;
    #else
            static_assert(false, "Unsupported compiler");
    #endif
#endif
        }

        template <class T> consteval static auto GetFuncNameLength() -> u64
        {
            constexpr auto kRaw = GetFuncNameRaw<T>();
            u64            len  = 0;
            while (kRaw[len] != '\0')
                ++len;
            return len;
        }
        template <auto T> consteval static auto GetVarNameLength() -> u64
        {
            constexpr auto kRaw = GetVarNameRaw<T>();
            u64            len  = 0;
            while (kRaw[len] != '\0')
                ++len;
            return len;
        }
        template <class T> consteval static auto GetFuncNameToArray() -> TArray<char, GetFuncNameLength<T>() + 1>
        {
            using TRet          = TArray<char, GetFuncNameLength<T>() + 1>;
            constexpr auto kRaw = GetFuncNameRaw<T>();
            constexpr u64  kLen = GetFuncNameLength<T>();

            TRet           arr = {};
            for (u64 i = 0; i <= kLen; ++i)
            {
                arr[i] = kRaw[i];
            }
            return arr;
        }

        template <auto T> consteval static auto GetVarNameToArray() -> TArray<char, GetVarNameLength<T>() + 1>
        {
            using TRet          = TArray<char, GetVarNameLength<T>() + 1>;
            constexpr auto kRaw = GetVarNameRaw<T>();
            constexpr u64  kLen = GetVarNameLength<T>();

            TRet           arr = {};
            for (u64 i = 0; i <= kLen; ++i)
            {
                arr[i] = kRaw[i];
            }
            return arr;
        }
        template <unsigned N> consteval auto GetFuncNameHashImpl(TArray<char, N> const& str) -> u64
        {
            u64 hash = 0;
            for (u64 i = 0; i < str.Size(); ++i)
            {
                hash = (hash + str[i] + 1) * kHashingMultiplier;
            }
            return hash;
        }
        template <typename T> consteval static auto GetActualClassNameArray()
        {
            using namespace Core::Algorithm;
            constexpr auto kFunctionSignature = GetFuncNameToArray<T>();
            constexpr u64  kFirstPos          = GetOccurrencePosition(kFunctionSignature, '<', 0) + 1;
            constexpr u64  kLastPos           = GetLastOccurrencePosition(kFunctionSignature, '>') + 1;
            constexpr int  kFinalOffset       = kFirstPos + 0 + 0;
            constexpr auto kSubArray          = GetSubArray<kFinalOffset, kLastPos>(kFunctionSignature);
            return kSubArray;
        }

        template <auto T> consteval static auto GetActualVarNameArray()
        {
            constexpr auto kFunctionSignature = GetVarNameToArray<T>();
            constexpr u64  kFirstPos          = GetOccurrencePositionRefined(kFunctionSignature, '<', 0) + 1;
            constexpr u64  kLastPos           = GetLastOccurrencePositionRefined(kFunctionSignature, '>') + 1;
            constexpr int  kFinalOffset       = kFirstPos + 0 + 0;
            constexpr auto kSubArray          = GetSubArray<kFinalOffset, kLastPos>(kFunctionSignature);
            return kSubArray;
        }

        template <typename T> consteval static auto GetFuncNameHashId() -> u64
        {
            constexpr auto kArr = GetActualClassNameArray<T>();
            return GetFuncNameHashImpl(kArr);
        }
    } // namespace Detail

    using Container::TNativeStringView;
    template <class T> struct TMetaTypeInfo
    {

        static constexpr u64               kHash      = Detail::GetFuncNameHashId<T>();
        static constexpr auto              kNameArray = Detail::GetActualClassNameArray<T>();
        static constexpr TNativeStringView kName      = TNativeStringView(kNameArray.Data(), kNameArray.Size() - 1);
        static constexpr bool              kDefaultConstructible = TTypeIsDefaultConstructible_v<T>;

        static auto                        GetTypeInfo() -> FTypeInfo const&
        {
            static FTypeInfo const* typeInfo = &typeid(T);
            return *typeInfo;
        }
    };
} // namespace AltinaEngine::Core::TypeMeta
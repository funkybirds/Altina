#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include "Base/AltinaBase.h"
#include "Types/Aliases.h"
#include "Container/Array.h"
#include "Algorithm/CompileTimeUtils/ArrayUtils.h"
#include "Container/StringView.h"
#include "Types/Traits.h"
#include "Types/RTTI.h"

namespace AltinaEngine::Core::TypeMeta {
    using FTypeMetaHash = u64; // NOLINT

    namespace Detail {
        constexpr FTypeMetaHash kHashingMultiplier = 257;
        using Container::TArray;

        template <class T> consteval static auto GetFuncNameRaw() -> const char* {
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
        template <auto T> consteval static auto GetVarNameRaw() -> const char* {
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

        template <class T> consteval static auto GetFuncNameLength() -> u64 {
            constexpr auto kRaw = GetFuncNameRaw<T>();
            u64            len  = 0;
            while (kRaw[len] != '\0')
                ++len;
            return len;
        }
        template <auto T> consteval static auto GetVarNameLength() -> u64 {
            constexpr auto kRaw = GetVarNameRaw<T>();
            u64            len  = 0;
            while (kRaw[len] != '\0')
                ++len;
            return len;
        }
        template <class T>
        consteval static auto GetFuncNameToArray() -> TArray<char, GetFuncNameLength<T>() + 1> {
            using TRet          = TArray<char, GetFuncNameLength<T>() + 1>;
            constexpr auto kRaw = GetFuncNameRaw<T>();
            constexpr u64  kLen = GetFuncNameLength<T>();

            TRet           arr = {};
            for (u64 i = 0; i <= kLen; ++i) {
                arr[i] = kRaw[i];
            }
            return arr;
        }

        template <auto T>
        consteval static auto GetVarNameToArray() -> TArray<char, GetVarNameLength<T>() + 1> {
            using TRet          = TArray<char, GetVarNameLength<T>() + 1>;
            constexpr auto kRaw = GetVarNameRaw<T>();
            constexpr u64  kLen = GetVarNameLength<T>();

            TRet           arr = {};
            for (u64 i = 0; i <= kLen; ++i) {
                arr[i] = kRaw[i];
            }
            return arr;
        }
        template <unsigned N>
        consteval auto GetFuncNameHashImpl(TArray<char, N> const& str) -> FTypeMetaHash {
            FTypeMetaHash hash = 0;
            for (FTypeMetaHash i = 0; i < static_cast<FTypeMetaHash>(str.Size()); ++i) {
                hash = (hash + str[i] + 1) * kHashingMultiplier;
            }
            return hash;
        }
        template <typename T> consteval static auto GetActualClassNameArray() {
            using namespace Core::Algorithm;
            constexpr auto kFunctionSignature = GetFuncNameToArray<T>();
            constexpr u64  kFirstPos =
                Algorithm::GetOccurrencePosition<char, kFunctionSignature.Size()>(
                    kFunctionSignature, '<', 0)
                + 1;
            constexpr u64 kLastPos =
                Algorithm::GetLastOccurrencePosition<char, kFunctionSignature.Size()>(
                    kFunctionSignature, '>')
                + 1;
            constexpr int  kFinalOffset = kFirstPos + 0 + 0;
            constexpr auto kSubArray =
                GetSubArray<kFinalOffset, kLastPos, char, kFunctionSignature.Size()>(
                    kFunctionSignature);
            return kSubArray;
        }

        template <auto T> consteval static auto GetActualVarNameArray() {
            using namespace Core::Algorithm;
            constexpr auto kFunctionSignature = GetVarNameToArray<T>();
            constexpr u64  kFirstPos =
                Algorithm::GetOccurrencePosition<char, kFunctionSignature.Size()>(
                    kFunctionSignature, '<', 0)
                + 1;
            constexpr u64 kLastPos =
                Algorithm::GetLastOccurrencePosition<char, kFunctionSignature.Size()>(
                    kFunctionSignature, '>')
                + 1;
            constexpr int  kFinalOffset = kFirstPos + 0 + 0;
            constexpr auto kSubArray =
                GetSubArray<kFinalOffset, kLastPos, char, kFunctionSignature.Size()>(
                    kFunctionSignature);
            return kSubArray;
        }

        template <typename T> consteval static auto GetFuncNameHashId() -> FTypeMetaHash {
            constexpr auto kArr = GetActualClassNameArray<T>();
            return GetFuncNameHashImpl<kArr.Size()>(kArr);
        }
        template <auto T> consteval static auto GetVarNameHashId() -> FTypeMetaHash {
            constexpr auto kArr = GetActualVarNameArray<T>();
            return GetFuncNameHashImpl<kArr.Size()>(kArr);
        }
    } // namespace Detail

    using Container::FNativeStringView;
    template <class T> struct TMetaTypeInfo {
        static constexpr FTypeMetaHash kHash      = Detail::GetFuncNameHashId<T>();
        static constexpr auto          kNameArray = Detail::GetActualClassNameArray<T>();
        static constexpr auto kName = FNativeStringView(kNameArray.Data(), kNameArray.Size() - 1);
        static constexpr bool kDefaultConstructible = CDefaultConstructible<T>;
        static constexpr bool kDestructible         = CDestructible<T>;
        static constexpr bool kCopyConstructible    = CCopyConstructible<T>;

        static auto           GetTypeInfo() -> FTypeInfo const& {
            static FTypeInfo const* typeInfo = &typeid(T);
            return *typeInfo;
        }

        static auto InvokeDtor(void* p) {
            if constexpr (kDestructible)
                delete static_cast<T*>(p);
        }
        static auto InvokeCopyCtor(void* p) -> void* {
            if constexpr (kCopyConstructible)
                return static_cast<void*>(new T(*static_cast<T*>(p)));
            return nullptr;
        }
        static auto InvokeDefaultCtor() -> void* {
            if constexpr (kDefaultConstructible)
                return static_cast<void*>(new T());
            return nullptr;
        }
    };

    template <auto T>
        requires CMemberFunctionPointer<decltype(T)>
    struct TMetaMemberFunctionInfo {
        static constexpr FTypeMetaHash kHash      = Detail::GetVarNameHashId<T>();
        static constexpr auto          kNameArray = Detail::GetActualVarNameArray<T>();
        static constexpr auto kName = FNativeStringView(kNameArray.Data(), kNameArray.Size() - 1);

        using TReturnType = TMemberFunctionTrait<decltype(T)>::TReturnType;
        using TClassType  = TMemberFunctionTrait<decltype(T)>::TClassType;
        using TArgsTuple  = TMemberFunctionTrait<decltype(T)>::TArgsTuple;
    };

    template <auto T>
        requires CMemberPointer<decltype(T)>
    struct TMetaPropertyInfo {
        static constexpr FTypeMetaHash kHash      = Detail::GetVarNameHashId<T>();
        static constexpr auto          kNameArray = Detail::GetActualVarNameArray<T>();
        static constexpr auto kName = FNativeStringView(kNameArray.Data(), kNameArray.Size() - 1);

        using TBaseType  = TMemberType<decltype(T)>::TBaseType;
        using TClassType = TMemberType<decltype(T)>::TClassType;
    };

    struct FMetaTypeInfo {
        template <class T> static auto Create() -> FMetaTypeInfo {
            return FMetaTypeInfo(TMetaTypeInfo<T>::kDefaultConstructible,
                TMetaTypeInfo<T>::kCopyConstructible, TMetaTypeInfo<T>::kDestructible,
                TMetaTypeInfo<T>::kHash, TMetaTypeInfo<T>::kName, &TMetaTypeInfo<T>::GetTypeInfo,
                &TMetaTypeInfo<T>::InvokeDtor, &TMetaTypeInfo<T>::InvokeCopyCtor,
                &TMetaTypeInfo<T>::InvokeDefaultCtor);
        }
        static auto CreateVoid() {
            return FMetaTypeInfo(
                false, false, false, 0, FNativeStringView{},
                []() -> FTypeInfo const& {
                    static FTypeInfo const* typeInfo = &typeid(void);
                    return *typeInfo;
                },
                nullptr, nullptr, nullptr);
        }
        static auto CreatePlaceHolder() { return FMetaTypeInfo(); }

        friend struct FMetaPropertyInfo;
        friend struct FMetaMethodInfo;

        [[nodiscard]] auto GetHash() const noexcept -> FTypeMetaHash { return mHash; }
        [[nodiscard]] auto GetName() const noexcept -> FNativeStringView { return mName; }
        [[nodiscard]] auto GetTypeInfo() const noexcept -> FTypeInfo const& {
            return mGetTypeInfo();
        }
        [[nodiscard]] auto IsCopyConstructible() const noexcept -> bool {
            return mCopyConstructible;
        }
        [[nodiscard]] auto IsDestructible() const noexcept -> bool { return mDestructible; }

        void               CallDestructor(void* obj) const { mDestructor(obj); }
        [[nodiscard]] auto CallCopyConstructor(void* obj) const -> void* {
            return mCopyConstructor(obj);
        }
        [[nodiscard]] auto CallDefaultConstructor() const -> void* { return mDefaultConstructor(); }

        [[nodiscard]] auto operator==(const FMetaTypeInfo& p) const -> bool {
            return mHash == p.mHash;
        }

    private:
        FMetaTypeInfo()
            : mDefaultConstructible(false)
            , mCopyConstructible(false)
            , mDestructible(false)
            , mHash(0)
            , mGetTypeInfo(nullptr)
            , mDestructor(nullptr)
            , mCopyConstructor(nullptr)
            , mDefaultConstructor(nullptr) {}
        FMetaTypeInfo(bool bDefaultConstructible, bool bCopyConstructible, bool bDestructible,
            FTypeMetaHash hash, FNativeStringView name, FTypeInfo const& (*getTypeInfo)(),
            void (*destructor)(void*), void* (*copyCtor)(void*), void* (*defaultCtor)())
            : mDefaultConstructible(bDefaultConstructible)
            , mCopyConstructible(bCopyConstructible)
            , mDestructible(bDestructible)
            , mHash(hash)
            , mName(name)
            , mGetTypeInfo(getTypeInfo)
            , mDestructor(destructor)
            , mCopyConstructor(copyCtor)
            , mDefaultConstructor(defaultCtor) {}

        bool              mDefaultConstructible;
        bool              mCopyConstructible;
        bool              mDestructible;
        FTypeMetaHash     mHash;
        FNativeStringView mName;

        FTypeInfo const& (*mGetTypeInfo)();
        void (*mDestructor)(void*);
        void* (*mCopyConstructor)(void*);
        void* (*mDefaultConstructor)();
    };

    struct FMetaPropertyInfo {
        [[nodiscard]] auto GetHash() const noexcept -> FTypeMetaHash { return mHash; }
        [[nodiscard]] auto GetName() const noexcept -> FNativeStringView { return mName; }
        [[nodiscard]] auto GetPropertyTypeMetadata() const noexcept -> FMetaTypeInfo const& {
            return mMemberTypeInfo;
        }
        [[nodiscard]] auto GetClassTypeMetadata() const noexcept -> FMetaTypeInfo const& {
            return mClassTypeInfo;
        }

        template <auto Member>
            requires CMemberPointer<decltype(Member)>
        static auto Create() -> FMetaPropertyInfo {
            using TPropertyType = TMetaPropertyInfo<Member>::TBaseType;
            using TClassType    = TMetaPropertyInfo<Member>::TClassType;
            return FMetaPropertyInfo(FMetaTypeInfo::Create<TClassType>(),
                FMetaTypeInfo::Create<TPropertyType>(), TMetaPropertyInfo<Member>::kHash,
                TMetaPropertyInfo<Member>::kName);
        }

        static auto CreatePlaceHolder() {
            return FMetaPropertyInfo(FMetaTypeInfo::CreatePlaceHolder(),
                FMetaTypeInfo::CreatePlaceHolder(), 0, FNativeStringView{});
        }

    private:
        FMetaPropertyInfo(FMetaTypeInfo classTypeInfo, FMetaTypeInfo memberTypeInfo,
            FTypeMetaHash hash, FNativeStringView name)
            : mClassTypeInfo(classTypeInfo)
            , mMemberTypeInfo(memberTypeInfo)
            , mHash(hash)
            , mName(name) {}

        FMetaTypeInfo     mClassTypeInfo;
        FMetaTypeInfo     mMemberTypeInfo;
        FTypeMetaHash     mHash;
        FNativeStringView mName;
    };

    struct FMetaMethodInfo {
        [[nodiscard]] auto GetHash() const noexcept -> FTypeMetaHash { return mHash; }
        [[nodiscard]] auto GetName() const noexcept -> FNativeStringView { return mName; }
        [[nodiscard]] auto GetReturnTypeMetadata() const noexcept -> FMetaTypeInfo const& {
            return mReturnTypeInfo;
        }
        [[nodiscard]] auto GetClassTypeMetadata() const noexcept -> FMetaTypeInfo const& {
            return mClassTypeInfo;
        }

        template <auto Member>
            requires CMemberFunctionPointer<decltype(Member)>
        static auto Create() -> FMetaMethodInfo {
            using TReturnType = TMetaMemberFunctionInfo<Member>::TReturnType;
            using TClassType  = TMetaMemberFunctionInfo<Member>::TClassType;
            return FMetaMethodInfo(FMetaTypeInfo::Create<TClassType>(),
                FMetaTypeInfo::Create<TReturnType>(), TMetaMemberFunctionInfo<Member>::kHash,
                TMetaMemberFunctionInfo<Member>::kName);
        }

        static auto CreatePlaceHolder() {
            return FMetaMethodInfo(FMetaTypeInfo::CreatePlaceHolder(),
                FMetaTypeInfo::CreatePlaceHolder(), 0, FNativeStringView{});
        }

    private:
        FMetaMethodInfo(FMetaTypeInfo classTypeInfo, FMetaTypeInfo returnTypeInfo,
            FTypeMetaHash hash, FNativeStringView name)
            : mClassTypeInfo(classTypeInfo)
            , mReturnTypeInfo(returnTypeInfo)
            , mHash(hash)
            , mName(name) {}

        FMetaTypeInfo     mClassTypeInfo;
        FMetaTypeInfo     mReturnTypeInfo;
        FTypeMetaHash     mHash;
        FNativeStringView mName;
    };

} // namespace AltinaEngine::Core::TypeMeta
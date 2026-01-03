#include "Reflection/Reflection.h"
#include "Container/HashMap.h"
#include "Container/HashSet.h"
#include "Types/NonCopyable.h"
#include "Container/String.h"
#include "Utility/CompilerHint.h"

namespace AltinaEngine::Core::Reflection::Detail {
    using Container::FNativeString;
    using Container::THashMap;
    using Container::THashSet;
    using TStdHashType = decltype(Declval<FTypeInfo>().hash_code());

    struct FPropertyField {
        FNativeString             mName;
        FMetaPropertyInfo         mMeta;
        TFnMemberPropertyAccessor mAccessor;

        FPropertyField() : mMeta(FMetaPropertyInfo::CreatePlaceHolder()), mAccessor(nullptr) {}
        FPropertyField(FNativeStringView name, const FMetaPropertyInfo& meta,
            TFnMemberPropertyAccessor accessor)
            : mName(name), mMeta(meta), mAccessor(accessor) {}
    };

    struct FMethodField {
        FNativeString            mName;
        FMetaMethodInfo          mMeta;
        TFnMemberFunctionInvoker mInvoker;

        FMethodField() : mMeta(FMetaMethodInfo::CreatePlaceHolder()), mInvoker(nullptr) {}
        FMethodField(
            FNativeStringView name, const FMetaMethodInfo& meta, TFnMemberFunctionInvoker invoker)
            : mName(name), mMeta(meta), mInvoker(invoker) {}
    };

    struct FBaseTypeEntry {
        TFnPolymorphismUpCaster mUpCaster;
    };

    struct FReflectionTypeMetaInfo {
        FMetaTypeInfo                           mMeta;
        THashMap<FTypeMetaHash, FPropertyField> mProperties;
        THashMap<FTypeMetaHash, FMethodField>   mMethods;
        THashSet<FTypeMetaHash>                 mDerivedTypes;
        THashMap<FTypeMetaHash, FBaseTypeEntry> mBaseTypes;
        bool                                    mIsPolymorphic = false;

        FReflectionTypeMetaInfo() : mMeta(FMetaTypeInfo::CreatePlaceHolder()) {}
        [[nodiscard]] auto operator==(const FReflectionTypeMetaInfo& rhs) const -> bool {
            return mMeta == rhs.mMeta;
        }

        static auto CreateEntry(const FMetaTypeInfo& meta) -> FReflectionTypeMetaInfo {
            FReflectionTypeMetaInfo ret;
            ret.mMeta = meta;
            return ret;
        }
    };

    struct FDynamicReflectionManager : FNonMovableStruct {
        THashMap<u64, FReflectionTypeMetaInfo> mRegistry;
        THashMap<TStdHashType, u64>            mRttiIdMap;
    };

    [[nodiscard]] auto GetReflectionManager() -> FDynamicReflectionManager& {
        static FDynamicReflectionManager manager;
        return manager;
    }

    AE_CORE_API void RegisterType(const FTypeInfo& stdTypeInfo, const FMetaTypeInfo& meta) {
        auto&      manager  = GetReflectionManager();
        const auto metaHash = meta.GetHash();

        if (ReflectionAssert(!manager.mRegistry.HasKey(metaHash),
                EReflectionErrorCode::TypeHashConflict, FReflectionDumpData{})) [[likely]] {
            manager.mRttiIdMap[GetRttiTypeObjectHash(stdTypeInfo)] = metaHash;
            manager.mRegistry[metaHash] = FReflectionTypeMetaInfo::CreateEntry(meta);
        }
    }

    AE_CORE_API void RegisterPolymorphicRelation(
        FTypeMetaHash baseType, FTypeMetaHash derivedType, TFnPolymorphismUpCaster upCaster) {
        auto& manager            = GetReflectionManager();
        auto  bBaseRegistered    = ReflectionAssert(manager.mRegistry.HasKey(baseType),
                EReflectionErrorCode::TypeUnregistered, FReflectionDumpData{});
        auto  bDerivedRegistered = ReflectionAssert(manager.mRegistry.HasKey(derivedType),
             EReflectionErrorCode::TypeUnregistered, FReflectionDumpData{});
        if (bBaseRegistered && bDerivedRegistered) [[likely]] {
            auto& baseEntry    = manager.mRegistry[baseType];
            auto& derivedEntry = manager.mRegistry[derivedType];
            baseEntry.mDerivedTypes.Insert(derivedType);
            baseEntry.mIsPolymorphic          = true;
            derivedEntry.mBaseTypes[baseType] = FBaseTypeEntry(upCaster);
            derivedEntry.mIsPolymorphic       = true;

            // Copy base class properties to derived class so they can be accessed
            for (auto& [propHash, propField] : baseEntry.mProperties) {
                if (!derivedEntry.mProperties.HasKey(propHash)) {
                    derivedEntry.mProperties[propHash] = propField;
                }
            }
        }
    }

    AE_CORE_API void RegisterPropertyField(const FMetaPropertyInfo& propMeta,
        FNativeStringView name, TFnMemberPropertyAccessor accessor) {
        auto& manager           = GetReflectionManager();
        auto  classTypeMetaHash = propMeta.GetClassTypeMetadata().GetHash();
        if (!ReflectionAssert(manager.mRegistry.HasKey(classTypeMetaHash),
                EReflectionErrorCode::TypeUnregistered, FReflectionDumpData{})) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto& tpMeta   = manager.mRegistry[classTypeMetaHash];
        auto  propHash = propMeta.GetHash();
        if (ReflectionAssert(!tpMeta.mProperties.HasKey(propHash),
                EReflectionErrorCode::TypeHashConflict, FReflectionDumpData{})) [[likely]] {
            tpMeta.mProperties[propHash] = FPropertyField(name, propMeta, accessor);
            return;
        }
        Utility::CompilerHint::Unreachable();
    }
    AE_CORE_API void RegisterMethodField(const FMetaMethodInfo& methodMeta, FNativeStringView name,
        TFnMemberFunctionInvoker invoker) {
        auto& manager           = GetReflectionManager();
        auto  classTypeMetaHash = methodMeta.GetClassTypeMetadata().GetHash();
        if (!ReflectionAssert(manager.mRegistry.HasKey(classTypeMetaHash),
                EReflectionErrorCode::TypeUnregistered, FReflectionDumpData{})) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto& tpMeta     = manager.mRegistry[classTypeMetaHash];
        auto  methodHash = methodMeta.GetHash();
        if (ReflectionAssert(!tpMeta.mMethods.HasKey(methodHash),
                EReflectionErrorCode::TypeHashConflict, FReflectionDumpData{})) [[likely]] {
            tpMeta.mMethods[methodHash] = FMethodField(name, methodMeta, invoker);
            return;
        }
        Utility::CompilerHint::Unreachable();
    }

    AE_CORE_API auto ConstructObject(FTypeMetaHash classHash) -> FObject {
        auto& manager = GetReflectionManager();
        if (ReflectionAssert(manager.mRegistry.HasKey(classHash),
                EReflectionErrorCode::TypeUnregistered, FReflectionDumpData{})) [[likely]] {
            const auto& tpMeta = manager.mRegistry[classHash];
            void*       objPtr = tpMeta.mMeta.CallDefaultConstructor();
            auto        obj    = FObject::CreateFromMetadata(objPtr, tpMeta.mMeta);
            return obj;
        }
        Utility::CompilerHint::Unreachable();
    }
    AE_CORE_API auto GetProperty(FObject& object, FTypeMetaHash propHash, FTypeMetaHash classHash)
        -> FObject {
        auto& manager = GetReflectionManager();
        if (!ReflectionAssert(manager.mRegistry.HasKey(classHash),
                EReflectionErrorCode::TypeUnregistered, FReflectionDumpData{})) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto& tpMeta = manager.mRegistry[classHash];
        if (!ReflectionAssert(tpMeta.mProperties.HasKey(propHash),
                EReflectionErrorCode::PropertyUnregistered, FReflectionDumpData{})) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto& entry = tpMeta.mProperties[propHash];
        return entry.mAccessor(object);
    }
    AE_CORE_API auto InvokeMethod(FObject& object, FTypeMetaHash methodHash, TSpan<FObject> args)
        -> FObject {
        auto& manager   = GetReflectionManager();
        auto  classHash = object.GetTypeHash();
        if (!ReflectionAssert(manager.mRegistry.HasKey(classHash),
                EReflectionErrorCode::TypeUnregistered, FReflectionDumpData{})) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto& tpMeta = manager.mRegistry[classHash];
        if (!ReflectionAssert(tpMeta.mMethods.HasKey(methodHash),
                EReflectionErrorCode::PropertyUnregistered, FReflectionDumpData{})) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto& entry = tpMeta.mMethods[methodHash];
        return entry.mInvoker(object, args);
    }

    AE_CORE_API auto TryChainedUpcast(void* ptr, FTypeMetaHash srcType, FTypeMetaHash dstType)
        -> void* {
        if (ptr == nullptr) [[unlikely]]
            return nullptr;

        auto& manager = GetReflectionManager();
        if (srcType == dstType)
            return ptr;
        if (!manager.mRegistry.HasKey(srcType))
            return nullptr;
        auto& tpEntry = manager.mRegistry[srcType];
        void* ret     = nullptr;
        for (const auto& [baseHash, entry] : tpEntry.mBaseTypes) {
            const auto upcasted = entry.mUpCaster(ptr);
            if (const auto rPtr = TryChainedUpcast(upcasted, baseHash, dstType)) {
                ret = rPtr;
                break;
            }
        }
        return ret;
    }

} // namespace AltinaEngine::Core::Reflection::Detail
#include "Reflection/Reflection.h"
#include "Reflection/Serializer.h"
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
        auto&               manager  = GetReflectionManager();
        const auto          metaHash = meta.GetHash();

        FReflectionDumpData dump;
        dump.mTypeInfo = &meta;
        dump.mTypeHash = metaHash;
        if (ReflectionAssert(!manager.mRegistry.HasKey(metaHash),
                EReflectionErrorCode::TypeHashConflict, dump)) [[likely]] {
            manager.mRttiIdMap[GetRttiTypeObjectHash(stdTypeInfo)] = metaHash;
            manager.mRegistry[metaHash] = FReflectionTypeMetaInfo::CreateEntry(meta);
        }
    }

    AE_CORE_API void RegisterPolymorphicRelation(
        FTypeMetaHash baseType, FTypeMetaHash derivedType, TFnPolymorphismUpCaster upCaster) {
        auto&               manager = GetReflectionManager();
        FReflectionDumpData baseDump;
        baseDump.mTypeHash   = baseType;
        auto bBaseRegistered = ReflectionAssert(
            manager.mRegistry.HasKey(baseType), EReflectionErrorCode::TypeUnregistered, baseDump);

        FReflectionDumpData derivedDump;
        derivedDump.mTypeHash   = derivedType;
        auto bDerivedRegistered = ReflectionAssert(manager.mRegistry.HasKey(derivedType),
            EReflectionErrorCode::TypeUnregistered, derivedDump);
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
        auto&               manager           = GetReflectionManager();
        auto                classTypeMetaHash = propMeta.GetClassTypeMetadata().GetHash();
        FReflectionDumpData typeDump;
        typeDump.mTypeHash     = classTypeMetaHash;
        typeDump.mPropertyInfo = &propMeta;
        typeDump.mPropertyHash = propMeta.GetHash();
        if (!ReflectionAssert(manager.mRegistry.HasKey(classTypeMetaHash),
                EReflectionErrorCode::TypeUnregistered, typeDump)) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto&               tpMeta   = manager.mRegistry[classTypeMetaHash];
        auto                propHash = propMeta.GetHash();
        FReflectionDumpData propDump;
        propDump.mTypeHash     = classTypeMetaHash;
        propDump.mPropertyInfo = &propMeta;
        propDump.mPropertyHash = propHash;
        if (ReflectionAssert(!tpMeta.mProperties.HasKey(propHash),
                EReflectionErrorCode::TypeHashConflict, propDump)) [[likely]] {
            tpMeta.mProperties[propHash] = FPropertyField(name, propMeta, accessor);
            return;
        }
        Utility::CompilerHint::Unreachable();
    }
    AE_CORE_API void RegisterMethodField(const FMetaMethodInfo& methodMeta, FNativeStringView name,
        TFnMemberFunctionInvoker invoker) {
        auto&               manager           = GetReflectionManager();
        auto                classTypeMetaHash = methodMeta.GetClassTypeMetadata().GetHash();
        FReflectionDumpData typeDump;
        typeDump.mTypeHash   = classTypeMetaHash;
        typeDump.mMethodInfo = &methodMeta;
        typeDump.mMethodHash = methodMeta.GetHash();
        if (!ReflectionAssert(manager.mRegistry.HasKey(classTypeMetaHash),
                EReflectionErrorCode::TypeUnregistered, typeDump)) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto&               tpMeta     = manager.mRegistry[classTypeMetaHash];
        auto                methodHash = methodMeta.GetHash();
        FReflectionDumpData methodDump;
        methodDump.mTypeHash   = classTypeMetaHash;
        methodDump.mMethodInfo = &methodMeta;
        methodDump.mMethodHash = methodHash;
        if (ReflectionAssert(!tpMeta.mMethods.HasKey(methodHash),
                EReflectionErrorCode::TypeHashConflict, methodDump)) [[likely]] {
            tpMeta.mMethods[methodHash] = FMethodField(name, methodMeta, invoker);
            return;
        }
        Utility::CompilerHint::Unreachable();
    }

    AE_CORE_API auto ConstructObject(FTypeMetaHash classHash) -> FObject {
        auto&               manager = GetReflectionManager();
        FReflectionDumpData dump;
        dump.mTypeHash = classHash;
        if (ReflectionAssert(manager.mRegistry.HasKey(classHash),
                EReflectionErrorCode::TypeUnregistered, dump)) [[likely]] {
            const auto& tpMeta = manager.mRegistry[classHash];
            void*       objPtr = tpMeta.mMeta.CallDefaultConstructor();
            auto        obj    = FObject::CreateFromMetadata(objPtr, tpMeta.mMeta);
            return obj;
        }
        Utility::CompilerHint::Unreachable();
    }
    AE_CORE_API auto GetProperty(FObject& object, FTypeMetaHash propHash, FTypeMetaHash classHash)
        -> FObject {
        auto&               manager = GetReflectionManager();
        FReflectionDumpData typeDump;
        typeDump.mTypeHash       = classHash;
        typeDump.mPropertyHash   = propHash;
        typeDump.mObjectTypeHash = object.GetTypeHash();
        if (!ReflectionAssert(manager.mRegistry.HasKey(classHash),
                EReflectionErrorCode::TypeUnregistered, typeDump)) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto&               tpMeta = manager.mRegistry[classHash];
        FReflectionDumpData propDump;
        propDump.mTypeHash       = classHash;
        propDump.mPropertyHash   = propHash;
        propDump.mObjectTypeHash = object.GetTypeHash();
        if (!ReflectionAssert(tpMeta.mProperties.HasKey(propHash),
                EReflectionErrorCode::PropertyUnregistered, propDump)) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto& entry = tpMeta.mProperties[propHash];
        return entry.mAccessor(object);
    }
    AE_CORE_API auto InvokeMethod(FObject& object, FTypeMetaHash methodHash, TSpan<FObject> args)
        -> FObject {
        auto&               manager   = GetReflectionManager();
        auto                classHash = object.GetTypeHash();
        FReflectionDumpData typeDump;
        typeDump.mTypeHash       = classHash;
        typeDump.mMethodHash     = methodHash;
        typeDump.mObjectTypeHash = object.GetTypeHash();
        if (!ReflectionAssert(manager.mRegistry.HasKey(classHash),
                EReflectionErrorCode::TypeUnregistered, typeDump)) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }
        auto&               tpMeta = manager.mRegistry[classHash];
        FReflectionDumpData methodDump;
        methodDump.mTypeHash       = classHash;
        methodDump.mMethodHash     = methodHash;
        methodDump.mObjectTypeHash = object.GetTypeHash();
        if (!ReflectionAssert(tpMeta.mMethods.HasKey(methodHash),
                EReflectionErrorCode::PropertyUnregistered, methodDump)) [[unlikely]] {
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

    AE_CORE_API void DynamicSerializeInvokerImpl(void* ptr, ISerializer& serializer, u64 hash) {
        auto&               manager = GetReflectionManager();

        // Check if hash is registered
        FReflectionDumpData typeDump;
        typeDump.mTypeHash  = hash;
        typeDump.mObjectPtr = ptr;
        if (!ReflectionAssert(manager.mRegistry.HasKey(hash),
                EReflectionErrorCode::TypeUnregistered, typeDump)) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }

        const auto& tpMeta = manager.mRegistry[hash];

        // Construct FObject from void* and hash
        FObject     obj = FObject::CreateFromMetadata(ptr, tpMeta.mMeta);
        serializer.BeginObject("object");
        serializer.BeginObject("object_data");
        serializer.WriteFieldName("AE_REFLHASH");
        serializer.Write(hash);
        serializer.EndObject();
        for (const auto& [propHash, propField] : tpMeta.mProperties) {
            serializer.BeginObject(propField.mName);
            FObject propValue = propField.mAccessor(obj);
            propValue.Serialize(serializer);
            serializer.EndObject();
        }
        serializer.EndObject();
    }

    AE_CORE_API void DynamicDeserializeInvokerImpl(
        void* ptr, IDeserializer& deserializer, u64 hash) {
        auto&               manager = GetReflectionManager();

        // Check if hash is registered
        FReflectionDumpData typeDump;
        typeDump.mTypeHash  = hash;
        typeDump.mObjectPtr = ptr;
        if (!ReflectionAssert(manager.mRegistry.HasKey(hash),
                EReflectionErrorCode::TypeUnregistered, typeDump)) [[unlikely]] {
            Utility::CompilerHint::Unreachable();
        }

        const auto& tpMeta = manager.mRegistry[hash];

        // Construct FObject from void* and hash
        FObject     obj = FObject::CreateFromMetadata(ptr, tpMeta.mMeta);

        // Begin object twice (matching serialization)
        deserializer.BeginObject();
        deserializer.BeginObject();

        // Read field AE_REFLHASH and verify type hash
        if (deserializer.TryReadFieldName("AE_REFLHASH")) {
            u64                 readHash = deserializer.Read<u64>();
            FReflectionDumpData mismatchDump;
            mismatchDump.mTypeHash         = hash;
            mismatchDump.mExpectedTypeHash = hash;
            mismatchDump.mReadTypeHash     = readHash;
            mismatchDump.mObjectPtr        = ptr;
            ReflectionAssert(
                readHash == hash, EReflectionErrorCode::ObjectAndTypeMismatch, mismatchDump);
        }

        // End object once
        deserializer.EndObject();

        // Deserialize all properties
        for (const auto& [propHash, propField] : tpMeta.mProperties) {
            deserializer.BeginObject();
            FObject propValue = propField.mAccessor(obj);
            propValue.Deserialize(deserializer);
            deserializer.EndObject();
        }
        deserializer.EndObject();
    }

} // namespace AltinaEngine::Core::Reflection::Detail

namespace AltinaEngine::Core::Reflection {
    using Container::FString;
    using Container::TVector;

    AE_CORE_API auto GetAllProperties(FObject& object) -> TVector<FPropertyDesc> {
        using Detail::GetReflectionManager;

        auto&                  manager   = GetReflectionManager();
        auto                   classHash = object.GetTypeHash();
        TVector<FPropertyDesc> result;

        FReflectionDumpData    dump;
        dump.mTypeHash       = classHash;
        dump.mObjectTypeHash = classHash;
        if (!ReflectionAssert(manager.mRegistry.HasKey(classHash),
                EReflectionErrorCode::TypeUnregistered, dump)) [[unlikely]] {
            return result;
        }

        const auto& tpMeta = manager.mRegistry[classHash];
        result.Reserve(tpMeta.mProperties.size());

        for (const auto& [propHash, propField] : tpMeta.mProperties) {
            FObject propObject = propField.mAccessor(object);
            result.PushBack(FPropertyDesc(FString(propField.mName), Move(propObject)));
        }

        return result;
    }

} // namespace AltinaEngine::Core::Reflection

#include "Reflection/Reflection.h"
#include "Container/HashMap.h"
#include "Container/HashSet.h"
#include "Types/NonCopyable.h"
#include "Container/String.h"
namespace AltinaEngine::Core::Reflection::Detail
{
    using Container::FNativeString;
    using Container::THashMap;
    using Container::THashSet;
    using TStdHashType = decltype(Declval<FTypeInfo>().hash_code());

    struct FPropertyField
    {
        FNativeString             mName;
        FMetaPropertyInfo         mMeta;
        TFnMemberPropertyAccessor mAccessor;

        FPropertyField() : mMeta(FMetaPropertyInfo::CreatePlaceHolder()), mAccessor(nullptr) {}
        FPropertyField(FNativeStringView name, const FMetaPropertyInfo& meta, TFnMemberPropertyAccessor accessor)
            : mName(name), mMeta(meta), mAccessor(accessor)
        {
        }
    };

    struct FMethodField
    {
    };

    struct FReflectionTypeMetaInfo
    {
        FMetaTypeInfo                           mMeta;
        THashMap<FTypeMetaHash, FPropertyField> mProperties;
        THashMap<FTypeMetaHash, FMethodField>   mMethods;
        THashSet<FTypeMetaHash>                 mDerivedTypes;
        THashSet<FTypeMetaHash>                 mBaseTypes;
        bool                                    mIsPolymorphic = false;

        FReflectionTypeMetaInfo() : mMeta(FMetaTypeInfo::CreatePlaceHolder()) {}
        [[nodiscard]] auto operator==(const FReflectionTypeMetaInfo& rhs) const -> bool { return mMeta == rhs.mMeta; }

        static auto        CreateEntry(const FMetaTypeInfo& meta) -> FReflectionTypeMetaInfo
        {
            FReflectionTypeMetaInfo ret;
            ret.mMeta = meta;
            return ret;
        }
    };

    struct FDynamicReflectionManager : FNonMovableStruct
    {
        THashMap<u64, FReflectionTypeMetaInfo> mRegistry;
        THashMap<TStdHashType, u64>            mRttiIdMap;
    };

    [[nodiscard]] auto GetReflectionManager() -> FDynamicReflectionManager&
    {
        static FDynamicReflectionManager manager;
        return manager;
    }

    AE_CORE_API void RegisterType(const FTypeInfo& stdTypeInfo, const FMetaTypeInfo& meta)
    {
        auto&      manager  = GetReflectionManager();
        const auto metaHash = meta.GetHash();

        if (ReflectionAssert(!manager.mRegistry.HasKey(metaHash), EReflectionErrorCode::TypeHashConflict,
                FReflectionDumpData{})) [[likely]]
        {
            manager.mRttiIdMap[GetRttiTypeObjectHash(stdTypeInfo)] = metaHash;
            manager.mRegistry[metaHash]                            = FReflectionTypeMetaInfo::CreateEntry(meta);
        }
    }

    AE_CORE_API void RegisterPolymorphicRelation(FTypeMetaHash baseType, FTypeMetaHash derivedType)
    {
        auto& manager         = GetReflectionManager();
        auto  bBaseRegistered = ReflectionAssert(
            manager.mRegistry.HasKey(baseType), EReflectionErrorCode::TypeUnregistered, FReflectionDumpData{});
        auto bDerivedRegistered = ReflectionAssert(
            manager.mRegistry.HasKey(baseType), EReflectionErrorCode::TypeUnregistered, FReflectionDumpData{});
        if (bBaseRegistered && bDerivedRegistered) [[likely]]
        {
            auto& baseEntry    = manager.mRegistry[baseType];
            auto& derivedEntry = manager.mRegistry[derivedType];
            baseEntry.mDerivedTypes.Insert(derivedType);
            baseEntry.mIsPolymorphic = true;
            derivedEntry.mBaseTypes.Insert(baseType);
            derivedEntry.mIsPolymorphic = true;
        }
    }

    AE_CORE_API void RegisterPropertyField(
        const FMetaPropertyInfo& propMeta, FNativeStringView name, TFnMemberPropertyAccessor accessor)
    {
        auto& manager           = GetReflectionManager();
        auto  classTypeMetaHash = propMeta.GetClassTypeMetadata().GetHash();
        if (!ReflectionAssert(manager.mRegistry.HasKey(classTypeMetaHash), EReflectionErrorCode::TypeUnregistered,
                FReflectionDumpData{})) [[unlikely]]
        {
            Utility::CompilerHint::Unreachable();
        }
        auto& tpMeta   = manager.mRegistry[classTypeMetaHash];
        auto  propHash = propMeta.GetHash();
        if (ReflectionAssert(!tpMeta.mProperties.HasKey(propHash), EReflectionErrorCode::TypeHashConflict,
                FReflectionDumpData{})) [[likely]]
        {
            tpMeta.mProperties[propHash] = FPropertyField(name, propMeta, accessor);
            return;
        }
        Utility::CompilerHint::Unreachable();
    }

    AE_CORE_API auto ConstructObject(FTypeMetaHash classHash) -> FObject
    {
        auto& manager = GetReflectionManager();
        if (ReflectionAssert(manager.mRegistry.HasKey(classHash), EReflectionErrorCode::TypeUnregistered,
                FReflectionDumpData{})) [[likely]]
        {
            const auto& tpMeta = manager.mRegistry[classHash];
            void*       objPtr = tpMeta.mMeta.CallDefaultConstructor();
            auto        obj    = FObject::CreateFromMetadata(objPtr, tpMeta.mMeta);
            return obj;
        }
        Utility::CompilerHint::Unreachable();
    }
    AE_CORE_API auto GetProperty(FObject& object, FTypeMetaHash propHash, FTypeMetaHash classHash) -> FObject
    {
        auto&      manager         = GetReflectionManager();
        const auto actualClassHash = object.GetTypeHash();
        if (!ReflectionAssert(classHash == actualClassHash, EReflectionErrorCode::ObjectAndTypeMismatch,
                FReflectionDumpData{})) [[unlikely]]
        {
            Utility::CompilerHint::Unreachable();
        }
        if (!ReflectionAssert(manager.mRegistry.HasKey(classHash), EReflectionErrorCode::TypeUnregistered,
                FReflectionDumpData{})) [[unlikely]]
        {
            Utility::CompilerHint::Unreachable();
        }
        auto& tpMeta = manager.mRegistry[classHash];
        if (!ReflectionAssert(tpMeta.mProperties.HasKey(propHash), EReflectionErrorCode::PropertyUnregistered,
                FReflectionDumpData{})) [[unlikely]]
        {
            Utility::CompilerHint::Unreachable();
        }
        auto& entry = tpMeta.mProperties[propHash];
        return entry.mAccessor(object);
    }

} // namespace AltinaEngine::Core::Reflection::Detail
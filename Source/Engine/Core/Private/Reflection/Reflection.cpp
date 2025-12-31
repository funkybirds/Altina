#include "Reflection/Reflection.h"
#include "Container/HashMap.h"
#include "Container/HashSet.h"
#include "Types/NonCopyable.h"
namespace AltinaEngine::Core::Reflection::Detail
{
    using Container::THashMap;
    using Container::THashSet;
    using TStdHashType = decltype(Declval<FTypeInfo>().hash_code());

    struct FPropertyField
    {
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

    AE_CORE_API void RegisterPropertyField(const FMetaTypeInfo& valueMeta, FNativeStringView name,
        TFnMemberPropertyAccessor accessor, FTypeMetaHash classTypeMetaHash)
    {
        (void)valueMeta;
        (void)name;
        (void)accessor;
        (void)classTypeMetaHash;
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

} // namespace AltinaEngine::Core::Reflection::Detail
#include "Reflection/Reflection.h"
#include "Container/HashMap.h"
#include "Types/NonCopyable.h"
namespace AltinaEngine::Core::Reflection::Detail
{
    using Container::THashMap;
    using TStdHashType = decltype(Declval<FTypeInfo>().hash_code());

    struct FPropertyField
    {
    };

    struct FMethodField
    {
    };

    struct FReflectionTypeMetaInfo
    {
        FMetaTypeInfo                 mMeta;
        THashMap<u64, FPropertyField> mProperties;
        THashMap<u64, FMethodField>   mMethods;

        [[nodiscard]] auto operator==(const FReflectionTypeMetaInfo& rhs) const -> bool { return mMeta == rhs.mMeta; }
    };

    struct FDynamicReflectionManager
    {
        THashMap<u64, FReflectionTypeMetaInfo> mRegistry;
        THashMap<TStdHashType, u64>            mRttiIdMap;
    };

    [[nodiscard]] auto GetReflectionManager() -> FDynamicReflectionManager&
    {
        static FDynamicReflectionManager manager;
        return manager;
    }

} // namespace AltinaEngine::Core::Reflection::Detail
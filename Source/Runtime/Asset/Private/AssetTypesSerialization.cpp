#include "Asset/AssetTypes.h"

#include "Reflection/Serializer.h"

namespace AltinaEngine::Asset {
    void FAssetHandle::Serialize(Core::Reflection::ISerializer& serializer) const {
        serializer.Write(static_cast<u8>(mType));
        mUuid.Serialize(serializer);
    }

    auto FAssetHandle::Deserialize(Core::Reflection::IDeserializer& deserializer) -> FAssetHandle {
        FAssetHandle handle{};
        handle.mType = static_cast<EAssetType>(deserializer.Read<u8>());
        handle.mUuid = FUuid::Deserialize(deserializer);
        return handle;
    }
} // namespace AltinaEngine::Asset

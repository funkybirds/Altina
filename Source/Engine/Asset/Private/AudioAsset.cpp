#include "Asset/AudioAsset.h"

#include "Types/Traits.h"

namespace AltinaEngine::Asset {
    FAudioAsset::FAudioAsset(
        FAudioRuntimeDesc desc, TVector<FAudioChunkDesc> chunks, TVector<u8> data)
        : mDesc(desc), mChunks(AltinaEngine::Move(chunks)), mData(AltinaEngine::Move(data)) {}

} // namespace AltinaEngine::Asset

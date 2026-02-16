#include "Asset/AudioAsset.h"

#include "Types/Traits.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    FAudioAsset::FAudioAsset(
        FAudioRuntimeDesc desc, TVector<FAudioChunkDesc> chunks, TVector<u8> data)
        : mDesc(desc), mChunks(Move(chunks)), mData(Move(data)) {}

} // namespace AltinaEngine::Asset

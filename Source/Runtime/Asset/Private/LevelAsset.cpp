#include "Asset/LevelAsset.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    FLevelAsset::FLevelAsset(u32 encoding, TVector<u8> payload)
        : mEncoding(encoding), mPayload(Move(payload)) {}
} // namespace AltinaEngine::Asset

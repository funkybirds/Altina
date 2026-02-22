#include "Asset/ShaderAsset.h"

#include "Types/Traits.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    FShaderAsset::FShaderAsset(u32 language, FNativeString source)
        : mLanguage(language)
        , mSource(Move(source)) {}
} // namespace AltinaEngine::Asset

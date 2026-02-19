#pragma once

#include "AssetAPI.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"
#include "Utility/Uuid.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::FNativeString;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;

    enum class EAssetType : u8 {
        Unknown = 0,
        Texture2D,
        Mesh,
        Material,
        Audio,
        Script,
        Redirector,
    };

    struct AE_ASSET_API FAssetHandle {
        FUuid                        Uuid;
        EAssetType                   Type = EAssetType::Unknown;

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool {
            return !Uuid.IsNil() && Type != EAssetType::Unknown;
        }

        [[nodiscard]] constexpr auto operator==(const FAssetHandle& other) const noexcept -> bool {
            return Uuid == other.Uuid && Type == other.Type;
        }

        [[nodiscard]] constexpr auto operator!=(const FAssetHandle& other) const noexcept -> bool {
            return !(*this == other);
        }
    };

    struct AE_ASSET_API FAssetRedirector {
        FUuid   OldUuid;
        FUuid   NewUuid;
        FString OldVirtualPath;
    };

    struct AE_ASSET_API FTexture2DDesc {
        u32  Width    = 0;
        u32  Height   = 0;
        u32  MipCount = 0;
        u32  Format   = 0;
        bool SRGB     = true;
    };

    struct AE_ASSET_API FMeshDesc {
        u32 VertexFormat = 0;
        u32 IndexFormat  = 0;
        u32 SubMeshCount = 0;
    };

    struct AE_ASSET_API FMaterialDesc {
        u32                   ShadingModel = 0;
        u32                   BlendMode    = 0;
        u32                   Flags        = 0;
        f32                   AlphaCutoff  = 0.0f;
        TVector<FAssetHandle> TextureBindings;
    };

    struct AE_ASSET_API FAudioDesc {
        u32 Codec           = 0;
        u32 Channels        = 0;
        u32 SampleRate      = 0;
        f32 DurationSeconds = 0.0f;
    };

    struct AE_ASSET_API FScriptDesc {
        FNativeString AssemblyPath;
        FNativeString TypeName;
    };

    struct AE_ASSET_API FAssetDesc {
        FAssetHandle          Handle;
        FString               VirtualPath;
        FString               CookedPath;
        TVector<FAssetHandle> Dependencies;

        FTexture2DDesc        Texture;
        FMeshDesc             Mesh;
        FMaterialDesc         Material;
        FAudioDesc            Audio;
        FScriptDesc           Script;
    };

} // namespace AltinaEngine::Asset

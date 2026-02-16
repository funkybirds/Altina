#pragma once

#include "Engine/EngineAPI.h"
#include "Types/Aliases.h"
#include "Types/Meta.h"

namespace AltinaEngine::GameScene {
    using FComponentTypeHash = Core::TypeMeta::FTypeMetaHash;

    /**
     * @brief Opaque identifier for a game object inside a world.
     */
    struct AE_ENGINE_API FGameObjectId {
        u32 Index      = 0;
        u32 Generation = 0;
        u32 WorldId    = 0;

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return Generation != 0; }

        [[nodiscard]] constexpr auto operator==(const FGameObjectId& other) const noexcept
            -> bool {
            return Index == other.Index && Generation == other.Generation && WorldId == other.WorldId;
        }

        [[nodiscard]] constexpr auto operator!=(const FGameObjectId& other) const noexcept
            -> bool {
            return !(*this == other);
        }
    };

    /**
     * @brief Opaque identifier for a component instance.
     */
    struct AE_ENGINE_API FComponentId {
        u32               Index      = 0;
        u32               Generation = 0;
        FComponentTypeHash Type      = 0;

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool {
            return Generation != 0 && Type != 0;
        }

        [[nodiscard]] constexpr auto operator==(const FComponentId& other) const noexcept
            -> bool {
            return Index == other.Index && Generation == other.Generation && Type == other.Type;
        }

        [[nodiscard]] constexpr auto operator!=(const FComponentId& other) const noexcept
            -> bool {
            return !(*this == other);
        }
    };

    struct FGameObjectIdHash {
        auto operator()(const FGameObjectId& id) const noexcept -> usize {
            const u64 a = static_cast<u64>(id.Index);
            const u64 b = static_cast<u64>(id.Generation);
            const u64 c = static_cast<u64>(id.WorldId);

            u64       h = a;
            h = (h ^ (b + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U)));
            h = (h ^ (c + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U)));
            return static_cast<usize>(h);
        }
    };

    struct FComponentIdHash {
        auto operator()(const FComponentId& id) const noexcept -> usize {
            const u64 a = static_cast<u64>(id.Index);
            const u64 b = static_cast<u64>(id.Generation);
            const u64 c = static_cast<u64>(id.Type);

            u64       h = a;
            h = (h ^ (b + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U)));
            h = (h ^ (c + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U)));
            return static_cast<usize>(h);
        }
    };

    template <typename T> [[nodiscard]] inline auto GetComponentTypeHash() -> FComponentTypeHash {
        static const FComponentTypeHash kHash =
            Core::TypeMeta::FMetaTypeInfo::Create<T>().GetHash();
        return kHash;
    }
} // namespace AltinaEngine::GameScene






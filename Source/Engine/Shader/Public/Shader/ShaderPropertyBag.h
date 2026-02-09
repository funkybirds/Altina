#pragma once

#include "Shader/ShaderReflection.h"
#include "ShaderAPI.h"
#include "Container/HashMap.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Shader {
    class AE_SHADER_API FShaderPropertyBag {
    public:
        struct FPropertyDesc {
            u32 mOffset        = 0U;
            u32 mSize          = 0U;
            u32 mElementCount  = 0U;
            u32 mElementStride = 0U;
        };

        FShaderPropertyBag() = default;
        explicit FShaderPropertyBag(const FShaderConstantBuffer& cbuffer) { Init(cbuffer); }

        void Init(const FShaderConstantBuffer& cbuffer);
        void Reset();

        [[nodiscard]] auto IsValid() const noexcept -> bool { return mSizeBytes > 0U; }
        [[nodiscard]] auto GetName() const noexcept -> const FString& { return mName; }
        [[nodiscard]] auto GetSizeBytes() const noexcept -> u32 { return mSizeBytes; }
        [[nodiscard]] auto GetSet() const noexcept -> u32 { return mSet; }
        [[nodiscard]] auto GetBinding() const noexcept -> u32 { return mBinding; }
        [[nodiscard]] auto GetRegister() const noexcept -> u32 { return mRegister; }
        [[nodiscard]] auto GetSpace() const noexcept -> u32 { return mSpace; }

        [[nodiscard]] auto GetData() const noexcept -> const u8* { return mData.Data(); }
        [[nodiscard]] auto GetData() noexcept -> u8* { return mData.Data(); }

        [[nodiscard]] auto FindProperty(const FString& name) const noexcept -> const FPropertyDesc*;
        [[nodiscard]] auto HasProperty(const FString& name) const noexcept -> bool {
            return FindProperty(name) != nullptr;
        }

        auto SetRaw(const FString& name, const void* data, u32 sizeBytes) -> bool;
        auto SetRaw(const TChar* name, const void* data, u32 sizeBytes) -> bool;

        template <typename T>
        auto Set(const FString& name, const T& value) -> bool {
            return SetRaw(name, &value, static_cast<u32>(sizeof(T)));
        }

        template <typename T>
        auto Set(const TChar* name, const T& value) -> bool {
            return SetRaw(name, &value, static_cast<u32>(sizeof(T)));
        }

        template <typename T>
        auto SetArray(const FString& name, const T* values, u32 count) -> bool {
            if (values == nullptr || count == 0U) {
                return false;
            }
            return SetRaw(name, values, static_cast<u32>(sizeof(T) * count));
        }

        template <typename T>
        auto SetArray(const TChar* name, const T* values, u32 count) -> bool {
            if (values == nullptr || count == 0U) {
                return false;
            }
            return SetRaw(name, values, static_cast<u32>(sizeof(T) * count));
        }

    private:
        struct FStringHash {
            auto operator()(const FString& s) const noexcept -> usize;
        };

        struct FStringEqual {
            auto operator()(const FString& a, const FString& b) const noexcept -> bool;
        };

        using FPropertyMap = Core::Container::THashMap<FString, FPropertyDesc, FStringHash,
            FStringEqual>;

        FString    mName;
        u32        mSizeBytes = 0U;
        u32        mSet = 0U;
        u32        mBinding = 0U;
        u32        mRegister = 0U;
        u32        mSpace = 0U;
        TVector<u8> mData;
        FPropertyMap mProperties;
    };
} // namespace AltinaEngine::Shader

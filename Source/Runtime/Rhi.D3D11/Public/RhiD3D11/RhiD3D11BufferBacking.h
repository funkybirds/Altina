#pragma once

#include "RhiD3D11API.h"
#include "Types/Aliases.h"

struct ID3D11Buffer;
struct ID3D11DeviceContext;

namespace AltinaEngine::Rhi {
    enum class ED3D11MapMode : u8 {
        WriteDiscard,
        WriteNoOverwrite
    };

    class AE_RHI_D3D11_API FD3D11BufferBacking final {
    public:
        FD3D11BufferBacking() = default;
        FD3D11BufferBacking(ID3D11Buffer* buffer, ID3D11DeviceContext* context, u64 sizeBytes);

        void Reset();
        void SetBuffer(ID3D11Buffer* buffer, ID3D11DeviceContext* context, u64 sizeBytes);

        [[nodiscard]] auto IsValid() const noexcept -> bool;
        [[nodiscard]] auto GetBuffer() const noexcept -> ID3D11Buffer*;
        [[nodiscard]] auto GetSizeBytes() const noexcept -> u64;
        [[nodiscard]] auto IsMapped() const noexcept -> bool;
        [[nodiscard]] auto GetData() noexcept -> void* { return mMappedData; }
        [[nodiscard]] auto GetData() const noexcept -> const void* { return mMappedData; }

        void               SetDefaultMapMode(ED3D11MapMode mode);
        auto               BeginWrite(ED3D11MapMode mode) -> bool;
        void               EndWrite();

        auto               Write(u64 offset, const void* data, u64 sizeBytes) -> bool;

    private:
        ID3D11Buffer*        mBuffer         = nullptr;
        ID3D11DeviceContext* mContext        = nullptr;
        u8*                  mMappedData     = nullptr;
        u64                  mSizeBytes      = 0ULL;
        ED3D11MapMode        mDefaultMapMode = ED3D11MapMode::WriteDiscard;
    };
} // namespace AltinaEngine::Rhi

#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiEnums.h"
#include "Types/Aliases.h"
#include "Container/Vector.h"

using AltinaEngine::Core::Container::TVector;

struct ID3D11DeviceContext;

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    class FRhiD3D11Device;

    enum class ED3D11StagingMapMode : u8 {
        Read,
        Write,
        ReadWrite
    };

    struct FD3D11StagingAllocation {
        FRhiBuffer*        mBuffer    = nullptr;
        u64                mSize      = 0ULL;
        u32                mPoolIndex = 0U;

        [[nodiscard]] auto IsValid() const noexcept -> bool { return mBuffer != nullptr; }
    };

    class AE_RHI_D3D11_API FD3D11StagingBufferManager {
    public:
        FD3D11StagingBufferManager() = default;

        void Init(FRhiD3D11Device* device);
        void Reset();

        auto Acquire(u64 sizeBytes, ERhiCpuAccess access) -> FD3D11StagingAllocation;
        void Release(const FD3D11StagingAllocation& allocation);

        auto Map(const FD3D11StagingAllocation& allocation, ED3D11StagingMapMode mode) -> void*;
        void Unmap(const FD3D11StagingAllocation& allocation);

    private:
        struct FStagingEntry {
            FRhiBufferRef mBuffer;
            u64           mSizeBytes = 0ULL;
            ERhiCpuAccess mCpuAccess = ERhiCpuAccess::Read;
            bool          mInUse     = false;
        };

        FRhiD3D11Device*       mDevice  = nullptr;
        ::ID3D11DeviceContext* mContext = nullptr;
        TVector<FStagingEntry> mEntries;
    };
} // namespace AltinaEngine::Rhi

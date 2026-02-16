#include "Asset/MaterialLoader.h"

#include "Asset/AssetBinary.h"
#include "Asset/MaterialAsset.h"
#include "Types/Traits.h"

#include <limits>

using AltinaEngine::Forward;
using AltinaEngine::Move;
using AltinaEngine::Core::Container::DestroyPolymorphic;
namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    namespace {
        auto ReadExact(IAssetStream& stream, void* outBuffer, usize size) -> bool {
            if (outBuffer == nullptr || size == 0U) {
                return false;
            }

            auto*       out       = static_cast<u8*>(outBuffer);
            usize       totalRead = 0;
            const usize target    = size;
            while (totalRead < target) {
                const usize read = stream.Read(out + totalRead, target - totalRead);
                if (read == 0U) {
                    return false;
                }
                totalRead += read;
            }
            return true;
        }

        auto ReadHeader(IAssetStream& stream, FAssetBlobHeader& outHeader) -> bool {
            if (!ReadExact(stream, &outHeader, sizeof(FAssetBlobHeader))) {
                return false;
            }

            if (outHeader.Magic != kAssetBlobMagic || outHeader.Version != kAssetBlobVersion) {
                return false;
            }

            if (outHeader.Type != static_cast<u8>(EAssetType::Material)) {
                return false;
            }

            if (outHeader.DescSize != sizeof(FMaterialBlobDesc)) {
                return false;
            }

            return true;
        }

        auto TryComputeBytes(u64 count, u64 stride, u64& outBytes) noexcept -> bool {
            if (count == 0U) {
                outBytes = 0U;
                return true;
            }
            if (stride == 0U) {
                return false;
            }
            if (count > (std::numeric_limits<u64>::max() / stride)) {
                return false;
            }
            outBytes = count * stride;
            return true;
        }

        auto RangeWithin(u64 offset, u64 size, u64 dataSize) noexcept -> bool {
            return offset <= dataSize && size <= (dataSize - offset);
        }

        template <typename TDerived, typename... Args>
        auto MakeSharedAsset(Args&&... args) -> TShared<IAsset> {
            using Container::kSmartPtrUseManagedAllocator;
            using Container::TAllocator;
            using Container::TAllocatorTraits;
            using Container::TPolymorphicDeleter;

            TDerived* ptr = nullptr;
            if constexpr (kSmartPtrUseManagedAllocator) {
                TAllocator<TDerived> allocator;
                ptr = TAllocatorTraits<TAllocator<TDerived>>::Allocate(allocator, 1);
                if (ptr == nullptr) {
                    return {};
                }

                try {
                    TAllocatorTraits<TAllocator<TDerived>>::Construct(
                        allocator, ptr, Forward<Args>(args)...);
                } catch (...) {
                    TAllocatorTraits<TAllocator<TDerived>>::Deallocate(allocator, ptr, 1);
                    return {};
                }
            } else {
                ptr = new TDerived(Forward<Args>(args)...); // NOLINT
            }

            return TShared<IAsset>(
                ptr, TPolymorphicDeleter<IAsset>(&DestroyPolymorphic<IAsset, TDerived>));
        }
    } // namespace

    auto FMaterialLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Material;
    }

    auto FMaterialLoader::Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> {
        FAssetBlobHeader header{};
        if (!ReadHeader(stream, header)) {
            return {};
        }

        FMaterialBlobDesc blobDesc{};
        if (!ReadExact(stream, &blobDesc, sizeof(FMaterialBlobDesc))) {
            return {};
        }

        if (desc.Material.ShadingModel != 0U
            && desc.Material.ShadingModel != blobDesc.ShadingModel) {
            return {};
        }

        u64 scalarBytes = 0;
        if (!TryComputeBytes(blobDesc.ScalarCount, sizeof(FMaterialScalarParam), scalarBytes)) {
            return {};
        }
        u64 vectorBytes = 0;
        if (!TryComputeBytes(blobDesc.VectorCount, sizeof(FMaterialVectorParam), vectorBytes)) {
            return {};
        }
        u64 textureBytes = 0;
        if (!TryComputeBytes(blobDesc.TextureCount, sizeof(FMaterialTextureParam), textureBytes)) {
            return {};
        }

        const u64 dataSize = header.DataSize;
        if (!RangeWithin(blobDesc.ScalarsOffset, scalarBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.VectorsOffset, vectorBytes, dataSize)) {
            return {};
        }
        if (!RangeWithin(blobDesc.TexturesOffset, textureBytes, dataSize)) {
            return {};
        }

        const usize baseOffset = stream.Tell();
        const u64   totalSize  = static_cast<u64>(baseOffset) + dataSize;
        const u64   streamSize = stream.Size();
        if (streamSize != 0U && totalSize > streamSize) {
            return {};
        }

        TVector<FMaterialScalarParam> scalars;
        if (blobDesc.ScalarCount > 0U) {
            scalars.Resize(static_cast<usize>(blobDesc.ScalarCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.ScalarsOffset));
            if (!ReadExact(stream, scalars.Data(), static_cast<usize>(scalarBytes))) {
                return {};
            }
        }

        TVector<FMaterialVectorParam> vectors;
        if (blobDesc.VectorCount > 0U) {
            vectors.Resize(static_cast<usize>(blobDesc.VectorCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.VectorsOffset));
            if (!ReadExact(stream, vectors.Data(), static_cast<usize>(vectorBytes))) {
                return {};
            }
        }

        TVector<FMaterialTextureParam> textures;
        if (blobDesc.TextureCount > 0U) {
            textures.Resize(static_cast<usize>(blobDesc.TextureCount));
            stream.Seek(baseOffset + static_cast<usize>(blobDesc.TexturesOffset));
            if (!ReadExact(stream, textures.Data(), static_cast<usize>(textureBytes))) {
                return {};
            }
        }

        for (const auto& param : textures) {
            if (!param.Texture.IsValid()) {
                return {};
            }
            if (param.Texture.Type != EAssetType::Texture2D) {
                return {};
            }
        }

        if (!desc.Material.TextureBindings.IsEmpty()) {
            for (const auto& param : textures) {
                bool found = false;
                for (const auto& binding : desc.Material.TextureBindings) {
                    if (binding == param.Texture) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return {};
                }
            }
        }

        FMaterialRuntimeDesc runtimeDesc{};
        runtimeDesc.ShadingModel = blobDesc.ShadingModel;
        runtimeDesc.BlendMode    = blobDesc.BlendMode;
        runtimeDesc.Flags        = blobDesc.Flags;
        runtimeDesc.AlphaCutoff  = blobDesc.AlphaCutoff;

        return MakeSharedAsset<FMaterialAsset>(
            runtimeDesc, Move(scalars), Move(vectors), Move(textures));
    }

} // namespace AltinaEngine::Asset

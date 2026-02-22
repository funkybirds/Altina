#include "Asset/ShaderLoader.h"

#include "Asset/ShaderAsset.h"
#include "Types/Traits.h"

using AltinaEngine::Forward;
using AltinaEngine::Move;
using AltinaEngine::Core::Container::DestroyPolymorphic;
namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;
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

        auto ReadAllBytes(IAssetStream& stream, TVector<u8>& outBytes) -> bool {
            const usize size = stream.Size();
            if (size == 0U) {
                return false;
            }

            outBytes.Resize(size);
            stream.Seek(0U);
            return ReadExact(stream, outBytes.Data(), size);
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

    auto FShaderLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Shader;
    }

    auto FShaderLoader::Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> {
        TVector<u8> bytes;
        if (!ReadAllBytes(stream, bytes)) {
            return {};
        }

        Container::FNativeString source;
        if (!bytes.IsEmpty()) {
            source.Append(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
        }

        return MakeSharedAsset<FShaderAsset>(desc.Shader.Language, Move(source));
    }
} // namespace AltinaEngine::Asset

#include "TestHarness.h"

#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/CubeMapAsset.h"
#include "Asset/CubeMapLoader.h"
#include "Engine/GameScene/SkyCubeComponent.h"

#include "RhiMock/RhiMockContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiResourceView.h"
#include "Rhi/RhiStructs.h"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <cstring>

namespace {
    using AltinaEngine::FUuid;
    using AltinaEngine::TChar;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::u8;
    using AltinaEngine::usize;
    namespace Container = AltinaEngine::Core::Container;
    using Container::FString;
    using Container::TVector;

    auto MakeUuid(u8 seed) -> FUuid {
        FUuid::FBytes bytes{};
        for (usize i = 0U; i < FUuid::kByteCount; ++i) {
            bytes[i] = static_cast<u8>(seed + static_cast<u8>(i));
        }
        return FUuid(bytes);
    }

    auto ToFString(const std::filesystem::path& path) -> FString {
        FString out;
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        const auto wide = path.wstring();
        if (!wide.empty()) {
            out.Append(wide.c_str(), wide.size());
        }
#else
        const auto narrow = path.string();
        if (!narrow.empty()) {
            out.Append(narrow.c_str(), narrow.size());
        }
#endif
        return out;
    }

    auto MakeAdapterDesc(const TChar* name) -> AltinaEngine::Rhi::FRhiAdapterDesc {
        using AltinaEngine::Rhi::ERhiAdapterType;
        using AltinaEngine::Rhi::ERhiVendorId;
        AltinaEngine::Rhi::FRhiAdapterDesc desc{};
        desc.mName.Assign(name);
        desc.mType     = ERhiAdapterType::Discrete;
        desc.mVendorId = ERhiVendorId::Nvidia;
        return desc;
    }

    auto BuildCookedCubeBlob(u32 size, u32 mipCount, u32 format, bool srgb) -> TVector<u8> {
        using namespace AltinaEngine::Asset;

        const u32 bytesPerPixel = GetTextureBytesPerPixel(format);
        if (bytesPerPixel == 0U || size == 0U || mipCount == 0U) {
            return {};
        }

        u32 curSize = size;
        u64 total   = 0ULL;
        for (u32 mip = 0U; mip < mipCount; ++mip) {
            total += static_cast<u64>(curSize) * static_cast<u64>(curSize)
                * static_cast<u64>(bytesPerPixel) * 6ULL;
            curSize = (curSize > 1U) ? (curSize >> 1U) : 1U;
        }

        FAssetBlobHeader header{};
        header.Type     = static_cast<u8>(EAssetType::CubeMap);
        header.Flags    = MakeAssetBlobFlags(srgb);
        header.DescSize = static_cast<u32>(sizeof(FCubeMapBlobDesc));
        header.DataSize = static_cast<u32>(total);

        FCubeMapBlobDesc blobDesc{};
        blobDesc.Size     = size;
        blobDesc.Format   = format;
        blobDesc.MipCount = mipCount;
        blobDesc.RowPitch = size * bytesPerPixel;

        TVector<u8> cooked;
        cooked.Resize(sizeof(header) + sizeof(blobDesc) + static_cast<usize>(total));

        u8* writePtr = cooked.Data();
        std::memcpy(writePtr, &header, sizeof(header));
        writePtr += sizeof(header);
        std::memcpy(writePtr, &blobDesc, sizeof(blobDesc));
        writePtr += sizeof(blobDesc);

        for (usize i = 0U; i < static_cast<usize>(total); ++i) {
            writePtr[i] = static_cast<u8>((i * 7U) & 0xFFU);
        }

        return cooked;
    }
} // namespace

TEST_CASE("GameScene.SkyCubeComponent.AssetToRhi") {
    using namespace AltinaEngine;
    namespace Asset     = AltinaEngine::Asset;
    namespace Rhi       = AltinaEngine::Rhi;
    namespace GameScene = AltinaEngine::GameScene;

    // Init global RHI device for RHICreateTexture / RHIGetDevice used by the converter.
    Rhi::FRhiMockContext context;
    context.AddAdapter(MakeAdapterDesc(TEXT("Mock Discrete")));
    Rhi::FRhiInitDesc initDesc{};
    initDesc.mAdapterPreference = Rhi::ERhiGpuPreference::HighPerformance;
    const auto device           = Rhi::RHIInit(context, initDesc, Rhi::FRhiDeviceDesc{}, 0U);
    REQUIRE(device);

    const auto cooked = BuildCookedCubeBlob(4U, 2U, Asset::kTextureFormatRGBA8, false);
    REQUIRE(!cooked.IsEmpty());

    std::error_code ec;
    const auto      tempDir = std::filesystem::temp_directory_path(ec);
    REQUIRE(!ec);
    const auto cookedPath = tempDir / "AltinaEngine_Test_SkyCube.cube";

    {
        std::ofstream file(cookedPath, std::ios::binary | std::ios::trunc);
        REQUIRE(file.good());
        file.write(reinterpret_cast<const char*>(cooked.Data()),
            static_cast<std::streamsize>(cooked.Size()));
        REQUIRE(file.good());
    }

    Asset::FAssetRegistry registry;
    Asset::FAssetDesc     desc{};
    desc.Handle.Uuid      = MakeUuid(0x42U);
    desc.Handle.Type      = Asset::EAssetType::CubeMap;
    desc.VirtualPath      = TEXT("test/sky");
    desc.CookedPath       = ToFString(cookedPath);
    desc.CubeMap.Size     = 4U;
    desc.CubeMap.MipCount = 2U;
    desc.CubeMap.Format   = Asset::kTextureFormatRGBA8;
    desc.CubeMap.SRGB     = false;
    registry.AddAsset(desc);

    Asset::FAssetManager  manager;
    Asset::FCubeMapLoader loader;
    manager.SetRegistry(&registry);
    manager.RegisterLoader(&loader);

    // Bind the conversion (mimics Launch::FEngineLoop binding).
    GameScene::FSkyCubeComponent::AssetToSkyCubeConverter = [&manager](
                                                                const Asset::FAssetHandle& handle)
        -> GameScene::FSkyCubeComponent::FSkyCubeRhiResources {
        GameScene::FSkyCubeComponent::FSkyCubeRhiResources out{};
        auto*                                              devicePtr = Rhi::RHIGetDevice();
        if (devicePtr == nullptr || !handle.IsValid()) {
            return out;
        }

        auto assetRef = manager.Load(handle);
        if (!assetRef) {
            return out;
        }

        const auto* cubeAsset = static_cast<const Asset::FCubeMapAsset*>(assetRef.Get());
        if (cubeAsset == nullptr) {
            return out;
        }

        const auto& assetDesc     = cubeAsset->GetDesc();
        const u32   bytesPerPixel = Asset::GetTextureBytesPerPixel(assetDesc.Format);
        REQUIRE(bytesPerPixel == 4U);

        Rhi::FRhiTextureDesc texDesc{};
        texDesc.mWidth       = assetDesc.Size;
        texDesc.mHeight      = assetDesc.Size;
        texDesc.mMipLevels   = assetDesc.MipCount;
        texDesc.mArrayLayers = 6U;
        texDesc.mDepth       = 1U;
        texDesc.mFormat      = Rhi::ERhiFormat::R8G8B8A8Unorm;
        texDesc.mDimension   = Rhi::ERhiTextureDimension::Cube;
        texDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::ShaderResource;

        out.Texture = Rhi::RHICreateTexture(texDesc);
        REQUIRE(out.Texture);

        const auto& pixels = cubeAsset->GetPixels();
        REQUIRE(!pixels.IsEmpty());

        u32   size   = assetDesc.Size;
        usize offset = 0U;
        for (u32 mip = 0U; mip < assetDesc.MipCount; ++mip) {
            const u32 rowPitch   = size * bytesPerPixel;
            const u32 slicePitch = rowPitch * size;
            for (u32 face = 0U; face < 6U; ++face) {
                Rhi::FRhiTextureSubresource subresource{};
                subresource.mMipLevel   = mip;
                subresource.mArrayLayer = face;
                devicePtr->UpdateTextureSubresource(
                    out.Texture.Get(), subresource, pixels.Data() + offset, rowPitch, slicePitch);
                offset += static_cast<usize>(slicePitch);
            }
            size = (size > 1U) ? (size >> 1U) : 1U;
        }

        Rhi::FRhiShaderResourceViewDesc viewDesc{};
        viewDesc.mTexture                  = out.Texture.Get();
        viewDesc.mFormat                   = texDesc.mFormat;
        viewDesc.mTextureRange.mMipCount   = texDesc.mMipLevels;
        viewDesc.mTextureRange.mLayerCount = texDesc.mArrayLayers;
        out.SRV                            = devicePtr->CreateShaderResourceView(viewDesc);
        REQUIRE(out.SRV);
        return out;
    };

    GameScene::FSkyCubeComponent component;
    component.SetCubeMapAsset(desc.Handle);

    const auto& rhi = component.GetCubeMapRhi();
    REQUIRE(rhi.IsValid());
    REQUIRE(rhi.SRV->GetTexture() != nullptr);
    REQUIRE(rhi.SRV->GetTexture()->GetDesc().mDimension == Rhi::ERhiTextureDimension::Cube);
    REQUIRE(rhi.SRV->GetTexture()->GetDesc().mArrayLayers == 6U);

    // Cleanup.
    manager.UnregisterLoader(&loader);
    manager.SetRegistry(nullptr);
    std::filesystem::remove(cookedPath, ec);
    Rhi::RHIExit(context);
}

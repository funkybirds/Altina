
#include "Importers/Model/AssimpModelImporter.h"

#include "Asset/AssetBinary.h"
#include "AssetToolIO.h"
#include "Importers/Mesh/MeshBuild.h"
#include "Importers/Texture/TextureImporter.h"
#include "Imaging/Image.h"
#include "Imaging/ImageIO.h"
#include "Utility/Json.h"
#include "Utility/Uuid.h"

#include <assimp/material.h>
#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <cstring>
#include <iostream>
#include <sstream>
#include <limits>
#include <unordered_map>

using AltinaEngine::Move;
namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetBoolValue;

        constexpr u64 kFnvOffset = 1469598103934665603ULL;
        constexpr u64 kFnvPrime  = 1099511628211ULL;

        auto          HashBytes(u64 hash, const void* data, size_t size) -> u64 {
            const auto* bytes = static_cast<const u8*>(data);
            for (size_t i = 0; i < size; ++i) {
                hash ^= static_cast<u64>(bytes[i]);
                hash *= kFnvPrime;
            }
            return hash;
        }

        auto HashString(u64 hash, const std::string& text) -> u64 {
            return HashBytes(hash, text.data(), text.size());
        }

        auto MakeDerivedUuid(const FUuid& base, const std::string& salt) -> FUuid {
            u64 h1 = HashBytes(kFnvOffset, base.Data(), FUuid::kByteCount);
            h1     = HashString(h1, salt);
            u64 h2 = HashString(kFnvOffset, salt);
            h2     = HashBytes(h2, base.Data(), FUuid::kByteCount);

            FUuid::FBytes bytes{};
            for (u32 i = 0; i < 8U; ++i) {
                bytes[i]     = static_cast<u8>((h1 >> (i * 8U)) & 0xFFU);
                bytes[i + 8] = static_cast<u8>((h2 >> (i * 8U)) & 0xFFU);
            }
            return FUuid(bytes);
        }

        auto ToLowerAscii(char value) -> char {
            if (value >= 'A' && value <= 'Z') {
                return static_cast<char>(value - 'A' + 'a');
            }
            return value;
        }

        void ToLowerAscii(std::string& value) {
            for (char& character : value) {
                character = ToLowerAscii(character);
            }
        }

        auto NormalizeVirtualPath(std::string value) -> std::string {
            for (auto& ch : value) {
                if (ch == '\\') {
                    ch = '/';
                }
            }
            ToLowerAscii(value);
            return value;
        }

        auto SanitizeName(std::string value, const char* fallback) -> std::string {
            if (value.empty() && fallback != nullptr) {
                value = fallback;
            }
            for (auto& ch : value) {
                const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
                    || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.';
                if (!ok) {
                    ch = '_';
                }
            }
            ToLowerAscii(value);
            return value;
        }

        auto ToStdString(const Core::Container::FNativeString& value) -> std::string {
            if (value.IsEmptyString()) {
                return {};
            }
            return std::string(value.GetData(), value.Length());
        }

        struct FModelImportOptions {
            // Many DCC/FBX pipelines produce UVs where V increases upward, while our decoded image
            // data is top-down (V increases downward). Flipping V aligns the conventions.
            bool mFlipTexCoordV = true;
        };

        auto LoadModelImportOptions(const std::filesystem::path& sourcePath,
            FModelImportOptions& outOptions, std::string& outError) -> bool {
            outOptions = {};

            std::filesystem::path metaPath = sourcePath;
            metaPath += ".meta";

            std::error_code ec;
            if (!std::filesystem::exists(metaPath, ec)) {
                return true;
            }

            std::string metaText;
            if (!ReadFileText(metaPath, metaText)) {
                // Fail-open: treat missing/unreadable meta as default options.
                return true;
            }

            Core::Container::FNativeString native;
            native.Append(metaText.c_str(), metaText.size());
            const Core::Container::FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument                            document;
            if (!document.Parse(view)) {
                outError = "Invalid .meta JSON.";
                return false;
            }

            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                outError = "Invalid .meta JSON root.";
                return false;
            }

            // Optional override:
            //   "FlipTexCoordV": true/false
            // Also accept some aliases to keep things ergonomic.
            static constexpr const char* kKeys[] = {
                "FlipTexCoordV",
                "FlipTexcoordV",
                "FlipUVV",
                "FlipUvV",
                "FlipV",
            };

            for (const char* key : kKeys) {
                const FJsonValue* value = FindObjectValueInsensitive(*root, key);
                if (value == nullptr) {
                    continue;
                }
                bool flag = false;
                if (!GetBoolValue(value, flag)) {
                    outError = "Invalid FlipTexCoordV value in .meta (expected boolean).";
                    return false;
                }
                outOptions.mFlipTexCoordV = flag;
                break;
            }

            return true;
        }

        auto BuildMeshBlob(const FMeshBuildResult& mesh, std::vector<u8>& outCooked,
            Asset::FMeshDesc& outDesc) -> bool {
            if (mesh.VertexCount == 0 || mesh.IndexCount == 0 || mesh.VertexStride == 0) {
                return false;
            }

            Asset::FMeshBlobDesc blobDesc{};
            blobDesc.mVertexCount    = mesh.VertexCount;
            blobDesc.mIndexCount     = mesh.IndexCount;
            blobDesc.mVertexStride   = mesh.VertexStride;
            blobDesc.mIndexType      = mesh.IndexType;
            blobDesc.mAttributeCount = static_cast<u32>(mesh.Attributes.size());
            blobDesc.mSubMeshCount   = static_cast<u32>(mesh.SubMeshes.size());
            blobDesc.mVertexDataSize = static_cast<u32>(mesh.VertexData.size());
            blobDesc.mIndexDataSize  = static_cast<u32>(mesh.IndexData.size());
            blobDesc.mBoundsMin[0]   = mesh.BoundsMin[0];
            blobDesc.mBoundsMin[1]   = mesh.BoundsMin[1];
            blobDesc.mBoundsMin[2]   = mesh.BoundsMin[2];
            blobDesc.mBoundsMax[0]   = mesh.BoundsMax[0];
            blobDesc.mBoundsMax[1]   = mesh.BoundsMax[1];
            blobDesc.mBoundsMax[2]   = mesh.BoundsMax[2];
            blobDesc.mFlags          = 1U;

            const u32 attrBytes =
                blobDesc.mAttributeCount * sizeof(Asset::FMeshVertexAttributeDesc);
            const u32 subMeshBytes = blobDesc.mSubMeshCount * sizeof(Asset::FMeshSubMeshDesc);

            blobDesc.mAttributesOffset = 0;
            blobDesc.mSubMeshesOffset  = blobDesc.mAttributesOffset + attrBytes;
            blobDesc.mVertexDataOffset = blobDesc.mSubMeshesOffset + subMeshBytes;
            blobDesc.mIndexDataOffset  = blobDesc.mVertexDataOffset + blobDesc.mVertexDataSize;

            const u32               dataSize = blobDesc.mIndexDataOffset + blobDesc.mIndexDataSize;

            Asset::FAssetBlobHeader header{};
            header.mType     = static_cast<u8>(Asset::EAssetType::Mesh);
            header.mDescSize = static_cast<u32>(sizeof(Asset::FMeshBlobDesc));
            header.mDataSize = dataSize;

            const usize totalSize =
                sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FMeshBlobDesc) + dataSize;
            outCooked.resize(totalSize);

            u8* writePtr = outCooked.data();
            std::memcpy(writePtr, &header, sizeof(header));
            writePtr += sizeof(header);
            std::memcpy(writePtr, &blobDesc, sizeof(blobDesc));
            writePtr += sizeof(blobDesc);

            if (!mesh.Attributes.empty()) {
                std::memcpy(
                    writePtr + blobDesc.mAttributesOffset, mesh.Attributes.data(), attrBytes);
            }
            if (!mesh.SubMeshes.empty()) {
                std::memcpy(
                    writePtr + blobDesc.mSubMeshesOffset, mesh.SubMeshes.data(), subMeshBytes);
            }
            if (!mesh.VertexData.empty()) {
                std::memcpy(writePtr + blobDesc.mVertexDataOffset, mesh.VertexData.data(),
                    mesh.VertexData.size());
            }
            if (!mesh.IndexData.empty()) {
                std::memcpy(writePtr + blobDesc.mIndexDataOffset, mesh.IndexData.data(),
                    mesh.IndexData.size());
            }

            outDesc.VertexFormat = mesh.VertexFormatMask;
            outDesc.IndexFormat  = mesh.IndexType;
            outDesc.SubMeshCount = blobDesc.mSubMeshCount;
            return true;
        }
        auto BuildMeshFromAssimp(const aiMesh* mesh, FMeshBuildResult& outMesh, bool flipTexCoordV)
            -> bool {
            if (mesh == nullptr || mesh->mNumVertices == 0) {
                return false;
            }

            const bool hasNormals   = mesh->HasNormals();
            const bool hasTangents  = mesh->HasTangentsAndBitangents();
            const bool hasTexcoords = mesh->HasTextureCoords(0);

            u32        offset = 0;
            outMesh.Attributes.clear();
            {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.mSemantic      = Asset::kMeshSemanticPosition;
                attr.mFormat        = Asset::kMeshVertexFormatR32G32B32Float;
                attr.mAlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 12;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskPosition;
            }
            if (hasNormals) {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.mSemantic      = Asset::kMeshSemanticNormal;
                attr.mFormat        = Asset::kMeshVertexFormatR32G32B32Float;
                attr.mAlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 12;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskNormal;
            }
            if (hasTangents) {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.mSemantic      = Asset::kMeshSemanticTangent;
                attr.mFormat        = Asset::kMeshVertexFormatR32G32B32Float;
                attr.mAlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 12;
            }
            if (hasTexcoords) {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.mSemantic      = Asset::kMeshSemanticTexCoord;
                attr.mSemanticIndex = 0;
                attr.mFormat        = Asset::kMeshVertexFormatR32G32Float;
                attr.mAlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 8;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskTexCoord0;
            }

            outMesh.VertexStride = offset;
            outMesh.VertexCount  = mesh->mNumVertices;
            outMesh.VertexData.resize(
                static_cast<size_t>(outMesh.VertexStride) * outMesh.VertexCount);

            bool boundsSet = false;
            for (u32 i = 0; i < mesh->mNumVertices; ++i) {
                u8* dst = outMesh.VertexData.data()
                    + static_cast<size_t>(i) * static_cast<size_t>(outMesh.VertexStride);
                const auto& pos = mesh->mVertices[i];
                const FVec3 position{ pos.x, pos.y, pos.z };
                std::memcpy(dst, &position, sizeof(FVec3));
                u32 writeOffset = 12;
                if (hasNormals) {
                    const auto& n = mesh->mNormals[i];
                    const FVec3 normal{ n.x, n.y, n.z };
                    std::memcpy(dst + writeOffset, &normal, sizeof(FVec3));
                    writeOffset += 12;
                }
                if (hasTangents) {
                    const auto& t = mesh->mTangents[i];
                    const FVec3 tangent{ t.x, t.y, t.z };
                    std::memcpy(dst + writeOffset, &tangent, sizeof(FVec3));
                    writeOffset += 12;
                }
                if (hasTexcoords) {
                    const auto& uv = mesh->mTextureCoords[0][i];
                    const float v  = flipTexCoordV ? (1.0f - uv.y) : uv.y;
                    const FVec2 tex{ uv.x, v };
                    std::memcpy(dst + writeOffset, &tex, sizeof(FVec2));
                }

                if (!boundsSet) {
                    outMesh.BoundsMin[0] = position.X;
                    outMesh.BoundsMin[1] = position.Y;
                    outMesh.BoundsMin[2] = position.Z;
                    outMesh.BoundsMax[0] = position.X;
                    outMesh.BoundsMax[1] = position.Y;
                    outMesh.BoundsMax[2] = position.Z;
                    boundsSet            = true;
                } else {
                    outMesh.BoundsMin[0] = std::min(outMesh.BoundsMin[0], position.X);
                    outMesh.BoundsMin[1] = std::min(outMesh.BoundsMin[1], position.Y);
                    outMesh.BoundsMin[2] = std::min(outMesh.BoundsMin[2], position.Z);
                    outMesh.BoundsMax[0] = std::max(outMesh.BoundsMax[0], position.X);
                    outMesh.BoundsMax[1] = std::max(outMesh.BoundsMax[1], position.Y);
                    outMesh.BoundsMax[2] = std::max(outMesh.BoundsMax[2], position.Z);
                }
            }

            std::vector<u32> indices;
            indices.reserve(mesh->mNumFaces * 3ULL);
            for (u32 i = 0; i < mesh->mNumFaces; ++i) {
                const auto& face = mesh->mFaces[i];
                if (face.mNumIndices != 3) {
                    return false;
                }
                indices.push_back(face.mIndices[0]);
                indices.push_back(face.mIndices[1]);
                indices.push_back(face.mIndices[2]);
            }

            u32 maxIndex = 0;
            for (u32 idx : indices) {
                maxIndex = std::max(maxIndex, idx);
            }

            outMesh.IndexType =
                (maxIndex <= 0xFFFFu) ? Asset::kMeshIndexTypeUint16 : Asset::kMeshIndexTypeUint32;
            outMesh.IndexCount = static_cast<u32>(indices.size());

            if (outMesh.IndexType == Asset::kMeshIndexTypeUint16) {
                outMesh.IndexData.resize(indices.size() * sizeof(u16));
                auto* dst = reinterpret_cast<u16*>(outMesh.IndexData.data());
                for (size_t i = 0; i < indices.size(); ++i) {
                    dst[i] = static_cast<u16>(indices[i]);
                }
            } else {
                outMesh.IndexData.resize(indices.size() * sizeof(u32));
                std::memcpy(outMesh.IndexData.data(), indices.data(), indices.size() * sizeof(u32));
            }

            Asset::FMeshSubMeshDesc subMesh{};
            subMesh.mIndexStart   = 0;
            subMesh.mIndexCount   = outMesh.IndexCount;
            subMesh.mBaseVertex   = 0;
            subMesh.mMaterialSlot = 0;
            outMesh.SubMeshes     = { subMesh };
            return true;
        }

        auto BuildMaterialCookedJson(const std::string&                      name,
            const std::vector<std::pair<std::string, std::string>>&          textureOverrides,
            const std::vector<std::pair<std::string, std::array<float, 4>>>& vectorOverrides,
            const std::vector<std::pair<std::string, double>>&               scalarOverrides,
            std::string&                                                     outJson) -> bool {
            std::ostringstream stream;
            stream << "{\n";
            if (!name.empty()) {
                stream << "  \"Name\": \"" << name << "\",\n";
            }
            stream << "  \"Passes\": {\n";
            stream << "    \"BasePass\": {\n";
            stream << "      \"Preset\": \"Deferred/Lit/PBR.Standard\",\n";
            stream << "      \"Overrides\": {\n";

            bool first     = true;
            auto emitComma = [&]() {
                if (!first) {
                    stream << ",\n";
                }
                first = false;
            };

            for (const auto& scalar : scalarOverrides) {
                emitComma();
                stream << "        \"" << scalar.first
                       << "\": { \"Type\": \"float\", \"Value\": " << scalar.second << " }";
            }

            for (const auto& vec : vectorOverrides) {
                emitComma();
                stream << "        \"" << vec.first << "\": { \"Type\": \"float4\", \"Value\": ["
                       << vec.second[0] << ", " << vec.second[1] << ", " << vec.second[2] << ", "
                       << vec.second[3] << "] }";
            }

            for (const auto& tex : textureOverrides) {
                emitComma();
                stream << "        \"" << tex.first
                       << "\": { \"Type\": \"texture2d\", \"Value\": { "
                       << "\"Uuid\": \"" << tex.second << "\", \"Type\": \"Texture2D\" } }";
            }

            stream << "\n";
            stream << "      }\n";
            stream << "    }\n";
            stream << "  },\n";
            stream << "  \"Precompile_Variants\": []\n";
            stream << "}\n";

            outJson = stream.str();
            return true;
        }

        auto BuildModelBlob(const std::vector<Asset::FModelNodeDesc>& nodes,
            const std::vector<Asset::FModelMeshRef>&                  meshRefs,
            const std::vector<Asset::FAssetHandle>& materialSlots, std::vector<u8>& outCooked,
            Asset::FModelDesc& outDesc) -> bool {
            Asset::FModelBlobDesc blobDesc{};
            blobDesc.mNodeCount         = static_cast<u32>(nodes.size());
            blobDesc.mMeshRefCount      = static_cast<u32>(meshRefs.size());
            blobDesc.mMaterialSlotCount = static_cast<u32>(materialSlots.size());

            const u32 nodesBytes    = blobDesc.mNodeCount * sizeof(Asset::FModelNodeDesc);
            const u32 meshRefBytes  = blobDesc.mMeshRefCount * sizeof(Asset::FModelMeshRef);
            const u32 materialBytes = blobDesc.mMaterialSlotCount * sizeof(Asset::FAssetHandle);

            blobDesc.mNodesOffset         = 0;
            blobDesc.mMeshRefsOffset      = blobDesc.mNodesOffset + nodesBytes;
            blobDesc.mMaterialSlotsOffset = blobDesc.mMeshRefsOffset + meshRefBytes;

            const u32               dataSize = blobDesc.mMaterialSlotsOffset + materialBytes;

            Asset::FAssetBlobHeader header{};
            header.mType     = static_cast<u8>(Asset::EAssetType::Model);
            header.mDescSize = static_cast<u32>(sizeof(Asset::FModelBlobDesc));
            header.mDataSize = dataSize;

            const usize totalSize =
                sizeof(Asset::FAssetBlobHeader) + sizeof(Asset::FModelBlobDesc) + dataSize;
            outCooked.resize(totalSize);

            u8* writePtr = outCooked.data();
            std::memcpy(writePtr, &header, sizeof(header));
            writePtr += sizeof(header);
            std::memcpy(writePtr, &blobDesc, sizeof(blobDesc));
            writePtr += sizeof(blobDesc);

            if (!nodes.empty()) {
                std::memcpy(writePtr + blobDesc.mNodesOffset, nodes.data(), nodesBytes);
            }
            if (!meshRefs.empty()) {
                std::memcpy(writePtr + blobDesc.mMeshRefsOffset, meshRefs.data(), meshRefBytes);
            }
            if (!materialSlots.empty()) {
                std::memcpy(
                    writePtr + blobDesc.mMaterialSlotsOffset, materialSlots.data(), materialBytes);
            }

            outDesc.NodeCount         = blobDesc.mNodeCount;
            outDesc.MeshRefCount      = blobDesc.mMeshRefCount;
            outDesc.MaterialSlotCount = blobDesc.mMaterialSlotCount;
            return true;
        }

        auto GetMaterialName(const aiMaterial* material, u32 index) -> std::string {
            if (material != nullptr) {
                aiString name;
                if (material->Get(AI_MATKEY_NAME, name) == aiReturn_SUCCESS) {
                    return name.C_Str();
                }
            }
            return "Material_" + std::to_string(index);
        }

        auto GetMeshName(const aiMesh* mesh, u32 index) -> std::string {
            if (mesh != nullptr && mesh->mName.length > 0) {
                return mesh->mName.C_Str();
            }
            return "Mesh_" + std::to_string(index);
        }

        auto ToAssetHandle(const FUuid& uuid, Asset::EAssetType type) -> Asset::FAssetHandle {
            Asset::FAssetHandle handle{};
            handle.mUuid = uuid;
            handle.mType = type;
            return handle;
        }

        auto TryExtractEmbeddedTextureBytes(const aiTexture* embedded, std::vector<u8>& outBytes)
            -> bool {
            outBytes.clear();
            if (embedded == nullptr) {
                return false;
            }

            if (embedded->mHeight == 0) {
                const auto* bytes = reinterpret_cast<const u8*>(embedded->pcData);
                if (bytes == nullptr || embedded->mWidth == 0) {
                    return false;
                }
                outBytes.assign(bytes, bytes + static_cast<size_t>(embedded->mWidth));
                return !outBytes.empty();
            }

            const u32 width  = embedded->mWidth;
            const u32 height = embedded->mHeight;
            if (width == 0U || height == 0U || embedded->pcData == nullptr) {
                return false;
            }

            Imaging::FImage image(width, height, Imaging::EImageFormat::RGBA8);
            if (!image.IsValid()) {
                return false;
            }

            auto*       dst        = image.GetData();
            const auto* src        = embedded->pcData;
            const usize pixelCount = static_cast<usize>(width) * static_cast<usize>(height);
            for (usize i = 0; i < pixelCount; ++i) {
                dst[i * 4 + 0] = src[i].r;
                dst[i * 4 + 1] = src[i].g;
                dst[i * 4 + 2] = src[i].b;
                dst[i * 4 + 3] = src[i].a;
            }

            Imaging::FPngImageWriter     writer;
            Core::Container::TVector<u8> pngBytes;
            if (!writer.Write(image.View(), pngBytes)) {
                return false;
            }

            if (!pngBytes.IsEmpty()) {
                outBytes.assign(pngBytes.begin(), pngBytes.end());
            }
            return !outBytes.empty();
        }
    } // namespace
    auto CookModelFromAssimpScene(const std::filesystem::path& sourcePath, const aiScene* scene,
        const Asset::FAssetHandle& baseHandle, const std::string& baseVirtualPath,
        FModelCookResult& outResult, std::string& outError) -> bool {
        outResult       = {};
        auto hasEnvFlag = [](const char* name) -> bool {
            if (name == nullptr || name[0] == '\0') {
                return false;
            }
#if defined(_MSC_VER)
            char*  value  = nullptr;
            size_t length = 0;
            if (_dupenv_s(&value, &length, name) != 0) {
                return false;
            }
            if (value != nullptr) {
                std::free(value);
                value = nullptr;
            }
            return length > 0;
#else
            return std::getenv(name) != nullptr;
#endif
        };

        const bool      debugTextures = hasEnvFlag("AE_ASSETTOOL_DEBUG_MODEL_TEXTURES");
        std::vector<u8> externalTextureCookKey;

        if (scene == nullptr || scene->mRootNode == nullptr) {
            outError = "Invalid Assimp scene.";
            return false;
        }
        if (debugTextures) {
            std::cout << "[AssetTool][ModelImporter] CookModel source=" << sourcePath.string()
                      << " materials=" << scene->mNumMaterials
                      << " embeddedTextures=" << scene->mNumTextures << "\n";
        }

        std::vector<FGeneratedAsset>      generated;
        std::unordered_map<u32, u32>      meshIndexToRef;
        std::vector<Asset::FModelMeshRef> meshRefs;
        std::vector<Asset::FAssetHandle>  materialSlots;

        std::vector<Asset::FAssetHandle>  materialHandles;
        materialHandles.resize(scene->mNumMaterials);
        std::unordered_map<std::string, Asset::FAssetHandle> textureByVirtual;

        const std::string   basePath = NormalizeVirtualPath(baseVirtualPath);
        FModelImportOptions importOptions{};
        {
            std::string optError;
            if (!LoadModelImportOptions(sourcePath, importOptions, optError)) {
                outError = optError;
                return false;
            }
        }

        for (u32 i = 0; i < scene->mNumMaterials; ++i) {
            const aiMaterial* material   = scene->mMaterials[i];
            const std::string matNameRaw = GetMaterialName(material, i);
            const std::string matName    = SanitizeName(matNameRaw, "material");

            if (debugTextures && material != nullptr) {
                auto printTypeCount = [&](aiTextureType type, const char* label) {
                    const unsigned int count = material->GetTextureCount(type);
                    std::cout << "  [Material] " << matNameRaw << " " << label
                              << " count=" << count;
                    if (count > 0) {
                        aiString path;
                        if (material->GetTexture(type, 0, &path) == aiReturn_SUCCESS) {
                            std::cout << " first='" << path.C_Str() << "'";
                        }
                    }
                    std::cout << "\n";
                };

                printTypeCount(aiTextureType_DIFFUSE, "DIFFUSE");
                printTypeCount(aiTextureType_BASE_COLOR, "BASE_COLOR");
                printTypeCount(aiTextureType_EMISSIVE, "EMISSIVE");
                printTypeCount(aiTextureType_NORMALS, "NORMALS");
                printTypeCount(aiTextureType_METALNESS, "METALNESS");
                printTypeCount(aiTextureType_DIFFUSE_ROUGHNESS, "DIFFUSE_ROUGHNESS");
                printTypeCount(aiTextureType_AMBIENT_OCCLUSION, "AMBIENT_OCCLUSION");
            }

            const std::string matVirtual = NormalizeVirtualPath(basePath + "/materials/" + matName);
            const FUuid       matUuid    = MakeDerivedUuid(baseHandle.mUuid, matVirtual);

            Asset::FAssetHandle matHandle =
                ToAssetHandle(matUuid, Asset::EAssetType::MaterialTemplate);
            materialHandles[i] = matHandle;

            std::vector<std::pair<std::string, std::string>>          textureOverrides;
            std::vector<std::pair<std::string, std::array<float, 4>>> vectorOverrides;

            aiColor4D diffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
            if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuseColor)
                == aiReturn_SUCCESS) {
                vectorOverrides.push_back({ "BaseColor",
                    { diffuseColor.r, diffuseColor.g, diffuseColor.b, diffuseColor.a } });
            } else {
                vectorOverrides.push_back({ "BaseColor", { 1.0f, 1.0f, 1.0f, 1.0f } });
            }

            auto resolveTexture = [&](aiTextureType type, const char* paramName, bool srgb) {
                aiString texPath;
                if (material->GetTexture(type, 0, &texPath) != aiReturn_SUCCESS) {
                    if (debugTextures) {
                        std::cout << "    [Texture] " << matNameRaw << " " << paramName
                                  << " GetTexture=FAIL\n";
                    }
                    return;
                }

                std::string textureKey = texPath.C_Str();
                if (textureKey.empty()) {
                    if (debugTextures) {
                        std::cout << "    [Texture] " << matNameRaw << " " << paramName
                                  << " path=EMPTY\n";
                    }
                    return;
                }

                std::vector<u8> textureBytes;
                if (textureKey[0] == '*') {
                    const u32 index = static_cast<u32>(std::atoi(textureKey.c_str() + 1));
                    if (index < scene->mNumTextures) {
                        const aiTexture* embedded = scene->mTextures[index];
                        if (embedded != nullptr) {
                            TryExtractEmbeddedTextureBytes(embedded, textureBytes);
                            if (embedded->mFilename.length > 0) {
                                textureKey = embedded->mFilename.C_Str();
                            }
                            if (debugTextures) {
                                std::cout << "    [Texture] " << matNameRaw << " " << paramName
                                          << " embeddedIndex=" << index
                                          << " bytes=" << textureBytes.size() << " formatHint='"
                                          << embedded->achFormatHint << "'\n";
                            }
                        }
                    }
                } else {
                    const std::filesystem::path rawPath(textureKey);
                    const auto                  parent = sourcePath.parent_path();

                    auto readCandidate = [&](const std::filesystem::path& p) -> bool {
                        if (p.empty()) {
                            return false;
                        }
                        if (!ReadFileBytes(p, textureBytes)) {
                            return false;
                        }
                        return !textureBytes.empty();
                    };

                    std::filesystem::path resolvedPath;
                    auto                  tryResolve = [&](const std::filesystem::path& p) -> bool {
                        if (readCandidate(p)) {
                            resolvedPath = p;
                            return true;
                        }
                        return false;
                    };

                    bool loaded = false;
                    if (rawPath.is_absolute()) {
                        loaded = tryResolve(rawPath);
                    } else {
                        loaded = tryResolve(parent / rawPath);
                    }

                    if (!loaded) {
                        const auto fileName = rawPath.filename();
                        loaded              = tryResolve(parent / fileName)
                            || tryResolve(parent / "textures" / fileName)
                            || tryResolve(parent / "Textures" / fileName);
                    }

                    if (!loaded) {
                        // Preserve relative subpath after a ".../textures/..." component if
                        // present.
                        auto                               normalized = rawPath.lexically_normal();
                        std::vector<std::filesystem::path> parts;
                        for (const auto& part : normalized) {
                            parts.push_back(part);
                        }
                        for (i32 i = static_cast<i32>(parts.size()) - 1; i >= 0; --i) {
                            if (parts[static_cast<size_t>(i)] == "textures"
                                || parts[static_cast<size_t>(i)] == "Textures") {
                                std::filesystem::path sub;
                                for (size_t j = static_cast<size_t>(i + 1); j < parts.size(); ++j) {
                                    sub /= parts[j];
                                }
                                if (!sub.empty()) {
                                    loaded = tryResolve(parent / "textures" / sub)
                                        || tryResolve(parent / "Textures" / sub);
                                }
                                break;
                            }
                        }
                    }

                    if (loaded && !resolvedPath.empty() && resolvedPath.has_filename()) {
                        // Use the resolved local filename for stable derived UUIDs / virtual paths.
                        textureKey = resolvedPath.filename().string();
                    }

                    if (debugTextures) {
                        std::cout << "    [Texture] " << matNameRaw << " " << paramName << " file='"
                                  << (resolvedPath.empty() ? rawPath : resolvedPath).string()
                                  << "' bytes=" << textureBytes.size() << "\n";
                    }
                }

                if (textureBytes.empty()) {
                    if (debugTextures) {
                        std::cout << "    [Texture] " << matNameRaw << " " << paramName
                                  << " loadBytes=EMPTY (skipped)\n";
                    }
                    return;
                }

                // Track external texture content in the model cook key so changes to referenced
                // files (or previously-missing files becoming available) invalidate the cook cache.
                const u64 texHash = HashBytes(
                    kFnvOffset, textureBytes.data(), static_cast<size_t>(textureBytes.size()));
                for (u32 b = 0U; b < 8U; ++b) {
                    externalTextureCookKey.push_back(
                        static_cast<u8>((texHash >> (b * 8U)) & 0xFFU));
                }
                const u64 nameHash = HashString(kFnvOffset, textureKey);
                for (u32 b = 0U; b < 8U; ++b) {
                    externalTextureCookKey.push_back(
                        static_cast<u8>((nameHash >> (b * 8U)) & 0xFFU));
                }
                externalTextureCookKey.push_back(srgb ? 1U : 0U);

                const std::string texName = SanitizeName(textureKey, "tex");
                const std::string texVirtual =
                    NormalizeVirtualPath(basePath + "/textures/" + texName);
                if (const auto it = textureByVirtual.find(texVirtual);
                    it != textureByVirtual.end()) {
                    const std::string uuidText = ToStdString(it->second.mUuid.ToNativeString());
                    textureOverrides.push_back({ paramName, uuidText });
                    return;
                }
                const FUuid           texUuid = MakeDerivedUuid(baseHandle.mUuid, texVirtual);

                Asset::FTexture2DDesc texDesc{};
                std::vector<u8>       cookedTex;
                if (!CookTexture2D(textureBytes, srgb, cookedTex, texDesc)) {
                    if (debugTextures) {
                        std::cout << "    [Texture] " << matNameRaw << " " << paramName
                                  << " CookTexture2D=FAIL (maybe unsupported format)\n";
                    }
                    return;
                }

                FGeneratedAsset gen{};
                gen.Handle      = ToAssetHandle(texUuid, Asset::EAssetType::Texture2D);
                gen.Type        = Asset::EAssetType::Texture2D;
                gen.VirtualPath = texVirtual;
                gen.CookedBytes = Move(cookedTex);
                gen.TextureDesc = texDesc;
                generated.push_back(Move(gen));
                textureByVirtual.emplace(
                    texVirtual, ToAssetHandle(texUuid, Asset::EAssetType::Texture2D));

                const std::string uuidText = ToStdString(texUuid.ToNativeString());
                textureOverrides.push_back({ paramName, uuidText });
            };

            resolveTexture(aiTextureType_DIFFUSE, "BaseColorTex", true);
            resolveTexture(aiTextureType_BASE_COLOR, "BaseColorTex", true);
            resolveTexture(aiTextureType_EMISSIVE, "EmissiveTex", true);
            resolveTexture(aiTextureType_NORMALS, "NormalTex", false);
            resolveTexture(aiTextureType_METALNESS, "MetallicTex", false);
            resolveTexture(aiTextureType_DIFFUSE_ROUGHNESS, "RoughnessTex", false);
            resolveTexture(aiTextureType_SPECULAR, "SpecularTex", false);
            resolveTexture(aiTextureType_AMBIENT_OCCLUSION, "OcclusionTex", false);
            resolveTexture(aiTextureType_DISPLACEMENT, "DisplacementTex", false);

            // If the source model doesn't provide a normal map, disable normal mapping so the
            // renderer falls back to vertex normals (avoids incorrect derivative-based TBN on
            // albedo-only assets).
            const bool hasNormalTex = std::any_of(textureOverrides.begin(), textureOverrides.end(),
                [](const auto& p) { return p.first == "NormalTex"; });
            std::vector<std::pair<std::string, double>> scalarOverrides;
            scalarOverrides.push_back({ "NormalMapStrength", hasNormalTex ? 1.0 : 0.0 });

            std::string materialJson;
            if (!BuildMaterialCookedJson(
                    matNameRaw, textureOverrides, vectorOverrides, scalarOverrides, materialJson)) {
                return false;
            }

            FGeneratedAsset materialAsset{};
            materialAsset.Handle      = matHandle;
            materialAsset.Type        = Asset::EAssetType::MaterialTemplate;
            materialAsset.VirtualPath = matVirtual;
            materialAsset.CookedBytes.assign(materialJson.begin(), materialJson.end());
            materialAsset.MaterialDesc.PassCount    = 1U;
            materialAsset.MaterialDesc.ShaderCount  = 0U;
            materialAsset.MaterialDesc.VariantCount = 0U;
            for (const auto& tex : textureOverrides) {
                FUuid                          uuid;
                Core::Container::FNativeString native;
                native.Append(tex.second.c_str(), tex.second.size());
                Core::Container::FNativeStringView view(native.GetData(), native.Length());
                if (FUuid::TryParse(view, uuid)) {
                    materialAsset.Dependencies.push_back(
                        ToAssetHandle(uuid, Asset::EAssetType::Texture2D));
                }
            }
            generated.push_back(Move(materialAsset));
        }

        std::vector<Asset::FModelNodeDesc> nodes;
        nodes.reserve(128);

        auto AddNode = [&](i32 parentIndex, const aiMatrix4x4& transform, i32 meshRefIndex) -> i32 {
            aiVector3D   scaling;
            aiQuaternion rotation;
            aiVector3D   translation;
            transform.Decompose(scaling, rotation, translation);

            Asset::FModelNodeDesc node{};
            node.mParentIndex    = parentIndex;
            node.mMeshRefIndex   = meshRefIndex;
            node.mTranslation[0] = translation.x;
            node.mTranslation[1] = translation.y;
            node.mTranslation[2] = translation.z;
            node.mRotation[0]    = rotation.x;
            node.mRotation[1]    = rotation.y;
            node.mRotation[2]    = rotation.z;
            node.mRotation[3]    = rotation.w;
            node.mScale[0]       = scaling.x;
            node.mScale[1]       = scaling.y;
            node.mScale[2]       = scaling.z;
            nodes.push_back(node);
            return static_cast<i32>(nodes.size() - 1);
        };

        std::function<void(const aiNode*, i32)> Traverse;
        Traverse = [&](const aiNode* node, i32 parentIndex) {
            if (node == nullptr) {
                return;
            }

            i32 nodeIndex = AddNode(parentIndex, node->mTransformation, -1);
            if (node->mNumMeshes > 0) {
                for (u32 i = 0; i < node->mNumMeshes; ++i) {
                    const u32  meshIndex    = node->mMeshes[i];
                    i32        meshRefIndex = -1;
                    const auto it           = meshIndexToRef.find(meshIndex);
                    if (it != meshIndexToRef.end()) {
                        meshRefIndex = static_cast<i32>(it->second);
                    } else if (meshIndex < scene->mNumMeshes) {
                        const aiMesh*    mesh = scene->mMeshes[meshIndex];
                        FMeshBuildResult buildResult{};
                        if (!BuildMeshFromAssimp(mesh, buildResult, importOptions.mFlipTexCoordV)) {
                            return;
                        }

                        Asset::FMeshDesc meshDesc{};
                        std::vector<u8>  meshCooked;
                        if (!BuildMeshBlob(buildResult, meshCooked, meshDesc)) {
                            return;
                        }

                        const std::string meshName =
                            SanitizeName(GetMeshName(mesh, meshIndex), "mesh");
                        const std::string meshVirtual =
                            NormalizeVirtualPath(basePath + "/meshes/" + meshName);
                        const FUuid     meshUuid = MakeDerivedUuid(baseHandle.mUuid, meshVirtual);

                        FGeneratedAsset meshAsset{};
                        meshAsset.Handle      = ToAssetHandle(meshUuid, Asset::EAssetType::Mesh);
                        meshAsset.Type        = Asset::EAssetType::Mesh;
                        meshAsset.VirtualPath = meshVirtual;
                        meshAsset.CookedBytes = Move(meshCooked);
                        meshAsset.MeshDesc    = meshDesc;
                        generated.push_back(Move(meshAsset));

                        Asset::FModelMeshRef meshRef{};
                        meshRef.mMesh = ToAssetHandle(meshUuid, Asset::EAssetType::Mesh);
                        meshRef.mMaterialSlotOffset = static_cast<u32>(materialSlots.size());
                        meshRef.mMaterialSlotCount  = 1U;
                        const u32 materialIndex     = mesh->mMaterialIndex;
                        if (materialIndex < materialHandles.size()) {
                            materialSlots.push_back(materialHandles[materialIndex]);
                        } else {
                            materialSlots.push_back({});
                        }

                        meshRefs.push_back(meshRef);
                        meshRefIndex = static_cast<i32>(meshRefs.size() - 1);
                        meshIndexToRef.emplace(meshIndex, static_cast<u32>(meshRefIndex));
                    }

                    if (meshRefIndex >= 0) {
                        if (i == 0) {
                            nodes[static_cast<size_t>(nodeIndex)].mMeshRefIndex = meshRefIndex;
                        } else {
                            AddNode(nodeIndex, aiMatrix4x4(), meshRefIndex);
                        }
                    }
                }
            }

            for (u32 i = 0; i < node->mNumChildren; ++i) {
                Traverse(node->mChildren[i], nodeIndex);
            }
        };

        Traverse(scene->mRootNode, -1);

        if (!BuildModelBlob(
                nodes, meshRefs, materialSlots, outResult.CookedBytes, outResult.Desc)) {
            return false;
        }

        outResult.Generated = Move(generated);
        outResult.ModelDependencies.reserve(meshRefs.size() + materialHandles.size());
        for (const auto& meshRef : meshRefs) {
            if (meshRef.mMesh.IsValid()) {
                outResult.ModelDependencies.push_back(meshRef.mMesh);
            }
        }
        for (const auto& handle : materialHandles) {
            if (handle.IsValid()) {
                outResult.ModelDependencies.push_back(handle);
            }
        }

        outResult.CookKeyExtras = outResult.CookedBytes;
        if (!externalTextureCookKey.empty()) {
            outResult.CookKeyExtras.insert(outResult.CookKeyExtras.end(),
                externalTextureCookKey.begin(), externalTextureCookKey.end());
        }
        return true;
    }
} // namespace AltinaEngine::Tools::AssetPipeline

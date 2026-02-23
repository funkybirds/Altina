#include "Importers/Model/GltfImporter.h"

#include "AssetToolIO.h"
#include "Utility/Json.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using AltinaEngine::Move;
namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetStringValue;

        auto ToLowerAscii(char value) -> char {
            if (value >= 'A' && value <= 'Z') {
                return static_cast<char>(value - 'A' + 'a');
            }
            return value;
        }

        void ToLowerAscii(std::string& value) {
            for (char& ch : value) {
                ch = ToLowerAscii(ch);
            }
        }

        auto NormalizeExt(const std::filesystem::path& path) -> std::string {
            std::string ext = path.extension().string();
            ToLowerAscii(ext);
            return ext;
        }

        auto AppendFileBytes(const std::filesystem::path& path, std::vector<u8>& ioBytes) -> bool {
            std::vector<u8> bytes;
            if (!ReadFileBytes(path, bytes)) {
                return false;
            }
            ioBytes.insert(ioBytes.end(), bytes.begin(), bytes.end());
            return true;
        }

        auto GetGltfArray(const FJsonValue& root, const char* key) -> const FJsonValue* {
            const FJsonValue* value = FindObjectValueInsensitive(root, key);
            if (value == nullptr || value->Type != EJsonType::Array) {
                return nullptr;
            }
            return value;
        }

        auto GetGltfObject(const FJsonValue& arrayValue, u32 index) -> const FJsonValue* {
            if (arrayValue.Type != EJsonType::Array) {
                return nullptr;
            }
            if (index >= arrayValue.Array.Size()) {
                return nullptr;
            }
            const FJsonValue* obj = arrayValue.Array[index];
            if (obj == nullptr || obj->Type != EJsonType::Object) {
                return nullptr;
            }
            return obj;
        }

        auto AppendGltfCookKeyBytes(
            const std::filesystem::path& sourcePath, std::vector<u8>& outCookKeyBytes) -> bool {
            outCookKeyBytes.clear();

            const std::string ext = NormalizeExt(sourcePath);
            if (ext == ".glb") {
                // For GLB, the single file contains all buffer chunks.
                return AppendFileBytes(sourcePath, outCookKeyBytes);
            }

            if (ext != ".gltf") {
                return false;
            }

            std::string jsonText;
            if (!ReadFileText(sourcePath, jsonText)) {
                return false;
            }

            // Include the .gltf text itself.
            outCookKeyBytes.insert(outCookKeyBytes.end(), jsonText.begin(), jsonText.end());

            Core::Container::FNativeString native;
            native.Append(jsonText.c_str(), jsonText.size());
            const Core::Container::FNativeStringView view(native.GetData(), native.Length());

            FJsonDocument                            document;
            if (!document.Parse(view)) {
                return false;
            }
            const FJsonValue* root = document.GetRoot();
            if (root == nullptr || root->Type != EJsonType::Object) {
                return false;
            }

            const FJsonValue* buffersValue = GetGltfArray(*root, "buffers");
            if (buffersValue == nullptr) {
                return true;
            }

            const std::filesystem::path basePath = sourcePath.parent_path();
            for (u32 i = 0; i < buffersValue->Array.Size(); ++i) {
                const FJsonValue* bufferObj = GetGltfObject(*buffersValue, i);
                if (bufferObj == nullptr) {
                    return false;
                }

                Core::Container::FNativeString uriText;
                if (!GetStringValue(FindObjectValueInsensitive(*bufferObj, "uri"), uriText)
                    && !GetStringValue(FindObjectValueInsensitive(*bufferObj, "Uri"), uriText)) {
                    continue;
                }

                std::string uri = std::string(uriText.GetData(), uriText.Length());
                if (uri.rfind("data:", 0) == 0) {
                    // Data URI contents are already captured in the .gltf JSON text.
                    continue;
                }

                const std::filesystem::path bufferPath = basePath / uri;
                if (!AppendFileBytes(bufferPath, outCookKeyBytes)) {
                    return false;
                }
            }

            return true;
        }

        auto BuildMeshFromAssimp(const aiMesh* mesh, FMeshBuildResult& outMesh) -> bool {
            if (mesh == nullptr || mesh->mNumVertices == 0) {
                return false;
            }
            if (!mesh->HasPositions()) {
                return false;
            }

            const bool includeNormals = mesh->HasNormals();
            const bool includeUv0     = mesh->HasTextureCoords(0);

            u32        offset = 0;
            outMesh.Attributes.clear();
            outMesh.SubMeshes.clear();
            outMesh.VertexFormatMask = 0;

            {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.Semantic      = Asset::kMeshSemanticPosition;
                attr.Format        = Asset::kMeshVertexFormatR32G32B32Float;
                attr.AlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 12;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskPosition;
            }
            if (includeNormals) {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.Semantic      = Asset::kMeshSemanticNormal;
                attr.Format        = Asset::kMeshVertexFormatR32G32B32Float;
                attr.AlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 12;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskNormal;
            }
            if (includeUv0) {
                Asset::FMeshVertexAttributeDesc attr{};
                attr.Semantic      = Asset::kMeshSemanticTexCoord;
                attr.SemanticIndex = 0;
                attr.Format        = Asset::kMeshVertexFormatR32G32Float;
                attr.AlignedOffset = offset;
                outMesh.Attributes.push_back(attr);
                offset += 8;
                outMesh.VertexFormatMask |= Asset::kMeshVertexMaskTexCoord0;
            }

            outMesh.VertexStride = offset;
            outMesh.VertexCount  = static_cast<u32>(mesh->mNumVertices);
            outMesh.VertexData.resize(
                static_cast<size_t>(outMesh.VertexStride) * outMesh.VertexCount);

            bool boundsSet = false;
            for (u32 i = 0; i < outMesh.VertexCount; ++i) {
                u8* dst = outMesh.VertexData.data()
                    + static_cast<size_t>(i) * static_cast<size_t>(outMesh.VertexStride);

                const aiVector3D& p = mesh->mVertices[i];
                const FVec3       pos{ p.x, p.y, p.z };
                std::memcpy(dst, &pos, sizeof(FVec3));

                u32 writeOffset = 12;
                if (includeNormals) {
                    const aiVector3D& n = mesh->mNormals[i];
                    const FVec3       normal{ n.x, n.y, n.z };
                    std::memcpy(dst + writeOffset, &normal, sizeof(FVec3));
                    writeOffset += 12;
                }
                if (includeUv0) {
                    const aiVector3D& uv3 = mesh->mTextureCoords[0][i];
                    const FVec2       uv{ uv3.x, uv3.y };
                    std::memcpy(dst + writeOffset, &uv, sizeof(FVec2));
                }

                if (!boundsSet) {
                    outMesh.BoundsMin[0] = pos.X;
                    outMesh.BoundsMin[1] = pos.Y;
                    outMesh.BoundsMin[2] = pos.Z;
                    outMesh.BoundsMax[0] = pos.X;
                    outMesh.BoundsMax[1] = pos.Y;
                    outMesh.BoundsMax[2] = pos.Z;
                    boundsSet            = true;
                } else {
                    outMesh.BoundsMin[0] = std::min(outMesh.BoundsMin[0], pos.X);
                    outMesh.BoundsMin[1] = std::min(outMesh.BoundsMin[1], pos.Y);
                    outMesh.BoundsMin[2] = std::min(outMesh.BoundsMin[2], pos.Z);
                    outMesh.BoundsMax[0] = std::max(outMesh.BoundsMax[0], pos.X);
                    outMesh.BoundsMax[1] = std::max(outMesh.BoundsMax[1], pos.Y);
                    outMesh.BoundsMax[2] = std::max(outMesh.BoundsMax[2], pos.Z);
                }
            }

            std::vector<u32> indices;
            indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
            for (u32 faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
                const aiFace& face = mesh->mFaces[faceIndex];
                if (face.mNumIndices != 3) {
                    return false;
                }
                indices.push_back(face.mIndices[0]);
                indices.push_back(face.mIndices[1]);
                indices.push_back(face.mIndices[2]);
            }

            if (indices.empty()) {
                return false;
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
            subMesh.IndexStart   = 0;
            subMesh.IndexCount   = outMesh.IndexCount;
            subMesh.BaseVertex   = 0;
            subMesh.MaterialSlot = 0;
            outMesh.SubMeshes    = { subMesh };
            return true;
        }
    } // namespace

    auto CookMeshFromGltf(const std::filesystem::path& sourcePath, FMeshBuildResult& outMesh,
        std::vector<u8>& outCookKeyBytes) -> bool {
        outMesh = {};

        if (!AppendGltfCookKeyBytes(sourcePath, outCookKeyBytes)) {
            return false;
        }

        Assimp::Importer importer;
        const aiScene*   scene = importer.ReadFile(sourcePath.string(),
              aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
                  | aiProcess_ImproveCacheLocality);

        if (scene == nullptr || !scene->HasMeshes()) {
            return false;
        }

        const aiMesh* mesh = scene->mMeshes[0];
        return BuildMeshFromAssimp(mesh, outMesh);
    }
} // namespace AltinaEngine::Tools::AssetPipeline

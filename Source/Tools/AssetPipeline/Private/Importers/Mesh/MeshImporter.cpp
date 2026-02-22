#include "Importers/Mesh/MeshImporter.h"

#include "Importers/Mesh/MeshBuild.h"
#include "Importers/Model/GltfImporter.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

using AltinaEngine::Move;
namespace AltinaEngine::Tools::AssetPipeline {
    namespace {
        struct FObjIndex {
            int  V  = -1;
            int  Vt = -1;
            int  Vn = -1;

            auto operator==(const FObjIndex& other) const noexcept -> bool {
                return V == other.V && Vt == other.Vt && Vn == other.Vn;
            }
        };

        struct FObjIndexHash {
            auto operator()(const FObjIndex& key) const noexcept -> size_t {
                const auto hv = static_cast<size_t>(key.V + 1);
                const auto ht = static_cast<size_t>(key.Vt + 1);
                const auto hn = static_cast<size_t>(key.Vn + 1);
                return (hv * 73856093u) ^ (ht * 19349663u) ^ (hn * 83492791u);
            }
        };

        auto FixObjIndex(int idx, size_t count) -> int {
            if (idx > 0) {
                return idx - 1;
            }
            if (idx < 0) {
                const int fixed = static_cast<int>(count) + idx;
                return fixed >= 0 ? fixed : -1;
            }
            return -1;
        }

        auto ParseObjIndexToken(const std::string& token, size_t vCount, size_t vtCount,
            size_t vnCount, FObjIndex& out) -> bool {
            if (token.empty()) {
                return false;
            }

            int    v  = 0;
            int    vt = 0;
            int    vn = 0;

            size_t firstSlash = token.find('/');
            if (firstSlash == std::string::npos) {
                v = std::stoi(token);
            } else {
                v                  = std::stoi(token.substr(0, firstSlash));
                size_t secondSlash = token.find('/', firstSlash + 1);
                if (secondSlash == std::string::npos) {
                    const auto vtText = token.substr(firstSlash + 1);
                    if (!vtText.empty()) {
                        vt = std::stoi(vtText);
                    }
                } else {
                    const auto vtText = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
                    if (!vtText.empty()) {
                        vt = std::stoi(vtText);
                    }
                    const auto vnText = token.substr(secondSlash + 1);
                    if (!vnText.empty()) {
                        vn = std::stoi(vnText);
                    }
                }
            }

            out.V  = FixObjIndex(v, vCount);
            out.Vt = FixObjIndex(vt, vtCount);
            out.Vn = FixObjIndex(vn, vnCount);
            return out.V >= 0;
        }

        auto BuildMeshBlob(const FMeshBuildResult& mesh, std::vector<u8>& outCooked,
            Asset::FMeshDesc& outDesc) -> bool {
            if (mesh.VertexCount == 0 || mesh.IndexCount == 0 || mesh.VertexStride == 0) {
                return false;
            }

            Asset::FMeshBlobDesc blobDesc{};
            blobDesc.VertexCount    = mesh.VertexCount;
            blobDesc.IndexCount     = mesh.IndexCount;
            blobDesc.VertexStride   = mesh.VertexStride;
            blobDesc.IndexType      = mesh.IndexType;
            blobDesc.AttributeCount = static_cast<u32>(mesh.Attributes.size());
            blobDesc.SubMeshCount   = static_cast<u32>(mesh.SubMeshes.size());
            blobDesc.VertexDataSize = static_cast<u32>(mesh.VertexData.size());
            blobDesc.IndexDataSize  = static_cast<u32>(mesh.IndexData.size());
            blobDesc.BoundsMin[0]   = mesh.BoundsMin[0];
            blobDesc.BoundsMin[1]   = mesh.BoundsMin[1];
            blobDesc.BoundsMin[2]   = mesh.BoundsMin[2];
            blobDesc.BoundsMax[0]   = mesh.BoundsMax[0];
            blobDesc.BoundsMax[1]   = mesh.BoundsMax[1];
            blobDesc.BoundsMax[2]   = mesh.BoundsMax[2];
            blobDesc.Flags          = 1U;

            const u32 attrBytes = blobDesc.AttributeCount * sizeof(Asset::FMeshVertexAttributeDesc);
            const u32 subMeshBytes = blobDesc.SubMeshCount * sizeof(Asset::FMeshSubMeshDesc);

            blobDesc.AttributesOffset = 0;
            blobDesc.SubMeshesOffset  = blobDesc.AttributesOffset + attrBytes;
            blobDesc.VertexDataOffset = blobDesc.SubMeshesOffset + subMeshBytes;
            blobDesc.IndexDataOffset  = blobDesc.VertexDataOffset + blobDesc.VertexDataSize;

            const u32               dataSize = blobDesc.IndexDataOffset + blobDesc.IndexDataSize;

            Asset::FAssetBlobHeader header{};
            header.Type     = static_cast<u8>(Asset::EAssetType::Mesh);
            header.DescSize = static_cast<u32>(sizeof(Asset::FMeshBlobDesc));
            header.DataSize = dataSize;

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
                    writePtr + blobDesc.AttributesOffset, mesh.Attributes.data(), attrBytes);
            }
            if (!mesh.SubMeshes.empty()) {
                std::memcpy(
                    writePtr + blobDesc.SubMeshesOffset, mesh.SubMeshes.data(), subMeshBytes);
            }
            if (!mesh.VertexData.empty()) {
                std::memcpy(writePtr + blobDesc.VertexDataOffset, mesh.VertexData.data(),
                    mesh.VertexData.size());
            }
            if (!mesh.IndexData.empty()) {
                std::memcpy(writePtr + blobDesc.IndexDataOffset, mesh.IndexData.data(),
                    mesh.IndexData.size());
            }

            outDesc.VertexFormat = mesh.VertexFormatMask;
            outDesc.IndexFormat  = mesh.IndexType;
            outDesc.SubMeshCount = blobDesc.SubMeshCount;

            return true;
        }

        auto CookMeshFromObj(const std::filesystem::path& sourcePath, FMeshBuildResult& outMesh)
            -> bool {
            std::ifstream file(sourcePath);
            if (!file) {
                return false;
            }

            std::vector<FVec3>                                positions;
            std::vector<FVec3>                                normals;
            std::vector<FVec2>                                texcoords;

            std::vector<FVec3>                                outPositions;
            std::vector<FVec3>                                outNormals;
            std::vector<FVec2>                                outTexcoords;
            std::vector<u32>                                  indices;
            std::unordered_map<FObjIndex, u32, FObjIndexHash> indexMap;

            bool                                              hasNormal   = false;
            bool                                              hasTexcoord = false;
            bool                                              boundsSet   = false;

            std::string                                       line;
            while (std::getline(file, line)) {
                if (line.empty()) {
                    continue;
                }
                std::istringstream stream(line);
                std::string        tag;
                stream >> tag;
                if (tag == "v") {
                    FVec3 v{};
                    stream >> v.X >> v.Y >> v.Z;
                    positions.push_back(v);
                    continue;
                }
                if (tag == "vn") {
                    FVec3 n{};
                    stream >> n.X >> n.Y >> n.Z;
                    normals.push_back(n);
                    continue;
                }
                if (tag == "vt") {
                    FVec2 t{};
                    stream >> t.X >> t.Y;
                    texcoords.push_back(t);
                    continue;
                }
                if (tag != "f") {
                    continue;
                }

                std::vector<FObjIndex> face;
                std::string            token;
                while (stream >> token) {
                    FObjIndex idx{};
                    if (!ParseObjIndexToken(
                            token, positions.size(), texcoords.size(), normals.size(), idx)) {
                        return false;
                    }
                    if (idx.Vt >= 0) {
                        hasTexcoord = true;
                    }
                    if (idx.Vn >= 0) {
                        hasNormal = true;
                    }
                    face.push_back(idx);
                }

                if (face.size() < 3) {
                    continue;
                }

                auto emitVertex = [&](const FObjIndex& idx) -> u32 {
                    auto it = indexMap.find(idx);
                    if (it != indexMap.end()) {
                        return it->second;
                    }

                    const FVec3 pos  = positions[static_cast<size_t>(idx.V)];
                    const FVec3 norm = (idx.Vn >= 0 && static_cast<size_t>(idx.Vn) < normals.size())
                        ? normals[static_cast<size_t>(idx.Vn)]
                        : FVec3{};
                    const FVec2 uv = (idx.Vt >= 0 && static_cast<size_t>(idx.Vt) < texcoords.size())
                        ? texcoords[static_cast<size_t>(idx.Vt)]
                        : FVec2{};

                    outPositions.push_back(pos);
                    outNormals.push_back(norm);
                    outTexcoords.push_back(uv);

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

                    const u32 newIndex = static_cast<u32>(outPositions.size() - 1);
                    indexMap.emplace(idx, newIndex);
                    return newIndex;
                };

                const FObjIndex& v0 = face[0];
                for (size_t i = 1; i + 1 < face.size(); ++i) {
                    const u32 i0 = emitVertex(v0);
                    const u32 i1 = emitVertex(face[i]);
                    const u32 i2 = emitVertex(face[i + 1]);
                    indices.push_back(i0);
                    indices.push_back(i1);
                    indices.push_back(i2);
                }
            }

            if (outPositions.empty() || indices.empty()) {
                return false;
            }

            const bool includeNormals   = hasNormal;
            const bool includeTexcoords = hasTexcoord;

            u32        offset = 0;
            outMesh.Attributes.clear();
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
            if (includeTexcoords) {
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
            outMesh.VertexCount  = static_cast<u32>(outPositions.size());

            outMesh.VertexData.resize(
                static_cast<size_t>(outMesh.VertexStride) * outMesh.VertexCount);
            for (size_t i = 0; i < outPositions.size(); ++i) {
                u8* dst = outMesh.VertexData.data() + i * static_cast<size_t>(outMesh.VertexStride);
                std::memcpy(dst, &outPositions[i], sizeof(FVec3));
                u32 writeOffset = 12;
                if (includeNormals) {
                    std::memcpy(dst + writeOffset, &outNormals[i], sizeof(FVec3));
                    writeOffset += 12;
                }
                if (includeTexcoords) {
                    std::memcpy(dst + writeOffset, &outTexcoords[i], sizeof(FVec2));
                }
            }

            const u32 maxIndex =
                indices.empty() ? 0U : *std::max_element(indices.begin(), indices.end());
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

    auto CookMesh(const std::filesystem::path& sourcePath, std::vector<u8>& outCooked,
        Asset::FMeshDesc& outDesc, std::vector<u8>& outCookKeyBytes) -> bool {
        const std::string ext      = sourcePath.extension().string();
        std::string       extLower = ext;
        for (char& ch : extLower) {
            if (ch >= 'A' && ch <= 'Z') {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }

        FMeshBuildResult mesh{};
        bool             ok = false;
        if (extLower == ".obj") {
            ok = CookMeshFromObj(sourcePath, mesh);
        } else if (extLower == ".gltf" || extLower == ".glb") {
            ok = CookMeshFromGltf(sourcePath, mesh, outCookKeyBytes);
        } else {
            return false;
        }

        if (!ok) {
            return false;
        }

        return BuildMeshBlob(mesh, outCooked, outDesc);
    }
} // namespace AltinaEngine::Tools::AssetPipeline

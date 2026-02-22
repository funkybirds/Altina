#include "Importers/Model/GltfImporter.h"

#include "AssetToolIO.h"
#include "Container/Span.h"
#include "Container/Vector.h"
#include "Utility/Json.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>

using AltinaEngine::Move;
namespace AltinaEngine::Tools::AssetPipeline {
    namespace Container = Core::Container;
    namespace {
        using Container::FNativeString;
        using Container::FNativeStringView;
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetNumberValue;
        using Core::Utility::Json::GetStringValue;

        struct FGltfBufferView {
            u32 Buffer     = 0;
            u32 ByteOffset = 0;
            u32 ByteLength = 0;
            u32 ByteStride = 0;
        };

        struct FGltfAccessor {
            u32         BufferView    = 0;
            u32         ByteOffset    = 0;
            u32         Count         = 0;
            u32         ComponentType = 0;
            std::string Type;
        };

        auto ToStdString(const FNativeString& value) -> std::string {
            if (value.IsEmptyString()) {
                return {};
            }
            return std::string(value.GetData(), value.Length());
        }

        auto ReadJsonU32(const FJsonValue* value, u32& out) -> bool {
            double number = 0.0;
            if (!GetNumberValue(value, number)) {
                return false;
            }
            if (number < 0.0 || number > static_cast<double>(std::numeric_limits<u32>::max())) {
                return false;
            }
            out = static_cast<u32>(number);
            return true;
        }

        auto ReadGltfBufferUri(const std::filesystem::path& basePath, const std::string& uri,
            std::vector<u8>& outBytes) -> bool {
            if (uri.rfind("data:", 0) == 0) {
                return false;
            }
            const std::filesystem::path bufferPath = basePath / uri;
            return ReadFileBytes(bufferPath, outBytes);
        }

        auto LoadGltfJson(const std::filesystem::path& sourcePath, std::string& outJson,
            std::vector<u8>& outBin) -> bool {
            const auto ext = sourcePath.extension().string();
            if (ext == ".glb") {
                std::vector<u8> bytes;
                if (!ReadFileBytes(sourcePath, bytes)) {
                    return false;
                }
                if (bytes.size() < 12) {
                    return false;
                }
                auto ReadU32 = [&](size_t offset) -> u32 {
                    u32 value = 0;
                    if (offset + sizeof(u32) > bytes.size()) {
                        return 0;
                    }
                    std::memcpy(&value, bytes.data() + offset, sizeof(u32));
                    return value;
                };
                if (ReadU32(0) != 0x46546C67u || ReadU32(4) != 2u) { // "glTF"
                    return false;
                }
                size_t offset = 12;
                outJson.clear();
                outBin.clear();
                while (offset + 8 <= bytes.size()) {
                    const u32 chunkLength = ReadU32(offset);
                    const u32 chunkType   = ReadU32(offset + 4);
                    offset += 8;
                    if (offset + chunkLength > bytes.size()) {
                        return false;
                    }
                    if (chunkType == 0x4E4F534Au) { // JSON
                        outJson.assign(reinterpret_cast<const char*>(bytes.data() + offset),
                            reinterpret_cast<const char*>(bytes.data() + offset + chunkLength));
                    } else if (chunkType == 0x004E4942u) { // BIN
                        outBin.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                            bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
                    }
                    offset += chunkLength;
                }
                return !outJson.empty();
            }

            return ReadFileText(sourcePath, outJson);
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

        auto ReadAccessorFloats(const std::vector<std::vector<u8>>& buffers,
            const std::vector<FGltfBufferView>& views, const std::vector<FGltfAccessor>& accessors,
            u32 accessorIndex, u32 expectedComponents, std::vector<float>& outValues) -> bool {
            if (accessorIndex >= accessors.size()) {
                return false;
            }
            const auto& accessor = accessors[accessorIndex];
            if (accessor.ComponentType != 5126u) {
                return false;
            }
            if (accessor.Count == 0) {
                return false;
            }
            if (accessor.BufferView >= views.size()) {
                return false;
            }
            const auto& view = views[accessor.BufferView];
            if (view.Buffer >= buffers.size()) {
                return false;
            }
            const auto& buffer = buffers[view.Buffer];

            u32         components = 0;
            if (accessor.Type == "VEC2") {
                components = 2;
            } else if (accessor.Type == "VEC3") {
                components = 3;
            } else if (accessor.Type == "VEC4") {
                components = 4;
            } else if (accessor.Type == "SCALAR") {
                components = 1;
            }

            if (components != expectedComponents) {
                return false;
            }

            const u32 componentSize = 4;
            const u32 stride =
                (view.ByteStride != 0) ? view.ByteStride : (components * componentSize);
            const u64 baseOffset = static_cast<u64>(view.ByteOffset) + accessor.ByteOffset;
            const u64 required   = static_cast<u64>(stride) * (accessor.Count - 1)
                + static_cast<u64>(components * componentSize);
            if (baseOffset + required > buffer.size()) {
                return false;
            }

            outValues.resize(static_cast<size_t>(accessor.Count) * components);
            for (u32 i = 0; i < accessor.Count; ++i) {
                const u64 srcOffset = baseOffset + static_cast<u64>(i) * stride;
                std::memcpy(outValues.data() + static_cast<size_t>(i) * components,
                    buffer.data() + static_cast<size_t>(srcOffset), components * componentSize);
            }
            return true;
        }

        auto ReadAccessorIndices(const std::vector<std::vector<u8>>& buffers,
            const std::vector<FGltfBufferView>& views, const std::vector<FGltfAccessor>& accessors,
            u32 accessorIndex, std::vector<u32>& outIndices) -> bool {
            if (accessorIndex >= accessors.size()) {
                return false;
            }
            const auto& accessor = accessors[accessorIndex];
            if (accessor.Count == 0) {
                return false;
            }
            if (accessor.BufferView >= views.size()) {
                return false;
            }
            const auto& view = views[accessor.BufferView];
            if (view.Buffer >= buffers.size()) {
                return false;
            }
            const auto& buffer = buffers[view.Buffer];

            u32         componentSize = 0;
            if (accessor.ComponentType == 5123u) {
                componentSize = 2;
            } else if (accessor.ComponentType == 5125u) {
                componentSize = 4;
            } else {
                return false;
            }

            const u32 stride     = (view.ByteStride != 0) ? view.ByteStride : componentSize;
            const u64 baseOffset = static_cast<u64>(view.ByteOffset) + accessor.ByteOffset;
            const u64 required =
                static_cast<u64>(stride) * (accessor.Count - 1) + static_cast<u64>(componentSize);
            if (baseOffset + required > buffer.size()) {
                return false;
            }

            outIndices.resize(accessor.Count);
            for (u32 i = 0; i < accessor.Count; ++i) {
                const u64 srcOffset = baseOffset + static_cast<u64>(i) * stride;
                if (componentSize == 2) {
                    u16 value = 0;
                    std::memcpy(
                        &value, buffer.data() + static_cast<size_t>(srcOffset), sizeof(u16));
                    outIndices[i] = value;
                } else {
                    u32 value = 0;
                    std::memcpy(
                        &value, buffer.data() + static_cast<size_t>(srcOffset), sizeof(u32));
                    outIndices[i] = value;
                }
            }
            return true;
        }
    } // namespace

    auto CookMeshFromGltf(const std::filesystem::path& sourcePath, FMeshBuildResult& outMesh,
        std::vector<u8>& outCookKeyBytes) -> bool {
        std::string     jsonText;
        std::vector<u8> binChunk;
        if (!LoadGltfJson(sourcePath, jsonText, binChunk)) {
            return false;
        }

        FNativeString native;
        native.Append(jsonText.c_str(), jsonText.size());
        const FNativeStringView view(native.GetData(), native.Length());

        FJsonDocument           document;
        if (!document.Parse(view)) {
            return false;
        }

        const FJsonValue* root = document.GetRoot();
        if (root == nullptr || root->Type != EJsonType::Object) {
            return false;
        }

        const auto* buffersValue = GetGltfArray(*root, "buffers");
        if (buffersValue == nullptr) {
            return false;
        }

        std::vector<std::vector<u8>> buffers;
        buffers.reserve(buffersValue->Array.Size());

        const std::filesystem::path basePath = sourcePath.parent_path();
        for (u32 i = 0; i < buffersValue->Array.Size(); ++i) {
            const FJsonValue* bufferObj = GetGltfObject(*buffersValue, i);
            if (bufferObj == nullptr) {
                return false;
            }

            std::vector<u8> bufferBytes;
            FNativeString   uriText;
            if (GetStringValue(FindObjectValueInsensitive(*bufferObj, "Uri"), uriText)) {
                const std::string uri = ToStdString(uriText);
                if (!ReadGltfBufferUri(basePath, uri, bufferBytes)) {
                    return false;
                }
            } else {
                if (i != 0 || binChunk.empty()) {
                    return false;
                }
                bufferBytes = binChunk;
            }

            outCookKeyBytes.insert(outCookKeyBytes.end(), bufferBytes.begin(), bufferBytes.end());
            buffers.push_back(Move(bufferBytes));
        }

        const auto* bufferViewsValue = GetGltfArray(*root, "bufferViews");
        if (bufferViewsValue == nullptr) {
            return false;
        }

        std::vector<FGltfBufferView> bufferViews;
        bufferViews.reserve(bufferViewsValue->Array.Size());
        for (u32 i = 0; i < bufferViewsValue->Array.Size(); ++i) {
            const FJsonValue* viewObj = GetGltfObject(*bufferViewsValue, i);
            if (viewObj == nullptr) {
                return false;
            }

            FGltfBufferView bufferView{};
            if (!ReadJsonU32(FindObjectValueInsensitive(*viewObj, "Buffer"), bufferView.Buffer)
                || !ReadJsonU32(
                    FindObjectValueInsensitive(*viewObj, "ByteLength"), bufferView.ByteLength)) {
                return false;
            }
            ReadJsonU32(FindObjectValueInsensitive(*viewObj, "ByteOffset"), bufferView.ByteOffset);
            ReadJsonU32(FindObjectValueInsensitive(*viewObj, "ByteStride"), bufferView.ByteStride);
            bufferViews.push_back(bufferView);
        }

        const auto* accessorsValue = GetGltfArray(*root, "accessors");
        if (accessorsValue == nullptr) {
            return false;
        }

        std::vector<FGltfAccessor> accessors;
        accessors.reserve(accessorsValue->Array.Size());
        for (u32 i = 0; i < accessorsValue->Array.Size(); ++i) {
            const FJsonValue* accessorObj = GetGltfObject(*accessorsValue, i);
            if (accessorObj == nullptr) {
                return false;
            }

            FGltfAccessor accessor{};
            if (!ReadJsonU32(
                    FindObjectValueInsensitive(*accessorObj, "BufferView"), accessor.BufferView)
                || !ReadJsonU32(FindObjectValueInsensitive(*accessorObj, "ComponentType"),
                    accessor.ComponentType)
                || !ReadJsonU32(
                    FindObjectValueInsensitive(*accessorObj, "Count"), accessor.Count)) {
                return false;
            }
            ReadJsonU32(
                FindObjectValueInsensitive(*accessorObj, "ByteOffset"), accessor.ByteOffset);

            FNativeString typeText;
            if (!GetStringValue(FindObjectValueInsensitive(*accessorObj, "Type"), typeText)) {
                return false;
            }
            accessor.Type = ToStdString(typeText);
            accessors.push_back(accessor);
        }

        const auto* meshesValue = GetGltfArray(*root, "meshes");
        if (meshesValue == nullptr || meshesValue->Array.IsEmpty()) {
            return false;
        }
        const FJsonValue* meshObj = GetGltfObject(*meshesValue, 0);
        if (meshObj == nullptr) {
            return false;
        }

        const auto* primsValue = GetGltfArray(*meshObj, "primitives");
        if (primsValue == nullptr || primsValue->Array.IsEmpty()) {
            return false;
        }
        const FJsonValue* primObj = GetGltfObject(*primsValue, 0);
        if (primObj == nullptr) {
            return false;
        }

        u32 mode = 4;
        ReadJsonU32(FindObjectValueInsensitive(*primObj, "Mode"), mode);
        if (mode != 4) {
            return false;
        }

        const FJsonValue* attrsObj = FindObjectValueInsensitive(*primObj, "Attributes");
        if (attrsObj == nullptr || attrsObj->Type != EJsonType::Object) {
            return false;
        }

        u32 positionAccessor = 0;
        if (!ReadJsonU32(FindObjectValueInsensitive(*attrsObj, "POSITION"), positionAccessor)) {
            return false;
        }

        u32 normalAccessor = std::numeric_limits<u32>::max();
        ReadJsonU32(FindObjectValueInsensitive(*attrsObj, "NORMAL"), normalAccessor);
        u32 uvAccessor = std::numeric_limits<u32>::max();
        ReadJsonU32(FindObjectValueInsensitive(*attrsObj, "TEXCOORD_0"), uvAccessor);

        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> uvs;
        if (!ReadAccessorFloats(buffers, bufferViews, accessors, positionAccessor, 3, positions)) {
            return false;
        }
        if (normalAccessor != std::numeric_limits<u32>::max()) {
            if (!ReadAccessorFloats(buffers, bufferViews, accessors, normalAccessor, 3, normals)) {
                return false;
            }
        }
        if (uvAccessor != std::numeric_limits<u32>::max()) {
            if (!ReadAccessorFloats(buffers, bufferViews, accessors, uvAccessor, 2, uvs)) {
                return false;
            }
        }

        const u32 vertexCount = static_cast<u32>(positions.size() / 3);
        if (vertexCount == 0) {
            return false;
        }
        if (!normals.empty() && normals.size() / 3 != vertexCount) {
            return false;
        }
        if (!uvs.empty() && uvs.size() / 2 != vertexCount) {
            return false;
        }

        std::vector<u32> indices;
        u32              indicesAccessor = std::numeric_limits<u32>::max();
        ReadJsonU32(FindObjectValueInsensitive(*primObj, "Indices"), indicesAccessor);
        if (indicesAccessor != std::numeric_limits<u32>::max()) {
            if (!ReadAccessorIndices(buffers, bufferViews, accessors, indicesAccessor, indices)) {
                return false;
            }
        } else {
            if (vertexCount % 3 != 0) {
                return false;
            }
            indices.resize(vertexCount);
            for (u32 i = 0; i < vertexCount; ++i) {
                indices[i] = i;
            }
        }

        if (indices.empty() || (indices.size() % 3 != 0)) {
            return false;
        }

        const bool includeNormals   = !normals.empty();
        const bool includeTexcoords = !uvs.empty();

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
        outMesh.VertexCount  = vertexCount;

        outMesh.VertexData.resize(static_cast<size_t>(outMesh.VertexStride) * vertexCount);
        bool boundsSet = false;
        for (u32 i = 0; i < vertexCount; ++i) {
            u8* dst = outMesh.VertexData.data()
                + static_cast<size_t>(i) * static_cast<size_t>(outMesh.VertexStride);
            const FVec3 pos{ positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2] };
            std::memcpy(dst, &pos, sizeof(FVec3));
            u32 writeOffset = 12;
            if (includeNormals) {
                const FVec3 n{ normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2] };
                std::memcpy(dst + writeOffset, &n, sizeof(FVec3));
                writeOffset += 12;
            }
            if (includeTexcoords) {
                const FVec2 uv{ uvs[i * 2], uvs[i * 2 + 1] };
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
} // namespace AltinaEngine::Tools::AssetPipeline

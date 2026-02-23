
#include "Importers/Model/FbxImporter.h"

#include "Importers/Model/AssimpModelImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookModelFromFbx(const std::filesystem::path& sourcePath,
        const Asset::FAssetHandle& baseHandle, const std::string& baseVirtualPath,
        FModelCookResult& outResult, std::string& outError) -> bool {
        Assimp::Importer importer;
        const aiScene*   scene = importer.ReadFile(sourcePath.string(),
              aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality
                  | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights);
        if (scene == nullptr || scene->mRootNode == nullptr) {
            outError = "Assimp failed to load FBX.";
            return false;
        }

        return CookModelFromAssimpScene(
            sourcePath, scene, baseHandle, baseVirtualPath, outResult, outError);
    }
} // namespace AltinaEngine::Tools::AssetPipeline

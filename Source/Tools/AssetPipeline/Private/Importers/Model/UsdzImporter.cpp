
#include "Importers/Model/UsdzImporter.h"

#include "Importers/Model/AssimpModelImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookModelFromUsdz(const std::filesystem::path& sourcePath,
        const Asset::FAssetHandle& baseHandle, const std::string& baseVirtualPath,
        FModelCookResult& outResult, std::string& outError) -> bool {
        Assimp::Importer importer;
        const aiScene*   scene = importer.ReadFile(sourcePath.string(),
              aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality
                  | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace
                  | aiProcess_LimitBoneWeights
                  // Our default raster state culls front faces in the main deferred passes.
                  // Some USDZ assets come in with the opposite winding compared to the demo's
                  // reference meshes, resulting in them being fully culled. Flip winding so
                  // imported geometry is visible under the engine's convention.
                  | aiProcess_FlipWindingOrder);
        if (scene == nullptr || scene->mRootNode == nullptr) {
            outError = "Assimp failed to load USDZ: ";
            outError += importer.GetErrorString();
            return false;
        }

        return CookModelFromAssimpScene(
            sourcePath, scene, baseHandle, baseVirtualPath, outResult, outError);
    }
} // namespace AltinaEngine::Tools::AssetPipeline

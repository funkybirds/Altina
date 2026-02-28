#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace {
    struct FCommandLine {
        std::string                        Command;
        std::map<std::string, std::string> Options;
    };

    static auto ParseCommandLine(int argc, char** argv, FCommandLine& out, std::string& outError)
        -> bool {
        if (argc < 2) {
            outError = "Missing command.";
            return false;
        }

        out.Command = argv[1];
        for (int index = 2; index < argc; ++index) {
            std::string arg = argv[index];
            if (arg.rfind("--", 0) == 0) {
                std::string key   = arg.substr(2);
                std::string value = "true";
                if (index + 1 < argc && std::string(argv[index + 1]).rfind("--", 0) != 0) {
                    value = argv[index + 1];
                    ++index;
                }
                out.Options[key] = value;
            }
        }

        return true;
    }

    static void PrintUsage() {
        std::cout << "ProjectGenerator commands:\n";
        std::cout << "  new-demo --root <RepoRoot> --name <DemoName> [--out-root <Path>]\n";
        std::cout << "          [--managed <true|false>] [--update-cmake <true|false>] [--force]\n";
    }

    static auto ToLowerAscii(std::string value) -> std::string {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    }

    static auto IsValidDemoName(std::string_view name) -> bool {
        if (name.empty()) {
            return false;
        }
        for (const char ch : name) {
            const bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
                || (ch >= '0' && ch <= '9') || (ch == '_');
            if (!ok) {
                return false;
            }
        }
        return true;
    }

    static auto ParseBool(std::string_view value, bool defaultValue) -> bool {
        if (value.empty()) {
            return defaultValue;
        }
        const std::string v = ToLowerAscii(std::string(value));
        if (v == "1" || v == "true" || v == "yes" || v == "on") {
            return true;
        }
        if (v == "0" || v == "false" || v == "no" || v == "off") {
            return false;
        }
        return defaultValue;
    }

    static auto ReadFileText(const std::filesystem::path& path, std::string& out) -> bool {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return false;
        }
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        out = buffer.str();
        return true;
    }

    static auto WriteFileText(
        const std::filesystem::path& path, std::string_view text, std::string& outError) -> bool {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            outError = "Failed to create directory: " + path.parent_path().string();
            return false;
        }

        std::ofstream stream(path, std::ios::binary);
        if (!stream) {
            outError = "Failed to open for write: " + path.string();
            return false;
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream) {
            outError = "Failed to write file: " + path.string();
            return false;
        }
        return true;
    }

    static auto ReplaceAll(std::string text, std::string_view from, std::string_view to)
        -> std::string {
        if (from.empty()) {
            return text;
        }
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, from.size(), to);
            pos += to.size();
        }
        return text;
    }

    struct FDemoTemplateContext {
        std::string DemoName;
        std::string DemoNameLower;
        std::string ManagedAssemblyName;
    };

    static auto MakeContext(std::string_view demoName) -> FDemoTemplateContext {
        FDemoTemplateContext ctx{};
        ctx.DemoName            = std::string(demoName);
        ctx.DemoNameLower       = ToLowerAscii(std::string(demoName));
        ctx.ManagedAssemblyName = "AltinaEngine.Demo." + ctx.DemoName;
        return ctx;
    }

    static auto ApplyTemplate(std::string_view templ, const FDemoTemplateContext& ctx)
        -> std::string {
        std::string out(templ);
        out = ReplaceAll(std::move(out), "@DEMO_NAME@", ctx.DemoName);
        out = ReplaceAll(std::move(out), "@DEMO_NAME_LOWER@", ctx.DemoNameLower);
        out = ReplaceAll(std::move(out), "@MANAGED_ASSEMBLY@", ctx.ManagedAssemblyName);
        return out;
    }

    static auto EnsureEmptyDirectory(
        const std::filesystem::path& dir, bool force, std::string& outError) -> bool {
        std::error_code ec;
        if (std::filesystem::exists(dir, ec)) {
            if (!force) {
                outError =
                    "Directory already exists: " + dir.string() + " (pass --force to overwrite)";
                return false;
            }
            std::filesystem::remove_all(dir, ec);
            if (ec) {
                outError = "Failed to remove existing directory: " + dir.string();
                return false;
            }
        }

        std::filesystem::create_directories(dir, ec);
        if (ec) {
            outError = "Failed to create directory: " + dir.string();
            return false;
        }
        return true;
    }

    static auto UpdateRootCMakeLists(const std::filesystem::path& repoRoot,
        std::string_view demoName, std::string& outError) -> bool {
        const std::filesystem::path cmakePath = repoRoot / "CMakeLists.txt";
        std::string                 text;
        if (!ReadFileText(cmakePath, text)) {
            outError = "Failed to read: " + cmakePath.string();
            return false;
        }

        const std::string needle = "add_subdirectory(Demo/" + std::string(demoName) + ")";
        if (text.find(needle) != std::string::npos) {
            return true;
        }

        size_t insertPos = std::string::npos;
        size_t searchPos = 0;
        while (true) {
            const size_t pos = text.find("add_subdirectory(Demo/", searchPos);
            if (pos == std::string::npos) {
                break;
            }
            const size_t end = text.find('\n', pos);
            insertPos        = (end == std::string::npos) ? text.size() : (end + 1);
            searchPos        = (end == std::string::npos) ? text.size() : (end + 1);
        }

        if (insertPos == std::string::npos) {
            const size_t toolsPos = text.find("add_subdirectory(Source/Tools/AssetPipeline)");
            if (toolsPos != std::string::npos) {
                const size_t end = text.find('\n', toolsPos);
                insertPos        = (end == std::string::npos) ? text.size() : (end + 1);
            } else {
                insertPos = text.size();
                if (!text.empty() && text.back() != '\n') {
                    text.push_back('\n');
                }
            }
        }

        const std::string line = "add_subdirectory(Demo/" + std::string(demoName) + ")\n";
        text.insert(insertPos, line);

        return WriteFileText(cmakePath, text, outError);
    }

    static constexpr const char* kDemoCMakeTemplate = R"cmake(
set(TargetName AltinaEngineDemo@DEMO_NAME@)

add_executable(${TargetName})

option(AE_DEMO_ENABLE_SHIPPING "Stage Demo/<Name>/Shipping from Demo/<Name>/Binaries (copies Assets + *.exe/*.dll only)" ON)

set(Target_Private_Sources
    Source/Main.cpp
)

target_sources(${TargetName}
    PRIVATE
        ${Target_Private_Sources}
)

target_link_libraries(${TargetName}
    PRIVATE
        AltinaEngine::Launch
        AltinaEngine::ShaderCompiler
        AltinaEngine::Gameplay
)

if(WIN32)
    target_link_libraries(${TargetName}
        PRIVATE
            AltinaEngine::Rhi::D3D11
            d3dcompiler
            dxguid
    )
endif()

target_compile_features(${TargetName}
    PRIVATE
        cxx_std_23
)

set(DemoBinaryDir ${CMAKE_SOURCE_DIR}/Demo/@DEMO_NAME@/Binaries)
set(DemoShippingDir ${CMAKE_CURRENT_SOURCE_DIR}/Shipping)
set(DemoAssetCookOutput ${DemoBinaryDir}/Assets/Registry/AssetRegistry.json)
set(DemoAssetsRoot ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(DemoConfigRoot ${CMAKE_CURRENT_SOURCE_DIR}/Config)
set(DemoCookedRoot ${DemoBinaryDir}/Assets)
set(EngineShaderRoot ${CMAKE_SOURCE_DIR}/Source/Shader)
set(DemoStagedShaderRoot ${DemoBinaryDir}/Assets/Shader)
set(AE_DEMO_STAGE_SHIPPING_SCRIPT ${CMAKE_SOURCE_DIR}/Scripts/StageDemoShipping.cmake)
file(GLOB_RECURSE DemoAssetSources CONFIGURE_DEPENDS
    ${DemoAssetsRoot}/*
)
file(GLOB_RECURSE EngineShaderSources CONFIGURE_DEPENDS
    ${EngineShaderRoot}/*
)

set(DemoPreCleanTarget ${TargetName}PreClean)
add_custom_target(${DemoPreCleanTarget}
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DemoBinaryDir}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DemoShippingDir}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DemoBinaryDir}"
    COMMENT "Cleaning @DEMO_NAME@ demo output folders (Binaries/Shipping)"
    VERBATIM
)
set_property(TARGET ${DemoPreCleanTarget} PROPERTY FOLDER "Demo/@DEMO_NAME@")

set_target_properties(${TargetName}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${DemoBinaryDir}
        FOLDER "Demo/@DEMO_NAME@"
)

add_custom_command(
    TARGET ${TargetName}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TargetName}>
        ${DemoBinaryDir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}/Assets/Config
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${DemoConfigRoot} ${DemoBinaryDir}/Assets/Config
    COMMENT "Staging @DEMO_NAME@ demo executable"
)

set(DemoShaderStageStamp ${CMAKE_BINARY_DIR}/Demo/@DEMO_NAME@/StageShaders.stamp)
add_custom_command(
    OUTPUT ${DemoShaderStageStamp}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${EngineShaderRoot} ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E touch ${DemoShaderStageStamp}
    DEPENDS ${EngineShaderSources}
    COMMENT "Staging engine shaders for @DEMO_NAME@ demo"
    VERBATIM
)
add_custom_target(AltinaEngineDemo@DEMO_NAME@StageShaders ALL
    DEPENDS ${DemoShaderStageStamp}
)
set_property(TARGET AltinaEngineDemo@DEMO_NAME@StageShaders PROPERTY FOLDER "Demo/@DEMO_NAME@")
add_dependencies(${TargetName} AltinaEngineDemo@DEMO_NAME@StageShaders)

if(WIN32)
    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${TargetName}>
            ${DemoBinaryDir}
        COMMAND_EXPAND_LISTS
        COMMENT "Staging @DEMO_NAME@ demo runtime DLLs"
    )
endif()

if(AE_DEMO_ENABLE_SHIPPING)
    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging @DEMO_NAME@ demo Shipping folder"
        VERBATIM
    )

    add_custom_command(
        TARGET AltinaEngineDemo@DEMO_NAME@StageShaders
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging @DEMO_NAME@ demo Shipping folder (shader changes)"
        VERBATIM
    )
endif()

find_program(AE_DOTNET_HOST dotnet)
if(AE_DOTNET_HOST)
    set(AE_DEMO_MANAGED_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Source/Managed/@MANAGED_ASSEMBLY@)
    set(AE_DEMO_MANAGED_PROJECT ${AE_DEMO_MANAGED_ROOT}/@MANAGED_ASSEMBLY@.csproj)
    set(AE_DEMO_MANAGED_OUT_DIR ${CMAKE_BINARY_DIR}/Demo/@DEMO_NAME@/Managed)
    set(AE_DEMO_MANAGED_STAMP ${AE_DEMO_MANAGED_OUT_DIR}/@MANAGED_ASSEMBLY@.build.stamp)

    if(EXISTS ${AE_DEMO_MANAGED_PROJECT})
        add_custom_command(
            OUTPUT ${AE_DEMO_MANAGED_STAMP}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${AE_DEMO_MANAGED_OUT_DIR}
            COMMAND ${AE_DOTNET_HOST} build ${AE_DEMO_MANAGED_PROJECT}
                -o ${AE_DEMO_MANAGED_OUT_DIR}
                -c $<IF:$<CONFIG:Debug>,Debug,Release>
                /t:Rebuild
                /p:NoIncremental=true
                /p:MSBuildProjectExtensionsPath=${AE_DEMO_MANAGED_OUT_DIR}/obj/
                /p:BaseIntermediateOutputPath=${AE_DEMO_MANAGED_OUT_DIR}/obj/
                /p:IntermediateOutputPath=${AE_DEMO_MANAGED_OUT_DIR}/obj/$<IF:$<CONFIG:Debug>,Debug,Release>/
            COMMAND ${CMAKE_COMMAND} -E touch ${AE_DEMO_MANAGED_STAMP}
            DEPENDS
                ${AE_DEMO_MANAGED_PROJECT}
                ${AE_DEMO_MANAGED_ROOT}/DemoScript.cs
                ${CMAKE_SOURCE_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.csproj
                ${CMAKE_SOURCE_DIR}/Source/Managed/AltinaEngine.Managed/ManagedLog.cs
            BYPRODUCTS
                ${AE_DEMO_MANAGED_OUT_DIR}/@MANAGED_ASSEMBLY@.dll
                ${AE_DEMO_MANAGED_OUT_DIR}/@MANAGED_ASSEMBLY@.runtimeconfig.json
                ${AE_DEMO_MANAGED_OUT_DIR}/@MANAGED_ASSEMBLY@.deps.json
                ${AE_DEMO_MANAGED_OUT_DIR}/@MANAGED_ASSEMBLY@.pdb
            COMMENT "Building @DEMO_NAME@ demo managed scripting assembly"
            VERBATIM
        )

        add_custom_target(AltinaEngineDemo@DEMO_NAME@ManagedBuild
            DEPENDS ${AE_DEMO_MANAGED_STAMP}
        )
        set_property(TARGET AltinaEngineDemo@DEMO_NAME@ManagedBuild PROPERTY BUILD_ALWAYS TRUE)
        add_dependencies(${TargetName} AltinaEngineDemo@DEMO_NAME@ManagedBuild)
        if(TARGET AltinaEngineManagedBuild)
            add_dependencies(AltinaEngineDemo@DEMO_NAME@ManagedBuild AltinaEngineManagedBuild)
        endif()

        add_custom_command(
            TARGET AltinaEngineDemo@DEMO_NAME@ManagedBuild
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${AE_DEMO_MANAGED_OUT_DIR}/@MANAGED_ASSEMBLY@.dll
                ${DemoBinaryDir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${AE_DEMO_MANAGED_OUT_DIR}/@MANAGED_ASSEMBLY@.runtimeconfig.json
                ${DemoBinaryDir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${AE_DEMO_MANAGED_OUT_DIR}/@MANAGED_ASSEMBLY@.deps.json
                ${DemoBinaryDir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${AE_DEMO_MANAGED_OUT_DIR}/@MANAGED_ASSEMBLY@.pdb
                ${DemoBinaryDir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.dll
                ${DemoBinaryDir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.host.runtimeconfig.json
                ${DemoBinaryDir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.runtimeconfig.json
                ${DemoBinaryDir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.deps.json
                ${DemoBinaryDir}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.pdb
                ${DemoBinaryDir}
            COMMENT "Staging @DEMO_NAME@ demo managed scripting assembly"
        )
    else()
        message(STATUS "@DEMO_NAME@ managed project not found; skipping managed build.")
    endif()
else()
    message(STATUS "dotnet not found; skipping @DEMO_NAME@ demo managed build.")
endif()

add_custom_command(
    OUTPUT ${DemoAssetCookOutput}
    COMMAND $<TARGET_FILE:AltinaEngineAssetTool> cook
        --root ${CMAKE_SOURCE_DIR}
        --platform Win64
        --demo @DEMO_NAME@
        --build-root ${CMAKE_BINARY_DIR}
        --cook-root ${DemoCookedRoot}
    DEPENDS
        AltinaEngineAssetTool
        ${DemoAssetSources}
    COMMENT "Cooking @DEMO_NAME@ demo assets"
    VERBATIM
)

add_custom_target(AltinaEngineDemo@DEMO_NAME@CookAssets
    DEPENDS ${DemoAssetCookOutput}
)

if(AE_DEMO_ENABLE_SHIPPING)
    add_custom_command(
        TARGET AltinaEngineDemo@DEMO_NAME@CookAssets
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging @DEMO_NAME@ demo Shipping folder (asset changes)"
        VERBATIM
    )
endif()

add_dependencies(${TargetName} AltinaEngineDemo@DEMO_NAME@CookAssets)
)cmake";

    static constexpr const char* kDemoMainCppTemplate = R"cpp(
#include "Base/AltinaBase.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Launch/EngineLoop.h"
#include "Launch/GameClient.h"
#include "Logging/Log.h"
#include "Math/Common.h"
#include "Math/Rotation.h"
#include "Math/Vector.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rendering/PostProcess/PostProcessSettings.h"
#include "Types/Aliases.h"

using namespace AltinaEngine;

namespace {
    class F@DEMO_NAME@GameClient final : public Launch::FGameClient {
    public:
        auto OnInit(Launch::FEngineLoop& engineLoop) -> bool override {
            Rendering::rPostProcessFxaa.Set(1);

            const auto meshHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/@DEMO_NAME_LOWER@/models/triangle"));
            const auto materialHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/@DEMO_NAME_LOWER@/materials/simple_deferred"));
            const auto scriptHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/@DEMO_NAME_LOWER@/scripts/demoscript"));

            if (!meshHandle.IsValid() || !materialHandle.IsValid()) {
                LogError(TEXT("@DEMO_NAME@ demo assets missing (mesh or material)."));
                return false;
            }

            auto&      worldManager = engineLoop.GetWorldManager();
            const auto worldHandle  = worldManager.CreateWorld();
            worldManager.SetActiveWorld(worldHandle);
            auto* world = worldManager.GetWorld(worldHandle);
            if (world == nullptr) {
                LogError(TEXT("@DEMO_NAME@ demo world creation failed."));
                return false;
            }

            {
                auto cameraObject    = world->CreateGameObject(TEXT("Camera"));
                auto cameraComponent = cameraObject.AddComponent<GameScene::FCameraComponent>();
                auto scriptComponent = cameraObject.AddComponent<GameScene::FScriptComponent>();

                if (cameraComponent.IsValid()) {
                    auto& camera = cameraComponent.Get();
                    camera.SetNearPlane(0.1f);
                    camera.SetFarPlane(1000.0f);

                    auto t = cameraObject.GetWorldTransform();
                    t.Translation = Core::Math::FVector3f(0.0f, 1.0f, -4.0f);
                    t.Rotation    = Core::Math::FQuaternion::Identity();
                    cameraObject.SetWorldTransform(t);
                }

                if (scriptHandle.IsValid() && scriptComponent.IsValid()) {
                    scriptComponent.Get().SetScriptAsset(scriptHandle);
                }
            }

            {
                auto triObject = world->CreateGameObject(TEXT("Triangle"));

                auto t = triObject.GetWorldTransform();
                t.Translation = Core::Math::FVector3f(0.0f, 0.0f, 0.0f);
                t.Rotation    = Core::Math::FQuaternion::Identity();
                triObject.SetWorldTransform(t);

                auto meshFilter = triObject.AddComponent<GameScene::FStaticMeshFilterComponent>();
                if (meshFilter.IsValid()) {
                    meshFilter.Get().SetStaticMeshAsset(meshHandle);
                }

                auto materialComp = triObject.AddComponent<GameScene::FMeshMaterialComponent>();
                if (materialComp.IsValid()) {
                    materialComp.Get().SetMaterialTemplate(0, materialHandle);
                }
            }

            {
                auto lightObject = world->CreateGameObject(TEXT("DirectionalLight"));
                auto lightComponent =
                    lightObject.AddComponent<GameScene::FDirectionalLightComponent>();
                if (lightComponent.IsValid()) {
                    auto& light        = lightComponent.Get();
                    light.mColor       = Core::Math::FVector3f(1.0f, 1.0f, 1.0f);
                    light.mIntensity   = 2.0f;
                    light.mCastShadows = true;

                    auto t = lightObject.GetWorldTransform();
                    t.Rotation = Core::Math::FEulerRotator(Core::Math::kPiF * 0.25f, 0.0f, 0.0f)
                                     .ToQuaternion();
                    lightObject.SetWorldTransform(t);
                }
            }

            return true;
        }

        auto OnTick(Launch::FEngineLoop& engineLoop, float deltaSeconds) -> bool override {
            engineLoop.Tick(deltaSeconds);
            Core::Platform::Generic::PlatformSleepMilliseconds(16);
            return engineLoop.IsRunning();
        }
    };
} // namespace

int main(int argc, char** argv) {
    FStartupParameters startupParams{};
    if (argc > 1) {
        startupParams.mCommandLine = argv[1];
    }

    F@DEMO_NAME@GameClient client;
    return Launch::RunGameClient(client, startupParams);
}
)cpp";

    static constexpr const char* kDefaultGameJsonTemplate = R"json(
{
    "Graphics":{
        "PreferredApis":["DX11","Vulkan"],
        "DebugMode":true
    },
    "GameClient":{
        "ResolutionX":1280,
        "ResolutionY":720
    }
}
)json";

    static constexpr const char* kTriangleObjTemplate = R"obj(
# AltinaEngine Demo Triangle
o Triangle
v -0.5 0.0 0.0
v  0.5 0.0 0.0
v  0.0 1.0 0.0
vn 0.0 0.0 1.0
f 1//1 2//1 3//1
)obj";

    static constexpr const char* kSimpleDeferredHlslTemplate = R"hlsl(
// @altina raster_state {
//     cull = none;
// }

AE_PER_FRAME_CBUFFER(ViewConstants)
{
    row_major float4x4 ViewProjection;
};

AE_PER_DRAW_CBUFFER(ObjectConstants)
{
    row_major float4x4 World;
    row_major float4x4 NormalMatrix;
};

AE_PER_MATERIAL_CBUFFER(MaterialConstants)
{
    float4 BaseColor;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Albedo   : COLOR0;
    float3 NormalWS : TEXCOORD0;
};

VSOutput VSBase(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(World, float4(input.Position, 1.0f));
    output.Position = mul(ViewProjection, worldPos);
    output.Albedo   = BaseColor;
    output.NormalWS = normalize(mul((float3x3)NormalMatrix, input.Normal));
    return output;
}

struct PSOutput
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
};

PSOutput PSBase(VSOutput input)
{
    PSOutput output;
    output.Albedo = input.Albedo;
    output.Normal = float4(normalize(input.NormalWS) * 0.5f + 0.5f, 1.0f);
    return output;
}
)hlsl";

    static constexpr const char* kSimpleDeferredMaterialTemplate = R"material(
{
  "name": "SimpleDeferred",
  "passes": {
    "BasePass": {
      "shaders": {
        "vs": { "asset": "demo/@DEMO_NAME_LOWER@/shaders/simple_deferred", "entry": "VSBase" },
        "ps": { "asset": "demo/@DEMO_NAME_LOWER@/shaders/simple_deferred", "entry": "PSBase" }
      },
      "overrides":{
        "BaseColor":{
          "type":"float4",
          "value":[0.2,0.6,1.0,1.0]
        }
      }
    }
  },
  "precompile_variants": []
}
)material";

    static constexpr const char* kDemoScriptAssetTemplate = R"script(
{
  "AssemblyPath": "@MANAGED_ASSEMBLY@.dll",
  "TypeName": "@MANAGED_ASSEMBLY@.DemoScript, @MANAGED_ASSEMBLY@"
}
)script";

    static constexpr const char* kManagedCsprojTemplate = R"csproj(
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
    <GenerateRuntimeConfigurationFiles>true</GenerateRuntimeConfigurationFiles>
    <ImplicitUsings>disable</ImplicitUsings>
    <Nullable>enable</Nullable>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="**\\*.cs" Exclude="obj\\**\\*;bin\\**\\*" />
    <ProjectReference Include="..\\..\\..\\..\\..\\Source\\Managed\\AltinaEngine.Managed\\AltinaEngine.Managed.csproj" />
  </ItemGroup>
</Project>
)csproj";

    static constexpr const char* kManagedDemoScriptCsTemplate = R"cs(
using System;
using AltinaEngine.Managed;

namespace @MANAGED_ASSEMBLY@;

public sealed class DemoScript : ScriptComponent
{
    private float _elapsedSeconds;

    public override void OnCreate()
    {
        ManagedLog.Info("[DemoScript] OnCreate");
    }

    public override void Tick(float dt)
    {
        _elapsedSeconds += dt;
        if (_elapsedSeconds >= 1.0f)
        {
            _elapsedSeconds = 0.0f;
            ManagedLog.Info($"[DemoScript] Tick mouse=({Input.MouseX},{Input.MouseY})");
        }

        if (Input.WasKeyPressed(EKey.Space))
        {
            ManagedLog.Info("[DemoScript] Space pressed (managed).");
        }
    }
}
)cs";

    static auto                  CreateDemoProject(const std::filesystem::path& repoRoot,
                         const std::filesystem::path& outRoot, const FDemoTemplateContext& ctx, bool managed,
                         bool updateCmake, bool force, std::string& outError) -> bool {
        const std::filesystem::path demoRoot = outRoot / "Demo" / ctx.DemoName;
        if (!EnsureEmptyDirectory(demoRoot, force, outError)) {
            return false;
        }

        // CMakeLists.txt
        {
            const std::string cmakeText = ApplyTemplate(kDemoCMakeTemplate, ctx);
            if (!WriteFileText(demoRoot / "CMakeLists.txt", cmakeText, outError)) {
                return false;
            }
        }

        // Source/Main.cpp
        {
            const std::string mainCpp = ApplyTemplate(kDemoMainCppTemplate, ctx);
            if (!WriteFileText(demoRoot / "Source" / "Main.cpp", mainCpp, outError)) {
                return false;
            }
        }

        // Config
        {
            if (!WriteFileText(
                    demoRoot / "Config" / "DefaultGame.json", kDefaultGameJsonTemplate, outError)) {
                return false;
            }
        }

        // Assets
        {
            if (!WriteFileText(demoRoot / "Assets" / ".gitkeep", "", outError)) {
                return false;
            }
            if (!WriteFileText(demoRoot / "Assets" / "Models" / "Triangle.obj",
                                     kTriangleObjTemplate, outError)) {
                return false;
            }
            if (!WriteFileText(demoRoot / "Assets" / "Shaders" / "Simple_Deferred.hlsl",
                                     kSimpleDeferredHlslTemplate, outError)) {
                return false;
            }
            const std::string matText = ApplyTemplate(kSimpleDeferredMaterialTemplate, ctx);
            if (!WriteFileText(demoRoot / "Assets" / "Materials" / "Simple_Deferred.material",
                                     matText, outError)) {
                return false;
            }
        }

        // Optional managed scripting.
        if (managed) {
            const std::string scriptText = ApplyTemplate(kDemoScriptAssetTemplate, ctx);
            if (!WriteFileText(
                    demoRoot / "Assets" / "Scripts" / "DemoScript.script", scriptText, outError)) {
                return false;
            }

            const std::filesystem::path managedRoot =
                demoRoot / "Source" / "Managed" / ctx.ManagedAssemblyName;
            const std::string csprojText = ApplyTemplate(kManagedCsprojTemplate, ctx);
            if (!WriteFileText(
                    managedRoot / (ctx.ManagedAssemblyName + ".csproj"), csprojText, outError)) {
                return false;
            }

            const std::string scriptCs = ApplyTemplate(kManagedDemoScriptCsTemplate, ctx);
            if (!WriteFileText(managedRoot / "DemoScript.cs", scriptCs, outError)) {
                return false;
            }

            static constexpr const char* kManagedGitIgnore = "bin/\nobj/\n";
            if (!WriteFileText(managedRoot / ".gitignore", kManagedGitIgnore, outError)) {
                return false;
            }
        }

        if (updateCmake) {
            if (!UpdateRootCMakeLists(repoRoot, ctx.DemoName, outError)) {
                return false;
            }
        }

        // Minimal validation: ensure the CMake target name is consistent with the folder.
        {
            std::string cmakeText;
            if (!ReadFileText(demoRoot / "CMakeLists.txt", cmakeText)
                || cmakeText.find("AltinaEngineDemo" + ctx.DemoName) == std::string::npos) {
                outError = "Generated CMakeLists.txt validation failed.";
                return false;
            }
        }

        return true;
    }

    static auto RunNewDemo(const FCommandLine& command) -> int {
        const std::filesystem::path repoRoot = command.Options.contains("root")
            ? std::filesystem::path(command.Options.at("root"))
            : std::filesystem::current_path();

        const std::string           demoName =
            command.Options.contains("name") ? command.Options.at("name") : std::string();
        if (!IsValidDemoName(demoName)) {
            std::cerr
                << "Invalid or missing demo name (use --name <DemoName>; allowed: A-Z a-z 0-9 _)\n";
            return 1;
        }

        const std::filesystem::path outRoot = command.Options.contains("out-root")
            ? std::filesystem::path(command.Options.at("out-root"))
            : repoRoot;

        std::string_view            managedOpt;
        if (const auto it = command.Options.find("managed"); it != command.Options.end()) {
            managedOpt = it->second;
        }
        const bool managed = ParseBool(managedOpt, true);

        const bool force = command.Options.contains("force")
            ? ParseBool(command.Options.at("force"), true)
            : false;

        bool       updateCmake = false;
        if (command.Options.contains("update-cmake")) {
            updateCmake = ParseBool(command.Options.at("update-cmake"), true);
        } else {
            std::error_code ec;
            updateCmake = std::filesystem::equivalent(outRoot, repoRoot, ec) && !ec;
        }

        const FDemoTemplateContext ctx = MakeContext(demoName);

        std::string                error;
        if (!CreateDemoProject(repoRoot, outRoot, ctx, managed, updateCmake, force, error)) {
            std::cerr << "Failed: " << error << "\n";
            return 1;
        }

        std::cout << "Created demo: Demo/" << ctx.DemoName << "\n";
        if (updateCmake) {
            std::cout << "Updated root CMakeLists.txt.\n";
        }
        return 0;
    }
} // namespace

int main(int argc, char** argv) {
    FCommandLine command{};
    std::string  error;
    if (!ParseCommandLine(argc, argv, command, error)) {
        std::cerr << "Error: " << error << "\n";
        PrintUsage();
        return 1;
    }

    if (command.Command == "new-demo") {
        return RunNewDemo(command);
    }

    std::cerr << "Unknown command: " << command.Command << "\n";
    PrintUsage();
    return 1;
}

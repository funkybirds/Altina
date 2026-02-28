set(TargetName SpaceshipGame)

add_executable(${TargetName})

option(AE_DEMO_ENABLE_SHIPPING "Stage Demo/<Name>/Shipping from Demo/<Name>/Binaries (copies runtime payload recursively; excludes debug artifacts)" ON)

set(Target_Private_Sources
    Source/Main.cpp
    Source/NativeScripts/ShipCameraModesNative.cpp
    Source/NativeScripts/ShipOrbitControllerNative.cpp
    Source/NativeScripts/SpaceshipNativeContext.cpp
    Source/NativeScripts/SpaceshipNativeScripts.cpp
)

target_sources(${TargetName}
    PRIVATE
        ${Target_Private_Sources}
)

target_link_libraries(${TargetName}
    PRIVATE
        AltinaEngine::Launch
        AltinaEngine::DebugGui
        AltinaEngine::ShaderCompiler
        AltinaEngine::Gameplay
)

if(TARGET AltinaEngineDemoLauncher)
    add_dependencies(${TargetName} AltinaEngineDemoLauncher)
endif()

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

set(DemoBinaryDir ${CMAKE_SOURCE_DIR}/Demo/SpaceshipGame/Binaries)
set(DemoShippingDir ${CMAKE_CURRENT_SOURCE_DIR}/Shipping)
set(DemoAssetCookOutput ${DemoBinaryDir}/Assets/Registry/AssetRegistry.json)
set(DemoAssetsRoot ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(DemoConfigRoot ${CMAKE_CURRENT_SOURCE_DIR}/Config)
set(DemoCookedRoot ${DemoBinaryDir}/Assets)
set(EngineShaderRoot ${CMAKE_SOURCE_DIR}/Source/Shader)
set(DemoStagedShaderRoot ${DemoBinaryDir}/Assets/Shader)
set(AE_DEMO_STAGE_SHIPPING_SCRIPT ${CMAKE_SOURCE_DIR}/Scripts/StageDemoShippingRuntime.cmake)
file(GLOB_RECURSE DemoAssetSources CONFIGURE_DEPENDS
    ${DemoAssetsRoot}/*
)
file(GLOB_RECURSE EngineShaderSources CONFIGURE_DEPENDS
    ${EngineShaderRoot}/*
)

# Clean demo output folders at the very start of a demo build (ignore if missing).
# This avoids stale files in Demo/<Name>/Binaries and Demo/<Name>/Shipping.
set(DemoPreCleanTarget ${TargetName}PreClean)
add_custom_target(${DemoPreCleanTarget}
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DemoBinaryDir}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DemoShippingDir}"
    # Ensure the linker has a valid output directory even if other staging steps are up-to-date.
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DemoBinaryDir}"
    COMMENT "Cleaning SpaceshipGame demo output folders (Binaries/Shipping)"
    VERBATIM
)
set_property(TARGET ${DemoPreCleanTarget} PROPERTY FOLDER "Demo/SpaceshipGame")

set_target_properties(${TargetName}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${DemoBinaryDir}
        FOLDER "Demo/SpaceshipGame"
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
    COMMENT "Staging SpaceshipGame demo executable"
)

# Put the stamp under the demo output tree so the PreClean target removes it, forcing restaging.
set(DemoShaderStageStamp ${DemoBinaryDir}/Assets/Shader/StageShaders.stamp)
add_custom_command(
    OUTPUT ${DemoShaderStageStamp}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${EngineShaderRoot} ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E touch ${DemoShaderStageStamp}
    DEPENDS ${EngineShaderSources}
    COMMENT "Staging engine shaders for SpaceshipGame demo"
    VERBATIM
)
set(DemoStageShadersTarget ${TargetName}StageShaders)
add_custom_target(${DemoStageShadersTarget} ALL
    DEPENDS ${DemoShaderStageStamp}
)
set_property(TARGET ${DemoStageShadersTarget} PROPERTY FOLDER "Demo/SpaceshipGame")
add_dependencies(${TargetName} ${DemoStageShadersTarget})
if(TARGET AltinaEngineDemoLauncher)
    add_dependencies(${DemoStageShadersTarget} AltinaEngineDemoLauncher)
endif()

if(WIN32)
    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${TargetName}>
            ${DemoBinaryDir}
        COMMAND_EXPAND_LISTS
        COMMENT "Staging SpaceshipGame demo runtime DLLs"
    )
endif()

if(AE_DEMO_ENABLE_SHIPPING)
    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -DDEMO_EXE_NAME:STRING=$<TARGET_FILE_NAME:${TargetName}>
            -DDEMO_LAUNCHER_EXE:FILEPATH=$<TARGET_FILE:AltinaEngineDemoLauncher>
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging SpaceshipGame demo Shipping folder"
        VERBATIM
    )

    # Keep Shipping in sync even when only assets/shaders change (the executable may not relink,
    # so its POST_BUILD step would not run).
    add_custom_command(
        TARGET ${DemoStageShadersTarget}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -DDEMO_EXE_NAME:STRING=$<TARGET_FILE_NAME:${TargetName}>
            -DDEMO_LAUNCHER_EXE:FILEPATH=$<TARGET_FILE:AltinaEngineDemoLauncher>
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging SpaceshipGame demo Shipping folder (shader changes)"
        VERBATIM
    )
endif()

add_custom_command(
    OUTPUT ${DemoAssetCookOutput}
    COMMAND $<TARGET_FILE:AltinaEngineAssetTool> cook
        --root ${CMAKE_SOURCE_DIR}
        --platform Win64
        --demo SpaceshipGame
        --build-root ${CMAKE_BINARY_DIR}
        --cook-root ${DemoCookedRoot}
    DEPENDS
        AltinaEngineAssetTool
        ${DemoAssetSources}
    COMMENT "Cooking SpaceshipGame demo assets"
    VERBATIM
)

set(DemoCookAssetsTarget ${TargetName}CookAssets)
add_custom_target(${DemoCookAssetsTarget}
    DEPENDS ${DemoAssetCookOutput}
)
if(TARGET AltinaEngineDemoLauncher)
    add_dependencies(${DemoCookAssetsTarget} AltinaEngineDemoLauncher)
endif()

if(AE_DEMO_ENABLE_SHIPPING)
    add_custom_command(
        TARGET ${DemoCookAssetsTarget}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -DDEMO_EXE_NAME:STRING=$<TARGET_FILE_NAME:${TargetName}>
            -DDEMO_LAUNCHER_EXE:FILEPATH=$<TARGET_FILE:AltinaEngineDemoLauncher>
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging SpaceshipGame demo Shipping folder (asset changes)"
        VERBATIM
    )
endif()

add_dependencies(${TargetName} ${DemoCookAssetsTarget})

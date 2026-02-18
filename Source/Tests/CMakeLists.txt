set(AE_TESTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(AE_TESTS_SHARED_DIR ${AE_TESTS_ROOT}/Shared)

set(AE_TEST_SHARED_SOURCES
    ${AE_TESTS_SHARED_DIR}/TestMain.cpp
)

function(ae_add_test_target TargetName)
    set(oneValueArgs FOLDER)
    set(multiValueArgs SOURCES LIBS)
    cmake_parse_arguments(AE_TEST "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${TargetName}
        ${AE_TEST_SHARED_SOURCES}
        ${AE_TEST_SOURCES}
    )
    target_link_libraries(${TargetName} PRIVATE ${AE_TEST_LIBS})
    target_include_directories(${TargetName} PRIVATE ${AE_TESTS_SHARED_DIR})
    target_compile_definitions(${TargetName} PRIVATE AE_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")

    if(AE_TEST_FOLDER)
        set_target_properties(${TargetName} PROPERTIES FOLDER "Tests/${AE_TEST_FOLDER}")
    else()
        set_target_properties(${TargetName} PROPERTIES FOLDER "Tests")
    endif()

    add_test(NAME ${TargetName} COMMAND $<TARGET_FILE:${TargetName}>)
endfunction()

file(GLOB_RECURSE AE_TEST_CORE_SOURCES
    CONFIGURE_DEPENDS
    ${AE_TESTS_ROOT}/Core/*.cpp
)

ae_add_test_target(AltinaEngineTestsCore
    FOLDER "Core"
    SOURCES ${AE_TEST_CORE_SOURCES}
    LIBS AltinaEngine::Core
)

file(GLOB_RECURSE AE_TEST_RENDERCORE_SOURCES
    CONFIGURE_DEPENDS
    ${AE_TESTS_ROOT}/RenderCore/*.cpp
)

ae_add_test_target(AltinaEngineTestsRenderCore
    FOLDER "RenderCore"
    SOURCES ${AE_TEST_RENDERCORE_SOURCES}
    LIBS
        AltinaEngine::RenderCore
        AltinaEngine::Rhi::Mock
)

file(GLOB_RECURSE AE_TEST_RHI_MOCK_SOURCES
    CONFIGURE_DEPENDS
    ${AE_TESTS_ROOT}/Rhi/Mock/*.cpp
)

ae_add_test_target(AltinaEngineTestsRhiMock
    FOLDER "Rhi/Mock"
    SOURCES ${AE_TEST_RHI_MOCK_SOURCES}
    LIBS AltinaEngine::Rhi::Mock
)

if(WIN32)
    file(GLOB_RECURSE AE_TEST_RHI_D3D11_SOURCES
        CONFIGURE_DEPENDS
        ${AE_TESTS_ROOT}/Rhi/D3D11/*.cpp
    )

    ae_add_test_target(AltinaEngineTestsRhiD3D11
        FOLDER "Rhi/D3D11"
        SOURCES ${AE_TEST_RHI_D3D11_SOURCES}
        LIBS
            AltinaEngine::Rhi::D3D11
            d3dcompiler
    )
endif()

file(GLOB_RECURSE AE_TEST_SHADER_COMPILER_SOURCES
    CONFIGURE_DEPENDS
    ${AE_TESTS_ROOT}/ShaderCompiler/*.cpp
)

ae_add_test_target(AltinaEngineTestsShaderCompiler
    FOLDER "ShaderCompiler"
    SOURCES ${AE_TEST_SHADER_COMPILER_SOURCES}
    LIBS
        AltinaEngine::ShaderCompiler
        AltinaEngine::Rhi::Mock
)

file(GLOB_RECURSE AE_TEST_IMAGING_SOURCES
    CONFIGURE_DEPENDS
    ${AE_TESTS_ROOT}/Imaging/*.cpp
)

ae_add_test_target(AltinaEngineTestsImaging
    FOLDER "Imaging"
    SOURCES ${AE_TEST_IMAGING_SOURCES}
    LIBS AltinaEngine::Imaging
)

file(GLOB_RECURSE AE_TEST_ASSET_SOURCES
    CONFIGURE_DEPENDS
    ${AE_TESTS_ROOT}/Asset/*.cpp
)

ae_add_test_target(AltinaEngineTestsAsset
    FOLDER "Asset"
    SOURCES ${AE_TEST_ASSET_SOURCES}
    LIBS AltinaEngine::Asset
)

if(TARGET AltinaEngineAssetTool)
    if(WIN32)
        set(AE_ASSET_COOK_PLATFORM "Win64")
    else()
        set(AE_ASSET_COOK_PLATFORM "Linux")
    endif()

    add_test(NAME AltinaEngineAssetCook
        COMMAND $<TARGET_FILE:AltinaEngineAssetTool> cook
            --root ${CMAKE_SOURCE_DIR}
            --platform ${AE_ASSET_COOK_PLATFORM}
            --demo Minimal
    )

    set_tests_properties(AltinaEngineAssetCook PROPERTIES
        FIXTURES_SETUP AssetCook
    )
    set_tests_properties(AltinaEngineTestsAsset PROPERTIES
        FIXTURES_REQUIRED AssetCook
    )

    add_dependencies(AltinaEngineTestsAsset AltinaEngineAssetTool)
endif()

file(GLOB_RECURSE AE_TEST_GAMESCENE_SOURCES
    CONFIGURE_DEPENDS
    ${AE_TESTS_ROOT}/GameScene/*.cpp
)

ae_add_test_target(AltinaEngineTestsGameScene
    FOLDER "GameScene"
    SOURCES ${AE_TEST_GAMESCENE_SOURCES}
    LIBS AltinaEngine::Engine
)

if(AE_ENABLE_SCRIPTING_CORECLR)
    find_program(AE_DOTNET_HOST dotnet)
    if(AE_DOTNET_HOST)
        set(AE_SCRIPTING_TEST_ROOT ${AE_TESTS_ROOT}/Scripting)
        set(AE_SCRIPTING_MANAGED_PROJECT
            ${AE_SCRIPTING_TEST_ROOT}/Managed/AltinaEngine.Scripting.Tests/AltinaEngine.Scripting.Tests.csproj)
        set(AE_SCRIPTING_MANAGED_OUT_DIR ${CMAKE_BINARY_DIR}/Source/Tests/ScriptingManaged)
        set(AE_SCRIPTING_MANAGED_DLL
            ${AE_SCRIPTING_MANAGED_OUT_DIR}/AltinaEngine.Scripting.Tests.dll)

        add_custom_command(
            OUTPUT ${AE_SCRIPTING_MANAGED_DLL}
            COMMAND ${AE_DOTNET_HOST} build ${AE_SCRIPTING_MANAGED_PROJECT}
                -o ${AE_SCRIPTING_MANAGED_OUT_DIR}
            DEPENDS
                ${AE_SCRIPTING_MANAGED_PROJECT}
                ${AE_SCRIPTING_TEST_ROOT}/Managed/AltinaEngine.Scripting.Tests/InteropEntry.cs
            COMMENT "Building managed CoreCLR interop test assembly"
            VERBATIM
        )

        add_custom_target(AltinaEngineScriptingManagedBuild
            DEPENDS ${AE_SCRIPTING_MANAGED_DLL}
        )

        file(GLOB_RECURSE AE_TEST_SCRIPTING_SOURCES
            CONFIGURE_DEPENDS
            ${AE_SCRIPTING_TEST_ROOT}/CoreCLR/*.cpp
        )

        ae_add_test_target(AltinaEngineTestsScriptingCoreCLR
            FOLDER "Scripting/CoreCLR"
            SOURCES ${AE_TEST_SCRIPTING_SOURCES}
            LIBS
                AltinaEngine::Scripting
                AltinaEngine::Scripting::CoreCLR
        )

        add_dependencies(AltinaEngineTestsScriptingCoreCLR AltinaEngineScriptingManagedBuild)

        add_custom_command(
            TARGET AltinaEngineTestsScriptingCoreCLR
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${AE_SCRIPTING_MANAGED_OUT_DIR}/AltinaEngine.Scripting.Tests.dll
                $<TARGET_FILE_DIR:AltinaEngineTestsScriptingCoreCLR>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${AE_SCRIPTING_MANAGED_OUT_DIR}/AltinaEngine.Scripting.Tests.runtimeconfig.json
                $<TARGET_FILE_DIR:AltinaEngineTestsScriptingCoreCLR>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${AE_SCRIPTING_MANAGED_OUT_DIR}/AltinaEngine.Scripting.Tests.deps.json
                $<TARGET_FILE_DIR:AltinaEngineTestsScriptingCoreCLR>
            COMMENT "Staging managed interop test assets"
        )
    else()
        message(STATUS "dotnet not found; skipping CoreCLR scripting interop tests.")
    endif()
endif()



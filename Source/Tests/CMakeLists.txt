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

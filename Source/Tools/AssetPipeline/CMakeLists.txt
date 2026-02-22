set(TargetName AltinaEngineAssetTool)

include(ExternalProject)

set(AssimpSourceDir "${CMAKE_SOURCE_DIR}/ThirdParty/assimp")
set(AssimpBuildDir "${CMAKE_BINARY_DIR}/assimp-build")
file(MAKE_DIRECTORY "${AssimpBuildDir}/include")

if(MSVC)
    if(MSVC_TOOLSET_VERSION)
        set(_assimp_msvc_prefix "vc${MSVC_TOOLSET_VERSION}")
    else()
        if(MSVC_VERSION LESS 1910)
            set(_assimp_msvc_prefix "vc140")
        elseif(MSVC_VERSION LESS 1920)
            set(_assimp_msvc_prefix "vc141")
        elseif(MSVC_VERSION LESS 1930)
            set(_assimp_msvc_prefix "vc142")
        else()
            set(_assimp_msvc_prefix "vc150")
        endif()
    endif()
    set(_assimp_libname "assimp-${_assimp_msvc_prefix}-mt.lib")
else()
    set(_assimp_libname "${CMAKE_STATIC_LIBRARY_PREFIX}assimp${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

set(_assimp_libdir "${AssimpBuildDir}/lib")
set(_assimp_byproduct "${_assimp_libdir}/${CMAKE_CFG_INTDIR}/${_assimp_libname}")

ExternalProject_Add(assimp_ext
    SOURCE_DIR "${AssimpSourceDir}"
    BINARY_DIR "${AssimpBuildDir}"
    CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF
        -DASSIMP_NO_EXPORT=ON
        -DASSIMP_BUILD_TESTS=OFF
        -DASSIMP_BUILD_ASSIMP_TOOLS=OFF
        -DASSIMP_INSTALL=OFF
        -DASSIMP_WARNINGS_AS_ERRORS=OFF
        -DASSIMP_INJECT_DEBUG_POSTFIX=OFF
    BUILD_BYPRODUCTS "${_assimp_byproduct}"
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(assimp_ext SOURCE_DIR BINARY_DIR)

add_library(assimp::assimp STATIC IMPORTED GLOBAL)
add_dependencies(assimp::assimp assimp_ext)

set_target_properties(assimp::assimp PROPERTIES
    IMPORTED_LOCATION "${_assimp_byproduct}"
    INTERFACE_INCLUDE_DIRECTORIES "${SOURCE_DIR}/include;${BINARY_DIR}/include"
)

add_executable(${TargetName})

add_dependencies(${TargetName} assimp_ext)

set(Target_Private_Sources
    Private/AssetToolMain.cpp
    Private/AssetToolIO.cpp
    Private/Importers/Audio/AudioImporter.cpp
    Private/Importers/Material/MaterialImporter.cpp
    Private/Importers/Mesh/MeshImporter.cpp
    Private/Importers/Model/GltfImporter.cpp
    Private/Importers/Model/ModelImporter.cpp
    Private/Importers/Shader/ShaderImporter.cpp
    Private/Importers/Texture/TextureImporter.cpp
)

target_sources(${TargetName}
    PRIVATE
        ${Target_Private_Sources}
)

target_include_directories(${TargetName}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
)

target_link_libraries(${TargetName}
    PRIVATE
        AltinaEngine::Core
        AltinaEngine::Asset
        AltinaEngine::Imaging
        assimp::assimp
)

target_compile_features(${TargetName}
    PRIVATE
        cxx_std_23
)

set(Warnings
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311 /w14826 /w14905 /w14906 /w14928>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(${TargetName}
    PRIVATE
        ${Warnings}
)

add_custom_command(
    TARGET ${TargetName}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:AltinaEngineCore>
        $<TARGET_FILE_DIR:${TargetName}>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:AltinaEngineAsset>
        $<TARGET_FILE_DIR:${TargetName}>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:AltinaEngineImaging>
        $<TARGET_FILE_DIR:${TargetName}>
    COMMENT "Staging AssetTool dependencies"
)

set_target_properties(${TargetName}
    PROPERTIES
        OUTPUT_NAME AssetTool
        FOLDER "Tools/AssetPipeline"
)

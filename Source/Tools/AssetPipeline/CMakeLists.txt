set(TargetName AltinaEngineAssetTool)

include(ExternalProject)

set(AssimpSourceDir "${CMAKE_SOURCE_DIR}/ThirdParty/assimp")
set(AssimpBuildDir "${CMAKE_BINARY_DIR}/assimp-build")
file(MAKE_DIRECTORY "${AssimpBuildDir}/include")

if(MSVC)
    # Assimp's suffix policy can lag behind the host toolset (for example VS 2026 may still
    # emit "assimp-vc143-mt.lib"). Resolve the actual output after assimp_ext is built.
    set(_assimp_libname "assimp-selected-mt.lib")
else()
    set(_assimp_libname "${CMAKE_STATIC_LIBRARY_PREFIX}assimp${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

set(_assimp_libdir "${AssimpBuildDir}/lib")
set(_assimp_byproduct "${_assimp_libdir}/${_assimp_libname}")
set(_assimp_zlib_libdir "${AssimpBuildDir}/contrib/zlib")
set(_assimp_zlib_byproduct "${_assimp_zlib_libdir}/zlibstatic${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(_assimp_external_byproducts "${_assimp_zlib_byproduct}")
if(NOT MSVC)
    list(APPEND _assimp_external_byproducts "${_assimp_byproduct}")
endif()

set(_assimp_cmake_args
    # Bump this value to force the ExternalProject configure step to re-run when assimp fork
    # sources change, even if other configure args remain the same.
    -DAE_ASSIMP_SUPERBUILD_REV=5
    -DBUILD_SHARED_LIBS=OFF
    -DASSIMP_NO_EXPORT=ON
    -DASSIMP_BUILD_TESTS=OFF
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF
    # Enable USDZ (*.usdz) import via Assimp's experimental USD importer (tinyusdz backend).
    # This is required by the SpaceshipGame demo to ingest provided USDZ models.
    -DASSIMP_BUILD_USD_IMPORTER=ON
    -DASSIMP_BUILD_ZLIB=ON
    -DASSIMP_BUILD_MINIZIP=ON
    -DASSIMP_INSTALL=OFF
    -DASSIMP_WARNINGS_AS_ERRORS=OFF
    -DASSIMP_INJECT_DEBUG_POSTFIX=OFF
)

if (CMAKE_BUILD_TYPE)
    list(APPEND _assimp_cmake_args -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
endif()
if (MSVC)
    list(APPEND _assimp_cmake_args
        -DCMAKE_C_FLAGS_DEBUG=/MDd
        -DCMAKE_C_FLAGS_RELWITHDEBINFO=/MD
        -DCMAKE_C_FLAGS_RELEASE=/MD
        -DCMAKE_C_FLAGS_MINSIZEREL=/MD
        -DCMAKE_CXX_FLAGS_DEBUG=/MDd
        -DCMAKE_CXX_FLAGS_RELWITHDEBINFO=/MD
        -DCMAKE_CXX_FLAGS_RELEASE=/MD
        -DCMAKE_CXX_FLAGS_MINSIZEREL=/MD
    )

endif()

ExternalProject_Add(assimp_ext
    SOURCE_DIR "${AssimpSourceDir}"
    BINARY_DIR "${AssimpBuildDir}"
    CMAKE_ARGS ${_assimp_cmake_args}
    BUILD_BYPRODUCTS ${_assimp_external_byproducts}
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(assimp_ext SOURCE_DIR BINARY_DIR)

add_library(assimp::assimp STATIC IMPORTED GLOBAL)
add_dependencies(assimp::assimp assimp_ext)

add_library(zlib::zlibstatic STATIC IMPORTED GLOBAL)
add_dependencies(zlib::zlibstatic assimp_ext)

if(MSVC)
    set(_assimp_resolve_script "${CMAKE_CURRENT_SOURCE_DIR}/CMake/ResolveAssimpLibrary.cmake")
    add_custom_command(
        OUTPUT "${_assimp_byproduct}"
        COMMAND ${CMAKE_COMMAND}
            -DASSIMP_LIB_DIR:PATH=${_assimp_libdir}
            -DASSIMP_SELECTED_LIB:FILEPATH=${_assimp_byproduct}
            -P "${_assimp_resolve_script}"
        DEPENDS assimp_ext
        COMMENT "Resolving assimp static library output name"
        VERBATIM
    )
    add_custom_target(assimp_resolved_lib DEPENDS "${_assimp_byproduct}")
    add_dependencies(assimp_resolved_lib assimp_ext)
    add_dependencies(assimp::assimp assimp_resolved_lib)
endif()

set_target_properties(assimp::assimp PROPERTIES
    IMPORTED_LOCATION "${_assimp_byproduct}"
    INTERFACE_INCLUDE_DIRECTORIES "${SOURCE_DIR}/include;${BINARY_DIR}/include"
    INTERFACE_LINK_LIBRARIES "zlib::zlibstatic"
)

set_target_properties(zlib::zlibstatic PROPERTIES
    IMPORTED_LOCATION "${_assimp_zlib_byproduct}"
)

add_executable(${TargetName})

add_dependencies(${TargetName} assimp_ext)

# Avoid MSVC dependency scanner glitches in environments where standard include paths are not
# available to the scanner (build is still driven by our scripts via VsDevCmd).
set_property(TARGET ${TargetName} PROPERTY CXX_SCAN_FOR_MODULES OFF)

set(Target_Private_Sources
    Private/AssetToolMain.cpp
    Private/AssetToolIO.cpp
    Private/Importers/Audio/AudioImporter.cpp
    Private/Importers/EnvMap/EnvMapImporter.cpp
    ${CMAKE_SOURCE_DIR}/ThirdParty/tinyexr/miniz.cpp
    Private/Importers/Material/MaterialImporter.cpp
    Private/Importers/Mesh/MeshImporter.cpp
    Private/Importers/Model/GltfImporter.cpp
    Private/Importers/Model/AssimpModelImporter.cpp
    Private/Importers/Model/FbxImporter.cpp
    Private/Importers/Model/ModelImporter.cpp
    Private/Importers/Model/PmxImporter.cpp
    Private/Importers/Model/UsdzImporter.cpp
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
        ${CMAKE_SOURCE_DIR}/ThirdParty/tinyexr
)

target_link_libraries(${TargetName}
    PRIVATE
        AltinaEngine::Core
        AltinaEngine::Asset
        AltinaEngine::Imaging
        assimp::assimp
)

if (MSVC)
    set_property(TARGET ${TargetName} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

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

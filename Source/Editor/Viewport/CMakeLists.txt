set(TargetName AltinaEditorViewport)

add_library(${TargetName} SHARED)
add_library(AltinaEditor::Viewport ALIAS ${TargetName})

file(GLOB_RECURSE Target_Public_Headers
    CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h
)

file(GLOB_RECURSE Target_Private_Sources
    CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp
)

target_sources(${TargetName}
    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS Public
            FILES ${Target_Public_Headers}
    PRIVATE
        ${Target_Private_Sources}
)

target_include_directories(${TargetName}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Public>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

target_link_libraries(${TargetName}
    PUBLIC
        AltinaEngine::Core
        AltinaEngine::Launch
        AltinaEngine::Engine
)

target_compile_features(${TargetName}
    PUBLIC
        cxx_std_23
)

target_compile_definitions(${TargetName}
    PRIVATE
        AE_EDITOR_VIEWPORT_BUILD
)

set_target_properties(${TargetName}
    PROPERTIES
        OUTPUT_NAME AltinaEditor.Viewport
        FOLDER "Editor/Viewport"
)

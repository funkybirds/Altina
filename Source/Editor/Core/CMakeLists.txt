set(TargetName AltinaEditorCore)

add_library(${TargetName} SHARED)
add_library(AltinaEditor::Core ALIAS ${TargetName})

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
)

target_compile_features(${TargetName}
    PUBLIC
        cxx_std_23
)

target_compile_definitions(${TargetName}
    PRIVATE
        AE_EDITOR_CORE_BUILD
)

set_target_properties(${TargetName}
    PROPERTIES
        OUTPUT_NAME AltinaEditor.Core
        FOLDER "Editor/Core"
)

set(AE_TEST_BIN_DIR ${CMAKE_BINARY_DIR}/Source/Tests)
add_custom_command(
    TARGET ${TargetName}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${AE_TEST_BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TargetName}>
        ${AE_TEST_BIN_DIR}
    COMMENT "Copying ${TargetName} to test binaries"
)

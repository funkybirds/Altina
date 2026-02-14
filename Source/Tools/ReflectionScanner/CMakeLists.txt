set(TargetName AltinaEngineReflectionScanner)

add_executable(${TargetName})

set(Target_Private_Sources
    Private/ReflectionScannerMain.cpp
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
    COMMENT "Staging ReflectionScanner dependencies"
)

set_target_properties(${TargetName}
    PROPERTIES
        OUTPUT_NAME ReflectionScanner
        FOLDER "Tools/ReflectionScanner"
)

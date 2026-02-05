set(TargetName AltinaEngineDemoMinimal)

add_executable(${TargetName})

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
)

target_compile_features(${TargetName}
    PRIVATE
        cxx_std_23
)

set(DemoBinaryDir ${CMAKE_SOURCE_DIR}/Demo/Minimal/Binaries)

set_target_properties(${TargetName}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${DemoBinaryDir}
        FOLDER "Demo/Minimal"
)

add_custom_command(
    TARGET ${TargetName}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TargetName}>
        ${DemoBinaryDir}
    COMMENT "Staging Minimal demo executable"
)

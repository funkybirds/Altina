set(TargetName AltinaEngineProjectGenerator)

add_executable(${TargetName})

set(Target_Private_Sources
    Private/ProjectGeneratorMain.cpp
)

target_sources(${TargetName}
    PRIVATE
        ${Target_Private_Sources}
)

target_link_libraries(${TargetName}
    PRIVATE
        AltinaEngine::Core
)

target_compile_features(${TargetName}
    PRIVATE
        cxx_std_23
)

set_target_properties(${TargetName}
    PROPERTIES
        FOLDER "Tools/ProjectGenerator"
)


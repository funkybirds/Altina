set(TargetName AltinaEngineDemoLauncher)

if(NOT WIN32)
    message(STATUS "AltinaEngineDemoLauncher is Windows-only; skipping target.")
    return()
endif()

add_executable(${TargetName})

set(Target_Private_Sources
    Private/DemoLauncherMain.cpp
)

target_sources(${TargetName}
    PRIVATE
        ${Target_Private_Sources}
)

target_compile_features(${TargetName}
    PRIVATE
        cxx_std_23
)

set_target_properties(${TargetName}
    PROPERTIES
        OUTPUT_NAME AltinaEngineDemoLauncher
        FOLDER "Tools/DemoLauncher"
)


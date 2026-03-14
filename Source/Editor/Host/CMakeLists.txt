set(TargetName AltinaEditor)

add_executable(${TargetName})

set(Target_Private_Sources
    Private/EditorMain.cpp
)

target_sources(${TargetName}
    PRIVATE
        ${Target_Private_Sources}
)

target_include_directories(${TargetName}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

target_link_libraries(${TargetName}
    PRIVATE
        AltinaEngine::Launch
        AltinaEngine::DebugGui
        AltinaEditor::Core
        AltinaEditor::UI
        AltinaEditor::Viewport
        AltinaEditor::PlaySession
)

target_compile_features(${TargetName}
    PRIVATE
        cxx_std_23
)

set(EditorBinaryDir ${CMAKE_SOURCE_DIR}/Source/Editor/Binaries)

set_target_properties(${TargetName}
    PROPERTIES
        OUTPUT_NAME AltinaEditor
        RUNTIME_OUTPUT_DIRECTORY ${EditorBinaryDir}
        FOLDER "Editor/Host"
)

add_custom_command(
    TARGET ${TargetName}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EditorBinaryDir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TargetName}>
        ${EditorBinaryDir}
    COMMENT "Staging AltinaEditor executable"
)

if(WIN32)
    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EditorBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${TargetName}>
            ${EditorBinaryDir}
        COMMAND_EXPAND_LISTS
        COMMENT "Staging AltinaEditor runtime DLLs"
    )

    add_custom_target(${TargetName}SyncRuntimeDlls ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EditorBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${TargetName}>
            ${EditorBinaryDir}
        COMMAND_EXPAND_LISTS
        DEPENDS ${TargetName}
        COMMENT "Syncing AltinaEditor runtime DLLs"
    )
    set_target_properties(${TargetName}SyncRuntimeDlls
        PROPERTIES
            FOLDER "Editor/Host"
    )
endif()

if(TARGET AltinaEngineManagedBuild)
    add_dependencies(${TargetName} AltinaEngineManagedBuild)
    if(TARGET AltinaEngineManagedPublishSelfContained)
        add_dependencies(${TargetName} AltinaEngineManagedPublishSelfContained)
    endif()

    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EditorBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.dll
            ${EditorBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.host.runtimeconfig.json
            ${EditorBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.runtimeconfig.json
            ${EditorBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.deps.json
            ${EditorBinaryDir}
        COMMENT "Staging AltinaEditor managed runtime files"
    )

    if(TARGET AltinaEngineManagedPublishSelfContained)
        add_custom_command(
            TARGET ${TargetName}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${AE_MANAGED_PUBLISH_SELFCONTAINED_DIR}
                ${EditorBinaryDir}
            COMMENT "Staging AltinaEditor self-contained .NET runtime"
            VERBATIM
        )
    endif()
endif()

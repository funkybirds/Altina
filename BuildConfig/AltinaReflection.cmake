include(CMakeParseArguments)

option(AE_ENABLE_REFLECTION "Enable reflection code generation." ON)
option(AE_REFLECTION_FAST_PATH "Skip reflection scanner when no annotations are found." ON)

function(ae_add_reflection_codegen)
    set(options FORBID_ANNOTATIONS)
    set(oneValueArgs TARGET MODULE_NAME MODULE_ROOT)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(AE_REFL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT AE_REFL_TARGET)
        message(FATAL_ERROR "ae_add_reflection_codegen requires TARGET.")
    endif()

    if (NOT TARGET ${AE_REFL_TARGET})
        message(FATAL_ERROR "ae_add_reflection_codegen target not found: ${AE_REFL_TARGET}.")
    endif()

    if (NOT AE_ENABLE_REFLECTION)
        return()
    endif()

    if (NOT WIN32)
        message(STATUS "Reflection codegen disabled for ${AE_REFL_TARGET}: Windows-only toolchain.")
        return()
    endif()

    if (NOT TARGET AltinaEngineReflectionScanner)
        message(FATAL_ERROR
            "AltinaEngineReflectionScanner target is missing. "
            "Ensure Source/Tools/ReflectionScanner is added before enabling reflection codegen.")
    endif()

    if (NOT AE_REFL_MODULE_NAME)
        set(AE_REFL_MODULE_NAME "${AE_REFL_TARGET}")
    endif()

    if (NOT AE_REFL_MODULE_ROOT)
        get_target_property(AE_REFL_MODULE_ROOT ${AE_REFL_TARGET} SOURCE_DIR)
    endif()

    if (AE_REFL_MODULE_ROOT STREQUAL "AE_REFL_MODULE_ROOT-NOTFOUND" OR AE_REFL_MODULE_ROOT STREQUAL "")
        message(FATAL_ERROR "Failed to resolve module root for ${AE_REFL_TARGET}.")
    endif()

    if (NOT AE_REFL_SOURCES)
        file(GLOB_RECURSE AE_REFL_SOURCES CONFIGURE_DEPENDS
            "${AE_REFL_MODULE_ROOT}/Private/*.c"
            "${AE_REFL_MODULE_ROOT}/Private/*.cc"
            "${AE_REFL_MODULE_ROOT}/Private/*.cpp"
            "${AE_REFL_MODULE_ROOT}/Private/*.cxx"
            "${AE_REFL_MODULE_ROOT}/Private/*.mm"
        )
    endif()

    set(abs_sources "")
    foreach(src IN LISTS AE_REFL_SOURCES)
        if (IS_ABSOLUTE "${src}")
            list(APPEND abs_sources "${src}")
        else()
            list(APPEND abs_sources "${AE_REFL_MODULE_ROOT}/${src}")
        endif()
    endforeach()

    if (abs_sources STREQUAL "")
        message(STATUS "Reflection codegen skipped for ${AE_REFL_MODULE_NAME}: no source files.")
        return()
    endif()

    file(GLOB_RECURSE refl_headers CONFIGURE_DEPENDS
        "${AE_REFL_MODULE_ROOT}/Public/*.h"
        "${AE_REFL_MODULE_ROOT}/Public/*.hpp"
        "${AE_REFL_MODULE_ROOT}/Public/*.hh"
        "${AE_REFL_MODULE_ROOT}/Public/*.hxx"
        "${AE_REFL_MODULE_ROOT}/Public/*.inl"
        "${AE_REFL_MODULE_ROOT}/Private/*.h"
        "${AE_REFL_MODULE_ROOT}/Private/*.hpp"
        "${AE_REFL_MODULE_ROOT}/Private/*.hh"
        "${AE_REFL_MODULE_ROOT}/Private/*.hxx"
        "${AE_REFL_MODULE_ROOT}/Private/*.inl"
    )

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${refl_headers})

    set(scan_args "")
    foreach(src IN LISTS abs_sources)
        list(APPEND scan_args --file "${src}")
    endforeach()

    set(modmap_files "")
    set(modmap_dirs "")
    file(RELATIVE_PATH module_rel "${CMAKE_SOURCE_DIR}" "${AE_REFL_MODULE_ROOT}")
    foreach(src IN LISTS abs_sources)
        file(RELATIVE_PATH src_rel "${AE_REFL_MODULE_ROOT}" "${src}")
        set(modmap "${CMAKE_BINARY_DIR}/${module_rel}/CMakeFiles/${AE_REFL_TARGET}.dir/${src_rel}.obj.modmap")
        list(APPEND modmap_files "${modmap}")
        get_filename_component(modmap_dir "${modmap}" DIRECTORY)
        list(APPEND modmap_dirs "${modmap_dir}")
    endforeach()
    list(REMOVE_DUPLICATES modmap_dirs)

    set(gen_dir "${CMAKE_BINARY_DIR}/Generated/${AE_REFL_MODULE_NAME}")
    set(gen_cpp "${gen_dir}/Reflection.gen.cpp")
    set(compile_commands_root "${CMAKE_BINARY_DIR}")

    set(has_annotations TRUE)
    if (AE_REFLECTION_FAST_PATH)
        set(has_annotations FALSE)
        foreach(header IN LISTS refl_headers)
            if (NOT EXISTS "${header}")
                continue()
            endif()
            file(READ "${header}" header_text)
            string(REGEX REPLACE "#[ \t]*define[ \t]+ACLASS[^\r\n]*" "" header_text "${header_text}")
            string(REGEX REPLACE "#[ \t]*define[ \t]+APROPERTY[^\r\n]*" "" header_text "${header_text}")
            string(REGEX REPLACE "#[ \t]*define[ \t]+AFUNCTION[^\r\n]*" "" header_text "${header_text}")
            if (header_text MATCHES "ACLASS[ \t\r\n]*\\("
                OR header_text MATCHES "APROPERTY[ \t\r\n]*\\("
                OR header_text MATCHES "AFUNCTION[ \t\r\n]*\\("
                OR header_text MATCHES "\\[\\[AltinaRefl::Class"
                OR header_text MATCHES "\\[\\[AltinaRefl::Property"
                OR header_text MATCHES "\\[\\[AltinaRefl::Function")
                set(has_annotations TRUE)
                break()
            endif()
        endforeach()
    endif()

    if (AE_REFLECTION_FAST_PATH AND has_annotations AND EXISTS "${gen_cpp}")
        file(READ "${gen_cpp}" existing_gen)
        if (existing_gen MATCHES "Auto-generated by CMake \\(no annotations\\)")
            file(REMOVE "${gen_cpp}")
        endif()
    endif()

    if (AE_REFLECTION_FAST_PATH AND NOT has_annotations)
        file(MAKE_DIRECTORY "${gen_dir}")
        if (AE_REFL_FORBID_ANNOTATIONS)
            set(stamp "${gen_dir}/Reflection.forbid.stamp")
            if (NOT EXISTS "${stamp}")
                file(WRITE "${stamp}" "")
            endif()
            set(check_target "${AE_REFL_TARGET}_ReflectionCheck")
            add_custom_target(${check_target} ALL DEPENDS "${stamp}")
            if (NOT AE_REFL_TARGET STREQUAL "AltinaEngineCore")
                add_dependencies(${AE_REFL_TARGET} ${check_target})
            endif()
        else()
            string(MAKE_C_IDENTIFIER "${AE_REFL_MODULE_NAME}" module_ident)
            set(stub_content
                "// Auto-generated by CMake (no annotations). Do not edit.\n"
                "// Module: ${AE_REFL_MODULE_NAME}\n"
                "#include \"Reflection/Reflection.h\"\n"
                "\n"
                "namespace AltinaEngine::Core::Reflection {\n"
                "void RegisterReflection_${module_ident}() {\n"
                "}\n"
                "} // namespace AltinaEngine::Core::Reflection\n"
            )
            list(JOIN stub_content "" stub_content_joined)
            file(GENERATE OUTPUT "${gen_cpp}" CONTENT "${stub_content_joined}")
            set_source_files_properties("${gen_cpp}" PROPERTIES GENERATED TRUE)
            target_sources(${AE_REFL_TARGET} PRIVATE "${gen_cpp}")
        endif()
        return()
    endif()

    if (AE_REFL_FORBID_ANNOTATIONS)
        set(stamp "${gen_dir}/Reflection.forbid.stamp")
        add_custom_command(
            OUTPUT "${stamp}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${gen_dir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory ${modmap_dirs}
            COMMAND ${CMAKE_COMMAND} -E touch ${modmap_files}
            COMMAND $<TARGET_FILE:AltinaEngineReflectionScanner>
                --compile-commands "${compile_commands_root}"
                --include-headers
                --module-name "${AE_REFL_MODULE_NAME}"
                --module-root "${AE_REFL_MODULE_ROOT}"
                --forbid-annotations
                --strict
                ${scan_args}
            COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
            DEPENDS
                ${abs_sources}
                ${refl_headers}
                "${compile_commands_root}/compile_commands.json"
                AltinaEngineReflectionScanner
            COMMENT "Checking reflection annotations for ${AE_REFL_MODULE_NAME}"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        set(check_target "${AE_REFL_TARGET}_ReflectionCheck")
        add_custom_target(${check_target} ALL DEPENDS "${stamp}")
        if (NOT AE_REFL_TARGET STREQUAL "AltinaEngineCore")
            add_dependencies(${AE_REFL_TARGET} ${check_target})
        endif()
        return()
    endif()

    set(gen_cpp "${gen_dir}/Reflection.gen.cpp")
    add_custom_command(
        OUTPUT "${gen_cpp}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${gen_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${modmap_dirs}
        COMMAND ${CMAKE_COMMAND} -E touch ${modmap_files}
        COMMAND $<TARGET_FILE:AltinaEngineReflectionScanner>
            --compile-commands "${compile_commands_root}"
            --include-headers
            --module-name "${AE_REFL_MODULE_NAME}"
            --module-root "${AE_REFL_MODULE_ROOT}"
            --gen-cpp "${gen_cpp}"
            --strict
            ${scan_args}
        DEPENDS
            ${abs_sources}
            ${refl_headers}
            "${compile_commands_root}/compile_commands.json"
            AltinaEngineReflectionScanner
        COMMENT "Generating reflection for ${AE_REFL_MODULE_NAME}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    set_source_files_properties("${gen_cpp}" PROPERTIES GENERATED TRUE)
    target_sources(${AE_REFL_TARGET} PRIVATE "${gen_cpp}")
    add_dependencies(${AE_REFL_TARGET} AltinaEngineReflectionScanner)
endfunction()

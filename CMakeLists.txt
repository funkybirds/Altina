cmake_minimum_required(VERSION 3.28)

project(AltinaEngine VERSION 0.1.0 LANGUAGES CXX)

# Require C++23 for features like "if consteval". Make the standard required
# and disable compiler extensions for consistent behaviour across toolchains.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Treat warnings as errors globally. For MSVC enable `/WX` and request a
# C++23 standard switch; for other compilers enable `-Werror`.
if(MSVC)
    add_compile_options(/WX)
    # Request MSVC to use the C++23 language standard explicitly.
    add_compile_options(/std:c++23)
else()
    add_compile_options(-Werror)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Please create a separate build directory.")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(Source/Engine/Core)
add_subdirectory(Demo/Minimal)
add_subdirectory(Test)

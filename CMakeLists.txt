cmake_minimum_required(VERSION 3.28)

project(AltinaEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Please create a separate build directory.")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(Source/Engine/Core)
add_subdirectory(Demo/Minimal)
add_subdirectory(Test)

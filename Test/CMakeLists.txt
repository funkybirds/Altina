add_executable(AltinaEngineTests main.cpp)
# Link against core so public headers are available and linkage is validated
target_link_libraries(AltinaEngineTests PRIVATE AltinaEngine::Core)
set_target_properties(AltinaEngineTests PROPERTIES FOLDER "Tests")

# Register with CTest
include(CTest)
enable_testing()
add_test(NAME AltinaEngineTests COMMAND AltinaEngineTests)

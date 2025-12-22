

file(GLOB_RECURSE ALTINA_TEST_SOURCES
	CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(AltinaEngineTests ${ALTINA_TEST_SOURCES})
target_link_libraries(AltinaEngineTests PRIVATE AltinaEngine::Core)
target_include_directories(AltinaEngineTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(AltinaEngineTests PROPERTIES FOLDER "Tests")

# Register with CTest (CTest is enabled at the top level).
add_test(NAME AltinaEngineTests COMMAND $<TARGET_FILE:AltinaEngineTests>)

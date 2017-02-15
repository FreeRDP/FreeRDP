
set(MODULE_NAME "TestCore")
set(MODULE_PREFIX "TEST_CORE")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestVersion.c
	TestSettings.c)

if(WITH_SAMPLE AND WITH_SERVER)
	set(${MODULE_PREFIX}_TESTS
		${${MODULE_PREFIX}_TESTS}
		TestConnect.c)
else()
	message("Skipping connection tests, requires WITH_SAMPLE and WITH_SERVER set!")
endif()

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

add_definitions(-DTESTING_OUTPUT_DIRECTORY="${CMAKE_BINARY_DIR}")
add_definitions(-DTESTING_SRC_DIRECTORY="${CMAKE_SOURCE_DIR}")

target_link_libraries(${MODULE_NAME} freerdp winpr freerdp-client)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Core/Test")


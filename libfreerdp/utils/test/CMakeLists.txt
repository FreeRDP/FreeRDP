
set(MODULE_NAME "TestFreeRDPUtils")
set(MODULE_PREFIX "TEST_FREERDP_UTILS")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestRingBuffer.c)

set(${MODULE_PREFIX}_FUZZERS
	TestFuzzReadPassphrase.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

target_link_libraries(${MODULE_NAME} freerdp)

if (BUILD_FUZZERS)
	foreach(test ${${MODULE_PREFIX}_FUZZERS})
		get_filename_component(TestName ${test} NAME_WE)
		add_executable(${TestName} ${test})
		add_compile_options(-fsanitize-coverage=trace-cmp -fsanitize=fuzzer,address)
		target_link_libraries(${TestName} freerdp winpr -fsanitize=fuzzer,address)
		add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
		set_target_properties(${TestName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")
	endforeach()
endif (BUILD_FUZZERS)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Test")


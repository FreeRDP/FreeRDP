
set(MODULE_NAME "TestWtsApi")
set(MODULE_PREFIX "TEST_WTSAPI")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestWtsApiEnumerateProcesses.c
	TestWtsApiEnumerateSessions.c
	TestWtsApiQuerySessionInformation.c
	TestWtsApiShutdownSystem.c
	TestWtsApiSessionNotification.c
	TestWtsApiWaitSystemEvent.c
	)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})


target_link_libraries(${MODULE_NAME} winpr)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")

if(TESTS_WTSAPI_EXTRA)

set(MODULE_NAME "TestWtsApiExtra")
set(MODULE_PREFIX "TEST_WTSAPI_EXTRA")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
		TestWtsApiExtraDisconnectSession.c
		TestWtsApiExtraDynamicVirtualChannel.c
		TestWtsApiExtraLogoffSession.c
		TestWtsApiExtraSendMessage.c
		TestWtsApiExtraVirtualChannel.c
		)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

target_link_libraries(${MODULE_NAME} winpr)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
	set_tests_properties(${TestName} PROPERTIES LABELS "WTSAPI_EXTRA")
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")
endif()

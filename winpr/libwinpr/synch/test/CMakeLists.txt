
set(MODULE_NAME "TestSynch")
set(MODULE_PREFIX "TEST_SYNCH")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestSynchInit.c
	TestSynchEvent.c
	TestSynchMutex.c
	TestSynchBarrier.c
	TestSynchCritical.c
	TestSynchSemaphore.c
	TestSynchThread.c
	TestSynchMultipleThreads.c
	TestSynchTimerQueue.c
	TestSynchWaitableTimer.c
	TestSynchWaitableTimerAPC.c
	TestSynchAPC.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

if(FREEBSD)
	include_directories(${EPOLLSHIM_INCLUDE_DIR})
endif()

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

target_link_libraries(${MODULE_NAME} winpr)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")


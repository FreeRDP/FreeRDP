
set(MODULE_NAME "TestWinPRUtils")
set(MODULE_PREFIX "TEST_WINPR_UTILS")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestIni.c
	TestImage.c
	TestBacktrace.c
	TestQueue.c
	TestPrint.c
	TestPubSub.c
	TestStream.c
	TestBitStream.c
	TestArrayList.c
	TestLinkedList.c
	TestListDictionary.c
	TestCmdLine.c
	TestWLog.c
	TestWLogCallback.c
	TestHashTable.c
	TestBufferPool.c
	TestStreamPool.c
	TestMessageQueue.c
	TestMessagePipe.c)

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


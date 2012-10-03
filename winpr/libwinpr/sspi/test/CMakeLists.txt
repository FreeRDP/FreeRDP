
set(MODULE_NAME "TestSspi")
set(MODULE_PREFIX "TEST_SSPI")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestQuerySecurityPackageInfo.c
	TestEnumerateSecurityPackages.c
	TestInitializeSecurityContext.c
	TestAcquireCredentialsHandle.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

if(WIN32)
	target_link_libraries(${MODULE_NAME} secur32)
endif()

target_link_libraries(${MODULE_NAME} winpr-sspi)

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${EXECUTABLE_OUTPUT_PATH}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")

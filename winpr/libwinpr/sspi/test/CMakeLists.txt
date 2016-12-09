
set(MODULE_NAME "TestSspi")
set(MODULE_PREFIX "TEST_SSPI")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
	TestQuerySecurityPackageInfo.c
	TestEnumerateSecurityPackages.c
	TestInitializeSecurityContext.c
	TestAcquireCredentialsHandle.c
	TestCredSSP.c
	#TestSchannel.c
	TestNTLM.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_DRIVER}
	${${MODULE_PREFIX}_TESTS})

include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

if(WIN32)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} secur32 crypt32)
endif()

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS} winpr)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
	get_filename_component(TestName ${test} NAME_WE)
	add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")


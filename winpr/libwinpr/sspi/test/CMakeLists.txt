set(MODULE_NAME "TestSspi")
set(MODULE_PREFIX "TEST_SSPI")

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(${MODULE_PREFIX}_TESTS
    TestQuerySecurityPackageInfo.c
    TestEnumerateSecurityPackages.c
    TestInitializeSecurityContext.c
    TestAcquireCredentialsHandle.c
    TestCredSSP.c
    TestSspiTSSSP.c
    TestCredSSPTSRequest.c
    TestSspiCredSSP.c
    #TestSchannel.c
    TestSspiNTLM.c
    TestSspiPasswordHash.c
    TestCredentialZeroing.c
)

# Includes Kerberos/kerberos.c directly to reach the static SSPI entry points.
if(BUILD_TESTING_INTERNAL AND WITH_KRB5 AND KRB5_FLAVOUR STREQUAL "MIT")
  list(APPEND ${MODULE_PREFIX}_TESTS TestKerberosWrap.c)
  set(TEST_SSPI_KERBEROS_INTERNAL ON)
endif()

create_test_sourcelist(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_DRIVER} ${${MODULE_PREFIX}_TESTS})

set(FUZZERS TestFuzzNTLMMessage.c)

include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

if(WIN32)
  set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} secur32 crypt32)
endif()

target_link_libraries(${MODULE_NAME} PRIVATE ${${MODULE_PREFIX}_LIBS} winpr)

if(TEST_SSPI_KERBEROS_INTERNAL)
  target_compile_definitions(${MODULE_NAME} PRIVATE WITH_KRB5 WITH_KRB5_MIT)
  target_include_directories(${MODULE_NAME} SYSTEM PRIVATE ${KRB5_INCLUDEDIR} ${KRB5_INCLUDE_DIRS})
  target_compile_options(${MODULE_NAME} PRIVATE ${KRB5_CFLAGS})
  target_link_directories(${MODULE_NAME} PRIVATE ${KRB5_LIBRARY_DIRS})
  target_link_libraries(${MODULE_NAME} PRIVATE ${KRB5_LIBRARIES} ${KRB5_LIBRARY})
endif()

include(AddFuzzerTest)
add_fuzzer_test("${FUZZERS}" "winpr")

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(NAME ${TestName} COMMAND ${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")

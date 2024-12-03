set(MODULE_NAME "TestFreeRDPCrypto")
set(MODULE_PREFIX "TEST_FREERDP_CRYPTO")

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(DRIVER ${MODULE_NAME}.c)

set(TESTS TestKnownHosts.c TestBase64.c)

if(BUILD_TESTING_INTERNAL)
  list(APPEND TESTS Test_x509_utils.c)
endif()

create_test_sourcelist(SRCS ${DRIVER} ${TESTS})

include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

add_executable(${MODULE_NAME} ${SRCS})

set(TEST_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_compile_definitions(TEST_SOURCE_DIR="${TEST_PATH}")
target_link_libraries(${MODULE_NAME} freerdp winpr ${OPENSSL_LIBRARIES})

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Test")

set(MODULE_NAME "TestFreeRDPProxy")
set(MODULE_PREFIX "TEST_FREERDP_PROXY")

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(DRIVER ${MODULE_NAME}.c)

set(TESTS TestFreeRDPProxyConfig.c)

if(BUILD_TESTING_INTERNAL)

endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

create_test_sourcelist(SRCS ${DRIVER} ${TESTS})

add_compile_definitions(CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_compile_definitions(CMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
add_executable(${MODULE_NAME} ${SRCS} ${CURSOR_TESTCASES_H} ${CURSOR_TESTCASES_C} ${TESTCASE_HEADER} ${TEST_COMMON})

target_link_libraries(${MODULE_NAME} PRIVATE freerdp-server-proxy)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Test")

#set(FUZZERS TestFuzzCodecs.c TestFuzzNSCodec.c)
#
#include(AddFuzzerTest)
#add_fuzzer_test("${FUZZERS}" "freerdp winpr")

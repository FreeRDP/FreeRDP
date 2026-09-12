set(MODULE_NAME "TestClient")
set(MODULE_PREFIX "TEST_CLIENT")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(${MODULE_PREFIX}_TESTS TestClientRdpFile.c TestClientChannels.c TestClientCmdLine.c TestClientCmdLineStatus.c)

if(WITH_FUSE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND ${MODULE_PREFIX}_TESTS TestClientClipboardTransfer.c)
endif()

set(FUZZERS TestFuzzClientRdpFile.c)

create_test_sourcelist(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_DRIVER} ${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
target_compile_definitions(${MODULE_NAME} PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-client)

target_link_libraries(${MODULE_NAME} PRIVATE ${${MODULE_PREFIX}_LIBS})

include(AddFuzzerTest)
add_fuzzer_test("${FUZZERS}" "freerdp-client freerdp winpr")

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Client/Test")

if(WITH_FUSE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_tests_properties(TestClientClipboardTransfer PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
endif()

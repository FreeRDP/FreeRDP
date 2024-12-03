set(MODULE_NAME "TestCore")
set(MODULE_PREFIX "TEST_CORE")

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(DRIVER ${MODULE_NAME}.c)

set(TESTS TestVersion.c TestSettings.c)

if(BUILD_TESTING_INTERNAL)
  list(APPEND TESTS TestStreamDump.c)
endif()

set(FUZZERS TestFuzzCoreClient.c TestFuzzCoreServer.c TestFuzzCryptoCertificateDataSetPEM.c)

# Test not compatible with package tests, disable
if(WITH_SAMPLE AND WITH_SERVER AND NOT WIN32 AND BUILD_TESTING_INTERNAL)
  add_compile_definitions(CMAKE_EXECUTABLE_SUFFIX="${CMAKE_EXECUTABLE_SUFFIX}")
  set(TESTS ${TESTS} TestConnect.c)
else()
  message("Skipping connection tests, requires BUILD_TESTING_INTERNAL, WITH_SAMPLE and WITH_SERVER set!")
endif()

create_test_sourcelist(SRCS ${DRIVER} ${TESTS})

add_executable(${MODULE_NAME} ${SRCS})

add_compile_definitions(TESTING_OUTPUT_DIRECTORY="${PROJECT_BINARY_DIR}")
add_compile_definitions(TESTING_SRC_DIRECTORY="${PROJECT_SOURCE_DIR}")

target_link_libraries(${MODULE_NAME} freerdp winpr freerdp-client)

include(AddFuzzerTest)
add_fuzzer_test("${FUZZERS}" "freerdp-client freerdp winpr")

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Core/Test")

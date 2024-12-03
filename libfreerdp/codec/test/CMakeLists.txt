set(MODULE_NAME "TestFreeRDPCodec")
set(MODULE_PREFIX "TEST_FREERDP_CODEC")

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(DRIVER ${MODULE_NAME}.c)

set(TESTS
    TestFreeRDPRegion.c
    TestFreeRDPCodecZGfx.c
    TestFreeRDPCodecPlanar.c
    TestFreeRDPCodecCopy.c
    TestFreeRDPCodecClear.c
    TestFreeRDPCodecInterleaved.c
    TestFreeRDPCodecProgressive.c
    TestFreeRDPCodecRemoteFX.c
)

if(BUILD_TESTING_INTERNAL)
  list(APPEND TESTS TestFreeRDPCodecMppc.c TestFreeRDPCodecNCrush.c TestFreeRDPCodecXCrush.c)
endif()

create_test_sourcelist(SRCS ${DRIVER} ${TESTS})

add_compile_definitions(CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_compile_definitions(CMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
add_executable(${MODULE_NAME} ${SRCS})

target_link_libraries(${MODULE_NAME} freerdp winpr)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Test")

set(FUZZERS TestFuzzCodecs.c)

include(AddFuzzerTest)
add_fuzzer_test("${FUZZERS}" "freerdp winpr")

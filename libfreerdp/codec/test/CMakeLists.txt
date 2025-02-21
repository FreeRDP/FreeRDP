set(MODULE_NAME "TestFreeRDPCodec")
set(MODULE_PREFIX "TEST_FREERDP_CODEC")

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(DRIVER ${MODULE_NAME}.c)

set(TESTS
    TestFreeRDPRegion.c
    TestFreeRDPCodecZGfx.c
    TestFreeRDPCodecPlanar.c
    TestFreeRDPCodecCopy.c
    TestFreeRDPCodecCursor.c
    TestFreeRDPCodecClear.c
    TestFreeRDPCodecInterleaved.c
    TestFreeRDPCodecProgressive.c
    TestFreeRDPCodecRemoteFX.c
)

if(NOT BUILD_TESTING_NO_H264)
  list(APPEND TESTS TestFreeRDPCodecH264.c)
endif()

if(BUILD_TESTING_INTERNAL)
  list(APPEND TESTS TestFreeRDPCodecMppc.c TestFreeRDPCodecNCrush.c TestFreeRDPCodecXCrush.c)
endif()

file(GLOB CURSOR_TESTCASES_C LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "cursor/*.c")
file(GLOB CURSOR_TESTCASES_H LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "cursor/*.h")

# Create a combined header for all testcases
set(TESTCASE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/testcases.h")
write_file(${TESTCASE_HEADER} "#pragma once\n")
foreach(header ${CURSOR_TESTCASES_H})
  write_file(${TESTCASE_HEADER} "#include <${header}>" APPEND)
endforeach()

write_file(${TESTCASE_HEADER} "\nstatic const gdiPalette* testcase_palette[] = {" APPEND)
foreach(header ${CURSOR_TESTCASES_H})
  get_filename_component(NAME ${header} NAME_WE)
  write_file(${TESTCASE_HEADER} "&${NAME}_palette," APPEND)
endforeach()
write_file(${TESTCASE_HEADER} "};\n" APPEND)

write_file(${TESTCASE_HEADER} "static const rdpPointer* testcase_pointer[] = {" APPEND)
foreach(header ${CURSOR_TESTCASES_H})
  get_filename_component(NAME ${header} NAME_WE)
  write_file(${TESTCASE_HEADER} "&${NAME}_pointer," APPEND)
endforeach()
write_file(${TESTCASE_HEADER} "};\n" APPEND)

write_file(${TESTCASE_HEADER} "static const uint8_t* testcase_image_bgra32[] = {" APPEND)
foreach(header ${CURSOR_TESTCASES_H})
  get_filename_component(NAME ${header} NAME_WE)
  write_file(${TESTCASE_HEADER} "${NAME}_image_bgra32," APPEND)
endforeach()
write_file(${TESTCASE_HEADER} "};" APPEND)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

create_test_sourcelist(SRCS ${DRIVER} ${TESTS})

add_compile_definitions(CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_compile_definitions(CMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
add_executable(${MODULE_NAME} ${SRCS} ${CURSOR_TESTCASES_H} ${CURSOR_TESTCASES_C} ${TESTCASE_HEADER})

target_link_libraries(${MODULE_NAME} freerdp winpr)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Test")

add_executable(img2bgra img2bgra.c)
target_link_libraries(img2bgra winpr)

set(FUZZERS TestFuzzCodecs.c)

include(AddFuzzerTest)
add_fuzzer_test("${FUZZERS}" "freerdp winpr")

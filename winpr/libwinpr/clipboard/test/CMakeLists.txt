set(MODULE_NAME "TestClipboard")
set(MODULE_PREFIX "TEST_CLIPBOARD")

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(DRIVER ${MODULE_NAME}.c)

set(TESTS TestClipboardFormats.c)

if(BUILD_TESTING_INTERNAL)
  list(APPEND TESTS TestUri.c)
endif()

set(TEST_CLIP_PNG "${CMAKE_SOURCE_DIR}/resources/FreeRDP_Icon.png")
file(TO_NATIVE_PATH "${TEST_CLIP_PNG}" TEST_CLIP_PNG)

set(TEST_CLIP_BMP "${CMAKE_SOURCE_DIR}/resources/FreeRDP_Install.bmp")
file(TO_NATIVE_PATH "${TEST_CLIP_BMP}" TEST_CLIP_BMP)

if(WIN32)
  string(REPLACE "\\" "\\\\" TEST_CLIP_PNG "${TEST_CLIP_PNG}")
  string(REPLACE "\\" "\\\\" TEST_CLIP_BMP "${TEST_CLIP_BMP}")
endif()

add_compile_definitions(TEST_CLIP_BMP="${TEST_CLIP_BMP}")
add_compile_definitions(TEST_CLIP_PNG="${TEST_CLIP_PNG}")

create_test_sourcelist(SRCS ${DRIVER} ${TESTS})

add_executable(${MODULE_NAME} ${SRCS})
target_link_libraries(${MODULE_NAME} winpr)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")

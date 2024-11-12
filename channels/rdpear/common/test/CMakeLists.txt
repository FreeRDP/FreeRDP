set(MODULE_NAME "TestRdpear")
set(MODULE_PREFIX "TEST_RDPEAR")

set(TEST_RDPEAR_DRIVER TestRdpear.c)

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(TEST_RDPEAR_TESTS TestNdr.c)

if(BUILD_TESTING_INTERNAL)
  list(APPEND TEST_RDPEAR_TESTS TestNdrEar.c)
endif()

create_test_sourcelist(TEST_RDPEAR_SRCS TestRdpear.c ${TEST_RDPEAR_TESTS})

add_executable(${MODULE_NAME} ${TEST_RDPEAR_SRCS})

add_compile_definitions(TESTING_OUTPUT_DIRECTORY="${PROJECT_BINARY_DIR}")
add_compile_definitions(TESTING_SRC_DIRECTORY="${PROJECT_SOURCE_DIR}")

target_link_libraries(${MODULE_NAME} freerdp winpr freerdp-client)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Rdpear/Test")

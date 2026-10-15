set(MODULE_NAME "TestRdpewa")
set(MODULE_PREFIX "TEST_RDPEWA")

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(DRIVER ${MODULE_NAME}.c)
set(TESTS TestRdpewaMultipleConnections.c)

create_test_sourcelist(SRCS ${DRIVER} ${TESTS})
add_executable(${MODULE_NAME} ${SRCS})
target_link_libraries(${MODULE_NAME} PRIVATE winpr freerdp freerdp-client ${LIBCBOR_LIBRARIES}
                                             ${LIBFIDO2_LIBRARIES})
target_include_directories(${MODULE_NAME} PRIVATE ..)
set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Channel/Test")

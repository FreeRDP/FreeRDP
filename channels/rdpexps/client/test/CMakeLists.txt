set(MODULE_NAME "TestRdpeXps")
set(TEST_RDPEXPS_TESTS TestRdpeXps.c)

create_test_sourcelist(TEST_RDPEXPS_SRCS TestRdpeXpsDriver.c ${TEST_RDPEXPS_TESTS})
add_executable(${MODULE_NAME} ${TEST_RDPEXPS_SRCS} ../rdpexps_common.c)
target_link_libraries(${MODULE_NAME} PRIVATE winpr)
target_include_directories(${MODULE_NAME} PRIVATE ..)
set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

add_test(NAME ${MODULE_NAME} COMMAND ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${MODULE_NAME})

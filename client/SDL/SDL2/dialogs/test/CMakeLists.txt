set(MODULE_NAME "TestSDL")
set(MODULE_PREFIX "TEST_SDL")

set(DRIVER ${MODULE_NAME}.cpp)

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(TEST_SRCS TestSDLDialogs.cpp)

create_test_sourcelist(SRCS ${DRIVER} ${TEST_SRCS})

add_library(${MODULE_NAME} ${SRCS})

list(APPEND LIBS dialogs)

target_link_libraries(${MODULE_NAME} ${LIBS})

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Client/Test")

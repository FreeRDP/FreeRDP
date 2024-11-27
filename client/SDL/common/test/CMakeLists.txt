set(MODULE_NAME "TestSDL2")
set(MODULE_PREFIX "TEST_SDL")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.cpp)

set(${MODULE_PREFIX}_TESTS TestSDLPrefs.cpp)

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

create_test_sourcelist(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_DRIVER} ${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set(${MODULE_PREFIX}_LIBS freerdp winpr sdl-common-prefs)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

set(TEST_SRC_AREA "${CMAKE_CURRENT_SOURCE_DIR}")
set(TEST_BIN_AREA "${CMAKE_CURRENT_BINARY_DIR}")

if(WIN32)
  string(REPLACE "\\" "\\\\" TEST_SRC_AREA "${TEST_SRC_AREA}")
  string(REPLACE "\\" "\\\\" TEST_BIN_AREA "${TEST_BIN_AREA}")
endif()

add_compile_definitions(TEST_SRC_AREA="${TEST_SRC_AREA}")
add_compile_definitions(TEST_BIN_AREA="${TEST_BIN_AREA}")

foreach(test ${${MODULE_PREFIX}_TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Client/Test")

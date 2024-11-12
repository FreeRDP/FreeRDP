# Test not compatible with package tests, disable
if(BUILD_TESTING_INTERNAL)
  set(MODULE_NAME "TestLocale")
  set(MODULE_PREFIX "TEST_LOCALE")

  disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

  set(DRIVER ${MODULE_NAME}.c)

  set(TEST_SRCS TestLocaleKeyboard.c)

  create_test_sourcelist(SRCS ${DRIVER} ${TEST_SRCS})

  add_executable(${MODULE_NAME} ${SRCS})

  add_compile_definitions(TESTING_OUTPUT_DIRECTORY="${PROJECT_BINARY_DIR}")
  add_compile_definitions(TESTING_SRC_DIRECTORY="${PROJECT_SOURCE_DIR}")

  target_link_libraries(${MODULE_NAME} freerdp winpr freerdp-client)

  set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

  foreach(test ${TEST_SRCS})
    get_filename_component(TestName ${test} NAME_WE)
    add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
  endforeach()

  set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Locale/Test")
endif()

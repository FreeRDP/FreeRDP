set(MODULE_NAME "TestTimeZone")
set(MODULE_PREFIX "TEST_TIMEZONE")

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.c)

set(TESTS TestTimeZoneBrowser.c)

if(WITH_TIMEZONE_COMPILED)
  list(APPEND TESTS TestTimeZoneIanaAlias.c TestTimeZoneNestedName.c)
endif()

create_test_sourcelist(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_DRIVER} ${TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

target_link_libraries(${MODULE_NAME} PRIVATE winpr)

if(EMSCRIPTEN)
  set(_js TestTimeZoneBrowser.js)
  target_link_options(${MODULE_NAME} PRIVATE "--js-library=${CMAKE_CURRENT_SOURCE_DIR}/${_js}")
  set_source_files_properties(TestTimeZoneBrowser.c PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_js})
endif()

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(NAME ${TestName} COMMAND ${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Test")

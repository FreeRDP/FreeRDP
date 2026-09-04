if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(MODULE_NAME "TestXfRailIpc")

  add_executable(${MODULE_NAME} TestXfRailIpc.c)
  target_include_directories(${MODULE_NAME} PRIVATE .. ../../../channels/rail/client)
  target_link_libraries(${MODULE_NAME} PRIVATE freerdp winpr freerdp-client xfreerdp-client)
  set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

  add_test(NAME ${MODULE_NAME} COMMAND ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME})
  set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Client/X11/Test")

  target_compile_options(${MODULE_NAME} PRIVATE "-fvisibility=default")
  target_link_options(
    ${MODULE_NAME} PRIVATE "-Wl,--wrap=XGetWindowAttributes" "-Wl,--wrap=XSelectInput" "-Wl,--wrap=XGetWindowProperty"
  )

  set(MODULE_NAME "TestXfClientRailIpc")

  add_executable(${MODULE_NAME} TestXfClientRailIpc.c)
  target_include_directories(${MODULE_NAME} PRIVATE ..)
  target_link_libraries(${MODULE_NAME} PRIVATE freerdp winpr freerdp-client xfreerdp-client)
  set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")
  target_link_options(
    ${MODULE_NAME}
    PRIVATE
    "-Wl,--wrap=freerdp_connect"
    "-Wl,--wrap=freerdp_disconnect"
    "-Wl,--wrap=freerdp_get_event_handles"
    "-Wl,--wrap=freerdp_check_event_handles"
    "-Wl,--wrap=XPending"
  )

  add_test(NAME ${MODULE_NAME} COMMAND ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME})
  set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Client/X11/Test")
endif()

set(MODULE_NAME "TestXfCmdLine")

add_executable(${MODULE_NAME} TestXfCmdLine.c)
target_include_directories(${MODULE_NAME} PRIVATE ..)
target_link_libraries(${MODULE_NAME} PRIVATE freerdp winpr freerdp-client xfreerdp-client)
set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

add_test(NAME ${MODULE_NAME} COMMAND ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME})
set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Client/X11/Test")

if(CMAKE_CROSSCOMPILING)
  find_package(GenerateArgumentManpage)
else()
  include_directories(${CMAKE_BINARY_DIR}/include/)

  add_executable(generate_argument_manpage generate_argument_manpage.c ../cmdline.h)
  export(TARGETS generate_argument_manpage FILE "${CMAKE_BINARY_DIR}/GenerateArgumentManpageConfig.cmake")
endif()

set(MAN_OPTIONS_FILE "${CMAKE_CURRENT_BINARY_DIR}/freerdp-global-options.1")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MAN_OPTIONS_FILE})

add_custom_command(
  OUTPUT ${MAN_OPTIONS_FILE} COMMAND ${CMAKE_BINARY_DIR}/client/common/man/generate_argument_manpage
                                     "${MAN_OPTIONS_FILE}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS generate_argument_manpage
)
add_custom_target(
  generate_argument_manpage.target DEPENDS ${MAN_OPTIONS_FILE} ${CMAKE_BINARY_DIR}/include/freerdp/config.h
                                           generate_argument_manpage WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

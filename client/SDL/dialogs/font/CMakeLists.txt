
add_executable(
  convert_font_to_c
  convert_font_to_c.cpp
)

add_library(font OBJECT
  opensans_variable_font.hpp
  opensans_variable_font.cpp
  font_writer.hpp
  font_writer.cpp
)
set_property(TARGET font PROPERTY POSITION_INDEPENDENT_CODE ON)

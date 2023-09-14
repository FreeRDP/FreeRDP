

find_package(SDL3_ttf REQUIRED)
get_target_property(SDL3_TTF_INCLUDE_DIRS SDL3_ttf::SDL3_ttf INTERFACE_INCLUDE_DIRECTORIES)
include_directories(${SDL3_TTF_INCLUDE_DIRS})

set(SRCS
    sdl_button.hpp
    sdl_button.cpp
    sdl_buttons.hpp
    sdl_buttons.cpp
    sdl_dialogs.cpp
    sdl_dialogs.hpp
    sdl_widget.hpp
    sdl_widget.cpp
    sdl_input.hpp
    sdl_input.cpp
    sdl_input_widgets.cpp
    sdl_input_widgets.hpp
    sdl_select.hpp
    sdl_select.cpp
    sdl_selectlist.hpp
    sdl_selectlist.cpp
)

set(LIBS
	SDL3::SDL3
	SDL3_ttf::SDL3_ttf
    font
    winpr
)

add_subdirectory(font)

add_library(dialogs STATIC
    ${SRCS}
)

target_link_libraries(dialogs PRIVATE ${LIBS})

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

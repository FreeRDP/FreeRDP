

find_package(SDL2TTF REQUIRED COMPONENTS)
include_directories(${SDL2TTF_INCLUDE_DIR})
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
    ${SDL2TTF_LIBRARY}
    font
)
add_subdirectory(font)

add_library(dialogs STATIC
    ${SRCS}
)

target_link_libraries(dialogs PRIVATE ${LIBS})

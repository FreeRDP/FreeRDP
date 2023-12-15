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
    sdl_connection_dialog.cpp
    sdl_connection_dialog.hpp
)

set(LIBS
    ${SDL2_LIBRARIES}
    font
    winpr
)

find_package(SDL2_ttf COMPONENTS)
if (NOT SDL2_ttf_FOUND)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SDL2_ttf REQUIRED SDL2_ttf)

	if (BUILD_SHARED_LIBS)
		list(APPEND LIBS ${SDL2_ttf_LIBRARIES})
		link_directories(${SDL2_ttf_LIBRARY_DIRS})
		include_directories(${SDL2_ttf_INCLUDE_DIRS})
	else()
		list(APPEND LIBS ${SDL2_ttf_STATIC_LIBRARIES})
		link_directories(${SDL2_ttf_STATIC_LIBRARY_DIRS})
		include_directories(${SDL2_ttf_STATIC_INCLUDE_DIRS})
	endif()
else()
	if (BUILD_SHARED_LIBS)
		list(APPEND LIBS SDL2_ttf::SDL2_ttf)
	else()
		list(APPEND LIBS SDL2_ttf::SDL2_ttf-static)
	endif()
endif()

add_subdirectory(font)

add_library(dialogs STATIC
    ${SRCS}
)

target_link_libraries(dialogs PRIVATE ${LIBS})

if(BUILD_TESTING)
#	add_subdirectory(test)
endif()

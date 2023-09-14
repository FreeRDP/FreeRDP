
if (NOT WIN32 AND NOT ANDROID)
	option(WITH_X11 "[x11] clipboard implementation" ON)
	option(WITH_WAYLAND "[wayland] clipboard implementation" ON)
endif()

set(SRCS
    sdl_cliprdr_context.cpp
    sdl_cliprdr_context.hpp
    sdl_cliprdr_factory.cpp
)
set(LIBS
    winpr
    freerdp
    freerdp-client
)

if (WITH_X11)
    find_package(X11 REQUIRED)
    add_definitions()

    list(APPEND SRCS
        sdl_x11_cliprdr_context.cpp
        sdl_x11_cliprdr_context.hpp
    )
    add_compile_definitions(WITH_X11)
endif()

if (WITH_WAYLAND)
    list(APPEND SRCS
        sdl_wayland_cliprdr_context.cpp
        sdl_wayland_cliprdr_context.hpp
    )
    add_compile_definitions(WITH_WAYLAND)
endif()

if (APPLE)
    list(APPEND SRCS
            sdl_apple_cliprdr_context.cpp
            sdl_apple_cliprdr_context.hpp
        )
endif()

if (ANDROID)
    list(APPEND SRCS
            sdl_android_cliprdr_context.cpp
            sdl_android_cliprdr_context.hpp
        )
endif()

if (WIN32)
    list(APPEND SRCS
            sdl_win_cliprdr_context.cpp
            sdl_win_cliprdr_context.hpp
        )
endif()

add_library(sdl-cliprdr STATIC ${SRCS})
target_link_libraries(sdl-cliprdr PRIVATE ${LIBS})


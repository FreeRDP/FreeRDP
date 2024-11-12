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

list(APPEND LIBS sdl2_client_res winpr)

if(NOT WITH_SDL_LINK_SHARED)
  list(APPEND LIBS ${SDL2_STATIC_LIBRARIES})
else()
  list(APPEND LIBS ${SDL2_LIBRARIES})
endif()

macro(find_sdl_component name)
  find_package(${name})
  if(NOT ${name}_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(${name} REQUIRED ${name})

    if(BUILD_SHARED_LIBS)
      list(APPEND LIBS ${${name}_LIBRARIES})
      link_directories(${${name}_LIBRARY_DIRS})
      include_directories(SYSTEM ${${name}_INCLUDE_DIRS})
    else()
      list(APPEND LIBS ${${name}_STATIC_LIBRARIES})
      link_directories(${${name}_STATIC_LIBRARY_DIRS})
      include_directories(SYSTEM ${${name}_STATIC_INCLUDE_DIRS})
    endif()
  else()
    if(WITH_SDL_LINK_SHARED)
      list(APPEND LIBS ${name}::${name})
    else()
      list(APPEND LIBS ${name}::${name}-static)
    endif()
  endif()
endmacro()

find_sdl_component(SDL2_ttf)

option(WITH_SDL_IMAGE_DIALOGS "Build with SDL_image support (recommended)" OFF)
if(WITH_SDL_IMAGE_DIALOGS)
  find_sdl_component(SDL2_image)
  add_compile_definitions(WITH_SDL_IMAGE_DIALOGS)
endif()

add_subdirectory(res)

add_library(sdl2-dialogs STATIC ${SRCS})

target_link_libraries(sdl2-dialogs PRIVATE ${LIBS})

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  #	add_subdirectory(test)
endif()

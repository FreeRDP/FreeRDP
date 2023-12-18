
add_executable(freerdp-res2bin
       convert_res_to_c.cpp
)

set(SRCS
    sdl_resource_manager.cpp
    sdl_resource_manager.hpp  
)

option(SDL_USE_COMPILED_RESOURCES "Compile in images/fonts" ON)

if (SDL_USE_COMPILED_RESOURCES)
    list(APPEND SRCS
        sdl_resource_file.cpp
        sdl_resource_file.hpp
    )
    file(GLOB_RECURSE RES_CPP_FILES
        LIST_DIRECTORIES false
        "bin/*.cpp"
    )
    file(GLOB_RECURSE RES_HDR_FILES
        LIST_DIRECTORIES false
        "bin/*.hpp"
    )
    list(APPEND RES_FILES
        ${RES_CPP_FILES}
        ${RES_HDR_FILES}
    )
    add_definitions(-DSDL_USE_COMPILED_RESOURCES)
endif()

add_library(sdl_client_res OBJECT
    ${RES_FILES}
    ${SRCS}
)
set_property(TARGET sdl_client_res PROPERTY POSITION_INDEPENDENT_CODE ON)

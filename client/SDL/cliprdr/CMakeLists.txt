set(SRCS
    sdl_cliprdr_context.cpp
    sdl_cliprdr_context.hpp
)
set(LIBS
    winpr
    freerdp
    freerdp-client
    SDL3::SDL3
)

add_library(sdl-cliprdr STATIC ${SRCS})
target_link_libraries(sdl-cliprdr PRIVATE ${LIBS})


set(WITH_WEBVIEW_DEFAULT OFF)
if (UNIX AND NOT APPLE)
    set(WITH_WEBVIEW_DEFAULT ON)
endif()

option(WITH_WEBVIEW "Build with WebView support for AAD login popup browser" ${WITH_WEBVIEW_DEFAULT})
if (WITH_WEBVIEW)
  option(WITH_WEBVIEW_QT "Build with QtWebEngine support for AAD login broweser popup" OFF)

  set(SRCS
    sdl_webview.hpp
    webview_impl.hpp
    sdl_webview.cpp
  )
  set(LIBS
    winpr
  )

  if (WITH_WEBVIEW_QT)
    find_package(Qt5 COMPONENTS WebEngineWidgets REQUIRED)

    list(APPEND SRCS
      qt/webview_impl.cpp
    )

    list(APPEND LIBS
      Qt5::WebEngineWidgets
    )
  else()
    list(APPEND SRCS
      wrapper/webview.h
      wrapper/webview_impl.cpp
    )

    if (WIN32)
      find_package(unofficial-webview2 CONFIG REQUIRED)
      list(APPEND LIBS
        unofficial::webview2::webview2
      )
    elseif(APPLE)
      find_library(WEBKIT Webkit REQUIRED)
      list(APPEND LIBS
        ${WEBKIT}
      )
    else()
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(WEBVIEW_GTK webkit2gtk-4.0 REQUIRED)
      include_directories(${WEBVIEW_GTK_INCLUDE_DIRS})
      list(APPEND LIBS
        ${WEBVIEW_GTK_LIBRARIES}
      )
    endif()
  endif()
else()
  set(SRCS
    dummy.cpp
  )
endif()

configure_file(sdl_config.hpp.in sdl_config.hpp @ONLY)

add_library(sdl3-aad-view STATIC
  ${SRCS}
)
target_include_directories(sdl3-aad-view PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(sdl3-aad-view
  PRIVATE
  ${LIBS}
)
target_compile_definitions(
  sdl3-aad-view
  PUBLIC
  ${DEFINITIONS}
)


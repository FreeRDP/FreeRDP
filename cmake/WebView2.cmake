if(WIN32)
  # WebView2 SDK
  set(WEBVIEW2_VERSION "1.0.2535.41")
  set(WEBVIEW2_URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}")
  set(WEBVIEW2_SHA256 "c9c5518e4d7efa9079ad87bafb64f3c8e8edca0e95d34df878034b880a7af56b")
  set(WEBVIEW2_DEFAULT_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")

  if(NOT EXISTS ${WEBVIEW2_PACKAGE_DIR})
    unset(WEBVIEW2_PACKAGE_DIR CACHE)
  endif()
  find_path(WEBVIEW2_PACKAGE_DIR NAMES "build/native/include/WebView2.h"
            NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH # dont prepend CMAKE_PREFIX
  )
  if(NOT WEBVIEW2_PACKAGE_DIR)
    message(WARNING "WebView2 SDK not found locally, downloading ${WEBVIEW2_VERSION} ...")
    set(WEBVIEW2_PACKAGE_DIR ${WEBVIEW2_DEFAULT_PACKAGE_DIR} CACHE PATH "WebView2 SDK PATH" FORCE)
    file(DOWNLOAD ${WEBVIEW2_URL} ${CMAKE_CURRENT_BINARY_DIR}/webview2.nuget EXPECTED_HASH SHA256=${WEBVIEW2_SHA256})
    file(MAKE_DIRECTORY ${WEBVIEW2_PACKAGE_DIR})
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E tar x "${CMAKE_CURRENT_BINARY_DIR}/webview2.nuget"
      WORKING_DIRECTORY "${WEBVIEW2_PACKAGE_DIR}"
    )
  endif()
  set(WEBVIEW2_PACKAGE_DIR ${WEBVIEW2_PACKAGE_DIR} CACHE INTERNAL "" FORCE)
endif()

function(target_link_webview2 target)
  if(WIN32)
    if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
      set(ARCH ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID})
    elseif(CMAKE_C_COMPILER_ARCHITECTURE_ID)
      set(ARCH ${CMAKE_C_COMPILER_ARCHITECTURE_ID})
    else()
      message(FATAL_ERROR "Unknown CMAKE_<lang>_COMPILER_ARCHITECTURE_ID")
    endif()
    target_include_directories(${target} PRIVATE "${WEBVIEW2_PACKAGE_DIR}/build/native/include")
    target_link_libraries(
      ${target} PRIVATE shlwapi version "${WEBVIEW2_PACKAGE_DIR}/build/native/${ARCH}/WebView2LoaderStatic.lib"
    )
  endif()
endfunction()

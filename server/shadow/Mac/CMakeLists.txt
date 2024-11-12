include(WarnUnmaintained)
warn_unmaintained("mac shadow server subsystem")

find_library(IOKIT IOKit REQUIRED)
find_library(IOSURFACE IOSurface REQUIRED)
find_library(CARBON Carbon REQUIRED)
find_package(PAM)

set(LIBS ${IOKIT} ${IOSURFACE} ${CARBON})

if(PAM_FOUND)
  add_compile_definitions(WITH_PAM)
  include_directories(SYSTEM ${PAM_INCLUDE_DIR})
  list(APPEND LIBS ${PAM_LIBRARY})
else()
  message("building without PAM authentication support")
endif()

add_compile_definitions(WITH_SHADOW_MAC)
add_library(freerdp-shadow-subsystem-impl STATIC mac_shadow.h mac_shadow.c)
target_link_libraries(freerdp-shadow-subsystem-impl PRIVATE ${LIBS})

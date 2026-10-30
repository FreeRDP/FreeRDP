include(WarnUnmaintained)
warn_unmaintained("windows shadow server subsystem" "-DWITH_SHADOW_SUBSYSTEM=OFF")

add_compile_definitions(WITH_SHADOW_WIN)

# The capture backend: the Windows Desktop Sharing API (the default) or DXGI
# desktop duplication, which also captures the secure desktop (logon, lock
# screen, UAC) when the server runs as SYSTEM on the console session
option(WITH_SHADOW_WIN_DXGI "Capture with DXGI desktop duplication instead of the Windows Desktop Sharing API" OFF)
set(SRCS win_dxgi.h win_rdp.h win_sas.h win_shadow.c win_shadow.h win_wds.h)
if(WITH_SHADOW_WIN_DXGI)
  add_compile_definitions(WITH_DXGI_1_2)
  list(APPEND SRCS win_dxgi.c)
else()
  list(APPEND SRCS win_rdp.c win_wds.c)
endif()
add_library(freerdp-shadow-subsystem-impl STATIC ${SRCS})
target_link_libraries(freerdp-shadow-subsystem-impl PRIVATE freerdp-client freerdp winpr)

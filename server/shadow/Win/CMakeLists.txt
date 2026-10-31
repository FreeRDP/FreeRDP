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

# freerdp-shadow-service: runs freerdp-shadow-cli as SYSTEM on the console
# session (logon and lock screens, UAC prompts; DXGI capture needed)
option(WITH_SHADOW_WIN_SERVICE "Build freerdp-shadow-service, which runs the shadow server on the console session" OFF)
if(WITH_SHADOW_WIN_SERVICE)
  add_executable(freerdp-shadow-service shadow_service.c win_sas.h)
  target_link_libraries(freerdp-shadow-service PRIVATE wtsapi32 advapi32 shell32)
  install(TARGETS freerdp-shadow-service DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)
endif()

# The screen of a GNOME session, through Mutter's own D-Bus APIs on the session
# bus: org.gnome.Mutter.ScreenCast (frames as a PipeWire stream, with damage and
# cursor metadata) and org.gnome.Mutter.RemoteDesktop (input through libei).
# No portal and no permission dialog: Mutter takes calls from the session bus,
# which only the session's user can reach.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(LIBEI REQUIRED IMPORTED_TARGET libei-1.0)

set(LIBS PkgConfig::SYSTEMD PkgConfig::PIPEWIRE PkgConfig::LIBEI)

find_package(PAM)
if(PAM_FOUND)
  add_compile_definitions(WITH_PAM)
  include_directories(SYSTEM ${PAM_INCLUDE_DIR})
  list(APPEND LIBS ${PAM_LIBRARY})
else()
  message("building without PAM authentication support")
endif()

add_library(freerdp-shadow-subsystem-mutter STATIC mutter_shadow.h mutter_shadow.c)
target_link_libraries(freerdp-shadow-subsystem-mutter PRIVATE ${LIBS})

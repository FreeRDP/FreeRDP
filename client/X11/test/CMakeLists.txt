find_program(XVFB_RUN_EXECUTABLE xvfb-run)
if(XVFB_RUN_EXECUTABLE)
  add_executable(TestX11RailConfigure TestX11RailConfigure.c)
  target_link_libraries(TestX11RailConfigure PRIVATE xfreerdp-client)
  add_test(NAME TestX11RailConfigure COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x1024x24 -nolisten tcp"
                                             $<TARGET_FILE:TestX11RailConfigure>
  )
endif()

if(XVFB_RUN_EXECUTABLE AND WITH_XEXT AND X11_Xext_FOUND AND X11_Xtst_FOUND)
  add_executable(TestX11RailResize TestX11RailResize.c)
  target_link_libraries(TestX11RailResize PRIVATE xfreerdp-client ${X11_Xext_LIB} ${X11_Xtst_LIB})
  add_test(NAME TestX11RailResize COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x1024x24 -nolisten tcp"
                                          $<TARGET_FILE:TestX11RailResize>
  )
endif()

if(XVFB_RUN_EXECUTABLE AND WITH_XRANDR AND X11_Xrandr_FOUND)
  add_executable(TestX11Monitor TestX11Monitor.c)
  target_link_libraries(TestX11Monitor PRIVATE xfreerdp-client ${X11_Xrandr_LIB})
  add_test(NAME TestX11Monitor COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 3120x1920x24 -nolisten tcp"
                                       $<TARGET_FILE:TestX11Monitor>
  )
endif()

add_executable(TestX11DeferredAlt TestX11DeferredAlt.c)
target_link_libraries(TestX11DeferredAlt PRIVATE xfreerdp-client)
add_test(NAME TestX11DeferredAlt COMMAND TestX11DeferredAlt)

add_executable(TestX11CmdLine TestX11CmdLine.c)
target_link_libraries(TestX11CmdLine PRIVATE xfreerdp-client)
add_test(NAME TestX11CmdLine COMMAND TestX11CmdLine)

add_executable(TestX11RailGeometry TestX11RailGeometry.c)
target_link_libraries(TestX11RailGeometry PRIVATE xfreerdp-client)
add_test(NAME TestX11RailGeometry COMMAND TestX11RailGeometry)

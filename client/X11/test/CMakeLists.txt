find_program(XVFB_RUN_EXECUTABLE xvfb-run)
if(XVFB_RUN_EXECUTABLE AND WITH_XRANDR AND X11_Xrandr_FOUND)
  add_executable(TestX11Monitor TestX11Monitor.c)
  target_link_libraries(TestX11Monitor PRIVATE xfreerdp-client ${X11_Xrandr_LIB})
  add_test(NAME TestX11Monitor COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 3120x1920x24 -nolisten tcp"
                                       $<TARGET_FILE:TestX11Monitor>
  )

  add_executable(TestX11MonitorScale TestX11MonitorScale.c)
  target_include_directories(TestX11MonitorScale PRIVATE ..)
  target_link_libraries(TestX11MonitorScale PRIVATE xfreerdp-client freerdp-client freerdp winpr X11::Xrandr)
  add_test(NAME TestX11MonitorScale COMMAND ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 3120x1920x24 -nolisten tcp"
                                           $<TARGET_FILE:TestX11MonitorScale>
  )
endif()

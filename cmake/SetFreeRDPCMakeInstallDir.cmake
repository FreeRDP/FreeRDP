function(SetFreeRDPCMakeInstallDir SETVAR subdir)
  set(${SETVAR} "${CMAKE_INSTALL_LIBDIR}/cmake/${subdir}" PARENT_SCOPE)
endfunction()

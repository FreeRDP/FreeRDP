include(GNUInstallDirs)

function(install_freerdp_man manpage section)
 if(WITH_MANPAGES)
   install(FILES ${manpage} DESTINATION ${CMAKE_INSTALL_MANDIR}/man${section})
 endif()
endfunction()

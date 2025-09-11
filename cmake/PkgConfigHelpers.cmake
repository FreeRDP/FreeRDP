set(FREERDP_CLIENT_PC_REQUIRES_PRIVATE "" CACHE INTERNAL "dependencies")

function(freerdp_client_pc_add_requires_private)
  foreach(_lib ${ARGN})
    list(APPEND FREERDP_CLIENT_PC_REQUIRES_PRIVATE ${_lib})
  endforeach()
  set(FREERDP_CLIENT_PC_REQUIRES_PRIVATE ${FREERDP_CLIENT_PC_REQUIRES_PRIVATE} CACHE INTERNAL "dependencies")
endfunction()

set(FREERDP_CLIENT_PC_LIBRARY_PRIVATE "" CACHE INTERNAL "dependencies")

function(freerdp_client_pc_add_library_private)
  foreach(_lib ${ARGN})
    list(APPEND FREERDP_CLIENT_PC_LIBRARY_PRIVATE ${_lib})
  endforeach()
  set(FREERDP_CLIENT_PC_LIBRARY_PRIVATE ${FREERDP_CLIENT_PC_LIBRARY_PRIVATE} CACHE INTERNAL "dependencies")
endfunction()

if(NOT WITH_SANITIZE_ADDRESS AND NOT WITH_SANITIZE_MEMORY AND NOT WITH_SANITIZE_THREAD)
  return()
endif()

# SUSE 16 rpm-build injects -Wl,--no-undefined through %cmake. Sanitizer shared-library builds can
# leave sanitizer runtime symbols unresolved.
foreach(_freerdp_linker_flags CMAKE_EXE_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS)
  if(DEFINED ${_freerdp_linker_flags})
    string(REPLACE "-Wl,--no-undefined" "" _freerdp_flags "${${_freerdp_linker_flags}}")
    string(STRIP "${_freerdp_flags}" _freerdp_flags)
    set(${_freerdp_linker_flags} "${_freerdp_flags}" CACHE STRING "Linker flags" FORCE)
  endif()
endforeach()

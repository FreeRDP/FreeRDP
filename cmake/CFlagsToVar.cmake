function(CFlagsToVar NAME CFG)
  string(TOUPPER "${CFG}" UCFG)
  set(C_FLAGS ${CMAKE_C_FLAGS})
  string(REPLACE "${CMAKE_SOURCE_DIR}" "<src dir>" C_FLAGS "${C_FLAGS}")
  string(REPLACE "${CMAKE_BINARY_DIR}" "<build dir>" C_FLAGS "${C_FLAGS}")

  string(APPEND C_FLAGS " ${CMAKE_C_FLAGS_${UCFG}}")

  string(REPLACE "\$" "\\\$" C_FLAGS "${C_FLAGS}")
  string(REPLACE "\"" "\\\"" C_FLAGS "${C_FLAGS}")
  set(${NAME} ${C_FLAGS} PARENT_SCOPE)
endfunction()

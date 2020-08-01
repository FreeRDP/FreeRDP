set(MODULE_NAME "freerdp-mit-krb5-pkinit")
set(MODULE_PREFIX "FREERDP_MIT_KRB5_PKINIT")
set(${MODULE_PREFIX}_SRCS
  kinit.c
  kinit_kdb.c
  )
set(${MODULE_PREFIX}_LIBRARIES
  kdb5
  kadm5clnt
  )
freerdp_module_add(${${MODULE_PREFIX}_SRCS})
freerdp_library_add(${${MODULE_PREFIX}_LIBRARIES})

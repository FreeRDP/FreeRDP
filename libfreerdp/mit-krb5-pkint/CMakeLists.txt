set(MODULE_NAME "freerdp-mit-krb5-pkinit")
set(MODULE_PREFIX "FREERDP_MIT_KRB5_PKINIT")
set(${MODULE_PREFIX}_SRCS
    kinit.c
)
freerdp_module_add(${${MODULE_PREFIX}_SRCS})

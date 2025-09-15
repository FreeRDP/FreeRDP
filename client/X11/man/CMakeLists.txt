set(DEPS
    ../../common/man/freerdp-global-options.1
    xfreerdp-shortcuts.1
    ../../common/man/freerdp-global-envvar.1
    ../../common/man/freerdp-global-config.1
    xfreerdp-global-config.1
    xfreerdp-examples.1
    ../../common/man/freerdp-global-links.1
)

include(GetSysconfDir)
get_sysconf_dir("" SYSCONF_DIR)
set(VAR_NAMES "VENDOR" "PRODUCT" "VENDOR_PRODUCT" "SYSCONF_DIR")
generate_and_install_freerdp_man_from_xml(${MODULE_NAME} "1" "${DEPS}" "${VAR_NAMES}")

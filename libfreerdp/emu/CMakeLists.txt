if (WITH_SMARTCARD_EMULATE)
    list(APPEND EMULATE_SRCS
        scard/smartcard_emulate.c
        scard/FreeRDP.ico.h
        scard/FreeRDP.ico.c
        scard/smartcard_virtual_gids.h
        scard/smartcard_virtual_gids.c
        )
    freerdp_module_add(${EMULATE_SRCS})
    freerdp_library_add(ZLIB::ZLIB)
endif()

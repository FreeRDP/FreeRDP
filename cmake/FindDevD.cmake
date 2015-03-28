# Configure devd environment
#
# DEVD_FOUND - system has a devd
# DEVD_BIN_DIR - devd bin dir
# DEVD_SKT_DIR - devd socket dir
#
# Copyright (c) 2015 Rozhuk Ivan <rozhuk.im@gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.
#


FIND_PATH(
    DEVD_BIN_DIR
    NAMES devd
    PATHS /sbin /usr/sbin /usr/local/sbin
)

FIND_PATH(
    DEVD_SKT_DIR
    NAMES devd.seqpacket.pipe devd.pipe
    PATHS /var/run/
)


if (DEVD_BIN_DIR)
    set(DEVD_FOUND "YES")
    message(STATUS "devd found")
    if (NOT DEVD_SKT_DIR)
	message(STATUS "devd not running!")
    endif (NOT DEVD_SKT_DIR)
endif (DEVD_BIN_DIR)

file(GLOB_RECURSE archives "${FREERDP_LIB_DIR}/*.a")
string(REPLACE ";" "\n" archives_str "${archives}")
file(WRITE "${FREERDP_LIB_DIR}/freerdp_archives.rsp" "${archives_str}\n")

file(GLOB RDTK_HEADERS "rdkt/*.h")
install(FILES ${RDTK_HEADERS} DESTINATION include/rdkt${RDTK_API_VERSION}/rdkt COMPONENT headers)

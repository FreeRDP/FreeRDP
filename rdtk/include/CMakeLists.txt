file(GLOB RDTK_HEADERS "rdtk/*.h")
install(FILES ${RDTK_HEADERS} DESTINATION include/rdtk${RDTK_API_VERSION}/rdtk COMPONENT headers)

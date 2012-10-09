
option(WITH_MANPAGES "Generate manpages." ON)
option(WITH_NEON "Enable NEON optimization for rfx decoder" OFF)
option(WITH_PROFILER "Compile profiler." OFF)
option(WITH_SSE2_TARGET "Allow compiler to generate SSE2 instructions." OFF)
option(WITH_SSE2 "Use SSE2 optimization." OFF)
option(WITH_JPEG "Use JPEG decoding." OFF)

if(APPLE)
	option(WITH_CLANG "Build using clang" OFF)
endif()

if(MSVC)
	option(WITH_NATIVE_SSPI "Use native SSPI modules" ON)
	option(WITH_WIN8 "Use Windows 8 libraries" OFF)
endif()

option(BUILD_TESTING "Build unit tests" OFF)

if(${CMAKE_VERSION} VERSION_GREATER 2.8.8)
	option(MONOLITHIC_BUILD "Use monolithic build" OFF)
endif()

option(WITH_CLIENT "Build client binaries" ON)
option(WITH_SERVER "Build server binaries" OFF)

option(WITH_CHANNELS "Build virtual channel plugins" ON)

if(WITH_CLIENT AND WITH_CHANNELS)
	option(WITH_CLIENT_CHANNELS "Build virtual channel plugins" ON)
endif()

if(WITH_SERVER AND WITH_CHANNELS)
	option(WITH_SERVER_CHANNELS "Build virtual channel plugins" ON)
endif()

option(WITH_THIRD_PARTY "Build third-party components" OFF)

option(WITH_SERVER_INTERFACE "Build server as a library with an interface" OFF)

option(WITH_DEBUG_ALL "Print all debug messages." OFF)

if(WITH_DEBUG_ALL)
	set(DEFAULT_DEBUG_OPTION "ON")
else()
	set(DEFAULT_DEBUG_OPTION "OFF")
endif()

option(WITH_DEBUG_CERTIFICATE "Print certificate related debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_CHANNELS "Print channel manager debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_CLIPRDR "Print clipboard redirection debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_DVC "Print dynamic virtual channel debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_GDI "Print graphics debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_KBD "Print keyboard related debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_LICENSE "Print license debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_NEGO "Print negotiation related debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_NLA "Print authentication related debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_NTLM "Print NTLM debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_TSG "Print Terminal Server Gateway debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_ORDERS "Print drawing orders debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RAIL "Print RemoteApp debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RDP "Print RDP debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_REDIR "Redirection debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RFX "Print RemoteFX debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_SCARD "Print smartcard debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_SVC "Print static virtual channel debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_TRANSPORT "Print transport debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_TIMEZONE "Print timezone debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_WND "Print window order debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_X11_CLIPRDR "Print X11 clipboard redirection debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_X11_LOCAL_MOVESIZE "Print X11 Client local movesize debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_X11 "Print X11 Client debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_XV "Print XVideo debug messages" ${DEFAULT_DEBUG_OPTION})


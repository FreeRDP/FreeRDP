/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Settings
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@gmail.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_SETTINGS_TYPES_H
#define FREERDP_SETTINGS_TYPES_H

#include <winpr/timezone.h>
#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/config.h>
#include <freerdp/types.h>
#include <freerdp/redirection.h>

#include <freerdp/crypto/certificate.h>
#include <freerdp/crypto/privatekey.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @addtogroup rdpSettings
	 * @{
	 */

/* Surface Commands Flags */
#define SURFCMDS_SET_SURFACE_BITS 0x00000002    /** @since version 3.7.0 */
#define SURFCMDS_FRAME_MARKER 0x00000010        /** @since version 3.7.0 */
#define SURFCMDS_STREAM_SURFACE_BITS 0x00000040 /** @since version 3.7.0 */

/* RAIL Support Level */
#define RAIL_LEVEL_SUPPORTED 0x00000001
#define RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED 0x00000002
#define RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED 0x00000004
#define RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED 0x00000008
#define RAIL_LEVEL_SERVER_TO_CLIENT_IME_SYNC_SUPPORTED 0x00000010
#define RAIL_LEVEL_HIDE_MINIMIZED_APPS_SUPPORTED 0x00000020
#define RAIL_LEVEL_WINDOW_CLOAKING_SUPPORTED 0x00000040
#define RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED 0x00000080

/* Performance Flags */
#define PERF_FLAG_NONE 0x00000000
#define PERF_DISABLE_WALLPAPER 0x00000001
#define PERF_DISABLE_FULLWINDOWDRAG 0x00000002
#define PERF_DISABLE_MENUANIMATIONS 0x00000004
#define PERF_DISABLE_THEMING 0x00000008
#define PERF_DISABLE_CURSOR_SHADOW 0x00000020
#define PERF_DISABLE_CURSORSETTINGS 0x00000040
#define PERF_ENABLE_FONT_SMOOTHING 0x00000080
#define PERF_ENABLE_DESKTOP_COMPOSITION 0x00000100

/* Connection Types */
#define CONNECTION_TYPE_INVALID                                                               \
	0x00 /** @brief synthetic, removes RNS_UD_CS_VALID_CONNECTION_TYPE from ConnectionType in \
	      * EarlyCapabilityFlags                                                              \
	      * @since version 3.6.0                                                              \
	      */
#define CONNECTION_TYPE_MODEM 0x01
#define CONNECTION_TYPE_BROADBAND_LOW 0x02
#define CONNECTION_TYPE_SATELLITE 0x03
#define CONNECTION_TYPE_BROADBAND_HIGH 0x04
#define CONNECTION_TYPE_WAN 0x05
#define CONNECTION_TYPE_LAN 0x06
#define CONNECTION_TYPE_AUTODETECT 0x07

/* Client to Server (CS) data blocks */
#define CS_CORE 0xC001
#define CS_SECURITY 0xC002
#define CS_NET 0xC003
#define CS_CLUSTER 0xC004
#define CS_MONITOR 0xC005
#define CS_MCS_MSGCHANNEL 0xC006
#define CS_MONITOR_EX 0xC008
#define CS_UNUSED1 0xC00C
#define CS_MULTITRANSPORT 0xC00A

/* Server to Client (SC) data blocks */
#define SC_CORE 0x0C01
#define SC_SECURITY 0x0C02
#define SC_NET 0x0C03
#define SC_MCS_MSGCHANNEL 0x0C04
#define SC_MULTITRANSPORT 0x0C08

	/* RDP versions, see
	 * [MS-RDPBCGR] 2.2.1.3.2 Client Core Data (TS_UD_CS_CORE)
	 * [MS-RDPBCGR] 2.2.1.4.2 Server Core Data (TS_UD_SC_CORE)
	 */
	typedef enum
	{
		RDP_VERSION_4 = 0x00080001,
		RDP_VERSION_5_PLUS = 0x00080004,
		RDP_VERSION_10_0 = 0x00080005,
		RDP_VERSION_10_1 = 0x00080006,
		RDP_VERSION_10_2 = 0x00080007,
		RDP_VERSION_10_3 = 0x00080008,
		RDP_VERSION_10_4 = 0x00080009,
		RDP_VERSION_10_5 = 0x0008000a,
		RDP_VERSION_10_6 = 0x0008000b,
		RDP_VERSION_10_7 = 0x0008000C,
		RDP_VERSION_10_8 = 0x0008000D,
		RDP_VERSION_10_9 = 0x0008000E,
		RDP_VERSION_10_10 = 0x0008000F,
		RDP_VERSION_10_11 = 0x00080010,
		RDP_VERSION_10_12 = 0x00080011
	} RDP_VERSION;

/* Color depth */
#define RNS_UD_COLOR_4BPP 0xCA00
#define RNS_UD_COLOR_8BPP 0xCA01
#define RNS_UD_COLOR_16BPP_555 0xCA02
#define RNS_UD_COLOR_16BPP_565 0xCA03
#define RNS_UD_COLOR_24BPP 0xCA04

/* Secure Access Sequence */
#define RNS_UD_SAS_DEL 0xAA03

/* Supported Color Depths */
#define RNS_UD_24BPP_SUPPORT 0x0001
#define RNS_UD_16BPP_SUPPORT 0x0002
#define RNS_UD_15BPP_SUPPORT 0x0004
#define RNS_UD_32BPP_SUPPORT 0x0008

/* Audio Mode */
#define AUDIO_MODE_REDIRECT 0       /* Bring to this computer */
#define AUDIO_MODE_PLAY_ON_SERVER 1 /* Leave at remote computer */
#define AUDIO_MODE_NONE 2           /* Do not play */

/* Early Capability Flags (Client to Server) */
#define RNS_UD_CS_SUPPORT_ERRINFO_PDU 0x0001
#define RNS_UD_CS_WANT_32BPP_SESSION 0x0002
#define RNS_UD_CS_SUPPORT_STATUSINFO_PDU 0x0004
#define RNS_UD_CS_STRONG_ASYMMETRIC_KEYS 0x0008
#define RNS_UD_CS_RELATIVE_MOUSE_INPUT 0x0010
#define RNS_UD_CS_VALID_CONNECTION_TYPE 0x0020
#define RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU 0x0040
#define RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT 0x0080
#define RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL 0x0100
#define RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE 0x0200
#define RNS_UD_CS_SUPPORT_HEARTBEAT_PDU 0x0400
#define RNS_UD_CS_SUPPORT_SKIP_CHANNELJOIN 0x0800

/* Early Capability Flags (Server to Client) */
#define RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V1 0x00000001
#define RNS_UD_SC_DYNAMIC_DST_SUPPORTED 0x00000002
#define RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V2 0x00000004
#define RNS_UD_SC_SKIP_CHANNELJOIN_SUPPORTED 0x00000008

/* Cluster Information Flags */
#define REDIRECTION_SUPPORTED 0x00000001
#define REDIRECTED_SESSIONID_FIELD_VALID 0x00000002
#define REDIRECTED_SMARTCARD 0x00000040

#define ServerSessionRedirectionVersionMask 0x0000003c
#define REDIRECTION_VERSION1 0x00
#define REDIRECTION_VERSION2 0x01
#define REDIRECTION_VERSION3 0x02
#define REDIRECTION_VERSION4 0x03
#define REDIRECTION_VERSION5 0x04
#define REDIRECTION_VERSION6 0x05

#define MONITOR_PRIMARY 0x00000001

/* Encryption Methods */
#define ENCRYPTION_METHOD_NONE 0x00000000
#define ENCRYPTION_METHOD_40BIT 0x00000001
#define ENCRYPTION_METHOD_128BIT 0x00000002
#define ENCRYPTION_METHOD_56BIT 0x00000008
#define ENCRYPTION_METHOD_FIPS 0x00000010

/* Encryption Levels */
#define ENCRYPTION_LEVEL_NONE 0x00000000
#define ENCRYPTION_LEVEL_LOW 0x00000001
#define ENCRYPTION_LEVEL_CLIENT_COMPATIBLE 0x00000002
#define ENCRYPTION_LEVEL_HIGH 0x00000003
#define ENCRYPTION_LEVEL_FIPS 0x00000004

/* Multitransport Types */
#define TRANSPORT_TYPE_UDP_FECR 0x00000001
#define TRANSPORT_TYPE_UDP_FECL 0x00000004
#define TRANSPORT_TYPE_UDP_PREFERRED 0x00000100
#define SOFTSYNC_TCP_TO_UDP 0x00000200

/* Static Virtual Channel Options */
#define CHANNEL_OPTION_INITIALIZED 0x80000000
#define CHANNEL_OPTION_ENCRYPT_RDP 0x40000000
#define CHANNEL_OPTION_ENCRYPT_SC 0x20000000
#define CHANNEL_OPTION_ENCRYPT_CS 0x10000000
#define CHANNEL_OPTION_PRI_HIGH 0x08000000
#define CHANNEL_OPTION_PRI_MED 0x04000000
#define CHANNEL_OPTION_PRI_LOW 0x02000000
#define CHANNEL_OPTION_COMPRESS_RDP 0x00800000
#define CHANNEL_OPTION_COMPRESS 0x00400000
#define CHANNEL_OPTION_SHOW_PROTOCOL 0x00200000
#define CHANNEL_REMOTE_CONTROL_PERSISTENT 0x00100000

/* Virtual Channel Capability Flags */
#define VCCAPS_NO_COMPR 0x00000000
#define VCCAPS_COMPR_SC 0x00000001
#define VCCAPS_COMPR_CS_8K 0x00000002

/* Large Pointer Support Flags */
#define LARGE_POINTER_FLAG_96x96 0x00000001
#define LARGE_POINTER_FLAG_384x384 0x00000002

/* Auto Reconnect Version */
#define AUTO_RECONNECT_VERSION_1 0x00000001

/* Cookie Lengths */
#define MSTSC_COOKIE_MAX_LENGTH 9
#define DEFAULT_COOKIE_MAX_LENGTH 0xFF

	/* General capability set */
#define TS_CAPS_PROTOCOLVERSION 0x200 /** @since version 3.6.0 */

/* Order Support */
#define NEG_DSTBLT_INDEX 0x00
#define NEG_PATBLT_INDEX 0x01
#define NEG_SCRBLT_INDEX 0x02
#define NEG_MEMBLT_INDEX 0x03
#define NEG_MEM3BLT_INDEX 0x04
#define NEG_ATEXTOUT_INDEX 0x05
#define NEG_AEXTTEXTOUT_INDEX 0x06  /* Must be ignored */
#define NEG_DRAWNINEGRID_INDEX 0x07 /* Must be ignored */
#define NEG_LINETO_INDEX 0x08
#define NEG_MULTI_DRAWNINEGRID_INDEX 0x09
#define NEG_OPAQUE_RECT_INDEX 0x0A /* Must be ignored */
#define NEG_SAVEBITMAP_INDEX 0x0B
#define NEG_WTEXTOUT_INDEX 0x0C   /* Must be ignored */
#define NEG_MEMBLT_V2_INDEX 0x0D  /* Must be ignored */
#define NEG_MEM3BLT_V2_INDEX 0x0E /* Must be ignored */
#define NEG_MULTIDSTBLT_INDEX 0x0F
#define NEG_MULTIPATBLT_INDEX 0x10
#define NEG_MULTISCRBLT_INDEX 0x11
#define NEG_MULTIOPAQUERECT_INDEX 0x12
#define NEG_FAST_INDEX_INDEX 0x13
#define NEG_POLYGON_SC_INDEX 0x14
#define NEG_POLYGON_CB_INDEX 0x15
#define NEG_POLYLINE_INDEX 0x16
#define NEG_UNUSED23_INDEX 0x17 /* Must be ignored */
#define NEG_FAST_GLYPH_INDEX 0x18
#define NEG_ELLIPSE_SC_INDEX 0x19
#define NEG_ELLIPSE_CB_INDEX 0x1A
#define NEG_GLYPH_INDEX_INDEX 0x1B
#define NEG_GLYPH_WEXTTEXTOUT_INDEX 0x1C     /* Must be ignored */
#define NEG_GLYPH_WLONGTEXTOUT_INDEX 0x1D    /* Must be ignored */
#define NEG_GLYPH_WLONGEXTTEXTOUT_INDEX 0x1E /* Must be ignored */
#define NEG_UNUSED31_INDEX 0x1F              /* Must be ignored */

/* Glyph Support Level */
#define GLYPH_SUPPORT_NONE 0x0000
#define GLYPH_SUPPORT_PARTIAL 0x0001
#define GLYPH_SUPPORT_FULL 0x0002
#define GLYPH_SUPPORT_ENCODE 0x0003

/* Gateway Usage Method */
#define TSC_PROXY_MODE_NONE_DIRECT 0x0
#define TSC_PROXY_MODE_DIRECT 0x1
#define TSC_PROXY_MODE_DETECT 0x2
#define TSC_PROXY_MODE_DEFAULT 0x3
#define TSC_PROXY_MODE_NONE_DETECT 0x4

/* Gateway Credentials Source */
#define TSC_PROXY_CREDS_MODE_USERPASS 0x0
#define TSC_PROXY_CREDS_MODE_SMARTCARD 0x1
#define TSC_PROXY_CREDS_MODE_ANY 0x2

/* Keyboard Hook */
#define KEYBOARD_HOOK_LOCAL 0
#define KEYBOARD_HOOK_REMOTE 1
#define KEYBOARD_HOOK_FULLSCREEN_ONLY 2

	typedef struct
	{
		UINT32 Length;
		LPWSTR Address;
	} TARGET_NET_ADDRESS;

/* Logon Error Info */
#define LOGON_MSG_DISCONNECT_REFUSED 0xFFFFFFF9
#define LOGON_MSG_NO_PERMISSION 0xFFFFFFFA
#define LOGON_MSG_BUMP_OPTIONS 0xFFFFFFFB
#define LOGON_MSG_RECONNECT_OPTIONS 0xFFFFFFFC
#define LOGON_MSG_SESSION_TERMINATE 0xFFFFFFFD
#define LOGON_MSG_SESSION_CONTINUE 0xFFFFFFFE

#define LOGON_FAILED_BAD_PASSWORD 0x00000000
#define LOGON_FAILED_UPDATE_PASSWORD 0x00000001
#define LOGON_FAILED_OTHER 0x00000002
#define LOGON_WARNING 0x00000003

/* Server Status Info */
#define STATUS_FINDING_DESTINATION 0x00000401
#define STATUS_LOADING_DESTINATION 0x00000402
#define STATUS_BRINGING_SESSION_ONLINE 0x00000403
#define STATUS_REDIRECTING_TO_DESTINATION 0x00000404
#define STATUS_VM_LOADING 0x00000501
#define STATUS_VM_WAKING 0x00000502
#define STATUS_VM_BOOTING 0x00000503

/* Compression Flags */
#define PACKET_COMPR_TYPE_8K 0x00
#define PACKET_COMPR_TYPE_64K 0x01
#define PACKET_COMPR_TYPE_RDP6 0x02
#define PACKET_COMPR_TYPE_RDP61 0x03
#define PACKET_COMPR_TYPE_RDP8 0x04

/* Desktop Rotation Flags */
#define ORIENTATION_LANDSCAPE 0
#define ORIENTATION_PORTRAIT 90
#define ORIENTATION_LANDSCAPE_FLIPPED 180
#define ORIENTATION_PORTRAIT_FLIPPED 270

/* Clipboard feature mask */
#define CLIPRDR_FLAG_LOCAL_TO_REMOTE 0x01
#define CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES 0x02
#define CLIPRDR_FLAG_REMOTE_TO_LOCAL 0x10
#define CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES 0x20

#define CLIPRDR_FLAG_DEFAULT_MASK                                        \
	(CLIPRDR_FLAG_LOCAL_TO_REMOTE | CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES | \
	 CLIPRDR_FLAG_REMOTE_TO_LOCAL | CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES)

	/* ARC_CS_PRIVATE_PACKET */
	typedef struct
	{
		UINT32 cbLen;
		UINT32 version;
		UINT32 logonId;
		BYTE securityVerifier[16];
	} ARC_CS_PRIVATE_PACKET;

	/* ARC_SC_PRIVATE_PACKET */
	typedef struct
	{
		UINT32 cbLen;
		UINT32 version;
		UINT32 logonId;
		BYTE arcRandomBits[16];
	} ARC_SC_PRIVATE_PACKET;

	/* Channels */

	typedef struct
	{
		int argc;
		char** argv;
	} ADDIN_ARGV;

	/* Extensions */

	struct rdp_ext_set
	{
		char name[256]; /* plugin name or path */
		void* data;     /* plugin data */
	};

	/* Bitmap Cache */

	typedef struct
	{
		UINT16 numEntries;
		UINT16 maxSize;
	} BITMAP_CACHE_CELL_INFO;

	typedef struct
	{
		UINT32 numEntries;
		BOOL persistent;
	} BITMAP_CACHE_V2_CELL_INFO;

	/* Glyph Cache */

	typedef struct
	{
		UINT16 cacheEntries;
		UINT16 cacheMaximumCellSize;
	} GLYPH_CACHE_DEFINITION;

	/* Monitors */

	typedef struct
	{
		INT32 left;
		INT32 top;
		INT32 right;
		INT32 bottom;
		UINT32 flags;
	} MONITOR_DEF;

	typedef struct
	{
		UINT32 physicalWidth;
		UINT32 physicalHeight;
		UINT32 orientation;
		UINT32 desktopScaleFactor;
		UINT32 deviceScaleFactor;
	} MONITOR_ATTRIBUTES;

	typedef struct
	{
		INT32 x;
		INT32 y;
		INT32 width;
		INT32 height;
		UINT32 is_primary;
		UINT32 orig_screen;
		MONITOR_ATTRIBUTES attributes;
	} rdpMonitor;

/* Device Redirection */
#define RDPDR_DTYP_SERIAL 0x00000001
#define RDPDR_DTYP_PARALLEL 0x00000002
#define RDPDR_DTYP_PRINT 0x00000004
#define RDPDR_DTYP_FILESYSTEM 0x00000008
#define RDPDR_DTYP_SMARTCARD 0x00000020

	typedef struct
	{
		UINT32 Id;
		UINT32 Type;
		char* Name;
	} RDPDR_DEVICE;

	typedef struct
	{
		RDPDR_DEVICE device;
		char* Path;
		BOOL automount;
	} RDPDR_DRIVE;

	typedef struct
	{
		RDPDR_DEVICE device;
		char* DriverName;
		BOOL IsDefault;
	} RDPDR_PRINTER;

	typedef struct
	{
		RDPDR_DEVICE device;
	} RDPDR_SMARTCARD;

	typedef struct
	{
		RDPDR_DEVICE device;
		char* Path;
		char* Driver;
		char* Permissive;
	} RDPDR_SERIAL;

	typedef struct
	{
		RDPDR_DEVICE device;
		char* Path;
	} RDPDR_PARALLEL;

#define PROXY_TYPE_NONE 0
#define PROXY_TYPE_HTTP 1
#define PROXY_TYPE_SOCKS 2
#define PROXY_TYPE_IGNORE 0xFFFF

/* ThreadingFlags */
#define THREADING_FLAGS_DISABLE_THREADS 0x00000001

	enum rdp_settings_type
	{
		RDP_SETTINGS_TYPE_BOOL,
		RDP_SETTINGS_TYPE_UINT16,
		RDP_SETTINGS_TYPE_INT16,
		RDP_SETTINGS_TYPE_UINT32,
		RDP_SETTINGS_TYPE_INT32,
		RDP_SETTINGS_TYPE_UINT64,
		RDP_SETTINGS_TYPE_INT64,
		RDP_SETTINGS_TYPE_STRING,
		RDP_SETTINGS_TYPE_POINTER
	};

/**
 * rdpSettings creation flags
 */
#define FREERDP_SETTINGS_SERVER_MODE 0x00000001

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* FREERDP_SETTINGS_TYPES_H */

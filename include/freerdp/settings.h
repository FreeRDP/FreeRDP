/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Settings
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@gmail.com>
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

#ifndef FREERDP_SETTINGS_H
#define FREERDP_SETTINGS_H

#include <winpr/timezone.h>
#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/types.h>

/** \file
 * \brief This is the FreeRDP settings module.
 *
 * Settings are used to store configuration data for an RDP connection.
 * There are 3 different settings for each client and server:
 *
 * 1. The initial connection supplied by the user
 * 2. The settings sent from client or server during capability exchange
 * 3. The settings merged from the capability exchange and the initial configuration.
 *
 * The lifetime of the settings is as follows:
 * 1. Initial configuration is saved and will be valid for the whole application lifecycle
 * 2. The client or server settings from the other end are valid from capability exchange until the
 * connection is ended (disconnect/redirect/...)
 * 3. The merged settings are created from the initial configuration and server settings and have
 * the same lifetime, until the connection ends
 *
 *
 * So, when accessing the settings always ensure to know which one you are operating on! (this is
 * especially important for the proxy where you have a RDP client and RDP server in the same
 * application context)
 */

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
#define RNS_UD_CS_VALID_CONNECTION_TYPE 0x0020
#define RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU 0x0040
#define RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT 0x0080
#define RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL 0x0100
#define RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE 0x0200
#define RNS_UD_CS_SUPPORT_HEARTBEAT_PDU 0x0400

/* Early Capability Flags (Server to Client) */
#define RNS_UD_SC_EDGE_ACTIONS_SUPPORTED 0x00000001
#define RNS_UD_SC_DYNAMIC_DST_SUPPORTED 0x00000002
#define RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V2 0x00000004

/* Cluster Information Flags */
#define REDIRECTION_SUPPORTED 0x00000001
#define REDIRECTED_SESSIONID_FIELD_VALID 0x00000002
#define REDIRECTED_SMARTCARD 0x00000040

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

/* Auto Reconnect Version */
#define AUTO_RECONNECT_VERSION_1 0x00000001

/* Cookie Lengths */
#define MSTSC_COOKIE_MAX_LENGTH 9
#define DEFAULT_COOKIE_MAX_LENGTH 0xFF

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

/* Redirection Flags */
#define LB_TARGET_NET_ADDRESS 0x00000001
#define LB_LOAD_BALANCE_INFO 0x00000002
#define LB_USERNAME 0x00000004
#define LB_DOMAIN 0x00000008
#define LB_PASSWORD 0x00000010
#define LB_DONTSTOREUSERNAME 0x00000020
#define LB_SMARTCARD_LOGON 0x00000040
#define LB_NOREDIRECT 0x00000080
#define LB_TARGET_FQDN 0x00000100
#define LB_TARGET_NETBIOS_NAME 0x00000200
#define LB_TARGET_NET_ADDRESSES 0x00000800
#define LB_CLIENT_TSV_URL 0x00001000
#define LB_SERVER_TSV_CAPABLE 0x00002000

#define LB_PASSWORD_MAX_LENGTH 512

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

/* Certificates */

struct rdp_CertBlob
{
	UINT32 length;
	BYTE* data;
};
typedef struct rdp_CertBlob rdpCertBlob;

struct rdp_X509CertChain
{
	UINT32 count;
	rdpCertBlob* array;
};
typedef struct rdp_X509CertChain rdpX509CertChain;

struct rdp_CertInfo
{
	BYTE* Modulus;
	DWORD ModulusLength;
	BYTE exponent[4];
};
typedef struct rdp_CertInfo rdpCertInfo;

struct rdp_certificate
{
	rdpCertInfo cert_info;
	rdpX509CertChain* x509_cert_chain;
};
typedef struct rdp_certificate rdpCertificate;

typedef struct
{
	BYTE* Modulus;
	DWORD ModulusLength;
	BYTE* PrivateExponent;
	DWORD PrivateExponentLength;
	BYTE exponent[4];
} rdpRsaKey;

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
/* Settings */

#ifdef __GNUC__
#define ALIGN64 __attribute__((aligned(8)))
#else
#ifdef _WIN32
#define ALIGN64 __declspec(align(8))
#else
#define ALIGN64
#endif
#endif

/**
 * FreeRDP Settings Ids
 * This is generated with a script parsing the rdpSettings data structure
 */

#define FreeRDP_instance (0)
#define FreeRDP_ServerMode (16)
#define FreeRDP_ShareId (17)
#define FreeRDP_PduSource (18)
#define FreeRDP_ServerPort (19)
#define FreeRDP_ServerHostname (20)
#define FreeRDP_Username (21)
#define FreeRDP_Password (22)
#define FreeRDP_Domain (23)
#define FreeRDP_PasswordHash (24)
#define FreeRDP_WaitForOutputBufferFlush (25)
#define FreeRDP_AcceptedCert (27)
#define FreeRDP_AcceptedCertLength (28)
#define FreeRDP_UserSpecifiedServerName (29)
#define FreeRDP_ThreadingFlags (64)
#define FreeRDP_RdpVersion (128)
#define FreeRDP_DesktopWidth (129)
#define FreeRDP_DesktopHeight (130)
#define FreeRDP_ColorDepth (131)
#define FreeRDP_ConnectionType (132)
#define FreeRDP_ClientBuild (133)
#define FreeRDP_ClientHostname (134)
#define FreeRDP_ClientProductId (135)
#define FreeRDP_EarlyCapabilityFlags (136)
#define FreeRDP_NetworkAutoDetect (137)
#define FreeRDP_SupportAsymetricKeys (138)
#define FreeRDP_SupportErrorInfoPdu (139)
#define FreeRDP_SupportStatusInfoPdu (140)
#define FreeRDP_SupportMonitorLayoutPdu (141)
#define FreeRDP_SupportGraphicsPipeline (142)
#define FreeRDP_SupportDynamicTimeZone (143)
#define FreeRDP_SupportHeartbeatPdu (144)
#define FreeRDP_DesktopPhysicalWidth (145)
#define FreeRDP_DesktopPhysicalHeight (146)
#define FreeRDP_DesktopOrientation (147)
#define FreeRDP_DesktopScaleFactor (148)
#define FreeRDP_DeviceScaleFactor (149)
#define FreeRDP_UseRdpSecurityLayer (192)
#define FreeRDP_EncryptionMethods (193)
#define FreeRDP_ExtEncryptionMethods (194)
#define FreeRDP_EncryptionLevel (195)
#define FreeRDP_ServerRandom (196)
#define FreeRDP_ServerRandomLength (197)
#define FreeRDP_ServerCertificate (198)
#define FreeRDP_ServerCertificateLength (199)
#define FreeRDP_ClientRandom (200)
#define FreeRDP_ClientRandomLength (201)
#define FreeRDP_ServerLicenseRequired (202)
#define FreeRDP_ServerLicenseCompanyName (203)
#define FreeRDP_ServerLicenseProductVersion (204)
#define FreeRDP_ServerLicenseProductName (205)
#define FreeRDP_ServerLicenseProductIssuers (206)
#define FreeRDP_ServerLicenseProductIssuersCount (207)
#define FreeRDP_ChannelCount (256)
#define FreeRDP_ChannelDefArraySize (257)
#define FreeRDP_ChannelDefArray (258)
#define FreeRDP_ClusterInfoFlags (320)
#define FreeRDP_RedirectedSessionId (321)
#define FreeRDP_ConsoleSession (322)
#define FreeRDP_MonitorCount (384)
#define FreeRDP_MonitorDefArraySize (385)
#define FreeRDP_MonitorDefArray (386)
#define FreeRDP_SpanMonitors (387)
#define FreeRDP_UseMultimon (388)
#define FreeRDP_ForceMultimon (389)
#define FreeRDP_DesktopPosX (390)
#define FreeRDP_DesktopPosY (391)
#define FreeRDP_ListMonitors (392)
#define FreeRDP_MonitorIds (393)
#define FreeRDP_NumMonitorIds (394)
#define FreeRDP_MonitorLocalShiftX (395)
#define FreeRDP_MonitorLocalShiftY (396)
#define FreeRDP_HasMonitorAttributes (397)
#define FreeRDP_MonitorFlags (398)
#define FreeRDP_MonitorAttributeFlags (399)
#define FreeRDP_MultitransportFlags (512)
#define FreeRDP_SupportMultitransport (513)
#define FreeRDP_AlternateShell (640)
#define FreeRDP_ShellWorkingDirectory (641)
#define FreeRDP_AutoLogonEnabled (704)
#define FreeRDP_CompressionEnabled (705)
#define FreeRDP_DisableCtrlAltDel (706)
#define FreeRDP_EnableWindowsKey (707)
#define FreeRDP_MaximizeShell (708)
#define FreeRDP_LogonNotify (709)
#define FreeRDP_LogonErrors (710)
#define FreeRDP_MouseAttached (711)
#define FreeRDP_MouseHasWheel (712)
#define FreeRDP_RemoteConsoleAudio (713)
#define FreeRDP_AudioPlayback (714)
#define FreeRDP_AudioCapture (715)
#define FreeRDP_VideoDisable (716)
#define FreeRDP_PasswordIsSmartcardPin (717)
#define FreeRDP_UsingSavedCredentials (718)
#define FreeRDP_ForceEncryptedCsPdu (719)
#define FreeRDP_HiDefRemoteApp (720)
#define FreeRDP_CompressionLevel (721)
#define FreeRDP_IPv6Enabled (768)
#define FreeRDP_ClientAddress (769)
#define FreeRDP_ClientDir (770)
#define FreeRDP_ClientSessionId (771)
#define FreeRDP_AutoReconnectionEnabled (832)
#define FreeRDP_AutoReconnectMaxRetries (833)
#define FreeRDP_ClientAutoReconnectCookie (834)
#define FreeRDP_ServerAutoReconnectCookie (835)
#define FreeRDP_PrintReconnectCookie (836)
#define FreeRDP_ClientTimeZone (896)
#define FreeRDP_DynamicDSTTimeZoneKeyName (897)
#define FreeRDP_DynamicDaylightTimeDisabled (898)
#define FreeRDP_PerformanceFlags (960)
#define FreeRDP_AllowFontSmoothing (961)
#define FreeRDP_DisableWallpaper (962)
#define FreeRDP_DisableFullWindowDrag (963)
#define FreeRDP_DisableMenuAnims (964)
#define FreeRDP_DisableThemes (965)
#define FreeRDP_DisableCursorShadow (966)
#define FreeRDP_DisableCursorBlinking (967)
#define FreeRDP_AllowDesktopComposition (968)
#define FreeRDP_RemoteAssistanceMode (1024)
#define FreeRDP_RemoteAssistanceSessionId (1025)
#define FreeRDP_RemoteAssistancePassStub (1026)
#define FreeRDP_RemoteAssistancePassword (1027)
#define FreeRDP_RemoteAssistanceRCTicket (1028)
#define FreeRDP_EncomspVirtualChannel (1029)
#define FreeRDP_RemdeskVirtualChannel (1030)
#define FreeRDP_LyncRdpMode (1031)
#define FreeRDP_RemoteAssistanceRequestControl (1032)
#define FreeRDP_TlsSecurity (1088)
#define FreeRDP_NlaSecurity (1089)
#define FreeRDP_RdpSecurity (1090)
#define FreeRDP_ExtSecurity (1091)
#define FreeRDP_Authentication (1092)
#define FreeRDP_RequestedProtocols (1093)
#define FreeRDP_SelectedProtocol (1094)
#define FreeRDP_NegotiationFlags (1095)
#define FreeRDP_NegotiateSecurityLayer (1096)
#define FreeRDP_RestrictedAdminModeRequired (1097)
#define FreeRDP_AuthenticationServiceClass (1098)
#define FreeRDP_DisableCredentialsDelegation (1099)
#define FreeRDP_AuthenticationLevel (1100)
#define FreeRDP_AllowedTlsCiphers (1101)
#define FreeRDP_VmConnectMode (1102)
#define FreeRDP_NtlmSamFile (1103)
#define FreeRDP_FIPSMode (1104)
#define FreeRDP_TlsSecLevel (1105)
#define FreeRDP_SspiModule (1106)
#define FreeRDP_TLSMinVersion (1107)
#define FreeRDP_TLSMaxVersion (1108)
#define FreeRDP_TlsSecretsFile (1109)
#define FreeRDP_AuthenticationPackageList (1110)
#define FreeRDP_MstscCookieMode (1152)
#define FreeRDP_CookieMaxLength (1153)
#define FreeRDP_PreconnectionId (1154)
#define FreeRDP_PreconnectionBlob (1155)
#define FreeRDP_SendPreconnectionPdu (1156)
#define FreeRDP_RedirectionFlags (1216)
#define FreeRDP_TargetNetAddress (1217)
#define FreeRDP_LoadBalanceInfo (1218)
#define FreeRDP_LoadBalanceInfoLength (1219)
#define FreeRDP_RedirectionUsername (1220)
#define FreeRDP_RedirectionDomain (1221)
#define FreeRDP_RedirectionPassword (1222)
#define FreeRDP_RedirectionPasswordLength (1223)
#define FreeRDP_RedirectionTargetFQDN (1224)
#define FreeRDP_RedirectionTargetNetBiosName (1225)
#define FreeRDP_RedirectionTsvUrl (1226)
#define FreeRDP_RedirectionTsvUrlLength (1227)
#define FreeRDP_TargetNetAddressCount (1228)
#define FreeRDP_TargetNetAddresses (1229)
#define FreeRDP_TargetNetPorts (1230)
#define FreeRDP_RedirectionAcceptedCert (1231)
#define FreeRDP_RedirectionAcceptedCertLength (1232)
#define FreeRDP_RedirectionPreferType (1233)
#define FreeRDP_Password51 (1280)
#define FreeRDP_Password51Length (1281)
#define FreeRDP_SmartcardLogon (1282)
#define FreeRDP_PromptForCredentials (1283)
#define FreeRDP_SmartcardCertificate (1285)
#define FreeRDP_SmartcardPrivateKey (1286)
#define FreeRDP_SmartcardEmulation (1288)
#define FreeRDP_Pkcs11Module (1289)
#define FreeRDP_PkinitAnchors (1290)
#define FreeRDP_KeySpec (1291)
#define FreeRDP_CardName (1292)
#define FreeRDP_ReaderName (1293)
#define FreeRDP_ContainerName (1294)
#define FreeRDP_CspName (1295)
#define FreeRDP_KerberosKdcUrl (1344)
#define FreeRDP_KerberosRealm (1345)
#define FreeRDP_KerberosStartTime (1346)
#define FreeRDP_KerberosLifeTime (1347)
#define FreeRDP_KerberosRenewableLifeTime (1348)
#define FreeRDP_KerberosCache (1349)
#define FreeRDP_KerberosArmor (1350)
#define FreeRDP_KerberosKeytab (1351)
#define FreeRDP_KerberosRdgIsProxy (1352)
#define FreeRDP_IgnoreCertificate (1408)
#define FreeRDP_CertificateName (1409)
#define FreeRDP_CertificateFile (1410)
#define FreeRDP_PrivateKeyFile (1411)
#define FreeRDP_RdpServerRsaKey (1413)
#define FreeRDP_RdpServerCertificate (1414)
#define FreeRDP_ExternalCertificateManagement (1415)
#define FreeRDP_CertificateContent (1416)
#define FreeRDP_PrivateKeyContent (1417)
#define FreeRDP_AutoAcceptCertificate (1419)
#define FreeRDP_AutoDenyCertificate (1420)
#define FreeRDP_CertificateAcceptedFingerprints (1421)
#define FreeRDP_CertificateUseKnownHosts (1422)
#define FreeRDP_CertificateCallbackPreferPEM (1423)
#define FreeRDP_Workarea (1536)
#define FreeRDP_Fullscreen (1537)
#define FreeRDP_PercentScreen (1538)
#define FreeRDP_GrabKeyboard (1539)
#define FreeRDP_Decorations (1540)
#define FreeRDP_MouseMotion (1541)
#define FreeRDP_WindowTitle (1542)
#define FreeRDP_ParentWindowId (1543)
#define FreeRDP_AsyncUpdate (1545)
#define FreeRDP_AsyncChannels (1546)
#define FreeRDP_ToggleFullscreen (1548)
#define FreeRDP_WmClass (1549)
#define FreeRDP_EmbeddedWindow (1550)
#define FreeRDP_SmartSizing (1551)
#define FreeRDP_XPan (1552)
#define FreeRDP_YPan (1553)
#define FreeRDP_SmartSizingWidth (1554)
#define FreeRDP_SmartSizingHeight (1555)
#define FreeRDP_PercentScreenUseWidth (1556)
#define FreeRDP_PercentScreenUseHeight (1557)
#define FreeRDP_DynamicResolutionUpdate (1558)
#define FreeRDP_GrabMouse (1559)
#define FreeRDP_SoftwareGdi (1601)
#define FreeRDP_LocalConnection (1602)
#define FreeRDP_AuthenticationOnly (1603)
#define FreeRDP_CredentialsFromStdin (1604)
#define FreeRDP_UnmapButtons (1605)
#define FreeRDP_OldLicenseBehaviour (1606)
#define FreeRDP_MouseUseRelativeMove (1607)
#define FreeRDP_ComputerName (1664)
#define FreeRDP_ConnectionFile (1728)
#define FreeRDP_AssistanceFile (1729)
#define FreeRDP_HomePath (1792)
#define FreeRDP_ConfigPath (1793)
#define FreeRDP_CurrentPath (1794)
#define FreeRDP_DumpRemoteFx (1856)
#define FreeRDP_PlayRemoteFx (1857)
#define FreeRDP_DumpRemoteFxFile (1858)
#define FreeRDP_PlayRemoteFxFile (1859)
#define FreeRDP_TransportDump (1860)
#define FreeRDP_TransportDumpFile (1861)
#define FreeRDP_TransportDumpReplay (1862)
#define FreeRDP_DeactivateClientDecoding (1863)
#define FreeRDP_GatewayUsageMethod (1984)
#define FreeRDP_GatewayPort (1985)
#define FreeRDP_GatewayHostname (1986)
#define FreeRDP_GatewayUsername (1987)
#define FreeRDP_GatewayPassword (1988)
#define FreeRDP_GatewayDomain (1989)
#define FreeRDP_GatewayCredentialsSource (1990)
#define FreeRDP_GatewayUseSameCredentials (1991)
#define FreeRDP_GatewayEnabled (1992)
#define FreeRDP_GatewayBypassLocal (1993)
#define FreeRDP_GatewayRpcTransport (1994)
#define FreeRDP_GatewayHttpTransport (1995)
#define FreeRDP_GatewayUdpTransport (1996)
#define FreeRDP_GatewayAccessToken (1997)
#define FreeRDP_GatewayAcceptedCert (1998)
#define FreeRDP_GatewayAcceptedCertLength (1999)
#define FreeRDP_GatewayHttpUseWebsockets (2000)
#define FreeRDP_GatewayHttpExtAuthSspiNtlm (2001)
#define FreeRDP_ProxyType (2015)
#define FreeRDP_ProxyHostname (2016)
#define FreeRDP_ProxyPort (2017)
#define FreeRDP_ProxyUsername (2018)
#define FreeRDP_ProxyPassword (2019)
#define FreeRDP_RemoteApplicationMode (2112)
#define FreeRDP_RemoteApplicationName (2113)
#define FreeRDP_RemoteApplicationIcon (2114)
#define FreeRDP_RemoteApplicationProgram (2115)
#define FreeRDP_RemoteApplicationFile (2116)
#define FreeRDP_RemoteApplicationGuid (2117)
#define FreeRDP_RemoteApplicationCmdLine (2118)
#define FreeRDP_RemoteApplicationExpandCmdLine (2119)
#define FreeRDP_RemoteApplicationExpandWorkingDir (2120)
#define FreeRDP_DisableRemoteAppCapsCheck (2121)
#define FreeRDP_RemoteAppNumIconCaches (2122)
#define FreeRDP_RemoteAppNumIconCacheEntries (2123)
#define FreeRDP_RemoteAppLanguageBarSupported (2124)
#define FreeRDP_RemoteWndSupportLevel (2125)
#define FreeRDP_RemoteApplicationSupportLevel (2126)
#define FreeRDP_RemoteApplicationSupportMask (2127)
#define FreeRDP_RemoteApplicationWorkingDir (2128)
#define FreeRDP_ReceivedCapabilities (2240)
#define FreeRDP_ReceivedCapabilitiesSize (2241)
#define FreeRDP_ReceivedCapabilityData (2242)
#define FreeRDP_ReceivedCapabilityDataSizes (2243)
#define FreeRDP_OsMajorType (2304)
#define FreeRDP_OsMinorType (2305)
#define FreeRDP_RefreshRect (2306)
#define FreeRDP_SuppressOutput (2307)
#define FreeRDP_FastPathOutput (2308)
#define FreeRDP_SaltedChecksum (2309)
#define FreeRDP_LongCredentialsSupported (2310)
#define FreeRDP_NoBitmapCompressionHeader (2311)
#define FreeRDP_BitmapCompressionDisabled (2312)
#define FreeRDP_CapsProtocolVersion (2313)
#define FreeRDP_CapsGeneralCompressionTypes (2314)
#define FreeRDP_CapsUpdateCapabilityFlag (2315)
#define FreeRDP_CapsRemoteUnshareFlag (2316)
#define FreeRDP_CapsGeneralCompressionLevel (2317)
#define FreeRDP_DesktopResize (2368)
#define FreeRDP_DrawAllowDynamicColorFidelity (2369)
#define FreeRDP_DrawAllowColorSubsampling (2370)
#define FreeRDP_DrawAllowSkipAlpha (2371)
#define FreeRDP_OrderSupport (2432)
#define FreeRDP_BitmapCacheV3Enabled (2433)
#define FreeRDP_AltSecFrameMarkerSupport (2434)
#define FreeRDP_AllowUnanouncedOrdersFromServer (2435)
#define FreeRDP_OrderSupportFlags (2436)
#define FreeRDP_OrderSupportFlagsEx (2437)
#define FreeRDP_TerminalDescriptor (2438)
#define FreeRDP_TextANSICodePage (2439)
#define FreeRDP_BitmapCacheEnabled (2497)
#define FreeRDP_BitmapCacheVersion (2498)
#define FreeRDP_AllowCacheWaitingList (2499)
#define FreeRDP_BitmapCachePersistEnabled (2500)
#define FreeRDP_BitmapCacheV2NumCells (2501)
#define FreeRDP_BitmapCacheV2CellInfo (2502)
#define FreeRDP_BitmapCachePersistFile (2503)
#define FreeRDP_ColorPointerCacheSize (2560)
#define FreeRDP_PointerCacheSize (2561)
#define FreeRDP_KeyboardRemappingList (2622)
#define FreeRDP_KeyboardCodePage (2623)
#define FreeRDP_KeyboardLayout (2624)
#define FreeRDP_KeyboardType (2625)
#define FreeRDP_KeyboardSubType (2626)
#define FreeRDP_KeyboardFunctionKey (2627)
#define FreeRDP_ImeFileName (2628)
#define FreeRDP_UnicodeInput (2629)
#define FreeRDP_FastPathInput (2630)
#define FreeRDP_MultiTouchInput (2631)
#define FreeRDP_MultiTouchGestures (2632)
#define FreeRDP_KeyboardHook (2633)
#define FreeRDP_HasHorizontalWheel (2634)
#define FreeRDP_HasExtendedMouseEvent (2635)
#define FreeRDP_SuspendInput (2636)
#define FreeRDP_BrushSupportLevel (2688)
#define FreeRDP_GlyphSupportLevel (2752)
#define FreeRDP_GlyphCache (2753)
#define FreeRDP_FragCache (2754)
#define FreeRDP_OffscreenSupportLevel (2816)
#define FreeRDP_OffscreenCacheSize (2817)
#define FreeRDP_OffscreenCacheEntries (2818)
#define FreeRDP_VirtualChannelCompressionFlags (2880)
#define FreeRDP_VirtualChannelChunkSize (2881)
#define FreeRDP_SoundBeepsEnabled (2944)
#define FreeRDP_MultifragMaxRequestSize (3328)
#define FreeRDP_LargePointerFlag (3392)
#define FreeRDP_CompDeskSupportLevel (3456)
#define FreeRDP_SurfaceCommandsEnabled (3520)
#define FreeRDP_FrameMarkerCommandEnabled (3521)
#define FreeRDP_SurfaceFrameMarkerEnabled (3522)
#define FreeRDP_RemoteFxOnly (3648)
#define FreeRDP_RemoteFxCodec (3649)
#define FreeRDP_RemoteFxCodecId (3650)
#define FreeRDP_RemoteFxCodecMode (3651)
#define FreeRDP_RemoteFxImageCodec (3652)
#define FreeRDP_RemoteFxCaptureFlags (3653)
#define FreeRDP_NSCodec (3712)
#define FreeRDP_NSCodecId (3713)
#define FreeRDP_FrameAcknowledge (3714)
#define FreeRDP_NSCodecColorLossLevel (3715)
#define FreeRDP_NSCodecAllowSubsampling (3716)
#define FreeRDP_NSCodecAllowDynamicColorFidelity (3717)
#define FreeRDP_JpegCodec (3776)
#define FreeRDP_JpegCodecId (3777)
#define FreeRDP_JpegQuality (3778)
#define FreeRDP_GfxThinClient (3840)
#define FreeRDP_GfxSmallCache (3841)
#define FreeRDP_GfxProgressive (3842)
#define FreeRDP_GfxProgressiveV2 (3843)
#define FreeRDP_GfxH264 (3844)
#define FreeRDP_GfxAVC444 (3845)
#define FreeRDP_GfxSendQoeAck (3846)
#define FreeRDP_GfxAVC444v2 (3847)
#define FreeRDP_GfxCapsFilter (3848)
#define FreeRDP_GfxPlanar (3849)
#define FreeRDP_BitmapCacheV3CodecId (3904)
#define FreeRDP_DrawNineGridEnabled (3968)
#define FreeRDP_DrawNineGridCacheSize (3969)
#define FreeRDP_DrawNineGridCacheEntries (3970)
#define FreeRDP_DrawGdiPlusEnabled (4032)
#define FreeRDP_DrawGdiPlusCacheEnabled (4033)
#define FreeRDP_DeviceRedirection (4160)
#define FreeRDP_DeviceCount (4161)
#define FreeRDP_DeviceArraySize (4162)
#define FreeRDP_DeviceArray (4163)
#define FreeRDP_RedirectDrives (4288)
#define FreeRDP_RedirectHomeDrive (4289)
#define FreeRDP_DrivesToRedirect (4290)
#define FreeRDP_RedirectSmartCards (4416)
#define FreeRDP_RedirectPrinters (4544)
#define FreeRDP_RedirectSerialPorts (4672)
#define FreeRDP_RedirectParallelPorts (4673)
#define FreeRDP_PreferIPv6OverIPv4 (4674)
#define FreeRDP_RedirectClipboard (4800)
#define FreeRDP_StaticChannelCount (4928)
#define FreeRDP_StaticChannelArraySize (4929)
#define FreeRDP_StaticChannelArray (4930)
#define FreeRDP_DynamicChannelCount (5056)
#define FreeRDP_DynamicChannelArraySize (5057)
#define FreeRDP_DynamicChannelArray (5058)
#define FreeRDP_SupportDynamicChannels (5059)
#define FreeRDP_SupportEchoChannel (5184)
#define FreeRDP_SupportDisplayControl (5185)
#define FreeRDP_SupportGeometryTracking (5186)
#define FreeRDP_SupportSSHAgentChannel (5187)
#define FreeRDP_SupportVideoOptimized (5188)
#define FreeRDP_RDP2TCPArgs (5189)
#define FreeRDP_TcpKeepAlive (5190)
#define FreeRDP_TcpKeepAliveRetries (5191)
#define FreeRDP_TcpKeepAliveDelay (5192)
#define FreeRDP_TcpKeepAliveInterval (5193)
#define FreeRDP_TcpAckTimeout (5194)
#define FreeRDP_ActionScript (5195)
#define FreeRDP_Floatbar (5196)
#define FreeRDP_TcpConnectTimeout (5197)

/**
 * FreeRDP Settings Data Structure
 */

#define FreeRDP_Settings_StableAPI_MAX 5312
struct rdp_settings
{
	/**
	 * WARNING: this data structure is carefully padded for ABI stability!
	 * Keeping this area clean is particularly challenging, so unless you are
	 * a trusted developer you should NOT take the liberty of adding your own
	 * options straight into the ABI stable zone. Instead, append them to the
	 * very end of this data structure, in the zone marked as ABI unstable.
	 */

	ALIGN64 void* instance;    /* 0 */
	UINT64 padding001[16 - 1]; /* 1 */

	/* Core Parameters */
	ALIGN64 BOOL ServerMode;               /* 16 */
	ALIGN64 UINT32 ShareId;                /* 17 */
	ALIGN64 UINT32 PduSource;              /* 18 */
	ALIGN64 UINT32 ServerPort;             /* 19 */
	ALIGN64 char* ServerHostname;          /* 20 */
	ALIGN64 char* Username;                /* 21 */
	ALIGN64 char* Password;                /* 22 */
	ALIGN64 char* Domain;                  /* 23 */
	ALIGN64 char* PasswordHash;            /* 24 */
	ALIGN64 BOOL WaitForOutputBufferFlush; /* 25 */
	UINT64 padding26[27 - 26];             /* 26 */
	ALIGN64 char* AcceptedCert;            /* 27 */
	ALIGN64 UINT32 AcceptedCertLength;     /* 28 */
	ALIGN64 char* UserSpecifiedServerName; /* 29 */
	UINT64 padding0064[64 - 30];           /* 30 */
	/* resource management related options */
	ALIGN64 UINT32 ThreadingFlags; /* 64 */

	UINT64 padding0128[128 - 65]; /* 65 */

	/**
	 * GCC User Data Blocks
	 */

	/* Client/Server Core Data */
	ALIGN64 UINT32 RdpVersion;            /* 128 */
	ALIGN64 UINT32 DesktopWidth;          /* 129 */
	ALIGN64 UINT32 DesktopHeight;         /* 130 */
	ALIGN64 UINT32 ColorDepth;            /* 131 */
	ALIGN64 UINT32 ConnectionType;        /* 132 */
	ALIGN64 UINT32 ClientBuild;           /* 133 */
	ALIGN64 char* ClientHostname;         /* 134 */
	ALIGN64 char* ClientProductId;        /* 135 */
	ALIGN64 UINT32 EarlyCapabilityFlags;  /* 136 */
	ALIGN64 BOOL NetworkAutoDetect;       /* 137 */
	ALIGN64 BOOL SupportAsymetricKeys;    /* 138 */
	ALIGN64 BOOL SupportErrorInfoPdu;     /* 139 */
	ALIGN64 BOOL SupportStatusInfoPdu;    /* 140 */
	ALIGN64 BOOL SupportMonitorLayoutPdu; /* 141 */
	ALIGN64 BOOL SupportGraphicsPipeline; /* 142 */
	ALIGN64 BOOL SupportDynamicTimeZone;  /* 143 */
	ALIGN64 BOOL SupportHeartbeatPdu;     /* 144 */
	ALIGN64 UINT32 DesktopPhysicalWidth;  /* 145 */
	ALIGN64 UINT32 DesktopPhysicalHeight; /* 146 */
	ALIGN64 UINT16 DesktopOrientation;    /* 147 */
	ALIGN64 UINT32 DesktopScaleFactor;    /* 148 */
	ALIGN64 UINT32 DeviceScaleFactor;     /* 149 */
	UINT64 padding0192[192 - 150];        /* 150 */

	/* Client/Server Security Data */
	ALIGN64 BOOL UseRdpSecurityLayer;                /* 192 */
	ALIGN64 UINT32 EncryptionMethods;                /* 193 */
	ALIGN64 UINT32 ExtEncryptionMethods;             /* 194 */
	ALIGN64 UINT32 EncryptionLevel;                  /* 195 */
	ALIGN64 BYTE* ServerRandom;                      /* 196 */
	ALIGN64 UINT32 ServerRandomLength;               /* 197 */
	ALIGN64 BYTE* ServerCertificate;                 /* 198 */
	ALIGN64 UINT32 ServerCertificateLength;          /* 199 */
	ALIGN64 BYTE* ClientRandom;                      /* 200 */
	ALIGN64 UINT32 ClientRandomLength;               /* 201 */
	ALIGN64 BOOL ServerLicenseRequired;              /* 202 */
	ALIGN64 char* ServerLicenseCompanyName;          /* 203 */
	ALIGN64 UINT32 ServerLicenseProductVersion;      /* 204 */
	ALIGN64 char* ServerLicenseProductName;          /* 205 */
	ALIGN64 char** ServerLicenseProductIssuers;      /* 206 */
	ALIGN64 UINT32 ServerLicenseProductIssuersCount; /* 207 */
	UINT64 padding0256[256 - 208];                   /* 208 */

	/* Client Network Data */
	ALIGN64 UINT32 ChannelCount;          /* 256 */
	ALIGN64 UINT32 ChannelDefArraySize;   /* 257 */
	ALIGN64 CHANNEL_DEF* ChannelDefArray; /* 258 */
	UINT64 padding0320[320 - 259];        /* 259 */

	/* Client Cluster Data */
	ALIGN64 UINT32 ClusterInfoFlags;    /* 320 */
	ALIGN64 UINT32 RedirectedSessionId; /* 321 */
	ALIGN64 BOOL ConsoleSession;        /* 322 */
	UINT64 padding0384[384 - 323];      /* 323 */

	/* Client Monitor Data */
	ALIGN64 UINT32 MonitorCount;          /*    384 */
	ALIGN64 UINT32 MonitorDefArraySize;   /*    385 */
	ALIGN64 rdpMonitor* MonitorDefArray;  /*    386 */
	ALIGN64 BOOL SpanMonitors;            /*    387 */
	ALIGN64 BOOL UseMultimon;             /*    388 */
	ALIGN64 BOOL ForceMultimon;           /*    389 */
	ALIGN64 UINT32 DesktopPosX;           /*    390 */
	ALIGN64 UINT32 DesktopPosY;           /*    391 */
	ALIGN64 BOOL ListMonitors;            /*    392 */
	ALIGN64 UINT32* MonitorIds;           /*    393 */
	ALIGN64 UINT32 NumMonitorIds;         /*    394 */
	ALIGN64 UINT32 MonitorLocalShiftX;    /*395 */
	ALIGN64 UINT32 MonitorLocalShiftY;    /*    396 */
	ALIGN64 BOOL HasMonitorAttributes;    /*    397 */
	ALIGN64 UINT32 MonitorFlags;          /* 398 */
	ALIGN64 UINT32 MonitorAttributeFlags; /* 399 */
	UINT64 padding0448[448 - 400];        /* 400 */

	/* Client Message Channel Data */
	UINT64 padding0512[512 - 448]; /* 448 */

	/* Client Multitransport Channel Data */
	ALIGN64 UINT32 MultitransportFlags; /* 512 */
	ALIGN64 BOOL SupportMultitransport; /* 513 */
	UINT64 padding0576[576 - 514];      /* 514 */
	UINT64 padding0640[640 - 576];      /* 576 */

	/*
	 * Client Info
	 */

	/* Client Info (Shell) */
	ALIGN64 char* AlternateShell;        /* 640 */
	ALIGN64 char* ShellWorkingDirectory; /* 641 */
	UINT64 padding0704[704 - 642];       /* 642 */

	/* Client Info Flags */
	ALIGN64 BOOL AutoLogonEnabled;       /* 704 */
	ALIGN64 BOOL CompressionEnabled;     /* 705 */
	ALIGN64 BOOL DisableCtrlAltDel;      /* 706 */
	ALIGN64 BOOL EnableWindowsKey;       /* 707 */
	ALIGN64 BOOL MaximizeShell;          /* 708 */
	ALIGN64 BOOL LogonNotify;            /* 709 */
	ALIGN64 BOOL LogonErrors;            /* 710 */
	ALIGN64 BOOL MouseAttached;          /* 711 */
	ALIGN64 BOOL MouseHasWheel;          /* 712 */
	ALIGN64 BOOL RemoteConsoleAudio;     /* 713 */
	ALIGN64 BOOL AudioPlayback;          /* 714 */
	ALIGN64 BOOL AudioCapture;           /* 715 */
	ALIGN64 BOOL VideoDisable;           /* 716 */
	ALIGN64 BOOL PasswordIsSmartcardPin; /* 717 */
	ALIGN64 BOOL UsingSavedCredentials;  /* 718 */
	ALIGN64 BOOL ForceEncryptedCsPdu;    /* 719 */
	ALIGN64 BOOL HiDefRemoteApp;         /* 720 */
	ALIGN64 UINT32 CompressionLevel;     /* 721 */
	UINT64 padding0768[768 - 722];       /* 722 */

	/* Client Info (Extra) */
	ALIGN64 BOOL IPv6Enabled;       /* 768 */
	ALIGN64 char* ClientAddress;    /* 769 */
	ALIGN64 char* ClientDir;        /* 770 */
	ALIGN64 UINT32 ClientSessionId; /*  */
	UINT64 padding0832[832 - 772];  /* 772 */

	/* Client Info (Auto Reconnection) */
	ALIGN64 BOOL AutoReconnectionEnabled;                     /* 832 */
	ALIGN64 UINT32 AutoReconnectMaxRetries;                   /* 833 */
	ALIGN64 ARC_CS_PRIVATE_PACKET* ClientAutoReconnectCookie; /* 834 */
	ALIGN64 ARC_SC_PRIVATE_PACKET* ServerAutoReconnectCookie; /* 835 */
	ALIGN64 BOOL PrintReconnectCookie;                        /* 836 */
	UINT64 padding0896[896 - 837];                            /* 837 */

	/* Client Info (Time Zone) */
	ALIGN64 TIME_ZONE_INFORMATION* ClientTimeZone; /* 896 */
	ALIGN64 char* DynamicDSTTimeZoneKeyName;       /* 897 */
	ALIGN64 BOOL DynamicDaylightTimeDisabled;      /* 898 */
	UINT64 padding0960[960 - 899];                 /* 899 */

	/* Client Info (Performance Flags) */
	ALIGN64 UINT32 PerformanceFlags;      /* 960 */
	ALIGN64 BOOL AllowFontSmoothing;      /* 961 */
	ALIGN64 BOOL DisableWallpaper;        /* 962 */
	ALIGN64 BOOL DisableFullWindowDrag;   /* 963 */
	ALIGN64 BOOL DisableMenuAnims;        /* 964 */
	ALIGN64 BOOL DisableThemes;           /* 965 */
	ALIGN64 BOOL DisableCursorShadow;     /* 966 */
	ALIGN64 BOOL DisableCursorBlinking;   /* 967 */
	ALIGN64 BOOL AllowDesktopComposition; /* 968 */
	UINT64 padding1024[1024 - 969];       /* 969 */

	/* Remote Assistance */
	ALIGN64 BOOL RemoteAssistanceMode;           /* 1024 */
	ALIGN64 char* RemoteAssistanceSessionId;     /* 1025 */
	ALIGN64 char* RemoteAssistancePassStub;      /* 1026 */
	ALIGN64 char* RemoteAssistancePassword;      /* 1027 */
	ALIGN64 char* RemoteAssistanceRCTicket;      /* 1028 */
	ALIGN64 BOOL EncomspVirtualChannel;          /* 1029 */
	ALIGN64 BOOL RemdeskVirtualChannel;          /* 1030 */
	ALIGN64 BOOL LyncRdpMode;                    /* 1031 */
	ALIGN64 BOOL RemoteAssistanceRequestControl; /* 1032 */
	UINT64 padding1088[1088 - 1033];             /* 1033 */

	/**
	 * X.224 Connection Request/Confirm
	 */

	/* Protocol Security */
	ALIGN64 BOOL TlsSecurity;                  /* 1088 */
	ALIGN64 BOOL NlaSecurity;                  /* 1089 */
	ALIGN64 BOOL RdpSecurity;                  /* 1090 */
	ALIGN64 BOOL ExtSecurity;                  /* 1091 */
	ALIGN64 BOOL Authentication;               /* 1092 */
	ALIGN64 UINT32 RequestedProtocols;         /* 1093 */
	ALIGN64 UINT32 SelectedProtocol;           /* 1094 */
	ALIGN64 UINT32 NegotiationFlags;           /* 1095 */
	ALIGN64 BOOL NegotiateSecurityLayer;       /* 1096 */
	ALIGN64 BOOL RestrictedAdminModeRequired;  /* 1097 */
	ALIGN64 char* AuthenticationServiceClass;  /* 1098 */
	ALIGN64 BOOL DisableCredentialsDelegation; /* 1099 */
	ALIGN64 UINT32 AuthenticationLevel;        /* 1100 */
	ALIGN64 char* AllowedTlsCiphers;           /* 1101 */
	ALIGN64 BOOL VmConnectMode;                /* 1102 */
	ALIGN64 char* NtlmSamFile;                 /* 1103 */
	ALIGN64 BOOL FIPSMode;                     /* 1104 */
	ALIGN64 UINT32 TlsSecLevel;                /* 1105 */
	ALIGN64 char* SspiModule;                  /* 1106 */
	ALIGN64 UINT16 TLSMinVersion;              /* 1107 */
	ALIGN64 UINT16 TLSMaxVersion;              /* 1108 */
	ALIGN64 char* TlsSecretsFile;              /* 1109 */
	ALIGN64 char* AuthenticationPackageList;   /* 1110 */
	UINT64 padding1152[1152 - 1111];           /* 1111 */

	/* Connection Cookie */
	ALIGN64 BOOL MstscCookieMode;      /* 1152 */
	ALIGN64 UINT32 CookieMaxLength;    /* 1153 */
	ALIGN64 UINT32 PreconnectionId;    /* 1154 */
	ALIGN64 char* PreconnectionBlob;   /* 1155 */
	ALIGN64 BOOL SendPreconnectionPdu; /* 1156 */
	UINT64 padding1216[1216 - 1157];   /* 1157 */

	/* Server Redirection */
	ALIGN64 UINT32 RedirectionFlags;              /* 1216 */
	ALIGN64 char* TargetNetAddress;               /* 1217 */
	ALIGN64 BYTE* LoadBalanceInfo;                /* 1218 */
	ALIGN64 UINT32 LoadBalanceInfoLength;         /* 1219 */
	ALIGN64 char* RedirectionUsername;            /* 1220 */
	ALIGN64 char* RedirectionDomain;              /* 1221 */
	ALIGN64 BYTE* RedirectionPassword;            /* 1222 */
	ALIGN64 UINT32 RedirectionPasswordLength;     /* 1223 */
	ALIGN64 char* RedirectionTargetFQDN;          /* 1224 */
	ALIGN64 char* RedirectionTargetNetBiosName;   /* 1225 */
	ALIGN64 BYTE* RedirectionTsvUrl;              /* 1226 */
	ALIGN64 UINT32 RedirectionTsvUrlLength;       /* 1227 */
	ALIGN64 UINT32 TargetNetAddressCount;         /* 1228 */
	ALIGN64 char** TargetNetAddresses;            /* 1229 */
	ALIGN64 UINT32* TargetNetPorts;               /* 1230 */
	ALIGN64 char* RedirectionAcceptedCert;        /* 1231 */
	ALIGN64 UINT32 RedirectionAcceptedCertLength; /* 1232 */
	ALIGN64 UINT32 RedirectionPreferType;         /* 1233 */
	UINT64 padding1280[1280 - 1234];              /* 1234 */

	/**
	 * Security
	 */

	/* Credentials Cache */
	ALIGN64 BYTE* Password51;          /* 1280 */
	ALIGN64 UINT32 Password51Length;   /* 1281 */
	ALIGN64 BOOL SmartcardLogon;       /* 1282 */
	ALIGN64 BOOL PromptForCredentials; /* 1283 */
	UINT64 padding1284[1285 - 1284];   /* 1284 */

	/* Settings used for smartcard emulation */
	ALIGN64 char* SmartcardCertificate; /* 1285 */
	ALIGN64 char* SmartcardPrivateKey;  /* 1286 */
	UINT64 padding1287[1288 - 1287];    /* 1287 */
	ALIGN64 BOOL SmartcardEmulation;    /* 1288 */
	ALIGN64 char* Pkcs11Module;         /* 1289 */
	ALIGN64 char* PkinitAnchors;        /* 1290 */
	ALIGN64 UINT32 KeySpec;             /* 1291 */
	ALIGN64 char* CardName;             /* 1292 */
	ALIGN64 char* ReaderName;           /* 1293 */
	ALIGN64 char* ContainerName;        /* 1294 */
	ALIGN64 char* CspName;              /* 1295 */
	UINT64 padding1344[1344 - 1296];    /* 1296 */

	/* Kerberos Authentication */
	ALIGN64 char* KerberosKdcUrl;            /* 1344 */
	ALIGN64 char* KerberosRealm;             /* 1345 */
	ALIGN64 char* KerberosStartTime;         /* 1346 */
	ALIGN64 char* KerberosLifeTime;          /* 1347 */
	ALIGN64 char* KerberosRenewableLifeTime; /* 1348 */
	ALIGN64 char* KerberosCache;             /* 1349 */
	ALIGN64 char* KerberosArmor;             /* 1350 */
	ALIGN64 char* KerberosKeytab;            /* 1351 */
	ALIGN64 BOOL KerberosRdgIsProxy;         /* 1352 */
	UINT64 padding1408[1408 - 1353];         /* 1353 */

	/* Server Certificate */
	ALIGN64 BOOL IgnoreCertificate;                /* 1408 */
	ALIGN64 char* CertificateName;                 /* 1409 */
	ALIGN64 char* CertificateFile;                 /* 1410 */
	ALIGN64 char* PrivateKeyFile;                  /* 1411 */
	UINT64 padding1412[1413 - 1412];               /* 1412 */
	ALIGN64 rdpRsaKey* RdpServerRsaKey;            /* 1413 */
	ALIGN64 rdpCertificate* RdpServerCertificate;  /* 1414 */
	ALIGN64 BOOL ExternalCertificateManagement;    /* 1415 */
	ALIGN64 char* CertificateContent;              /* 1416 */
	ALIGN64 char* PrivateKeyContent;               /* 1417 */
	UINT64 padding1418[1419 - 1418];               /* 1418 */
	ALIGN64 BOOL AutoAcceptCertificate;            /* 1419 */
	ALIGN64 BOOL AutoDenyCertificate;              /* 1420 */
	ALIGN64 char* CertificateAcceptedFingerprints; /* 1421 */
	ALIGN64 BOOL CertificateUseKnownHosts;         /* 1422 */
	ALIGN64 BOOL CertificateCallbackPreferPEM;     /* 1423 */
	UINT64 padding1472[1472 - 1424];               /* 1424 */
	UINT64 padding1536[1536 - 1472];               /* 1472 */

	/**
	 * User Interface
	 */

	/* Window Settings */
	ALIGN64 BOOL Workarea;                /* 1536 */
	ALIGN64 BOOL Fullscreen;              /* 1537 */
	ALIGN64 UINT32 PercentScreen;         /* 1538 */
	ALIGN64 BOOL GrabKeyboard;            /* 1539 */
	ALIGN64 BOOL Decorations;             /* 1540 */
	ALIGN64 BOOL MouseMotion;             /* 1541 */
	ALIGN64 char* WindowTitle;            /* 1542 */
	ALIGN64 UINT64 ParentWindowId;        /* 1543 */
	UINT64 padding1544[1545 - 1544];      /* 1544 */
	ALIGN64 BOOL AsyncUpdate;             /* 1545 */
	ALIGN64 BOOL AsyncChannels;           /* 1546 */
	UINT64 padding1548[1548 - 1547];      /* 1547 */
	ALIGN64 BOOL ToggleFullscreen;        /* 1548 */
	ALIGN64 char* WmClass;                /* 1549 */
	ALIGN64 BOOL EmbeddedWindow;          /* 1550 */
	ALIGN64 BOOL SmartSizing;             /* 1551 */
	ALIGN64 INT32 XPan;                   /* 1552 */
	ALIGN64 INT32 YPan;                   /* 1553 */
	ALIGN64 UINT32 SmartSizingWidth;      /* 1554 */
	ALIGN64 UINT32 SmartSizingHeight;     /* 1555 */
	ALIGN64 BOOL PercentScreenUseWidth;   /* 1556 */
	ALIGN64 BOOL PercentScreenUseHeight;  /* 1557 */
	ALIGN64 BOOL DynamicResolutionUpdate; /* 1558 */
	ALIGN64 BOOL GrabMouse;               /* 1559 */
	UINT64 padding1601[1601 - 1560];      /* 1560 */

	/* Miscellaneous */
	ALIGN64 BOOL SoftwareGdi;          /* 1601 */
	ALIGN64 BOOL LocalConnection;      /* 1602 */
	ALIGN64 BOOL AuthenticationOnly;   /* 1603 */
	ALIGN64 BOOL CredentialsFromStdin; /* 1604 */
	ALIGN64 BOOL UnmapButtons;         /* 1605 */
	ALIGN64 BOOL OldLicenseBehaviour;  /* 1606 */
	ALIGN64 BOOL MouseUseRelativeMove; /* 1607 */
	UINT64 padding1664[1664 - 1608];   /* 1608 */

	/* Names */
	ALIGN64 char* ComputerName;      /* 1664 */
	UINT64 padding1728[1728 - 1665]; /* 1665 */

	/* Files */
	ALIGN64 char* ConnectionFile;    /* 1728 */
	ALIGN64 char* AssistanceFile;    /* 1729 */
	UINT64 padding1792[1792 - 1730]; /* 1730 */

	/* Paths */
	ALIGN64 char* HomePath;          /* 1792 */
	ALIGN64 char* ConfigPath;        /* 1793 */
	ALIGN64 char* CurrentPath;       /* 1794 */
	UINT64 padding1856[1856 - 1795]; /* 1795 */

	/* Recording */
	ALIGN64 BOOL DumpRemoteFx;             /* 1856 */
	ALIGN64 BOOL PlayRemoteFx;             /* 1857 */
	ALIGN64 char* DumpRemoteFxFile;        /* 1858 */
	ALIGN64 char* PlayRemoteFxFile;        /* 1859 */
	ALIGN64 BOOL TransportDump;            /* 1860 */
	ALIGN64 char* TransportDumpFile;       /* 1861 */
	ALIGN64 BOOL TransportDumpReplay;      /* 1862 */
	ALIGN64 BOOL DeactivateClientDecoding; /* 1863 */
	UINT64 padding1920[1920 - 1864];       /* 1864 */
	UINT64 padding1984[1984 - 1920];       /* 1920 */

	/**
	 * Gateway
	 */

	/* Gateway */
	ALIGN64 UINT32 GatewayUsageMethod;        /* 1984 */
	ALIGN64 UINT32 GatewayPort;               /* 1985 */
	ALIGN64 char* GatewayHostname;            /* 1986 */
	ALIGN64 char* GatewayUsername;            /* 1987 */
	ALIGN64 char* GatewayPassword;            /* 1988 */
	ALIGN64 char* GatewayDomain;              /* 1989 */
	ALIGN64 UINT32 GatewayCredentialsSource;  /* 1990 */
	ALIGN64 BOOL GatewayUseSameCredentials;   /* 1991 */
	ALIGN64 BOOL GatewayEnabled;              /* 1992 */
	ALIGN64 BOOL GatewayBypassLocal;          /* 1993 */
	ALIGN64 BOOL GatewayRpcTransport;         /* 1994 */
	ALIGN64 BOOL GatewayHttpTransport;        /* 1995 */
	ALIGN64 BOOL GatewayUdpTransport;         /* 1996 */
	ALIGN64 char* GatewayAccessToken;         /* 1997 */
	ALIGN64 char* GatewayAcceptedCert;        /* 1998 */
	ALIGN64 UINT32 GatewayAcceptedCertLength; /* 1999 */
	ALIGN64 BOOL GatewayHttpUseWebsockets;    /* 2000 */
	ALIGN64 BOOL GatewayHttpExtAuthSspiNtlm;  /* 2001 */
	UINT64 padding2015[2015 - 2002];          /* 2002 */

	/* Proxy */
	ALIGN64 UINT32 ProxyType;        /* 2015 */
	ALIGN64 char* ProxyHostname;     /* 2016 */
	ALIGN64 UINT16 ProxyPort;        /* 2017 */
	ALIGN64 char* ProxyUsername;     /* 2018 */
	ALIGN64 char* ProxyPassword;     /* 2019 */
	UINT64 padding2112[2112 - 2020]; /* 2020 */

	/**
	 * RemoteApp
	 */

	/* RemoteApp */
	ALIGN64 BOOL RemoteApplicationMode;               /* 2112 */
	ALIGN64 char* RemoteApplicationName;              /* 2113 */
	ALIGN64 char* RemoteApplicationIcon;              /* 2114 */
	ALIGN64 char* RemoteApplicationProgram;           /* 2115 */
	ALIGN64 char* RemoteApplicationFile;              /* 2116 */
	ALIGN64 char* RemoteApplicationGuid;              /* 2117 */
	ALIGN64 char* RemoteApplicationCmdLine;           /* 2118 */
	ALIGN64 UINT32 RemoteApplicationExpandCmdLine;    /* 2119 */
	ALIGN64 UINT32 RemoteApplicationExpandWorkingDir; /* 2120 */
	ALIGN64 BOOL DisableRemoteAppCapsCheck;           /* 2121 */
	ALIGN64 UINT32 RemoteAppNumIconCaches;            /* 2122 */
	ALIGN64 UINT32 RemoteAppNumIconCacheEntries;      /* 2123 */
	ALIGN64 BOOL RemoteAppLanguageBarSupported;       /* 2124 */
	ALIGN64 UINT32 RemoteWndSupportLevel;             /* 2125 */
	ALIGN64 UINT32 RemoteApplicationSupportLevel;     /* 2126 */
	ALIGN64 UINT32 RemoteApplicationSupportMask;      /* 2127 */
	ALIGN64 char* RemoteApplicationWorkingDir;        /* 2128 */
	UINT64 padding2176[2176 - 2129];                  /* 2129 */
	UINT64 padding2240[2240 - 2176];                  /* 2176 */

	/**
	 * Mandatory Capabilities
	 */

	/* Capabilities */
	ALIGN64 BYTE* ReceivedCapabilities;          /* 2240 */
	ALIGN64 UINT32 ReceivedCapabilitiesSize;     /* 2241 */
	ALIGN64 BYTE** ReceivedCapabilityData;       /* 2242 */
	ALIGN64 UINT32* ReceivedCapabilityDataSizes; /* 2243 */
	UINT64 padding2304[2304 - 2244];             /* 2244 */

	/* General Capabilities */
	ALIGN64 UINT32 OsMajorType;                 /* 2304 */
	ALIGN64 UINT32 OsMinorType;                 /* 2305 */
	ALIGN64 BOOL RefreshRect;                   /* 2306 */
	ALIGN64 BOOL SuppressOutput;                /* 2307 */
	ALIGN64 BOOL FastPathOutput;                /* 2308 */
	ALIGN64 BOOL SaltedChecksum;                /* 2309 */
	ALIGN64 BOOL LongCredentialsSupported;      /* 2310 */
	ALIGN64 BOOL NoBitmapCompressionHeader;     /* 2311 */
	ALIGN64 BOOL BitmapCompressionDisabled;     /* 2312 */
	ALIGN64 UINT16 CapsProtocolVersion;         /* 2313 */
	ALIGN64 UINT16 CapsGeneralCompressionTypes; /* 2314 */
	ALIGN64 UINT16 CapsUpdateCapabilityFlag;    /* 2315 */
	ALIGN64 UINT16 CapsRemoteUnshareFlag;       /* 2316 */
	ALIGN64 UINT16 CapsGeneralCompressionLevel; /* 2317 */
	UINT64 padding2368[2368 - 2318];            /* 2318 */

	/* Bitmap Capabilities */
	ALIGN64 BOOL DesktopResize;                 /* 2368 */
	ALIGN64 BOOL DrawAllowDynamicColorFidelity; /* 2369 */
	ALIGN64 BOOL DrawAllowColorSubsampling;     /* 2370 */
	ALIGN64 BOOL DrawAllowSkipAlpha;            /* 2371 */
	UINT64 padding2432[2432 - 2372];            /* 2372 */

	/* Order Capabilities */
	ALIGN64 BYTE* OrderSupport;                   /* 2432 */
	ALIGN64 BOOL BitmapCacheV3Enabled;            /* 2433 */
	ALIGN64 BOOL AltSecFrameMarkerSupport;        /* 2434 */
	ALIGN64 BOOL AllowUnanouncedOrdersFromServer; /* 2435 */
	ALIGN64 UINT16 OrderSupportFlags;             /* 2436 */
	ALIGN64 UINT16 OrderSupportFlagsEx;           /* 2437 */
	ALIGN64 char* TerminalDescriptor;             /* 2438 */
	ALIGN64 UINT16 TextANSICodePage;              /* 2439 */
	UINT64 padding2497[2497 - 2440];              /* 2440 */

	/* Bitmap Cache Capabilities */
	ALIGN64 BOOL BitmapCacheEnabled;                          /* 2497 */
	ALIGN64 UINT32 BitmapCacheVersion;                        /* 2498 */
	ALIGN64 BOOL AllowCacheWaitingList;                       /* 2499 */
	ALIGN64 BOOL BitmapCachePersistEnabled;                   /* 2500 */
	ALIGN64 UINT32 BitmapCacheV2NumCells;                     /* 2501 */
	ALIGN64 BITMAP_CACHE_V2_CELL_INFO* BitmapCacheV2CellInfo; /* 2502 */
	ALIGN64 char* BitmapCachePersistFile;                     /* 2503 */
	UINT64 padding2560[2560 - 2504];                          /* 2504 */

	/* Pointer Capabilities */
	ALIGN64 UINT32 ColorPointerCacheSize; /* 2560 */
	ALIGN64 UINT32 PointerCacheSize;      /* 2561 */
	UINT64 padding2624[2622 - 2562];      /* 2562 */

	/* Input Capabilities */
	ALIGN64 char* KeyboardRemappingList; /* 2622 */
	ALIGN64 UINT32 KeyboardCodePage;     /* 2623 */
	ALIGN64 UINT32 KeyboardLayout;       /* 2624 */
	ALIGN64 UINT32 KeyboardType;         /* 2625 */
	ALIGN64 UINT32 KeyboardSubType;      /* 2626 */
	ALIGN64 UINT32 KeyboardFunctionKey;  /* 2627 */
	ALIGN64 char* ImeFileName;           /* 2628 */
	ALIGN64 BOOL UnicodeInput;           /* 2629 */
	ALIGN64 BOOL FastPathInput;          /* 2630 */
	ALIGN64 BOOL MultiTouchInput;        /* 2631 */
	ALIGN64 BOOL MultiTouchGestures;     /* 2632 */
	ALIGN64 UINT32 KeyboardHook;         /* 2633 */
	ALIGN64 BOOL HasHorizontalWheel;     /* 2634 */
	ALIGN64 BOOL HasExtendedMouseEvent;  /* 2635 */

	/** SuspendInput disables processing of keyboard/mouse/multitouch input.
	 * If used by an implementation ensure proper state resync after reenabling
	 * input
	 */
	ALIGN64 BOOL SuspendInput;       /* 2636 */
	UINT64 padding2688[2688 - 2637]; /* 2637 */

	/* Brush Capabilities */
	ALIGN64 UINT32 BrushSupportLevel; /* 2688 */
	UINT64 padding2752[2752 - 2689];  /* 2689 */

	/* Glyph Cache Capabilities */
	ALIGN64 UINT32 GlyphSupportLevel;           /* 2752 */
	ALIGN64 GLYPH_CACHE_DEFINITION* GlyphCache; /* 2753 */
	ALIGN64 GLYPH_CACHE_DEFINITION* FragCache;  /* 2754 */
	UINT64 padding2816[2816 - 2755];            /* 2755 */

	/* Offscreen Bitmap Cache */
	ALIGN64 UINT32 OffscreenSupportLevel; /* 2816 */
	ALIGN64 UINT32 OffscreenCacheSize;    /* 2817 */
	ALIGN64 UINT32 OffscreenCacheEntries; /* 2818 */
	UINT64 padding2880[2880 - 2819];      /* 2819 */

	/* Virtual Channel Capabilities */
	ALIGN64 UINT32 VirtualChannelCompressionFlags; /* 2880 */
	ALIGN64 UINT32 VirtualChannelChunkSize;        /* 2881 */
	UINT64 padding2944[2944 - 2882];               /* 2882 */

	/* Sound Capabilities */
	ALIGN64 BOOL SoundBeepsEnabled;  /* 2944 */
	UINT64 padding3008[3008 - 2945]; /* 2945 */
	UINT64 padding3072[3072 - 3008]; /* 3008 */

	/**
	 * Optional Capabilities
	 */

	/* Bitmap Cache Host Capabilities */
	UINT64 padding3136[3136 - 3072]; /* 3072 */

	/* Control Capabilities */
	UINT64 padding3200[3200 - 3136]; /* 3136 */

	/* Window Activation Capabilities */
	UINT64 padding3264[3264 - 3200]; /* 3200 */

	/* Font Capabilities */
	UINT64 padding3328[3328 - 3264]; /* 3264 */

	/* Multifragment Update Capabilities */
	ALIGN64 UINT32 MultifragMaxRequestSize; /* 3328 */
	UINT64 padding3392[3392 - 3329];        /* 3329 */

	/* Large Pointer Update Capabilities */
	ALIGN64 UINT32 LargePointerFlag; /* 3392 */
	UINT64 padding3456[3456 - 3393]; /* 3393 */

	/* Desktop Composition Capabilities */
	ALIGN64 UINT32 CompDeskSupportLevel; /* 3456 */
	UINT64 padding3520[3520 - 3457];     /* 3457 */

	/* Surface Commands Capabilities */
	ALIGN64 BOOL SurfaceCommandsEnabled;    /* 3520 */
	ALIGN64 BOOL FrameMarkerCommandEnabled; /* 3521 */
	ALIGN64 BOOL SurfaceFrameMarkerEnabled; /* 3522 */
	UINT64 padding3584[3584 - 3523];        /* 3523 */
	UINT64 padding3648[3648 - 3584];        /* 3584 */

	/*
	 * Bitmap Codecs Capabilities
	 */

	/* RemoteFX */
	ALIGN64 BOOL RemoteFxOnly;           /* 3648 */
	ALIGN64 BOOL RemoteFxCodec;          /* 3649 */
	ALIGN64 UINT32 RemoteFxCodecId;      /* 3650 */
	ALIGN64 UINT32 RemoteFxCodecMode;    /* 3651 */
	ALIGN64 BOOL RemoteFxImageCodec;     /* 3652 */
	ALIGN64 UINT32 RemoteFxCaptureFlags; /* 3653 */
	UINT64 padding3712[3712 - 3654];     /* 3654 */

	/* NSCodec */
	ALIGN64 BOOL NSCodec;                          /* 3712 */
	ALIGN64 UINT32 NSCodecId;                      /* 3713 */
	ALIGN64 UINT32 FrameAcknowledge;               /* 3714 */
	ALIGN64 UINT32 NSCodecColorLossLevel;          /* 3715 */
	ALIGN64 BOOL NSCodecAllowSubsampling;          /* 3716 */
	ALIGN64 BOOL NSCodecAllowDynamicColorFidelity; /* 3717 */
	UINT64 padding3776[3776 - 3718];               /* 3718 */

	/* JPEG */
	ALIGN64 BOOL JpegCodec;          /* 3776 */
	ALIGN64 UINT32 JpegCodecId;      /* 3777 */
	ALIGN64 UINT32 JpegQuality;      /* 3778 */
	UINT64 padding3840[3840 - 3779]; /* 3779 */

	ALIGN64 BOOL GfxThinClient;      /* 3840 */
	ALIGN64 BOOL GfxSmallCache;      /* 3841 */
	ALIGN64 BOOL GfxProgressive;     /* 3842 */
	ALIGN64 BOOL GfxProgressiveV2;   /* 3843 */
	ALIGN64 BOOL GfxH264;            /* 3844 */
	ALIGN64 BOOL GfxAVC444;          /* 3845 */
	ALIGN64 BOOL GfxSendQoeAck;      /* 3846 */
	ALIGN64 BOOL GfxAVC444v2;        /* 3847 */
	ALIGN64 UINT32 GfxCapsFilter;    /* 3848 */
	ALIGN64 BOOL GfxPlanar;          /* 3849 */
	UINT64 padding3904[3904 - 3850]; /* 3850 */

	/**
	 * Caches
	 */

	/* Bitmap Cache V3 */
	ALIGN64 UINT32 BitmapCacheV3CodecId; /* 3904 */
	UINT64 padding3968[3968 - 3905];     /* 3905 */

	/* Draw Nine Grid */
	ALIGN64 BOOL DrawNineGridEnabled;        /* 3968 */
	ALIGN64 UINT32 DrawNineGridCacheSize;    /* 3969 */
	ALIGN64 UINT32 DrawNineGridCacheEntries; /* 3970 */
	UINT64 padding4032[4032 - 3971];         /* 3971 */

	/* Draw GDI+ */
	ALIGN64 BOOL DrawGdiPlusEnabled;      /* 4032 */
	ALIGN64 BOOL DrawGdiPlusCacheEnabled; /* 4033 */
	UINT64 padding4096[4096 - 4034];      /* 4034 */
	UINT64 padding4160[4160 - 4096];      /* 4096 */

	/**
	 * Device Redirection
	 */

	/* Device Redirection */
	ALIGN64 BOOL DeviceRedirection;     /* 4160 */
	ALIGN64 UINT32 DeviceCount;         /* 4161 */
	ALIGN64 UINT32 DeviceArraySize;     /* 4162 */
	ALIGN64 RDPDR_DEVICE** DeviceArray; /* 4163 */
	UINT64 padding4288[4288 - 4164];    /* 4164 */

	/* Drive Redirection */
	ALIGN64 BOOL RedirectDrives;     /* 4288 */
	ALIGN64 BOOL RedirectHomeDrive;  /* 4289 */
	ALIGN64 char* DrivesToRedirect;  /* 4290 */
	UINT64 padding4416[4416 - 4291]; /* 4291 */

	/* Smartcard Redirection */
	ALIGN64 BOOL RedirectSmartCards; /* 4416 */
	UINT64 padding4544[4544 - 4417]; /* 4417 */

	/* Printer Redirection */
	ALIGN64 BOOL RedirectPrinters;   /* 4544 */
	UINT64 padding4672[4672 - 4545]; /* 4545 */

	/* Serial and Parallel Port Redirection */
	ALIGN64 BOOL RedirectSerialPorts;   /* 4672 */
	ALIGN64 BOOL RedirectParallelPorts; /* 4673 */
	ALIGN64 BOOL PreferIPv6OverIPv4;    /* 4674 */
	UINT64 padding4800[4800 - 4675];    /* 4675 */

	/**
	 * Other Redirection
	 */

	ALIGN64 BOOL RedirectClipboard;  /* 4800 */
	UINT64 padding4928[4928 - 4801]; /* 4801 */

	/**
	 * Static Virtual Channels
	 */

	ALIGN64 UINT32 StaticChannelCount;       /* 4928 */
	ALIGN64 UINT32 StaticChannelArraySize;   /* 4929 */
	ALIGN64 ADDIN_ARGV** StaticChannelArray; /* 4930 */
	UINT64 padding5056[5056 - 4931];         /* 4931 */

	/**
	 * Dynamic Virtual Channels
	 */

	ALIGN64 UINT32 DynamicChannelCount;       /* 5056 */
	ALIGN64 UINT32 DynamicChannelArraySize;   /* 5057 */
	ALIGN64 ADDIN_ARGV** DynamicChannelArray; /* 5058 */
	ALIGN64 BOOL SupportDynamicChannels;      /* 5059 */
	UINT64 padding5184[5184 - 5060];          /* 5060 */

	ALIGN64 BOOL SupportEchoChannel;      /* 5184 */
	ALIGN64 BOOL SupportDisplayControl;   /* 5185 */
	ALIGN64 BOOL SupportGeometryTracking; /* 5186 */
	ALIGN64 BOOL SupportSSHAgentChannel;  /* 5187 */
	ALIGN64 BOOL SupportVideoOptimized;   /* 5188 */
	ALIGN64 char* RDP2TCPArgs;            /* 5189 */
	ALIGN64 BOOL TcpKeepAlive;            /* 5190 */
	ALIGN64 UINT32 TcpKeepAliveRetries;   /* 5191 */
	ALIGN64 UINT32 TcpKeepAliveDelay;     /* 5192 */
	ALIGN64 UINT32 TcpKeepAliveInterval;  /* 5193 */
	ALIGN64 UINT32 TcpAckTimeout;         /* 5194 */
	ALIGN64 char* ActionScript;           /* 5195 */
	ALIGN64 UINT32 Floatbar;              /* 5196 */
	ALIGN64 UINT32 TcpConnectTimeout;     /* 5197 */
	UINT64 padding5312[5312 - 5198];      /* 5198 */

	/**
	 * WARNING: End of ABI stable zone!
	 *
	 * The zone below this point is ABI unstable, and
	 * is therefore potentially subject to ABI breakage.
	 */

	/*
	 * Extensions
	 */

	/* Extensions */
	ALIGN64 INT32 num_extensions;              /*  */
	ALIGN64 struct rdp_ext_set extensions[16]; /*  */

	ALIGN64 BYTE* SettingsModified; /* byte array marking fields that have been modified from their
	                                   default value - currently UNUSED! */
	ALIGN64 char* XSelectionAtom;
};
typedef struct rdp_settings rdpSettings;

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

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * rdpSettings creation flags
 */
#define FREERDP_SETTINGS_SERVER_MODE 0x00000001

	/** \brief creates a new setting struct
	 *
	 *  \param flags Flags for creation, use \b FREERDP_SETTINGS_SERVER_MODE for server settings, 0
	 * for client.
	 *
	 *  \return A newly allocated settings struct or NULL
	 */
	FREERDP_API rdpSettings* freerdp_settings_new(DWORD flags);

	/** \brief Creates a deep copy of settings
	 *
	 *  \param settings A pointer to a settings struct to copy. May be NULL (returns NULL)
	 *
	 *  \return A newly allocated copy of \b settings or NULL
	 */
	FREERDP_API rdpSettings* freerdp_settings_clone(const rdpSettings* settings);

	/** \brief Deep copies settings from \b src to \b dst
	 *
	 * The function frees up all allocated data in \b dst before copying the data from \b src
	 *
	 * \param dst A pointer for the settings to copy data to. May be NULL (fails copy)
	 * \param src A pointer to the settings to copy. May be NULL (fails copy)
	 *
	 *  \return \b TRUE for success, \b FALSE for failure.
	 */
	FREERDP_API BOOL freerdp_settings_copy(rdpSettings* dst, const rdpSettings* src);

	/** \brief Free a settings struct with all data in it
	 *
	 *  \param settings A pointer to the settings to free, May be NULL
	 */
	FREERDP_API void freerdp_settings_free(rdpSettings* settings);

	/** \brief Dumps the contents of a settings struct to a WLog logger
	 *
	 *  \param log The logger to write to, must not be NULL
	 *  \param level The WLog level to use for the log entries
	 *  \param settings A pointer to the settings to dump. May be NULL.
	 */
	FREERDP_API void freerdp_settings_dump(wLog* log, DWORD level, const rdpSettings* settings);

	/** \brief Dumps the difference between two settings structs to a WLog
	 *
	 *  \param log The logger to write to, must not be NULL.
	 *  \param  level The WLog level to use for the log entries.
	 *  \param src A pointer to the settings to dump. May be NULL.
	 *  \param other A pointer to the settings to dump. May be NULL.
	 *
	 *  \return \b TRUE if not equal, \b FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_settings_print_diff(wLog* log, DWORD level, const rdpSettings* src,
	                                             const rdpSettings* other);

	FREERDP_API ADDIN_ARGV* freerdp_addin_argv_new(size_t argc, const char* argv[]);
	FREERDP_API ADDIN_ARGV* freerdp_addin_argv_clone(const ADDIN_ARGV* args);
	FREERDP_API void freerdp_addin_argv_free(ADDIN_ARGV* args);

	FREERDP_API BOOL freerdp_addin_argv_add_argument(ADDIN_ARGV* args, const char* argument);
	FREERDP_API BOOL freerdp_addin_argv_add_argument_ex(ADDIN_ARGV* args, const char* argument,
	                                                    size_t len);
	FREERDP_API BOOL freerdp_addin_argv_del_argument(ADDIN_ARGV* args, const char* argument);

	FREERDP_API int freerdp_addin_set_argument(ADDIN_ARGV* args, const char* argument);
	FREERDP_API int freerdp_addin_replace_argument(ADDIN_ARGV* args, const char* previous,
	                                               const char* argument);
	FREERDP_API int freerdp_addin_set_argument_value(ADDIN_ARGV* args, const char* option,
	                                                 const char* value);
	FREERDP_API int freerdp_addin_replace_argument_value(ADDIN_ARGV* args, const char* previous,
	                                                     const char* option, const char* value);

	FREERDP_API BOOL freerdp_device_collection_add(rdpSettings* settings, RDPDR_DEVICE* device);
	FREERDP_API RDPDR_DEVICE* freerdp_device_collection_find(rdpSettings* settings,
	                                                         const char* name);
	FREERDP_API RDPDR_DEVICE* freerdp_device_collection_find_type(rdpSettings* settings,
	                                                              UINT32 type);

	FREERDP_API RDPDR_DEVICE* freerdp_device_new(UINT32 Type, size_t count, const char* args[]);
	FREERDP_API RDPDR_DEVICE* freerdp_device_clone(const RDPDR_DEVICE* device);
	FREERDP_API void freerdp_device_free(RDPDR_DEVICE* device);
	FREERDP_API BOOL freerdp_device_equal(const RDPDR_DEVICE* one, const RDPDR_DEVICE* other);

	FREERDP_API void freerdp_device_collection_free(rdpSettings* settings);

	FREERDP_API BOOL freerdp_static_channel_collection_add(rdpSettings* settings,
	                                                       ADDIN_ARGV* channel);
	FREERDP_API BOOL freerdp_static_channel_collection_del(rdpSettings* settings, const char* name);
	FREERDP_API ADDIN_ARGV* freerdp_static_channel_collection_find(rdpSettings* settings,
	                                                               const char* name);
#if defined(WITH_FREERDP_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED(ADDIN_ARGV* freerdp_static_channel_clone(ADDIN_ARGV* channel));
#endif

	FREERDP_API void freerdp_static_channel_collection_free(rdpSettings* settings);

	FREERDP_API BOOL freerdp_dynamic_channel_collection_add(rdpSettings* settings,
	                                                        ADDIN_ARGV* channel);
	FREERDP_API BOOL freerdp_dynamic_channel_collection_del(rdpSettings* settings,
	                                                        const char* name);
	FREERDP_API ADDIN_ARGV* freerdp_dynamic_channel_collection_find(const rdpSettings* settings,
	                                                                const char* name);

#if defined(WITH_FREERDP_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED(ADDIN_ARGV* freerdp_dynamic_channel_clone(ADDIN_ARGV* channel));
#endif

	FREERDP_API void freerdp_dynamic_channel_collection_free(rdpSettings* settings);
	FREERDP_API void freerdp_capability_buffer_free(rdpSettings* settings);
	FREERDP_API BOOL freerdp_capability_buffer_copy(rdpSettings* settings, const rdpSettings* src);

	FREERDP_API void freerdp_server_license_issuers_free(rdpSettings* settings);
	FREERDP_API BOOL freerdp_server_license_issuers_copy(rdpSettings* settings, char** addresses,
	                                                     UINT32 count);

	FREERDP_API void freerdp_target_net_addresses_free(rdpSettings* settings);
	FREERDP_API BOOL freerdp_target_net_addresses_copy(rdpSettings* settings, char** addresses,
	                                                   UINT32 count);

	FREERDP_API void freerdp_performance_flags_make(rdpSettings* settings);
	FREERDP_API void freerdp_performance_flags_split(rdpSettings* settings);

	FREERDP_API BOOL freerdp_set_gateway_usage_method(rdpSettings* settings,
	                                                  UINT32 GatewayUsageMethod);
	FREERDP_API void freerdp_update_gateway_usage_method(rdpSettings* settings,
	                                                     UINT32 GatewayEnabled,
	                                                     UINT32 GatewayBypassLocal);

	/* DEPRECATED:
	 * the functions freerdp_get_param_* and freerdp_set_param_* are deprecated.
	 * use freerdp_settings_get_* and freerdp_settings_set_* as a replacement!
	 */
#if defined(WITH_FREERDP_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_get_bool instead",
	                                 BOOL freerdp_get_param_bool(const rdpSettings* settings,
	                                                             int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_bool instead",
	                                 int freerdp_set_param_bool(rdpSettings* settings, int id,
	                                                            BOOL param));

	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_get_int[16|32] instead",
	                                 int freerdp_get_param_int(const rdpSettings* settings,
	                                                           int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_int[16|32] instead",
	                                 int freerdp_set_param_int(rdpSettings* settings, int id,
	                                                           int param));

	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_uint32 instead",
	                                 UINT32 freerdp_get_param_uint32(const rdpSettings* settings,
	                                                                 int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_uint32 instead",
	                                 int freerdp_set_param_uint32(rdpSettings* settings, int id,
	                                                              UINT32 param));

	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_get_uint64 instead",
	                                 UINT64 freerdp_get_param_uint64(const rdpSettings* settings,
	                                                                 int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_uint64 instead",
	                                 int freerdp_set_param_uint64(rdpSettings* settings, int id,
	                                                              UINT64 param));

	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_get_string instead",
	                                 char* freerdp_get_param_string(const rdpSettings* settings,
	                                                                int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_string instead",
	                                 int freerdp_set_param_string(rdpSettings* settings, int id,
	                                                              const char* param));
#endif

	/** \brief Returns a boolean settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the boolean key
	 */
	FREERDP_API BOOL freerdp_settings_get_bool(const rdpSettings* settings, size_t id);

	/** \brief Sets a BOOL settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_bool(rdpSettings* settings, size_t id, BOOL param);

	/** \brief Returns a INT16 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the INT16 key
	 */
	FREERDP_API INT16 freerdp_settings_get_int16(const rdpSettings* settings, size_t id);

	/** \brief Sets a INT16 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_int16(rdpSettings* settings, size_t id, INT16 param);

	/** \brief Returns a UINT16 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the UINT16 key
	 */
	FREERDP_API UINT16 freerdp_settings_get_uint16(const rdpSettings* settings, size_t id);

	/** \brief Sets a UINT16 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_uint16(rdpSettings* settings, size_t id, UINT16 param);

	/** \brief Returns a INT32 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the INT32 key
	 */
	FREERDP_API INT32 freerdp_settings_get_int32(const rdpSettings* settings, size_t id);

	/** \brief Sets a INT32 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_int32(rdpSettings* settings, size_t id, INT32 param);

	/** \brief Returns a UINT32 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the UINT32 key
	 */
	FREERDP_API UINT32 freerdp_settings_get_uint32(const rdpSettings* settings, size_t id);

	/** \brief Sets a UINT32 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_uint32(rdpSettings* settings, size_t id, UINT32 param);

	/** \brief Returns a INT64 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the INT64 key
	 */
	FREERDP_API INT64 freerdp_settings_get_int64(const rdpSettings* settings, size_t id);

	/** \brief Sets a INT64 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_int64(rdpSettings* settings, size_t id, INT64 param);

	/** \brief Returns a UINT64 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the UINT64 key
	 */
	FREERDP_API UINT64 freerdp_settings_get_uint64(const rdpSettings* settings, size_t id);

	/** \brief Sets a UINT64 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_uint64(rdpSettings* settings, size_t id, UINT64 param);

	/** \brief Returns a immutable string settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the immutable string pointer
	 */
	FREERDP_API const char* freerdp_settings_get_string(const rdpSettings* settings, size_t id);

	/** \brief Returns a string settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the string pointer
	 */
	FREERDP_API char* freerdp_settings_get_string_writable(rdpSettings* settings, size_t id);

	/** \brief Sets a string settings value. The \b param is copied.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set. If NULL allocates an empty string buffer of \b len size,
	 * otherwise a copy is created. \param len The length of \b param, 0 to remove the old entry.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_string_len(rdpSettings* settings, size_t id,
	                                                 const char* param, size_t len);

	/** \brief Sets a string settings value. The \b param is copied.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set. If NULL removes the old entry, otherwise a copy is created.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_string(rdpSettings* settings, size_t id,
	                                             const char* param);

	/** \brief Sets a string settings value. The \b param is converted to UTF-8 and the copy stored.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set. If NULL removes the old entry, otherwise a copy is created.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_string_from_utf16(rdpSettings* settings, size_t id,
	                                                        const WCHAR* param);

	/** \brief Sets a string settings value. The \b param is converted to UTF-8 and the copy stored.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set. If NULL removes the old entry, otherwise a copy is created.
	 *  \param length The length of the WCHAR string in number of WCHAR characters
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_string_from_utf16N(rdpSettings* settings, size_t id,
	                                                         const WCHAR* param, size_t length);
	/** \brief Return an allocated UTF16 string
	 *
	 * \param settings A pointer to the settings struct to use
	 * \param id The settings identifier
	 *
	 * \return An allocated, '\0' terminated WCHAR string or NULL
	 */
	FREERDP_API WCHAR* freerdp_settings_get_string_as_utf16(const rdpSettings* settings, size_t id,
	                                                        size_t* pCharLen);

	/** \brief Returns a immutable pointer settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the immutable pointer value
	 */
	FREERDP_API const void* freerdp_settings_get_pointer(const rdpSettings* settings, size_t id);

	/** \brief Returns a mutable pointer settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the mutable pointer value
	 */
	FREERDP_API void* freerdp_settings_get_pointer_writable(rdpSettings* settings, size_t id);
	FREERDP_API BOOL freerdp_settings_set_pointer(rdpSettings* settings, size_t id,
	                                              const void* data);
	FREERDP_API BOOL freerdp_settings_set_pointer_len(rdpSettings* settings, size_t id,
	                                                  const void* data, size_t len);

	FREERDP_API const void* freerdp_settings_get_pointer_array(const rdpSettings* settings,
	                                                           size_t id, size_t offset);
	FREERDP_API void* freerdp_settings_get_pointer_array_writable(const rdpSettings* settings,
	                                                              size_t id, size_t offset);
	FREERDP_API BOOL freerdp_settings_set_pointer_array(rdpSettings* settings, size_t id,
	                                                    size_t offset, const void* data);

	FREERDP_API BOOL freerdp_settings_set_value_for_name(rdpSettings* settings, const char* name,
	                                                     const char* value);

	/** \brief Get a key index for the name string of that key
	 *
	 *  \param value A key name string like FreeRDP_ServerMode
	 *
	 *  \return The key index or -1 in case of an error (e.g. name does not exist)
	 */
	FREERDP_API SSIZE_T freerdp_settings_get_key_for_name(const char* value);

	/** \brief Get a key type for the name string of that key
	 *
	 *  \param value A key name string like FreeRDP_ServerMode
	 *
	 *  \return The key type (e.g. FREERDP_SETTINGS_TYPE_BOOL) or -1 in case of an error (e.g. name
	 * does not exist)
	 */
	FREERDP_API SSIZE_T freerdp_settings_get_type_for_name(const char* value);

	/** \brief Get a key type for the key index
	 *
	 *  \param key The key index like FreeRDP_ServerMode
	 *
	 *  \return The key type (e.g. FREERDP_SETTINGS_TYPE_BOOL) or -1 in case of an error (e.g. name
	 * does not exist)
	 */
	FREERDP_API SSIZE_T freerdp_settings_get_type_for_key(size_t key);
	FREERDP_API const char* freerdp_settings_get_type_name_for_key(size_t key);
	FREERDP_API const char* freerdp_settings_get_type_name_for_type(SSIZE_T type);

	FREERDP_API const char* freerdp_settings_get_name_for_key(size_t key);
	FREERDP_API UINT32 freerdp_settings_get_codecs_flags(const rdpSettings* settings);

	/** \brief Parse capability data and apply to settings
	 *
	 *  The capability message is stored in raw form in the settings, the data parsed and applied to
	 * the settings.
	 *
	 *  \param settings A pointer to the settings to use
	 *  \param capsFlags A pointer to the capablity flags, must have capsCount fields
	 *  \param capsData A pointer array to the RAW capability data, must have capsCount fields
	 *  \param capsSizes A pointer to an array of RAW capability sizes, must have capsCount fields
	 *  \param capsCount The number of capabilities contained in the RAW data
	 *  \param serverReceivedCaps Indicates if the parser should assume to be a server or client
	 * instance
	 *
	 *  \return \b TRUE for success, \b FALSE in case of an error
	 */
	FREERDP_API BOOL freerdp_settings_update_from_caps(rdpSettings* settings, const BYTE* capsFlags,
	                                                   const BYTE** capsData,
	                                                   const UINT32* capsSizes, UINT32 capsCount,
	                                                   BOOL serverReceivedCaps);

	/** \brief A helper function to return the correct server name.
	 *
	 * The server name might be in key FreeRDP_ServerHostname or if used in
	 * FreeRDP_UserSpecifiedServerName. This function returns the correct name to use.
	 *
	 *  \param settings The settings to query, must not be NULL.
	 *
	 *  \return A string pointer or NULL in case of failure.
	 */
	FREERDP_API const char* freerdp_settings_get_server_name(const rdpSettings* settings);

	/** \brief Returns a stringified representation of RAIL support flags
	 *
	 *  \param flags The flags to stringify
	 *  \param buffer A pointer to the string buffer to write to
	 *  \param length The size of the string buffer
	 *
	 *  \return A pointer to \b buffer for success, NULL otherwise
	 */
	FREERDP_API char* freerdp_rail_support_flags_to_string(UINT32 flags, char* buffer,
	                                                       size_t length);

	/** \brief Returns a stringified representation of the RDP protocol version.
	 *
	 *  \param version The RDP protocol version number.
	 *
	 *  \return A string representation of the protocol version as "RDP_VERSION_10_11" or
	 * "RDP_VERSION_UNKNOWN" for invalid/unknown versions
	 */
	FREERDP_API const char* freerdp_rdp_version_string(UINT32 version);

	/** \brief Returns a string representation of \b RDPDR_DTYP_*
	 *
	 *  \param type The integer of the \b RDPDR_DTYP_* to stringify
	 *
	 *  \return A string representation of the \b RDPDR_DTYP_* or "RDPDR_DTYP_UNKNOWN"
	 */
	FREERDP_API const char* freerdp_rdpdr_dtyp_string(UINT32 type);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SETTINGS_H */

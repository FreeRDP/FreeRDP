/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Settings
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/api.h>
#include <freerdp/types.h>

/* Performance Flags */
#define PERF_FLAG_NONE                  	0x00000000
#define PERF_DISABLE_WALLPAPER          	0x00000001
#define PERF_DISABLE_FULLWINDOWDRAG    		0x00000002
#define PERF_DISABLE_MENUANIMATIONS     	0x00000004
#define PERF_DISABLE_THEMING            	0x00000008
#define PERF_DISABLE_CURSOR_SHADOW      	0x00000020
#define PERF_DISABLE_CURSORSETTINGS     	0x00000040
#define PERF_ENABLE_FONT_SMOOTHING      	0x00000080
#define PERF_ENABLE_DESKTOP_COMPOSITION 	0x00000100

/* Connection Types */
#define CONNECTION_TYPE_MODEM			0x01
#define CONNECTION_TYPE_BROADBAND_LOW		0x02
#define CONNECTION_TYPE_SATELLITE		0x03
#define CONNECTION_TYPE_BROADBAND_HIGH		0x04
#define CONNECTION_TYPE_WAN			0x05
#define CONNECTION_TYPE_LAN			0x06
#define CONNECTION_TYPE_AUTODETECT		0x07

/* Client to Server (CS) data blocks */
#define CS_CORE			0xC001
#define CS_SECURITY		0xC002
#define CS_NET			0xC003
#define CS_CLUSTER		0xC004
#define CS_MONITOR		0xC005
#define CS_MCS_MSGCHANNEL	0xC006
#define CS_MULTITRANSPORT	0xC008

/* Server to Client (SC) data blocks */
#define SC_CORE			0x0C01
#define SC_SECURITY		0x0C02
#define SC_NET			0x0C03
#define SC_MULTITRANSPORT	0x0C06

/* RDP version */
#define RDP_VERSION_4		0x00080001
#define RDP_VERSION_5_PLUS	0x00080004

/* Color depth */
#define RNS_UD_COLOR_4BPP	0xCA00
#define RNS_UD_COLOR_8BPP	0xCA01
#define RNS_UD_COLOR_16BPP_555	0xCA02
#define RNS_UD_COLOR_16BPP_565	0xCA03
#define RNS_UD_COLOR_24BPP	0xCA04

/* Secure Access Sequence */
#define RNS_UD_SAS_DEL		0xAA03

/* Supported Color Depths */
#define RNS_UD_24BPP_SUPPORT	0x0001
#define RNS_UD_16BPP_SUPPORT	0x0002
#define RNS_UD_15BPP_SUPPORT	0x0004
#define RNS_UD_32BPP_SUPPORT	0x0008

/* Audio Mode */
#define AUDIO_MODE_REDIRECT		0 /* Bring to this computer */
#define AUDIO_MODE_PLAY_ON_SERVER	1 /* Leave at remote computer */
#define AUDIO_MODE_NONE			2 /* Do not play */

/* Early Capability Flags (Client to Server) */
#define RNS_UD_CS_SUPPORT_ERRINFO_PDU		0x0001
#define RNS_UD_CS_WANT_32BPP_SESSION		0x0002
#define RNS_UD_CS_SUPPORT_STATUSINFO_PDU	0x0004
#define RNS_UD_CS_STRONG_ASYMMETRIC_KEYS	0x0008
#define RNS_UD_CS_VALID_CONNECTION_TYPE		0x0020
#define RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU	0x0040
#define RNS_UD_CS_SUPPORT_NETWORK_AUTODETECT	0x0080
#define RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL	0x0100
#define RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE	0x0200

/* Early Capability Flags (Server to Client) */
#define RNS_UD_SC_EDGE_ACTIONS_SUPPORTED	0x00000001
#define RNS_UD_SC_DYNAMIC_DST_SUPPORTED		0x00000002

/* Cluster Information Flags */
#define REDIRECTION_SUPPORTED			0x00000001
#define REDIRECTED_SESSIONID_FIELD_VALID	0x00000002
#define REDIRECTED_SMARTCARD			0x00000040

#define REDIRECTION_VERSION1			0x00
#define REDIRECTION_VERSION2			0x01
#define REDIRECTION_VERSION3			0x02
#define REDIRECTION_VERSION4			0x03
#define REDIRECTION_VERSION5			0x04
#define REDIRECTION_VERSION6			0x05

#define MONITOR_PRIMARY				0x00000001

/* Encryption Methods */
#define ENCRYPTION_METHOD_NONE			0x00000000
#define ENCRYPTION_METHOD_40BIT			0x00000001
#define ENCRYPTION_METHOD_128BIT		0x00000002
#define ENCRYPTION_METHOD_56BIT			0x00000008
#define ENCRYPTION_METHOD_FIPS			0x00000010

/* Encryption Levels */
#define ENCRYPTION_LEVEL_NONE			0x00000000
#define ENCRYPTION_LEVEL_LOW			0x00000001
#define ENCRYPTION_LEVEL_CLIENT_COMPATIBLE	0x00000002
#define ENCRYPTION_LEVEL_HIGH			0x00000003
#define ENCRYPTION_LEVEL_FIPS			0x00000004

/* Multitransport Types */
#define TRANSPORT_TYPE_UDP_FECR			0x00000001
#define TRANSPORT_TYPE_UDP_FECL			0x00000004
#define TRANSPORT_TYPE_UDP_PREFERRED		0x00000100

/* Static Virtual Channel Options */
#define CHANNEL_OPTION_INITIALIZED		0x80000000
#define CHANNEL_OPTION_ENCRYPT_RDP		0x40000000
#define CHANNEL_OPTION_ENCRYPT_SC		0x20000000
#define CHANNEL_OPTION_ENCRYPT_CS		0x10000000
#define CHANNEL_OPTION_PRI_HIGH			0x08000000
#define CHANNEL_OPTION_PRI_MED			0x04000000
#define CHANNEL_OPTION_PRI_LOW			0x02000000
#define CHANNEL_OPTION_COMPRESS_RDP		0x00800000
#define CHANNEL_OPTION_COMPRESS			0x00400000
#define CHANNEL_OPTION_SHOW_PROTOCOL		0x00200000
#define CHANNEL_REMOTE_CONTROL_PERSISTENT	0x00100000

/* Auto Reconnect Version */
#define AUTO_RECONNECT_VERSION_1		0x00000001

/* Cookie Lengths */
#define MSTSC_COOKIE_MAX_LENGTH			9
#define DEFAULT_COOKIE_MAX_LENGTH		0xFF

/* Order Support */
#define NEG_DSTBLT_INDEX			0x00
#define NEG_PATBLT_INDEX			0x01
#define NEG_SCRBLT_INDEX			0x02
#define NEG_MEMBLT_INDEX			0x03
#define NEG_MEM3BLT_INDEX			0x04
#define NEG_ATEXTOUT_INDEX			0x05
#define NEG_AEXTTEXTOUT_INDEX			0x06
#define NEG_DRAWNINEGRID_INDEX			0x07
#define NEG_LINETO_INDEX			0x08
#define NEG_MULTI_DRAWNINEGRID_INDEX		0x09
#define NEG_OPAQUE_RECT_INDEX			0x0A
#define NEG_SAVEBITMAP_INDEX			0x0B
#define NEG_WTEXTOUT_INDEX			0x0C
#define NEG_MEMBLT_V2_INDEX			0x0D
#define NEG_MEM3BLT_V2_INDEX			0x0E
#define NEG_MULTIDSTBLT_INDEX			0x0F
#define NEG_MULTIPATBLT_INDEX			0x10
#define NEG_MULTISCRBLT_INDEX			0x11
#define NEG_MULTIOPAQUERECT_INDEX		0x12
#define NEG_FAST_INDEX_INDEX			0x13
#define NEG_POLYGON_SC_INDEX			0x14
#define NEG_POLYGON_CB_INDEX			0x15
#define NEG_POLYLINE_INDEX			0x16
#define NEG_UNUSED23_INDEX			0x17
#define NEG_FAST_GLYPH_INDEX			0x18
#define NEG_ELLIPSE_SC_INDEX			0x19
#define NEG_ELLIPSE_CB_INDEX			0x1A
#define NEG_GLYPH_INDEX_INDEX			0x1B
#define NEG_GLYPH_WEXTTEXTOUT_INDEX		0x1C
#define NEG_GLYPH_WLONGTEXTOUT_INDEX		0x1D
#define NEG_GLYPH_WLONGEXTTEXTOUT_INDEX		0x1E
#define NEG_UNUSED31_INDEX			0x1F

/* Glyph Support Level */
#define GLYPH_SUPPORT_NONE			0x0000
#define GLYPH_SUPPORT_PARTIAL			0x0001
#define GLYPH_SUPPORT_FULL			0x0002
#define GLYPH_SUPPORT_ENCODE			0x0003

/* Gateway Usage Method */
#define TSC_PROXY_MODE_NONE_DIRECT		0x0
#define TSC_PROXY_MODE_DIRECT			0x1
#define TSC_PROXY_MODE_DETECT			0x2
#define TSC_PROXY_MODE_DEFAULT			0x3
#define TSC_PROXY_MODE_NONE_DETECT		0x4

/* Gateway Credentials Source */
#define TSC_PROXY_CREDS_MODE_USERPASS		0x0
#define TSC_PROXY_CREDS_MODE_SMARTCARD		0x1
#define TSC_PROXY_CREDS_MODE_ANY		0x2

/* Redirection Flags */
#define LB_TARGET_NET_ADDRESS			0x00000001
#define LB_LOAD_BALANCE_INFO			0x00000002
#define LB_USERNAME				0x00000004
#define LB_DOMAIN				0x00000008
#define LB_PASSWORD				0x00000010
#define LB_DONTSTOREUSERNAME			0x00000020
#define LB_SMARTCARD_LOGON			0x00000040
#define LB_NOREDIRECT				0x00000080
#define LB_TARGET_FQDN				0x00000100
#define LB_TARGET_NETBIOS_NAME			0x00000200
#define LB_TARGET_NET_ADDRESSES			0x00000800
#define LB_CLIENT_TSV_URL			0x00001000
#define LB_SERVER_TSV_CAPABLE			0x00002000

struct _TARGET_NET_ADDRESS
{
	UINT32 Length;
	LPWSTR Address;
};
typedef struct _TARGET_NET_ADDRESS TARGET_NET_ADDRESS;

/* Logon Error Info */

#define LOGON_MSG_NO_PERMISSION			0xFFFFFFFA
#define LOGON_MSG_BUMP_OPTIONS			0xFFFFFFFB
#define LOGON_MSG_SESSION_RECONNECT		0xFFFFFFFC
#define LOGON_MSG_SESSION_TERMINATE		0xFFFFFFFD
#define LOGON_MSG_SESSION_CONTINUE		0xFFFFFFFE

#define LOGON_FAILED_BAD_PASSWORD		0x00000000
#define LOGON_FAILED_UPDATE_PASSWORD		0x00000001
#define LOGON_FAILED_OTHER			0x00000002
#define LOGON_WARNING				0x00000003

/* SYSTEM_TIME */
typedef struct
{
	UINT16 wYear;
	UINT16 wMonth;
	UINT16 wDayOfWeek;
	UINT16 wDay;
	UINT16 wHour;
	UINT16 wMinute;
	UINT16 wSecond;
	UINT16 wMilliseconds;
} SYSTEM_TIME;

/* TIME_ZONE_INFORMATION */
struct _TIME_ZONE_INFO
{
	UINT32 bias;
	char standardName[32];
	SYSTEM_TIME standardDate;
	UINT32 standardBias;
	char daylightName[32];
	SYSTEM_TIME daylightDate;
	UINT32 daylightBias;
};
typedef struct _TIME_ZONE_INFO TIME_ZONE_INFO;

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

struct rdp_rsa_key
{
	BYTE* Modulus;
	DWORD ModulusLength;
	BYTE* PrivateExponent;
	DWORD PrivateExponentLength;
	BYTE exponent[4];
};
typedef struct rdp_rsa_key rdpRsaKey;

/* Channels */

struct rdp_channel
{
	char Name[8];
	UINT32 options;
	int ChannelId;
	BOOL joined;
	void* handle;
};
typedef struct rdp_channel rdpChannel;

struct _ADDIN_ARGV
{
	int argc;
	char** argv;
};
typedef struct _ADDIN_ARGV ADDIN_ARGV;

/* Extensions */

struct rdp_ext_set
{
	char name[256]; /* plugin name or path */
	void* data; /* plugin data */
};

/* Bitmap Cache */

struct _BITMAP_CACHE_CELL_INFO
{
	UINT16 numEntries;
	UINT16 maxSize;
};
typedef struct _BITMAP_CACHE_CELL_INFO BITMAP_CACHE_CELL_INFO;

struct _BITMAP_CACHE_V2_CELL_INFO
{
	UINT32 numEntries;
	BOOL persistent;
};
typedef struct _BITMAP_CACHE_V2_CELL_INFO BITMAP_CACHE_V2_CELL_INFO;

/* Glyph Cache */

struct _GLYPH_CACHE_DEFINITION
{
	UINT16 cacheEntries;
	UINT16 cacheMaximumCellSize;
};
typedef struct _GLYPH_CACHE_DEFINITION GLYPH_CACHE_DEFINITION;

/* Monitors */

struct rdp_monitor
{
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;
	UINT32 is_primary;
};
typedef struct rdp_monitor rdpMonitor;

/* Device Redirection */

#define RDPDR_DTYP_SERIAL		0x00000001
#define RDPDR_DTYP_PARALLEL		0x00000002
#define RDPDR_DTYP_PRINT		0x00000004
#define RDPDR_DTYP_FILESYSTEM		0x00000008
#define RDPDR_DTYP_SMARTCARD		0x00000020

struct _RDPDR_DEVICE
{
	UINT32 Id;
	UINT32 Type;
	char* Name;
};
typedef struct _RDPDR_DEVICE RDPDR_DEVICE;

struct _RDPDR_DRIVE
{
	UINT32 Id;
	UINT32 Type;
	char* Name;
	char* Path;
};
typedef struct _RDPDR_DRIVE RDPDR_DRIVE;

struct _RDPDR_PRINTER
{
	UINT32 Id;
	UINT32 Type;
	char* Name;
	char* DriverName;
};
typedef struct _RDPDR_PRINTER RDPDR_PRINTER;

struct _RDPDR_SMARTCARD
{
	UINT32 Id;
	UINT32 Type;
	char* Name;
	char* Path;
};
typedef struct _RDPDR_SMARTCARD RDPDR_SMARTCARD;

struct _RDPDR_SERIAL
{
	UINT32 Id;
	UINT32 Type;
	char* Name;
	char* Path;
};
typedef struct _RDPDR_SERIAL RDPDR_SERIAL;

struct _RDPDR_PARALLEL
{
	UINT32 Id;
	UINT32 Type;
	char* Name;
	char* Path;
};
typedef struct _RDPDR_PARALLEL RDPDR_PARALLEL;

/* Settings */

#ifdef __GNUC__
#define ALIGN64	__attribute__((aligned(8)))
#else
#ifdef _WIN32
#define ALIGN64	__declspec(align(8))
#else
#define ALIGN64
#endif
#endif

/**
 * FreeRDP Settings Ids
 * This is generated with a script parsing the rdpSettings data structure
 */

#define FreeRDP_instance					0
#define FreeRDP_ServerMode					16
#define FreeRDP_ShareId						17
#define FreeRDP_PduSource					18
#define FreeRDP_ServerPort					19
#define FreeRDP_ServerHostname					20
#define FreeRDP_Username					21
#define FreeRDP_Password					22
#define FreeRDP_Domain						23
#define FreeRDP_RdpVersion					128
#define FreeRDP_DesktopWidth					129
#define FreeRDP_DesktopHeight					130
#define FreeRDP_ColorDepth					131
#define FreeRDP_ConnectionType					132
#define FreeRDP_ClientBuild					133
#define FreeRDP_ClientHostname					134
#define FreeRDP_ClientProductId					135
#define FreeRDP_EarlyCapabilityFlags				136
#define FreeRDP_NetworkAutoDetect				137
#define FreeRDP_SupportAsymetricKeys				138
#define FreeRDP_SupportErrorInfoPdu				139
#define FreeRDP_SupportStatusInfoPdu				140
#define FreeRDP_SupportMonitorLayoutPdu				141
#define FreeRDP_SupportGraphicsPipeline				142
#define FreeRDP_SupportDynamicTimeZone				143
#define FreeRDP_DisableEncryption				192
#define FreeRDP_EncryptionMethods				193
#define FreeRDP_ExtEncryptionMethods				194
#define FreeRDP_EncryptionLevel					195
#define FreeRDP_ServerRandom					196
#define FreeRDP_ServerRandomLength				197
#define FreeRDP_ServerCertificate				198
#define FreeRDP_ServerCertificateLength				199
#define FreeRDP_ChannelCount					256
#define FreeRDP_ChannelDefArraySize				257
#define FreeRDP_ChannelDefArray					258
#define FreeRDP_ClusterInfoFlags				320
#define FreeRDP_RedirectedSessionId				321
#define FreeRDP_ConsoleSession					322
#define FreeRDP_MonitorCount					384
#define FreeRDP_MonitorDefArraySize				385
#define FreeRDP_MonitorDefArray					386
#define FreeRDP_SpanMonitors					387
#define FreeRDP_UseMultimon					388
#define FreeRDP_ForceMultimon					389
#define FreeRDP_DesktopPosX					390
#define FreeRDP_DesktopPosY					391
#define FreeRDP_MultitransportFlags				512
#define FreeRDP_AlternateShell					640
#define FreeRDP_ShellWorkingDirectory				641
#define FreeRDP_AutoLogonEnabled				704
#define FreeRDP_CompressionEnabled				705
#define FreeRDP_DisableCtrlAltDel				706
#define FreeRDP_EnableWindowsKey				707
#define FreeRDP_MaximizeShell					708
#define FreeRDP_LogonNotify					709
#define FreeRDP_LogonErrors					710
#define FreeRDP_MouseAttached					711
#define FreeRDP_MouseHasWheel					712
#define FreeRDP_RemoteConsoleAudio				713
#define FreeRDP_AudioPlayback					714
#define FreeRDP_AudioCapture					715
#define FreeRDP_VideoDisable					716
#define FreeRDP_PasswordIsSmartcardPin				717
#define FreeRDP_UsingSavedCredentials				718
#define FreeRDP_ForceEncryptedCsPdu				719
#define FreeRDP_IPv6Enabled					768
#define FreeRDP_ClientAddress					769
#define FreeRDP_ClientDir					770
#define FreeRDP_AutoReconnectionEnabled				832
#define FreeRDP_AutoReconnectMaxRetries				833
#define FreeRDP_ClientAutoReconnectCookie			834
#define FreeRDP_ServerAutoReconnectCookie			835
#define FreeRDP_ClientTimeZone					896
#define FreeRDP_DynamicDSTTimeZoneKeyName			897
#define FreeRDP_DynamicDaylightTimeDisabled			898
#define FreeRDP_PerformanceFlags				960
#define FreeRDP_AllowFontSmoothing				961
#define FreeRDP_DisableWallpaper				962
#define FreeRDP_DisableFullWindowDrag				963
#define FreeRDP_DisableMenuAnims				964
#define FreeRDP_DisableThemes					965
#define FreeRDP_DisableCursorShadow				966
#define FreeRDP_DisableCursorBlinking				967
#define FreeRDP_AllowDesktopComposition				968
#define FreeRDP_TlsSecurity					1088
#define FreeRDP_NlaSecurity					1089
#define FreeRDP_RdpSecurity					1090
#define FreeRDP_ExtSecurity					1091
#define FreeRDP_Authentication					1092
#define FreeRDP_RequestedProtocols				1093
#define FreeRDP_SelectedProtocol				1094
#define FreeRDP_NegotiationFlags				1095
#define FreeRDP_NegotiateSecurityLayer				1096
#define FreeRDP_MstscCookieMode					1152
#define FreeRDP_CookieMaxLength					1153
#define FreeRDP_PreconnectionId					1154
#define FreeRDP_PreconnectionBlob				1155
#define FreeRDP_SendPreconnectionPdu				1156
#define FreeRDP_RedirectionFlags				1216
#define FreeRDP_LoadBalanceInfo					1217
#define FreeRDP_LoadBalanceInfoLength				1218
#define FreeRDP_RedirectionUsername				1219
#define FreeRDP_RedirectionUsernameLength			1220
#define FreeRDP_RedirectionDomain				1221
#define FreeRDP_RedirectionDomainLength				1222
#define FreeRDP_RedirectionPassword				1223
#define FreeRDP_RedirectionPasswordLength			1224
#define FreeRDP_RedirectionTargetFQDN				1225
#define FreeRDP_RedirectionTargetFQDNLength			1226
#define FreeRDP_RedirectionTargetNetBiosName			1227
#define FreeRDP_RedirectionTargetNetBiosNameLength		1228
#define FreeRDP_RedirectionTsvUrl				1229
#define FreeRDP_RedirectionTsvUrlLength				1230
#define FreeRDP_TargetNetAddressCount				1231
#define FreeRDP_TargetNetAddresses				1232
#define FreeRDP_Password51					1280
#define FreeRDP_Password51Length				1281
#define FreeRDP_KerberosKdc					1344
#define FreeRDP_KerberosRealm					1345
#define FreeRDP_IgnoreCertificate				1408
#define FreeRDP_CertificateName					1409
#define FreeRDP_CertificateFile					1410
#define FreeRDP_PrivateKeyFile					1411
#define FreeRDP_RdpKeyFile					1412
#define FreeRDP_RdpServerRsaKey					1413
#define FreeRDP_RdpServerCertificate				1414
#define FreeRDP_Workarea					1536
#define FreeRDP_Fullscreen					1537
#define FreeRDP_PercentScreen					1538
#define FreeRDP_GrabKeyboard					1539
#define FreeRDP_Decorations					1540
#define FreeRDP_MouseMotion					1541
#define FreeRDP_WindowTitle					1542
#define FreeRDP_ParentWindowId					1543
#define FreeRDP_AsyncInput					1544
#define FreeRDP_AsyncUpdate					1545
#define FreeRDP_AsyncChannels					1546
#define FreeRDP_AsyncTransport					1547
#define FreeRDP_ToggleFullscreen				1548
#define FreeRDP_WmClass						1549
#define FreeRDP_EmbeddedWindow					1550
#define FreeRDP_SmartSizing					1551
#define FreeRDP_XPan						1552
#define FreeRDP_YPan						1553
#define FreeRDP_ScalingFactor					1554
#define FreeRDP_SoftwareGdi					1601
#define FreeRDP_LocalConnection					1602
#define FreeRDP_AuthenticationOnly				1603
#define FreeRDP_CredentialsFromStdin				1604
#define FreeRDP_ComputerName					1664
#define FreeRDP_ConnectionFile					1728
#define FreeRDP_HomePath					1792
#define FreeRDP_ConfigPath					1793
#define FreeRDP_CurrentPath					1794
#define FreeRDP_DumpRemoteFx					1856
#define FreeRDP_PlayRemoteFx					1857
#define FreeRDP_DumpRemoteFxFile				1858
#define FreeRDP_PlayRemoteFxFile				1859
#define FreeRDP_GatewayUsageMethod				1984
#define FreeRDP_GatewayPort					1985
#define FreeRDP_GatewayHostname					1986
#define FreeRDP_GatewayUsername					1987
#define FreeRDP_GatewayPassword					1988
#define FreeRDP_GatewayDomain					1989
#define FreeRDP_GatewayCredentialsSource			1990
#define FreeRDP_GatewayUseSameCredentials			1991
#define FreeRDP_GatewayEnabled					1992
#define FreeRDP_RemoteApplicationMode				2112
#define FreeRDP_RemoteApplicationName				2113
#define FreeRDP_RemoteApplicationIcon				2114
#define FreeRDP_RemoteApplicationProgram			2115
#define FreeRDP_RemoteApplicationFile				2116
#define FreeRDP_RemoteApplicationGuid				2117
#define FreeRDP_RemoteApplicationCmdLine			2118
#define FreeRDP_RemoteApplicationExpandCmdLine			2119
#define FreeRDP_RemoteApplicationExpandWorkingDir		2120
#define FreeRDP_DisableRemoteAppCapsCheck			2121
#define FreeRDP_RemoteAppNumIconCaches				2122
#define FreeRDP_RemoteAppNumIconCacheEntries			2123
#define FreeRDP_RemoteAppLanguageBarSupported			2124
#define FreeRDP_ReceivedCapabilities				2240
#define FreeRDP_ReceivedCapabilitiesSize			2241
#define FreeRDP_OsMajorType					2304
#define FreeRDP_OsMinorType					2305
#define FreeRDP_RefreshRect					2306
#define FreeRDP_SuppressOutput					2307
#define FreeRDP_FastPathOutput					2308
#define FreeRDP_SaltedChecksum					2309
#define FreeRDP_LongCredentialsSupported			2310
#define FreeRDP_NoBitmapCompressionHeader			2311
#define FreeRDP_BitmapCompressionDisabled			2312
#define FreeRDP_DesktopResize					2368
#define FreeRDP_DrawAllowDynamicColorFidelity			2369
#define FreeRDP_DrawAllowColorSubsampling			2370
#define FreeRDP_DrawAllowSkipAlpha				2371
#define FreeRDP_OrderSupport					2432
#define FreeRDP_BitmapCacheV3Enabled				2433
#define FreeRDP_AltSecFrameMarkerSupport			2434
#define FreeRDP_BitmapCacheEnabled				2497
#define FreeRDP_BitmapCacheVersion				2498
#define FreeRDP_AllowCacheWaitingList				2499
#define FreeRDP_BitmapCachePersistEnabled			2500
#define FreeRDP_BitmapCacheV2NumCells				2501
#define FreeRDP_BitmapCacheV2CellInfo				2502
#define FreeRDP_ColorPointerFlag				2560
#define FreeRDP_PointerCacheSize				2561
#define FreeRDP_KeyboardLayout					2624
#define FreeRDP_KeyboardType					2625
#define FreeRDP_KeyboardSubType					2626
#define FreeRDP_KeyboardFunctionKey				2627
#define FreeRDP_ImeFileName					2628
#define FreeRDP_UnicodeInput					2629
#define FreeRDP_FastPathInput					2630
#define FreeRDP_MultiTouchInput					2631
#define FreeRDP_MultiTouchGestures				2632
#define FreeRDP_BrushSupportLevel				2688
#define FreeRDP_GlyphSupportLevel				2752
#define FreeRDP_GlyphCache					2753
#define FreeRDP_FragCache					2754
#define FreeRDP_OffscreenSupportLevel				2816
#define FreeRDP_OffscreenCacheSize				2817
#define FreeRDP_OffscreenCacheEntries				2818
#define FreeRDP_VirtualChannelCompressionFlags			2880
#define FreeRDP_VirtualChannelChunkSize				2881
#define FreeRDP_SoundBeepsEnabled				2944
#define FreeRDP_MultifragMaxRequestSize				3328
#define FreeRDP_LargePointerFlag				3392
#define FreeRDP_CompDeskSupportLevel				3456
#define FreeRDP_SurfaceCommandsEnabled				3520
#define FreeRDP_FrameMarkerCommandEnabled			3521
#define FreeRDP_RemoteFxOnly					3648
#define FreeRDP_RemoteFxCodec					3649
#define FreeRDP_RemoteFxCodecId					3650
#define FreeRDP_RemoteFxCodecMode				3651
#define FreeRDP_RemoteFxImageCodec				3652
#define FreeRDP_RemoteFxCaptureFlags				3653
#define FreeRDP_NSCodec						3712
#define FreeRDP_NSCodecId					3713
#define FreeRDP_FrameAcknowledge				3714
#define FreeRDP_JpegCodec					3776
#define FreeRDP_JpegCodecId					3777
#define FreeRDP_JpegQuality					3778
#define FreeRDP_BitmapCacheV3CodecId				3904
#define FreeRDP_DrawNineGridEnabled				3968
#define FreeRDP_DrawNineGridCacheSize				3969
#define FreeRDP_DrawNineGridCacheEntries			3970
#define FreeRDP_DrawGdiPlusEnabled				4032
#define FreeRDP_DrawGdiPlusCacheEnabled				4033
#define FreeRDP_DeviceRedirection				4160
#define FreeRDP_DeviceCount					4161
#define FreeRDP_DeviceArraySize					4162
#define FreeRDP_DeviceArray					4163
#define FreeRDP_RedirectDrives					4288
#define FreeRDP_RedirectHomeDrive				4289
#define FreeRDP_DrivesToRedirect				4290
#define FreeRDP_RedirectSmartCards				4416
#define FreeRDP_RedirectPrinters				4544
#define FreeRDP_RedirectSerialPorts				4672
#define FreeRDP_RedirectParallelPorts				4673
#define FreeRDP_RedirectClipboard				4800
#define FreeRDP_StaticChannelCount				4928
#define FreeRDP_StaticChannelArraySize				4929
#define FreeRDP_StaticChannelArray				4930
#define FreeRDP_DynamicChannelCount				5056
#define FreeRDP_DynamicChannelArraySize				5057
#define FreeRDP_DynamicChannelArray				5058

/**
 * FreeRDP Settings Data Structure
 */

struct rdp_settings
{
	/**
	 * WARNING: this data structure is carefully padded for ABI stability!
	 * Keeping this area clean is particularly challenging, so unless you are
	 * a trusted developer you should NOT take the liberty of adding your own
	 * options straight into the ABI stable zone. Instead, append them to the
	 * very end of this data structure, in the zone marked as ABI unstable.
	 */

	ALIGN64 void* instance; /* 0 */
	UINT64 padding001[16 - 1]; /* 1 */

	/* Core Parameters */
	ALIGN64 BOOL ServerMode; /* 16 */
	ALIGN64 UINT32 ShareId; /* 17 */
	ALIGN64 UINT32 PduSource; /* 18 */
	ALIGN64 UINT32 ServerPort; /* 19 */
	ALIGN64 char* ServerHostname; /* 20 */
	ALIGN64 char* Username; /* 21 */
	ALIGN64 char* Password; /* 22 */
	ALIGN64 char* Domain; /* 23 */
	UINT64 padding0064[64 - 24]; /* 24 */
	UINT64 padding0128[128 - 64]; /* 64 */

	/**
	 * GCC User Data Blocks
	 */

	/* Client/Server Core Data */
	ALIGN64 UINT32 RdpVersion; /* 128 */
	ALIGN64 UINT32 DesktopWidth; /* 129 */
	ALIGN64 UINT32 DesktopHeight; /* 130 */
	ALIGN64 UINT32 ColorDepth; /* 131 */
	ALIGN64 UINT32 ConnectionType; /* 132 */
	ALIGN64 UINT32 ClientBuild; /* 133 */
	ALIGN64 char* ClientHostname; /* 134 */
	ALIGN64 char* ClientProductId; /* 135 */
	ALIGN64 UINT32 EarlyCapabilityFlags; /* 136 */
	ALIGN64 BOOL NetworkAutoDetect; /* 137 */
	ALIGN64 BOOL SupportAsymetricKeys; /* 138 */
	ALIGN64 BOOL SupportErrorInfoPdu; /* 139 */
	ALIGN64 BOOL SupportStatusInfoPdu; /* 140 */
	ALIGN64 BOOL SupportMonitorLayoutPdu; /* 141 */
	ALIGN64 BOOL SupportGraphicsPipeline; /* 142 */
	ALIGN64 BOOL SupportDynamicTimeZone; /* 143 */
	UINT64 padding0192[192 - 143]; /* 143 */

	/* Client/Server Security Data */
	ALIGN64 BOOL DisableEncryption; /* 192 */
	ALIGN64 UINT32 EncryptionMethods; /* 193 */
	ALIGN64 UINT32 ExtEncryptionMethods; /* 194 */
	ALIGN64 UINT32 EncryptionLevel; /* 195 */
	ALIGN64 BYTE* ServerRandom; /* 196 */
	ALIGN64 DWORD ServerRandomLength; /* 197 */
	ALIGN64 BYTE* ServerCertificate; /* 198 */
	ALIGN64 DWORD ServerCertificateLength; /* 199 */
	UINT64 padding0256[256 - 200]; /* 200 */

	/* Client Network Data */
	ALIGN64 UINT32 ChannelCount; /* 256 */
	ALIGN64 UINT32 ChannelDefArraySize; /* 257 */
	ALIGN64 rdpChannel* ChannelDefArray; /* 258 */
	UINT64 padding0320[320 - 259]; /* 259 */

	/* Client Cluster Data */
	ALIGN64 UINT32 ClusterInfoFlags; /* 320 */
	ALIGN64 UINT32 RedirectedSessionId; /* 321 */
	ALIGN64 BOOL ConsoleSession; /* 322 */
	UINT64 padding0384[384 - 323]; /* 323 */

	/* Client Monitor Data */
	ALIGN64 int MonitorCount; /* 384 */
	ALIGN64 UINT32 MonitorDefArraySize; /* 385 */
	ALIGN64 rdpMonitor* MonitorDefArray; /* 386 */
	ALIGN64 BOOL SpanMonitors; /* 387 */
	ALIGN64 BOOL UseMultimon; /* 388 */
	ALIGN64 BOOL ForceMultimon; /* 389 */
	ALIGN64 UINT32 DesktopPosX; /* 390 */
	ALIGN64 UINT32 DesktopPosY; /* 391 */
	ALIGN64 BOOL ListMonitors; /* 392 */
	ALIGN64 UINT32* MonitorIds; /* 393 */
	ALIGN64 UINT32 NumMonitorIds; /* 394 */
	UINT64 padding0448[448 - 395]; /* 395 */

	/* Client Message Channel Data */
	UINT64 padding0512[512 - 448]; /* 448 */

	/* Client Multitransport Channel Data */
	ALIGN64 UINT32 MultitransportFlags; /* 512 */
	UINT64 padding0576[576 - 513]; /* 513 */
	UINT64 padding0640[640 - 576]; /* 576 */

	/*
	 * Client Info
	 */

	/* Client Info (Shell) */
	ALIGN64 char* AlternateShell; /* 640 */
	ALIGN64 char* ShellWorkingDirectory; /* 641 */
	UINT64 padding0704[704 - 642]; /* 642 */

	/* Client Info Flags */
	ALIGN64 BOOL AutoLogonEnabled; /* 704 */
	ALIGN64 BOOL CompressionEnabled; /* 705 */
	ALIGN64 BOOL DisableCtrlAltDel; /* 706 */
	ALIGN64 BOOL EnableWindowsKey; /* 707 */
	ALIGN64 BOOL MaximizeShell; /* 708 */
	ALIGN64 BOOL LogonNotify; /* 709 */
	ALIGN64 BOOL LogonErrors; /* 710 */
	ALIGN64 BOOL MouseAttached; /* 711 */
	ALIGN64 BOOL MouseHasWheel; /* 712 */
	ALIGN64 BOOL RemoteConsoleAudio; /* 713 */
	ALIGN64 BOOL AudioPlayback; /* 714 */
	ALIGN64 BOOL AudioCapture; /* 715 */
	ALIGN64 BOOL VideoDisable; /* 716 */
	ALIGN64 BOOL PasswordIsSmartcardPin; /* 717 */
	ALIGN64 BOOL UsingSavedCredentials; /* 718 */
	ALIGN64 BOOL ForceEncryptedCsPdu; /* 719 */
	UINT64 padding0768[768 - 720]; /* 720 */

	/* Client Info (Extra) */
	ALIGN64 BOOL IPv6Enabled; /* 768 */
	ALIGN64 char* ClientAddress; /* 769 */
	ALIGN64 char* ClientDir; /* 770 */
	UINT64 padding0832[832 - 771]; /* 771 */

	/* Client Info (Auto Reconnection) */
	ALIGN64 BOOL AutoReconnectionEnabled; /* 832 */
	ALIGN64 UINT32 AutoReconnectMaxRetries; /* 833 */
	ALIGN64 ARC_CS_PRIVATE_PACKET* ClientAutoReconnectCookie; /* 834 */
	ALIGN64 ARC_SC_PRIVATE_PACKET* ServerAutoReconnectCookie; /* 835 */
	UINT64 padding0896[896 - 835]; /* 835 */

	/* Client Info (Time Zone) */
	ALIGN64 TIME_ZONE_INFO* ClientTimeZone; /* 896 */
	ALIGN64 char* DynamicDSTTimeZoneKeyName; /* 897 */
	ALIGN64 BOOL DynamicDaylightTimeDisabled; /* 898 */
	UINT64 padding0960[960 - 899]; /* 899 */

	/* Client Info (Performance Flags) */
	ALIGN64 UINT32 PerformanceFlags; /* 960 */
	ALIGN64 BOOL AllowFontSmoothing; /* 961 */
	ALIGN64 BOOL DisableWallpaper; /* 962 */
	ALIGN64 BOOL DisableFullWindowDrag; /* 963 */
	ALIGN64 BOOL DisableMenuAnims; /* 964 */
	ALIGN64 BOOL DisableThemes; /* 965 */
	ALIGN64 BOOL DisableCursorShadow; /* 966 */
	ALIGN64 BOOL DisableCursorBlinking; /* 967 */
	ALIGN64 BOOL AllowDesktopComposition; /* 968 */
	UINT64 padding1024[1024 - 969]; /* 969 */
	UINT64 padding1088[1088 - 1024]; /* 1024 */

	/**
	 * X.224 Connection Request/Confirm
	 */

	/* Protocol Security */
	ALIGN64 BOOL TlsSecurity; /* 1088 */
	ALIGN64 BOOL NlaSecurity; /* 1089 */
	ALIGN64 BOOL RdpSecurity; /* 1090 */
	ALIGN64 BOOL ExtSecurity; /* 1091 */
	ALIGN64 BOOL Authentication; /* 1092 */
	ALIGN64 UINT32 RequestedProtocols; /* 1093 */
	ALIGN64 UINT32 SelectedProtocol; /* 1094 */
	ALIGN64 UINT32 NegotiationFlags; /* 1095 */
	ALIGN64 BOOL NegotiateSecurityLayer; /* 1096 */
	UINT64 padding1152[1152 - 1097]; /* 1097 */

	/* Connection Cookie */
	ALIGN64 BOOL MstscCookieMode; /* 1152 */
	ALIGN64 UINT32 CookieMaxLength; /* 1153 */
	ALIGN64 UINT32 PreconnectionId; /* 1154 */
	ALIGN64 char* PreconnectionBlob; /* 1155 */
	ALIGN64 BOOL SendPreconnectionPdu; /* 1156 */
	UINT64 padding1216[1216 - 1057]; /* 1157 */

	/* Server Redirection */
	ALIGN64 UINT32 RedirectionFlags; /* 1216 */
	ALIGN64 BYTE* LoadBalanceInfo; /* 1217 */
	ALIGN64 UINT32 LoadBalanceInfoLength; /* 1218 */
	ALIGN64 BYTE* RedirectionUsername; /* 1219 */
	ALIGN64 UINT32 RedirectionUsernameLength; /* 1220 */
	ALIGN64 BYTE* RedirectionDomain; /* 1221 */
	ALIGN64 UINT32 RedirectionDomainLength; /* 1222 */
	ALIGN64 BYTE* RedirectionPassword; /* 1223 */
	ALIGN64 UINT32 RedirectionPasswordLength; /* 1224 */
	ALIGN64 BYTE* RedirectionTargetFQDN; /* 1225 */
	ALIGN64 UINT32 RedirectionTargetFQDNLength; /* 1226 */
	ALIGN64 BYTE* RedirectionTargetNetBiosName; /* 1227 */
	ALIGN64 UINT32 RedirectionTargetNetBiosNameLength; /* 1228 */
	ALIGN64 BYTE* RedirectionTsvUrl; /* 1229 */
	ALIGN64 UINT32 RedirectionTsvUrlLength; /* 1230 */
	ALIGN64 UINT32 TargetNetAddressCount; /* 1231 */
	ALIGN64 TARGET_NET_ADDRESS* TargetNetAddresses; /* 1232 */
	UINT64 padding1280[1280 - 1233]; /* 1233 */

	/**
	 * Security
	 */

	/* Credentials Cache */
	ALIGN64 BYTE* Password51; /* 1280 */
	ALIGN64 DWORD Password51Length; /* 1281 */
	UINT64 padding1344[1344 - 1282]; /* 1282 */

	/* Kerberos Authentication */
	ALIGN64 char* KerberosKdc; /* 1344 */
	ALIGN64 char* KerberosRealm; /* 1345 */
	UINT64 padding1408[1408 - 1346]; /* 1346 */

	/* Server Certificate */
	ALIGN64 BOOL IgnoreCertificate; /* 1408 */
	ALIGN64 char* CertificateName; /* 1409 */
	ALIGN64 char* CertificateFile; /* 1410 */
	ALIGN64 char* PrivateKeyFile; /* 1411 */
	ALIGN64 char* RdpKeyFile; /* 1412 */
	ALIGN64 rdpRsaKey* RdpServerRsaKey; /* 1413 */
	ALIGN64 rdpCertificate* RdpServerCertificate; /* 1414 */
	UINT64 padding1472[1472 - 1350]; /* 1415 */
	UINT64 padding1536[1536 - 1472]; /* 1472 */

	/**
	 * User Interface
	 */

	/* Window Settings */
	ALIGN64 BOOL Workarea; /* 1536 */
	ALIGN64 BOOL Fullscreen; /* 1537 */
	ALIGN64 UINT32 PercentScreen; /* 1538 */
	ALIGN64 BOOL GrabKeyboard; /* 1539 */
	ALIGN64 BOOL Decorations; /* 1540 */
	ALIGN64 BOOL MouseMotion; /* 1541 */
	ALIGN64 char* WindowTitle; /* 1542 */
	ALIGN64 UINT64 ParentWindowId; /* 1543 */
	ALIGN64 BOOL AsyncInput; /* 1544 */
	ALIGN64 BOOL AsyncUpdate; /* 1545 */
	ALIGN64 BOOL AsyncChannels; /* 1546 */
	ALIGN64 BOOL AsyncTransport; /* 1547 */
	ALIGN64 BOOL ToggleFullscreen; /* 1548 */
	ALIGN64 char* WmClass; /* 1549 */
	ALIGN64 BOOL EmbeddedWindow; /* 1550 */
	ALIGN64 BOOL SmartSizing; /* 1551 */
	ALIGN64 int XPan; /* 1552 */
	ALIGN64 int YPan; /* 1553 */
	ALIGN64 double ScalingFactor; /* 1554 */
	UINT64 padding1600[1600 - 1555]; /* 1555 */

	/* Miscellaneous */
	ALIGN64 BOOL SoftwareGdi; /* 1601 */
	ALIGN64 BOOL LocalConnection; /* 1602 */
	ALIGN64 BOOL AuthenticationOnly; /* 1603 */
	ALIGN64 BOOL CredentialsFromStdin; /* 1604 */
	UINT64 padding1664[1664 - 1605]; /* 1605 */

	/* Names */
	ALIGN64 char* ComputerName; /* 1664 */
	UINT64 padding1728[1728 - 1605]; /* 1665 */

	/* Files */
	ALIGN64 char* ConnectionFile; /* 1728 */
	UINT64 padding1792[1792 - 1729]; /* 1729 */

	/* Paths */
	ALIGN64 char* HomePath; /* 1792 */
	ALIGN64 char* ConfigPath; /* 1793 */
	ALIGN64 char* CurrentPath; /* 1794 */
	UINT64 padding1856[1856 - 1795]; /* 1795 */

	/* Recording */
	ALIGN64 BOOL DumpRemoteFx; /* 1856 */
	ALIGN64 BOOL PlayRemoteFx; /* 1857 */
	ALIGN64 char* DumpRemoteFxFile; /* 1858 */
	ALIGN64 char* PlayRemoteFxFile; /* 1859 */
	UINT64 padding1920[1920 - 1860]; /* 1860 */
	UINT64 padding1984[1984 - 1920]; /* 1920 */

	/**
	 * Gateway
	 */

	/* Gateway */
	ALIGN64 UINT32 GatewayUsageMethod; /* 1984 */
	ALIGN64 UINT32 GatewayPort; /* 1985 */
	ALIGN64 char* GatewayHostname; /* 1986 */
	ALIGN64 char* GatewayUsername; /* 1987 */
	ALIGN64 char* GatewayPassword; /* 1988 */
	ALIGN64 char* GatewayDomain; /* 1989 */
	ALIGN64 UINT32 GatewayCredentialsSource; /* 1990 */
	ALIGN64 BOOL GatewayUseSameCredentials; /* 1991 */
	ALIGN64 BOOL GatewayEnabled; /* 1992 */
	UINT64 padding2048[2048 - 1993]; /* 1993 */
	UINT64 padding2112[2112 - 2048]; /* 2048 */

	/**
	 * RemoteApp
	 */

	/* RemoteApp */
	ALIGN64 BOOL RemoteApplicationMode; /* 2112 */
	ALIGN64 char* RemoteApplicationName; /* 2113 */
	ALIGN64 char* RemoteApplicationIcon; /* 2114 */
	ALIGN64 char* RemoteApplicationProgram; /* 2115 */
	ALIGN64 char* RemoteApplicationFile; /* 2116 */
	ALIGN64 char* RemoteApplicationGuid; /* 2117 */
	ALIGN64 char* RemoteApplicationCmdLine; /* 2118 */
	ALIGN64 DWORD RemoteApplicationExpandCmdLine; /* 2119 */
	ALIGN64 DWORD RemoteApplicationExpandWorkingDir; /* 2120 */
	ALIGN64 BOOL DisableRemoteAppCapsCheck; /* 2121 */
	ALIGN64 UINT32 RemoteAppNumIconCaches; /* 2122 */
	ALIGN64 UINT32 RemoteAppNumIconCacheEntries; /* 2123 */
	ALIGN64 BOOL RemoteAppLanguageBarSupported; /* 2124 */
	UINT64 padding2176[2176 - 2125]; /* 2125 */
	UINT64 padding2240[2240 - 2124]; /* 2176 */

	/**
	 * Mandatory Capabilities
	 */

	/* Capabilities */
	ALIGN64 BYTE* ReceivedCapabilities; /* 2240 */
	ALIGN64 UINT32 ReceivedCapabilitiesSize; /* 2241 */
	UINT64 padding2304[2304 - 2242]; /* 2242 */

	/* General Capabilities */
	ALIGN64 UINT32 OsMajorType; /* 2304 */
	ALIGN64 UINT32 OsMinorType; /* 2305 */
	ALIGN64 BOOL RefreshRect; /* 2306 */
	ALIGN64 BOOL SuppressOutput; /* 2307 */
	ALIGN64 BOOL FastPathOutput; /* 2308 */
	ALIGN64 BOOL SaltedChecksum; /* 2309 */
	ALIGN64 BOOL LongCredentialsSupported; /* 2310 */
	ALIGN64 BOOL NoBitmapCompressionHeader; /* 2311 */
	ALIGN64 BOOL BitmapCompressionDisabled; /* 2312 */
	UINT64 padding2368[2368 - 2313]; /* 2313 */

	/* Bitmap Capabilities */
	ALIGN64 BOOL DesktopResize; /* 2368 */
	ALIGN64 BOOL DrawAllowDynamicColorFidelity; /* 2369 */
	ALIGN64 BOOL DrawAllowColorSubsampling; /* 2370 */
	ALIGN64 BOOL DrawAllowSkipAlpha; /* 2371 */
	UINT64 padding2432[2432 - 2372]; /* 2372 */

	/* Order Capabilities */
	ALIGN64 BYTE* OrderSupport; /* 2432 */
	ALIGN64 BOOL BitmapCacheV3Enabled; /* 2433 */
	ALIGN64 BOOL AltSecFrameMarkerSupport; /* 2434 */
	UINT64 padding2496[2496 - 2435]; /* 2435 */

	/* Bitmap Cache Capabilities */
	ALIGN64 BOOL BitmapCacheEnabled; /* 2497 */
	ALIGN64 UINT32 BitmapCacheVersion; /* 2498 */
	ALIGN64 BOOL AllowCacheWaitingList; /* 2499 */
	ALIGN64 BOOL BitmapCachePersistEnabled; /* 2500 */
	ALIGN64 UINT32 BitmapCacheV2NumCells; /* 2501 */
	ALIGN64 BITMAP_CACHE_V2_CELL_INFO* BitmapCacheV2CellInfo; /* 2502 */
	UINT64 padding2560[2560 - 2503]; /* 2503 */

	/* Pointer Capabilities */
	ALIGN64 BOOL ColorPointerFlag; /* 2560 */
	ALIGN64 UINT32 PointerCacheSize; /* 2561 */
	UINT64 padding2624[2624 - 2562]; /* 2562 */

	/* Input Capabilities */
	ALIGN64 UINT32 KeyboardLayout; /* 2624 */
	ALIGN64 UINT32 KeyboardType; /* 2625 */
	ALIGN64 UINT32 KeyboardSubType; /* 2626 */
	ALIGN64 UINT32 KeyboardFunctionKey; /* 2627 */
	ALIGN64 char* ImeFileName; /* 2628 */
	ALIGN64 BOOL UnicodeInput; /* 2629 */
	ALIGN64 BOOL FastPathInput; /* 2630 */
	ALIGN64 BOOL MultiTouchInput; /* 2631 */
	ALIGN64 BOOL MultiTouchGestures; /* 2632 */
	UINT64 padding2688[2688 - 2633]; /* 2633 */

	/* Brush Capabilities */
	ALIGN64 UINT32 BrushSupportLevel; /* 2688 */
	UINT64 padding2752[2752 - 2689]; /* 2689 */

	/* Glyph Cache Capabilities */
	ALIGN64 UINT32 GlyphSupportLevel; /* 2752 */
	ALIGN64 GLYPH_CACHE_DEFINITION* GlyphCache; /* 2753 */
	ALIGN64 GLYPH_CACHE_DEFINITION* FragCache; /* 2754 */
	UINT64 padding2816[2816 - 2755]; /* 2755 */

	/* Offscreen Bitmap Cache */
	ALIGN64 UINT32 OffscreenSupportLevel; /* 2816 */
	ALIGN64 UINT32 OffscreenCacheSize; /* 2817 */
	ALIGN64 UINT32 OffscreenCacheEntries; /* 2818 */
	UINT64 padding2880[2880 - 2819]; /* 2819 */

	/* Virtual Channel Capabilities */
	ALIGN64 UINT32 VirtualChannelCompressionFlags; /* 2880 */
	ALIGN64 UINT32 VirtualChannelChunkSize; /* 2881 */
	UINT64 padding2944[2944 - 2882]; /* 2882 */

	/* Sound Capabilities */
	ALIGN64 BOOL SoundBeepsEnabled; /* 2944 */
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
	UINT64 padding3328[3328 - 3200]; /* 3264 */

	/* Multifragment Update Capabilities */
	ALIGN64 UINT32 MultifragMaxRequestSize; /* 3328 */
	UINT64 padding3392[3392 - 3329]; /* 3329 */

	/* Large Pointer Update Capabilities */
	ALIGN64 UINT32 LargePointerFlag; /* 3392 */
	UINT64 padding3456[3456 - 3393]; /* 3393 */

	/* Desktop Composition Capabilities */
	ALIGN64 UINT32 CompDeskSupportLevel; /* 3456 */
	UINT64 padding3520[3520 - 3457]; /* 3457 */

	/* Surface Commands Capabilities */
	ALIGN64 BOOL SurfaceCommandsEnabled; /* 3520 */
	ALIGN64 BOOL FrameMarkerCommandEnabled; /* 3521 */
	UINT64 padding3584[3584 - 3522]; /* 3522 */
	UINT64 padding3648[3648 - 3584]; /* 3584 */

	/*
	 * Bitmap Codecs Capabilities
	 */

	/* RemoteFX */
	ALIGN64 BOOL RemoteFxOnly; /* 3648 */
	ALIGN64 BOOL RemoteFxCodec; /* 3649 */
	ALIGN64 UINT32 RemoteFxCodecId; /* 3650 */
	ALIGN64 UINT32 RemoteFxCodecMode; /* 3651 */
	ALIGN64 BOOL RemoteFxImageCodec; /* 3652 */
	ALIGN64 UINT32 RemoteFxCaptureFlags; /* 3653 */
	UINT64 padding3712[3712 - 3654]; /* 3654 */

	/* NSCodec */
	ALIGN64 BOOL NSCodec; /* 3712 */
	ALIGN64 UINT32 NSCodecId; /* 3713 */
	ALIGN64 UINT32 FrameAcknowledge; /* 3714 */
	UINT64 padding3776[3776 - 3715]; /* 3715 */

	/* JPEG */
	ALIGN64 BOOL JpegCodec; /* 3776 */
	ALIGN64 UINT32 JpegCodecId; /* 3777 */
	ALIGN64 UINT32 JpegQuality; /* 3778 */
	UINT64 padding3840[3840 - 3779]; /* 3779 */
	UINT64 padding3904[3904 - 3779]; /* 3840 */

	/**
	 * Caches
	 */

	/* Bitmap Cache V3 */
	ALIGN64 UINT32 BitmapCacheV3CodecId; /* 3904 */
	UINT64 padding3968[3968 - 3905]; /* 3905 */

	/* Draw Nine Grid */
	ALIGN64 BOOL DrawNineGridEnabled; /* 3968 */
	ALIGN64 UINT32 DrawNineGridCacheSize; /* 3969 */
	ALIGN64 UINT32 DrawNineGridCacheEntries; /* 3970 */
	UINT64 padding4032[4032 - 3971]; /* 3971 */

	/* Draw GDI+ */
	ALIGN64 BOOL DrawGdiPlusEnabled; /* 4032 */
	ALIGN64 BOOL DrawGdiPlusCacheEnabled; /* 4033 */
	UINT64 padding4096[4096 - 4034]; /* 4034 */
	UINT64 padding4160[4160 - 4096]; /* 4096 */

	/**
	 * Device Redirection
	 */

	/* Device Redirection */
	ALIGN64 BOOL DeviceRedirection; /* 4160 */
	ALIGN64 UINT32 DeviceCount; /* 4161 */
	ALIGN64 UINT32 DeviceArraySize; /* 4162 */
	ALIGN64 RDPDR_DEVICE** DeviceArray; /* 4163 */
	UINT64 padding4288[4288 - 4164]; /* 4164 */

	/* Drive Redirection */
	ALIGN64 BOOL RedirectDrives; /* 4288 */
	ALIGN64 BOOL RedirectHomeDrive; /* 4289 */
	ALIGN64 char* DrivesToRedirect; /* 4290 */
	UINT64 padding4416[4416 - 4291]; /* 4291 */

	/* Smartcard Redirection */
	ALIGN64 BOOL RedirectSmartCards; /* 4416 */
	UINT64 padding4544[4544 - 4417]; /* 4417 */

	/* Printer Redirection */
	ALIGN64 BOOL RedirectPrinters; /* 4544 */
	UINT64 padding4672[4672 - 4545]; /* 4545 */

	/* Serial and Parallel Port Redirection */
	ALIGN64 BOOL RedirectSerialPorts; /* 4672 */
	ALIGN64 BOOL RedirectParallelPorts; /* 4673 */
	UINT64 padding4800[4800 - 4674]; /* 4674 */

	/**
	 * Other Redirection
	 */

	ALIGN64 BOOL RedirectClipboard; /* 4800 */
	UINT64 padding4928[4928 - 4801]; /* 4801 */

	/**
	 * Static Virtual Channels
	 */

	ALIGN64 UINT32 StaticChannelCount; /* 4928 */
	ALIGN64 UINT32 StaticChannelArraySize; /* 4929 */
	ALIGN64 ADDIN_ARGV** StaticChannelArray; /* 4930 */
	UINT64 padding5056[5056 - 4931]; /* 4931 */

	/**
	 * Dynamic Virtual Channels
	 */

	ALIGN64 UINT32 DynamicChannelCount; /* 5056 */
	ALIGN64 UINT32 DynamicChannelArraySize; /* 5057 */
	ALIGN64 ADDIN_ARGV** DynamicChannelArray; /* 5058 */
	UINT64 padding5184[5184 - 5059]; /* 5059 */

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
	ALIGN64 int num_extensions; /*  */
	ALIGN64 struct rdp_ext_set extensions[16]; /*  */
	
	ALIGN64 BYTE* settings_modified; /* byte array marking fields that have been modified from their default value */
};
typedef struct rdp_settings rdpSettings;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API rdpSettings* freerdp_settings_new(void* instance);
FREERDP_API void freerdp_settings_free(rdpSettings* settings);

FREERDP_API int freerdp_addin_set_argument(ADDIN_ARGV* args, char* argument);
FREERDP_API int freerdp_addin_replace_argument(ADDIN_ARGV* args, char* previous, char* argument);
FREERDP_API int freerdp_addin_set_argument_value(ADDIN_ARGV* args, char* option, char* value);
FREERDP_API int freerdp_addin_replace_argument_value(ADDIN_ARGV* args, char* previous, char* option, char* value);

FREERDP_API void freerdp_device_collection_add(rdpSettings* settings, RDPDR_DEVICE* device);
FREERDP_API RDPDR_DEVICE* freerdp_device_collection_find(rdpSettings* settings, const char* name);
FREERDP_API void freerdp_device_collection_free(rdpSettings* settings);

FREERDP_API void freerdp_static_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel);
FREERDP_API ADDIN_ARGV* freerdp_static_channel_collection_find(rdpSettings* settings, const char* name);
FREERDP_API void freerdp_static_channel_collection_free(rdpSettings* settings);

FREERDP_API void freerdp_dynamic_channel_collection_add(rdpSettings* settings, ADDIN_ARGV* channel);
FREERDP_API ADDIN_ARGV* freerdp_dynamic_channel_collection_find(rdpSettings* settings, const char* name);
FREERDP_API void freerdp_dynamic_channel_collection_free(rdpSettings* settings);

FREERDP_API void freerdp_performance_flags_make(rdpSettings* settings);
FREERDP_API void freerdp_performance_flags_split(rdpSettings* settings);

FREERDP_API BOOL freerdp_get_param_bool(rdpSettings* settings, int id);
FREERDP_API int freerdp_set_param_bool(rdpSettings* settings, int id, BOOL param);

FREERDP_API int freerdp_get_param_int(rdpSettings* settings, int id);
FREERDP_API int freerdp_set_param_int(rdpSettings* settings, int id, int param);

FREERDP_API UINT32 freerdp_get_param_uint32(rdpSettings* settings, int id);
FREERDP_API int freerdp_set_param_uint32(rdpSettings* settings, int id, UINT32 param);

FREERDP_API UINT64 freerdp_get_param_uint64(rdpSettings* settings, int id);
FREERDP_API int freerdp_set_param_uint64(rdpSettings* settings, int id, UINT64 param);

FREERDP_API char* freerdp_get_param_string(rdpSettings* settings, int id);
FREERDP_API int freerdp_set_param_string(rdpSettings* settings, int id, char* param);

FREERDP_API double freerdp_get_param_double(rdpSettings* settings, int id);
FREERDP_API int freerdp_set_param_double(rdpSettings* settings, int id, double param);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SETTINGS_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Settings
 *
 * Copyright 2009-2011 Jay Sorg
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

#ifndef __RDP_SETTINGS_H
#define __RDP_SETTINGS_H

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
#define NEG_FAST_GLYPH_INDEX			0x18
#define NEG_ELLIPSE_SC_INDEX			0x19
#define NEG_ELLIPSE_CB_INDEX			0x1A
#define NEG_GLYPH_INDEX_INDEX			0x1B
#define NEG_GLYPH_WEXTTEXTOUT_INDEX		0x1C
#define NEG_GLYPH_WLONGTEXTOUT_INDEX		0x1D
#define NEG_GLYPH_WLONGEXTTEXTOUT_INDEX		0x1E

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

struct rdp_key
{
	BYTE* Modulus;
	DWORD ModulusLength;
	BYTE* PrivateExponent;
	DWORD PrivateExponentLength;
	BYTE exponent[4];
};
typedef struct rdp_key rdpKey;

/* Channels */

struct _RDPDR_DRIVE
{
	char* name;
	char* path;
};
typedef struct _RDPDR_DRIVE RDPDR_DRIVE;

struct rdp_channel
{
	char name[8]; /* ui sets */
	int options; /* ui sets */
	int channel_id; /* core sets */
	BOOL joined; /* client has joined the channel */
	void* handle; /* just for ui */
};
typedef struct rdp_channel rdpChannel;

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
	int x;
	int y;
	int width;
	int height;
	int is_primary;
};

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

struct rdp_settings
{
	ALIGN64 void* instance; /* 0 */
	UINT64 padding001[16 - 1]; /* 1 */

	/* Core Protocol Parameters */
	ALIGN64 UINT32 RdpVersion; /* 16 */
	ALIGN64 UINT32 DesktopWidth; /* 17 */
	ALIGN64 UINT32 DesktopHeight; /* 18 */
	ALIGN64 UINT32 ColorDepth; /* 19 */
	ALIGN64 UINT32 KeyboardLayout; /* 20 */
	ALIGN64 UINT32 KeyboardType; /* 21 */
	ALIGN64 UINT32 KeyboardSubType; /* 22 */
	ALIGN64 UINT32 KeyboardFunctionKey; /* 23 */
	ALIGN64 char* ImeFileName; /* 24 */
	ALIGN64 UINT32 ClientBuild; /* 25 */
	ALIGN64 char* ClientName; /* 26 */
	ALIGN64 char* ClientDigProductId; /* 27 */

	/* Protocol Security Negotiation */
	ALIGN64 UINT32 RequestedProtocols; /* 28 */
	ALIGN64 UINT32 SelectedProtocol; /* 29 */
	ALIGN64 UINT32 EncryptionMethod; /* 30 */
	ALIGN64 UINT32 EncryptionLevel; /* 31 */
	ALIGN64 BOOL Authentication; /* 32 */
	ALIGN64 UINT32 NegotiationFlags; /* 33 */
	ALIGN64 BOOL NegotiateSecurityLayer; /* 34 */
	UINT64 padding002[48 - 35]; /* 35 */

	/* Connection Settings */
	ALIGN64 BOOL IPv6; /* 48 */
	ALIGN64 UINT32 ServerPort; /* 49 */
	ALIGN64 char* Hostname; /* 50 */
	ALIGN64 char* Username; /* 51 */
	ALIGN64 char* Password; /* 52 */
	ALIGN64 char* Domain; /* 53 */
	ALIGN64 char* AlternateShell; /* 54 */
	ALIGN64 char* ShellWorkingDirectory; /* 55 */
	ALIGN64 char* ip_address; /* 56 */
	ALIGN64 char* client_dir; /* 57 */
	ALIGN64 BOOL autologon; /* 58 */
	ALIGN64 BOOL compression; /* 59 */
	ALIGN64 BYTE* Password51; /* 60 */
	ALIGN64 DWORD Password51Length; /* 61 */
	ALIGN64 BYTE* PasswordCookie; /* 62 */
	ALIGN64 DWORD PasswordCookieLength; /* 63 */
	ALIGN64 char* kerberos_kdc; /* 64 */
	ALIGN64 char* kerberos_realm; /* 65 */

	ALIGN64 BOOL local; /* 69 */
	ALIGN64 BOOL authentication_only; /* 70 */
	ALIGN64 BOOL from_stdin; /* 71 */
	ALIGN64 BOOL SendPreconnectionPdu; /* 72 */
	ALIGN64 UINT32 PreconnectionId; /* 73 */
	ALIGN64 char* PreconnectionBlob; /* 74 */
	ALIGN64 char* ComputerName; /* 75 */
	ALIGN64 char* ConnectionFile; /* 76 */
	UINT64 padding003[80 - 77]; /* 77 */

	/* User Interface Parameters */
	ALIGN64 BOOL sw_gdi; /* 80 */
	ALIGN64 BOOL workarea; /* 81 */
	ALIGN64 BOOL fullscreen; /* 82 */
	ALIGN64 BOOL grab_keyboard; /* 83 */
	ALIGN64 BOOL decorations; /* 84 */
	ALIGN64 UINT32 percent_screen; /* 85 */
	ALIGN64 BOOL mouse_motion; /* 86 */
	ALIGN64 char* window_title; /* 87 */
	ALIGN64 UINT64 parent_window_xid; /* 88 */
	UINT64 padding004[112 - 89]; /* 89 */

	/* Internal Parameters */
	ALIGN64 char* home_path; /* 112 */
	ALIGN64 UINT32 share_id; /* 113 */
	ALIGN64 UINT32 pdu_source; /* 114 */
	ALIGN64 BOOL server_mode; /* 116 */
	ALIGN64 char* config_path; /* 117 */
	ALIGN64 char* current_path; /* 118 */
	ALIGN64 char* development_path; /* 119 */
	ALIGN64 BOOL development_mode; /* 120 */
	UINT64 padding005[144 - 121]; /* 121 */

	/* Security */
	ALIGN64 BOOL Encryption; /* 144 */
	ALIGN64 BOOL TlsSecurity; /* 145 */
	ALIGN64 BOOL NlaSecurity; /* 146 */
	ALIGN64 BOOL RdpSecurity; /* 147 */
	ALIGN64 BOOL ExtSecurity; /* 148 */
	ALIGN64 BOOL SaltedChecksum; /* 149 */
	ALIGN64 BOOL MstscCookieMode; /* 150 */
	ALIGN64 UINT32 CookieMaxLength; /* 151 */
	UINT64 padding006[160 - 152]; /* 152 */

	/* Session */
	ALIGN64 BOOL ConsoleAudio; /* 160 */
	ALIGN64 BOOL ConsoleSession; /* 161 */
	ALIGN64 UINT32 RedirectedSessionId; /* 162 */
	ALIGN64 BOOL AudioPlayback; /* 163 */
	ALIGN64 BOOL AudioCapture; /* 164 */
	UINT64 padding007[176 - 165]; /* 165 */

	/* Output Control */
	ALIGN64 BOOL RefreshRect; /* 176 */
	ALIGN64 BOOL SuppressOutput; /* 177 */
	ALIGN64 BOOL DesktopResize; /* 178 */
	UINT64 padding008[192 - 179]; /* 179 */

	/* Reconnection */
	ALIGN64 BOOL AutoReconnectionEnabled; /* 192 */
	ALIGN64 UINT32 AutoReconnectMaxRetries; /* 193 */
	ALIGN64 ARC_CS_PRIVATE_PACKET* ClientAutoReconnectCookie; /* 194 */
	ALIGN64 ARC_SC_PRIVATE_PACKET* ServerAutoReconnectCookie; /* 195 */
	UINT64 padding009[208 - 196]; /* 196 */

	/* Time Zone */
	ALIGN64 TIME_ZONE_INFO* ClientTimeZone; /* 208 */
	UINT64 padding010[216 - 209]; /* 209 */

	/* Capabilities */
	ALIGN64 UINT32 OsMajorType; /* 216 */
	ALIGN64 UINT32 OsMinorType; /* 217 */
	ALIGN64 UINT32 VirtualChannelChunkSize; /* 218 */
	ALIGN64 BOOL SoundBeepsEnabled; /* 219 */
	ALIGN64 BOOL FrameMarkerEnabled; /* 221 */
	ALIGN64 BOOL FastpathInput; /* 222 */
	ALIGN64 BOOL FastpathOutput; /* 223 */
	ALIGN64 BYTE* ReceivedCapabilities; /* 224 */
	ALIGN64 BYTE* OrderSupport; /* 225 */
	ALIGN64 BOOL SurfaceCommandsEnabled; /* 226 */
	ALIGN64 UINT32 MultifragMaxRequestSize; /* 227 */

	ALIGN64 UINT32 PerformanceFlags; /* 220 */
	ALIGN64 UINT32 ConnectionType; /* 221 */
	ALIGN64 BOOL AllowFontSmoothing; /* 222 */
	ALIGN64 BOOL DisableWallpaper; /* 222 */
	ALIGN64 BOOL DisableFullWindowDrag; /* 228 */
	ALIGN64 BOOL DisableMenuAnims; /* 229 */
	ALIGN64 BOOL DisableThemes; /* 230 */
	ALIGN64 BOOL DisableCursorShadow; /* 231 */
	ALIGN64 BOOL DisableCursorBlinking; /* 232 */
	ALIGN64 BOOL AllowDesktopComposition; /* 233 */
	UINT64 padding011[248 - 233]; /* 235 */

	/* Certificate */
	ALIGN64 char* CertificateFile; /* 248 */
	ALIGN64 char* PrivateKeyFile; /* 249 */
	ALIGN64 char* ClientHostname; /* 250 */
	ALIGN64 char* ClientProductId; /* 251 */
	ALIGN64 BYTE* ServerRandom; /* 252 */
	ALIGN64 DWORD ServerRandomLength; /* 253 */
	ALIGN64 BYTE* ServerCertificate; /* 254 */
	ALIGN64 DWORD ServerCertificateLength; /* 255 */
	ALIGN64 BOOL IgnoreCertificate; /* 256 */
	ALIGN64 rdpCertificate* ServerCert; /* 257 */
	ALIGN64 char* RdpKeyFile; /* 258 */
	ALIGN64 rdpKey* ServerKey; /* 259 */
	ALIGN64 char* CertificateName; /* 260 */
	UINT64 padding012[280 - 261]; /* 261 */

	/* RemoteFX */
	ALIGN64 BOOL RemoteFxOnly; /* 290 */
	ALIGN64 BOOL RemoteFxCodec; /* 280 */
	ALIGN64 UINT32 RemoteFxCodecId; /* 282 */
	ALIGN64 UINT32 RemoteFxCodecMode; /* 284 */

	/* NSCodec */
	ALIGN64 BOOL NSCodec; /* 281 */
	ALIGN64 UINT32 NSCodecId; /* 283 */
	ALIGN64 BOOL FrameAcknowledge; /* 285 */

	/* JPEG */
	ALIGN64 BOOL JpegCodec; /* 286 */
	ALIGN64 UINT32 JpegCodecId; /* 287 */
	ALIGN64 UINT32 JpegQuality; /* 288 */

	UINT64 padding013[296 - 291]; /* 291 */

	/* Recording */
	ALIGN64 BOOL DumpRemoteFx; /* 296 */
	ALIGN64 BOOL PlayRemoteFx; /* 297 */
	ALIGN64 char* DumpRemoteFxFile; /* 298 */
	ALIGN64 char* PlayRemoteFxFile; /* 299 */
	UINT64 padding014[312 - 300]; /* 300 */

	/* RemoteApp */
	ALIGN64 BOOL RemoteApplicationMode; /* 312 */
	ALIGN64 char* RemoteApplicationName;
	ALIGN64 char* RemoteApplicationIcon;
	ALIGN64 char* RemoteApplicationProgram;
	ALIGN64 char* RemoteApplicationFile;
	ALIGN64 char* RemoteApplicationCmdLine;
	ALIGN64 DWORD RemoteApplicationExpandCmdLine;
	ALIGN64 DWORD RemoteApplicationExpandWorkingDir;
	ALIGN64 DWORD DisableRemoteAppCapsCheck;
	ALIGN64 UINT32 RemoteAppNumIconCaches; /* 313 */
	ALIGN64 UINT32 RemoteAppNumIconCacheEntries; /* 314 */
	ALIGN64 BOOL RemoteAppLanguageBarSupported; /* 315 */
	UINT64 padding015[320 - 316]; /* 316 */

	/* Pointer */
	ALIGN64 BOOL LargePointer; /* 320 */
	ALIGN64 BOOL ColorPointer; /* 321 */
	ALIGN64 UINT32 PointerCacheSize; /* 322 */
	UINT64 padding016[328 - 323]; /* 323 */

	/* Bitmap Cache */
	ALIGN64 BOOL BitmapCacheEnabled; /* 328 */
	ALIGN64 BOOL AllowCacheWaitingList; /* 333 */
	ALIGN64 BOOL BitmapCachePersistEnabled; /* 330 */
	ALIGN64 UINT32 BitmapCacheV2NumCells; /* 331 */
	ALIGN64 BITMAP_CACHE_V2_CELL_INFO* BitmapCacheV2CellInfo; /* 332 */
	UINT64 padding017[344 - 334]; /* 334 */

	/* Bitmap Cache V3 */
	ALIGN64 BOOL BitmapCacheV3Enabled; /* 329 */
	ALIGN64 UINT32 BitmapCacheV3CodecId; /* 289 */

	/* Offscreen Bitmap Cache */
	ALIGN64 BOOL OffscreenBitmapCacheEnabled; /* 344 */
	ALIGN64 UINT32 OffscreenBitmapCacheSize; /* 345 */
	ALIGN64 UINT32 OffscreenBitmapCacheEntries; /* 346 */
	UINT64 padding018[352 - 347]; /* 347 */

	/* Glyph Cache */
	ALIGN64 UINT32 GlyphSupportLevel; /* 353 */
	ALIGN64 GLYPH_CACHE_DEFINITION* GlyphCache; /* 354 */
	ALIGN64 GLYPH_CACHE_DEFINITION* FragCache; /* 355 */
	UINT64 padding019[360 - 356]; /* 356 */

	/* Draw Nine Grid */
	ALIGN64 BOOL DrawNineGridEnabled; /* 360 */
	ALIGN64 UINT32 DrawNineGridCacheSize; /* 361 */
	ALIGN64 UINT32 DrawNineGridCacheEntries; /* 362 */
	UINT64 padding020[368 - 363]; /* 363 */

	/* Draw GDI+ */
	ALIGN64 BOOL DrawGdiPlusEnabled; /* 368 */
	ALIGN64 BOOL DrawGdiPlusCacheEnabled; /* 369 */
	UINT64 padding021[376 - 370]; /* 370 */

	/* Desktop Composition */
	UINT64 padding022[384 - 377]; /* 377 */

	/* Gateway */
	ALIGN64 BOOL GatewayUsageMethod; /* 384 */
	ALIGN64 UINT32 GatewayPort; /* 385 */
	ALIGN64 char* GatewayHostname; /* 386 */
	ALIGN64 char* GatewayUsername; /* 387 */
	ALIGN64 char* GatewayPassword; /* 388 */
	ALIGN64 char* GatewayDomain; /* 389 */
	ALIGN64 UINT32 GatewayCredentialsSource; /* 390 */
	ALIGN64 BOOL GatewayUseSameCredentials; /* 391 */
	UINT64 padding023[400 - 391]; /* 392 */

	/* Device Redirection */
	ALIGN64 BOOL DeviceRedirection;

	/* Drive Redirection */
	ALIGN64 BOOL RedirectDrives;

	/* Smartcard Redirection */
	ALIGN64 BOOL RedirectSmartCards;

	/* Printer Redirection */
	ALIGN64 BOOL RedirectPrinters;

	/* Serial Port Redirection */
	ALIGN64 BOOL RedirectSerialPorts;

	/* Parallel Port Redirection */
	ALIGN64 BOOL RedirectParallelPorts;

	/* Channels */
	ALIGN64 int num_channels;
	ALIGN64 rdpChannel channels[16];

	/* Monitors */
	ALIGN64 int num_monitors;
	ALIGN64 struct rdp_monitor monitors[16];

	/* Extensions */
	ALIGN64 int num_extensions;
	ALIGN64 struct rdp_ext_set extensions[16];
};
typedef struct rdp_settings rdpSettings;

rdpSettings* settings_new(void* instance);
void settings_free(rdpSettings* settings);

#endif /* __RDP_SETTINGS_H */

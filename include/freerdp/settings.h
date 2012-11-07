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

	/**
	 * Section: Core
	 */

	/* Core Parameters */
	ALIGN64 BOOL ServerMode; /* 16 */
	ALIGN64 UINT32 ShareId; /* 17 */
	ALIGN64 UINT32 PduSource; /* 18 */
	ALIGN64 UINT32 RdpVersion; /* 19 */
	ALIGN64 UINT32 DesktopWidth; /* 20 */
	ALIGN64 UINT32 DesktopHeight; /* 21 */
	ALIGN64 UINT32 ColorDepth; /* 22 */
	ALIGN64 UINT32 ClientBuild; /* 28 */
	ALIGN64 char* ClientName; /* 29 */
	ALIGN64 char* ClientDigProductId; /* 30 */
	UINT64 padding0064[64 - 31]; /* 31 */

	/* Server Info */
	ALIGN64 UINT32 ServerPort; /* 65 */
	ALIGN64 char* ServerHostname; /* 66 */

	/* Client Info */
	ALIGN64 char* Username; /* 67 */
	ALIGN64 char* Password; /* 68 */
	ALIGN64 char* Domain; /* 69 */
	ALIGN64 char* AlternateShell; /* 54 */
	ALIGN64 char* ShellWorkingDirectory; /* 55 */
	UINT64 padding0128[128 - 70]; /* 70 */

	/* Client Info Flags */
	ALIGN64 BOOL AutoLogonEnabled; /* 58 */
	ALIGN64 BOOL CompressionEnabled; /* 59 */
	ALIGN64 BOOL DisableCtrlAltDel;
	ALIGN64 BOOL EnableWindowsKey;
	ALIGN64 BOOL MaximizeShell;
	ALIGN64 BOOL LogonNotify;
	ALIGN64 BOOL LogonErrors;
	ALIGN64 BOOL MouseAttached;
	ALIGN64 BOOL MouseHasWheel;
	ALIGN64 BOOL RemoteConsoleAudio;
	ALIGN64 BOOL AudioPlayback; /* 163 */
	ALIGN64 BOOL AudioCapture; /* 164 */
	ALIGN64 BOOL VideoDisable; /* 164 */
	ALIGN64 BOOL PasswordIsSmartcardPin;
	ALIGN64 BOOL UsingSavedCredentials;
	ALIGN64 BOOL ForceEncryptedCsPdu;

	/* Client Info (Extra) */
	ALIGN64 BOOL IPv6Enabled; /* 64 */
	ALIGN64 char* ClientAddress; /* 56 */
	ALIGN64 char* ClientDir; /* 57 */

	/* Client Info (Time Zone) */
	ALIGN64 TIME_ZONE_INFO* ClientTimeZone; /* 208 */
	ALIGN64 char* DynamicDSTTimeZoneKeyName; /*  */
	ALIGN64 BOOL DynamicDaylightTimeDisabled; /*  */

	/* Reconnection */
	ALIGN64 BYTE* ReceivedCapabilities; /* 224 */
	ALIGN64 BOOL AutoReconnectionEnabled; /* 768 */
	ALIGN64 UINT32 AutoReconnectMaxRetries; /* 769 */
	ALIGN64 ARC_CS_PRIVATE_PACKET* ClientAutoReconnectCookie; /* 770 */
	ALIGN64 ARC_SC_PRIVATE_PACKET* ServerAutoReconnectCookie; /* 771 */
	ALIGN64 BYTE* PasswordCookie; /* 772 */
	ALIGN64 DWORD PasswordCookieLength; /* 773 */
	ALIGN64 BYTE* Password51; /* 774 */
	ALIGN64 DWORD Password51Length; /* 775 */
	UINT64 padding0832[832 - 776]; /* 776 */

	/* Performance Flags */
	ALIGN64 UINT32 PerformanceFlags; /* 128 */
	ALIGN64 UINT32 ConnectionType; /* 129 */
	ALIGN64 BOOL AllowFontSmoothing; /* 130 */
	ALIGN64 BOOL DisableWallpaper; /* 131 */
	ALIGN64 BOOL DisableFullWindowDrag; /* 132 */
	ALIGN64 BOOL DisableMenuAnims; /* 133 */
	ALIGN64 BOOL DisableThemes; /* 134 */
	ALIGN64 BOOL DisableCursorShadow; /* 135 */
	ALIGN64 BOOL DisableCursorBlinking; /* 136 */
	ALIGN64 BOOL AllowDesktopComposition; /* 137 */
	UINT64 padding0192[192 - 138]; /* 138 */

	/* Protocol Security */
	ALIGN64 BOOL TlsSecurity; /* 192 */
	ALIGN64 BOOL NlaSecurity; /* 193 */
	ALIGN64 BOOL RdpSecurity; /* 194 */
	ALIGN64 BOOL ExtSecurity; /* 195 */
	ALIGN64 BOOL Encryption; /* 196 */
	ALIGN64 BOOL Authentication; /* 197 */
	UINT64 padding0256[256 - 198]; /* 198 */

	/* Connection Cookie */
	ALIGN64 BOOL MstscCookieMode; /* 256 */
	ALIGN64 UINT32 CookieMaxLength; /* 257 */
	ALIGN64 UINT32 PreconnectionId; /* 258 */
	ALIGN64 char* PreconnectionBlob; /* 259 */
	ALIGN64 BOOL SendPreconnectionPdu; /* 260 */
	UINT64 padding0320[320 - 261]; /* 261 */

	/* Protocol Security Negotiation */
	ALIGN64 UINT32 RequestedProtocols; /* 320 */
	ALIGN64 UINT32 SelectedProtocol; /* 321 */
	ALIGN64 UINT32 EncryptionMethod; /* 322 */
	ALIGN64 UINT32 EncryptionLevel; /* 323 */
	ALIGN64 UINT32 NegotiationFlags; /* 324 */
	ALIGN64 BOOL NegotiateSecurityLayer; /* 325 */
	UINT64 padding0384[384 - 326]; /* 326 */

	UINT64 padding0448[448 - 384]; /* 384 */
	UINT64 padding0512[512 - 448]; /* 448 */

	/**
	 * Section: Gateway
	 */

	/* Gateway */
	ALIGN64 BOOL GatewayUsageMethod; /* 512 */
	ALIGN64 UINT32 GatewayPort; /* 513 */
	ALIGN64 char* GatewayHostname; /* 514 */
	ALIGN64 char* GatewayUsername; /* 515 */
	ALIGN64 char* GatewayPassword; /* 516 */
	ALIGN64 char* GatewayDomain; /* 517 */
	ALIGN64 UINT32 GatewayCredentialsSource; /* 518 */
	ALIGN64 BOOL GatewayUseSameCredentials; /* 519 */
	UINT64 padding0576[576 - 520]; /* 520 */

	UINT64 padding0640[640 - 576]; /* 576 */

	/**
	 * Section: RemoteApp
	 */

	/* RemoteApp */
	ALIGN64 BOOL RemoteApplicationMode; /* 640 */
	ALIGN64 char* RemoteApplicationName; /* 641 */
	ALIGN64 char* RemoteApplicationIcon; /* 642 */
	ALIGN64 char* RemoteApplicationProgram; /* 643 */
	ALIGN64 char* RemoteApplicationFile; /* 644 */
	ALIGN64 char* RemoteApplicationCmdLine; /* 645 */
	ALIGN64 DWORD RemoteApplicationExpandCmdLine; /* 646 */
	ALIGN64 DWORD RemoteApplicationExpandWorkingDir; /* 647 */
	ALIGN64 DWORD DisableRemoteAppCapsCheck; /* 648 */
	ALIGN64 UINT32 RemoteAppNumIconCaches; /* 649 */
	ALIGN64 UINT32 RemoteAppNumIconCacheEntries; /* 650 */
	ALIGN64 BOOL RemoteAppLanguageBarSupported; /* 651 */
	UINT64 padding0704[704 - 652]; /* 652 */

	UINT64 padding0768[768 - 704]; /* 704 */

	/**
	 * Section: Mandatory Capabilities
	 */

	/* General Capabilities */
	ALIGN64 UINT32 OsMajorType; /* 216 */
	ALIGN64 UINT32 OsMinorType; /* 217 */
	ALIGN64 BOOL RefreshRect; /* 176 */
	ALIGN64 BOOL SuppressOutput; /* 177 */
	ALIGN64 BOOL FastPathOutput; /* 223 */
	ALIGN64 BOOL SaltedChecksum; /* 197 */
	ALIGN64 BOOL LongCredentialsSupported; /* 197 */
	ALIGN64 BOOL NoBitmapCompressionHeader; /* 197 */

	/* Bitmap Capabilities */
	ALIGN64 BOOL DesktopResize; /* 178 */
	ALIGN64 BOOL DrawAllowDynamicColorFidelity; /* */
	ALIGN64 BOOL DrawAllowColorSubsampling; /* */
	ALIGN64 BOOL DrawAllowSkipAlpha; /* */

	/* Order Capabilities */
	ALIGN64 BYTE* OrderSupport; /* 225 */
	ALIGN64 BOOL AltSecFrameMarkerSupport; /* 221 */

	/* Bitmap Cache Capabilities */
	ALIGN64 UINT32 BitmapCacheVersion; /* 328 */
	ALIGN64 BOOL BitmapCacheEnabled; /* 328 */
	ALIGN64 BOOL AllowCacheWaitingList; /* 333 */
	ALIGN64 BOOL BitmapCachePersistEnabled; /* 330 */
	ALIGN64 UINT32 BitmapCacheV2NumCells; /* 331 */
	ALIGN64 BITMAP_CACHE_V2_CELL_INFO* BitmapCacheV2CellInfo; /* 332 */
	UINT64 padding017[344 - 334]; /* 334 */

	/* Pointer Capabilities */
	ALIGN64 BOOL ColorPointerFlag; /* 321 */
	ALIGN64 UINT32 PointerCacheSize; /* 322 */
	UINT64 padding016[328 - 323]; /* 323 */

	/* Input Capabilities */
	ALIGN64 UINT32 KeyboardLayout; /*  */
	ALIGN64 UINT32 KeyboardType; /*  */
	ALIGN64 UINT32 KeyboardSubType; /*  */
	ALIGN64 UINT32 KeyboardFunctionKey; /*  */
	ALIGN64 char* ImeFileName; /*  */
	ALIGN64 BOOL UnicodeInput; /*  */
	ALIGN64 BOOL FastPathInput; /*  */

	/* Brush Capabilities */
	ALIGN64 UINT32 BrushSupportLevel; /*  */

	/* Glyph Cache Capabilities */
	ALIGN64 UINT32 GlyphSupportLevel; /*  */
	ALIGN64 GLYPH_CACHE_DEFINITION* GlyphCache; /*  */
	ALIGN64 GLYPH_CACHE_DEFINITION* FragCache; /*  */

	/* Offscreen Bitmap Cache */
	ALIGN64 UINT32 OffscreenSupportLevel; /*  */
	ALIGN64 UINT32 OffscreenCacheSize; /*  */
	ALIGN64 UINT32 OffscreenCacheEntries; /*  */

	/* Virtual Channel Capabilities */
	ALIGN64 UINT32 VirtualChannelCompressionFlags; /*  */
	ALIGN64 UINT32 VirtualChannelChunkSize; /*  */

	/* Sound Capabilities */
	ALIGN64 BOOL SoundBeepsEnabled; /* 219 */

	/**
	 * Section: Optional Capabilities
	 */

	/* Bitmap Cache Host Capabilities */

	/* Control Capabilities */

	/* Window Activation Capabilities */

	/* Font Capabilities */

	/* Multifragment Update Capabilities */
	ALIGN64 UINT32 MultifragMaxRequestSize; /*  */

	/* Large Pointer Update Capabilities */
	ALIGN64 UINT32 LargePointerFlag; /*  */

	/* Desktop Composition Capabilities */
	ALIGN64 UINT32 CompDeskSupportLevel; /*  */

	/* Surface Commands Capabilities */
	ALIGN64 BOOL SurfaceCommandsEnabled; /* 226 */
	ALIGN64 BOOL FrameMarkerCommandEnabled; /* 226 */

	/* Bitmap Codecs Capabilities */

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

	/* Session */
	ALIGN64 BOOL ConsoleSession; /* 161 */
	ALIGN64 UINT32 RedirectedSessionId; /* 162 */
	UINT64 padding007[176 - 165]; /* 165 */

	/* Kerberos Authentication */
	ALIGN64 char* KerberosKdc; /* 64 */
	ALIGN64 char* KerberosRealm; /* 65 */

	/* Certificate */
	ALIGN64 char* ClientHostname; /* 250 */
	ALIGN64 char* ClientProductId; /* 251 */
	ALIGN64 BYTE* ServerRandom; /* 252 */
	ALIGN64 DWORD ServerRandomLength; /* 253 */
	ALIGN64 BYTE* ServerCertificate; /* 254 */
	ALIGN64 DWORD ServerCertificateLength; /* 255 */
	ALIGN64 BOOL IgnoreCertificate; /* 256 */
	ALIGN64 char* CertificateName; /* 260 */

	ALIGN64 char* CertificateFile; /* 248 */
	ALIGN64 char* PrivateKeyFile; /* 249 */
	ALIGN64 rdpCertificate* ServerCert; /* 257 */
	ALIGN64 char* RdpKeyFile; /* 258 */
	ALIGN64 rdpKey* ServerKey; /* 259 */

	ALIGN64 BOOL LocalConnection; /* 69 */
	ALIGN64 BOOL AuthenticationOnly; /* 70 */
	ALIGN64 BOOL CredentialsFromStdin; /* 71 */
	ALIGN64 char* ComputerName; /* 75 */
	ALIGN64 char* ConnectionFile; /* 76 */

	/* User Interface Parameters */
	ALIGN64 BOOL SoftwareGdi; /* 80 */
	ALIGN64 BOOL Workarea; /* 81 */
	ALIGN64 BOOL Fullscreen; /* 82 */
	ALIGN64 BOOL GrabKeyboard; /* 83 */
	ALIGN64 BOOL Decorations; /* 84 */
	ALIGN64 UINT32 PercentScreen; /* 85 */
	ALIGN64 BOOL MouseMotion; /* 86 */
	ALIGN64 char* WindowTitle; /* 87 */
	ALIGN64 UINT64 ParentWindowId; /* 88 */
	UINT64 padding004[112 - 89]; /* 89 */

	/* Paths */
	ALIGN64 char* HomePath; /* 112 */
	ALIGN64 char* ConfigPath; /* 117 */
	ALIGN64 char* CurrentPath; /* 118 */
	UINT64 padding005[144 - 121]; /* 121 */

	/**
	 * Section: Caches
	 */

	/* Bitmap Cache V3 */
	ALIGN64 BOOL BitmapCacheV3Enabled; /* 329 */
	ALIGN64 UINT32 BitmapCacheV3CodecId; /* 289 */

	/* Draw Nine Grid */
	ALIGN64 BOOL DrawNineGridEnabled; /* 360 */
	ALIGN64 UINT32 DrawNineGridCacheSize; /* 361 */
	ALIGN64 UINT32 DrawNineGridCacheEntries; /* 362 */
	UINT64 padding020[368 - 363]; /* 363 */

	/* Draw GDI+ */
	ALIGN64 BOOL DrawGdiPlusEnabled; /* 368 */
	ALIGN64 BOOL DrawGdiPlusCacheEnabled; /* 369 */
	UINT64 padding021[376 - 370]; /* 370 */

	/**
	 * Section: Device Redirection
	 */

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

	/**
	 * Section: Unstable
	 * Anything below this point is subject to ABI breakage
	 */

	/* Recording */
	ALIGN64 BOOL DumpRemoteFx; /* 296 */
	ALIGN64 BOOL PlayRemoteFx; /* 297 */
	ALIGN64 char* DumpRemoteFxFile; /* 298 */
	ALIGN64 char* PlayRemoteFxFile; /* 299 */
	UINT64 padding014[312 - 300]; /* 300 */

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

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
	ALIGN64 UINT32 requested_protocols; /* 28 */
	ALIGN64 UINT32 selected_protocol; /* 29 */
	ALIGN64 UINT32 encryption_method; /* 30 */
	ALIGN64 UINT32 encryption_level; /* 31 */
	ALIGN64 BOOL authentication; /* 32 */
	ALIGN64 UINT32 negotiationFlags; /* 33 */
	ALIGN64 BOOL security_layer_negotiation; /* 34 */
	UINT64 padding002[48 - 35]; /* 35 */

	/* Connection Settings */
	ALIGN64 UINT32 port; /* 48 */
	ALIGN64 BOOL ipv6; /* 49 */
	ALIGN64 char* hostname; /* 50 */
	ALIGN64 char* username; /* 51 */
	ALIGN64 char* password; /* 52 */
	ALIGN64 char* domain; /* 53 */
	ALIGN64 char* shell; /* 54 */
	ALIGN64 char* directory; /* 55 */
	ALIGN64 char* ip_address; /* 56 */
	ALIGN64 char* client_dir; /* 57 */
	ALIGN64 BOOL autologon; /* 58 */
	ALIGN64 BOOL compression; /* 59 */
	ALIGN64 UINT32 performance_flags; /* 60 */
	ALIGN64 BYTE* password_cookie; /* 61 */
	ALIGN64 DWORD password_cookie_length; /* 62 */
	ALIGN64 char* kerberos_kdc; /* 63 */
	ALIGN64 char* kerberos_realm; /* 64 */
	ALIGN64 void* unused65; /* 65 */
	ALIGN64 void* unused66; /* 66 */
	ALIGN64 void* unused67; /* 67 */
	ALIGN64 void* unused68; /* 68 */
	ALIGN64 BOOL local; /* 69 */
	ALIGN64 BOOL authentication_only; /* 70 */
	ALIGN64 BOOL from_stdin; /* 71 */
	ALIGN64 BOOL send_preconnection_pdu; /* 72 */
	ALIGN64 UINT32 preconnection_id; /* 73 */
	ALIGN64 char* preconnection_blob; /* 74 */
	ALIGN64 char* computer_name; /* 75 */
	ALIGN64 char* connection_file; /* 76 */
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
	ALIGN64 void* unused115; /* 115 */
	ALIGN64 BOOL server_mode; /* 116 */
	ALIGN64 char* config_path; /* 117 */
	ALIGN64 char* current_path; /* 118 */
	ALIGN64 char* development_path; /* 119 */
	ALIGN64 BOOL development_mode; /* 120 */
	UINT64 padding005[144 - 121]; /* 121 */

	/* Security */
	ALIGN64 BOOL encryption; /* 144 */
	ALIGN64 BOOL tls_security; /* 145 */
	ALIGN64 BOOL nla_security; /* 146 */
	ALIGN64 BOOL rdp_security; /* 147 */
	ALIGN64 BOOL ext_security; /* 148 */
	ALIGN64 UINT32 ntlm_version; /* 149 */
	ALIGN64 BOOL salted_checksum; /* 150 */
	ALIGN64 BOOL mstsc_cookie_mode; /* 151 */
	ALIGN64 UINT32 cookie_max_length; /* 152 */
	UINT64 padding006[160 - 153]; /* 153 */

	/* Session */
	ALIGN64 BOOL console_audio; /* 160 */
	ALIGN64 BOOL console_session; /* 161 */
	ALIGN64 UINT32 redirected_session_id; /* 162 */
	ALIGN64 BOOL audio_playback; /* 163 */
	ALIGN64 BOOL audio_capture; /* 164 */
	UINT64 padding007[176 - 165]; /* 165 */

	/* Output Control */
	ALIGN64 BOOL refresh_rect; /* 176 */
	ALIGN64 BOOL suppress_output; /* 177 */
	ALIGN64 BOOL desktop_resize; /* 178 */
	UINT64 padding008[192 - 179]; /* 179 */

	/* Reconnection */
	ALIGN64 BOOL auto_reconnection; /* 192 */
	ALIGN64 ARC_CS_PRIVATE_PACKET* client_auto_reconnect_cookie; /* 193 */
	ALIGN64 ARC_SC_PRIVATE_PACKET* server_auto_reconnect_cookie; /* 194 */
	UINT64 padding009[208 - 195]; /* 195 */

	/* Time Zone */
	ALIGN64 TIME_ZONE_INFO* client_time_zone; /* 208 */
	UINT64 padding010[216 - 209]; /* 209 */

	/* Capabilities */
	ALIGN64 UINT32 os_major_type; /* 216 */
	ALIGN64 UINT32 os_minor_type; /* 217 */
	ALIGN64 UINT32 vc_chunk_size; /* 218 */
	ALIGN64 BOOL sound_beeps; /* 219 */
	ALIGN64 BOOL smooth_fonts; /* 220 */
	ALIGN64 BOOL frame_marker; /* 221 */
	ALIGN64 BOOL fastpath_input; /* 222 */
	ALIGN64 BOOL fastpath_output; /* 223 */
	ALIGN64 BYTE* received_caps; /* 224 */
	ALIGN64 BYTE* order_support; /* 225 */
	ALIGN64 BOOL surface_commands; /* 226 */
	ALIGN64 BOOL disable_wallpaper; /* 227 */
	ALIGN64 BOOL disable_full_window_drag; /* 228 */
	ALIGN64 BOOL disable_menu_animations; /* 229 */
	ALIGN64 BOOL disable_theming; /* 230 */
	ALIGN64 UINT32 ConnectionType; /* 231 */
	ALIGN64 UINT32 multifrag_max_request_size; /* 232 */
	UINT64 padding011[248 - 233]; /* 233 */

	/* Certificate */
	ALIGN64 char* cert_file; /* 248 */
	ALIGN64 char* privatekey_file; /* 249 */
	ALIGN64 char* client_hostname; /* 250 */
	ALIGN64 char* client_product_id; /* 251 */
	ALIGN64 BYTE* server_random; /* 252 */
	ALIGN64 DWORD server_random_length; /* 253 */
	ALIGN64 BYTE* server_certificate; /* 254 */
	ALIGN64 DWORD server_certificate_length; /* 255 */
	ALIGN64 BOOL ignore_certificate; /* 256 */
	ALIGN64 rdpCertificate* server_cert; /* 257 */
	ALIGN64 char* rdp_key_file; /* 258 */
	ALIGN64 rdpKey* server_key; /* 259 */
	ALIGN64 char* certificate_name; /* 260 */
	UINT64 padding012[280 - 261]; /* 261 */

	/* Codecs */
	ALIGN64 BOOL rfx_codec; /* 280 */
	ALIGN64 BOOL ns_codec; /* 281 */
	ALIGN64 UINT32 rfx_codec_id; /* 282 */
	ALIGN64 UINT32 ns_codec_id; /* 283 */
	ALIGN64 UINT32 rfx_codec_mode; /* 284 */
	ALIGN64 BOOL frame_acknowledge; /* 285 */
	ALIGN64 BOOL jpeg_codec; /* 286 */
	ALIGN64 UINT32 jpeg_codec_id; /* 287 */
	ALIGN64 UINT32 jpeg_quality; /* 288 */
	ALIGN64 UINT32 v3_codec_id; /* 289 */
	ALIGN64 BOOL rfx_codec_only; /* 290 */
	UINT64 padding013[296 - 291]; /* 291 */

	/* Recording */
	ALIGN64 BOOL dump_rfx; /* 296 */
	ALIGN64 BOOL play_rfx; /* 297 */
	ALIGN64 char* dump_rfx_file; /* 298 */
	ALIGN64 char* play_rfx_file; /* 299 */
	UINT64 padding014[312 - 300]; /* 300 */

	/* RemoteApp */
	ALIGN64 BOOL remote_app; /* 312 */
	ALIGN64 UINT32 num_icon_caches; /* 313 */
	ALIGN64 UINT32 num_icon_cache_entries; /* 314 */
	ALIGN64 BOOL rail_langbar_supported; /* 315 */
	UINT64 padding015[320 - 316]; /* 316 */

	/* Pointer */
	ALIGN64 BOOL large_pointer; /* 320 */
	ALIGN64 BOOL color_pointer; /* 321 */
	ALIGN64 UINT32 pointer_cache_size; /* 322 */
	UINT64 padding016[328 - 323]; /* 323 */

	/* Bitmap Cache */
	ALIGN64 BOOL bitmap_cache; /* 328 */
	ALIGN64 BOOL bitmap_cache_v3; /* 329 */
	ALIGN64 BOOL persistent_bitmap_cache; /* 330 */
	ALIGN64 UINT32 bitmapCacheV2NumCells; /* 331 */
	ALIGN64 BITMAP_CACHE_V2_CELL_INFO* bitmapCacheV2CellInfo; /* 332 */
	ALIGN64 BOOL allow_cache_waiting_list; /* 333 */
	UINT64 padding017[344 - 334]; /* 334 */

	/* Offscreen Bitmap Cache */
	ALIGN64 BOOL offscreen_bitmap_cache; /* 344 */
	ALIGN64 UINT32 offscreen_bitmap_cache_size; /* 345 */
	ALIGN64 UINT32 offscreen_bitmap_cache_entries; /* 346 */
	UINT64 padding018[352 - 347]; /* 347 */

	/* Glyph Cache */
	ALIGN64 void* unused352; /* 352 */
	ALIGN64 UINT32 glyphSupportLevel; /* 353 */
	ALIGN64 GLYPH_CACHE_DEFINITION* glyphCache; /* 354 */
	ALIGN64 GLYPH_CACHE_DEFINITION* fragCache; /* 355 */
	UINT64 padding019[360 - 356]; /* 356 */

	/* Draw Nine Grid */
	ALIGN64 BOOL draw_nine_grid; /* 360 */
	ALIGN64 UINT32 draw_nine_grid_cache_size; /* 361 */
	ALIGN64 UINT32 draw_nine_grid_cache_entries; /* 362 */
	UINT64 padding020[368 - 363]; /* 363 */

	/* Draw GDI+ */
	ALIGN64 BOOL draw_gdi_plus; /* 368 */
	ALIGN64 BOOL draw_gdi_plus_cache; /* 369 */
	UINT64 padding021[376 - 370]; /* 370 */

	/* Desktop Composition */
	ALIGN64 BOOL desktop_composition; /* 376 */
	UINT64 padding022[384 - 377]; /* 377 */

	/* TS Gateway */
	ALIGN64 BOOL ts_gateway; /* 384 */
	ALIGN64 UINT32 tsg_port; /* 385 */
	ALIGN64 char* tsg_hostname; /* 386 */
	ALIGN64 char* tsg_username; /* 387 */
	ALIGN64 char* tsg_password; /* 388 */
	ALIGN64 char* tsg_domain; /* 389 */
	ALIGN64 BOOL tsg_same_credentials; /* 390 */
	UINT64 padding023[400 - 391]; /* 391 */

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

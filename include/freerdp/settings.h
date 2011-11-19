/**
 * FreeRDP: A Remote Desktop Protocol Client
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
#include <freerdp/utils/blob.h>
#include <freerdp/utils/unicode.h>

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
	uint16 wYear;
	uint16 wMonth;
	uint16 wDayOfWeek;
	uint16 wDay;
	uint16 wHour;
	uint16 wMinute;
	uint16 wSecond;
	uint16 wMilliseconds;
} SYSTEM_TIME;

/* TIME_ZONE_INFORMATION */
struct _TIME_ZONE_INFO
{
	uint32 bias;
	char standardName[32];
	SYSTEM_TIME standardDate;
	uint32 standardBias;
	char daylightName[32];
	SYSTEM_TIME daylightDate;
	uint32 daylightBias;
};
typedef struct _TIME_ZONE_INFO TIME_ZONE_INFO;

/* ARC_CS_PRIVATE_PACKET */
typedef struct
{
	uint32 cbLen;
	uint32 version;
	uint32 logonId;
	uint8 securityVerifier[16];
} ARC_CS_PRIVATE_PACKET;

/* ARC_SC_PRIVATE_PACKET */
typedef struct
{
	uint32 cbLen;
	uint32 version;
	uint32 logonId;
	uint8 arcRandomBits[16];
} ARC_SC_PRIVATE_PACKET;

struct rdp_channel
{
	char name[8]; /* ui sets */
	int options; /* ui sets */
	int channel_id; /* core sets */
	boolean joined; /* client has joined the channel */
	void* handle; /* just for ui */
};
typedef struct rdp_channel rdpChannel;

struct rdp_ext_set
{
	char name[256]; /* plugin name or path */
	void* data; /* plugin data */
};

struct _BITMAP_CACHE_CELL_INFO
{
	uint16 numEntries;
	uint16 maxSize;
};
typedef struct _BITMAP_CACHE_CELL_INFO BITMAP_CACHE_CELL_INFO;

struct _BITMAP_CACHE_V2_CELL_INFO
{
	uint32 numEntries;
	boolean persistent;
};
typedef struct _BITMAP_CACHE_V2_CELL_INFO BITMAP_CACHE_V2_CELL_INFO;

struct _GLYPH_CACHE_DEFINITION
{
	uint16 cacheEntries;
	uint16 cacheMaximumCellSize;
};
typedef struct _GLYPH_CACHE_DEFINITION GLYPH_CACHE_DEFINITION;

struct rdp_monitor
{
	int x;
	int y;
	int width;
	int height;
	int is_primary;
};

struct rdp_settings
{
	void* instance;
	uint32 paddingA[16 - 1]; /* offset 64 */

	/* Core Protocol Parameters */
	uint32 width;
	uint32 height;
	uint32 rdp_version;
	uint32 color_depth;
	uint32 kbd_layout;
	uint32 kbd_type;
	uint32 kbd_subtype;
	uint32 kbd_fn_keys;
	uint32 client_build;
	uint32 requested_protocols;
	uint32 selected_protocol;
	uint32 encryption_method;
	uint32 encryption_level;
	boolean authentication;
	uint32 paddingB[32 - 14]; /* offset 192 */

	/* Connection Settings */
	uint32 port;
	boolean ipv6;
	char* hostname;
	char* username;
	char* password;
	char* domain;
	char* shell;
	char* directory;
	char* ip_address;
	char* client_dir;
	boolean autologon;
	boolean compression;
	uint32 performance_flags;
	uint32 paddingC[32 - 13]; /* offset 320 */

	/* User Interface Parameters */
	boolean sw_gdi;
	boolean workarea;
	boolean fullscreen;
	boolean grab_keyboard;
	boolean decorations;
	uint32 percent_screen;
	boolean mouse_motion;
	uint32 paddingD[32 - 7]; /* offset 448 */

	/* Internal Parameters */
	char* home_path;
	uint32 share_id;
	uint32 pdu_source;
	UNICONV* uniconv;
	boolean server_mode;
	uint32 paddingE[32 - 5]; /* offset 576 */

	/* Session */
	boolean console_audio;
	boolean console_session;
	uint32 redirected_session_id;

	/* Security */
	boolean encryption;
	boolean tls_security;
	boolean nla_security;
	boolean rdp_security;

	/* Reconnection */
	boolean auto_reconnection;
	ARC_CS_PRIVATE_PACKET client_auto_reconnect_cookie;
	ARC_SC_PRIVATE_PACKET server_auto_reconnect_cookie;

	/* Output Control */
	boolean refresh_rect;
	boolean suppress_output;
	boolean desktop_resize;

	/* Time Zone */
	TIME_ZONE_INFO client_time_zone;

	/* Capabilities */
	uint32 vc_chunk_size;
	boolean sound_beeps;
	boolean smooth_fonts;
	boolean frame_marker;
	boolean fastpath_input;
	boolean fastpath_output;
	uint8 received_caps[32];
	uint8 order_support[32];
	boolean surface_commands;
	uint32 multifrag_max_request_size;

	/* Certificate */
	char* cert_file;
	char* privatekey_file;
	char client_hostname[32];
	char client_product_id[32];
	rdpBlob server_random;
	rdpBlob server_certificate;
	boolean ignore_certificate;
	struct rdp_certificate* server_cert;

	/* Codecs */
	boolean rfx_codec;
	boolean ns_codec;
	uint32 rfx_codec_id;
	uint32 ns_codec_id;
	boolean frame_acknowledge;

	/* Recording */
	boolean dump_rfx;
	boolean play_rfx;
	char* dump_rfx_file;
	char* play_rfx_file;

	/* RemoteApp */
	boolean remote_app;
	uint32 num_icon_caches;
	uint32 num_icon_cache_entries;
	boolean rail_langbar_supported;

	/* Pointer */
	boolean large_pointer;
	boolean color_pointer;
	uint32 pointer_cache_size;

	/* Bitmap Cache */
	boolean bitmap_cache;
	boolean bitmap_cache_v3;
	boolean persistent_bitmap_cache;
	uint32 bitmapCacheV2NumCells;
	BITMAP_CACHE_V2_CELL_INFO bitmapCacheV2CellInfo[6];

	/* Offscreen Bitmap Cache */
	boolean offscreen_bitmap_cache;
	uint32 offscreen_bitmap_cache_size;
	uint32 offscreen_bitmap_cache_entries;

	/* Glyph Cache */
	boolean glyph_cache;
	uint32 glyphSupportLevel;
	GLYPH_CACHE_DEFINITION glyphCache[10];
	GLYPH_CACHE_DEFINITION fragCache;

	/* Draw Nine Grid */
	boolean draw_nine_grid;
	uint32 draw_nine_grid_cache_size;
	uint32 draw_nine_grid_cache_entries;

	/* Draw GDI+ */
	boolean draw_gdi_plus;
	boolean draw_gdi_plus_cache;

	/* Desktop Composition */
	boolean desktop_composition;

	/* Channels */
	int num_channels;
	rdpChannel channels[16];

	/* Monitors */
	int num_monitors;
	struct rdp_monitor monitors[16];

	/* Extensions */
	int num_extensions;
	struct rdp_ext_set extensions[16];
};
typedef struct rdp_settings rdpSettings;

rdpSettings* settings_new(void* instance);
void settings_free(rdpSettings* settings);

#endif /* __RDP_SETTINGS_H */

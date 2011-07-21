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
typedef struct
{
	uint32 bias;
	uint8 standardName[32];
	SYSTEM_TIME standardDate;
	uint32 standardBias;
	uint8 daylightName[32];
	SYSTEM_TIME daylightDate;
	uint32 daylightBias;
} TIME_ZONE_INFORMATION;

/* ARC_CS_PRIVATE_PACKET */
typedef struct
{
	uint32 cbLen;
	uint32 version;
	uint32 logonId;
	uint8 securityVerifier[16];
} ARC_CS_PRIVATE_PACKET;

struct rdp_chan
{
	char name[8]; /* ui sets */
	int options; /* ui sets */
	int chan_id; /* core sets */
	void * handle; /* just for ui */
};

struct rdp_ext_set
{
	char name[256]; /* plugin name or path */
	void * data; /* plugin data */
};

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
	uint16 width;
	uint16 height;
	uint32 rdp_version;
	uint16 color_depth;
	uint32 kbd_layout;
	uint32 kbd_type;
	uint32 kbd_subtype;
	uint32 kbd_fn_keys;
	uint32 client_build;
	uint32 selected_protocol;
	uint32 encryption_method;
	uint32 encryption_level;

	BLOB server_random;
	BLOB server_certificate;

	boolean console_audio;
	boolean console_session;
	uint32 redirected_session_id;

	int num_channels;
	struct rdp_chan channels[16];

	int num_monitors;
	struct rdp_monitor monitors[16];

	struct rdp_ext_set extensions[16];

	UNICONV* uniconv;
	char client_hostname[32];
	char client_product_id[32];

	uint16 port;
	uint8* hostname;
	uint8* username;
	uint8* password;
	uint8* domain;
	uint8* shell;
	uint8* directory;
	uint32 performance_flags;

	boolean autologon;
	boolean compression;

	boolean ipv6;
	uint8* ip_address;
	uint8* client_dir;
	TIME_ZONE_INFORMATION client_time_zone;
	ARC_CS_PRIVATE_PACKET auto_reconnect_cookie;

	boolean encryption;
	boolean tls_security;
	boolean nla_security;
	boolean rdp_security;

	uint32 share_id;

	boolean refresh_rect;
	boolean suppress_output;
	boolean desktop_resize;

	uint8 order_support[32];

	boolean color_pointer;
	boolean sound_beeps;

	boolean fast_path_input;

	boolean offscreen_bitmap_cache;
	uint16 offscreen_bitmap_cache_size;
	uint16 offscreen_bitmap_cache_entries;

	boolean persistent_bitmap_cache;

	uint32 vc_chunk_size;

	boolean draw_nine_grid;
	uint16 draw_nine_grid_cache_size;
	uint16 draw_nine_grid_cache_entries;

	boolean draw_gdi_plus;
	boolean draw_gdi_plus_cache;

	boolean remote_app;

	char app_name[64];
	int bitmap_cache;
	int bitmap_cache_persist_enable;
	int bitmap_cache_precache;
	int bitmap_compression;
	int desktop_save;
	int polygon_ellipse_orders;
	int off_screen_bitmaps;
	int triblt;
	int new_cursors;
	int mouse_motion;
	int rfx_flags;
	int ui_decode_flags;
	int use_frame_ack;
	int software_gdi;
};
typedef struct rdp_settings rdpSettings;

rdpSettings* settings_new();
void settings_free(rdpSettings* settings);

#endif /* __RDP_SETTINGS_H */

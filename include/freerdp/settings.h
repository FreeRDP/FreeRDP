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
#define ENCRYPTION_40BIT_FLAG			0x00000001
#define ENCRYPTION_128BIT_FLAG			0x00000002
#define ENCRYPTION_56BIT_FLAG			0x00000008
#define ENCRYPTION_FIPS_FLAG			0x00000010

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
	uint32 encryption_methods;

	BLOB server_random;
	BLOB server_certificate;

	int console_session;
	uint32 redirected_session_id;

	int num_channels;
	struct rdp_chan channels[16];

	int num_monitors;
	struct rdp_monitor monitors[16];

	UNICONV* uniconv;
	char client_hostname[32];
	char client_product_id[32];

	char* hostname;
	char* username;
	char* password;
	char* domain;

	char shell[256];
	char directory[256];
	int performance_flags;
	int tcp_port_rdp;

	int encryption;
	int tls_security;
	int nla_security;
	int rdp_security;

	int remote_app;
	char app_name[64];
	int bitmap_cache;
	int bitmap_cache_persist_enable;
	int bitmap_cache_precache;
	int bitmap_compression;
	int desktop_save;
	int polygon_ellipse_orders;
	int autologin;
	int console_audio;
	int off_screen_bitmaps;
	int triblt;
	int new_cursors;
	int mouse_motion;
	int bulk_compression;
	int rfx_flags;
	int ui_decode_flags;
	int use_frame_ack;
	int software_gdi;
	struct rdp_ext_set extensions[16];
};
typedef struct rdp_settings rdpSettings;

rdpSettings* settings_new();
void settings_free(rdpSettings* settings);

#endif /* __RDP_SETTINGS_H */

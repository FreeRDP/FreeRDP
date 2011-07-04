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

struct rdp_chan
{
	char name[8]; /* ui sets */
	int flags; /* ui sets */
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
	int width;
	int height;
	char hostname[16];
	char server[64];
	char domain[16];
	char password[64];
	char shell[256];
	char directory[256];
	char username[256];
	int tcp_port_rdp;
	int keyboard_layout;
	int keyboard_type;
	int keyboard_subtype;
	int keyboard_functionkeys;
	char xkb_layout[32];
	char xkb_variant[32];
	int tls_security;
	int nla_security;
	int rdp_security;
	int encryption;
	int rdp_version;
	int remote_app;
	char app_name[64];
	int console_session;
	int server_depth;
	int bitmap_cache;
	int bitmap_cache_persist_enable;
	int bitmap_cache_precache;
	int bitmap_compression;
	int performanceflags;
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
	int num_channels;
	int software_gdi;
	struct rdp_chan channels[16];
	struct rdp_ext_set extensions[16];
	int num_monitors;
	struct rdp_monitor monitors[16];
};

#endif /* __RDP_SETTINGS_H */

/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Capability Sets
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __CAPABILITIES_H
#define __CAPABILITIES_H

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/utils/stream.h>

/* Capability Set Types */
#define CAPSET_TYPE_GENERAL			0x0001
#define CAPSET_TYPE_BITMAP			0x0002
#define CAPSET_TYPE_ORDER			0x0003
#define CAPSET_TYPE_BITMAP_CACHE		0x0004
#define CAPSET_TYPE_CONTROL			0x0005
#define CAPSET_TYPE_ACTIVATION			0x0007
#define CAPSET_TYPE_POINTER			0x0008
#define CAPSET_TYPE_SHARE			0x0009
#define CAPSET_TYPE_COLOR_CACHE			0x000A
#define CAPSET_TYPE_SOUND			0x000C
#define CAPSET_TYPE_INPUT			0x000D
#define CAPSET_TYPE_FONT			0x000E
#define CAPSET_TYPE_BRUSH			0x000F
#define CAPSET_TYPE_GLYPH_CACHE			0x0010
#define CAPSET_TYPE_OFFSCREEN_CACHE		0x0011
#define CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT	0x0012
#define CAPSET_TYPE_BITMAP_CACHE_V2		0x0013
#define CAPSET_TYPE_VIRTUAL_CHANNEL		0x0014
#define CAPSET_TYPE_DRAW_NINE_GRID_CACHE	0x0015
#define CAPSET_TYPE_DRAW_GDI_PLUS		0x0016
#define CAPSET_TYPE_RAIL			0x0017
#define CAPSET_TYPE_WINDOW			0x0018
#define CAPSET_TYPE_COMP_DESK			0x0019
#define CAPSET_TYPE_MULTI_FRAGMENT_UPDATE	0x001A
#define CAPSET_TYPE_LARGE_POINTER		0x001B
#define CAPSET_TYPE_SURFACE_COMMANDS		0x001C
#define CAPSET_TYPE_BITMAP_CODECS		0x001D

#define CAPSET_HEADER_LENGTH			4

void rdp_read_demand_active(STREAM* s, rdpSettings* settings);
void rdp_read_deactivate_all(STREAM* s, rdpSettings* settings);

void rdp_read_general_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_bitmap_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_order_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_control_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_window_activation_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_pointer_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_share_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_color_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_sound_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_input_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_font_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_brush_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_glyph_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_offscreen_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_bitmap_cache_host_support_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_bitmap_cache_v2_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_virtual_channel_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_draw_nine_grid_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_draw_gdi_plus_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_remote_programs_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_window_list_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_desktop_composition_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_multifragment_update_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_large_pointer_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_surface_commands_capability_set(STREAM* s, rdpSettings* settings);
void rdp_read_bitmap_codecs_capability_set(STREAM* s, rdpSettings* settings);

void rdp_write_general_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_bitmap_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_order_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_control_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_window_activation_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_pointer_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_share_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_color_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_sound_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_input_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_font_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_brush_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_glyph_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_offscreen_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_bitmap_cache_host_support_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_bitmap_cache_v2_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_virtual_channel_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_draw_nine_grid_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_draw_gdi_plus_cache_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_remote_programs_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_window_list_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_desktop_composition_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_multifragment_update_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_large_pointer_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_surface_commands_capability_set(STREAM* s, rdpSettings* settings);
void rdp_write_bitmap_codecs_capability_set(STREAM* s, rdpSettings* settings);

#endif /* __CAPABILITIES_H */

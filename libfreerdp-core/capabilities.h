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

#include "rdp.h"

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
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
#define CAPSET_TYPE_FRAME_ACKNOWLEDGE           0x001E

#define CAPSET_HEADER_LENGTH			4

#define SOURCE_DESCRIPTOR			"FREERDP"

/* Capabilities Protocol Version */
#define CAPS_PROTOCOL_VERSION			0x0200

/* General Capability Flags */
#define FASTPATH_OUTPUT_SUPPORTED		0x0001
#define NO_BITMAP_COMPRESSION_HDR		0x0400
#define LONG_CREDENTIALS_SUPPORTED		0x0004
#define AUTORECONNECT_SUPPORTED			0x0008
#define ENC_SALTED_CHECKSUM			0x0010

/* Drawing Flags */
#define DRAW_ALLOW_DYNAMIC_COLOR_FIDELITY	0x02
#define DRAW_ALLOW_COLOR_SUBSAMPLING		0x04
#define DRAW_ALLOW_SKIP_ALPHA			0x08

/* Order Flags */
#define NEGOTIATE_ORDER_SUPPORT			0x0002
#define ZERO_BOUNDS_DELTA_SUPPORT		0x0008
#define COLOR_INDEX_SUPPORT			0x0020
#define SOLID_PATTERN_BRUSH_ONLY		0x0040
#define ORDER_FLAGS_EXTRA_SUPPORT		0x0080

/* Extended Order Flags */
#define CACHE_BITMAP_V3_SUPPORT			0x0002
#define ALTSEC_FRAME_MARKER_SUPPORT		0x0004

/* Sound Flags */
#define SOUND_BEEPS_FLAG			0x0001

/* Input Flags */
#define INPUT_FLAG_SCANCODES			0x0001
#define INPUT_FLAG_MOUSEX			0x0004
#define INPUT_FLAG_FASTPATH_INPUT		0x0008
#define INPUT_FLAG_UNICODE			0x0010
#define INPUT_FLAG_FASTPATH_INPUT2		0x0020

/* Font Support Flags */
#define FONTSUPPORT_FONTLIST			0x0001

/* Brush Support Level */
#define BRUSH_DEFAULT				0x00000000
#define BRUSH_COLOR_8x8				0x00000001
#define BRUSH_COLOR_FULL			0x00000002

/* Bitmap Cache Version */
#define BITMAP_CACHE_V2				0x01

/* Bitmap Cache V2 Flags */
#define PERSISTENT_KEYS_EXPECTED_FLAG		0x0001
#define ALLOW_CACHE_WAITING_LIST_FLAG		0x0002

/* Virtual Channel Flags */
#define VCCAPS_NO_COMPR				0x00000000
#define VCCAPS_COMPR_SC				0x00000001
#define VCCAPS_COMPR_CS_8K			0x00000002

/* Draw Nine Grid Support Level */
#define DRAW_NINEGRID_NO_SUPPORT		0x00000000
#define DRAW_NINEGRID_SUPPORTED			0x00000001
#define DRAW_NINEGRID_SUPPORTED_V2		0x00000002

/* Draw GDI+ Support Level */
#define DRAW_GDIPLUS_DEFAULT			0x00000000
#define DRAW_GDIPLUS_SUPPORTED			0x00000001

/* Draw GDI+ Cache Level */
#define DRAW_GDIPLUS_CACHE_LEVEL_DEFAULT	0x00000000
#define DRAW_GDIPLUS_CACHE_LEVEL_ONE		0x00000001

/* RAIL Support Level */
#define RAIL_LEVEL_SUPPORTED			0x00000001
#define RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED	0x00000002

/* Window Support Level */
#define WINDOW_LEVEL_NOT_SUPPORTED		0x00000000
#define WINDOW_LEVEL_SUPPORTED			0x00000001
#define WINDOW_LEVEL_SUPPORTED_EX		0x00000002

/* Desktop Composition Support Level */
#define COMPDESK_NOT_SUPPORTED			0x0000
#define COMPDESK_SUPPORTED			0x0001

/* Large Pointer Support Flags */
#define LARGE_POINTER_FLAG_96x96		0x00000001

/* Surface Commands Flags */
#define SURFCMDS_SET_SURFACE_BITS		0x00000002
#define SURFCMDS_FRAME_MARKER			0x00000010
#define SURFCMDS_STREAM_SURFACE_BITS		0x00000040

/* Bitmap Codec Constants */
#define CARDP_CAPS_CAPTURE_NON_CAC		0x00000001
#define CBY_CAPS				0xCBC0
#define CBY_CAPSET				0xCBC1
#define CLY_CAPSET				0xCFC0
#define CLW_VERSION_1_0				0x0100
#define CT_TILE_64x64				0x0040
#define CLW_COL_CONV_ICT			0x1
#define CLW_XFORM_DWT_53_A			0x1
#define CLW_ENTROPY_RLGR1			0x01
#define CLW_ENTROPY_RLGR3			0x04

boolean rdp_recv_demand_active(rdpRdp* rdp, STREAM* s);
void rdp_write_demand_active(STREAM* s, rdpSettings* settings);
boolean rdp_send_demand_active(rdpRdp* rdp);
boolean rdp_recv_confirm_active(rdpRdp* rdp, STREAM* s);
void rdp_write_confirm_active(STREAM* s, rdpSettings* settings);
boolean rdp_send_confirm_active(rdpRdp* rdp);

#endif /* __CAPABILITIES_H */

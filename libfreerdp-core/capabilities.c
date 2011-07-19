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

#include "capabilities.h"

uint8 CAPSET_TYPE_STRINGS[][32] =
{
		"Unknown",
		"General",
		"Bitmap",
		"Order",
		"Bitmap Cache",
		"Control",
		"Unknown",
		"Window Activation",
		"Pointer",
		"Share",
		"Color Cache",
		"Unknown",
		"Sound",
		"Input",
		"Font",
		"Brush",
		"Glyph Cache",
		"Offscreen Bitmap Cache",
		"Bitmap Cache Host Support",
		"Bitmap Cache v2",
		"Virtual Channel",
		"DrawNineGrid Cache",
		"Draw GDI+ Cache",
		"Remote Programs",
		"Window List",
		"Desktop Composition",
		"Multifragment Update",
		"Large Pointer",
		"Surface Commands",
		"Bitmap Codecs"
};

void rdp_read_capability_set_header(STREAM* s, uint16* length, uint16* type)
{
	stream_read_uint16(s, *type); /* capabilitySetType */
	stream_read_uint16(s, *length); /* lengthCapability */
}

void rdp_read_general_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_bitmap_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_order_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_control_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_window_activation_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_pointer_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_share_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_color_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_sound_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_input_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_font_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_brush_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_glyph_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_offscreen_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_bitmap_cache_host_support_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_bitmap_cache_v2_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_virtual_channel_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_draw_nine_grid_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_draw_gdi_plus_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_remote_programs_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_window_list_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_desktop_composition_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_multifragment_update_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_large_pointer_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_surface_commands_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_bitmap_codecs_capability_set(STREAM* s, rdpSettings* settings)
{

}

void rdp_read_demand_active(STREAM* s, rdpSettings* settings)
{
	uint16 type;
	uint16 length;
	uint16 numberCapabilities;
	uint16 lengthSourceDescriptor;
	uint16 lengthCombinedCapabilities;

	printf("Demand Active PDU\n");

	stream_read_uint32(s, settings->share_id); /* shareId (4 bytes) */
	stream_read_uint16(s, lengthSourceDescriptor); /* lengthSourceDescriptor (2 bytes) */
	stream_read_uint16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */
	stream_seek(s, lengthSourceDescriptor); /* sourceDescriptor */
	stream_read_uint16(s, numberCapabilities); /* numberCapabilities (2 bytes) */
	stream_seek(s, 2); /* pad2Octets (2 bytes) */

	/* capabilitySets */
	while (numberCapabilities > 0)
	{
		rdp_read_capability_set_header(s, &length, &type);
		printf("%s Capability Set (0x%02X), length:%d\n", CAPSET_TYPE_STRINGS[type], type, length);
		stream_seek(s, length - CAPSET_HEADER_LENGTH);
		numberCapabilities--;

		switch (type)
		{
			case CAPSET_TYPE_GENERAL:
				rdp_read_general_capability_set(s, settings);
				break;

			case CAPSET_TYPE_BITMAP:
				rdp_read_bitmap_capability_set(s, settings);
				break;

			case CAPSET_TYPE_ORDER:
				rdp_read_order_capability_set(s, settings);
				break;

			case CAPSET_TYPE_BITMAP_CACHE:
				rdp_read_bitmap_cache_capability_set(s, settings);
				break;

			case CAPSET_TYPE_CONTROL:
				rdp_read_control_capability_set(s, settings);
				break;

			case CAPSET_TYPE_ACTIVATION:
				rdp_read_window_activation_capability_set(s, settings);
				break;

			case CAPSET_TYPE_POINTER:
				rdp_read_pointer_capability_set(s, settings);
				break;

			case CAPSET_TYPE_SHARE:
				rdp_read_share_capability_set(s, settings);
				break;

			case CAPSET_TYPE_COLOR_CACHE:
				rdp_read_color_cache_capability_set(s, settings);
				break;

			case CAPSET_TYPE_SOUND:
				rdp_read_sound_capability_set(s, settings);
				break;

			case CAPSET_TYPE_INPUT:
				rdp_read_input_capability_set(s, settings);
				break;

			case CAPSET_TYPE_FONT:
				rdp_read_font_capability_set(s, settings);
				break;

			case CAPSET_TYPE_BRUSH:
				rdp_read_brush_capability_set(s, settings);
				break;

			case CAPSET_TYPE_GLYPH_CACHE:
				rdp_read_glyph_cache_capability_set(s, settings);
				break;

			case CAPSET_TYPE_OFFSCREEN_CACHE:
				rdp_read_offscreen_bitmap_cache_capability_set(s, settings);
				break;

			case CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT:
				rdp_read_bitmap_cache_host_support_capability_set(s, settings);
				break;

			case CAPSET_TYPE_BITMAP_CACHE_V2:
				rdp_read_bitmap_cache_v2_capability_set(s, settings);
				break;

			case CAPSET_TYPE_VIRTUAL_CHANNEL:
				rdp_read_virtual_channel_capability_set(s, settings);
				break;

			case CAPSET_TYPE_DRAW_NINE_GRID_CACHE:
				rdp_read_draw_nine_grid_cache_capability_set(s, settings);
				break;

			case CAPSET_TYPE_DRAW_GDI_PLUS:
				rdp_read_draw_gdi_plus_cache_capability_set(s, settings);
				break;

			case CAPSET_TYPE_RAIL:
				rdp_read_remote_programs_capability_set(s, settings);
				break;

			case CAPSET_TYPE_WINDOW:
				rdp_read_window_list_capability_set(s, settings);
				break;

			case CAPSET_TYPE_COMP_DESK:
				rdp_read_desktop_composition_capability_set(s, settings);
				break;

			case CAPSET_TYPE_MULTI_FRAGMENT_UPDATE:
				rdp_read_multifragment_update_capability_set(s, settings);
				break;

			case CAPSET_TYPE_LARGE_POINTER:
				rdp_read_large_pointer_capability_set(s, settings);
				break;

			case CAPSET_TYPE_SURFACE_COMMANDS:
				rdp_read_surface_commands_capability_set(s, settings);
				break;

			case CAPSET_TYPE_BITMAP_CODECS:
				rdp_read_bitmap_codecs_capability_set(s, settings);
				break;

			default:
				break;
		}
	}
}

void rdp_read_deactivate_all(STREAM* s, rdpSettings* settings)
{
	printf("Deactivate All PDU\n");
}

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

/**
 * Read general capability set.\n
 * @msdn{cc240549}
 * @param s stream
 * @param settings settings
 */

void rdp_read_general_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read bitmap capability set.\n
 * @msdn{cc240554}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read order capability set.\n
 * @msdn{cc240556}
 * @param s stream
 * @param settings settings
 */

void rdp_read_order_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read bitmap cache capability set.\n
 * @msdn{cc240559}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read control capability set.\n
 * @msdn{cc240568}
 * @param s stream
 * @param settings settings
 */

void rdp_read_control_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read window activation capability set.\n
 * @msdn{cc240569}
 * @param s stream
 * @param settings settings
 */

void rdp_read_window_activation_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read pointer capability set.\n
 * @msdn{cc240562}
 * @param s stream
 * @param settings settings
 */

void rdp_read_pointer_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read share capability set.\n
 * @msdn{cc240570}
 * @param s stream
 * @param settings settings
 */

void rdp_read_share_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read color cache capability set.\n
 * @msdn{cc241564}
 * @param s stream
 * @param settings settings
 */

void rdp_read_color_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read sound capability set.\n
 * @msdn{cc240552}
 * @param s stream
 * @param settings settings
 */

void rdp_read_sound_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read input capability set.\n
 * @msdn{cc240563}
 * @param s stream
 * @param settings settings
 */

void rdp_read_input_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read font capability set.\n
 * @msdn{cc240571}
 * @param s stream
 * @param settings settings
 */

void rdp_read_font_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read brush capability set.\n
 * @msdn{cc240564}
 * @param s stream
 * @param settings settings
 */

void rdp_read_brush_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read glyph cache capability set.\n
 * @msdn{cc240565}
 * @param s stream
 * @param settings settings
 */

void rdp_read_glyph_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read offscreen bitmap cache capability set.\n
 * @msdn{cc240550}
 * @param s stream
 * @param settings settings
 */

void rdp_read_offscreen_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read bitmap cache host support capability set.\n
 * @msdn{cc240557}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_cache_host_support_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read bitmap cache v2 capability set.\n
 * @msdn{cc240560}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_cache_v2_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read virtual channel capability set.\n
 * @msdn{cc240551}
 * @param s stream
 * @param settings settings
 */

void rdp_read_virtual_channel_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read drawn nine grid cache capability set.\n
 * @msdn{cc241565}
 * @param s stream
 * @param settings settings
 */

void rdp_read_draw_nine_grid_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read GDI+ cache capability set.\n
 * @msdn{cc241566}
 * @param s stream
 * @param settings settings
 */

void rdp_read_draw_gdi_plus_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read remote programs capability set.\n
 * @msdn{cc242518}
 * @param s stream
 * @param settings settings
 */

void rdp_read_remote_programs_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read window list capability set.\n
 * @msdn{cc242564}
 * @param s stream
 * @param settings settings
 */

void rdp_read_window_list_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read desktop composition capability set.\n
 * @msdn{cc240855}
 * @param s stream
 * @param settings settings
 */

void rdp_read_desktop_composition_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read multifragment update capability set.\n
 * @msdn{cc240649}
 * @param s stream
 * @param settings settings
 */

void rdp_read_multifragment_update_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read large pointer capability set.\n
 * @msdn{cc240650}
 * @param s stream
 * @param settings settings
 */

void rdp_read_large_pointer_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read surface commands capability set.\n
 * @msdn{dd871563}
 * @param s stream
 * @param settings settings
 */

void rdp_read_surface_commands_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Read bitmap codecs capability set.\n
 * @msdn{dd891377}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_codecs_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write general capability set.\n
 * @msdn{cc240549}
 * @param s stream
 * @param settings settings
 */

void rdp_write_general_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write bitmap capability set.\n
 * @msdn{cc240554}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write order capability set.\n
 * @msdn{cc240556}
 * @param s stream
 * @param settings settings
 */

void rdp_write_order_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write bitmap cache capability set.\n
 * @msdn{cc240559}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write control capability set.\n
 * @msdn{cc240568}
 * @param s stream
 * @param settings settings
 */

void rdp_write_control_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write window activation capability set.\n
 * @msdn{cc240569}
 * @param s stream
 * @param settings settings
 */

void rdp_write_window_activation_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write pointer capability set.\n
 * @msdn{cc240562}
 * @param s stream
 * @param settings settings
 */

void rdp_write_pointer_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write share capability set.\n
 * @msdn{cc240570}
 * @param s stream
 * @param settings settings
 */

void rdp_write_share_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write color cache capability set.\n
 * @msdn{cc241564}
 * @param s stream
 * @param settings settings
 */

void rdp_write_color_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write sound capability set.\n
 * @msdn{cc240552}
 * @param s stream
 * @param settings settings
 */

void rdp_write_sound_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write input capability set.\n
 * @msdn{cc240563}
 * @param s stream
 * @param settings settings
 */

void rdp_write_input_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write font capability set.\n
 * @msdn{cc240571}
 * @param s stream
 * @param settings settings
 */

void rdp_write_font_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write brush capability set.\n
 * @msdn{cc240564}
 * @param s stream
 * @param settings settings
 */

void rdp_write_brush_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write glyph cache capability set.\n
 * @msdn{cc240565}
 * @param s stream
 * @param settings settings
 */

void rdp_write_glyph_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write offscreen bitmap cache capability set.\n
 * @msdn{cc240550}
 * @param s stream
 * @param settings settings
 */

void rdp_write_offscreen_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write bitmap cache host support capability set.\n
 * @msdn{cc240557}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_cache_host_support_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write bitmap cache v2 capability set.\n
 * @msdn{cc240560}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_cache_v2_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write virtual channel capability set.\n
 * @msdn{cc240551}
 * @param s stream
 * @param settings settings
 */

void rdp_write_virtual_channel_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write drawn nine grid cache capability set.\n
 * @msdn{cc241565}
 * @param s stream
 * @param settings settings
 */

void rdp_write_draw_nine_grid_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write GDI+ cache capability set.\n
 * @msdn{cc241566}
 * @param s stream
 * @param settings settings
 */

void rdp_write_draw_gdi_plus_cache_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write remote programs capability set.\n
 * @msdn{cc242518}
 * @param s stream
 * @param settings settings
 */

void rdp_write_remote_programs_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write window list capability set.\n
 * @msdn{cc242564}
 * @param s stream
 * @param settings settings
 */

void rdp_write_window_list_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write desktop composition capability set.\n
 * @msdn{cc240855}
 * @param s stream
 * @param settings settings
 */

void rdp_write_desktop_composition_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write multifragment update capability set.\n
 * @msdn{cc240649}
 * @param s stream
 * @param settings settings
 */

void rdp_write_multifragment_update_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write large pointer capability set.\n
 * @msdn{cc240650}
 * @param s stream
 * @param settings settings
 */

void rdp_write_large_pointer_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write surface commands capability set.\n
 * @msdn{dd871563}
 * @param s stream
 * @param settings settings
 */

void rdp_write_surface_commands_capability_set(STREAM* s, rdpSettings* settings)
{

}

/**
 * Write bitmap codecs capability set.\n
 * @msdn{dd891377}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_codecs_capability_set(STREAM* s, rdpSettings* settings)
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

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
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "capabilities.h"

/*
static const char* const CAPSET_TYPE_STRINGS[] =
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
		"Bitmap Codecs",
		"Frame Acknowledge"
};
*/

/* CODEC_GUID_REMOTEFX 0x76772F12BD724463AFB3B73C9C6F7886 */
#define CODEC_GUID_REMOTEFX "\x12\x2F\x77\x76\x72\xBD\x63\x44\xAF\xB3\xB7\x3C\x9C\x6F\x78\x86"

/* CODEC_GUID_NSCODEC  0xCA8D1BB9000F154F589FAE2D1A87E2D6 */
#define CODEC_GUID_NSCODEC "\xb9\x1b\x8d\xca\x0f\x00\x4f\x15\x58\x9f\xae\x2d\x1a\x87\xe2\xd6"

void rdp_read_capability_set_header(STREAM* s, uint16* length, uint16* type)
{
	stream_read_uint16(s, *type); /* capabilitySetType */
	stream_read_uint16(s, *length); /* lengthCapability */
}

void rdp_write_capability_set_header(STREAM* s, uint16 length, uint16 type)
{
	stream_write_uint16(s, type); /* capabilitySetType */
	stream_write_uint16(s, length); /* lengthCapability */
}

uint8* rdp_capability_set_start(STREAM* s)
{
	uint8* header;

	stream_get_mark(s, header);
	stream_write_zero(s, CAPSET_HEADER_LENGTH);

	return header;
}

void rdp_capability_set_finish(STREAM* s, uint8* header, uint16 type)
{
	uint16 length;
	uint8* footer;

	footer = s->p;
	length = footer - header;
	stream_set_mark(s, header);

	rdp_write_capability_set_header(s, length, type);
	stream_set_mark(s, footer);
}

/**
 * Read general capability set.\n
 * @msdn{cc240549}
 * @param s stream
 * @param settings settings
 */

void rdp_read_general_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint16 extraFlags;
	uint8 refreshRectSupport;
	uint8 suppressOutputSupport;

	if (settings->server_mode)
	{
		stream_read_uint16(s, settings->os_major_type); /* osMajorType (2 bytes) */
		stream_read_uint16(s, settings->os_minor_type); /* osMinorType (2 bytes) */
	}
	else
	{
		stream_seek_uint16(s); /* osMajorType (2 bytes) */
		stream_seek_uint16(s); /* osMinorType (2 bytes) */
	}
	stream_seek_uint16(s); /* protocolVersion (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsA (2 bytes) */
	stream_seek_uint16(s); /* generalCompressionTypes (2 bytes) */
	stream_read_uint16(s, extraFlags); /* extraFlags (2 bytes) */
	stream_seek_uint16(s); /* updateCapabilityFlag (2 bytes) */
	stream_seek_uint16(s); /* remoteUnshareFlag (2 bytes) */
	stream_seek_uint16(s); /* generalCompressionLevel (2 bytes) */
	stream_read_uint8(s, refreshRectSupport); /* refreshRectSupport (1 byte) */
	stream_read_uint8(s, suppressOutputSupport); /* suppressOutputSupport (1 byte) */

	if (!(extraFlags & FASTPATH_OUTPUT_SUPPORTED))
		settings->fastpath_output = false;

	if (refreshRectSupport == false)
		settings->refresh_rect = false;

	if (suppressOutputSupport == false)
		settings->suppress_output = false;
}

/**
 * Write general capability set.\n
 * @msdn{cc240549}
 * @param s stream
 * @param settings settings
 */

void rdp_write_general_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint16 extraFlags;

	header = rdp_capability_set_start(s);

	extraFlags = LONG_CREDENTIALS_SUPPORTED | NO_BITMAP_COMPRESSION_HDR;

	if (settings->auto_reconnection)
		extraFlags |= AUTORECONNECT_SUPPORTED;

	if (settings->fastpath_output)
		extraFlags |= FASTPATH_OUTPUT_SUPPORTED;

	if (settings->server_mode)
	{
		/* not yet supported server-side */
		settings->refresh_rect = false;
		settings->suppress_output = false;
	}

	stream_write_uint16(s, settings->os_major_type); /* osMajorType (2 bytes) */
	stream_write_uint16(s, settings->os_minor_type); /* osMinorType (2 bytes) */
	stream_write_uint16(s, CAPS_PROTOCOL_VERSION); /* protocolVersion (2 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsA (2 bytes) */
	stream_write_uint16(s, 0); /* generalCompressionTypes (2 bytes) */
	stream_write_uint16(s, extraFlags); /* extraFlags (2 bytes) */
	stream_write_uint16(s, 0); /* updateCapabilityFlag (2 bytes) */
	stream_write_uint16(s, 0); /* remoteUnshareFlag (2 bytes) */
	stream_write_uint16(s, 0); /* generalCompressionLevel (2 bytes) */
	stream_write_uint8(s, settings->refresh_rect); /* refreshRectSupport (1 byte) */
	stream_write_uint8(s, settings->suppress_output); /* suppressOutputSupport (1 byte) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_GENERAL);
}

/**
 * Read bitmap capability set.\n
 * @msdn{cc240554}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint8 drawingFlags;
	uint16 desktopWidth;
	uint16 desktopHeight;
	uint16 desktopResizeFlag;
	uint16 preferredBitsPerPixel;

	stream_read_uint16(s, preferredBitsPerPixel); /* preferredBitsPerPixel (2 bytes) */
	stream_seek_uint16(s); /* receive1BitPerPixel (2 bytes) */
	stream_seek_uint16(s); /* receive4BitsPerPixel (2 bytes) */
	stream_seek_uint16(s); /* receive8BitsPerPixel (2 bytes) */
	stream_read_uint16(s, desktopWidth); /* desktopWidth (2 bytes) */
	stream_read_uint16(s, desktopHeight); /* desktopHeight (2 bytes) */
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */
	stream_read_uint16(s, desktopResizeFlag); /* desktopResizeFlag (2 bytes) */
	stream_seek_uint16(s); /* bitmapCompressionFlag (2 bytes) */
	stream_seek_uint8(s); /* highColorFlags (1 byte) */
	stream_read_uint8(s, drawingFlags); /* drawingFlags (1 byte) */
	stream_seek_uint16(s); /* multipleRectangleSupport (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsB (2 bytes) */

	if (!settings->server_mode && preferredBitsPerPixel != settings->color_depth)
	{
		/* The client must respect the actual color depth used by the server */
		settings->color_depth = preferredBitsPerPixel;
	}

	if (desktopResizeFlag == false)
		settings->desktop_resize = false;

	if (!settings->server_mode && settings->desktop_resize)
	{
		/* The server may request a different desktop size during Deactivation-Reactivation sequence */
		settings->width = desktopWidth;
		settings->height = desktopHeight;
	}
}

/**
 * Write bitmap capability set.\n
 * @msdn{cc240554}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint8 drawingFlags;
	uint16 desktopResizeFlag;
	uint16 preferredBitsPerPixel;

	header = rdp_capability_set_start(s);

	drawingFlags = 0;

	if (settings->rdp_version > 5)
		preferredBitsPerPixel = settings->color_depth;
	else
		preferredBitsPerPixel = 8;

	desktopResizeFlag = settings->desktop_resize;

	stream_write_uint16(s, preferredBitsPerPixel); /* preferredBitsPerPixel (2 bytes) */
	stream_write_uint16(s, 1); /* receive1BitPerPixel (2 bytes) */
	stream_write_uint16(s, 1); /* receive4BitsPerPixel (2 bytes) */
	stream_write_uint16(s, 1); /* receive8BitsPerPixel (2 bytes) */
	stream_write_uint16(s, settings->width); /* desktopWidth (2 bytes) */
	stream_write_uint16(s, settings->height); /* desktopHeight (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */
	stream_write_uint16(s, desktopResizeFlag); /* desktopResizeFlag (2 bytes) */
	stream_write_uint16(s, 1); /* bitmapCompressionFlag (2 bytes) */
	stream_write_uint8(s, 0); /* highColorFlags (1 byte) */
	stream_write_uint8(s, drawingFlags); /* drawingFlags (1 byte) */
	stream_write_uint16(s, 1); /* multipleRectangleSupport (2 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsB (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP);
}

/**
 * Read order capability set.\n
 * @msdn{cc240556}
 * @param s stream
 * @param settings settings
 */

void rdp_read_order_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	int i;
	uint16 orderFlags;
	uint8 orderSupport[32];
	uint16 orderSupportExFlags;

	stream_seek(s, 16); /* terminalDescriptor (16 bytes) */
	stream_seek_uint32(s); /* pad4OctetsA (4 bytes) */
	stream_seek_uint16(s); /* desktopSaveXGranularity (2 bytes) */
	stream_seek_uint16(s); /* desktopSaveYGranularity (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsA (2 bytes) */
	stream_seek_uint16(s); /* maximumOrderLevel (2 bytes) */
	stream_seek_uint16(s); /* numberFonts (2 bytes) */
	stream_read_uint16(s, orderFlags); /* orderFlags (2 bytes) */
	stream_read(s, orderSupport, 32); /* orderSupport (32 bytes) */
	stream_seek_uint16(s); /* textFlags (2 bytes) */
	stream_read_uint16(s, orderSupportExFlags); /* orderSupportExFlags (2 bytes) */
	stream_seek_uint32(s); /* pad4OctetsB (4 bytes) */
	stream_seek_uint32(s); /* desktopSaveSize (4 bytes) */
	stream_seek_uint16(s); /* pad2OctetsC (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsD (2 bytes) */
	stream_seek_uint16(s); /* textANSICodePage (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsE (2 bytes) */

	for (i = 0; i < 32; i++)
	{
		if (orderSupport[i] == false)
			settings->order_support[i] = false;
	}
}

/**
 * Write order capability set.\n
 * @msdn{cc240556}
 * @param s stream
 * @param settings settings
 */

void rdp_write_order_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint16 orderFlags;
	uint16 orderSupportExFlags;
	uint16 textANSICodePage;

	header = rdp_capability_set_start(s);

	/* see [MSDN-CP]: http://msdn.microsoft.com/en-us/library/dd317756 */
	textANSICodePage = 65001; /* Unicode (UTF-8) */

	orderSupportExFlags = 0;
	orderFlags = NEGOTIATE_ORDER_SUPPORT | ZERO_BOUNDS_DELTA_SUPPORT | COLOR_INDEX_SUPPORT;

	if (settings->bitmap_cache_v3)
	{
		orderSupportExFlags |= CACHE_BITMAP_V3_SUPPORT;
		orderFlags |= ORDER_FLAGS_EXTRA_SUPPORT;
	}

	if (settings->frame_marker)
	{
		orderSupportExFlags |= ALTSEC_FRAME_MARKER_SUPPORT;
		orderFlags |= ORDER_FLAGS_EXTRA_SUPPORT;
	}

	stream_write_zero(s, 16); /* terminalDescriptor (16 bytes) */
	stream_write_uint32(s, 0); /* pad4OctetsA (4 bytes) */
	stream_write_uint16(s, 1); /* desktopSaveXGranularity (2 bytes) */
	stream_write_uint16(s, 20); /* desktopSaveYGranularity (2 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsA (2 bytes) */
	stream_write_uint16(s, 1); /* maximumOrderLevel (2 bytes) */
	stream_write_uint16(s, 0); /* numberFonts (2 bytes) */
	stream_write_uint16(s, orderFlags); /* orderFlags (2 bytes) */
	stream_write(s, settings->order_support, 32); /* orderSupport (32 bytes) */
	stream_write_uint16(s, 0); /* textFlags (2 bytes) */
	stream_write_uint16(s, orderSupportExFlags); /* orderSupportExFlags (2 bytes) */
	stream_write_uint32(s, 0); /* pad4OctetsB (4 bytes) */
	stream_write_uint32(s, 230400); /* desktopSaveSize (4 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsC (2 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsD (2 bytes) */
	stream_write_uint16(s, 0); /* textANSICodePage (2 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsE (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_ORDER);
}

/**
 * Read bitmap cache capability set.\n
 * @msdn{cc240559}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_cache_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint32(s); /* pad1 (4 bytes) */
	stream_seek_uint32(s); /* pad2 (4 bytes) */
	stream_seek_uint32(s); /* pad3 (4 bytes) */
	stream_seek_uint32(s); /* pad4 (4 bytes) */
	stream_seek_uint32(s); /* pad5 (4 bytes) */
	stream_seek_uint32(s); /* pad6 (4 bytes) */
	stream_seek_uint16(s); /* Cache0Entries (2 bytes) */
	stream_seek_uint16(s); /* Cache0MaximumCellSize (2 bytes) */
	stream_seek_uint16(s); /* Cache1Entries (2 bytes) */
	stream_seek_uint16(s); /* Cache1MaximumCellSize (2 bytes) */
	stream_seek_uint16(s); /* Cache2Entries (2 bytes) */
	stream_seek_uint16(s); /* Cache2MaximumCellSize (2 bytes) */
}

/**
 * Write bitmap cache capability set.\n
 * @msdn{cc240559}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
{
	int bpp;
	uint16 size;
	uint8* header;

	header = rdp_capability_set_start(s);

	bpp = (settings->color_depth + 7) / 8;

	stream_write_uint32(s, 0); /* pad1 (4 bytes) */
	stream_write_uint32(s, 0); /* pad2 (4 bytes) */
	stream_write_uint32(s, 0); /* pad3 (4 bytes) */
	stream_write_uint32(s, 0); /* pad4 (4 bytes) */
	stream_write_uint32(s, 0); /* pad5 (4 bytes) */
	stream_write_uint32(s, 0); /* pad6 (4 bytes) */

	size = bpp * 256;
	stream_write_uint16(s, 200); /* Cache0Entries (2 bytes) */
	stream_write_uint16(s, size); /* Cache0MaximumCellSize (2 bytes) */

	size = bpp * 1024;
	stream_write_uint16(s, 600); /* Cache1Entries (2 bytes) */
	stream_write_uint16(s, size); /* Cache1MaximumCellSize (2 bytes) */

	size = bpp * 4096;
	stream_write_uint16(s, 1000); /* Cache2Entries (2 bytes) */
	stream_write_uint16(s, size); /* Cache2MaximumCellSize (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE);
}

/**
 * Read control capability set.\n
 * @msdn{cc240568}
 * @param s stream
 * @param settings settings
 */

void rdp_read_control_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint16(s); /* controlFlags (2 bytes) */
	stream_seek_uint16(s); /* remoteDetachFlag (2 bytes) */
	stream_seek_uint16(s); /* controlInterest (2 bytes) */
	stream_seek_uint16(s); /* detachInterest (2 bytes) */
}

/**
 * Write control capability set.\n
 * @msdn{cc240568}
 * @param s stream
 * @param settings settings
 */

void rdp_write_control_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;

	header = rdp_capability_set_start(s);

	stream_write_uint16(s, 0); /* controlFlags (2 bytes) */
	stream_write_uint16(s, 0); /* remoteDetachFlag (2 bytes) */
	stream_write_uint16(s, 2); /* controlInterest (2 bytes) */
	stream_write_uint16(s, 2); /* detachInterest (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_CONTROL);
}

/**
 * Read window activation capability set.\n
 * @msdn{cc240569}
 * @param s stream
 * @param settings settings
 */

void rdp_read_window_activation_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint16(s); /* helpKeyFlag (2 bytes) */
	stream_seek_uint16(s); /* helpKeyIndexFlag (2 bytes) */
	stream_seek_uint16(s); /* helpExtendedKeyFlag (2 bytes) */
	stream_seek_uint16(s); /* windowManagerKeyFlag (2 bytes) */
}

/**
 * Write window activation capability set.\n
 * @msdn{cc240569}
 * @param s stream
 * @param settings settings
 */

void rdp_write_window_activation_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;

	header = rdp_capability_set_start(s);

	stream_write_uint16(s, 0); /* helpKeyFlag (2 bytes) */
	stream_write_uint16(s, 0); /* helpKeyIndexFlag (2 bytes) */
	stream_write_uint16(s, 0); /* helpExtendedKeyFlag (2 bytes) */
	stream_write_uint16(s, 0); /* windowManagerKeyFlag (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_ACTIVATION);
}

/**
 * Read pointer capability set.\n
 * @msdn{cc240562}
 * @param s stream
 * @param settings settings
 */

void rdp_read_pointer_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint16 colorPointerFlag;
	uint16 colorPointerCacheSize;
	uint16 pointerCacheSize;

	stream_read_uint16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	stream_read_uint16(s, colorPointerCacheSize); /* colorPointerCacheSize (2 bytes) */
	stream_read_uint16(s, pointerCacheSize); /* pointerCacheSize (2 bytes) */

	if (colorPointerFlag == false)
		settings->color_pointer = false;

	if (settings->server_mode)
	{
		settings->pointer_cache_size = pointerCacheSize;
	}
}

/**
 * Write pointer capability set.\n
 * @msdn{cc240562}
 * @param s stream
 * @param settings settings
 */

void rdp_write_pointer_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint16 colorPointerFlag;

	header = rdp_capability_set_start(s);

	colorPointerFlag = (settings->color_pointer) ? 1 : 0;

	stream_write_uint16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	stream_write_uint16(s, settings->pointer_cache_size); /* colorPointerCacheSize (2 bytes) */

	if (settings->large_pointer)
	{
		stream_write_uint16(s, settings->pointer_cache_size); /* pointerCacheSize (2 bytes) */
	}

	rdp_capability_set_finish(s, header, CAPSET_TYPE_POINTER);
}

/**
 * Read share capability set.\n
 * @msdn{cc240570}
 * @param s stream
 * @param settings settings
 */

void rdp_read_share_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint16(s); /* nodeId (2 bytes) */
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */
}

/**
 * Write share capability set.\n
 * @msdn{cc240570}
 * @param s stream
 * @param settings settings
 */

void rdp_write_share_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint16 nodeId;

	header = rdp_capability_set_start(s);

	nodeId = (settings->server_mode) ? 0x03EA : 0;

	stream_write_uint16(s, nodeId); /* nodeId (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SHARE);
}

/**
 * Read color cache capability set.\n
 * @msdn{cc241564}
 * @param s stream
 * @param settings settings
 */

void rdp_read_color_cache_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint16(s); /* colorTableCacheSize (2 bytes) */
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */
}

/**
 * Write color cache capability set.\n
 * @msdn{cc241564}
 * @param s stream
 * @param settings settings
 */

void rdp_write_color_cache_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;

	header = rdp_capability_set_start(s);

	stream_write_uint16(s, 6); /* colorTableCacheSize (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_COLOR_CACHE);
}

/**
 * Read sound capability set.\n
 * @msdn{cc240552}
 * @param s stream
 * @param settings settings
 */

void rdp_read_sound_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint16 soundFlags;

	stream_read_uint16(s, soundFlags); /* soundFlags (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsA (2 bytes) */

	settings->sound_beeps = (soundFlags & SOUND_BEEPS_FLAG) ? true : false;
}

/**
 * Write sound capability set.\n
 * @msdn{cc240552}
 * @param s stream
 * @param settings settings
 */

void rdp_write_sound_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint16 soundFlags;

	header = rdp_capability_set_start(s);

	soundFlags = (settings->sound_beeps) ? SOUND_BEEPS_FLAG : 0;

	stream_write_uint16(s, soundFlags); /* soundFlags (2 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsA (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SOUND);
}

/**
 * Read input capability set.\n
 * @msdn{cc240563}
 * @param s stream
 * @param settings settings
 */

void rdp_read_input_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint16 inputFlags;

	stream_read_uint16(s, inputFlags); /* inputFlags (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsA (2 bytes) */

	if (settings->server_mode)
	{
		stream_read_uint32(s, settings->kbd_layout); /* keyboardLayout (4 bytes) */
		stream_read_uint32(s, settings->kbd_type); /* keyboardType (4 bytes) */
		stream_read_uint32(s, settings->kbd_subtype); /* keyboardSubType (4 bytes) */
		stream_read_uint32(s, settings->kbd_fn_keys); /* keyboardFunctionKeys (4 bytes) */
	}
	else
	{
		stream_seek_uint32(s); /* keyboardLayout (4 bytes) */
		stream_seek_uint32(s); /* keyboardType (4 bytes) */
		stream_seek_uint32(s); /* keyboardSubType (4 bytes) */
		stream_seek_uint32(s); /* keyboardFunctionKeys (4 bytes) */
	}

	stream_seek(s, 64); /* imeFileName (64 bytes) */

	if (settings->server_mode != true)
	{
		if (inputFlags & INPUT_FLAG_FASTPATH_INPUT)
		{
			/* advertised by RDP 5.0 and 5.1 servers */
		}
		else if (inputFlags & INPUT_FLAG_FASTPATH_INPUT2)
		{
			/* avertised by RDP 5.2, 6.0, 6.1 and 7.0 servers */
		}
		else
		{
			/* server does not support fastpath input */
			settings->fastpath_input = false;
		}
	}
}

/**
 * Write input capability set.\n
 * @msdn{cc240563}
 * @param s stream
 * @param settings settings
 */

void rdp_write_input_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint16 inputFlags;

	header = rdp_capability_set_start(s);

	inputFlags = INPUT_FLAG_SCANCODES | INPUT_FLAG_MOUSEX | INPUT_FLAG_UNICODE;

	if (settings->fastpath_input)
	{
		inputFlags |= INPUT_FLAG_FASTPATH_INPUT;
		inputFlags |= INPUT_FLAG_FASTPATH_INPUT2;
	}

	stream_write_uint16(s, inputFlags); /* inputFlags (2 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsA (2 bytes) */
	stream_write_uint32(s, settings->kbd_layout); /* keyboardLayout (4 bytes) */
	stream_write_uint32(s, settings->kbd_type); /* keyboardType (4 bytes) */
	stream_write_uint32(s, settings->kbd_subtype); /* keyboardSubType (4 bytes) */
	stream_write_uint32(s, settings->kbd_fn_keys); /* keyboardFunctionKeys (4 bytes) */
	stream_write_zero(s, 64); /* imeFileName (64 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_INPUT);
}

/**
 * Read font capability set.\n
 * @msdn{cc240571}
 * @param s stream
 * @param settings settings
 */

void rdp_read_font_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	if (length > 4)
		stream_seek_uint16(s); /* fontSupportFlags (2 bytes) */

	if (length > 6)
		stream_seek_uint16(s); /* pad2Octets (2 bytes) */
}

/**
 * Write font capability set.\n
 * @msdn{cc240571}
 * @param s stream
 * @param settings settings
 */

void rdp_write_font_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;

	header = rdp_capability_set_start(s);

	stream_write_uint16(s, FONTSUPPORT_FONTLIST); /* fontSupportFlags (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_FONT);
}

/**
 * Read brush capability set.\n
 * @msdn{cc240564}
 * @param s stream
 * @param settings settings
 */

void rdp_read_brush_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint32(s); /* brushSupportLevel (4 bytes) */
}

/**
 * Write brush capability set.\n
 * @msdn{cc240564}
 * @param s stream
 * @param settings settings
 */

void rdp_write_brush_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;

	header = rdp_capability_set_start(s);

	stream_write_uint32(s, BRUSH_COLOR_FULL); /* brushSupportLevel (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BRUSH);
}

/**
 * Read cache definition (glyph).\n
 * @msdn{cc240566}
 * @param s stream
 */
void rdp_read_cache_definition(STREAM* s, GLYPH_CACHE_DEFINITION* cache_definition)
{
	stream_read_uint16(s, cache_definition->cacheEntries); /* cacheEntries (2 bytes) */
	stream_read_uint16(s, cache_definition->cacheMaximumCellSize); /* cacheMaximumCellSize (2 bytes) */
}

/**
 * Write cache definition (glyph).\n
 * @msdn{cc240566}
 * @param s stream
 */
void rdp_write_cache_definition(STREAM* s, GLYPH_CACHE_DEFINITION* cache_definition)
{
	stream_write_uint16(s, cache_definition->cacheEntries); /* cacheEntries (2 bytes) */
	stream_write_uint16(s, cache_definition->cacheMaximumCellSize); /* cacheMaximumCellSize (2 bytes) */
}

/**
 * Read glyph cache capability set.\n
 * @msdn{cc240565}
 * @param s stream
 * @param settings settings
 */

void rdp_read_glyph_cache_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint16 glyphSupportLevel;

	stream_seek(s, 40); /* glyphCache (40 bytes) */
	stream_seek_uint32(s); /* fragCache (4 bytes) */
	stream_read_uint16(s, glyphSupportLevel); /* glyphSupportLevel (2 bytes) */
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */

	settings->glyphSupportLevel = glyphSupportLevel;
}

/**
 * Write glyph cache capability set.\n
 * @msdn{cc240565}
 * @param s stream
 * @param settings settings
 */

void rdp_write_glyph_cache_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;

	header = rdp_capability_set_start(s);

	/* glyphCache (40 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[0])); /* glyphCache0 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[1])); /* glyphCache1 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[2])); /* glyphCache2 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[3])); /* glyphCache3 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[4])); /* glyphCache4 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[5])); /* glyphCache5 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[6])); /* glyphCache6 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[7])); /* glyphCache7 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[8])); /* glyphCache8 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->glyphCache[9])); /* glyphCache9 (4 bytes) */

	rdp_write_cache_definition(s, settings->fragCache);  /* fragCache (4 bytes) */

	stream_write_uint16(s, settings->glyphSupportLevel); /* glyphSupportLevel (2 bytes) */

	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_GLYPH_CACHE);
}

/**
 * Read offscreen bitmap cache capability set.\n
 * @msdn{cc240550}
 * @param s stream
 * @param settings settings
 */

void rdp_read_offscreen_bitmap_cache_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint32 offscreenSupportLevel;

	stream_read_uint32(s, offscreenSupportLevel); /* offscreenSupportLevel (4 bytes) */
	stream_read_uint16(s, settings->offscreen_bitmap_cache_size); /* offscreenCacheSize (2 bytes) */
	stream_read_uint16(s, settings->offscreen_bitmap_cache_entries); /* offscreenCacheEntries (2 bytes) */

	if (offscreenSupportLevel & true)
		settings->offscreen_bitmap_cache = true;
}

/**
 * Write offscreen bitmap cache capability set.\n
 * @msdn{cc240550}
 * @param s stream
 * @param settings settings
 */

void rdp_write_offscreen_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint32 offscreenSupportLevel = false;

	header = rdp_capability_set_start(s);

	if (settings->offscreen_bitmap_cache)
		offscreenSupportLevel = true;

	stream_write_uint32(s, offscreenSupportLevel); /* offscreenSupportLevel (4 bytes) */
	stream_write_uint16(s, settings->offscreen_bitmap_cache_size); /* offscreenCacheSize (2 bytes) */
	stream_write_uint16(s, settings->offscreen_bitmap_cache_entries); /* offscreenCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_OFFSCREEN_CACHE);
}

/**
 * Read bitmap cache host support capability set.\n
 * @msdn{cc240557}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_cache_host_support_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint8 cacheVersion;

	stream_read_uint8(s, cacheVersion); /* cacheVersion (1 byte) */
	stream_seek_uint8(s); /* pad1 (1 byte) */
	stream_seek_uint16(s); /* pad2 (2 bytes) */

	if (cacheVersion & BITMAP_CACHE_V2)
		settings->persistent_bitmap_cache = true;
}

/**
 * Write bitmap cache host support capability set.\n
 * @msdn{cc240557}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_cache_host_support_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;

	header = rdp_capability_set_start(s);

	stream_write_uint8(s, BITMAP_CACHE_V2); /* cacheVersion (1 byte) */
	stream_write_uint8(s, 0); /* pad1 (1 byte) */
	stream_write_uint16(s, 0); /* pad2 (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT);
}

void rdp_write_bitmap_cache_cell_info(STREAM* s, BITMAP_CACHE_V2_CELL_INFO* cellInfo)
{
	uint32 info;

	/**
	 * numEntries is in the first 31 bits, while the last bit (k)
	 * is used to indicate a persistent bitmap cache.
	 */

	info = (cellInfo->numEntries | (cellInfo->persistent << 31));
	stream_write_uint32(s, info);
}

/**
 * Read bitmap cache v2 capability set.\n
 * @msdn{cc240560}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_cache_v2_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint16(s); /* cacheFlags (2 bytes) */
	stream_seek_uint8(s); /* pad2 (1 byte) */
	stream_seek_uint8(s); /* numCellCaches (1 byte) */
	stream_seek(s, 4); /* bitmapCache0CellInfo (4 bytes) */
	stream_seek(s, 4); /* bitmapCache1CellInfo (4 bytes) */
	stream_seek(s, 4); /* bitmapCache2CellInfo (4 bytes) */
	stream_seek(s, 4); /* bitmapCache3CellInfo (4 bytes) */
	stream_seek(s, 4); /* bitmapCache4CellInfo (4 bytes) */
	stream_seek(s, 12); /* pad3 (12 bytes) */
}

/**
 * Write bitmap cache v2 capability set.\n
 * @msdn{cc240560}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_cache_v2_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint16 cacheFlags;

	header = rdp_capability_set_start(s);

	cacheFlags = ALLOW_CACHE_WAITING_LIST_FLAG;

	if (settings->persistent_bitmap_cache)
		cacheFlags |= PERSISTENT_KEYS_EXPECTED_FLAG;

	stream_write_uint16(s, cacheFlags); /* cacheFlags (2 bytes) */
	stream_write_uint8(s, 0); /* pad2 (1 byte) */
	stream_write_uint8(s, settings->bitmapCacheV2NumCells); /* numCellCaches (1 byte) */
	rdp_write_bitmap_cache_cell_info(s, &settings->bitmapCacheV2CellInfo[0]); /* bitmapCache0CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, &settings->bitmapCacheV2CellInfo[1]); /* bitmapCache1CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, &settings->bitmapCacheV2CellInfo[2]); /* bitmapCache2CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, &settings->bitmapCacheV2CellInfo[3]); /* bitmapCache3CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, &settings->bitmapCacheV2CellInfo[4]); /* bitmapCache4CellInfo (4 bytes) */
	stream_write_zero(s, 12); /* pad3 (12 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_V2);
}

/**
 * Read virtual channel capability set.\n
 * @msdn{cc240551}
 * @param s stream
 * @param settings settings
 */

void rdp_read_virtual_channel_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint32 flags;
	uint32 VCChunkSize;

	stream_read_uint32(s, flags); /* flags (4 bytes) */

	if (length > 8)
		stream_read_uint32(s, VCChunkSize); /* VCChunkSize (4 bytes) */
	else
		VCChunkSize = 1600;

	if (settings->server_mode != true)
		settings->vc_chunk_size = VCChunkSize;
}

/**
 * Write virtual channel capability set.\n
 * @msdn{cc240551}
 * @param s stream
 * @param settings settings
 */

void rdp_write_virtual_channel_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint32 flags;

	header = rdp_capability_set_start(s);

	flags = (settings->server_mode) ? VCCAPS_COMPR_CS_8K : VCCAPS_NO_COMPR;

	stream_write_uint32(s, flags); /* flags (4 bytes) */
	stream_write_uint32(s, settings->vc_chunk_size); /* VCChunkSize (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_VIRTUAL_CHANNEL);
}

/**
 * Read drawn nine grid cache capability set.\n
 * @msdn{cc241565}
 * @param s stream
 * @param settings settings
 */

void rdp_read_draw_nine_grid_cache_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint32 drawNineGridSupportLevel;

	stream_read_uint32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	stream_read_uint16(s, settings->draw_nine_grid_cache_size); /* drawNineGridCacheSize (2 bytes) */
	stream_read_uint16(s, settings->draw_nine_grid_cache_entries); /* drawNineGridCacheEntries (2 bytes) */

	if ((drawNineGridSupportLevel & DRAW_NINEGRID_SUPPORTED) ||
			(drawNineGridSupportLevel & DRAW_NINEGRID_SUPPORTED_V2))
		settings->draw_nine_grid = true;
}

/**
 * Write drawn nine grid cache capability set.\n
 * @msdn{cc241565}
 * @param s stream
 * @param settings settings
 */

void rdp_write_draw_nine_grid_cache_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint32 drawNineGridSupportLevel;

	header = rdp_capability_set_start(s);

	drawNineGridSupportLevel = (settings->draw_nine_grid) ? DRAW_NINEGRID_SUPPORTED : DRAW_NINEGRID_NO_SUPPORT;

	stream_write_uint32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	stream_write_uint16(s, settings->draw_nine_grid_cache_size); /* drawNineGridCacheSize (2 bytes) */
	stream_write_uint16(s, settings->draw_nine_grid_cache_entries); /* drawNineGridCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_DRAW_NINE_GRID_CACHE);
}

void rdp_write_gdiplus_cache_entries(STREAM* s, uint16 gce, uint16 bce, uint16 pce, uint16 ice, uint16 ace)
{
	stream_write_uint16(s, gce); /* gdipGraphicsCacheEntries (2 bytes) */
	stream_write_uint16(s, bce); /* gdipBrushCacheEntries (2 bytes) */
	stream_write_uint16(s, pce); /* gdipPenCacheEntries (2 bytes) */
	stream_write_uint16(s, ice); /* gdipImageCacheEntries (2 bytes) */
	stream_write_uint16(s, ace); /* gdipImageAttributesCacheEntries (2 bytes) */
}

void rdp_write_gdiplus_cache_chunk_size(STREAM* s, uint16 gccs, uint16 obccs, uint16 opccs, uint16 oiaccs)
{
	stream_write_uint16(s, gccs); /* gdipGraphicsCacheChunkSize (2 bytes) */
	stream_write_uint16(s, obccs); /* gdipObjectBrushCacheChunkSize (2 bytes) */
	stream_write_uint16(s, opccs); /* gdipObjectPenCacheChunkSize (2 bytes) */
	stream_write_uint16(s, oiaccs); /* gdipObjectImageAttributesCacheChunkSize (2 bytes) */
}

void rdp_write_gdiplus_image_cache_properties(STREAM* s, uint16 oiccs, uint16 oicts, uint16 oicms)
{
	stream_write_uint16(s, oiccs); /* gdipObjectImageCacheChunkSize (2 bytes) */
	stream_write_uint16(s, oicts); /* gdipObjectImageCacheTotalSize (2 bytes) */
	stream_write_uint16(s, oicms); /* gdipObjectImageCacheMaxSize (2 bytes) */
}

/**
 * Read GDI+ cache capability set.\n
 * @msdn{cc241566}
 * @param s stream
 * @param settings settings
 */

void rdp_read_draw_gdiplus_cache_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint32 drawGDIPlusSupportLevel;
	uint32 drawGdiplusCacheLevel;

	stream_read_uint32(s, drawGDIPlusSupportLevel); /* drawGDIPlusSupportLevel (4 bytes) */
	stream_seek_uint32(s); /* GdipVersion (4 bytes) */
	stream_read_uint32(s, drawGdiplusCacheLevel); /* drawGdiplusCacheLevel (4 bytes) */
	stream_seek(s, 10); /* GdipCacheEntries (10 bytes) */
	stream_seek(s, 8); /* GdipCacheChunkSize (8 bytes) */
	stream_seek(s, 6); /* GdipImageCacheProperties (6 bytes) */

	if (drawGDIPlusSupportLevel & DRAW_GDIPLUS_SUPPORTED)
		settings->draw_gdi_plus = true;

	if (drawGdiplusCacheLevel & DRAW_GDIPLUS_CACHE_LEVEL_ONE)
		settings->draw_gdi_plus_cache = true;
}

/**
 * Write GDI+ cache capability set.\n
 * @msdn{cc241566}
 * @param s stream
 * @param settings settings
 */

void rdp_write_draw_gdiplus_cache_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint32 drawGDIPlusSupportLevel;
	uint32 drawGdiplusCacheLevel;

	header = rdp_capability_set_start(s);

	drawGDIPlusSupportLevel = (settings->draw_gdi_plus) ? DRAW_GDIPLUS_SUPPORTED : DRAW_GDIPLUS_DEFAULT;
	drawGdiplusCacheLevel = (settings->draw_gdi_plus) ? DRAW_GDIPLUS_CACHE_LEVEL_ONE : DRAW_GDIPLUS_CACHE_LEVEL_DEFAULT;

	stream_write_uint32(s, drawGDIPlusSupportLevel); /* drawGDIPlusSupportLevel (4 bytes) */
	stream_write_uint32(s, 0); /* GdipVersion (4 bytes) */
	stream_write_uint32(s, drawGdiplusCacheLevel); /* drawGdiplusCacheLevel (4 bytes) */
	rdp_write_gdiplus_cache_entries(s, 10, 5, 5, 10, 2); /* GdipCacheEntries (10 bytes) */
	rdp_write_gdiplus_cache_chunk_size(s, 512, 2048, 1024, 64); /* GdipCacheChunkSize (8 bytes) */
	rdp_write_gdiplus_image_cache_properties(s, 4096, 256, 128); /* GdipImageCacheProperties (6 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_DRAW_GDI_PLUS);
}

/**
 * Read remote programs capability set.\n
 * @msdn{cc242518}
 * @param s stream
 * @param settings settings
 */

void rdp_read_remote_programs_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint32 railSupportLevel;

	stream_read_uint32(s, railSupportLevel); /* railSupportLevel (4 bytes) */

	if ((railSupportLevel & RAIL_LEVEL_SUPPORTED) == 0)
	{
		if (settings->remote_app == true)
		{
			/* RemoteApp Failure! */
			settings->remote_app = false;
		}
	}
}

/**
 * Write remote programs capability set.\n
 * @msdn{cc242518}
 * @param s stream
 * @param settings settings
 */

void rdp_write_remote_programs_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint32 railSupportLevel;

	header = rdp_capability_set_start(s);

	railSupportLevel = RAIL_LEVEL_SUPPORTED;

	if (settings->rail_langbar_supported)
		railSupportLevel |= RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED;

	stream_write_uint32(s, railSupportLevel); /* railSupportLevel (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_RAIL);
}

/**
 * Read window list capability set.\n
 * @msdn{cc242564}
 * @param s stream
 * @param settings settings
 */

void rdp_read_window_list_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint32(s); /* wndSupportLevel (4 bytes) */
	stream_seek_uint8(s); /* numIconCaches (1 byte) */
	stream_seek_uint16(s); /* numIconCacheEntries (2 bytes) */
}

/**
 * Write window list capability set.\n
 * @msdn{cc242564}
 * @param s stream
 * @param settings settings
 */

void rdp_write_window_list_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint32 wndSupportLevel;

	header = rdp_capability_set_start(s);

	wndSupportLevel = WINDOW_LEVEL_SUPPORTED_EX;

	stream_write_uint32(s, wndSupportLevel); /* wndSupportLevel (4 bytes) */
	stream_write_uint8(s, settings->num_icon_caches); /* numIconCaches (1 byte) */
	stream_write_uint16(s, settings->num_icon_cache_entries); /* numIconCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_WINDOW);
}

/**
 * Read desktop composition capability set.\n
 * @msdn{cc240855}
 * @param s stream
 * @param settings settings
 */

void rdp_read_desktop_composition_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint16(s); /* compDeskSupportLevel (2 bytes) */
}

/**
 * Write desktop composition capability set.\n
 * @msdn{cc240855}
 * @param s stream
 * @param settings settings
 */

void rdp_write_desktop_composition_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint16 compDeskSupportLevel;

	header = rdp_capability_set_start(s);

	compDeskSupportLevel = (settings->desktop_composition) ? COMPDESK_SUPPORTED : COMPDESK_NOT_SUPPORTED;

	stream_write_uint16(s, compDeskSupportLevel); /* compDeskSupportLevel (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_COMP_DESK);
}

/**
 * Read multifragment update capability set.\n
 * @msdn{cc240649}
 * @param s stream
 * @param settings settings
 */

void rdp_read_multifragment_update_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_read_uint32(s, settings->multifrag_max_request_size); /* MaxRequestSize (4 bytes) */
}

/**
 * Write multifragment update capability set.\n
 * @msdn{cc240649}
 * @param s stream
 * @param settings settings
 */

void rdp_write_multifragment_update_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;

	header = rdp_capability_set_start(s);

	stream_write_uint32(s, settings->multifrag_max_request_size); /* MaxRequestSize (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_MULTI_FRAGMENT_UPDATE);
}

/**
 * Read large pointer capability set.\n
 * @msdn{cc240650}
 * @param s stream
 * @param settings settings
 */

void rdp_read_large_pointer_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint16(s); /* largePointerSupportFlags (2 bytes) */
}

/**
 * Write large pointer capability set.\n
 * @msdn{cc240650}
 * @param s stream
 * @param settings settings
 */

void rdp_write_large_pointer_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint16 largePointerSupportFlags;

	header = rdp_capability_set_start(s);

	largePointerSupportFlags = (settings->large_pointer) ? LARGE_POINTER_FLAG_96x96 : 0;

	stream_write_uint16(s, largePointerSupportFlags); /* largePointerSupportFlags (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_LARGE_POINTER);
}

/**
 * Read surface commands capability set.\n
 * @msdn{dd871563}
 * @param s stream
 * @param settings settings
 */

void rdp_read_surface_commands_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint32(s); /* cmdFlags (4 bytes) */
	stream_seek_uint32(s); /* reserved (4 bytes) */

	settings->surface_commands = true;
}

/**
 * Write surface commands capability set.\n
 * @msdn{dd871563}
 * @param s stream
 * @param settings settings
 */

void rdp_write_surface_commands_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint32 cmdFlags;

	header = rdp_capability_set_start(s);

	cmdFlags = SURFCMDS_FRAME_MARKER |
			SURFCMDS_SET_SURFACE_BITS |
			SURFCMDS_STREAM_SURFACE_BITS;

	stream_write_uint32(s, cmdFlags); /* cmdFlags (4 bytes) */
	stream_write_uint32(s, 0); /* reserved (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SURFACE_COMMANDS);
}

/**
 * Read bitmap codecs capability set.\n
 * @msdn{dd891377}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_codecs_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	uint8 bitmapCodecCount;
	uint16 codecPropertiesLength;

	stream_read_uint8(s, bitmapCodecCount); /* bitmapCodecCount (1 byte) */

	if (settings->server_mode)
	{
		settings->rfx_codec = false;
		settings->ns_codec = false;
	}

	while (bitmapCodecCount > 0)
	{
		if (settings->server_mode && strncmp((char*)stream_get_tail(s), CODEC_GUID_REMOTEFX, 16) == 0)
		{
			stream_seek(s, 16); /* codecGUID (16 bytes) */
			stream_read_uint8(s, settings->rfx_codec_id);
			settings->rfx_codec = true;
		}
		else if (settings->server_mode && strncmp((char*)stream_get_tail(s),CODEC_GUID_NSCODEC, 16) == 0)
		{
			stream_seek(s, 16); /*codec GUID (16 bytes) */
			stream_read_uint8(s, settings->ns_codec_id);
			settings->ns_codec = true;
		}
		else
		{
			stream_seek(s, 16); /* codecGUID (16 bytes) */
			stream_seek_uint8(s); /* codecID (1 byte) */
		}

		stream_read_uint16(s, codecPropertiesLength); /* codecPropertiesLength (2 bytes) */
		stream_seek(s, codecPropertiesLength); /* codecProperties */

		bitmapCodecCount--;
	}
}

/**
 * Write RemoteFX Client Capability Container.\n
 * @param s stream
 * @param settings settings
 */
void rdp_write_rfx_client_capability_container(STREAM* s, rdpSettings* settings)
{
	uint32 captureFlags;
	uint8 codecMode;

	captureFlags = settings->dump_rfx ? 0 : CARDP_CAPS_CAPTURE_NON_CAC;
	codecMode = settings->rfx_codec_mode;

	stream_write_uint16(s, 49); /* codecPropertiesLength */

	/* TS_RFX_CLNT_CAPS_CONTAINER */
	stream_write_uint32(s, 49); /* length */
	stream_write_uint32(s, captureFlags); /* captureFlags */
	stream_write_uint32(s, 37); /* capsLength */

	/* TS_RFX_CAPS */
	stream_write_uint16(s, CBY_CAPS); /* blockType */
	stream_write_uint32(s, 8); /* blockLen */
	stream_write_uint16(s, 1); /* numCapsets */

	/* TS_RFX_CAPSET */
	stream_write_uint16(s, CBY_CAPSET); /* blockType */
	stream_write_uint32(s, 29); /* blockLen */
	stream_write_uint8(s, 0x01); /* codecId (MUST be set to 0x01) */
	stream_write_uint16(s, CLY_CAPSET); /* capsetType */
	stream_write_uint16(s, 2); /* numIcaps */
	stream_write_uint16(s, 8); /* icapLen */

	/* TS_RFX_ICAP (RLGR1) */
	stream_write_uint16(s, CLW_VERSION_1_0); /* version */
	stream_write_uint16(s, CT_TILE_64x64); /* tileSize */
	stream_write_uint8(s, codecMode); /* flags */
	stream_write_uint8(s, CLW_COL_CONV_ICT); /* colConvBits */
	stream_write_uint8(s, CLW_XFORM_DWT_53_A); /* transformBits */
	stream_write_uint8(s, CLW_ENTROPY_RLGR1); /* entropyBits */

	/* TS_RFX_ICAP (RLGR3) */
	stream_write_uint16(s, CLW_VERSION_1_0); /* version */
	stream_write_uint16(s, CT_TILE_64x64); /* tileSize */
	stream_write_uint8(s, codecMode); /* flags */
	stream_write_uint8(s, CLW_COL_CONV_ICT); /* colConvBits */
	stream_write_uint8(s, CLW_XFORM_DWT_53_A); /* transformBits */
	stream_write_uint8(s, CLW_ENTROPY_RLGR3); /* entropyBits */
}

/**
 * Write NSCODEC Client Capability Container.\n
 * @param s stream
 * @param settings settings
 */
void rdp_write_nsc_client_capability_container(STREAM* s, rdpSettings* settings)
{
	stream_write_uint16(s, 3); /* codecPropertiesLength */

	/* TS_NSCODEC_CAPABILITYSET */
	stream_write_uint8(s, 1);  /* fAllowDynamicFidelity */
	stream_write_uint8(s, 1);  /* fAllowSubsampling */
	stream_write_uint8(s, 3);  /* colorLossLevel */
}

/**
 * Write RemoteFX Server Capability Container.\n
 * @param s stream
 * @param settings settings
 */
void rdp_write_rfx_server_capability_container(STREAM* s, rdpSettings* settings)
{
	stream_write_uint16(s, 4); /* codecPropertiesLength */
	stream_write_uint32(s, 0); /* reserved */
}

/**
 * Write NSCODEC Server Capability Container.\n
 * @param s stream
 * @param settings settings
 */
void rdp_write_nsc_server_capability_container(STREAM* s, rdpSettings* settings)
{
	stream_write_uint16(s, 4); /* codecPropertiesLength */
	stream_write_uint32(s, 0); /* reserved */
}


/**
 * Write bitmap codecs capability set.\n
 * @msdn{dd891377}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_codecs_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;
	uint8 bitmapCodecCount;

	header = rdp_capability_set_start(s);

	bitmapCodecCount = 0;
	if (settings->rfx_codec)
		bitmapCodecCount++;
	if (settings->ns_codec)
		bitmapCodecCount++;

	stream_write_uint8(s, bitmapCodecCount);

	if (settings->rfx_codec)
	{
		stream_write(s, CODEC_GUID_REMOTEFX, 16); /* codecGUID */

		if (settings->server_mode)
		{
			stream_write_uint8(s, 0); /* codecID is defined by the client */
			rdp_write_rfx_server_capability_container(s, settings);
		}
		else
		{
			stream_write_uint8(s, CODEC_ID_REMOTEFX); /* codecID */
			rdp_write_rfx_client_capability_container(s, settings);
		}
	}
	if (settings->ns_codec)
	{
		stream_write(s, CODEC_GUID_NSCODEC, 16);
		if (settings->server_mode)
		{
			stream_write_uint8(s, 0); /* codecID is defined by the client */
			rdp_write_nsc_server_capability_container(s, settings);
		}
		else
		{
			stream_write_uint8(s, CODEC_ID_NSCODEC); /* codecID */
			rdp_write_nsc_client_capability_container(s, settings);
		}
	}
	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CODECS);
}

/**
 * Read frame acknowledge capability set.\n
 * @param s stream
 * @param settings settings
 */

void rdp_read_frame_acknowledge_capability_set(STREAM* s, uint16 length, rdpSettings* settings)
{
	stream_seek_uint32(s); /* (4 bytes) */
}

/**
 * Write frame acknowledge capability set.\n
 * @param s stream
 * @param settings settings
 */

void rdp_write_frame_acknowledge_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8* header;

	header = rdp_capability_set_start(s);

	stream_write_uint32(s, 2); /* (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_FRAME_ACKNOWLEDGE);
}

boolean rdp_read_capability_sets(STREAM* s, rdpSettings* settings, uint16 numberCapabilities)
{
	uint16 type;
	uint16 length;
	uint8 *bm, *em;

	while (numberCapabilities > 0)
	{
		stream_get_mark(s, bm);

		rdp_read_capability_set_header(s, &length, &type);
		//printf("%s Capability Set (0x%02X), length:%d\n", CAPSET_TYPE_STRINGS[type], type, length);
		settings->received_caps[type] = true;
		em = bm + length;

		if (stream_get_left(s) < length - 4)
		{
			printf("error processing stream\n");
			return false;
		}

		switch (type)
		{
			case CAPSET_TYPE_GENERAL:
				rdp_read_general_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_BITMAP:
				rdp_read_bitmap_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_ORDER:
				rdp_read_order_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_BITMAP_CACHE:
				rdp_read_bitmap_cache_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_CONTROL:
				rdp_read_control_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_ACTIVATION:
				rdp_read_window_activation_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_POINTER:
				rdp_read_pointer_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_SHARE:
				rdp_read_share_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_COLOR_CACHE:
				rdp_read_color_cache_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_SOUND:
				rdp_read_sound_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_INPUT:
				rdp_read_input_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_FONT:
				rdp_read_font_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_BRUSH:
				rdp_read_brush_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_GLYPH_CACHE:
				rdp_read_glyph_cache_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_OFFSCREEN_CACHE:
				rdp_read_offscreen_bitmap_cache_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT:
				rdp_read_bitmap_cache_host_support_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_BITMAP_CACHE_V2:
				rdp_read_bitmap_cache_v2_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_VIRTUAL_CHANNEL:
				rdp_read_virtual_channel_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_DRAW_NINE_GRID_CACHE:
				rdp_read_draw_nine_grid_cache_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_DRAW_GDI_PLUS:
				rdp_read_draw_gdiplus_cache_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_RAIL:
				rdp_read_remote_programs_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_WINDOW:
				rdp_read_window_list_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_COMP_DESK:
				rdp_read_desktop_composition_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_MULTI_FRAGMENT_UPDATE:
				rdp_read_multifragment_update_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_LARGE_POINTER:
				rdp_read_large_pointer_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_SURFACE_COMMANDS:
				rdp_read_surface_commands_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_BITMAP_CODECS:
				rdp_read_bitmap_codecs_capability_set(s, length, settings);
				break;

			case CAPSET_TYPE_FRAME_ACKNOWLEDGE:
				rdp_read_frame_acknowledge_capability_set(s, length, settings);
				break;

			default:
				printf("unknown capability type %d\n", type);
				break;
		}

		if (s->p != em)
		{
			printf("incorrect offset, type:0x%02X actual:%d expected:%d\n",
				type, (int) (s->p - bm), (int) (em - bm));
		}

		stream_set_mark(s, em);
		numberCapabilities--;
	}

	return true;
}

boolean rdp_recv_demand_active(rdpRdp* rdp, STREAM* s)
{
	uint16 length;
	uint16 channelId;
	uint16 pduType;
	uint16 pduLength;
	uint16 pduSource;
	uint16 numberCapabilities;
	uint16 lengthSourceDescriptor;
	uint16 lengthCombinedCapabilities;
	uint16 securityFlags;

	if (!rdp_read_header(rdp, s, &length, &channelId))
		return false;

	if (rdp->disconnect)
		return true;

	if (rdp->settings->encryption)
	{
		rdp_read_security_header(s, &securityFlags);
		if (securityFlags & SEC_ENCRYPT)
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				printf("rdp_decrypt failed\n");
				return false;
			}
		}
	}

	if (channelId != MCS_GLOBAL_CHANNEL_ID)
	{
		printf("channelId bad\n");
		return false;
	}

	if (!rdp_read_share_control_header(s, &pduLength, &pduType, &pduSource))
	{
		printf("rdp_read_share_control_header failed\n");
		return false;
	}

	rdp->settings->pdu_source = pduSource;

	if (pduType != PDU_TYPE_DEMAND_ACTIVE)
	{
		printf("pduType bad\n");
		return false;
	}

	stream_read_uint32(s, rdp->settings->share_id); /* shareId (4 bytes) */
	stream_read_uint16(s, lengthSourceDescriptor); /* lengthSourceDescriptor (2 bytes) */
	stream_read_uint16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */
	stream_seek(s, lengthSourceDescriptor); /* sourceDescriptor */
	stream_read_uint16(s, numberCapabilities); /* numberCapabilities (2 bytes) */
	stream_seek(s, 2); /* pad2Octets (2 bytes) */

	/* capabilitySets */
	if (!rdp_read_capability_sets(s, rdp->settings, numberCapabilities))
	{
		printf("rdp_read_capability_sets failed\n");
		return false;
	}

	rdp->update->secondary->glyph_v2 = (rdp->settings->glyphSupportLevel > GLYPH_SUPPORT_FULL) ? true : false;

	return true;
}

void rdp_write_demand_active(STREAM* s, rdpSettings* settings)
{
	uint8 *bm, *em, *lm;
	uint16 numberCapabilities;
	uint16 lengthCombinedCapabilities;

	stream_write_uint32(s, settings->share_id); /* shareId (4 bytes) */
	stream_write_uint16(s, 4); /* lengthSourceDescriptor (2 bytes) */

	stream_get_mark(s, lm);
	stream_seek_uint16(s); /* lengthCombinedCapabilities (2 bytes) */
	stream_write(s, "RDP", 4); /* sourceDescriptor */

	stream_get_mark(s, bm);
	stream_seek_uint16(s); /* numberCapabilities (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */

	numberCapabilities = 14;
	rdp_write_general_capability_set(s, settings);
	rdp_write_bitmap_capability_set(s, settings);
	rdp_write_order_capability_set(s, settings);
	rdp_write_pointer_capability_set(s, settings);
	rdp_write_input_capability_set(s, settings);
	rdp_write_virtual_channel_capability_set(s, settings);
	rdp_write_bitmap_cache_host_support_capability_set(s, settings);
	rdp_write_share_capability_set(s, settings);
	rdp_write_font_capability_set(s, settings);
	rdp_write_multifragment_update_capability_set(s, settings);
	rdp_write_large_pointer_capability_set(s, settings);
	rdp_write_desktop_composition_capability_set(s, settings);
	rdp_write_surface_commands_capability_set(s, settings);
	rdp_write_bitmap_codecs_capability_set(s, settings);

	stream_get_mark(s, em);

	stream_set_mark(s, lm); /* go back to lengthCombinedCapabilities */
	lengthCombinedCapabilities = (em - bm);
	stream_write_uint16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */

	stream_set_mark(s, bm); /* go back to numberCapabilities */
	stream_write_uint16(s, numberCapabilities); /* numberCapabilities (2 bytes) */

	stream_set_mark(s, em);

	stream_write_uint32(s, 0); /* sessionId */
}

boolean rdp_send_demand_active(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_pdu_init(rdp);

	rdp->settings->share_id = 0x10000 + rdp->mcs->user_id;

	rdp_write_demand_active(s, rdp->settings);

	rdp_send_pdu(rdp, s, PDU_TYPE_DEMAND_ACTIVE, rdp->mcs->user_id);

	return true;
}

boolean rdp_recv_confirm_active(rdpRdp* rdp, STREAM* s)
{
	uint16 length;
	uint16 channelId;
	uint16 pduType;
	uint16 pduLength;
	uint16 pduSource;
	uint16 lengthSourceDescriptor;
	uint16 lengthCombinedCapabilities;
	uint16 numberCapabilities;
	uint16 securityFlags;

	if (!rdp_read_header(rdp, s, &length, &channelId))
		return false;

	if (rdp->settings->encryption)
	{
		rdp_read_security_header(s, &securityFlags);
		if (securityFlags & SEC_ENCRYPT)
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				printf("rdp_decrypt failed\n");
				return false;
			}
		}
	}

	if (channelId != MCS_GLOBAL_CHANNEL_ID)
		return false;

	if (!rdp_read_share_control_header(s, &pduLength, &pduType, &pduSource))
		return false;

	rdp->settings->pdu_source = pduSource;

	if (pduType != PDU_TYPE_CONFIRM_ACTIVE)
		return false;

	stream_seek_uint32(s); /* shareId (4 bytes) */
	stream_seek_uint16(s); /* originatorId (2 bytes) */
	stream_read_uint16(s, lengthSourceDescriptor); /* lengthSourceDescriptor (2 bytes) */
	stream_read_uint16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */
	stream_seek(s, lengthSourceDescriptor); /* sourceDescriptor */
	stream_read_uint16(s, numberCapabilities); /* numberCapabilities (2 bytes) */
	stream_seek(s, 2); /* pad2Octets (2 bytes) */

	if (!rdp_read_capability_sets(s, rdp->settings, numberCapabilities))
		return false;

	return true;
}

void rdp_write_confirm_active(STREAM* s, rdpSettings* settings)
{
	uint8 *bm, *em, *lm;
	uint16 numberCapabilities;
	uint16 lengthSourceDescriptor;
	uint16 lengthCombinedCapabilities;

	lengthSourceDescriptor = sizeof(SOURCE_DESCRIPTOR);

	stream_write_uint32(s, settings->share_id); /* shareId (4 bytes) */
	stream_write_uint16(s, 0x03EA); /* originatorId (2 bytes) */
	stream_write_uint16(s, lengthSourceDescriptor);/* lengthSourceDescriptor (2 bytes) */

	stream_get_mark(s, lm);
	stream_seek_uint16(s); /* lengthCombinedCapabilities (2 bytes) */
	stream_write(s, SOURCE_DESCRIPTOR, lengthSourceDescriptor); /* sourceDescriptor */

	stream_get_mark(s, bm);
	stream_seek_uint16(s); /* numberCapabilities (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */

	/* Capability Sets */
	numberCapabilities = 15;
	rdp_write_general_capability_set(s, settings);
	rdp_write_bitmap_capability_set(s, settings);
	rdp_write_order_capability_set(s, settings);

	if (settings->rdp_version >= 5)
		rdp_write_bitmap_cache_v2_capability_set(s, settings);
	else
		rdp_write_bitmap_cache_capability_set(s, settings);

	rdp_write_pointer_capability_set(s, settings);
	rdp_write_input_capability_set(s, settings);
	rdp_write_brush_capability_set(s, settings);
	rdp_write_glyph_cache_capability_set(s, settings);
	rdp_write_virtual_channel_capability_set(s, settings);
	rdp_write_sound_capability_set(s, settings);
	rdp_write_share_capability_set(s, settings);
	rdp_write_font_capability_set(s, settings);
	rdp_write_control_capability_set(s, settings);
	rdp_write_color_cache_capability_set(s, settings);
	rdp_write_window_activation_capability_set(s, settings);

	if (settings->offscreen_bitmap_cache)
	{
		numberCapabilities++;
		rdp_write_offscreen_bitmap_cache_capability_set(s, settings);
	}

	if (settings->received_caps[CAPSET_TYPE_LARGE_POINTER])
	{
		if (settings->large_pointer)
		{
			numberCapabilities++;
			rdp_write_large_pointer_capability_set(s, settings);
		}
	}

	if (settings->remote_app)
	{
		numberCapabilities += 2;
		rdp_write_remote_programs_capability_set(s, settings);
		rdp_write_window_list_capability_set(s, settings);
	}

	if (settings->received_caps[CAPSET_TYPE_MULTI_FRAGMENT_UPDATE])
	{
		numberCapabilities++;
		rdp_write_multifragment_update_capability_set(s, settings);
	}

	if (settings->received_caps[CAPSET_TYPE_SURFACE_COMMANDS])
	{
		numberCapabilities++;
		rdp_write_surface_commands_capability_set(s, settings);
	}

	if (settings->received_caps[CAPSET_TYPE_BITMAP_CODECS])
	{
		numberCapabilities++;
		rdp_write_bitmap_codecs_capability_set(s, settings);
	}

	if (settings->received_caps[CAPSET_TYPE_FRAME_ACKNOWLEDGE])
	{
		if (settings->frame_acknowledge)
		{
			numberCapabilities++;
			rdp_write_frame_acknowledge_capability_set(s, settings);
		}
	}

	stream_get_mark(s, em);

	stream_set_mark(s, lm); /* go back to lengthCombinedCapabilities */
	lengthCombinedCapabilities = (em - bm);
	stream_write_uint16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */

	stream_set_mark(s, bm); /* go back to numberCapabilities */
	stream_write_uint16(s, numberCapabilities); /* numberCapabilities (2 bytes) */

	stream_set_mark(s, em);
}

boolean rdp_send_confirm_active(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_pdu_init(rdp);

	rdp_write_confirm_active(s, rdp->settings);

	return rdp_send_pdu(rdp, s, PDU_TYPE_CONFIRM_ACTIVE, rdp->mcs->user_id);
}


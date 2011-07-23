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

void rdp_write_capability_set_header(STREAM* s, uint16 length, uint16 type)
{
	stream_write_uint16(s, type); /* capabilitySetType */
	stream_write_uint16(s, length); /* lengthCapability */
}

uint8* rdp_capability_set_start(STREAM* s)
{
	uint8* header;

	stream_get_mark(s, header);
	stream_seek(s, CAPSET_HEADER_LENGTH);

	return header;
}

void rdp_capability_set_finish(STREAM* s, uint8* header, uint16 type)
{
	uint16 length;

	length = s->p - header;
	stream_set_mark(s, header);
	rdp_write_capability_set_header(s, length, type);
	stream_set_mark(s, header + length);
}

/**
 * Read general capability set.\n
 * @msdn{cc240549}
 * @param s stream
 * @param settings settings
 */

void rdp_read_general_capability_set(STREAM* s, rdpSettings* settings)
{
	uint16 extraFlags;
	uint8 refreshRectSupport;
	uint8 suppressOutputSupport;

	stream_seek_uint16(s); /* osMajorType (2 bytes) */
	stream_seek_uint16(s); /* osMinorType (2 bytes) */
	stream_seek_uint16(s); /* protocolVersion (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsA (2 bytes) */
	stream_seek_uint16(s); /* generalCompressionTypes (2 bytes) */
	stream_read_uint16(s, extraFlags); /* extraFlags (2 bytes) */
	stream_seek_uint16(s); /* updateCapabilityFlag (2 bytes) */
	stream_seek_uint16(s); /* remoteUnshareFlag (2 bytes) */
	stream_seek_uint16(s); /* generalCompressionLevel (2 bytes) */
	stream_read_uint8(s, refreshRectSupport); /* refreshRectSupport (1 byte) */
	stream_read_uint8(s, suppressOutputSupport); /* suppressOutputSupport (1 byte) */

	if (refreshRectSupport == False)
		settings->refresh_rect = False;

	if (suppressOutputSupport == False)
		settings->suppress_output = False;
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

	if (settings->fast_path_input)
		extraFlags |= FASTPATH_OUTPUT_SUPPORTED;

	stream_write_uint16(s, 0); /* osMajorType (2 bytes) */
	stream_write_uint16(s, 0); /* osMinorType (2 bytes) */
	stream_write_uint16(s, CAPS_PROTOCOL_VERSION); /* protocolVersion (2 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsA (2 bytes) */
	stream_write_uint16(s, 0); /* generalCompressionTypes (2 bytes) */
	stream_write_uint16(s, 0); /* extraFlags (2 bytes) */
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

void rdp_read_bitmap_capability_set(STREAM* s, rdpSettings* settings)
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

	if (desktopResizeFlag == False)
		settings->desktop_resize = False;
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
	uint16 preferredBitsPerPixel;

	header = rdp_capability_set_start(s);

	drawingFlags = 0;

	if (settings->rdp_version > 5)
		preferredBitsPerPixel = settings->color_depth;
	else
		preferredBitsPerPixel = 8;

	stream_write_uint16(s, preferredBitsPerPixel); /* preferredBitsPerPixel (2 bytes) */
	stream_write_uint16(s, 1); /* receive1BitPerPixel (2 bytes) */
	stream_write_uint16(s, 1); /* receive4BitsPerPixel (2 bytes) */
	stream_write_uint16(s, 1); /* receive8BitsPerPixel (2 bytes) */
	stream_write_uint16(s, settings->width); /* desktopWidth (2 bytes) */
	stream_write_uint16(s, settings->height); /* desktopHeight (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */
	stream_write_uint16(s, settings->desktop_resize); /* desktopResizeFlag (2 bytes) */
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

void rdp_read_order_capability_set(STREAM* s, rdpSettings* settings)
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
		if (orderSupport[i] == False)
			settings->order_support[i] = False;
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

	header = rdp_capability_set_start(s);

	orderFlags = NEGOTIATE_ORDER_SUPPORT |
			ZERO_BOUNDS_DELTA_SUPPORT |
			COLOR_INDEX_SUPPORT |
			ORDER_FLAGS_EXTRA_SUPPORT;

	orderSupportExFlags = 0;

	if (settings->frame_marker)
		orderSupportExFlags |= CACHE_BITMAP_V3_SUPPORT;

	if (settings->bitmap_cache_v3)
		orderSupportExFlags |= ALTSEC_FRAME_MARKER_SUPPORT;

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

void rdp_read_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
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

void rdp_read_control_capability_set(STREAM* s, rdpSettings* settings)
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

void rdp_read_window_activation_capability_set(STREAM* s, rdpSettings* settings)
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

void rdp_read_pointer_capability_set(STREAM* s, rdpSettings* settings)
{
	uint16 colorPointerFlag;
	uint16 colorPointerCacheSize;
	uint16 pointerCacheSize;

	stream_read_uint16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	stream_read_uint16(s, colorPointerCacheSize); /* colorPointerCacheSize (2 bytes) */
	stream_read_uint16(s, pointerCacheSize); /* pointerCacheSize (2 bytes) */

	if (colorPointerFlag == False)
		settings->color_pointer = False;
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

	colorPointerFlag = (settings->color_pointer) ? True : False;

	stream_write_uint16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	stream_write_uint16(s, 20); /* colorPointerCacheSize (2 bytes) */
	stream_write_uint16(s, 20); /* pointerCacheSize (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_POINTER);
}

/**
 * Read share capability set.\n
 * @msdn{cc240570}
 * @param s stream
 * @param settings settings
 */

void rdp_read_share_capability_set(STREAM* s, rdpSettings* settings)
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

	header = rdp_capability_set_start(s);

	stream_write_uint16(s, 0); /* nodeId (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SHARE);
}

/**
 * Read color cache capability set.\n
 * @msdn{cc241564}
 * @param s stream
 * @param settings settings
 */

void rdp_read_color_cache_capability_set(STREAM* s, rdpSettings* settings)
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

void rdp_read_sound_capability_set(STREAM* s, rdpSettings* settings)
{
	uint16 soundFlags;

	stream_read_uint16(s, soundFlags); /* soundFlags (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsA (2 bytes) */

	settings->sound_beeps = (soundFlags & SOUND_BEEPS_FLAG) ? True : False;
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

void rdp_read_input_capability_set(STREAM* s, rdpSettings* settings)
{
	uint16 inputFlags;

	stream_read_uint16(s, inputFlags); /* inputFlags (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsA (2 bytes) */
	stream_read_uint32(s, settings->kbd_layout); /* keyboardLayout (4 bytes) */
	stream_read_uint32(s, settings->kbd_type); /* keyboardType (4 bytes) */
	stream_read_uint32(s, settings->kbd_subtype); /* keyboardSubType (4 bytes) */
	stream_read_uint32(s, settings->kbd_fn_keys); /* keyboardFunctionKeys (4 bytes) */
	stream_seek(s, 64); /* imeFileName (64 bytes) */

	if ((inputFlags & INPUT_FLAG_FASTPATH_INPUT) || (inputFlags & INPUT_FLAG_FASTPATH_INPUT2))
	{
		if (settings->fast_path_input != False)
			settings->fast_path_input = True;
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

	if (settings->fast_path_input)
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

void rdp_read_font_capability_set(STREAM* s, rdpSettings* settings)
{
	stream_seek_uint16(s); /* fontSupportFlags (2 bytes) */
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

void rdp_read_brush_capability_set(STREAM* s, rdpSettings* settings)
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
void rdp_read_cache_definition(STREAM* s, uint16* numEntries, uint16* maxCellSize)
{
	stream_read_uint16(s, *numEntries); /* cacheEntries (2 bytes) */
	stream_read_uint16(s, *maxCellSize); /* cacheMaximumCellSize (2 bytes) */
}

/**
 * Write cache definition (glyph).\n
 * @msdn{cc240566}
 * @param s stream
 */
void rdp_write_cache_definition(STREAM* s, uint16 numEntries, uint16 maxCellSize)
{
	stream_write_uint16(s, numEntries); /* cacheEntries (2 bytes) */
	stream_write_uint16(s, maxCellSize); /* cacheMaximumCellSize (2 bytes) */
}

/**
 * Read glyph cache capability set.\n
 * @msdn{cc240565}
 * @param s stream
 * @param settings settings
 */

void rdp_read_glyph_cache_capability_set(STREAM* s, rdpSettings* settings)
{
	stream_seek(s, 40); /* glyphCache (40 bytes) */
	stream_seek_uint32(s); /* fragCache (4 bytes) */
	stream_seek_uint16(s); /* glyphSupportLevel (2 bytes) */
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */
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
	rdp_write_cache_definition(s, 254, 4);
	rdp_write_cache_definition(s, 254, 4);
	rdp_write_cache_definition(s, 254, 8);
	rdp_write_cache_definition(s, 254, 8);
	rdp_write_cache_definition(s, 254, 16);
	rdp_write_cache_definition(s, 254, 32);
	rdp_write_cache_definition(s, 254, 64);
	rdp_write_cache_definition(s, 254, 256);
	rdp_write_cache_definition(s, 64, 2048);

	rdp_write_cache_definition(s, 64, 2048); /* fragCache */

	stream_write_uint16(s, GLYPH_SUPPORT_FULL); /* glyphSupportLevel (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_GLYPH_CACHE);
}

/**
 * Read offscreen bitmap cache capability set.\n
 * @msdn{cc240550}
 * @param s stream
 * @param settings settings
 */

void rdp_read_offscreen_bitmap_cache_capability_set(STREAM* s, rdpSettings* settings)
{
	uint32 offscreenSupportLevel;
	uint16 offscreenCacheSize;
	uint16 offscreenCacheEntries;

	stream_read_uint32(s, offscreenSupportLevel); /* offscreenSupportLevel (4 bytes) */
	stream_read_uint16(s, settings->offscreen_bitmap_cache_size); /* offscreenCacheSize (2 bytes) */
	stream_read_uint16(s, settings->offscreen_bitmap_cache_entries); /* offscreenCacheEntries (2 bytes) */

	if (offscreenSupportLevel & True)
		settings->offscreen_bitmap_cache = True;
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

	header = rdp_capability_set_start(s);

	stream_read_uint32(s, settings->offscreen_bitmap_cache); /* offscreenSupportLevel (4 bytes) */
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

void rdp_read_bitmap_cache_host_support_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8 cacheVersion;

	stream_read_uint8(s, cacheVersion); /* cacheVersion (1 byte) */
	stream_seek_uint8(s); /* pad1 (1 byte) */
	stream_seek_uint16(s); /* pad2 (2 bytes) */

	if (cacheVersion & BITMAP_CACHE_V2)
		settings->persistent_bitmap_cache = True;
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

void rdp_write_bitmap_cache_cell_info(STREAM* s, uint16 numEntries, boolean persistent)
{
	uint32 info;

	/**
	 * numEntries is in the first 31 bits, while the last bit (k)
	 * is used to indicate a persistent bitmap cache.
	 */

	info = numEntries & persistent;

	stream_write_uint32(s, info);
}

/**
 * Read bitmap cache v2 capability set.\n
 * @msdn{cc240560}
 * @param s stream
 * @param settings settings
 */

void rdp_read_bitmap_cache_v2_capability_set(STREAM* s, rdpSettings* settings)
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
	stream_write_uint8(s, 5); /* numCellCaches (1 byte) */
	rdp_write_bitmap_cache_cell_info(s, 600, 0); /* bitmapCache0CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, 600, 0); /* bitmapCache1CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, 2048, 0); /* bitmapCache2CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, 4096, 0); /* bitmapCache3CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, 2048, 0); /* bitmapCache4CellInfo (4 bytes) */
	stream_write_zero(s, 12); /* pad3 (12 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_V2);
}

/**
 * Read virtual channel capability set.\n
 * @msdn{cc240551}
 * @param s stream
 * @param settings settings
 */

void rdp_read_virtual_channel_capability_set(STREAM* s, rdpSettings* settings)
{
	stream_seek_uint32(s); /* flags (4 bytes) */
	stream_read_uint32(s, settings->vc_chunk_size); /* VCChunkSize (4 bytes) */
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

	header = rdp_capability_set_start(s);

	stream_write_uint32(s, VCCAPS_COMPR_SC); /* flags (4 bytes) */
	stream_write_uint32(s, 0); /* VCChunkSize (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_VIRTUAL_CHANNEL);
}

/**
 * Read drawn nine grid cache capability set.\n
 * @msdn{cc241565}
 * @param s stream
 * @param settings settings
 */

void rdp_read_draw_nine_grid_cache_capability_set(STREAM* s, rdpSettings* settings)
{
	uint32 drawNineGridSupportLevel;
	uint16 drawNineGridCacheSize;
	uint16 drawNineGridCacheEntries;

	stream_read_uint32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	stream_read_uint16(s, settings->draw_nine_grid_cache_size); /* drawNineGridCacheSize (2 bytes) */
	stream_read_uint16(s, settings->draw_nine_grid_cache_entries); /* drawNineGridCacheEntries (2 bytes) */

	if ((drawNineGridSupportLevel & DRAW_NINEGRID_SUPPORTED) ||
			(drawNineGridSupportLevel & DRAW_NINEGRID_SUPPORTED_V2))
		settings->draw_nine_grid = True;
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

	stream_read_uint32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	stream_read_uint16(s, settings->draw_nine_grid_cache_size); /* drawNineGridCacheSize (2 bytes) */
	stream_read_uint16(s, settings->draw_nine_grid_cache_entries); /* drawNineGridCacheEntries (2 bytes) */

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

void rdp_read_draw_gdiplus_cache_capability_set(STREAM* s, rdpSettings* settings)
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
		settings->draw_gdi_plus = True;

	if (drawGdiplusCacheLevel & DRAW_GDIPLUS_CACHE_LEVEL_ONE)
		settings->draw_gdi_plus_cache = True;
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

void rdp_read_remote_programs_capability_set(STREAM* s, rdpSettings* settings)
{
	uint32 railSupportLevel;

	stream_read_uint32(s, railSupportLevel); /* railSupportLevel (4 bytes) */

	if ((railSupportLevel & RAIL_LEVEL_SUPPORTED) == 0)
	{
		if (settings->remote_app == True)
		{
			/* RemoteApp Failure! */
			settings->remote_app = False;
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
	uint32 railSupportLevel = 0;

	header = rdp_capability_set_start(s);

	if (settings->remote_app)
		railSupportLevel = RAIL_LEVEL_SUPPORTED | RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED;

	stream_read_uint32(s, railSupportLevel); /* railSupportLevel (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_RAIL);
}

/**
 * Read window list capability set.\n
 * @msdn{cc242564}
 * @param s stream
 * @param settings settings
 */

void rdp_read_window_list_capability_set(STREAM* s, rdpSettings* settings)
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

	wndSupportLevel = WINDOW_LEVEL_SUPPORTED | WINDOW_LEVEL_SUPPORTED_EX;

	stream_write_uint32(s, wndSupportLevel); /* wndSupportLevel (4 bytes) */
	stream_write_uint8(s, 3); /* numIconCaches (1 byte) */
	stream_write_uint16(s, 12); /* numIconCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_WINDOW);
}

/**
 * Read desktop composition capability set.\n
 * @msdn{cc240855}
 * @param s stream
 * @param settings settings
 */

void rdp_read_desktop_composition_capability_set(STREAM* s, rdpSettings* settings)
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

void rdp_read_multifragment_update_capability_set(STREAM* s, rdpSettings* settings)
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

void rdp_read_large_pointer_capability_set(STREAM* s, rdpSettings* settings)
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

void rdp_read_surface_commands_capability_set(STREAM* s, rdpSettings* settings)
{
	stream_seek_uint32(s); /* cmdFlags (4 bytes) */
	stream_seek_uint32(s); /* reserved (4 bytes) */

	settings->surface_commands = True;
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

void rdp_read_bitmap_codecs_capability_set(STREAM* s, rdpSettings* settings)
{
	uint8 bitmapCodecCount;
	uint16 codecPropertiesLength;

	stream_read_uint8(s, bitmapCodecCount); /* bitmapCodecCount (1 byte) */

	while (bitmapCodecCount > 0)
	{
		stream_seek(s, 16); /* codecGUID (16 bytes) */
		stream_seek_uint8(s); /* codecID (1 byte) */

		stream_read_uint16(s, codecPropertiesLength); /* codecPropertiesLength (2 bytes) */
		stream_seek(s, codecPropertiesLength); /* codecProperties */

		bitmapCodecCount--;
	}
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

	header = rdp_capability_set_start(s);



	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CODECS);
}

/**
 * Read frame acknowledge capability set.\n
 * @param s stream
 * @param settings settings
 */

void rdp_read_frame_acknowledge_capability_set(STREAM* s, rdpSettings* settings)
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

void rdp_read_demand_active(STREAM* s, rdpSettings* settings)
{
	uint16 type;
	uint16 length;
	uint8 *bm, *em;
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
		stream_get_mark(s, bm);

		rdp_read_capability_set_header(s, &length, &type);
		printf("%s Capability Set (0x%02X), length:%d\n", CAPSET_TYPE_STRINGS[type], type, length);

		em = bm + length;

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
				rdp_read_draw_gdiplus_cache_capability_set(s, settings);
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

			case CAPSET_TYPE_FRAME_ACKNOWLEDGE:
				rdp_read_frame_acknowledge_capability_set(s, settings);
				break;

			default:
				break;
		}

		if (s->p != em)
			printf("incorrect offset, actual:%d expected:%d\n", (int) (s->p - bm), (int) (em - bm));

		stream_set_mark(s, em);
		numberCapabilities--;
	}
}

void rdp_recv_demand_active(rdpRdp* rdp, STREAM* s, rdpSettings* settings)
{
	rdp_read_demand_active(s, settings);
	rdp_send_confirm_active(rdp);
	rdp_send_client_synchronize_pdu(rdp);
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
	rdp_write_bitmap_cache_capability_set(s, settings);
	rdp_write_pointer_capability_set(s, settings);
	rdp_write_input_capability_set(s, settings);
	rdp_write_brush_capability_set(s, settings);
	rdp_write_glyph_cache_capability_set(s, settings);
	rdp_write_offscreen_bitmap_cache_capability_set(s, settings);
	rdp_write_virtual_channel_capability_set(s, settings);
	rdp_write_sound_capability_set(s, settings);
	rdp_write_share_capability_set(s, settings);
	rdp_write_control_capability_set(s, settings);
	rdp_write_color_cache_capability_set(s, settings);
	rdp_write_window_activation_capability_set(s, settings);

	stream_get_mark(s, em);

	stream_set_mark(s, lm); /* go back to lengthCombinedCapabilities */
	lengthCombinedCapabilities = (em - bm);
	stream_write_uint16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */

	stream_set_mark(s, bm); /* go back to numberCapabilities */
	stream_write_uint16(s, numberCapabilities); /* numberCapabilities (2 bytes) */

	stream_set_mark(s, em);
}

void rdp_send_confirm_active(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_pdu_init(rdp);

	rdp_write_confirm_active(s, rdp->settings);

	rdp_send_pdu(rdp, s, PDU_TYPE_CONFIRM_ACTIVE, MCS_BASE_CHANNEL_ID + rdp->mcs->user_id);
}

void rdp_read_deactivate_all(STREAM* s, rdpSettings* settings)
{
	printf("Deactivate All PDU\n");
}

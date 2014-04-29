/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "capabilities.h"

#include <winpr/crt.h>
#include <winpr/rpc.h>

#ifdef WITH_DEBUG_CAPABILITIES

const char* const CAPSET_TYPE_STRINGS[] =
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

#endif

BOOL rdp_print_capability_sets(wStream* s, UINT16 numberCapabilities, BOOL receiving);

/* CODEC_GUID_REMOTEFX: 0x76772F12BD724463AFB3B73C9C6F7886 */

GUID CODEC_GUID_REMOTEFX =
{
	0x76772F12,
	0xBD72, 0x4463,
	{ 0xAF, 0xB3, 0xB7, 0x3C, 0x9C, 0x6F, 0x78, 0x86 }
};

/* CODEC_GUID_NSCODEC 0xCA8D1BB9000F154F589FAE2D1A87E2D6 */

GUID CODEC_GUID_NSCODEC =
{
	0xCA8D1BB9,
	0x000F, 0x154F,
	{ 0x58, 0x9F, 0xAE, 0x2D, 0x1A, 0x87, 0xE2, 0xD6 }
};

/* CODEC_GUID_IGNORE 0x9C4351A6353542AE910CCDFCE5760B58 */

GUID CODEC_GUID_IGNORE =
{
	0x9C4351A6,
	0x3535, 0x42AE,
	{ 0x91, 0x0C, 0xCD, 0xFC, 0xE5, 0x76, 0x0B, 0x58 }
};

/* CODEC_GUID_IMAGE_REMOTEFX 0x2744CCD49D8A4E74803C0ECBEEA19C54 */

GUID CODEC_GUID_IMAGE_REMOTEFX =
{
	0x2744CCD4,
	0x9D8A, 0x4E74,
	{ 0x80, 0x3C, 0x0E, 0xCB, 0xEE, 0xA1, 0x9C, 0x54 }
};

/* CODEC_GUID_JPEG 0x430C9EED1BAF4CE6869ACB8B37B66237 */

GUID CODEC_GUID_JPEG =
{
	0x430C9EED,
	0x1BAF, 0x4CE6,
	{ 0x86, 0x9A, 0xCB, 0x8B, 0x37, 0xB6, 0x62, 0x37 }
};

void rdp_read_capability_set_header(wStream* s, UINT16* length, UINT16* type)
{
	Stream_Read_UINT16(s, *type); /* capabilitySetType */
	Stream_Read_UINT16(s, *length); /* lengthCapability */
}

void rdp_write_capability_set_header(wStream* s, UINT16 length, UINT16 type)
{
	Stream_Write_UINT16(s, type); /* capabilitySetType */
	Stream_Write_UINT16(s, length); /* lengthCapability */
}

int rdp_capability_set_start(wStream* s)
{
	int header;

	header = Stream_GetPosition(s);
	Stream_Zero(s, CAPSET_HEADER_LENGTH);

	return header;
}

void rdp_capability_set_finish(wStream* s, int header, UINT16 type)
{
	int footer;
	UINT16 length;

	footer = Stream_GetPosition(s);
	length = footer - header;
	Stream_SetPosition(s, header);

	rdp_write_capability_set_header(s, length, type);
	Stream_SetPosition(s, footer);
}

/**
 * Read general capability set.\n
 * @msdn{cc240549}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_general_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT16 extraFlags;
	BYTE refreshRectSupport;
	BYTE suppressOutputSupport;

	if (length < 24)
		return FALSE;

	if (settings->ServerMode)
	{
		Stream_Read_UINT16(s, settings->OsMajorType); /* osMajorType (2 bytes) */
		Stream_Read_UINT16(s, settings->OsMinorType); /* osMinorType (2 bytes) */
	}
	else
	{
		Stream_Seek_UINT16(s); /* osMajorType (2 bytes) */
		Stream_Seek_UINT16(s); /* osMinorType (2 bytes) */
	}

	Stream_Seek_UINT16(s); /* protocolVersion (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2OctetsA (2 bytes) */
	Stream_Seek_UINT16(s); /* generalCompressionTypes (2 bytes) */
	Stream_Read_UINT16(s, extraFlags); /* extraFlags (2 bytes) */
	Stream_Seek_UINT16(s); /* updateCapabilityFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* remoteUnshareFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* generalCompressionLevel (2 bytes) */
	Stream_Read_UINT8(s, refreshRectSupport); /* refreshRectSupport (1 byte) */
	Stream_Read_UINT8(s, suppressOutputSupport); /* suppressOutputSupport (1 byte) */

	settings->NoBitmapCompressionHeader = (extraFlags & NO_BITMAP_COMPRESSION_HDR) ? TRUE : FALSE;

	if (!(extraFlags & FASTPATH_OUTPUT_SUPPORTED))
		settings->FastPathOutput = FALSE;

	if (refreshRectSupport == FALSE)
		settings->RefreshRect = FALSE;

	if (suppressOutputSupport == FALSE)
		settings->SuppressOutput = FALSE;

	return TRUE;
}

/**
 * Write general capability set.\n
 * @msdn{cc240549}
 * @param s stream
 * @param settings settings
 */

void rdp_write_general_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 extraFlags;

	Stream_EnsureRemainingCapacity(s, 64);

	header = rdp_capability_set_start(s);

	extraFlags = LONG_CREDENTIALS_SUPPORTED;

	if (settings->NoBitmapCompressionHeader)
		extraFlags |= NO_BITMAP_COMPRESSION_HDR;

	if (settings->AutoReconnectionEnabled)
		extraFlags |= AUTORECONNECT_SUPPORTED;

	if (settings->FastPathOutput)
		extraFlags |= FASTPATH_OUTPUT_SUPPORTED;

	if (settings->SaltedChecksum)
		extraFlags |= ENC_SALTED_CHECKSUM;

	Stream_Write_UINT16(s, settings->OsMajorType); /* osMajorType (2 bytes) */
	Stream_Write_UINT16(s, settings->OsMinorType); /* osMinorType (2 bytes) */
	Stream_Write_UINT16(s, CAPS_PROTOCOL_VERSION); /* protocolVersion (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsA (2 bytes) */
	Stream_Write_UINT16(s, 0); /* generalCompressionTypes (2 bytes) */
	Stream_Write_UINT16(s, extraFlags); /* extraFlags (2 bytes) */
	Stream_Write_UINT16(s, 0); /* updateCapabilityFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* remoteUnshareFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* generalCompressionLevel (2 bytes) */
	Stream_Write_UINT8(s, settings->RefreshRect); /* refreshRectSupport (1 byte) */
	Stream_Write_UINT8(s, settings->SuppressOutput); /* suppressOutputSupport (1 byte) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_GENERAL);
}

BOOL rdp_print_general_capability_set(wStream* s, UINT16 length)
{
	UINT16 osMajorType;
	UINT16 osMinorType;
	UINT16 protocolVersion;
	UINT16 pad2OctetsA;
	UINT16 generalCompressionTypes;
	UINT16 extraFlags;
	UINT16 updateCapabilityFlag;
	UINT16 remoteUnshareFlag;
	UINT16 generalCompressionLevel;
	BYTE refreshRectSupport;
	BYTE suppressOutputSupport;

	if (length < 24)
		return FALSE;

	fprintf(stderr, "GeneralCapabilitySet (length %d):\n", length);

	Stream_Read_UINT16(s, osMajorType); /* osMajorType (2 bytes) */
	Stream_Read_UINT16(s, osMinorType); /* osMinorType (2 bytes) */
	Stream_Read_UINT16(s, protocolVersion); /* protocolVersion (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA); /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT16(s, generalCompressionTypes); /* generalCompressionTypes (2 bytes) */
	Stream_Read_UINT16(s, extraFlags); /* extraFlags (2 bytes) */
	Stream_Read_UINT16(s, updateCapabilityFlag); /* updateCapabilityFlag (2 bytes) */
	Stream_Read_UINT16(s, remoteUnshareFlag); /* remoteUnshareFlag (2 bytes) */
	Stream_Read_UINT16(s, generalCompressionLevel); /* generalCompressionLevel (2 bytes) */
	Stream_Read_UINT8(s, refreshRectSupport); /* refreshRectSupport (1 byte) */
	Stream_Read_UINT8(s, suppressOutputSupport); /* suppressOutputSupport (1 byte) */

	fprintf(stderr, "\tosMajorType: 0x%04X\n", osMajorType);
	fprintf(stderr, "\tosMinorType: 0x%04X\n", osMinorType);
	fprintf(stderr, "\tprotocolVersion: 0x%04X\n", protocolVersion);
	fprintf(stderr, "\tpad2OctetsA: 0x%04X\n", pad2OctetsA);
	fprintf(stderr, "\tgeneralCompressionTypes: 0x%04X\n", generalCompressionTypes);
	fprintf(stderr, "\textraFlags: 0x%04X\n", extraFlags);
	fprintf(stderr, "\tupdateCapabilityFlag: 0x%04X\n", updateCapabilityFlag);
	fprintf(stderr, "\tremoteUnshareFlag: 0x%04X\n", remoteUnshareFlag);
	fprintf(stderr, "\tgeneralCompressionLevel: 0x%04X\n", generalCompressionLevel);
	fprintf(stderr, "\trefreshRectSupport: 0x%02X\n", refreshRectSupport);
	fprintf(stderr, "\tsuppressOutputSupport: 0x%02X\n", suppressOutputSupport);

	return TRUE;
}

/**
 * Read bitmap capability set.\n
 * @msdn{cc240554}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_bitmap_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	BYTE drawingFlags;
	UINT16 desktopWidth;
	UINT16 desktopHeight;
	UINT16 desktopResizeFlag;
	UINT16 preferredBitsPerPixel;

	if (length < 28)
		return FALSE;

	Stream_Read_UINT16(s, preferredBitsPerPixel); /* preferredBitsPerPixel (2 bytes) */
	Stream_Seek_UINT16(s); /* receive1BitPerPixel (2 bytes) */
	Stream_Seek_UINT16(s); /* receive4BitsPerPixel (2 bytes) */
	Stream_Seek_UINT16(s); /* receive8BitsPerPixel (2 bytes) */
	Stream_Read_UINT16(s, desktopWidth); /* desktopWidth (2 bytes) */
	Stream_Read_UINT16(s, desktopHeight); /* desktopHeight (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */
	Stream_Read_UINT16(s, desktopResizeFlag); /* desktopResizeFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* bitmapCompressionFlag (2 bytes) */
	Stream_Seek_UINT8(s); /* highColorFlags (1 byte) */
	Stream_Read_UINT8(s, drawingFlags); /* drawingFlags (1 byte) */
	Stream_Seek_UINT16(s); /* multipleRectangleSupport (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2OctetsB (2 bytes) */

	if (!settings->ServerMode && (preferredBitsPerPixel != settings->ColorDepth))
	{
		/* The client must respect the actual color depth used by the server */
		settings->ColorDepth = preferredBitsPerPixel;
	}

	if (desktopResizeFlag == FALSE)
		settings->DesktopResize = FALSE;

	if (!settings->ServerMode && settings->DesktopResize)
	{
		/* The server may request a different desktop size during Deactivation-Reactivation sequence */
		settings->DesktopWidth = desktopWidth;
		settings->DesktopHeight = desktopHeight;
	}

	return TRUE;
}

/**
 * Write bitmap capability set.\n
 * @msdn{cc240554}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	BYTE drawingFlags = 0;
	UINT16 desktopResizeFlag;
	UINT16 preferredBitsPerPixel;

	Stream_EnsureRemainingCapacity(s, 64);

	header = rdp_capability_set_start(s);

	drawingFlags |= DRAW_ALLOW_SKIP_ALPHA;
	drawingFlags |= DRAW_ALLOW_COLOR_SUBSAMPLING;

	if (settings->RdpVersion > 5)
		preferredBitsPerPixel = settings->ColorDepth;
	else
		preferredBitsPerPixel = 8;

	desktopResizeFlag = settings->DesktopResize;

	Stream_Write_UINT16(s, preferredBitsPerPixel); /* preferredBitsPerPixel (2 bytes) */
	Stream_Write_UINT16(s, 1); /* receive1BitPerPixel (2 bytes) */
	Stream_Write_UINT16(s, 1); /* receive4BitsPerPixel (2 bytes) */
	Stream_Write_UINT16(s, 1); /* receive8BitsPerPixel (2 bytes) */
	Stream_Write_UINT16(s, settings->DesktopWidth); /* desktopWidth (2 bytes) */
	Stream_Write_UINT16(s, settings->DesktopHeight); /* desktopHeight (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */
	Stream_Write_UINT16(s, desktopResizeFlag); /* desktopResizeFlag (2 bytes) */
	Stream_Write_UINT16(s, 1); /* bitmapCompressionFlag (2 bytes) */
	Stream_Write_UINT8(s, 0); /* highColorFlags (1 byte) */
	Stream_Write_UINT8(s, drawingFlags); /* drawingFlags (1 byte) */
	Stream_Write_UINT16(s, 1); /* multipleRectangleSupport (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsB (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP);
}

BOOL rdp_print_bitmap_capability_set(wStream* s, UINT16 length)
{
	UINT16 preferredBitsPerPixel;
	UINT16 receive1BitPerPixel;
	UINT16 receive4BitsPerPixel;
	UINT16 receive8BitsPerPixel;
	UINT16 desktopWidth;
	UINT16 desktopHeight;
	UINT16 pad2Octets;
	UINT16 desktopResizeFlag;
	UINT16 bitmapCompressionFlag;
	BYTE highColorFlags;
	BYTE drawingFlags;
	UINT16 multipleRectangleSupport;
	UINT16 pad2OctetsB;

	fprintf(stderr, "BitmapCapabilitySet (length %d):\n", length);

	if (length < 28)
		return FALSE;

	Stream_Read_UINT16(s, preferredBitsPerPixel); /* preferredBitsPerPixel (2 bytes) */
	Stream_Read_UINT16(s, receive1BitPerPixel); /* receive1BitPerPixel (2 bytes) */
	Stream_Read_UINT16(s, receive4BitsPerPixel); /* receive4BitsPerPixel (2 bytes) */
	Stream_Read_UINT16(s, receive8BitsPerPixel); /* receive8BitsPerPixel (2 bytes) */
	Stream_Read_UINT16(s, desktopWidth); /* desktopWidth (2 bytes) */
	Stream_Read_UINT16(s, desktopHeight); /* desktopHeight (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */
	Stream_Read_UINT16(s, desktopResizeFlag); /* desktopResizeFlag (2 bytes) */
	Stream_Read_UINT16(s, bitmapCompressionFlag); /* bitmapCompressionFlag (2 bytes) */
	Stream_Read_UINT8(s, highColorFlags); /* highColorFlags (1 byte) */
	Stream_Read_UINT8(s, drawingFlags); /* drawingFlags (1 byte) */
	Stream_Read_UINT16(s, multipleRectangleSupport); /* multipleRectangleSupport (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsB); /* pad2OctetsB (2 bytes) */

	fprintf(stderr, "\tpreferredBitsPerPixel: 0x%04X\n", preferredBitsPerPixel);
	fprintf(stderr, "\treceive1BitPerPixel: 0x%04X\n", receive1BitPerPixel);
	fprintf(stderr, "\treceive4BitsPerPixel: 0x%04X\n", receive4BitsPerPixel);
	fprintf(stderr, "\treceive8BitsPerPixel: 0x%04X\n", receive8BitsPerPixel);
	fprintf(stderr, "\tdesktopWidth: 0x%04X\n", desktopWidth);
	fprintf(stderr, "\tdesktopHeight: 0x%04X\n", desktopHeight);
	fprintf(stderr, "\tpad2Octets: 0x%04X\n", pad2Octets);
	fprintf(stderr, "\tdesktopResizeFlag: 0x%04X\n", desktopResizeFlag);
	fprintf(stderr, "\tbitmapCompressionFlag: 0x%04X\n", bitmapCompressionFlag);
	fprintf(stderr, "\thighColorFlags: 0x%02X\n", highColorFlags);
	fprintf(stderr, "\tdrawingFlags: 0x%02X\n", drawingFlags);
	fprintf(stderr, "\tmultipleRectangleSupport: 0x%04X\n", multipleRectangleSupport);
	fprintf(stderr, "\tpad2OctetsB: 0x%04X\n", pad2OctetsB);

	return TRUE;
}

/**
 * Read order capability set.\n
 * @msdn{cc240556}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_order_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	int i;
	UINT16 orderFlags;
	BYTE orderSupport[32];
	UINT16 orderSupportExFlags;
	BOOL BitmapCacheV3Enabled = FALSE;
	BOOL FrameMarkerCommandEnabled = FALSE;

	if (length < 88)
		return FALSE;

	Stream_Seek(s, 16); /* terminalDescriptor (16 bytes) */
	Stream_Seek_UINT32(s); /* pad4OctetsA (4 bytes) */
	Stream_Seek_UINT16(s); /* desktopSaveXGranularity (2 bytes) */
	Stream_Seek_UINT16(s); /* desktopSaveYGranularity (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2OctetsA (2 bytes) */
	Stream_Seek_UINT16(s); /* maximumOrderLevel (2 bytes) */
	Stream_Seek_UINT16(s); /* numberFonts (2 bytes) */
	Stream_Read_UINT16(s, orderFlags); /* orderFlags (2 bytes) */
	Stream_Read(s, orderSupport, 32); /* orderSupport (32 bytes) */
	Stream_Seek_UINT16(s); /* textFlags (2 bytes) */
	Stream_Read_UINT16(s, orderSupportExFlags); /* orderSupportExFlags (2 bytes) */
	Stream_Seek_UINT32(s); /* pad4OctetsB (4 bytes) */
	Stream_Seek_UINT32(s); /* desktopSaveSize (4 bytes) */
	Stream_Seek_UINT16(s); /* pad2OctetsC (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2OctetsD (2 bytes) */
	Stream_Seek_UINT16(s); /* textANSICodePage (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2OctetsE (2 bytes) */

	for (i = 0; i < 32; i++)
	{
		if (orderSupport[i] == FALSE)
			settings->OrderSupport[i] = FALSE;
	}

	if (orderFlags & ORDER_FLAGS_EXTRA_SUPPORT)
	{
		if (orderSupportExFlags & CACHE_BITMAP_V3_SUPPORT)
			BitmapCacheV3Enabled = TRUE;

		if (orderSupportExFlags & ALTSEC_FRAME_MARKER_SUPPORT)
			FrameMarkerCommandEnabled = TRUE;
	}

	if (settings->BitmapCacheV3Enabled && BitmapCacheV3Enabled)
		settings->BitmapCacheVersion = 3;
	else
		settings ->BitmapCacheV3Enabled = FALSE;

	if (settings->FrameMarkerCommandEnabled && !FrameMarkerCommandEnabled)
		settings->FrameMarkerCommandEnabled = FALSE;

	return TRUE;
}

/**
 * Write order capability set.\n
 * @msdn{cc240556}
 * @param s stream
 * @param settings settings
 */

void rdp_write_order_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 orderFlags;
	UINT16 orderSupportExFlags;
	UINT16 textANSICodePage;

	Stream_EnsureRemainingCapacity(s, 64);

	header = rdp_capability_set_start(s);

	/* see [MSDN-CP]: http://msdn.microsoft.com/en-us/library/dd317756 */
	textANSICodePage = 65001; /* Unicode (UTF-8) */

	orderSupportExFlags = 0;
	orderFlags = NEGOTIATE_ORDER_SUPPORT | ZERO_BOUNDS_DELTA_SUPPORT | COLOR_INDEX_SUPPORT;

	if (settings->BitmapCacheV3Enabled)
	{
		orderSupportExFlags |= CACHE_BITMAP_V3_SUPPORT;
		orderFlags |= ORDER_FLAGS_EXTRA_SUPPORT;
	}

	if (settings->FrameMarkerCommandEnabled)
	{
		orderSupportExFlags |= ALTSEC_FRAME_MARKER_SUPPORT;
		orderFlags |= ORDER_FLAGS_EXTRA_SUPPORT;
	}

	Stream_Zero(s, 16); /* terminalDescriptor (16 bytes) */
	Stream_Write_UINT32(s, 0); /* pad4OctetsA (4 bytes) */
	Stream_Write_UINT16(s, 1); /* desktopSaveXGranularity (2 bytes) */
	Stream_Write_UINT16(s, 20); /* desktopSaveYGranularity (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsA (2 bytes) */
	Stream_Write_UINT16(s, 1); /* maximumOrderLevel (2 bytes) */
	Stream_Write_UINT16(s, 0); /* numberFonts (2 bytes) */
	Stream_Write_UINT16(s, orderFlags); /* orderFlags (2 bytes) */
	Stream_Write(s, settings->OrderSupport, 32); /* orderSupport (32 bytes) */
	Stream_Write_UINT16(s, 0); /* textFlags (2 bytes) */
	Stream_Write_UINT16(s, orderSupportExFlags); /* orderSupportExFlags (2 bytes) */
	Stream_Write_UINT32(s, 0); /* pad4OctetsB (4 bytes) */
	Stream_Write_UINT32(s, 230400); /* desktopSaveSize (4 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsC (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsD (2 bytes) */
	Stream_Write_UINT16(s, 0); /* textANSICodePage (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsE (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_ORDER);
}

BOOL rdp_print_order_capability_set(wStream* s, UINT16 length)
{
	BYTE terminalDescriptor[16];
	UINT32 pad4OctetsA;
	UINT16 desktopSaveXGranularity;
	UINT16 desktopSaveYGranularity;
	UINT16 pad2OctetsA;
	UINT16 maximumOrderLevel;
	UINT16 numberFonts;
	UINT16 orderFlags;
	BYTE orderSupport[32];
	UINT16 textFlags;
	UINT16 orderSupportExFlags;
	UINT32 pad4OctetsB;
	UINT32 desktopSaveSize;
	UINT16 pad2OctetsC;
	UINT16 pad2OctetsD;
	UINT16 textANSICodePage;
	UINT16 pad2OctetsE;

	fprintf(stderr, "OrderCapabilitySet (length %d):\n", length);

	if (length < 88)
		return FALSE;

	Stream_Read(s, terminalDescriptor, 16); /* terminalDescriptor (16 bytes) */
	Stream_Read_UINT32(s, pad4OctetsA); /* pad4OctetsA (4 bytes) */
	Stream_Read_UINT16(s, desktopSaveXGranularity); /* desktopSaveXGranularity (2 bytes) */
	Stream_Read_UINT16(s, desktopSaveYGranularity); /* desktopSaveYGranularity (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA); /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT16(s, maximumOrderLevel); /* maximumOrderLevel (2 bytes) */
	Stream_Read_UINT16(s, numberFonts); /* numberFonts (2 bytes) */
	Stream_Read_UINT16(s, orderFlags); /* orderFlags (2 bytes) */
	Stream_Read(s, orderSupport, 32); /* orderSupport (32 bytes) */
	Stream_Read_UINT16(s, textFlags); /* textFlags (2 bytes) */
	Stream_Read_UINT16(s, orderSupportExFlags); /* orderSupportExFlags (2 bytes) */
	Stream_Read_UINT32(s, pad4OctetsB); /* pad4OctetsB (4 bytes) */
	Stream_Read_UINT32(s, desktopSaveSize); /* desktopSaveSize (4 bytes) */
	Stream_Read_UINT16(s, pad2OctetsC); /* pad2OctetsC (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsD); /* pad2OctetsD (2 bytes) */
	Stream_Read_UINT16(s, textANSICodePage); /* textANSICodePage (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsE); /* pad2OctetsE (2 bytes) */

	fprintf(stderr, "\tpad4OctetsA: 0x%08X\n", pad4OctetsA);
	fprintf(stderr, "\tdesktopSaveXGranularity: 0x%04X\n", desktopSaveXGranularity);
	fprintf(stderr, "\tdesktopSaveYGranularity: 0x%04X\n", desktopSaveYGranularity);
	fprintf(stderr, "\tpad2OctetsA: 0x%04X\n", pad2OctetsA);
	fprintf(stderr, "\tmaximumOrderLevel: 0x%04X\n", maximumOrderLevel);
	fprintf(stderr, "\tnumberFonts: 0x%04X\n", numberFonts);
	fprintf(stderr, "\torderFlags: 0x%04X\n", orderFlags);

	fprintf(stderr, "\torderSupport:\n");
	fprintf(stderr, "\t\tDSTBLT: %d\n", orderSupport[NEG_DSTBLT_INDEX]);
	fprintf(stderr, "\t\tPATBLT: %d\n", orderSupport[NEG_PATBLT_INDEX]);
	fprintf(stderr, "\t\tSCRBLT: %d\n", orderSupport[NEG_SCRBLT_INDEX]);
	fprintf(stderr, "\t\tMEMBLT: %d\n", orderSupport[NEG_MEMBLT_INDEX]);
	fprintf(stderr, "\t\tMEM3BLT: %d\n", orderSupport[NEG_MEM3BLT_INDEX]);
	fprintf(stderr, "\t\tATEXTOUT: %d\n", orderSupport[NEG_ATEXTOUT_INDEX]);
	fprintf(stderr, "\t\tAEXTTEXTOUT: %d\n", orderSupport[NEG_AEXTTEXTOUT_INDEX]);
	fprintf(stderr, "\t\tDRAWNINEGRID: %d\n", orderSupport[NEG_DRAWNINEGRID_INDEX]);
	fprintf(stderr, "\t\tLINETO: %d\n", orderSupport[NEG_LINETO_INDEX]);
	fprintf(stderr, "\t\tMULTI_DRAWNINEGRID: %d\n", orderSupport[NEG_MULTI_DRAWNINEGRID_INDEX]);
	fprintf(stderr, "\t\tOPAQUE_RECT: %d\n", orderSupport[NEG_OPAQUE_RECT_INDEX]);
	fprintf(stderr, "\t\tSAVEBITMAP: %d\n", orderSupport[NEG_SAVEBITMAP_INDEX]);
	fprintf(stderr, "\t\tWTEXTOUT: %d\n", orderSupport[NEG_WTEXTOUT_INDEX]);
	fprintf(stderr, "\t\tMEMBLT_V2: %d\n", orderSupport[NEG_MEMBLT_V2_INDEX]);
	fprintf(stderr, "\t\tMEM3BLT_V2: %d\n", orderSupport[NEG_MEM3BLT_V2_INDEX]);
	fprintf(stderr, "\t\tMULTIDSTBLT: %d\n", orderSupport[NEG_MULTIDSTBLT_INDEX]);
	fprintf(stderr, "\t\tMULTIPATBLT: %d\n", orderSupport[NEG_MULTIPATBLT_INDEX]);
	fprintf(stderr, "\t\tMULTISCRBLT: %d\n", orderSupport[NEG_MULTISCRBLT_INDEX]);
	fprintf(stderr, "\t\tMULTIOPAQUERECT: %d\n", orderSupport[NEG_MULTIOPAQUERECT_INDEX]);
	fprintf(stderr, "\t\tFAST_INDEX: %d\n", orderSupport[NEG_FAST_INDEX_INDEX]);
	fprintf(stderr, "\t\tPOLYGON_SC: %d\n", orderSupport[NEG_POLYGON_SC_INDEX]);
	fprintf(stderr, "\t\tPOLYGON_CB: %d\n", orderSupport[NEG_POLYGON_CB_INDEX]);
	fprintf(stderr, "\t\tPOLYLINE: %d\n", orderSupport[NEG_POLYLINE_INDEX]);
	fprintf(stderr, "\t\tUNUSED23: %d\n", orderSupport[NEG_UNUSED23_INDEX]);
	fprintf(stderr, "\t\tFAST_GLYPH: %d\n", orderSupport[NEG_FAST_GLYPH_INDEX]);
	fprintf(stderr, "\t\tELLIPSE_SC: %d\n", orderSupport[NEG_ELLIPSE_SC_INDEX]);
	fprintf(stderr, "\t\tELLIPSE_CB: %d\n", orderSupport[NEG_ELLIPSE_CB_INDEX]);
	fprintf(stderr, "\t\tGLYPH_INDEX: %d\n", orderSupport[NEG_GLYPH_INDEX_INDEX]);
	fprintf(stderr, "\t\tGLYPH_WEXTTEXTOUT: %d\n", orderSupport[NEG_GLYPH_WEXTTEXTOUT_INDEX]);
	fprintf(stderr, "\t\tGLYPH_WLONGTEXTOUT: %d\n", orderSupport[NEG_GLYPH_WLONGTEXTOUT_INDEX]);
	fprintf(stderr, "\t\tGLYPH_WLONGEXTTEXTOUT: %d\n", orderSupport[NEG_GLYPH_WLONGEXTTEXTOUT_INDEX]);
	fprintf(stderr, "\t\tUNUSED31: %d\n", orderSupport[NEG_UNUSED31_INDEX]);

	fprintf(stderr, "\ttextFlags: 0x%04X\n", textFlags);
	fprintf(stderr, "\torderSupportExFlags: 0x%04X\n", orderSupportExFlags);
	fprintf(stderr, "\tpad4OctetsB: 0x%08X\n", pad4OctetsB);
	fprintf(stderr, "\tdesktopSaveSize: 0x%08X\n", desktopSaveSize);
	fprintf(stderr, "\tpad2OctetsC: 0x%04X\n", pad2OctetsC);
	fprintf(stderr, "\tpad2OctetsD: 0x%04X\n", pad2OctetsD);
	fprintf(stderr, "\ttextANSICodePage: 0x%04X\n", textANSICodePage);
	fprintf(stderr, "\tpad2OctetsE: 0x%04X\n", pad2OctetsE);

	return TRUE;
}

/**
 * Read bitmap cache capability set.\n
 * @msdn{cc240559}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_bitmap_cache_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 40)
		return FALSE;

	Stream_Seek_UINT32(s); /* pad1 (4 bytes) */
	Stream_Seek_UINT32(s); /* pad2 (4 bytes) */
	Stream_Seek_UINT32(s); /* pad3 (4 bytes) */
	Stream_Seek_UINT32(s); /* pad4 (4 bytes) */
	Stream_Seek_UINT32(s); /* pad5 (4 bytes) */
	Stream_Seek_UINT32(s); /* pad6 (4 bytes) */
	Stream_Seek_UINT16(s); /* Cache0Entries (2 bytes) */
	Stream_Seek_UINT16(s); /* Cache0MaximumCellSize (2 bytes) */
	Stream_Seek_UINT16(s); /* Cache1Entries (2 bytes) */
	Stream_Seek_UINT16(s); /* Cache1MaximumCellSize (2 bytes) */
	Stream_Seek_UINT16(s); /* Cache2Entries (2 bytes) */
	Stream_Seek_UINT16(s); /* Cache2MaximumCellSize (2 bytes) */

	return TRUE;
}

/**
 * Write bitmap cache capability set.\n
 * @msdn{cc240559}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int bpp;
	int header;
	UINT16 size;

	Stream_EnsureRemainingCapacity(s, 64);

	header = rdp_capability_set_start(s);

	bpp = (settings->ColorDepth + 7) / 8;

	Stream_Write_UINT32(s, 0); /* pad1 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad2 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad3 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad4 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad5 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad6 (4 bytes) */

	size = bpp * 256;
	Stream_Write_UINT16(s, 200); /* Cache0Entries (2 bytes) */
	Stream_Write_UINT16(s, size); /* Cache0MaximumCellSize (2 bytes) */

	size = bpp * 1024;
	Stream_Write_UINT16(s, 600); /* Cache1Entries (2 bytes) */
	Stream_Write_UINT16(s, size); /* Cache1MaximumCellSize (2 bytes) */

	size = bpp * 4096;
	Stream_Write_UINT16(s, 1000); /* Cache2Entries (2 bytes) */
	Stream_Write_UINT16(s, size); /* Cache2MaximumCellSize (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE);
}

BOOL rdp_print_bitmap_cache_capability_set(wStream* s, UINT16 length)
{
	UINT32 pad1, pad2, pad3;
	UINT32 pad4, pad5, pad6;
	UINT16 Cache0Entries;
	UINT16 Cache0MaximumCellSize;
	UINT16 Cache1Entries;
	UINT16 Cache1MaximumCellSize;
	UINT16 Cache2Entries;
	UINT16 Cache2MaximumCellSize;

	fprintf(stderr, "BitmapCacheCapabilitySet (length %d):\n", length);

	if (length < 40)
		return FALSE;

	Stream_Read_UINT32(s, pad1); /* pad1 (4 bytes) */
	Stream_Read_UINT32(s, pad2); /* pad2 (4 bytes) */
	Stream_Read_UINT32(s, pad3); /* pad3 (4 bytes) */
	Stream_Read_UINT32(s, pad4); /* pad4 (4 bytes) */
	Stream_Read_UINT32(s, pad5); /* pad5 (4 bytes) */
	Stream_Read_UINT32(s, pad6); /* pad6 (4 bytes) */
	Stream_Read_UINT16(s, Cache0Entries); /* Cache0Entries (2 bytes) */
	Stream_Read_UINT16(s, Cache0MaximumCellSize); /* Cache0MaximumCellSize (2 bytes) */
	Stream_Read_UINT16(s, Cache1Entries); /* Cache1Entries (2 bytes) */
	Stream_Read_UINT16(s, Cache1MaximumCellSize); /* Cache1MaximumCellSize (2 bytes) */
	Stream_Read_UINT16(s, Cache2Entries); /* Cache2Entries (2 bytes) */
	Stream_Read_UINT16(s, Cache2MaximumCellSize); /* Cache2MaximumCellSize (2 bytes) */

	fprintf(stderr, "\tpad1: 0x%08X\n", pad1);
	fprintf(stderr, "\tpad2: 0x%08X\n", pad2);
	fprintf(stderr, "\tpad3: 0x%08X\n", pad3);
	fprintf(stderr, "\tpad4: 0x%08X\n", pad4);
	fprintf(stderr, "\tpad5: 0x%08X\n", pad5);
	fprintf(stderr, "\tpad6: 0x%08X\n", pad6);
	fprintf(stderr, "\tCache0Entries: 0x%04X\n", Cache0Entries);
	fprintf(stderr, "\tCache0MaximumCellSize: 0x%04X\n", Cache0MaximumCellSize);
	fprintf(stderr, "\tCache1Entries: 0x%04X\n", Cache1Entries);
	fprintf(stderr, "\tCache1MaximumCellSize: 0x%04X\n", Cache1MaximumCellSize);
	fprintf(stderr, "\tCache2Entries: 0x%04X\n", Cache2Entries);
	fprintf(stderr, "\tCache2MaximumCellSize: 0x%04X\n", Cache2MaximumCellSize);

	return TRUE;
}

/**
 * Read control capability set.\n
 * @msdn{cc240568}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_control_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 12)
		return FALSE;

	Stream_Seek_UINT16(s); /* controlFlags (2 bytes) */
	Stream_Seek_UINT16(s); /* remoteDetachFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* controlInterest (2 bytes) */
	Stream_Seek_UINT16(s); /* detachInterest (2 bytes) */

	return TRUE;
}

/**
 * Write control capability set.\n
 * @msdn{cc240568}
 * @param s stream
 * @param settings settings
 */

void rdp_write_control_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	Stream_Write_UINT16(s, 0); /* controlFlags (2 bytes) */
	Stream_Write_UINT16(s, 0); /* remoteDetachFlag (2 bytes) */
	Stream_Write_UINT16(s, 2); /* controlInterest (2 bytes) */
	Stream_Write_UINT16(s, 2); /* detachInterest (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_CONTROL);
}

BOOL rdp_print_control_capability_set(wStream* s, UINT16 length)
{
	UINT16 controlFlags;
	UINT16 remoteDetachFlag;
	UINT16 controlInterest;
	UINT16 detachInterest;

	fprintf(stderr, "ControlCapabilitySet (length %d):\n", length);

	if (length < 12)
		return FALSE;

	Stream_Read_UINT16(s, controlFlags); /* controlFlags (2 bytes) */
	Stream_Read_UINT16(s, remoteDetachFlag); /* remoteDetachFlag (2 bytes) */
	Stream_Read_UINT16(s, controlInterest); /* controlInterest (2 bytes) */
	Stream_Read_UINT16(s, detachInterest); /* detachInterest (2 bytes) */

	fprintf(stderr, "\tcontrolFlags: 0x%04X\n", controlFlags);
	fprintf(stderr, "\tremoteDetachFlag: 0x%04X\n", remoteDetachFlag);
	fprintf(stderr, "\tcontrolInterest: 0x%04X\n", controlInterest);
	fprintf(stderr, "\tdetachInterest: 0x%04X\n", detachInterest);

	return TRUE;
}

/**
 * Read window activation capability set.\n
 * @msdn{cc240569}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_window_activation_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 12)
		return FALSE;

	Stream_Seek_UINT16(s); /* helpKeyFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* helpKeyIndexFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* helpExtendedKeyFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* windowManagerKeyFlag (2 bytes) */

	return TRUE;
}

/**
 * Write window activation capability set.\n
 * @msdn{cc240569}
 * @param s stream
 * @param settings settings
 */

void rdp_write_window_activation_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	Stream_Write_UINT16(s, 0); /* helpKeyFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* helpKeyIndexFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* helpExtendedKeyFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* windowManagerKeyFlag (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_ACTIVATION);
}

BOOL rdp_print_window_activation_capability_set(wStream* s, UINT16 length)
{
	UINT16 helpKeyFlag;
	UINT16 helpKeyIndexFlag;
	UINT16 helpExtendedKeyFlag;
	UINT16 windowManagerKeyFlag;

	fprintf(stderr, "WindowActivationCapabilitySet (length %d):\n", length);

	if (length < 12)
		return FALSE;

	Stream_Read_UINT16(s, helpKeyFlag); /* helpKeyFlag (2 bytes) */
	Stream_Read_UINT16(s, helpKeyIndexFlag); /* helpKeyIndexFlag (2 bytes) */
	Stream_Read_UINT16(s, helpExtendedKeyFlag); /* helpExtendedKeyFlag (2 bytes) */
	Stream_Read_UINT16(s, windowManagerKeyFlag); /* windowManagerKeyFlag (2 bytes) */

	fprintf(stderr, "\thelpKeyFlag: 0x%04X\n", helpKeyFlag);
	fprintf(stderr, "\thelpKeyIndexFlag: 0x%04X\n", helpKeyIndexFlag);
	fprintf(stderr, "\thelpExtendedKeyFlag: 0x%04X\n", helpExtendedKeyFlag);
	fprintf(stderr, "\twindowManagerKeyFlag: 0x%04X\n", windowManagerKeyFlag);

	return TRUE;
}

/**
 * Read pointer capability set.\n
 * @msdn{cc240562}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_pointer_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT16 colorPointerFlag;
	UINT16 colorPointerCacheSize;
	UINT16 pointerCacheSize;

	if (length < 10)
		return FALSE;

	Stream_Read_UINT16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	Stream_Read_UINT16(s, colorPointerCacheSize); /* colorPointerCacheSize (2 bytes) */
	Stream_Read_UINT16(s, pointerCacheSize); /* pointerCacheSize (2 bytes) */

	if (colorPointerFlag == FALSE)
		settings->ColorPointerFlag = FALSE;

	if (settings->ServerMode)
	{
		settings->PointerCacheSize = pointerCacheSize;
	}
	return TRUE;
}

/**
 * Write pointer capability set.\n
 * @msdn{cc240562}
 * @param s stream
 * @param settings settings
 */

void rdp_write_pointer_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 colorPointerFlag;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	colorPointerFlag = (settings->ColorPointerFlag) ? 1 : 0;

	Stream_Write_UINT16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	Stream_Write_UINT16(s, settings->PointerCacheSize); /* colorPointerCacheSize (2 bytes) */

	if (settings->LargePointerFlag)
	{
		Stream_Write_UINT16(s, settings->PointerCacheSize); /* pointerCacheSize (2 bytes) */
	}

	rdp_capability_set_finish(s, header, CAPSET_TYPE_POINTER);
}

BOOL rdp_print_pointer_capability_set(wStream* s, UINT16 length)
{
	UINT16 colorPointerFlag;
	UINT16 colorPointerCacheSize;
	UINT16 pointerCacheSize;

	if (length < 10)
		return FALSE;

	fprintf(stderr, "PointerCapabilitySet (length %d):\n", length);

	Stream_Read_UINT16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	Stream_Read_UINT16(s, colorPointerCacheSize); /* colorPointerCacheSize (2 bytes) */
	Stream_Read_UINT16(s, pointerCacheSize); /* pointerCacheSize (2 bytes) */

	fprintf(stderr, "\tcolorPointerFlag: 0x%04X\n", colorPointerFlag);
	fprintf(stderr, "\tcolorPointerCacheSize: 0x%04X\n", colorPointerCacheSize);
	fprintf(stderr, "\tpointerCacheSize: 0x%04X\n", pointerCacheSize);

	return TRUE;
}

/**
 * Read share capability set.\n
 * @msdn{cc240570}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_share_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 8)
		return FALSE;

	Stream_Seek_UINT16(s); /* nodeId (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */

	return TRUE;
}

/**
 * Write share capability set.\n
 * @msdn{cc240570}
 * @param s stream
 * @param settings settings
 */

void rdp_write_share_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 nodeId;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	nodeId = (settings->ServerMode) ? 0x03EA : 0;

	Stream_Write_UINT16(s, nodeId); /* nodeId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SHARE);
}

BOOL rdp_print_share_capability_set(wStream* s, UINT16 length)
{
	UINT16 nodeId;
	UINT16 pad2Octets;

	fprintf(stderr, "ShareCapabilitySet (length %d):\n", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT16(s, nodeId); /* nodeId (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */

	fprintf(stderr, "\tnodeId: 0x%04X\n", nodeId);
	fprintf(stderr, "\tpad2Octets: 0x%04X\n", pad2Octets);

	return TRUE;
}

/**
 * Read color cache capability set.\n
 * @msdn{cc241564}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_color_cache_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 8)
		return FALSE;

	Stream_Seek_UINT16(s); /* colorTableCacheSize (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */

	return TRUE;
}

/**
 * Write color cache capability set.\n
 * @msdn{cc241564}
 * @param s stream
 * @param settings settings
 */

void rdp_write_color_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	Stream_Write_UINT16(s, 6); /* colorTableCacheSize (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_COLOR_CACHE);
}

BOOL rdp_print_color_cache_capability_set(wStream* s, UINT16 length)
{
	UINT16 colorTableCacheSize;
	UINT16 pad2Octets;

	fprintf(stderr, "ColorCacheCapabilitySet (length %d):\n", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT16(s, colorTableCacheSize); /* colorTableCacheSize (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */

	fprintf(stderr, "\tcolorTableCacheSize: 0x%04X\n", colorTableCacheSize);
	fprintf(stderr, "\tpad2Octets: 0x%04X\n", pad2Octets);

	return TRUE;
}

/**
 * Read sound capability set.\n
 * @msdn{cc240552}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_sound_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT16 soundFlags;

	if (length < 8)
		return FALSE;

	Stream_Read_UINT16(s, soundFlags); /* soundFlags (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2OctetsA (2 bytes) */

	settings->SoundBeepsEnabled = (soundFlags & SOUND_BEEPS_FLAG) ? TRUE : FALSE;

	return TRUE;
}

/**
 * Write sound capability set.\n
 * @msdn{cc240552}
 * @param s stream
 * @param settings settings
 */

void rdp_write_sound_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 soundFlags;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	soundFlags = (settings->SoundBeepsEnabled) ? SOUND_BEEPS_FLAG : 0;

	Stream_Write_UINT16(s, soundFlags); /* soundFlags (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsA (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SOUND);
}

BOOL rdp_print_sound_capability_set(wStream* s, UINT16 length)
{
	UINT16 soundFlags;
	UINT16 pad2OctetsA;

	fprintf(stderr, "SoundCapabilitySet (length %d):\n", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT16(s, soundFlags); /* soundFlags (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA); /* pad2OctetsA (2 bytes) */

	fprintf(stderr, "\tsoundFlags: 0x%04X\n", soundFlags);
	fprintf(stderr, "\tpad2OctetsA: 0x%04X\n", pad2OctetsA);

	return TRUE;
}

/**
 * Read input capability set.\n
 * @msdn{cc240563}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_input_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT16 inputFlags;

	if (length < 88)
		return FALSE;

	Stream_Read_UINT16(s, inputFlags); /* inputFlags (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2OctetsA (2 bytes) */

	if (settings->ServerMode)
	{
		Stream_Read_UINT32(s, settings->KeyboardLayout); /* keyboardLayout (4 bytes) */
		Stream_Read_UINT32(s, settings->KeyboardType); /* keyboardType (4 bytes) */
		Stream_Read_UINT32(s, settings->KeyboardSubType); /* keyboardSubType (4 bytes) */
		Stream_Read_UINT32(s, settings->KeyboardFunctionKey); /* keyboardFunctionKeys (4 bytes) */
	}
	else
	{
		Stream_Seek_UINT32(s); /* keyboardLayout (4 bytes) */
		Stream_Seek_UINT32(s); /* keyboardType (4 bytes) */
		Stream_Seek_UINT32(s); /* keyboardSubType (4 bytes) */
		Stream_Seek_UINT32(s); /* keyboardFunctionKeys (4 bytes) */
	}

	Stream_Seek(s, 64); /* imeFileName (64 bytes) */

	if (!settings->ServerMode)
	{
		if (inputFlags & INPUT_FLAG_FASTPATH_INPUT)
		{
			/* advertised by RDP 5.0 and 5.1 servers */
		}
		else if (inputFlags & INPUT_FLAG_FASTPATH_INPUT2)
		{
			/* advertised by RDP 5.2, 6.0, 6.1 and 7.0 servers */
		}
		else
		{
			/* server does not support fastpath input */
			settings->FastPathInput = FALSE;
		}
	}
	return TRUE;
}

/**
 * Write input capability set.\n
 * @msdn{cc240563}
 * @param s stream
 * @param settings settings
 */

void rdp_write_input_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 inputFlags;

	Stream_EnsureRemainingCapacity(s, 128);

	header = rdp_capability_set_start(s);

	inputFlags = INPUT_FLAG_SCANCODES | INPUT_FLAG_MOUSEX | INPUT_FLAG_UNICODE;

	if (settings->FastPathInput)
	{
		inputFlags |= INPUT_FLAG_FASTPATH_INPUT;
		inputFlags |= INPUT_FLAG_FASTPATH_INPUT2;
	}

	Stream_Write_UINT16(s, inputFlags); /* inputFlags (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsA (2 bytes) */
	Stream_Write_UINT32(s, settings->KeyboardLayout); /* keyboardLayout (4 bytes) */
	Stream_Write_UINT32(s, settings->KeyboardType); /* keyboardType (4 bytes) */
	Stream_Write_UINT32(s, settings->KeyboardSubType); /* keyboardSubType (4 bytes) */
	Stream_Write_UINT32(s, settings->KeyboardFunctionKey); /* keyboardFunctionKeys (4 bytes) */
	Stream_Zero(s, 64); /* imeFileName (64 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_INPUT);
}

BOOL rdp_print_input_capability_set(wStream* s, UINT16 length)
{
	UINT16 inputFlags;
	UINT16 pad2OctetsA;
	UINT32 keyboardLayout;
	UINT32 keyboardType;
	UINT32 keyboardSubType;
	UINT32 keyboardFunctionKey;

	fprintf(stderr, "InputCapabilitySet (length %d)\n", length);

	if (length < 88)
		return FALSE;

	Stream_Read_UINT16(s, inputFlags); /* inputFlags (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA); /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT32(s, keyboardLayout); /* keyboardLayout (4 bytes) */
	Stream_Read_UINT32(s, keyboardType); /* keyboardType (4 bytes) */
	Stream_Read_UINT32(s, keyboardSubType); /* keyboardSubType (4 bytes) */
	Stream_Read_UINT32(s, keyboardFunctionKey); /* keyboardFunctionKeys (4 bytes) */
	Stream_Seek(s, 64); /* imeFileName (64 bytes) */

	fprintf(stderr, "\tinputFlags: 0x%04X\n", inputFlags);
	fprintf(stderr, "\tpad2OctetsA: 0x%04X\n", pad2OctetsA);
	fprintf(stderr, "\tkeyboardLayout: 0x%08X\n", keyboardLayout);
	fprintf(stderr, "\tkeyboardType: 0x%08X\n", keyboardType);
	fprintf(stderr, "\tkeyboardSubType: 0x%08X\n", keyboardSubType);
	fprintf(stderr, "\tkeyboardFunctionKey: 0x%08X\n", keyboardFunctionKey);

	return TRUE;
}

/**
 * Read font capability set.\n
 * @msdn{cc240571}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_font_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length > 4)
		Stream_Seek_UINT16(s); /* fontSupportFlags (2 bytes) */

	if (length > 6)
		Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */

	return TRUE;
}

/**
 * Write font capability set.\n
 * @msdn{cc240571}
 * @param s stream
 * @param settings settings
 */

void rdp_write_font_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	Stream_Write_UINT16(s, FONTSUPPORT_FONTLIST); /* fontSupportFlags (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_FONT);
}

BOOL rdp_print_font_capability_set(wStream* s, UINT16 length)
{
	UINT16 fontSupportFlags = 0;
	UINT16 pad2Octets = 0;

	fprintf(stderr, "FontCapabilitySet (length %d):\n", length);

	if (length > 4)
		Stream_Read_UINT16(s, fontSupportFlags); /* fontSupportFlags (2 bytes) */

	if (length > 6)
		Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */

	fprintf(stderr, "\tfontSupportFlags: 0x%04X\n", fontSupportFlags);
	fprintf(stderr, "\tpad2Octets: 0x%04X\n", pad2Octets);

	return TRUE;
}

/**
 * Read brush capability set.\n
 * @msdn{cc240564}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_brush_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 8)
		return FALSE;

	Stream_Seek_UINT32(s); /* brushSupportLevel (4 bytes) */

	return TRUE;
}

/**
 * Write brush capability set.\n
 * @msdn{cc240564}
 * @param s stream
 * @param settings settings
 */

void rdp_write_brush_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	Stream_Write_UINT32(s, BRUSH_COLOR_FULL); /* brushSupportLevel (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BRUSH);
}

BOOL rdp_print_brush_capability_set(wStream* s, UINT16 length)
{
	UINT32 brushSupportLevel;

	fprintf(stderr, "BrushCapabilitySet (length %d):\n", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, brushSupportLevel); /* brushSupportLevel (4 bytes) */

	fprintf(stderr, "\tbrushSupportLevel: 0x%08X\n", brushSupportLevel);

	return TRUE;
}

/**
 * Read cache definition (glyph).\n
 * @msdn{cc240566}
 * @param s stream
 */
void rdp_read_cache_definition(wStream* s, GLYPH_CACHE_DEFINITION* cache_definition)
{
	Stream_Read_UINT16(s, cache_definition->cacheEntries); /* cacheEntries (2 bytes) */
	Stream_Read_UINT16(s, cache_definition->cacheMaximumCellSize); /* cacheMaximumCellSize (2 bytes) */
}

/**
 * Write cache definition (glyph).\n
 * @msdn{cc240566}
 * @param s stream
 */
void rdp_write_cache_definition(wStream* s, GLYPH_CACHE_DEFINITION* cache_definition)
{
	Stream_Write_UINT16(s, cache_definition->cacheEntries); /* cacheEntries (2 bytes) */
	Stream_Write_UINT16(s, cache_definition->cacheMaximumCellSize); /* cacheMaximumCellSize (2 bytes) */
}

/**
 * Read glyph cache capability set.\n
 * @msdn{cc240565}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_glyph_cache_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 52)
		return FALSE;

	/* glyphCache (40 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[0])); /* glyphCache0 (4 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[1])); /* glyphCache1 (4 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[2])); /* glyphCache2 (4 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[3])); /* glyphCache3 (4 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[4])); /* glyphCache4 (4 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[5])); /* glyphCache5 (4 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[6])); /* glyphCache6 (4 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[7])); /* glyphCache7 (4 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[8])); /* glyphCache8 (4 bytes) */
	rdp_read_cache_definition(s, &(settings->GlyphCache[9])); /* glyphCache9 (4 bytes) */
	rdp_read_cache_definition(s, settings->FragCache);  /* fragCache (4 bytes) */

	Stream_Read_UINT16(s, settings->GlyphSupportLevel); /* glyphSupportLevel (2 bytes) */

	Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */

	return TRUE;
}

/**
 * Write glyph cache capability set.\n
 * @msdn{cc240565}
 * @param s stream
 * @param settings settings
 */

void rdp_write_glyph_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 64);

	header = rdp_capability_set_start(s);

	/* glyphCache (40 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[0])); /* glyphCache0 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[1])); /* glyphCache1 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[2])); /* glyphCache2 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[3])); /* glyphCache3 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[4])); /* glyphCache4 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[5])); /* glyphCache5 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[6])); /* glyphCache6 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[7])); /* glyphCache7 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[8])); /* glyphCache8 (4 bytes) */
	rdp_write_cache_definition(s, &(settings->GlyphCache[9])); /* glyphCache9 (4 bytes) */
	rdp_write_cache_definition(s, settings->FragCache);  /* fragCache (4 bytes) */

	Stream_Write_UINT16(s, settings->GlyphSupportLevel); /* glyphSupportLevel (2 bytes) */

	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_GLYPH_CACHE);
}

BOOL rdp_print_glyph_cache_capability_set(wStream* s, UINT16 length)
{
	GLYPH_CACHE_DEFINITION glyphCache[10];
	GLYPH_CACHE_DEFINITION fragCache;
	UINT16 glyphSupportLevel;
	UINT16 pad2Octets;

	fprintf(stderr, "GlyphCacheCapabilitySet (length %d):\n", length);

	if (length < 52)
		return FALSE;

	/* glyphCache (40 bytes) */
	rdp_read_cache_definition(s, &glyphCache[0]); /* glyphCache0 (4 bytes) */
	rdp_read_cache_definition(s, &glyphCache[1]); /* glyphCache1 (4 bytes) */
	rdp_read_cache_definition(s, &glyphCache[2]); /* glyphCache2 (4 bytes) */
	rdp_read_cache_definition(s, &glyphCache[3]); /* glyphCache3 (4 bytes) */
	rdp_read_cache_definition(s, &glyphCache[4]); /* glyphCache4 (4 bytes) */
	rdp_read_cache_definition(s, &glyphCache[5]); /* glyphCache5 (4 bytes) */
	rdp_read_cache_definition(s, &glyphCache[6]); /* glyphCache6 (4 bytes) */
	rdp_read_cache_definition(s, &glyphCache[7]); /* glyphCache7 (4 bytes) */
	rdp_read_cache_definition(s, &glyphCache[8]); /* glyphCache8 (4 bytes) */
	rdp_read_cache_definition(s, &glyphCache[9]); /* glyphCache9 (4 bytes) */
	rdp_read_cache_definition(s, &fragCache);  /* fragCache (4 bytes) */

	Stream_Read_UINT16(s, glyphSupportLevel); /* glyphSupportLevel (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */

	fprintf(stderr, "\tglyphCache0: Entries: %d MaximumCellSize: %d\n", glyphCache[0].cacheEntries, glyphCache[0].cacheMaximumCellSize);
	fprintf(stderr, "\tglyphCache1: Entries: %d MaximumCellSize: %d\n", glyphCache[1].cacheEntries, glyphCache[1].cacheMaximumCellSize);
	fprintf(stderr, "\tglyphCache2: Entries: %d MaximumCellSize: %d\n", glyphCache[2].cacheEntries, glyphCache[2].cacheMaximumCellSize);
	fprintf(stderr, "\tglyphCache3: Entries: %d MaximumCellSize: %d\n", glyphCache[3].cacheEntries, glyphCache[3].cacheMaximumCellSize);
	fprintf(stderr, "\tglyphCache4: Entries: %d MaximumCellSize: %d\n", glyphCache[4].cacheEntries, glyphCache[4].cacheMaximumCellSize);
	fprintf(stderr, "\tglyphCache5: Entries: %d MaximumCellSize: %d\n", glyphCache[5].cacheEntries, glyphCache[5].cacheMaximumCellSize);
	fprintf(stderr, "\tglyphCache6: Entries: %d MaximumCellSize: %d\n", glyphCache[6].cacheEntries, glyphCache[6].cacheMaximumCellSize);
	fprintf(stderr, "\tglyphCache7: Entries: %d MaximumCellSize: %d\n", glyphCache[7].cacheEntries, glyphCache[7].cacheMaximumCellSize);
	fprintf(stderr, "\tglyphCache8: Entries: %d MaximumCellSize: %d\n", glyphCache[8].cacheEntries, glyphCache[8].cacheMaximumCellSize);
	fprintf(stderr, "\tglyphCache9: Entries: %d MaximumCellSize: %d\n", glyphCache[9].cacheEntries, glyphCache[9].cacheMaximumCellSize);
	fprintf(stderr, "\tfragCache: Entries: %d MaximumCellSize: %d\n", fragCache.cacheEntries, fragCache.cacheMaximumCellSize);
	fprintf(stderr, "\tglyphSupportLevel: 0x%04X\n", glyphSupportLevel);
	fprintf(stderr, "\tpad2Octets: 0x%04X\n", pad2Octets);

	return TRUE;
}

/**
 * Read offscreen bitmap cache capability set.\n
 * @msdn{cc240550}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_offscreen_bitmap_cache_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT32 offscreenSupportLevel;

	if (length < 12)
		return FALSE;

	Stream_Read_UINT32(s, offscreenSupportLevel); /* offscreenSupportLevel (4 bytes) */
	Stream_Read_UINT16(s, settings->OffscreenCacheSize); /* offscreenCacheSize (2 bytes) */
	Stream_Read_UINT16(s, settings->OffscreenCacheEntries); /* offscreenCacheEntries (2 bytes) */

	if (offscreenSupportLevel & TRUE)
		settings->OffscreenSupportLevel = TRUE;

	return TRUE;
}

/**
 * Write offscreen bitmap cache capability set.\n
 * @msdn{cc240550}
 * @param s stream
 * @param settings settings
 */

void rdp_write_offscreen_bitmap_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 offscreenSupportLevel = FALSE;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	if (settings->OffscreenSupportLevel)
		offscreenSupportLevel = TRUE;

	Stream_Write_UINT32(s, offscreenSupportLevel); /* offscreenSupportLevel (4 bytes) */
	Stream_Write_UINT16(s, settings->OffscreenCacheSize); /* offscreenCacheSize (2 bytes) */
	Stream_Write_UINT16(s, settings->OffscreenCacheEntries); /* offscreenCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_OFFSCREEN_CACHE);
}

BOOL rdp_print_offscreen_bitmap_cache_capability_set(wStream* s, UINT16 length)
{
	UINT32 offscreenSupportLevel;
	UINT16 offscreenCacheSize;
	UINT16 offscreenCacheEntries;

	fprintf(stderr, "OffscreenBitmapCacheCapabilitySet (length %d):\n", length);

	if (length < 12)
		return FALSE;

	Stream_Read_UINT32(s, offscreenSupportLevel); /* offscreenSupportLevel (4 bytes) */
	Stream_Read_UINT16(s, offscreenCacheSize); /* offscreenCacheSize (2 bytes) */
	Stream_Read_UINT16(s, offscreenCacheEntries); /* offscreenCacheEntries (2 bytes) */

	fprintf(stderr, "\toffscreenSupportLevel: 0x%08X\n", offscreenSupportLevel);
	fprintf(stderr, "\toffscreenCacheSize: 0x%04X\n", offscreenCacheSize);
	fprintf(stderr, "\toffscreenCacheEntries: 0x%04X\n", offscreenCacheEntries);

	return TRUE;
}

/**
 * Read bitmap cache host support capability set.\n
 * @msdn{cc240557}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_bitmap_cache_host_support_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	BYTE cacheVersion;

	if (length < 8)
		return FALSE;

	Stream_Read_UINT8(s, cacheVersion); /* cacheVersion (1 byte) */
	Stream_Seek_UINT8(s); /* pad1 (1 byte) */
	Stream_Seek_UINT16(s); /* pad2 (2 bytes) */

	if (cacheVersion & BITMAP_CACHE_V2)
		settings->BitmapCachePersistEnabled = TRUE;

	return TRUE;
}

/**
 * Write bitmap cache host support capability set.\n
 * @msdn{cc240557}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_cache_host_support_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	Stream_Write_UINT8(s, BITMAP_CACHE_V2); /* cacheVersion (1 byte) */
	Stream_Write_UINT8(s, 0); /* pad1 (1 byte) */
	Stream_Write_UINT16(s, 0); /* pad2 (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT);
}

BOOL rdp_print_bitmap_cache_host_support_capability_set(wStream* s, UINT16 length)
{
	BYTE cacheVersion;
	BYTE pad1;
	UINT16 pad2;

	fprintf(stderr, "BitmapCacheHostSupportCapabilitySet (length %d):\n", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT8(s, cacheVersion); /* cacheVersion (1 byte) */
	Stream_Read_UINT8(s, pad1); /* pad1 (1 byte) */
	Stream_Read_UINT16(s, pad2); /* pad2 (2 bytes) */

	fprintf(stderr, "\tcacheVersion: 0x%02X\n", cacheVersion);
	fprintf(stderr, "\tpad1: 0x%02X\n", pad1);
	fprintf(stderr, "\tpad2: 0x%04X\n", pad2);

	return TRUE;
}

void rdp_read_bitmap_cache_cell_info(wStream* s, BITMAP_CACHE_V2_CELL_INFO* cellInfo)
{
	UINT32 info;

	/**
	 * numEntries is in the first 31 bits, while the last bit (k)
	 * is used to indicate a persistent bitmap cache.
	 */

	Stream_Read_UINT32(s, info);

	cellInfo->numEntries = (info & 0x7FFFFFFF);
	cellInfo->persistent = (info & 0x80000000) ? 1 : 0;
}

void rdp_write_bitmap_cache_cell_info(wStream* s, BITMAP_CACHE_V2_CELL_INFO* cellInfo)
{
	UINT32 info;

	/**
	 * numEntries is in the first 31 bits, while the last bit (k)
	 * is used to indicate a persistent bitmap cache.
	 */

	info = (cellInfo->numEntries | (cellInfo->persistent << 31));
	Stream_Write_UINT32(s, info);
}

/**
 * Read bitmap cache v2 capability set.\n
 * @msdn{cc240560}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_bitmap_cache_v2_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 40)
		return FALSE;

	Stream_Seek_UINT16(s); /* cacheFlags (2 bytes) */
	Stream_Seek_UINT8(s); /* pad2 (1 byte) */
	Stream_Seek_UINT8(s); /* numCellCaches (1 byte) */
	Stream_Seek(s, 4); /* bitmapCache0CellInfo (4 bytes) */
	Stream_Seek(s, 4); /* bitmapCache1CellInfo (4 bytes) */
	Stream_Seek(s, 4); /* bitmapCache2CellInfo (4 bytes) */
	Stream_Seek(s, 4); /* bitmapCache3CellInfo (4 bytes) */
	Stream_Seek(s, 4); /* bitmapCache4CellInfo (4 bytes) */
	Stream_Seek(s, 12); /* pad3 (12 bytes) */

	return TRUE;
}

/**
 * Write bitmap cache v2 capability set.\n
 * @msdn{cc240560}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_cache_v2_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 cacheFlags;

	Stream_EnsureRemainingCapacity(s, 64);

	header = rdp_capability_set_start(s);

	cacheFlags = ALLOW_CACHE_WAITING_LIST_FLAG;

	if (settings->BitmapCachePersistEnabled)
		cacheFlags |= PERSISTENT_KEYS_EXPECTED_FLAG;

	Stream_Write_UINT16(s, cacheFlags); /* cacheFlags (2 bytes) */
	Stream_Write_UINT8(s, 0); /* pad2 (1 byte) */
	Stream_Write_UINT8(s, settings->BitmapCacheV2NumCells); /* numCellCaches (1 byte) */
	rdp_write_bitmap_cache_cell_info(s, &settings->BitmapCacheV2CellInfo[0]); /* bitmapCache0CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, &settings->BitmapCacheV2CellInfo[1]); /* bitmapCache1CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, &settings->BitmapCacheV2CellInfo[2]); /* bitmapCache2CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, &settings->BitmapCacheV2CellInfo[3]); /* bitmapCache3CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(s, &settings->BitmapCacheV2CellInfo[4]); /* bitmapCache4CellInfo (4 bytes) */
	Stream_Zero(s, 12); /* pad3 (12 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_V2);
}

BOOL rdp_print_bitmap_cache_v2_capability_set(wStream* s, UINT16 length)
{
	UINT16 cacheFlags;
	BYTE pad2;
	BYTE numCellCaches;
	BITMAP_CACHE_V2_CELL_INFO bitmapCacheV2CellInfo[5];

	fprintf(stderr, "BitmapCacheV2CapabilitySet (length %d):\n", length);

	if (length < 40)
		return FALSE;

	Stream_Read_UINT16(s, cacheFlags); /* cacheFlags (2 bytes) */
	Stream_Read_UINT8(s, pad2); /* pad2 (1 byte) */
	Stream_Read_UINT8(s, numCellCaches); /* numCellCaches (1 byte) */
	rdp_read_bitmap_cache_cell_info(s, &bitmapCacheV2CellInfo[0]); /* bitmapCache0CellInfo (4 bytes) */
	rdp_read_bitmap_cache_cell_info(s, &bitmapCacheV2CellInfo[1]); /* bitmapCache1CellInfo (4 bytes) */
	rdp_read_bitmap_cache_cell_info(s, &bitmapCacheV2CellInfo[2]); /* bitmapCache2CellInfo (4 bytes) */
	rdp_read_bitmap_cache_cell_info(s, &bitmapCacheV2CellInfo[3]); /* bitmapCache3CellInfo (4 bytes) */
	rdp_read_bitmap_cache_cell_info(s, &bitmapCacheV2CellInfo[4]); /* bitmapCache4CellInfo (4 bytes) */
	Stream_Seek(s, 12); /* pad3 (12 bytes) */

	fprintf(stderr, "\tcacheFlags: 0x%04X\n", cacheFlags);
	fprintf(stderr, "\tpad2: 0x%02X\n", pad2);
	fprintf(stderr, "\tnumCellCaches: 0x%02X\n", numCellCaches);
	fprintf(stderr, "\tbitmapCache0CellInfo: numEntries: %d persistent: %d\n", bitmapCacheV2CellInfo[0].numEntries, bitmapCacheV2CellInfo[0].persistent);
	fprintf(stderr, "\tbitmapCache1CellInfo: numEntries: %d persistent: %d\n", bitmapCacheV2CellInfo[1].numEntries, bitmapCacheV2CellInfo[1].persistent);
	fprintf(stderr, "\tbitmapCache2CellInfo: numEntries: %d persistent: %d\n", bitmapCacheV2CellInfo[2].numEntries, bitmapCacheV2CellInfo[2].persistent);
	fprintf(stderr, "\tbitmapCache3CellInfo: numEntries: %d persistent: %d\n", bitmapCacheV2CellInfo[3].numEntries, bitmapCacheV2CellInfo[3].persistent);
	fprintf(stderr, "\tbitmapCache4CellInfo: numEntries: %d persistent: %d\n", bitmapCacheV2CellInfo[4].numEntries, bitmapCacheV2CellInfo[4].persistent);

	return TRUE;
}

/**
 * Read virtual channel capability set.\n
 * @msdn{cc240551}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_virtual_channel_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT32 flags;
	UINT32 VCChunkSize;

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, flags); /* flags (4 bytes) */

	if (length > 8)
		Stream_Read_UINT32(s, VCChunkSize); /* VCChunkSize (4 bytes) */
	else
		VCChunkSize = 1600;

	if (settings->ServerMode != TRUE)
		settings->VirtualChannelChunkSize = VCChunkSize;

	return TRUE;
}

/**
 * Write virtual channel capability set.\n
 * @msdn{cc240551}
 * @param s stream
 * @param settings settings
 */

void rdp_write_virtual_channel_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 flags;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	flags = VCCAPS_NO_COMPR;

	Stream_Write_UINT32(s, flags); /* flags (4 bytes) */
	Stream_Write_UINT32(s, settings->VirtualChannelChunkSize); /* VCChunkSize (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_VIRTUAL_CHANNEL);
}

BOOL rdp_print_virtual_channel_capability_set(wStream* s, UINT16 length)
{
	UINT32 flags;
	UINT32 VCChunkSize;

	fprintf(stderr, "VirtualChannelCapabilitySet (length %d):\n", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, flags); /* flags (4 bytes) */

	if (length > 8)
		Stream_Read_UINT32(s, VCChunkSize); /* VCChunkSize (4 bytes) */
	else
		VCChunkSize = 1600;

	fprintf(stderr, "\tflags: 0x%08X\n", flags);
	fprintf(stderr, "\tVCChunkSize: 0x%08X\n", VCChunkSize);

	return TRUE;
}

/**
 * Read drawn nine grid cache capability set.\n
 * @msdn{cc241565}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_draw_nine_grid_cache_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT32 drawNineGridSupportLevel;

	if (length < 12)
		return FALSE;

	Stream_Read_UINT32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	Stream_Read_UINT16(s, settings->DrawNineGridCacheSize); /* drawNineGridCacheSize (2 bytes) */
	Stream_Read_UINT16(s, settings->DrawNineGridCacheEntries); /* drawNineGridCacheEntries (2 bytes) */

	if ((drawNineGridSupportLevel & DRAW_NINEGRID_SUPPORTED) ||
			(drawNineGridSupportLevel & DRAW_NINEGRID_SUPPORTED_V2))
		settings->DrawNineGridEnabled = TRUE;

	return TRUE;
}

/**
 * Write drawn nine grid cache capability set.\n
 * @msdn{cc241565}
 * @param s stream
 * @param settings settings
 */

void rdp_write_draw_nine_grid_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 drawNineGridSupportLevel;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	drawNineGridSupportLevel = (settings->DrawNineGridEnabled) ? DRAW_NINEGRID_SUPPORTED_V2 : DRAW_NINEGRID_NO_SUPPORT;

	Stream_Write_UINT32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	Stream_Write_UINT16(s, settings->DrawNineGridCacheSize); /* drawNineGridCacheSize (2 bytes) */
	Stream_Write_UINT16(s, settings->DrawNineGridCacheEntries); /* drawNineGridCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_DRAW_NINE_GRID_CACHE);
}

void rdp_write_gdiplus_cache_entries(wStream* s, UINT16 gce, UINT16 bce, UINT16 pce, UINT16 ice, UINT16 ace)
{
	Stream_Write_UINT16(s, gce); /* gdipGraphicsCacheEntries (2 bytes) */
	Stream_Write_UINT16(s, bce); /* gdipBrushCacheEntries (2 bytes) */
	Stream_Write_UINT16(s, pce); /* gdipPenCacheEntries (2 bytes) */
	Stream_Write_UINT16(s, ice); /* gdipImageCacheEntries (2 bytes) */
	Stream_Write_UINT16(s, ace); /* gdipImageAttributesCacheEntries (2 bytes) */
}

void rdp_write_gdiplus_cache_chunk_size(wStream* s, UINT16 gccs, UINT16 obccs, UINT16 opccs, UINT16 oiaccs)
{
	Stream_Write_UINT16(s, gccs); /* gdipGraphicsCacheChunkSize (2 bytes) */
	Stream_Write_UINT16(s, obccs); /* gdipObjectBrushCacheChunkSize (2 bytes) */
	Stream_Write_UINT16(s, opccs); /* gdipObjectPenCacheChunkSize (2 bytes) */
	Stream_Write_UINT16(s, oiaccs); /* gdipObjectImageAttributesCacheChunkSize (2 bytes) */
}

void rdp_write_gdiplus_image_cache_properties(wStream* s, UINT16 oiccs, UINT16 oicts, UINT16 oicms)
{
	Stream_Write_UINT16(s, oiccs); /* gdipObjectImageCacheChunkSize (2 bytes) */
	Stream_Write_UINT16(s, oicts); /* gdipObjectImageCacheTotalSize (2 bytes) */
	Stream_Write_UINT16(s, oicms); /* gdipObjectImageCacheMaxSize (2 bytes) */
}

BOOL rdp_print_draw_nine_grid_cache_capability_set(wStream* s, UINT16 length)
{
	UINT32 drawNineGridSupportLevel;
	UINT16 DrawNineGridCacheSize;
	UINT16 DrawNineGridCacheEntries;

	fprintf(stderr, "DrawNineGridCacheCapabilitySet (length %d):\n", length);

	if (length < 12)
		return FALSE;

	Stream_Read_UINT32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	Stream_Read_UINT16(s, DrawNineGridCacheSize); /* drawNineGridCacheSize (2 bytes) */
	Stream_Read_UINT16(s, DrawNineGridCacheEntries); /* drawNineGridCacheEntries (2 bytes) */

	return TRUE;
}

/**
 * Read GDI+ cache capability set.\n
 * @msdn{cc241566}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_draw_gdiplus_cache_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT32 drawGDIPlusSupportLevel;
	UINT32 drawGdiplusCacheLevel;

	if (length < 40)
		return FALSE;

	Stream_Read_UINT32(s, drawGDIPlusSupportLevel); /* drawGDIPlusSupportLevel (4 bytes) */
	Stream_Seek_UINT32(s); /* GdipVersion (4 bytes) */
	Stream_Read_UINT32(s, drawGdiplusCacheLevel); /* drawGdiplusCacheLevel (4 bytes) */
	Stream_Seek(s, 10); /* GdipCacheEntries (10 bytes) */
	Stream_Seek(s, 8); /* GdipCacheChunkSize (8 bytes) */
	Stream_Seek(s, 6); /* GdipImageCacheProperties (6 bytes) */

	if (drawGDIPlusSupportLevel & DRAW_GDIPLUS_SUPPORTED)
		settings->DrawGdiPlusEnabled = TRUE;

	if (drawGdiplusCacheLevel & DRAW_GDIPLUS_CACHE_LEVEL_ONE)
		settings->DrawGdiPlusCacheEnabled = TRUE;

	return TRUE;
}

/**
 * Write GDI+ cache capability set.\n
 * @msdn{cc241566}
 * @param s stream
 * @param settings settings
 */

void rdp_write_draw_gdiplus_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 drawGDIPlusSupportLevel;
	UINT32 drawGdiplusCacheLevel;

	Stream_EnsureRemainingCapacity(s, 64);

	header = rdp_capability_set_start(s);

	drawGDIPlusSupportLevel = (settings->DrawGdiPlusEnabled) ? DRAW_GDIPLUS_SUPPORTED : DRAW_GDIPLUS_DEFAULT;
	drawGdiplusCacheLevel = (settings->DrawGdiPlusEnabled) ? DRAW_GDIPLUS_CACHE_LEVEL_ONE : DRAW_GDIPLUS_CACHE_LEVEL_DEFAULT;

	Stream_Write_UINT32(s, drawGDIPlusSupportLevel); /* drawGDIPlusSupportLevel (4 bytes) */
	Stream_Write_UINT32(s, 0); /* GdipVersion (4 bytes) */
	Stream_Write_UINT32(s, drawGdiplusCacheLevel); /* drawGdiplusCacheLevel (4 bytes) */
	rdp_write_gdiplus_cache_entries(s, 10, 5, 5, 10, 2); /* GdipCacheEntries (10 bytes) */
	rdp_write_gdiplus_cache_chunk_size(s, 512, 2048, 1024, 64); /* GdipCacheChunkSize (8 bytes) */
	rdp_write_gdiplus_image_cache_properties(s, 4096, 256, 128); /* GdipImageCacheProperties (6 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_DRAW_GDI_PLUS);
}

BOOL rdp_print_draw_gdiplus_cache_capability_set(wStream* s, UINT16 length)
{
	UINT32 drawGdiPlusSupportLevel;
	UINT32 GdipVersion;
	UINT32 drawGdiplusCacheLevel;

	fprintf(stderr, "DrawGdiPlusCacheCapabilitySet (length %d):\n", length);

	if (length < 40)
		return FALSE;

	Stream_Read_UINT32(s, drawGdiPlusSupportLevel); /* drawGdiPlusSupportLevel (4 bytes) */
	Stream_Read_UINT32(s, GdipVersion); /* GdipVersion (4 bytes) */
	Stream_Read_UINT32(s, drawGdiplusCacheLevel); /* drawGdiPlusCacheLevel (4 bytes) */
	Stream_Seek(s, 10); /* GdipCacheEntries (10 bytes) */
	Stream_Seek(s, 8); /* GdipCacheChunkSize (8 bytes) */
	Stream_Seek(s, 6); /* GdipImageCacheProperties (6 bytes) */

	return TRUE;
}

/**
 * Read remote programs capability set.\n
 * @msdn{cc242518}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_remote_programs_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT32 railSupportLevel;

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, railSupportLevel); /* railSupportLevel (4 bytes) */

	if ((railSupportLevel & RAIL_LEVEL_SUPPORTED) == 0)
	{
		if (settings->RemoteApplicationMode == TRUE)
		{
			/* RemoteApp Failure! */
			settings->RemoteApplicationMode = FALSE;
		}
	}
	return TRUE;
}

/**
 * Write remote programs capability set.\n
 * @msdn{cc242518}
 * @param s stream
 * @param settings settings
 */

void rdp_write_remote_programs_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 railSupportLevel;

	Stream_EnsureRemainingCapacity(s, 64);

	header = rdp_capability_set_start(s);

	railSupportLevel = RAIL_LEVEL_SUPPORTED;

	if (settings->RemoteAppLanguageBarSupported)
		railSupportLevel |= RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED;

	Stream_Write_UINT32(s, railSupportLevel); /* railSupportLevel (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_RAIL);
}

BOOL rdp_print_remote_programs_capability_set(wStream* s, UINT16 length)
{
	UINT32 railSupportLevel;

	fprintf(stderr, "RemoteProgramsCapabilitySet (length %d):\n", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, railSupportLevel); /* railSupportLevel (4 bytes) */

	fprintf(stderr, "\trailSupportLevel: 0x%08X\n", railSupportLevel);

	return TRUE;
}

/**
 * Read window list capability set.\n
 * @msdn{cc242564}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_window_list_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 11)
		return FALSE;

	Stream_Seek_UINT32(s); /* wndSupportLevel (4 bytes) */
	Stream_Seek_UINT8(s); /* numIconCaches (1 byte) */
	Stream_Seek_UINT16(s); /* numIconCacheEntries (2 bytes) */

	return TRUE;
}

/**
 * Write window list capability set.\n
 * @msdn{cc242564}
 * @param s stream
 * @param settings settings
 */

void rdp_write_window_list_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 wndSupportLevel;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	wndSupportLevel = WINDOW_LEVEL_SUPPORTED_EX;

	Stream_Write_UINT32(s, wndSupportLevel); /* wndSupportLevel (4 bytes) */
	Stream_Write_UINT8(s, settings->RemoteAppNumIconCaches); /* numIconCaches (1 byte) */
	Stream_Write_UINT16(s, settings->RemoteAppNumIconCacheEntries); /* numIconCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_WINDOW);
}

BOOL rdp_print_window_list_capability_set(wStream* s, UINT16 length)
{
	UINT32 wndSupportLevel;
	BYTE numIconCaches;
	UINT16 numIconCacheEntries;

	fprintf(stderr, "WindowListCapabilitySet (length %d):\n", length);

	if (length < 11)
		return FALSE;

	Stream_Read_UINT32(s, wndSupportLevel); /* wndSupportLevel (4 bytes) */
	Stream_Read_UINT8(s, numIconCaches); /* numIconCaches (1 byte) */
	Stream_Read_UINT16(s, numIconCacheEntries); /* numIconCacheEntries (2 bytes) */

	fprintf(stderr, "\twndSupportLevel: 0x%08X\n", wndSupportLevel);
	fprintf(stderr, "\tnumIconCaches: 0x%02X\n", numIconCaches);
	fprintf(stderr, "\tnumIconCacheEntries: 0x%04X\n", numIconCacheEntries);

	return TRUE;
}

/**
 * Read desktop composition capability set.\n
 * @msdn{cc240855}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_desktop_composition_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 6)
		return FALSE;

	Stream_Seek_UINT16(s); /* compDeskSupportLevel (2 bytes) */

	return TRUE;
}

/**
 * Write desktop composition capability set.\n
 * @msdn{cc240855}
 * @param s stream
 * @param settings settings
 */

void rdp_write_desktop_composition_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 compDeskSupportLevel;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	compDeskSupportLevel = (settings->AllowDesktopComposition) ? COMPDESK_SUPPORTED : COMPDESK_NOT_SUPPORTED;

	Stream_Write_UINT16(s, compDeskSupportLevel); /* compDeskSupportLevel (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_COMP_DESK);
}

BOOL rdp_print_desktop_composition_capability_set(wStream* s, UINT16 length)
{
	UINT16 compDeskSupportLevel;

	fprintf(stderr, "DesktopCompositionCapabilitySet (length %d):\n", length);

	if (length < 6)
		return FALSE;

	Stream_Read_UINT16(s, compDeskSupportLevel); /* compDeskSupportLevel (2 bytes) */

	fprintf(stderr, "\tcompDeskSupportLevel: 0x%04X\n", compDeskSupportLevel);

	return TRUE;
}

/**
 * Read multifragment update capability set.\n
 * @msdn{cc240649}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_multifragment_update_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT32 multifragMaxRequestSize;

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, multifragMaxRequestSize); /* MaxRequestSize (4 bytes) */

	if (settings->ServerMode)
	{
		if (settings->RemoteFxCodec)
		{
			/**
			 * If we are using RemoteFX the client MUST use a value greater
			 * than or equal to the value we've previously sent in the server to
			 * client multi-fragment update capability set (MS-RDPRFX 1.5)
			 */
			if (multifragMaxRequestSize < settings->MultifragMaxRequestSize)
			{
				/**
				 * If it happens to be smaller we honor the client's value but
				 * have to disable RemoteFX
				 */
				settings->RemoteFxCodec = FALSE;
				settings->MultifragMaxRequestSize = multifragMaxRequestSize;
			}
			else
			{
				/* no need to increase server's max request size setting here */
			}
		}
		else
		{
			settings->MultifragMaxRequestSize = multifragMaxRequestSize;
		}
	}
	else
	{
		/**
		 * In client mode we keep up with the server's capabilites.
		 * In RemoteFX mode we MUST do this but it might also be useful to
		 * receive larger related bitmap updates.
		 */
		if (multifragMaxRequestSize > settings->MultifragMaxRequestSize)
			settings->MultifragMaxRequestSize = multifragMaxRequestSize;
	}

	return TRUE;
}

/**
 * Write multifragment update capability set.\n
 * @msdn{cc240649}
 * @param s stream
 * @param settings settings
 */

void rdp_write_multifragment_update_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 32);

	if (settings->ServerMode)
	{
		/**
		 * In server mode we prefer to use the highest useful request size that
		 * will allow us to pack a complete screen update into a single fast
		 * path PDU using any of the supported codecs.
		 * However, the client is completely free to accept our proposed
		 * max request size or send a different value in the client-to-server
		 * multi-fragment update capability set and we have to accept that,
		 * unless we are using RemoteFX where the client MUST announce a value
		 * greater than or equal to the value we're sending here.
		 * See [MS-RDPRFX 1.5 capability #2]
		 */

		UINT32 tileNumX = (settings->DesktopWidth+63)/64;
		UINT32 tileNumY = (settings->DesktopHeight+63)/64;

		settings->MultifragMaxRequestSize = tileNumX * tileNumY * 16384;

		/* and add room for headers, regions, frame markers, etc. */
		settings->MultifragMaxRequestSize += 16384;
	}

	header = rdp_capability_set_start(s);

	Stream_Write_UINT32(s, settings->MultifragMaxRequestSize); /* MaxRequestSize (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_MULTI_FRAGMENT_UPDATE);
}

BOOL rdp_print_multifragment_update_capability_set(wStream* s, UINT16 length)
{
	UINT32 maxRequestSize;

	fprintf(stderr, "MultifragmentUpdateCapabilitySet (length %d):\n", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, maxRequestSize); /* maxRequestSize (4 bytes) */

	fprintf(stderr, "\tmaxRequestSize: 0x%04X\n", maxRequestSize);

	return TRUE;
}

/**
 * Read large pointer capability set.\n
 * @msdn{cc240650}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_large_pointer_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT16 largePointerSupportFlags;

	if (length < 6)
		return FALSE;

	Stream_Read_UINT16(s, largePointerSupportFlags); /* largePointerSupportFlags (2 bytes) */

	settings->LargePointerFlag = (largePointerSupportFlags & LARGE_POINTER_FLAG_96x96) ? 1 : 0;

	return TRUE;
}

/**
 * Write large pointer capability set.\n
 * @msdn{cc240650}
 * @param s stream
 * @param settings settings
 */

void rdp_write_large_pointer_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 largePointerSupportFlags;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	largePointerSupportFlags = (settings->LargePointerFlag) ? LARGE_POINTER_FLAG_96x96 : 0;

	Stream_Write_UINT16(s, largePointerSupportFlags); /* largePointerSupportFlags (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_LARGE_POINTER);
}

BOOL rdp_print_large_pointer_capability_set(wStream* s, UINT16 length)
{
	UINT16 largePointerSupportFlags;

	fprintf(stderr, "LargePointerCapabilitySet (length %d):\n", length);

	if (length < 6)
		return FALSE;

	Stream_Read_UINT16(s, largePointerSupportFlags); /* largePointerSupportFlags (2 bytes) */

	fprintf(stderr, "\tlargePointerSupportFlags: 0x%04X\n", largePointerSupportFlags);

	return TRUE;
}

/**
 * Read surface commands capability set.\n
 * @msdn{dd871563}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_surface_commands_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	UINT32 cmdFlags;
	if (length < 12)
		return FALSE;

	Stream_Read_UINT32(s, cmdFlags); /* cmdFlags (4 bytes) */
	Stream_Seek_UINT32(s); /* reserved (4 bytes) */

	settings->SurfaceCommandsEnabled = TRUE;
	settings->SurfaceFrameMarkerEnabled = (cmdFlags & SURFCMDS_FRAME_MARKER) ? TRUE : FALSE;

	return TRUE;
}

/**
 * Write surface commands capability set.\n
 * @msdn{dd871563}
 * @param s stream
 * @param settings settings
 */

void rdp_write_surface_commands_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 cmdFlags;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	cmdFlags = SURFCMDS_SET_SURFACE_BITS |
			SURFCMDS_STREAM_SURFACE_BITS;
	if (settings->SurfaceFrameMarkerEnabled)
		cmdFlags |= SURFCMDS_FRAME_MARKER;

	Stream_Write_UINT32(s, cmdFlags); /* cmdFlags (4 bytes) */
	Stream_Write_UINT32(s, 0); /* reserved (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SURFACE_COMMANDS);
}

BOOL rdp_print_surface_commands_capability_set(wStream* s, UINT16 length)
{
	UINT32 cmdFlags;
	UINT32 reserved;

	fprintf(stderr, "SurfaceCommandsCapabilitySet (length %d):\n", length);

	if (length < 12)
		return FALSE;

	Stream_Read_UINT32(s, cmdFlags); /* cmdFlags (4 bytes) */
	Stream_Read_UINT32(s, reserved); /* reserved (4 bytes) */

	fprintf(stderr, "\tcmdFlags: 0x%08X\n", cmdFlags);
	fprintf(stderr, "\treserved: 0x%08X\n", reserved);

	return TRUE;
}

void rdp_read_bitmap_codec_guid(wStream* s, GUID* guid)
{
	BYTE g[16];

	Stream_Read(s, g, 16);

	guid->Data1 = (g[3] << 24) | (g[2] << 16) | (g[1] << 8) | g[0];
	guid->Data2 = (g[5] << 8) | g[4];
	guid->Data3 = (g[7] << 8) | g[6];
	guid->Data4[0] = g[8];
	guid->Data4[1] = g[9];
	guid->Data4[2] = g[10];
	guid->Data4[3] = g[11];
	guid->Data4[4] = g[12];
	guid->Data4[5] = g[13];
	guid->Data4[6] = g[14];
	guid->Data4[7] = g[15];
}

void rdp_write_bitmap_codec_guid(wStream* s, GUID* guid)
{
	BYTE g[16];

	g[0] = guid->Data1 & 0xFF;
	g[1] = (guid->Data1 >> 8) & 0xFF;
	g[2] = (guid->Data1 >> 16) & 0xFF;
	g[3] = (guid->Data1 >> 24) & 0xFF;
	g[4] = (guid->Data2) & 0xFF;
	g[5] = (guid->Data2 >> 8) & 0xFF;
	g[6] = (guid->Data3) & 0xFF;
	g[7] = (guid->Data3 >> 8) & 0xFF;
	g[8] = guid->Data4[0];
	g[9] = guid->Data4[1];
	g[10] = guid->Data4[2];
	g[11] = guid->Data4[3];
	g[12] = guid->Data4[4];
	g[13] = guid->Data4[5];
	g[14] = guid->Data4[6];
	g[15] = guid->Data4[7];

	Stream_Write(s, g, 16);
}

void rdp_print_bitmap_codec_guid(GUID* guid)
{
	fprintf(stderr, "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
		guid->Data1, guid->Data2, guid->Data3,
		guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
		guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}

char* rdp_get_bitmap_codec_guid_name(GUID* guid)
{
	RPC_STATUS rpc_status;

	if (UuidEqual(guid, &CODEC_GUID_REMOTEFX, &rpc_status))
		return "CODEC_GUID_REMOTEFX";
	else if (UuidEqual(guid, &CODEC_GUID_NSCODEC, &rpc_status))
		return "CODEC_GUID_NSCODEC";
	else if (UuidEqual(guid, &CODEC_GUID_IGNORE, &rpc_status))
		return "CODEC_GUID_IGNORE";
	else if (UuidEqual(guid, &CODEC_GUID_IMAGE_REMOTEFX, &rpc_status))
		return "CODEC_GUID_IMAGE_REMOTEFX";
	else if (UuidEqual(guid, &CODEC_GUID_JPEG, &rpc_status))
		return "CODEC_GUID_JPEG";

	return "CODEC_GUID_UNKNOWN";
}

/**
 * Read bitmap codecs capability set.\n
 * @msdn{dd891377}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_bitmap_codecs_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	GUID codecGuid;
	RPC_STATUS rpc_status;
	BYTE bitmapCodecCount;
	UINT16 codecPropertiesLength;
	UINT16 remainingLength;
	BOOL receivedRemoteFxCodec = FALSE;
	BOOL receivedNSCodec = FALSE;

	if (length < 5)
		return FALSE;

	Stream_Read_UINT8(s, bitmapCodecCount); /* bitmapCodecCount (1 byte) */
	remainingLength = length - 5;

	while (bitmapCodecCount > 0)
	{
		if (remainingLength < 19)
			return FALSE;

		rdp_read_bitmap_codec_guid(s, &codecGuid); /* codecGuid (16 bytes) */

		if (settings->ServerMode)
		{
			if (UuidEqual(&codecGuid, &CODEC_GUID_REMOTEFX, &rpc_status))
			{
				Stream_Read_UINT8(s, settings->RemoteFxCodecId);
				receivedRemoteFxCodec = TRUE;
			}
			else if (UuidEqual(&codecGuid, &CODEC_GUID_NSCODEC, &rpc_status))
			{
				Stream_Read_UINT8(s, settings->NSCodecId);
				receivedNSCodec = TRUE;
			}
			else
			{
				Stream_Seek_UINT8(s); /* codecID (1 byte) */
			}
		}
		else
		{
			Stream_Seek_UINT8(s); /* codecID (1 byte) */
		}

		Stream_Read_UINT16(s, codecPropertiesLength); /* codecPropertiesLength (2 bytes) */
		remainingLength -= 19;

		if (remainingLength < codecPropertiesLength)
			return FALSE;

		if (settings->ServerMode)
		{
			if (UuidEqual(&codecGuid, &CODEC_GUID_REMOTEFX, &rpc_status))
			{
				Stream_Seek_UINT32(s); /* length */
				Stream_Read_UINT32(s, settings->RemoteFxCaptureFlags); /* captureFlags */
				Stream_Rewind(s, 8);

				if (settings->RemoteFxCaptureFlags & CARDP_CAPS_CAPTURE_NON_CAC)
				{
					settings->RemoteFxOnly = TRUE;
				}
			}
		}

		Stream_Seek(s, codecPropertiesLength); /* codecProperties */
		remainingLength -= codecPropertiesLength;

		bitmapCodecCount--;
	}

	if (settings->ServerMode)
	{
		/* only enable a codec if we've announced/enabled it before */
		settings->RemoteFxCodec = settings->RemoteFxCodec && receivedRemoteFxCodec;
		settings->NSCodec = settings->NSCodec && receivedNSCodec;
		settings->JpegCodec = FALSE;
	}

	return TRUE;
}

/**
 * Write RemoteFX Client Capability Container.\n
 * @param s stream
 * @param settings settings
 */
void rdp_write_rfx_client_capability_container(wStream* s, rdpSettings* settings)
{
	UINT32 captureFlags;
	BYTE codecMode;

	Stream_EnsureRemainingCapacity(s, 64);

	captureFlags = settings->RemoteFxOnly ? 0 : CARDP_CAPS_CAPTURE_NON_CAC;
	codecMode = settings->RemoteFxCodecMode;

	Stream_Write_UINT16(s, 49); /* codecPropertiesLength */

	/* TS_RFX_CLNT_CAPS_CONTAINER */
	Stream_Write_UINT32(s, 49); /* length */
	Stream_Write_UINT32(s, captureFlags); /* captureFlags */
	Stream_Write_UINT32(s, 37); /* capsLength */

	/* TS_RFX_CAPS */
	Stream_Write_UINT16(s, CBY_CAPS); /* blockType */
	Stream_Write_UINT32(s, 8); /* blockLen */
	Stream_Write_UINT16(s, 1); /* numCapsets */

	/* TS_RFX_CAPSET */
	Stream_Write_UINT16(s, CBY_CAPSET); /* blockType */
	Stream_Write_UINT32(s, 29); /* blockLen */
	Stream_Write_UINT8(s, 0x01); /* codecId (MUST be set to 0x01) */
	Stream_Write_UINT16(s, CLY_CAPSET); /* capsetType */
	Stream_Write_UINT16(s, 2); /* numIcaps */
	Stream_Write_UINT16(s, 8); /* icapLen */

	/* TS_RFX_ICAP (RLGR1) */
	Stream_Write_UINT16(s, CLW_VERSION_1_0); /* version */
	Stream_Write_UINT16(s, CT_TILE_64x64); /* tileSize */
	Stream_Write_UINT8(s, codecMode); /* flags */
	Stream_Write_UINT8(s, CLW_COL_CONV_ICT); /* colConvBits */
	Stream_Write_UINT8(s, CLW_XFORM_DWT_53_A); /* transformBits */
	Stream_Write_UINT8(s, CLW_ENTROPY_RLGR1); /* entropyBits */

	/* TS_RFX_ICAP (RLGR3) */
	Stream_Write_UINT16(s, CLW_VERSION_1_0); /* version */
	Stream_Write_UINT16(s, CT_TILE_64x64); /* tileSize */
	Stream_Write_UINT8(s, codecMode); /* flags */
	Stream_Write_UINT8(s, CLW_COL_CONV_ICT); /* colConvBits */
	Stream_Write_UINT8(s, CLW_XFORM_DWT_53_A); /* transformBits */
	Stream_Write_UINT8(s, CLW_ENTROPY_RLGR3); /* entropyBits */
}

/**
 * Write NSCODEC Client Capability Container.\n
 * @param s stream
 * @param settings settings
 */
void rdp_write_nsc_client_capability_container(wStream* s, rdpSettings* settings)
{
	Stream_EnsureRemainingCapacity(s, 8);

	Stream_Write_UINT16(s, 3); /* codecPropertiesLength */

	/* TS_NSCODEC_CAPABILITYSET */
	Stream_Write_UINT8(s, 1);  /* fAllowDynamicFidelity */
	Stream_Write_UINT8(s, 1);  /* fAllowSubsampling */
	Stream_Write_UINT8(s, 3);  /* colorLossLevel */
}

void rdp_write_jpeg_client_capability_container(wStream* s, rdpSettings* settings)
{
	Stream_EnsureRemainingCapacity(s, 8);

	Stream_Write_UINT16(s, 1); /* codecPropertiesLength */
	Stream_Write_UINT8(s, settings->JpegQuality);
}

/**
 * Write RemoteFX Server Capability Container.\n
 * @param s stream
 * @param settings settings
 */
void rdp_write_rfx_server_capability_container(wStream* s, rdpSettings* settings)
{
	Stream_EnsureRemainingCapacity(s, 8);

	Stream_Write_UINT16(s, 4); /* codecPropertiesLength */
	Stream_Write_UINT32(s, 0); /* reserved */
}

void rdp_write_jpeg_server_capability_container(wStream* s, rdpSettings* settings)
{
	Stream_EnsureRemainingCapacity(s, 8);

	Stream_Write_UINT16(s, 1); /* codecPropertiesLength */
	Stream_Write_UINT8(s, 75);
}

/**
 * Write NSCODEC Server Capability Container.\n
 * @param s stream
 * @param settings settings
 */
void rdp_write_nsc_server_capability_container(wStream* s, rdpSettings* settings)
{
	Stream_EnsureRemainingCapacity(s, 8);

	Stream_Write_UINT16(s, 4); /* codecPropertiesLength */
	Stream_Write_UINT32(s, 0); /* reserved */
}

/**
 * Write bitmap codecs capability set.\n
 * @msdn{dd891377}
 * @param s stream
 * @param settings settings
 */

void rdp_write_bitmap_codecs_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	BYTE bitmapCodecCount;

	Stream_EnsureRemainingCapacity(s, 64);

	header = rdp_capability_set_start(s);

	bitmapCodecCount = 0;

	if (settings->RemoteFxCodec)
		settings->RemoteFxImageCodec = TRUE;

	if (settings->RemoteFxCodec)
		bitmapCodecCount++;
	if (settings->NSCodec)
		bitmapCodecCount++;
	if (settings->JpegCodec)
		bitmapCodecCount++;
	if (settings->RemoteFxImageCodec)
		bitmapCodecCount++;

	Stream_Write_UINT8(s, bitmapCodecCount);

	if (settings->RemoteFxCodec)
	{
		rdp_write_bitmap_codec_guid(s, &CODEC_GUID_REMOTEFX); /* codecGUID */

		if (settings->ServerMode)
		{
			Stream_Write_UINT8(s, 0); /* codecID is defined by the client */
			rdp_write_rfx_server_capability_container(s, settings);
		}
		else
		{
			Stream_Write_UINT8(s, RDP_CODEC_ID_REMOTEFX); /* codecID */
			rdp_write_rfx_client_capability_container(s, settings);
		}
	}

	if (settings->NSCodec)
	{
		rdp_write_bitmap_codec_guid(s, &CODEC_GUID_NSCODEC); /* codecGUID */

		if (settings->ServerMode)
		{
			Stream_Write_UINT8(s, 0); /* codecID is defined by the client */
			rdp_write_nsc_server_capability_container(s, settings);
		}
		else
		{
			Stream_Write_UINT8(s, RDP_CODEC_ID_NSCODEC); /* codecID */
			rdp_write_nsc_client_capability_container(s, settings);
		}
	}

	if (settings->JpegCodec)
	{
		rdp_write_bitmap_codec_guid(s, &CODEC_GUID_JPEG); /* codecGUID */

		if (settings->ServerMode)
		{
			Stream_Write_UINT8(s, 0); /* codecID is defined by the client */
			rdp_write_jpeg_server_capability_container(s, settings);
		}
		else
		{
			Stream_Write_UINT8(s, RDP_CODEC_ID_JPEG); /* codecID */
			rdp_write_jpeg_client_capability_container(s, settings);
		}
	}

	if (settings->RemoteFxImageCodec)
	{
		rdp_write_bitmap_codec_guid(s, &CODEC_GUID_IMAGE_REMOTEFX); /* codecGUID */

		if (settings->ServerMode)
		{
			Stream_Write_UINT8(s, 0); /* codecID is defined by the client */
			rdp_write_rfx_server_capability_container(s, settings);
		}
		else
		{
			Stream_Write_UINT8(s, RDP_CODEC_ID_IMAGE_REMOTEFX); /* codecID */
			rdp_write_rfx_client_capability_container(s, settings);
		}
	}

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CODECS);
}

BOOL rdp_print_bitmap_codecs_capability_set(wStream* s, UINT16 length)
{
	GUID codecGuid;
	BYTE bitmapCodecCount;
	BYTE codecId;
	UINT16 codecPropertiesLength;
	UINT16 remainingLength;

	fprintf(stderr, "BitmapCodecsCapabilitySet (length %d):\n", length);

	if (length < 5)
		return FALSE;

	Stream_Read_UINT8(s, bitmapCodecCount); /* bitmapCodecCount (1 byte) */
	remainingLength = length - 5;

	fprintf(stderr, "\tbitmapCodecCount: %d\n", bitmapCodecCount);

	while (bitmapCodecCount > 0)
	{
		if (remainingLength < 19)
			return FALSE;

		rdp_read_bitmap_codec_guid(s, &codecGuid); /* codecGuid (16 bytes) */
		Stream_Read_UINT8(s, codecId); /* codecId (1 byte) */

		fprintf(stderr, "\tcodecGuid: 0x");
		rdp_print_bitmap_codec_guid(&codecGuid);
		fprintf(stderr, " (%s)\n", rdp_get_bitmap_codec_guid_name(&codecGuid));

		fprintf(stderr, "\tcodecId: %d\n", codecId);

		Stream_Read_UINT16(s, codecPropertiesLength); /* codecPropertiesLength (2 bytes) */
		fprintf(stderr, "\tcodecPropertiesLength: %d\n", codecPropertiesLength);

		remainingLength -= 19;

		if (remainingLength < codecPropertiesLength)
			return FALSE;

		Stream_Seek(s, codecPropertiesLength); /* codecProperties */
		remainingLength -= codecPropertiesLength;

		bitmapCodecCount--;
	}

	return TRUE;
}

/**
 * Read frame acknowledge capability set.\n
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_frame_acknowledge_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	if (length < 8)
		return FALSE;

	if (settings->ServerMode)
	{
		Stream_Read_UINT32(s, settings->FrameAcknowledge); /* (4 bytes) */
	}
	else
	{
		Stream_Seek_UINT32(s); /* (4 bytes) */
	}

	return TRUE;
}

/**
 * Write frame acknowledge capability set.\n
 * @param s stream
 * @param settings settings
 */

void rdp_write_frame_acknowledge_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);

	Stream_Write_UINT32(s, settings->FrameAcknowledge); /* (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_FRAME_ACKNOWLEDGE);
}

BOOL rdp_print_frame_acknowledge_capability_set(wStream* s, UINT16 length)
{
	UINT32 frameAcknowledge;

	fprintf(stderr, "FrameAcknowledgeCapabilitySet (length %d):\n", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, frameAcknowledge); /* frameAcknowledge (4 bytes) */

	fprintf(stderr, "\tframeAcknowledge: 0x%08X\n", frameAcknowledge);

	return TRUE;
}

BOOL rdp_read_bitmap_cache_v3_codec_id_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	BYTE bitmapCacheV3CodecId;

	if (length < 5)
		return FALSE;

	Stream_Read_UINT8(s, bitmapCacheV3CodecId); /* bitmapCacheV3CodecId (1 byte) */

	return TRUE;
}

void rdp_write_bitmap_cache_v3_codec_id_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	Stream_EnsureRemainingCapacity(s, 32);

	header = rdp_capability_set_start(s);
	Stream_Write_UINT8(s, settings->BitmapCacheV3CodecId);

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID);
}

BOOL rdp_print_bitmap_cache_v3_codec_id_capability_set(wStream* s, UINT16 length)
{
	BYTE bitmapCacheV3CodecId;

	fprintf(stderr, "BitmapCacheV3CodecIdCapabilitySet (length %d):\n", length);

	if (length < 5)
		return FALSE;

	Stream_Read_UINT8(s, bitmapCacheV3CodecId); /* bitmapCacheV3CodecId (1 byte) */

	fprintf(stderr, "\tbitmapCacheV3CodecId: 0x%02X\n", bitmapCacheV3CodecId);

	return TRUE;
}

BOOL rdp_print_capability_sets(wStream* s, UINT16 numberCapabilities, BOOL receiving)
{
	UINT16 type;
	UINT16 length;
	BYTE *bm, *em;

	while (numberCapabilities > 0)
	{
		Stream_GetPointer(s, bm);

		rdp_read_capability_set_header(s, &length, &type);

		fprintf(stderr, "%s ", receiving ? "Receiving" : "Sending");

		em = bm + length;

		if (Stream_GetRemainingLength(s) < (size_t) (length - 4))
		{
			fprintf(stderr, "error processing stream\n");
			return FALSE;
		}

		switch (type)
		{
			case CAPSET_TYPE_GENERAL:
				if (!rdp_print_general_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP:
				if (!rdp_print_bitmap_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_ORDER:
				if (!rdp_print_order_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CACHE:
				if (!rdp_print_bitmap_cache_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_CONTROL:
				if (!rdp_print_control_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_ACTIVATION:
				if (!rdp_print_window_activation_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_POINTER:
				if (!rdp_print_pointer_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_SHARE:
				if (!rdp_print_share_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_COLOR_CACHE:
				if (!rdp_print_color_cache_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_SOUND:
				if (!rdp_print_sound_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_INPUT:
				if (!rdp_print_input_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_FONT:
				if (!rdp_print_font_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_BRUSH:
				if (!rdp_print_brush_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_GLYPH_CACHE:
				if (!rdp_print_glyph_cache_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_OFFSCREEN_CACHE:
				if (!rdp_print_offscreen_bitmap_cache_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT:
				if (!rdp_print_bitmap_cache_host_support_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CACHE_V2:
				if (!rdp_print_bitmap_cache_v2_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_VIRTUAL_CHANNEL:
				if (!rdp_print_virtual_channel_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_DRAW_NINE_GRID_CACHE:
				if (!rdp_print_draw_nine_grid_cache_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_DRAW_GDI_PLUS:
				if (!rdp_print_draw_gdiplus_cache_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_RAIL:
				if (!rdp_print_remote_programs_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_WINDOW:
				if (!rdp_print_window_list_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_COMP_DESK:
				if (!rdp_print_desktop_composition_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_MULTI_FRAGMENT_UPDATE:
				if (!rdp_print_multifragment_update_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_LARGE_POINTER:
				if (!rdp_print_large_pointer_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_SURFACE_COMMANDS:
				if (!rdp_print_surface_commands_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CODECS:
				if (!rdp_print_bitmap_codecs_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_FRAME_ACKNOWLEDGE:
				if (!rdp_print_frame_acknowledge_capability_set(s, length))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID:
				if (!rdp_print_bitmap_cache_v3_codec_id_capability_set(s, length))
					return FALSE;
				break;

			default:
				fprintf(stderr, "unknown capability type %d\n", type);
				break;
		}

		if (s->pointer != em)
		{
			fprintf(stderr, "incorrect offset, type:0x%02X actual:%d expected:%d\n",
				type, (int) (s->pointer - bm), (int) (em - bm));
		}

		Stream_SetPointer(s, em);
		numberCapabilities--;
	}

	return TRUE;
}

BOOL rdp_read_capability_sets(wStream* s, rdpSettings* settings, UINT16 numberCapabilities)
{
	BYTE* mark;
	UINT16 count;
	UINT16 type;
	UINT16 length;
	BYTE *bm, *em;

	Stream_GetPointer(s, mark);
	count = numberCapabilities;

	while (numberCapabilities > 0 && Stream_GetRemainingLength(s) >= 4)
	{
		Stream_GetPointer(s, bm);

		rdp_read_capability_set_header(s, &length, &type);

		if (type < 32)
		{
			settings->ReceivedCapabilities[type] = TRUE;
		}
		else
		{
			fprintf(stderr, "%s: not handling capability type %d yet\n", __FUNCTION__, type);
		}

		em = bm + length;

		if (Stream_GetRemainingLength(s) < ((size_t) length - 4))
		{
			fprintf(stderr, "error processing stream\n");
			return FALSE;
		}

		switch (type)
		{
			case CAPSET_TYPE_GENERAL:
				if (!rdp_read_general_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP:
				if (!rdp_read_bitmap_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_ORDER:
				if (!rdp_read_order_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CACHE:
				if (!rdp_read_bitmap_cache_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_CONTROL:
				if (!rdp_read_control_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_ACTIVATION:
				if (!rdp_read_window_activation_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_POINTER:
				if (!rdp_read_pointer_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_SHARE:
				if (!rdp_read_share_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_COLOR_CACHE:
				if (!rdp_read_color_cache_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_SOUND:
				if (!rdp_read_sound_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_INPUT:
				if (!rdp_read_input_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_FONT:
				if (!rdp_read_font_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_BRUSH:
				if (!rdp_read_brush_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_GLYPH_CACHE:
				if (!rdp_read_glyph_cache_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_OFFSCREEN_CACHE:
				if (!rdp_read_offscreen_bitmap_cache_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT:
				if (!rdp_read_bitmap_cache_host_support_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CACHE_V2:
				if (!rdp_read_bitmap_cache_v2_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_VIRTUAL_CHANNEL:
				if (!rdp_read_virtual_channel_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_DRAW_NINE_GRID_CACHE:
				if (!rdp_read_draw_nine_grid_cache_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_DRAW_GDI_PLUS:
				if (!rdp_read_draw_gdiplus_cache_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_RAIL:
				if (!rdp_read_remote_programs_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_WINDOW:
				if (!rdp_read_window_list_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_COMP_DESK:
				if (!rdp_read_desktop_composition_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_MULTI_FRAGMENT_UPDATE:
				if (!rdp_read_multifragment_update_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_LARGE_POINTER:
				if (!rdp_read_large_pointer_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_SURFACE_COMMANDS:
				if (!rdp_read_surface_commands_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CODECS:
				if (!rdp_read_bitmap_codecs_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_FRAME_ACKNOWLEDGE:
				if (!rdp_read_frame_acknowledge_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID:
				if (!rdp_read_bitmap_cache_v3_codec_id_capability_set(s, length, settings))
					return FALSE;
				break;

			default:
				fprintf(stderr, "unknown capability type %d\n", type);
				break;
		}

		if (s->pointer != em)
		{
			fprintf(stderr, "incorrect offset, type:0x%02X actual:%d expected:%d\n",
				type, (int) (s->pointer - bm), (int) (em - bm));
		}

		Stream_SetPointer(s, em);
		numberCapabilities--;
	}

	if (numberCapabilities)
	{
		fprintf(stderr, "%s: strange we haven't read the number of announced capacity sets, read=%d expected=%d\n",
				__FUNCTION__, count-numberCapabilities, count);
	}

#ifdef WITH_DEBUG_CAPABILITIES
	Stream_GetPointer(s, em);
	Stream_SetPointer(s, mark);
	numberCapabilities = count;
	rdp_print_capability_sets(s, numberCapabilities, TRUE);
	Stream_SetPointer(s, em);
#endif

	return TRUE;
}

BOOL rdp_recv_get_active_header(rdpRdp* rdp, wStream* s, UINT16* pChannelId)
{
	UINT16 length;
	UINT16 securityFlags;

	if (!rdp_read_header(rdp, s, &length, pChannelId))
		return FALSE;

	if (rdp->disconnect)
		return TRUE;

	if (rdp->settings->DisableEncryption)
	{
		if (!rdp_read_security_header(s, &securityFlags))
			return FALSE;

		if (securityFlags & SEC_ENCRYPT)
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				fprintf(stderr, "rdp_decrypt failed\n");
				return FALSE;
			}
		}
	}

	if (*pChannelId != MCS_GLOBAL_CHANNEL_ID)
	{
		UINT16 mcsMessageChannelId = rdp->mcs->messageChannelId;

		if ((mcsMessageChannelId == 0) || (*pChannelId != mcsMessageChannelId))
		{
			fprintf(stderr, "unexpected MCS channel id %04x received\n", *pChannelId);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL rdp_recv_demand_active(rdpRdp* rdp, wStream* s)
{
	UINT16 channelId;
	UINT16 pduType;
	UINT16 pduLength;
	UINT16 pduSource;
	UINT16 numberCapabilities;
	UINT16 lengthSourceDescriptor;
	UINT16 lengthCombinedCapabilities;

	if (!rdp_recv_get_active_header(rdp, s, &channelId))
		return FALSE;

	if (rdp->disconnect)
		return TRUE;

	if (!rdp_read_share_control_header(s, &pduLength, &pduType, &pduSource))
	{
		fprintf(stderr, "rdp_read_share_control_header failed\n");
		return FALSE;
	}

	if (pduType != PDU_TYPE_DEMAND_ACTIVE)
	{
		if (pduType != PDU_TYPE_SERVER_REDIRECTION)
			fprintf(stderr, "expected PDU_TYPE_DEMAND_ACTIVE %04x, got %04x\n", PDU_TYPE_DEMAND_ACTIVE, pduType);

		return FALSE;
	}

	rdp->settings->PduSource = pduSource;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, rdp->settings->ShareId); /* shareId (4 bytes) */
	Stream_Read_UINT16(s, lengthSourceDescriptor); /* lengthSourceDescriptor (2 bytes) */
	Stream_Read_UINT16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */

	if (!Stream_SafeSeek(s, lengthSourceDescriptor) || Stream_GetRemainingLength(s) < 4) /* sourceDescriptor */
		return FALSE;

	Stream_Read_UINT16(s, numberCapabilities); /* numberCapabilities (2 bytes) */
	Stream_Seek(s, 2); /* pad2Octets (2 bytes) */

	/* capabilitySets */
	if (!rdp_read_capability_sets(s, rdp->settings, numberCapabilities))
	{
		fprintf(stderr, "rdp_read_capability_sets failed\n");
		return FALSE;
	}

	rdp->update->secondary->glyph_v2 = (rdp->settings->GlyphSupportLevel > GLYPH_SUPPORT_FULL) ? TRUE : FALSE;

	return TRUE;
}

void rdp_write_demand_active(wStream* s, rdpSettings* settings)
{
	int bm, em, lm;
	UINT16 numberCapabilities;
	UINT16 lengthCombinedCapabilities;

	Stream_EnsureRemainingCapacity(s, 64);

	Stream_Write_UINT32(s, settings->ShareId); /* shareId (4 bytes) */
	Stream_Write_UINT16(s, 4); /* lengthSourceDescriptor (2 bytes) */

	lm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s); /* lengthCombinedCapabilities (2 bytes) */
	Stream_Write(s, "RDP", 4); /* sourceDescriptor */

	bm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s); /* numberCapabilities (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	numberCapabilities = 14;
	rdp_write_general_capability_set(s, settings);
	rdp_write_bitmap_capability_set(s, settings);
	rdp_write_order_capability_set(s, settings);
	rdp_write_pointer_capability_set(s, settings);
	rdp_write_input_capability_set(s, settings);
	rdp_write_virtual_channel_capability_set(s, settings);
	rdp_write_share_capability_set(s, settings);
	rdp_write_font_capability_set(s, settings);
	rdp_write_multifragment_update_capability_set(s, settings);
	rdp_write_large_pointer_capability_set(s, settings);
	rdp_write_desktop_composition_capability_set(s, settings);
	rdp_write_surface_commands_capability_set(s, settings);
	rdp_write_bitmap_codecs_capability_set(s, settings);
	rdp_write_frame_acknowledge_capability_set(s, settings);

	if (settings->BitmapCachePersistEnabled)
	{
		numberCapabilities++;
		rdp_write_bitmap_cache_host_support_capability_set(s, settings);
	}

	em = Stream_GetPosition(s);

	Stream_SetPosition(s, lm); /* go back to lengthCombinedCapabilities */
	lengthCombinedCapabilities = (em - bm);
	Stream_Write_UINT16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */

	Stream_SetPosition(s, bm); /* go back to numberCapabilities */
	Stream_Write_UINT16(s, numberCapabilities); /* numberCapabilities (2 bytes) */

#ifdef WITH_DEBUG_CAPABILITIES
	Stream_Seek_UINT16(s);
	rdp_print_capability_sets(s, numberCapabilities, FALSE);
	Stream_SetPosition(s, bm);
	Stream_Seek_UINT16(s);
#endif

	Stream_SetPosition(s, em);

	Stream_Write_UINT32(s, 0); /* sessionId */
}

BOOL rdp_send_demand_active(rdpRdp* rdp)
{
	wStream* s;
	BOOL status;

	s = Stream_New(NULL, 4096);
	rdp_init_stream_pdu(rdp, s);

	rdp->settings->ShareId = 0x10000 + rdp->mcs->userId;

	rdp_write_demand_active(s, rdp->settings);

	status = rdp_send_pdu(rdp, s, PDU_TYPE_DEMAND_ACTIVE, rdp->mcs->userId);

	Stream_Free(s, TRUE);

	return status;
}

BOOL rdp_recv_confirm_active(rdpRdp* rdp, wStream* s)
{
	BOOL status;
	rdpSettings* settings;
	UINT16 lengthSourceDescriptor;
	UINT16 lengthCombinedCapabilities;
	UINT16 numberCapabilities;

	settings = rdp->settings;

	if (Stream_GetRemainingLength(s) < 10)
		return FALSE;

	Stream_Seek_UINT32(s); /* shareId (4 bytes) */
	Stream_Seek_UINT16(s); /* originatorId (2 bytes) */
	Stream_Read_UINT16(s, lengthSourceDescriptor); /* lengthSourceDescriptor (2 bytes) */
	Stream_Read_UINT16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */

	if (((int) Stream_GetRemainingLength(s)) < lengthSourceDescriptor + 4)
		return FALSE;

	Stream_Seek(s, lengthSourceDescriptor); /* sourceDescriptor */
	Stream_Read_UINT16(s, numberCapabilities); /* numberCapabilities (2 bytes) */
	Stream_Seek(s, 2); /* pad2Octets (2 bytes) */

	status = rdp_read_capability_sets(s, rdp->settings, numberCapabilities);

	if (!settings->ReceivedCapabilities[CAPSET_TYPE_SURFACE_COMMANDS])
	{
		/* client does not support surface commands */
		settings->SurfaceCommandsEnabled = FALSE;
		settings->SurfaceFrameMarkerEnabled = FALSE;
	}

	if (!settings->ReceivedCapabilities[CAPSET_TYPE_FRAME_ACKNOWLEDGE])
	{
		/* client does not support frame acks */
		settings->FrameAcknowledge = 0;
	}

	if (!settings->ReceivedCapabilities[CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID])
	{
		/* client does not support bitmap cache v3 */
		settings->BitmapCacheV3Enabled = FALSE;
	}

	if (!settings->ReceivedCapabilities[CAPSET_TYPE_BITMAP_CODECS])
	{
		/* client does not support bitmap codecs */

		settings->RemoteFxCodec = FALSE;
		settings->NSCodec = FALSE;
		settings->JpegCodec = FALSE;
	}

	if (!settings->ReceivedCapabilities[CAPSET_TYPE_MULTI_FRAGMENT_UPDATE])
	{
		/* client does not support multi fragment updates */
	}

	if (!settings->ReceivedCapabilities[CAPSET_TYPE_LARGE_POINTER])
	{
		/* client does not support large pointers */
		settings->LargePointerFlag = 0;
	}

	return status;
}

void rdp_write_confirm_active(wStream* s, rdpSettings* settings)
{
	int bm, em, lm;
	UINT16 numberCapabilities;
	UINT16 lengthSourceDescriptor;
	UINT16 lengthCombinedCapabilities;

	lengthSourceDescriptor = sizeof(SOURCE_DESCRIPTOR);

	Stream_Write_UINT32(s, settings->ShareId); /* shareId (4 bytes) */
	Stream_Write_UINT16(s, 0x03EA); /* originatorId (2 bytes) */
	Stream_Write_UINT16(s, lengthSourceDescriptor);/* lengthSourceDescriptor (2 bytes) */

	lm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s); /* lengthCombinedCapabilities (2 bytes) */
	Stream_Write(s, SOURCE_DESCRIPTOR, lengthSourceDescriptor); /* sourceDescriptor */

	bm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s); /* numberCapabilities (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	/* Capability Sets */
	numberCapabilities = 15;
	rdp_write_general_capability_set(s, settings);
	rdp_write_bitmap_capability_set(s, settings);
	rdp_write_order_capability_set(s, settings);

	if (settings->RdpVersion >= 5)
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

	if (settings->OffscreenSupportLevel)
	{
		numberCapabilities++;
		rdp_write_offscreen_bitmap_cache_capability_set(s, settings);
	}

	if (settings->DrawNineGridEnabled)
	{
		numberCapabilities++;
		rdp_write_draw_nine_grid_cache_capability_set(s, settings);
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_LARGE_POINTER])
	{
		if (settings->LargePointerFlag)
		{
			numberCapabilities++;
			rdp_write_large_pointer_capability_set(s, settings);
		}
	}

	if (settings->RemoteApplicationMode)
	{
		numberCapabilities += 2;
		rdp_write_remote_programs_capability_set(s, settings);
		rdp_write_window_list_capability_set(s, settings);
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_MULTI_FRAGMENT_UPDATE])
	{
		numberCapabilities++;
		rdp_write_multifragment_update_capability_set(s, settings);
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_SURFACE_COMMANDS])
	{
		numberCapabilities++;
		rdp_write_surface_commands_capability_set(s, settings);
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_BITMAP_CODECS])
	{
		numberCapabilities++;
		rdp_write_bitmap_codecs_capability_set(s, settings);
	}

	if (!settings->ReceivedCapabilities[CAPSET_TYPE_FRAME_ACKNOWLEDGE])
		settings->FrameAcknowledge = 0;

	if (settings->FrameAcknowledge)
	{
		numberCapabilities++;
		rdp_write_frame_acknowledge_capability_set(s, settings);
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID])
	{
		if (settings->BitmapCacheV3CodecId != 0)
		{
			numberCapabilities++;
			rdp_write_bitmap_cache_v3_codec_id_capability_set(s, settings);
		}
	}

	em = Stream_GetPosition(s);

	Stream_SetPosition(s, lm); /* go back to lengthCombinedCapabilities */
	lengthCombinedCapabilities = (em - bm);
	Stream_Write_UINT16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */

	Stream_SetPosition(s, bm); /* go back to numberCapabilities */
	Stream_Write_UINT16(s, numberCapabilities); /* numberCapabilities (2 bytes) */

#ifdef WITH_DEBUG_CAPABILITIES
	Stream_Seek_UINT16(s);
	rdp_print_capability_sets(s, numberCapabilities, FALSE);
	Stream_SetPosition(s, bm);
	Stream_Seek_UINT16(s);
#endif

	Stream_SetPosition(s, em);
}

BOOL rdp_send_confirm_active(rdpRdp* rdp)
{
	wStream* s;
	BOOL status;

	s = Stream_New(NULL, 4096);
	rdp_init_stream_pdu(rdp, s);

	rdp_write_confirm_active(s, rdp->settings);

	status = rdp_send_pdu(rdp, s, PDU_TYPE_CONFIRM_ACTIVE, rdp->mcs->userId);

	Stream_Free(s, TRUE);

	return status;
}

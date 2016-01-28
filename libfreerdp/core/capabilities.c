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
#include "fastpath.h"

#include <winpr/crt.h>
#include <winpr/rpc.h>

#include <freerdp/log.h>

#define TAG FREERDP_TAG("core.capabilities")

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

static const char *get_capability_name(UINT16 type)
{
	if (type > CAPSET_TYPE_FRAME_ACKNOWLEDGE)
		return "<unknown>";

	return CAPSET_TYPE_STRINGS[type];
}

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

	if (!(extraFlags & ENC_SALTED_CHECKSUM))
		settings->SaltedChecksum = FALSE;


	if (!settings->ServerMode)
	{
		/**
		 * Note: refreshRectSupport and suppressOutputSupport are
		 * server-only flags indicating to the client weather the
		 * respective PDUs are supported. See MS-RDPBCGR 2.2.7.1.1
		 */
		if (!refreshRectSupport)
			settings->RefreshRect = FALSE;

		if (!suppressOutputSupport)
			settings->SuppressOutput = FALSE;
	}

	return TRUE;
}

/**
 * Write general capability set.\n
 * @msdn{cc240549}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_write_general_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 extraFlags;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

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
	return TRUE;
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

	WLog_INFO(TAG,  "GeneralCapabilitySet (length %d):", length);
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
	WLog_INFO(TAG,  "\tosMajorType: 0x%04X", osMajorType);
	WLog_INFO(TAG,  "\tosMinorType: 0x%04X", osMinorType);
	WLog_INFO(TAG,  "\tprotocolVersion: 0x%04X", protocolVersion);
	WLog_INFO(TAG,  "\tpad2OctetsA: 0x%04X", pad2OctetsA);
	WLog_INFO(TAG,  "\tgeneralCompressionTypes: 0x%04X", generalCompressionTypes);
	WLog_INFO(TAG,  "\textraFlags: 0x%04X", extraFlags);
	WLog_INFO(TAG,  "\tupdateCapabilityFlag: 0x%04X", updateCapabilityFlag);
	WLog_INFO(TAG,  "\tremoteUnshareFlag: 0x%04X", remoteUnshareFlag);
	WLog_INFO(TAG,  "\tgeneralCompressionLevel: 0x%04X", generalCompressionLevel);
	WLog_INFO(TAG,  "\trefreshRectSupport: 0x%02X", refreshRectSupport);
	WLog_INFO(TAG,  "\tsuppressOutputSupport: 0x%02X", suppressOutputSupport);
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

	if (settings->DrawAllowSkipAlpha)
		settings->DrawAllowSkipAlpha = (drawingFlags & DRAW_ALLOW_SKIP_ALPHA) ? TRUE : FALSE;

	if (settings->DrawAllowDynamicColorFidelity)
		settings->DrawAllowDynamicColorFidelity = (drawingFlags & DRAW_ALLOW_DYNAMIC_COLOR_FIDELITY) ? TRUE : FALSE;

	if (settings->DrawAllowColorSubsampling)
		settings->DrawAllowColorSubsampling = (drawingFlags & DRAW_ALLOW_COLOR_SUBSAMPLING) ? TRUE : FALSE;

	return TRUE;
}

/**
 * Write bitmap capability set.\n
 * @msdn{cc240554}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_write_bitmap_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	BYTE drawingFlags = 0;
	UINT16 preferredBitsPerPixel;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	header = rdp_capability_set_start(s);

	if (settings->DrawAllowSkipAlpha)
		drawingFlags |= DRAW_ALLOW_SKIP_ALPHA;

	if (settings->DrawAllowDynamicColorFidelity)
		drawingFlags |= DRAW_ALLOW_DYNAMIC_COLOR_FIDELITY;

	if (settings->DrawAllowColorSubsampling)
		drawingFlags |= DRAW_ALLOW_COLOR_SUBSAMPLING; /* currently unimplemented */

	/* While bitmap_decode.c now implements YCoCg, in turning it
	 * on we have found Microsoft is inconsistent on whether to invert R & B.
	 * And it's not only from one server to another; on Win7/2008R2, it appears
	 * to send the main content with a different inversion than the Windows
	 * button!  So... don't advertise that we support YCoCg and the server
	 * will not send it.  YCoCg is still needed for EGFX, but it at least
	 * appears consistent in its use.
	 */

	if (settings->RdpVersion > 5)
		preferredBitsPerPixel = settings->ColorDepth;
	else
		preferredBitsPerPixel = 8;


	Stream_Write_UINT16(s, preferredBitsPerPixel); /* preferredBitsPerPixel (2 bytes) */
	Stream_Write_UINT16(s, 1); /* receive1BitPerPixel (2 bytes) */
	Stream_Write_UINT16(s, 1); /* receive4BitsPerPixel (2 bytes) */
	Stream_Write_UINT16(s, 1); /* receive8BitsPerPixel (2 bytes) */
	Stream_Write_UINT16(s, settings->DesktopWidth); /* desktopWidth (2 bytes) */
	Stream_Write_UINT16(s, settings->DesktopHeight); /* desktopHeight (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */
	Stream_Write_UINT16(s, settings->DesktopResize); /* desktopResizeFlag (2 bytes) */
	Stream_Write_UINT16(s, 1); /* bitmapCompressionFlag (2 bytes) */
	Stream_Write_UINT8(s, 0); /* highColorFlags (1 byte) */
	Stream_Write_UINT8(s, drawingFlags); /* drawingFlags (1 byte) */
	Stream_Write_UINT16(s, 1); /* multipleRectangleSupport (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsB (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP);
	return TRUE;
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
	WLog_INFO(TAG,  "BitmapCapabilitySet (length %d):", length);

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
	WLog_INFO(TAG,  "\tpreferredBitsPerPixel: 0x%04X", preferredBitsPerPixel);
	WLog_INFO(TAG,  "\treceive1BitPerPixel: 0x%04X", receive1BitPerPixel);
	WLog_INFO(TAG,  "\treceive4BitsPerPixel: 0x%04X", receive4BitsPerPixel);
	WLog_INFO(TAG,  "\treceive8BitsPerPixel: 0x%04X", receive8BitsPerPixel);
	WLog_INFO(TAG,  "\tdesktopWidth: 0x%04X", desktopWidth);
	WLog_INFO(TAG,  "\tdesktopHeight: 0x%04X", desktopHeight);
	WLog_INFO(TAG,  "\tpad2Octets: 0x%04X", pad2Octets);
	WLog_INFO(TAG,  "\tdesktopResizeFlag: 0x%04X", desktopResizeFlag);
	WLog_INFO(TAG,  "\tbitmapCompressionFlag: 0x%04X", bitmapCompressionFlag);
	WLog_INFO(TAG,  "\thighColorFlags: 0x%02X", highColorFlags);
	WLog_INFO(TAG,  "\tdrawingFlags: 0x%02X", drawingFlags);
	WLog_INFO(TAG,  "\tmultipleRectangleSupport: 0x%04X", multipleRectangleSupport);
	WLog_INFO(TAG,  "\tpad2OctetsB: 0x%04X", pad2OctetsB);
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

static BOOL rdp_write_order_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 orderFlags;
	UINT16 orderSupportExFlags;
	UINT16 textANSICodePage = 0;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	header = rdp_capability_set_start(s);

	/* see [MSDN-CP]: http://msdn.microsoft.com/en-us/library/dd317756 */
	if (!settings->ServerMode)
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
	Stream_Write_UINT16(s, textANSICodePage); /* textANSICodePage (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsE (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_ORDER);
	return TRUE;
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
	WLog_INFO(TAG,  "OrderCapabilitySet (length %d):", length);

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
	WLog_INFO(TAG,  "\tpad4OctetsA: 0x%08X", pad4OctetsA);
	WLog_INFO(TAG,  "\tdesktopSaveXGranularity: 0x%04X", desktopSaveXGranularity);
	WLog_INFO(TAG,  "\tdesktopSaveYGranularity: 0x%04X", desktopSaveYGranularity);
	WLog_INFO(TAG,  "\tpad2OctetsA: 0x%04X", pad2OctetsA);
	WLog_INFO(TAG,  "\tmaximumOrderLevel: 0x%04X", maximumOrderLevel);
	WLog_INFO(TAG,  "\tnumberFonts: 0x%04X", numberFonts);
	WLog_INFO(TAG,  "\torderFlags: 0x%04X", orderFlags);
	WLog_INFO(TAG,  "\torderSupport:");
	WLog_INFO(TAG,  "\t\tDSTBLT: %d", orderSupport[NEG_DSTBLT_INDEX]);
	WLog_INFO(TAG,  "\t\tPATBLT: %d", orderSupport[NEG_PATBLT_INDEX]);
	WLog_INFO(TAG,  "\t\tSCRBLT: %d", orderSupport[NEG_SCRBLT_INDEX]);
	WLog_INFO(TAG,  "\t\tMEMBLT: %d", orderSupport[NEG_MEMBLT_INDEX]);
	WLog_INFO(TAG,  "\t\tMEM3BLT: %d", orderSupport[NEG_MEM3BLT_INDEX]);
	WLog_INFO(TAG,  "\t\tATEXTOUT: %d", orderSupport[NEG_ATEXTOUT_INDEX]);
	WLog_INFO(TAG,  "\t\tAEXTTEXTOUT: %d", orderSupport[NEG_AEXTTEXTOUT_INDEX]);
	WLog_INFO(TAG,  "\t\tDRAWNINEGRID: %d", orderSupport[NEG_DRAWNINEGRID_INDEX]);
	WLog_INFO(TAG,  "\t\tLINETO: %d", orderSupport[NEG_LINETO_INDEX]);
	WLog_INFO(TAG,  "\t\tMULTI_DRAWNINEGRID: %d", orderSupport[NEG_MULTI_DRAWNINEGRID_INDEX]);
	WLog_INFO(TAG,  "\t\tOPAQUE_RECT: %d", orderSupport[NEG_OPAQUE_RECT_INDEX]);
	WLog_INFO(TAG,  "\t\tSAVEBITMAP: %d", orderSupport[NEG_SAVEBITMAP_INDEX]);
	WLog_INFO(TAG,  "\t\tWTEXTOUT: %d", orderSupport[NEG_WTEXTOUT_INDEX]);
	WLog_INFO(TAG,  "\t\tMEMBLT_V2: %d", orderSupport[NEG_MEMBLT_V2_INDEX]);
	WLog_INFO(TAG,  "\t\tMEM3BLT_V2: %d", orderSupport[NEG_MEM3BLT_V2_INDEX]);
	WLog_INFO(TAG,  "\t\tMULTIDSTBLT: %d", orderSupport[NEG_MULTIDSTBLT_INDEX]);
	WLog_INFO(TAG,  "\t\tMULTIPATBLT: %d", orderSupport[NEG_MULTIPATBLT_INDEX]);
	WLog_INFO(TAG,  "\t\tMULTISCRBLT: %d", orderSupport[NEG_MULTISCRBLT_INDEX]);
	WLog_INFO(TAG,  "\t\tMULTIOPAQUERECT: %d", orderSupport[NEG_MULTIOPAQUERECT_INDEX]);
	WLog_INFO(TAG,  "\t\tFAST_INDEX: %d", orderSupport[NEG_FAST_INDEX_INDEX]);
	WLog_INFO(TAG,  "\t\tPOLYGON_SC: %d", orderSupport[NEG_POLYGON_SC_INDEX]);
	WLog_INFO(TAG,  "\t\tPOLYGON_CB: %d", orderSupport[NEG_POLYGON_CB_INDEX]);
	WLog_INFO(TAG,  "\t\tPOLYLINE: %d", orderSupport[NEG_POLYLINE_INDEX]);
	WLog_INFO(TAG,  "\t\tUNUSED23: %d", orderSupport[NEG_UNUSED23_INDEX]);
	WLog_INFO(TAG,  "\t\tFAST_GLYPH: %d", orderSupport[NEG_FAST_GLYPH_INDEX]);
	WLog_INFO(TAG,  "\t\tELLIPSE_SC: %d", orderSupport[NEG_ELLIPSE_SC_INDEX]);
	WLog_INFO(TAG,  "\t\tELLIPSE_CB: %d", orderSupport[NEG_ELLIPSE_CB_INDEX]);
	WLog_INFO(TAG,  "\t\tGLYPH_INDEX: %d", orderSupport[NEG_GLYPH_INDEX_INDEX]);
	WLog_INFO(TAG,  "\t\tGLYPH_WEXTTEXTOUT: %d", orderSupport[NEG_GLYPH_WEXTTEXTOUT_INDEX]);
	WLog_INFO(TAG,  "\t\tGLYPH_WLONGTEXTOUT: %d", orderSupport[NEG_GLYPH_WLONGTEXTOUT_INDEX]);
	WLog_INFO(TAG,  "\t\tGLYPH_WLONGEXTTEXTOUT: %d", orderSupport[NEG_GLYPH_WLONGEXTTEXTOUT_INDEX]);
	WLog_INFO(TAG,  "\t\tUNUSED31: %d", orderSupport[NEG_UNUSED31_INDEX]);
	WLog_INFO(TAG,  "\ttextFlags: 0x%04X", textFlags);
	WLog_INFO(TAG,  "\torderSupportExFlags: 0x%04X", orderSupportExFlags);
	WLog_INFO(TAG,  "\tpad4OctetsB: 0x%08X", pad4OctetsB);
	WLog_INFO(TAG,  "\tdesktopSaveSize: 0x%08X", desktopSaveSize);
	WLog_INFO(TAG,  "\tpad2OctetsC: 0x%04X", pad2OctetsC);
	WLog_INFO(TAG,  "\tpad2OctetsD: 0x%04X", pad2OctetsD);
	WLog_INFO(TAG,  "\ttextANSICodePage: 0x%04X", textANSICodePage);
	WLog_INFO(TAG,  "\tpad2OctetsE: 0x%04X", pad2OctetsE);
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

BOOL rdp_write_bitmap_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int bpp;
	int header;
	UINT16 size;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

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
	return TRUE;
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
	WLog_INFO(TAG,  "BitmapCacheCapabilitySet (length %d):", length);

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
	WLog_INFO(TAG,  "\tpad1: 0x%08X", pad1);
	WLog_INFO(TAG,  "\tpad2: 0x%08X", pad2);
	WLog_INFO(TAG,  "\tpad3: 0x%08X", pad3);
	WLog_INFO(TAG,  "\tpad4: 0x%08X", pad4);
	WLog_INFO(TAG,  "\tpad5: 0x%08X", pad5);
	WLog_INFO(TAG,  "\tpad6: 0x%08X", pad6);
	WLog_INFO(TAG,  "\tCache0Entries: 0x%04X", Cache0Entries);
	WLog_INFO(TAG,  "\tCache0MaximumCellSize: 0x%04X", Cache0MaximumCellSize);
	WLog_INFO(TAG,  "\tCache1Entries: 0x%04X", Cache1Entries);
	WLog_INFO(TAG,  "\tCache1MaximumCellSize: 0x%04X", Cache1MaximumCellSize);
	WLog_INFO(TAG,  "\tCache2Entries: 0x%04X", Cache2Entries);
	WLog_INFO(TAG,  "\tCache2MaximumCellSize: 0x%04X", Cache2MaximumCellSize);
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

BOOL rdp_write_control_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	Stream_Write_UINT16(s, 0); /* controlFlags (2 bytes) */
	Stream_Write_UINT16(s, 0); /* remoteDetachFlag (2 bytes) */
	Stream_Write_UINT16(s, 2); /* controlInterest (2 bytes) */
	Stream_Write_UINT16(s, 2); /* detachInterest (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_CONTROL);
	return TRUE;
}

BOOL rdp_print_control_capability_set(wStream* s, UINT16 length)
{
	UINT16 controlFlags;
	UINT16 remoteDetachFlag;
	UINT16 controlInterest;
	UINT16 detachInterest;
	WLog_INFO(TAG,  "ControlCapabilitySet (length %d):", length);

	if (length < 12)
		return FALSE;

	Stream_Read_UINT16(s, controlFlags); /* controlFlags (2 bytes) */
	Stream_Read_UINT16(s, remoteDetachFlag); /* remoteDetachFlag (2 bytes) */
	Stream_Read_UINT16(s, controlInterest); /* controlInterest (2 bytes) */
	Stream_Read_UINT16(s, detachInterest); /* detachInterest (2 bytes) */
	WLog_INFO(TAG,  "\tcontrolFlags: 0x%04X", controlFlags);
	WLog_INFO(TAG,  "\tremoteDetachFlag: 0x%04X", remoteDetachFlag);
	WLog_INFO(TAG,  "\tcontrolInterest: 0x%04X", controlInterest);
	WLog_INFO(TAG,  "\tdetachInterest: 0x%04X", detachInterest);
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

BOOL rdp_write_window_activation_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	Stream_Write_UINT16(s, 0); /* helpKeyFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* helpKeyIndexFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* helpExtendedKeyFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* windowManagerKeyFlag (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_ACTIVATION);
	return TRUE;
}

BOOL rdp_print_window_activation_capability_set(wStream* s, UINT16 length)
{
	UINT16 helpKeyFlag;
	UINT16 helpKeyIndexFlag;
	UINT16 helpExtendedKeyFlag;
	UINT16 windowManagerKeyFlag;
	WLog_INFO(TAG,  "WindowActivationCapabilitySet (length %d):", length);

	if (length < 12)
		return FALSE;

	Stream_Read_UINT16(s, helpKeyFlag); /* helpKeyFlag (2 bytes) */
	Stream_Read_UINT16(s, helpKeyIndexFlag); /* helpKeyIndexFlag (2 bytes) */
	Stream_Read_UINT16(s, helpExtendedKeyFlag); /* helpExtendedKeyFlag (2 bytes) */
	Stream_Read_UINT16(s, windowManagerKeyFlag); /* windowManagerKeyFlag (2 bytes) */
	WLog_INFO(TAG,  "\thelpKeyFlag: 0x%04X", helpKeyFlag);
	WLog_INFO(TAG,  "\thelpKeyIndexFlag: 0x%04X", helpKeyIndexFlag);
	WLog_INFO(TAG,  "\thelpExtendedKeyFlag: 0x%04X", helpExtendedKeyFlag);
	WLog_INFO(TAG,  "\twindowManagerKeyFlag: 0x%04X", windowManagerKeyFlag);
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

	if (length < 8)
		return FALSE;

	Stream_Read_UINT16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	Stream_Read_UINT16(s, colorPointerCacheSize); /* colorPointerCacheSize (2 bytes) */

	/* pointerCacheSize is optional */
	if (length >= 10)
		Stream_Read_UINT16(s, pointerCacheSize); /* pointerCacheSize (2 bytes) */
	else
		pointerCacheSize = 0;

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

BOOL rdp_write_pointer_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 colorPointerFlag;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	colorPointerFlag = (settings->ColorPointerFlag) ? 1 : 0;

	Stream_Write_UINT16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	Stream_Write_UINT16(s, settings->PointerCacheSize); /* colorPointerCacheSize (2 bytes) */

	if (settings->LargePointerFlag)
	{
		Stream_Write_UINT16(s, settings->PointerCacheSize); /* pointerCacheSize (2 bytes) */
	}

	rdp_capability_set_finish(s, header, CAPSET_TYPE_POINTER);
	return TRUE;
}

BOOL rdp_print_pointer_capability_set(wStream* s, UINT16 length)
{
	UINT16 colorPointerFlag;
	UINT16 colorPointerCacheSize;
	UINT16 pointerCacheSize;

	if (length < 10)
		return FALSE;

	WLog_INFO(TAG,  "PointerCapabilitySet (length %d):", length);
	Stream_Read_UINT16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	Stream_Read_UINT16(s, colorPointerCacheSize); /* colorPointerCacheSize (2 bytes) */
	Stream_Read_UINT16(s, pointerCacheSize); /* pointerCacheSize (2 bytes) */
	WLog_INFO(TAG,  "\tcolorPointerFlag: 0x%04X", colorPointerFlag);
	WLog_INFO(TAG,  "\tcolorPointerCacheSize: 0x%04X", colorPointerCacheSize);
	WLog_INFO(TAG,  "\tpointerCacheSize: 0x%04X", pointerCacheSize);
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

BOOL rdp_write_share_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 nodeId;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	nodeId = (settings->ServerMode) ? 0x03EA : 0;

	Stream_Write_UINT16(s, nodeId); /* nodeId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SHARE);
	return TRUE;
}

BOOL rdp_print_share_capability_set(wStream* s, UINT16 length)
{
	UINT16 nodeId;
	UINT16 pad2Octets;
	WLog_INFO(TAG,  "ShareCapabilitySet (length %d):", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT16(s, nodeId); /* nodeId (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */
	WLog_INFO(TAG,  "\tnodeId: 0x%04X", nodeId);
	WLog_INFO(TAG,  "\tpad2Octets: 0x%04X", pad2Octets);
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

BOOL rdp_write_color_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	Stream_Write_UINT16(s, 6); /* colorTableCacheSize (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_COLOR_CACHE);
	return TRUE;
}

BOOL rdp_print_color_cache_capability_set(wStream* s, UINT16 length)
{
	UINT16 colorTableCacheSize;
	UINT16 pad2Octets;
	WLog_INFO(TAG,  "ColorCacheCapabilitySet (length %d):", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT16(s, colorTableCacheSize); /* colorTableCacheSize (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */
	WLog_INFO(TAG,  "\tcolorTableCacheSize: 0x%04X", colorTableCacheSize);
	WLog_INFO(TAG,  "\tpad2Octets: 0x%04X", pad2Octets);
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

BOOL rdp_write_sound_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 soundFlags;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	soundFlags = (settings->SoundBeepsEnabled) ? SOUND_BEEPS_FLAG : 0;

	Stream_Write_UINT16(s, soundFlags); /* soundFlags (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsA (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SOUND);
	return TRUE;
}

BOOL rdp_print_sound_capability_set(wStream* s, UINT16 length)
{
	UINT16 soundFlags;
	UINT16 pad2OctetsA;
	WLog_INFO(TAG,  "SoundCapabilitySet (length %d):", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT16(s, soundFlags); /* soundFlags (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA); /* pad2OctetsA (2 bytes) */
	WLog_INFO(TAG,  "\tsoundFlags: 0x%04X", soundFlags);
	WLog_INFO(TAG,  "\tpad2OctetsA: 0x%04X", pad2OctetsA);
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
		if (inputFlags & TS_INPUT_FLAG_MOUSE_HWHEEL) {
			settings->HasHorizontalWheel = TRUE;
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

BOOL rdp_write_input_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 inputFlags;

	if (!Stream_EnsureRemainingCapacity(s, 128))
		return FALSE;

	header = rdp_capability_set_start(s);

	inputFlags = INPUT_FLAG_SCANCODES | INPUT_FLAG_MOUSEX | INPUT_FLAG_UNICODE | TS_INPUT_FLAG_MOUSE_HWHEEL;

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
	return TRUE;
}

BOOL rdp_print_input_capability_set(wStream* s, UINT16 length)
{
	UINT16 inputFlags;
	UINT16 pad2OctetsA;
	UINT32 keyboardLayout;
	UINT32 keyboardType;
	UINT32 keyboardSubType;
	UINT32 keyboardFunctionKey;
	WLog_INFO(TAG,  "InputCapabilitySet (length %d)", length);

	if (length < 88)
		return FALSE;

	Stream_Read_UINT16(s, inputFlags); /* inputFlags (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA); /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT32(s, keyboardLayout); /* keyboardLayout (4 bytes) */
	Stream_Read_UINT32(s, keyboardType); /* keyboardType (4 bytes) */
	Stream_Read_UINT32(s, keyboardSubType); /* keyboardSubType (4 bytes) */
	Stream_Read_UINT32(s, keyboardFunctionKey); /* keyboardFunctionKeys (4 bytes) */
	Stream_Seek(s, 64); /* imeFileName (64 bytes) */
	WLog_INFO(TAG,  "\tinputFlags: 0x%04X", inputFlags);
	WLog_INFO(TAG,  "\tpad2OctetsA: 0x%04X", pad2OctetsA);
	WLog_INFO(TAG,  "\tkeyboardLayout: 0x%08X", keyboardLayout);
	WLog_INFO(TAG,  "\tkeyboardType: 0x%08X", keyboardType);
	WLog_INFO(TAG,  "\tkeyboardSubType: 0x%08X", keyboardSubType);
	WLog_INFO(TAG,  "\tkeyboardFunctionKey: 0x%08X", keyboardFunctionKey);
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

BOOL rdp_write_font_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	Stream_Write_UINT16(s, FONTSUPPORT_FONTLIST); /* fontSupportFlags (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_FONT);
	return TRUE;
}

BOOL rdp_print_font_capability_set(wStream* s, UINT16 length)
{
	UINT16 fontSupportFlags = 0;
	UINT16 pad2Octets = 0;
	WLog_INFO(TAG,  "FontCapabilitySet (length %d):", length);

	if (length > 4)
		Stream_Read_UINT16(s, fontSupportFlags); /* fontSupportFlags (2 bytes) */

	if (length > 6)
		Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */

	WLog_INFO(TAG,  "\tfontSupportFlags: 0x%04X", fontSupportFlags);
	WLog_INFO(TAG,  "\tpad2Octets: 0x%04X", pad2Octets);
	return TRUE;
}

/**
 * Read brush capability set.
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

BOOL rdp_write_brush_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	Stream_Write_UINT32(s, BRUSH_COLOR_FULL); /* brushSupportLevel (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BRUSH);
	return TRUE;
}

BOOL rdp_print_brush_capability_set(wStream* s, UINT16 length)
{
	UINT32 brushSupportLevel;
	WLog_INFO(TAG,  "BrushCapabilitySet (length %d):", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, brushSupportLevel); /* brushSupportLevel (4 bytes) */
	WLog_INFO(TAG,  "\tbrushSupportLevel: 0x%08X", brushSupportLevel);
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

BOOL rdp_write_glyph_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

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
	return TRUE;
}

BOOL rdp_print_glyph_cache_capability_set(wStream* s, UINT16 length)
{
	GLYPH_CACHE_DEFINITION glyphCache[10];
	GLYPH_CACHE_DEFINITION fragCache;
	UINT16 glyphSupportLevel;
	UINT16 pad2Octets;
	WLog_INFO(TAG,  "GlyphCacheCapabilitySet (length %d):", length);

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
	WLog_INFO(TAG,  "\tglyphCache0: Entries: %d MaximumCellSize: %d", glyphCache[0].cacheEntries, glyphCache[0].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphCache1: Entries: %d MaximumCellSize: %d", glyphCache[1].cacheEntries, glyphCache[1].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphCache2: Entries: %d MaximumCellSize: %d", glyphCache[2].cacheEntries, glyphCache[2].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphCache3: Entries: %d MaximumCellSize: %d", glyphCache[3].cacheEntries, glyphCache[3].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphCache4: Entries: %d MaximumCellSize: %d", glyphCache[4].cacheEntries, glyphCache[4].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphCache5: Entries: %d MaximumCellSize: %d", glyphCache[5].cacheEntries, glyphCache[5].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphCache6: Entries: %d MaximumCellSize: %d", glyphCache[6].cacheEntries, glyphCache[6].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphCache7: Entries: %d MaximumCellSize: %d", glyphCache[7].cacheEntries, glyphCache[7].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphCache8: Entries: %d MaximumCellSize: %d", glyphCache[8].cacheEntries, glyphCache[8].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphCache9: Entries: %d MaximumCellSize: %d", glyphCache[9].cacheEntries, glyphCache[9].cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tfragCache: Entries: %d MaximumCellSize: %d", fragCache.cacheEntries, fragCache.cacheMaximumCellSize);
	WLog_INFO(TAG,  "\tglyphSupportLevel: 0x%04X", glyphSupportLevel);
	WLog_INFO(TAG,  "\tpad2Octets: 0x%04X", pad2Octets);
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

BOOL rdp_write_offscreen_bitmap_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 offscreenSupportLevel = FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	if (settings->OffscreenSupportLevel)
		offscreenSupportLevel = TRUE;

	Stream_Write_UINT32(s, offscreenSupportLevel); /* offscreenSupportLevel (4 bytes) */
	Stream_Write_UINT16(s, settings->OffscreenCacheSize); /* offscreenCacheSize (2 bytes) */
	Stream_Write_UINT16(s, settings->OffscreenCacheEntries); /* offscreenCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_OFFSCREEN_CACHE);
	return TRUE;
}

BOOL rdp_print_offscreen_bitmap_cache_capability_set(wStream* s, UINT16 length)
{
	UINT32 offscreenSupportLevel;
	UINT16 offscreenCacheSize;
	UINT16 offscreenCacheEntries;
	WLog_INFO(TAG,  "OffscreenBitmapCacheCapabilitySet (length %d):", length);

	if (length < 12)
		return FALSE;

	Stream_Read_UINT32(s, offscreenSupportLevel); /* offscreenSupportLevel (4 bytes) */
	Stream_Read_UINT16(s, offscreenCacheSize); /* offscreenCacheSize (2 bytes) */
	Stream_Read_UINT16(s, offscreenCacheEntries); /* offscreenCacheEntries (2 bytes) */
	WLog_INFO(TAG,  "\toffscreenSupportLevel: 0x%08X", offscreenSupportLevel);
	WLog_INFO(TAG,  "\toffscreenCacheSize: 0x%04X", offscreenCacheSize);
	WLog_INFO(TAG,  "\toffscreenCacheEntries: 0x%04X", offscreenCacheEntries);
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

BOOL rdp_write_bitmap_cache_host_support_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	Stream_Write_UINT8(s, BITMAP_CACHE_V2); /* cacheVersion (1 byte) */
	Stream_Write_UINT8(s, 0); /* pad1 (1 byte) */
	Stream_Write_UINT16(s, 0); /* pad2 (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT);
	return TRUE;
}

BOOL rdp_print_bitmap_cache_host_support_capability_set(wStream* s, UINT16 length)
{
	BYTE cacheVersion;
	BYTE pad1;
	UINT16 pad2;
	WLog_INFO(TAG,  "BitmapCacheHostSupportCapabilitySet (length %d):", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT8(s, cacheVersion); /* cacheVersion (1 byte) */
	Stream_Read_UINT8(s, pad1); /* pad1 (1 byte) */
	Stream_Read_UINT16(s, pad2); /* pad2 (2 bytes) */
	WLog_INFO(TAG,  "\tcacheVersion: 0x%02X", cacheVersion);
	WLog_INFO(TAG,  "\tpad1: 0x%02X", pad1);
	WLog_INFO(TAG,  "\tpad2: 0x%04X", pad2);
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

BOOL rdp_write_bitmap_cache_v2_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 cacheFlags;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

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
	return TRUE;
}

BOOL rdp_print_bitmap_cache_v2_capability_set(wStream* s, UINT16 length)
{
	UINT16 cacheFlags;
	BYTE pad2;
	BYTE numCellCaches;
	BITMAP_CACHE_V2_CELL_INFO bitmapCacheV2CellInfo[5];
	WLog_INFO(TAG,  "BitmapCacheV2CapabilitySet (length %d):", length);

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
	WLog_INFO(TAG,  "\tcacheFlags: 0x%04X", cacheFlags);
	WLog_INFO(TAG,  "\tpad2: 0x%02X", pad2);
	WLog_INFO(TAG,  "\tnumCellCaches: 0x%02X", numCellCaches);
	WLog_INFO(TAG,  "\tbitmapCache0CellInfo: numEntries: %d persistent: %d", bitmapCacheV2CellInfo[0].numEntries, bitmapCacheV2CellInfo[0].persistent);
	WLog_INFO(TAG,  "\tbitmapCache1CellInfo: numEntries: %d persistent: %d", bitmapCacheV2CellInfo[1].numEntries, bitmapCacheV2CellInfo[1].persistent);
	WLog_INFO(TAG,  "\tbitmapCache2CellInfo: numEntries: %d persistent: %d", bitmapCacheV2CellInfo[2].numEntries, bitmapCacheV2CellInfo[2].persistent);
	WLog_INFO(TAG,  "\tbitmapCache3CellInfo: numEntries: %d persistent: %d", bitmapCacheV2CellInfo[3].numEntries, bitmapCacheV2CellInfo[3].persistent);
	WLog_INFO(TAG,  "\tbitmapCache4CellInfo: numEntries: %d persistent: %d", bitmapCacheV2CellInfo[4].numEntries, bitmapCacheV2CellInfo[4].persistent);
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

BOOL rdp_write_virtual_channel_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 flags;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	flags = VCCAPS_NO_COMPR;

	Stream_Write_UINT32(s, flags); /* flags (4 bytes) */
	Stream_Write_UINT32(s, settings->VirtualChannelChunkSize); /* VCChunkSize (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_VIRTUAL_CHANNEL);
	return TRUE;
}

BOOL rdp_print_virtual_channel_capability_set(wStream* s, UINT16 length)
{
	UINT32 flags;
	UINT32 VCChunkSize;
	WLog_INFO(TAG,  "VirtualChannelCapabilitySet (length %d):", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, flags); /* flags (4 bytes) */

	if (length > 8)
		Stream_Read_UINT32(s, VCChunkSize); /* VCChunkSize (4 bytes) */
	else
		VCChunkSize = 1600;

	WLog_INFO(TAG,  "\tflags: 0x%08X", flags);
	WLog_INFO(TAG,  "\tVCChunkSize: 0x%08X", VCChunkSize);
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

BOOL rdp_write_draw_nine_grid_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 drawNineGridSupportLevel;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	drawNineGridSupportLevel = (settings->DrawNineGridEnabled) ? DRAW_NINEGRID_SUPPORTED_V2 : DRAW_NINEGRID_NO_SUPPORT;

	Stream_Write_UINT32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	Stream_Write_UINT16(s, settings->DrawNineGridCacheSize); /* drawNineGridCacheSize (2 bytes) */
	Stream_Write_UINT16(s, settings->DrawNineGridCacheEntries); /* drawNineGridCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_DRAW_NINE_GRID_CACHE);
	return TRUE;
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
	WLog_INFO(TAG,  "DrawNineGridCacheCapabilitySet (length %d):", length);

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

BOOL rdp_write_draw_gdiplus_cache_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 drawGDIPlusSupportLevel;
	UINT32 drawGdiplusCacheLevel;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

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
	return TRUE;
}

BOOL rdp_print_draw_gdiplus_cache_capability_set(wStream* s, UINT16 length)
{
	UINT32 drawGdiPlusSupportLevel;
	UINT32 GdipVersion;
	UINT32 drawGdiplusCacheLevel;
	WLog_INFO(TAG,  "DrawGdiPlusCacheCapabilitySet (length %d):", length);

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

BOOL rdp_write_remote_programs_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 railSupportLevel;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	header = rdp_capability_set_start(s);

	railSupportLevel = RAIL_LEVEL_SUPPORTED;

	if (settings->RemoteAppLanguageBarSupported)
		railSupportLevel |= RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED;

	Stream_Write_UINT32(s, railSupportLevel); /* railSupportLevel (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_RAIL);
	return TRUE;
}

BOOL rdp_print_remote_programs_capability_set(wStream* s, UINT16 length)
{
	UINT32 railSupportLevel;
	WLog_INFO(TAG,  "RemoteProgramsCapabilitySet (length %d):", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, railSupportLevel); /* railSupportLevel (4 bytes) */
	WLog_INFO(TAG,  "\trailSupportLevel: 0x%08X", railSupportLevel);
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

BOOL rdp_write_window_list_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 wndSupportLevel;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	wndSupportLevel = WINDOW_LEVEL_SUPPORTED_EX;

	Stream_Write_UINT32(s, wndSupportLevel); /* wndSupportLevel (4 bytes) */
	Stream_Write_UINT8(s, settings->RemoteAppNumIconCaches); /* numIconCaches (1 byte) */
	Stream_Write_UINT16(s, settings->RemoteAppNumIconCacheEntries); /* numIconCacheEntries (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_WINDOW);
	return TRUE;
}

BOOL rdp_print_window_list_capability_set(wStream* s, UINT16 length)
{
	UINT32 wndSupportLevel;
	BYTE numIconCaches;
	UINT16 numIconCacheEntries;
	WLog_INFO(TAG,  "WindowListCapabilitySet (length %d):", length);

	if (length < 11)
		return FALSE;

	Stream_Read_UINT32(s, wndSupportLevel); /* wndSupportLevel (4 bytes) */
	Stream_Read_UINT8(s, numIconCaches); /* numIconCaches (1 byte) */
	Stream_Read_UINT16(s, numIconCacheEntries); /* numIconCacheEntries (2 bytes) */
	WLog_INFO(TAG,  "\twndSupportLevel: 0x%08X", wndSupportLevel);
	WLog_INFO(TAG,  "\tnumIconCaches: 0x%02X", numIconCaches);
	WLog_INFO(TAG,  "\tnumIconCacheEntries: 0x%04X", numIconCacheEntries);
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

BOOL rdp_write_desktop_composition_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 compDeskSupportLevel;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	compDeskSupportLevel = (settings->AllowDesktopComposition) ? COMPDESK_SUPPORTED : COMPDESK_NOT_SUPPORTED;

	Stream_Write_UINT16(s, compDeskSupportLevel); /* compDeskSupportLevel (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_COMP_DESK);
	return TRUE;
}

BOOL rdp_print_desktop_composition_capability_set(wStream* s, UINT16 length)
{
	UINT16 compDeskSupportLevel;
	WLog_INFO(TAG,  "DesktopCompositionCapabilitySet (length %d):", length);

	if (length < 6)
		return FALSE;

	Stream_Read_UINT16(s, compDeskSupportLevel); /* compDeskSupportLevel (2 bytes) */
	WLog_INFO(TAG,  "\tcompDeskSupportLevel: 0x%04X", compDeskSupportLevel);
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
		/*
		 * Special case: The client announces multifragment update support but sets the maximum request size
		 * to something smaller than maximum size for *one* fast-path PDU.
		 * In this case behave like no multifragment updates were supported and make sure no
		 * fragmentation happens by setting FASTPATH_FRAGMENT_SAFE_SIZE.
		 *
		 * This behaviour was observed with some windows ce rdp clients.
		 */
		if (multifragMaxRequestSize < FASTPATH_MAX_PACKET_SIZE)
			multifragMaxRequestSize = FASTPATH_FRAGMENT_SAFE_SIZE;

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

BOOL rdp_write_multifragment_update_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

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
	return TRUE;
}

BOOL rdp_print_multifragment_update_capability_set(wStream* s, UINT16 length)
{
	UINT32 maxRequestSize;
	WLog_INFO(TAG,  "MultifragmentUpdateCapabilitySet (length %d):", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, maxRequestSize); /* maxRequestSize (4 bytes) */
	WLog_INFO(TAG,  "\tmaxRequestSize: 0x%04X", maxRequestSize);
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

BOOL rdp_write_large_pointer_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT16 largePointerSupportFlags;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	largePointerSupportFlags = (settings->LargePointerFlag) ? LARGE_POINTER_FLAG_96x96 : 0;

	Stream_Write_UINT16(s, largePointerSupportFlags); /* largePointerSupportFlags (2 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_LARGE_POINTER);
	return TRUE;
}

BOOL rdp_print_large_pointer_capability_set(wStream* s, UINT16 length)
{
	UINT16 largePointerSupportFlags;
	WLog_INFO(TAG,  "LargePointerCapabilitySet (length %d):", length);

	if (length < 6)
		return FALSE;

	Stream_Read_UINT16(s, largePointerSupportFlags); /* largePointerSupportFlags (2 bytes) */
	WLog_INFO(TAG,  "\tlargePointerSupportFlags: 0x%04X", largePointerSupportFlags);
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

BOOL rdp_write_surface_commands_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	UINT32 cmdFlags;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	cmdFlags = SURFCMDS_SET_SURFACE_BITS | SURFCMDS_STREAM_SURFACE_BITS;

	if (settings->SurfaceFrameMarkerEnabled)
		cmdFlags |= SURFCMDS_FRAME_MARKER;

	Stream_Write_UINT32(s, cmdFlags); /* cmdFlags (4 bytes) */
	Stream_Write_UINT32(s, 0); /* reserved (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_SURFACE_COMMANDS);
	return TRUE;
}

BOOL rdp_print_surface_commands_capability_set(wStream* s, UINT16 length)
{
	UINT32 cmdFlags;
	UINT32 reserved;
	WLog_INFO(TAG,  "SurfaceCommandsCapabilitySet (length %d):", length);

	if (length < 12)
		return FALSE;

	Stream_Read_UINT32(s, cmdFlags); /* cmdFlags (4 bytes) */
	Stream_Read_UINT32(s, reserved); /* reserved (4 bytes) */
	WLog_INFO(TAG,  "\tcmdFlags: 0x%08X", cmdFlags);
	WLog_INFO(TAG,  "\treserved: 0x%08X", reserved);
	return TRUE;
}

void rdp_print_bitmap_codec_guid(GUID* guid)
{
	WLog_INFO(TAG,  "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
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

/**
 * Read bitmap codecs capability set.\n
 * @msdn{dd891377}
 * @param s stream
 * @param settings settings
 * @return if the operation completed successfully
 */

BOOL rdp_read_bitmap_codecs_capability_set(wStream* s, UINT16 length, rdpSettings* settings)
{
	BYTE codecId;
	GUID codecGuid;
	RPC_STATUS rpc_status;
	BYTE bitmapCodecCount;
	UINT16 codecPropertiesLength;
	UINT16 remainingLength;
	BOOL guidNSCodec = FALSE;
	BOOL guidRemoteFx = FALSE;
	BOOL guidRemoteFxImage = FALSE;

	if (length < 5)
		return FALSE;

	Stream_Read_UINT8(s, bitmapCodecCount); /* bitmapCodecCount (1 byte) */
	remainingLength = length - 5;

	while (bitmapCodecCount > 0)
	{
		if (remainingLength < 19)
			return FALSE;

		rdp_read_bitmap_codec_guid(s, &codecGuid); /* codecGuid (16 bytes) */

		Stream_Read_UINT8(s, codecId); /* codecId (1 byte) */
		Stream_Read_UINT16(s, codecPropertiesLength); /* codecPropertiesLength (2 bytes) */
		remainingLength -= 19;

		if (remainingLength < codecPropertiesLength)
			return FALSE;

		if (settings->ServerMode)
		{
			UINT32 beg;
			UINT32 end;

			beg = (UINT32) Stream_GetPosition(s);
			end = beg + codecPropertiesLength;

			if (UuidEqual(&codecGuid, &CODEC_GUID_REMOTEFX, &rpc_status))
			{
				UINT32 rfxCapsLength;
				UINT32 rfxPropsLength;
				UINT32 captureFlags;

				guidRemoteFx = TRUE;
				settings->RemoteFxCodecId = codecId;

				Stream_Read_UINT32(s, rfxPropsLength); /* length (4 bytes) */
				Stream_Read_UINT32(s, captureFlags); /* captureFlags (4 bytes) */
				Stream_Read_UINT32(s, rfxCapsLength); /* capsLength (4 bytes) */

				settings->RemoteFxCaptureFlags = captureFlags;
				settings->RemoteFxOnly = (captureFlags & CARDP_CAPS_CAPTURE_NON_CAC) ? TRUE : FALSE;

				if (rfxCapsLength)
				{
					UINT16 blockType;
					UINT32 blockLen;
					UINT16 numCapsets;
					BYTE rfxCodecId;
					UINT16 capsetType;
					UINT16 numIcaps;
					UINT16 icapLen;

					/* TS_RFX_CAPS */

					Stream_Read_UINT16(s, blockType); /* blockType (2 bytes) */
					Stream_Read_UINT32(s, blockLen); /* blockLen (4 bytes) */
					Stream_Read_UINT16(s, numCapsets); /* numCapsets (2 bytes) */

					if (blockType != 0xCBC0)
						return FALSE;

					if (blockLen != 8)
						return FALSE;

					if (numCapsets != 1)
						return FALSE;

					/* TS_RFX_CAPSET */

					Stream_Read_UINT16(s, blockType); /* blockType (2 bytes) */
					Stream_Read_UINT32(s, blockLen); /* blockLen (4 bytes) */
					Stream_Read_UINT8(s, rfxCodecId); /* codecId (1 byte) */
					Stream_Read_UINT16(s, capsetType); /* capsetType (2 bytes) */
					Stream_Read_UINT16(s, numIcaps); /* numIcaps (2 bytes) */
					Stream_Read_UINT16(s, icapLen); /* icapLen (2 bytes) */

					if (blockType != 0xCBC1)
						return FALSE;

					if (rfxCodecId != 1)
						return FALSE;

					if (capsetType != 0xCFC0)
						return FALSE;

					while (numIcaps--)
					{
						UINT16 version;
						UINT16 tileSize;
						BYTE codecFlags;
						BYTE colConvBits;
						BYTE transformBits;
						BYTE entropyBits;

						/* TS_RFX_ICAP */

						Stream_Read_UINT16(s, version); /* version (2 bytes) */
						Stream_Read_UINT16(s, tileSize); /* tileSize (2 bytes) */
						Stream_Read_UINT8(s, codecFlags); /* flags (1 byte) */
						Stream_Read_UINT8(s, colConvBits); /* colConvBits (1 byte) */
						Stream_Read_UINT8(s, transformBits); /* transformBits (1 byte) */
						Stream_Read_UINT8(s, entropyBits); /* entropyBits (1 byte) */

						if (version == 0x0009)
						{
							/* Version 0.9 */

							if (tileSize != 0x0080)
								return FALSE;
						}
						else if (version == 0x0100)
						{
							/* Version 1.0 */

							if (tileSize != 0x0040)
								return FALSE;
						}
						else
							return FALSE;

						if (colConvBits != 1)
							return FALSE;

						if (transformBits != 1)
							return FALSE;
					}
				}
			}
			else if (UuidEqual(&codecGuid, &CODEC_GUID_IMAGE_REMOTEFX, &rpc_status))
			{
				/* Microsoft RDP servers ignore CODEC_GUID_IMAGE_REMOTEFX codec properties */

				guidRemoteFxImage = TRUE;
				Stream_Seek(s, codecPropertiesLength); /* codecProperties */
			}
			else if (UuidEqual(&codecGuid, &CODEC_GUID_NSCODEC, &rpc_status))
			{
				BYTE colorLossLevel;
				BYTE fAllowSubsampling;
				BYTE fAllowDynamicFidelity;

				guidNSCodec = TRUE;
				settings->NSCodecId = codecId;

				Stream_Read_UINT8(s, fAllowDynamicFidelity); /* fAllowDynamicFidelity (1 byte) */
				Stream_Read_UINT8(s, fAllowSubsampling); /* fAllowSubsampling (1 byte) */
				Stream_Read_UINT8(s, colorLossLevel); /* colorLossLevel (1 byte) */

				if (colorLossLevel < 1)
					colorLossLevel = 1;

				if (colorLossLevel > 7)
					colorLossLevel = 7;

				settings->NSCodecAllowDynamicColorFidelity = fAllowDynamicFidelity;
				settings->NSCodecAllowSubsampling = fAllowSubsampling;
				settings->NSCodecColorLossLevel = colorLossLevel;
			}
			else if (UuidEqual(&codecGuid, &CODEC_GUID_IGNORE, &rpc_status))
			{
				Stream_Seek(s, codecPropertiesLength); /* codecProperties */
			}
			else
			{
				Stream_Seek(s, codecPropertiesLength); /* codecProperties */
			}

			if (Stream_GetPosition(s) != end)
			{
				WLog_ERR(TAG, "error while reading codec properties: actual offset: %d expected offset: %d",
						(int) Stream_GetPosition(s), end);
				Stream_SetPosition(s, end);
			}

			remainingLength -= codecPropertiesLength;
		}
		else
		{
			Stream_Seek(s, codecPropertiesLength); /* codecProperties */
			remainingLength -= codecPropertiesLength;
		}

		bitmapCodecCount--;
	}

	if (settings->ServerMode)
	{
		/* only enable a codec if we've announced/enabled it before */
		settings->RemoteFxCodec = settings->RemoteFxCodec && guidRemoteFx;
		settings->RemoteFxImageCodec = settings->RemoteFxImageCodec && guidRemoteFxImage;
		settings->NSCodec = settings->NSCodec && guidNSCodec;
		settings->JpegCodec = FALSE;
	}

	return TRUE;
}

/**
 * Write RemoteFX Client Capability Container.\n
 * @param s stream
 * @param settings settings
 */
BOOL rdp_write_rfx_client_capability_container(wStream* s, rdpSettings* settings)
{
	UINT32 captureFlags;
	BYTE codecMode;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

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
	return TRUE;
}

/**
 * Write NSCODEC Client Capability Container.\n
 * @param s stream
 * @param settings settings
 */
BOOL rdp_write_nsc_client_capability_container(wStream* s, rdpSettings* settings)
{
	BYTE colorLossLevel;
	BYTE fAllowSubsampling;
	BYTE fAllowDynamicFidelity;

	fAllowDynamicFidelity = settings->NSCodecAllowDynamicColorFidelity;
	fAllowSubsampling = settings->NSCodecAllowSubsampling;
	colorLossLevel = settings->NSCodecColorLossLevel;

	if (colorLossLevel < 1)
		colorLossLevel = 1;

	if (colorLossLevel > 7)
		colorLossLevel = 7;

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, 3); /* codecPropertiesLength */

	/* TS_NSCODEC_CAPABILITYSET */
	Stream_Write_UINT8(s, fAllowDynamicFidelity);  /* fAllowDynamicFidelity (1 byte) */
	Stream_Write_UINT8(s, fAllowSubsampling);  /* fAllowSubsampling (1 byte) */
	Stream_Write_UINT8(s, colorLossLevel);  /* colorLossLevel (1 byte) */
	return TRUE;
}

BOOL rdp_write_jpeg_client_capability_container(wStream* s, rdpSettings* settings)
{
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, 1); /* codecPropertiesLength */
	Stream_Write_UINT8(s, settings->JpegQuality);
	return TRUE;
}

/**
 * Write RemoteFX Server Capability Container.\n
 * @param s stream
 * @param settings settings
 */
BOOL rdp_write_rfx_server_capability_container(wStream* s, rdpSettings* settings)
{
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, 4); /* codecPropertiesLength */
	Stream_Write_UINT32(s, 0); /* reserved */
	return TRUE;
}

BOOL rdp_write_jpeg_server_capability_container(wStream* s, rdpSettings* settings)
{
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, 1); /* codecPropertiesLength */
	Stream_Write_UINT8(s, 75);
	return TRUE;
}

/**
 * Write NSCODEC Server Capability Container.\n
 * @param s stream
 * @param settings settings
 */
BOOL rdp_write_nsc_server_capability_container(wStream* s, rdpSettings* settings)
{
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, 4); /* codecPropertiesLength */
	Stream_Write_UINT32(s, 0); /* reserved */
	return TRUE;
}

/**
 * Write bitmap codecs capability set.\n
 * @msdn{dd891377}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_write_bitmap_codecs_capability_set(wStream* s, rdpSettings* settings)
{
	int header;
	BYTE bitmapCodecCount;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	header = rdp_capability_set_start(s);

	bitmapCodecCount = 0;

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
			if (!rdp_write_rfx_server_capability_container(s, settings))
				return FALSE;
		}
		else
		{
			Stream_Write_UINT8(s, RDP_CODEC_ID_REMOTEFX); /* codecID */
			if (!rdp_write_rfx_client_capability_container(s, settings))
				return FALSE;
		}
	}

	if (settings->NSCodec)
	{
		rdp_write_bitmap_codec_guid(s, &CODEC_GUID_NSCODEC); /* codecGUID */

		if (settings->ServerMode)
		{
			Stream_Write_UINT8(s, 0); /* codecID is defined by the client */
			if (!rdp_write_nsc_server_capability_container(s, settings))
				return FALSE;
		}
		else
		{
			Stream_Write_UINT8(s, RDP_CODEC_ID_NSCODEC); /* codecID */
			if (!rdp_write_nsc_client_capability_container(s, settings))
				return FALSE;
		}
	}

	if (settings->JpegCodec)
	{
		rdp_write_bitmap_codec_guid(s, &CODEC_GUID_JPEG); /* codecGUID */

		if (settings->ServerMode)
		{
			Stream_Write_UINT8(s, 0); /* codecID is defined by the client */
			if (!rdp_write_jpeg_server_capability_container(s, settings))
				return FALSE;
		}
		else
		{
			Stream_Write_UINT8(s, RDP_CODEC_ID_JPEG); /* codecID */
			if (!rdp_write_jpeg_client_capability_container(s, settings))
				return FALSE;
		}
	}

	if (settings->RemoteFxImageCodec)
	{
		rdp_write_bitmap_codec_guid(s, &CODEC_GUID_IMAGE_REMOTEFX); /* codecGUID */

		if (settings->ServerMode)
		{
			Stream_Write_UINT8(s, 0); /* codecID is defined by the client */
			if (!rdp_write_rfx_server_capability_container(s, settings))
				return FALSE;
		}
		else
		{
			Stream_Write_UINT8(s, RDP_CODEC_ID_IMAGE_REMOTEFX); /* codecID */
			if (!rdp_write_rfx_client_capability_container(s, settings))
				return FALSE;
		}
	}

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CODECS);
	return TRUE;
}

BOOL rdp_print_bitmap_codecs_capability_set(wStream* s, UINT16 length)
{
	GUID codecGuid;
	BYTE bitmapCodecCount;
	BYTE codecId;
	UINT16 codecPropertiesLength;
	UINT16 remainingLength;
	WLog_INFO(TAG,  "BitmapCodecsCapabilitySet (length %d):", length);

	if (length < 5)
		return FALSE;

	Stream_Read_UINT8(s, bitmapCodecCount); /* bitmapCodecCount (1 byte) */
	remainingLength = length - 5;
	WLog_INFO(TAG,  "\tbitmapCodecCount: %d", bitmapCodecCount);

	while (bitmapCodecCount > 0)
	{
		if (remainingLength < 19)
			return FALSE;

		rdp_read_bitmap_codec_guid(s, &codecGuid); /* codecGuid (16 bytes) */
		Stream_Read_UINT8(s, codecId); /* codecId (1 byte) */
		WLog_INFO(TAG,  "\tcodecGuid: 0x");
		rdp_print_bitmap_codec_guid(&codecGuid);
		WLog_INFO(TAG,  " (%s)", rdp_get_bitmap_codec_guid_name(&codecGuid));
		WLog_INFO(TAG,  "\tcodecId: %d", codecId);
		Stream_Read_UINT16(s, codecPropertiesLength); /* codecPropertiesLength (2 bytes) */
		WLog_INFO(TAG,  "\tcodecPropertiesLength: %d", codecPropertiesLength);
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

BOOL rdp_write_frame_acknowledge_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);

	Stream_Write_UINT32(s, settings->FrameAcknowledge); /* (4 bytes) */

	rdp_capability_set_finish(s, header, CAPSET_TYPE_FRAME_ACKNOWLEDGE);
	return TRUE;
}

BOOL rdp_print_frame_acknowledge_capability_set(wStream* s, UINT16 length)
{
	UINT32 frameAcknowledge;
	WLog_INFO(TAG,  "FrameAcknowledgeCapabilitySet (length %d):", length);

	if (length < 8)
		return FALSE;

	Stream_Read_UINT32(s, frameAcknowledge); /* frameAcknowledge (4 bytes) */
	WLog_INFO(TAG,  "\tframeAcknowledge: 0x%08X", frameAcknowledge);
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

BOOL rdp_write_bitmap_cache_v3_codec_id_capability_set(wStream* s, rdpSettings* settings)
{
	int header;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	header = rdp_capability_set_start(s);
	Stream_Write_UINT8(s, settings->BitmapCacheV3CodecId);

	rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID);
	return TRUE;
}

BOOL rdp_print_bitmap_cache_v3_codec_id_capability_set(wStream* s, UINT16 length)
{
	BYTE bitmapCacheV3CodecId;
	WLog_INFO(TAG,  "BitmapCacheV3CodecIdCapabilitySet (length %d):", length);

	if (length < 5)
		return FALSE;

	Stream_Read_UINT8(s, bitmapCacheV3CodecId); /* bitmapCacheV3CodecId (1 byte) */
	WLog_INFO(TAG,  "\tbitmapCacheV3CodecId: 0x%02X", bitmapCacheV3CodecId);
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
		WLog_INFO(TAG,  "%s ", receiving ? "Receiving" : "Sending");
		em = bm + length;

		if (Stream_GetRemainingLength(s) < (size_t) (length - 4))
		{
			WLog_ERR(TAG,  "error processing stream");
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
				WLog_ERR(TAG,  "unknown capability type %d", type);
				break;
		}

		if (s->pointer != em)
		{
			WLog_ERR(TAG,  "incorrect offset, type:0x%02X actual:%d expected:%d",
					 type, (int)(s->pointer - bm), (int)(em - bm));
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
	BOOL treated;

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
			WLog_WARN(TAG,  "not handling capability type %d yet", type);
		}

		em = bm + length;

		if (Stream_GetRemainingLength(s) < ((size_t) length - 4))
		{
			WLog_ERR(TAG,  "error processing stream");
			return FALSE;
		}

		treated = TRUE;
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

			case CAPSET_TYPE_POINTER:
				if (!rdp_read_pointer_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_INPUT:
				if (!rdp_read_input_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_VIRTUAL_CHANNEL:
				if (!rdp_read_virtual_channel_capability_set(s, length, settings))
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

			case CAPSET_TYPE_FONT:
				if (!rdp_read_font_capability_set(s, length, settings))
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

			case CAPSET_TYPE_MULTI_FRAGMENT_UPDATE:
				if (!rdp_read_multifragment_update_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_LARGE_POINTER:
				if (!rdp_read_large_pointer_capability_set(s, length, settings))
					return FALSE;
				break;

			case CAPSET_TYPE_COMP_DESK:
				if (!rdp_read_desktop_composition_capability_set(s, length, settings))
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
				treated = FALSE;
				break;
		}

		if (!treated)
		{
			if (settings->ServerMode)
			{
				/* treating capabilities that are supposed to be send only from the client */
				switch (type)
				{
				case CAPSET_TYPE_BITMAP_CACHE:
					if (!rdp_read_bitmap_cache_capability_set(s, length, settings))
						return FALSE;
					break;

				case CAPSET_TYPE_BITMAP_CACHE_V2:
					if (!rdp_read_bitmap_cache_v2_capability_set(s, length, settings))
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

				case CAPSET_TYPE_SOUND:
					if (!rdp_read_sound_capability_set(s, length, settings))
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

				case CAPSET_TYPE_DRAW_NINE_GRID_CACHE:
					if (!rdp_read_draw_nine_grid_cache_capability_set(s, length, settings))
						return FALSE;
					break;

				default:
					WLog_ERR(TAG, "capability %s(%d) not expected from client", get_capability_name(type), type);
					return FALSE;
				}
			}
			else
			{
				/* treating capabilities that are supposed to be send only from the server */
				switch (type)
				{
				case CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT:
					if (!rdp_read_bitmap_cache_host_support_capability_set(s, length, settings))
						return FALSE;
					break;

				default:
					WLog_ERR(TAG, "capability %s(%d) not expected from server", get_capability_name(type), type);
					return FALSE;
				}
			}
		}

		if (s->pointer != em)
		{
			WLog_ERR(TAG,  "incorrect offset, type:0x%02X actual:%d expected:%d",
					 type, (int)(s->pointer - bm), (int)(em - bm));
		}

		Stream_SetPointer(s, em);
		numberCapabilities--;
	}

	if (numberCapabilities)
	{
		WLog_ERR(TAG,  "strange we haven't read the number of announced capacity sets, read=%d expected=%d",
				 count-numberCapabilities, count);
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

	if (freerdp_shall_disconnect(rdp->instance))
		return TRUE;

	if (rdp->settings->UseRdpSecurityLayer)
	{
		if (!rdp_read_security_header(s, &securityFlags))
			return FALSE;

		if (securityFlags & SEC_ENCRYPT)
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				WLog_ERR(TAG,  "rdp_decrypt failed");
				return FALSE;
			}
		}
	}

	if (*pChannelId != MCS_GLOBAL_CHANNEL_ID)
	{
		UINT16 mcsMessageChannelId = rdp->mcs->messageChannelId;

		if ((mcsMessageChannelId == 0) || (*pChannelId != mcsMessageChannelId))
		{
			WLog_ERR(TAG,  "unexpected MCS channel id %04x received", *pChannelId);
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

	if (freerdp_shall_disconnect(rdp->instance))
		return TRUE;

	if (!rdp_read_share_control_header(s, &pduLength, &pduType, &pduSource))
	{
		WLog_ERR(TAG,  "rdp_read_share_control_header failed");
		return FALSE;
	}

	if (pduType == PDU_TYPE_DATA)
	{
		/**
		 * We can receive a Save Session Info Data PDU containing a LogonErrorInfo
		 * structure at this point from the server to indicate a connection error.
		 */

		if (rdp_recv_data_pdu(rdp, s) < 0)
			return FALSE;

		return FALSE;
	}

	if (pduType != PDU_TYPE_DEMAND_ACTIVE)
	{
		if (pduType != PDU_TYPE_SERVER_REDIRECTION)
			WLog_ERR(TAG, "expected PDU_TYPE_DEMAND_ACTIVE %04x, got %04x", PDU_TYPE_DEMAND_ACTIVE, pduType);

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
		WLog_ERR(TAG,  "rdp_read_capability_sets failed");
		return FALSE;
	}

	rdp->update->secondary->glyph_v2 = (rdp->settings->GlyphSupportLevel > GLYPH_SUPPORT_FULL) ? TRUE : FALSE;

	return TRUE;
}

BOOL rdp_write_demand_active(wStream* s, rdpSettings* settings)
{
	int bm, em, lm;
	UINT16 numberCapabilities;
	UINT16 lengthCombinedCapabilities;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	Stream_Write_UINT32(s, settings->ShareId); /* shareId (4 bytes) */
	Stream_Write_UINT16(s, 4); /* lengthSourceDescriptor (2 bytes) */

	lm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s); /* lengthCombinedCapabilities (2 bytes) */
	Stream_Write(s, "RDP", 4); /* sourceDescriptor */

	bm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s); /* numberCapabilities (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */

	numberCapabilities = 14;
	if (!rdp_write_general_capability_set(s, settings) ||
		!rdp_write_bitmap_capability_set(s, settings) ||
		!rdp_write_order_capability_set(s, settings) ||
		!rdp_write_pointer_capability_set(s, settings) ||
		!rdp_write_input_capability_set(s, settings) ||
		!rdp_write_virtual_channel_capability_set(s, settings) ||
		!rdp_write_share_capability_set(s, settings) ||
		!rdp_write_font_capability_set(s, settings) ||
		!rdp_write_multifragment_update_capability_set(s, settings) ||
		!rdp_write_large_pointer_capability_set(s, settings) ||
		!rdp_write_desktop_composition_capability_set(s, settings) ||
		!rdp_write_surface_commands_capability_set(s, settings) ||
		!rdp_write_bitmap_codecs_capability_set(s, settings) ||
		!rdp_write_frame_acknowledge_capability_set(s, settings))
	{
		return FALSE;
	}

	if (settings->BitmapCachePersistEnabled)
	{
		numberCapabilities++;
		if (!rdp_write_bitmap_cache_host_support_capability_set(s, settings))
			return FALSE;
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
	return TRUE;
}

BOOL rdp_send_demand_active(rdpRdp* rdp)
{
	wStream* s;
	BOOL status;

	if (!(s = Stream_New(NULL, 4096)))
		return FALSE;

	rdp_init_stream_pdu(rdp, s);

	rdp->settings->ShareId = 0x10000 + rdp->mcs->userId;

	status = rdp_write_demand_active(s, rdp->settings) &&
			rdp_send_pdu(rdp, s, PDU_TYPE_DEMAND_ACTIVE, rdp->mcs->userId);

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
		/* client does not support multi fragment updates - make sure packages are not fragmented */
		settings->MultifragMaxRequestSize = FASTPATH_FRAGMENT_SAFE_SIZE;
	}

	if (!settings->ReceivedCapabilities[CAPSET_TYPE_LARGE_POINTER])
	{
		/* client does not support large pointers */
		settings->LargePointerFlag = 0;
	}

	return status;
}

BOOL rdp_write_confirm_active(wStream* s, rdpSettings* settings)
{
	int bm, em, lm;
	UINT16 numberCapabilities;
	UINT16 lengthSourceDescriptor;
	UINT16 lengthCombinedCapabilities;
	BOOL ret;

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
	if (!rdp_write_general_capability_set(s, settings) ||
		!rdp_write_bitmap_capability_set(s, settings) ||
		!rdp_write_order_capability_set(s, settings))
		return FALSE;

	if (settings->RdpVersion >= 5)
		ret = rdp_write_bitmap_cache_v2_capability_set(s, settings);
	else
		ret = rdp_write_bitmap_cache_capability_set(s, settings);

	if (!ret)
		return FALSE;

	if (!rdp_write_pointer_capability_set(s, settings) ||
		!rdp_write_input_capability_set(s, settings) ||
		!rdp_write_brush_capability_set(s, settings) ||
		!rdp_write_glyph_cache_capability_set(s, settings) ||
		!rdp_write_virtual_channel_capability_set(s, settings) ||
		!rdp_write_sound_capability_set(s, settings) ||
		!rdp_write_share_capability_set(s, settings) ||
		!rdp_write_font_capability_set(s, settings) ||
		!rdp_write_control_capability_set(s, settings) ||
		!rdp_write_color_cache_capability_set(s, settings) ||
		!rdp_write_window_activation_capability_set(s, settings))
	{
		return FALSE;
	}

	if (settings->OffscreenSupportLevel)
	{
		numberCapabilities++;
		if (!rdp_write_offscreen_bitmap_cache_capability_set(s, settings))
			return FALSE;
	}

	if (settings->DrawNineGridEnabled)
	{
		numberCapabilities++;
		if (!rdp_write_draw_nine_grid_cache_capability_set(s, settings))
			return FALSE;
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_LARGE_POINTER])
	{
		if (settings->LargePointerFlag)
		{
			numberCapabilities++;
			if (!rdp_write_large_pointer_capability_set(s, settings))
				return FALSE;
		}
	}

	if (settings->RemoteApplicationMode)
	{
		numberCapabilities += 2;
		if (!rdp_write_remote_programs_capability_set(s, settings) ||
			!rdp_write_window_list_capability_set(s, settings))
			return FALSE;
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_MULTI_FRAGMENT_UPDATE])
	{
		numberCapabilities++;
		if (!rdp_write_multifragment_update_capability_set(s, settings))
			return FALSE;
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_SURFACE_COMMANDS])
	{
		numberCapabilities++;
		if (!rdp_write_surface_commands_capability_set(s, settings))
			return FALSE;
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_BITMAP_CODECS])
	{
		numberCapabilities++;
		if (!rdp_write_bitmap_codecs_capability_set(s, settings))
			return FALSE;
	}

	if (!settings->ReceivedCapabilities[CAPSET_TYPE_FRAME_ACKNOWLEDGE])
		settings->FrameAcknowledge = 0;

	if (settings->FrameAcknowledge)
	{
		numberCapabilities++;
		if (!rdp_write_frame_acknowledge_capability_set(s, settings))
			return FALSE;
	}

	if (settings->ReceivedCapabilities[CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID])
	{
		if (settings->BitmapCacheV3CodecId != 0)
		{
			numberCapabilities++;
			if (!rdp_write_bitmap_cache_v3_codec_id_capability_set(s, settings))
				return FALSE;
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
	return TRUE;
}

BOOL rdp_send_confirm_active(rdpRdp* rdp)
{
	wStream* s;
	BOOL status;

	if (!(s = Stream_New(NULL, 4096)))
		return FALSE;

	rdp_init_stream_pdu(rdp, s);

	status = rdp_write_confirm_active(s, rdp->settings) &&
			rdp_send_pdu(rdp, s, PDU_TYPE_CONFIRM_ACTIVE, rdp->mcs->userId);

	Stream_Free(s, TRUE);

	return status;
}

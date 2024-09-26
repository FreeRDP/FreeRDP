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

#include <winpr/wtsapi.h>
#include <freerdp/config.h>

#include "settings.h"
#include "capabilities.h"
#include "fastpath.h"

#include <winpr/crt.h>
#include <winpr/rpc.h>

#include <freerdp/log.h>

#define TAG FREERDP_TAG("core.capabilities")

static const char* const CAPSET_TYPE_STRINGS[] = { "Unknown",
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
	                                               "Frame Acknowledge" };

static const char* get_capability_name(UINT16 type)
{
	if (type > CAPSET_TYPE_FRAME_ACKNOWLEDGE)
		return "<unknown>";

	return CAPSET_TYPE_STRINGS[type];
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_capability_sets(wStream* s, size_t start, BOOL receiving);
#endif

/* CODEC_GUID_REMOTEFX: 0x76772F12BD724463AFB3B73C9C6F7886 */

static const GUID CODEC_GUID_REMOTEFX = {
	0x76772F12, 0xBD72, 0x4463, { 0xAF, 0xB3, 0xB7, 0x3C, 0x9C, 0x6F, 0x78, 0x86 }
};

/* CODEC_GUID_NSCODEC 0xCA8D1BB9000F154F589FAE2D1A87E2D6 */

static const GUID CODEC_GUID_NSCODEC = {
	0xCA8D1BB9, 0x000F, 0x154F, { 0x58, 0x9F, 0xAE, 0x2D, 0x1A, 0x87, 0xE2, 0xD6 }
};

/* CODEC_GUID_IGNORE 0x9C4351A6353542AE910CCDFCE5760B58 */

static const GUID CODEC_GUID_IGNORE = {
	0x9C4351A6, 0x3535, 0x42AE, { 0x91, 0x0C, 0xCD, 0xFC, 0xE5, 0x76, 0x0B, 0x58 }
};

/* CODEC_GUID_IMAGE_REMOTEFX 0x2744CCD49D8A4E74803C0ECBEEA19C54 */

static const GUID CODEC_GUID_IMAGE_REMOTEFX = {
	0x2744CCD4, 0x9D8A, 0x4E74, { 0x80, 0x3C, 0x0E, 0xCB, 0xEE, 0xA1, 0x9C, 0x54 }
};

#if defined(WITH_JPEG)
/* CODEC_GUID_JPEG 0x430C9EED1BAF4CE6869ACB8B37B66237 */

static const GUID CODEC_GUID_JPEG = {
	0x430C9EED, 0x1BAF, 0x4CE6, { 0x86, 0x9A, 0xCB, 0x8B, 0x37, 0xB6, 0x62, 0x37 }
};
#endif

static BOOL rdp_read_capability_set_header(wStream* s, UINT16* length, UINT16* type)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(length);
	WINPR_ASSERT(type);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;
	Stream_Read_UINT16(s, *type);   /* capabilitySetType */
	Stream_Read_UINT16(s, *length); /* lengthCapability */
	if (*length < 4)
		return FALSE;
	return TRUE;
}

static void rdp_write_capability_set_header(wStream* s, UINT16 length, UINT16 type)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 4);
	Stream_Write_UINT16(s, type);   /* capabilitySetType */
	Stream_Write_UINT16(s, length); /* lengthCapability */
}

static size_t rdp_capability_set_start(wStream* s)
{
	size_t header = Stream_GetPosition(s);
	if (!Stream_CheckAndLogRequiredCapacity(TAG, (s), CAPSET_HEADER_LENGTH))
		return SIZE_MAX;
	Stream_Zero(s, CAPSET_HEADER_LENGTH);
	return header;
}

static BOOL rdp_capability_set_finish(wStream* s, size_t header, UINT16 type)
{
	const size_t footer = Stream_GetPosition(s);
	if (header > footer)
		return FALSE;
	if (header > UINT16_MAX)
		return FALSE;
	const size_t length = footer - header;
	if ((Stream_Capacity(s) < header + 4ULL) || (length > UINT16_MAX))
		return FALSE;
	Stream_SetPosition(s, header);
	rdp_write_capability_set_header(s, (UINT16)length, type);
	Stream_SetPosition(s, footer);
	return TRUE;
}

static BOOL rdp_apply_general_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	if (settings->ServerMode)
	{
		settings->OsMajorType = src->OsMajorType;
		settings->OsMinorType = src->OsMinorType;
	}

	settings->CapsProtocolVersion = src->CapsProtocolVersion;
	settings->NoBitmapCompressionHeader = src->NoBitmapCompressionHeader;
	settings->LongCredentialsSupported = src->LongCredentialsSupported;
	settings->AutoReconnectionPacketSupported = src->AutoReconnectionPacketSupported;
	if (!src->FastPathOutput)
		settings->FastPathOutput = FALSE;

	if (!src->SaltedChecksum)
		settings->SaltedChecksum = FALSE;

	if (!settings->ServerMode)
	{
		/*
		 * Note: refreshRectSupport and suppressOutputSupport are
		 * server-only flags indicating to the client weather the
		 * respective PDUs are supported. See MS-RDPBCGR 2.2.7.1.1
		 */
		if (!src->RefreshRect)
			settings->RefreshRect = FALSE;

		if (!src->SuppressOutput)
			settings->SuppressOutput = FALSE;
	}
	return TRUE;
}

/*
 * Read general capability set.
 * msdn{cc240549}
 */

static BOOL rdp_read_general_capability_set(wStream* s, rdpSettings* settings)
{
	UINT16 extraFlags = 0;
	BYTE refreshRectSupport = 0;
	BYTE suppressOutputSupport = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return FALSE;

	WINPR_ASSERT(settings);
	Stream_Read_UINT16(s, settings->OsMajorType); /* osMajorType (2 bytes) */
	Stream_Read_UINT16(s, settings->OsMinorType); /* osMinorType (2 bytes) */

	Stream_Read_UINT16(s, settings->CapsProtocolVersion); /* protocolVersion (2 bytes) */
	if (settings->CapsProtocolVersion != TS_CAPS_PROTOCOLVERSION)
	{
		WLog_ERR(TAG,
		         "TS_GENERAL_CAPABILITYSET::protocolVersion(0x%04" PRIx16
		         ") != TS_CAPS_PROTOCOLVERSION(0x%04" PRIx32 ")",
		         settings->CapsProtocolVersion, TS_CAPS_PROTOCOLVERSION);
		if (settings->CapsProtocolVersion == 0x0000)
		{
			WLog_WARN(TAG,
			          "TS_GENERAL_CAPABILITYSET::protocolVersion(0x%04" PRIx16
			          " assuming old FreeRDP, ignoring protocol violation, correcting value.",
			          settings->CapsProtocolVersion);
			settings->CapsProtocolVersion = TS_CAPS_PROTOCOLVERSION;
		}
		else
			return FALSE;
	}
	Stream_Seek_UINT16(s);                                /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT16(
	    s, settings->CapsGeneralCompressionTypes); /* generalCompressionTypes (2 bytes) */
	Stream_Read_UINT16(s, extraFlags);             /* extraFlags (2 bytes) */
	Stream_Read_UINT16(s, settings->CapsUpdateCapabilityFlag); /* updateCapabilityFlag (2 bytes) */
	Stream_Read_UINT16(s, settings->CapsRemoteUnshareFlag);    /* remoteUnshareFlag (2 bytes) */
	Stream_Read_UINT16(
	    s, settings->CapsGeneralCompressionLevel); /* generalCompressionLevel (2 bytes) */
	Stream_Read_UINT8(s, refreshRectSupport);      /* refreshRectSupport (1 byte) */
	Stream_Read_UINT8(s, suppressOutputSupport);   /* suppressOutputSupport (1 byte) */
	settings->NoBitmapCompressionHeader = (extraFlags & NO_BITMAP_COMPRESSION_HDR) ? TRUE : FALSE;
	settings->LongCredentialsSupported = (extraFlags & LONG_CREDENTIALS_SUPPORTED) ? TRUE : FALSE;

	settings->AutoReconnectionPacketSupported =
	    (extraFlags & AUTORECONNECT_SUPPORTED) ? TRUE : FALSE;
	settings->FastPathOutput = (extraFlags & FASTPATH_OUTPUT_SUPPORTED) ? TRUE : FALSE;
	settings->SaltedChecksum = (extraFlags & ENC_SALTED_CHECKSUM) ? TRUE : FALSE;
	settings->RefreshRect = refreshRectSupport;
	settings->SuppressOutput = suppressOutputSupport;

	return TRUE;
}

/*
 * Write general capability set.
 * msdn{cc240549}
 */

static BOOL rdp_write_general_capability_set(wStream* s, const rdpSettings* settings)
{
	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	UINT16 extraFlags = 0;

	WINPR_ASSERT(settings);
	if (settings->LongCredentialsSupported)
		extraFlags |= LONG_CREDENTIALS_SUPPORTED;

	if (settings->NoBitmapCompressionHeader)
		extraFlags |= NO_BITMAP_COMPRESSION_HDR;

	if (settings->AutoReconnectionPacketSupported)
		extraFlags |= AUTORECONNECT_SUPPORTED;

	if (settings->FastPathOutput)
		extraFlags |= FASTPATH_OUTPUT_SUPPORTED;

	if (settings->SaltedChecksum)
		extraFlags |= ENC_SALTED_CHECKSUM;

	if ((settings->OsMajorType > UINT16_MAX) || (settings->OsMinorType > UINT16_MAX))
	{
		WLog_ERR(TAG,
		         "OsMajorType=%08" PRIx32 ", OsMinorType=%08" PRIx32
		         " they need to be smaller %04" PRIx16,
		         settings->OsMajorType, settings->OsMinorType, UINT16_MAX);
		return FALSE;
	}
	if (settings->CapsProtocolVersion != TS_CAPS_PROTOCOLVERSION)
	{
		WLog_ERR(TAG,
		         "TS_GENERAL_CAPABILITYSET::protocolVersion(0x%04" PRIx16
		         ") != TS_CAPS_PROTOCOLVERSION(0x%04" PRIx32 ")",
		         settings->CapsProtocolVersion, TS_CAPS_PROTOCOLVERSION);
		return FALSE;
	}
	Stream_Write_UINT16(s, (UINT16)settings->OsMajorType); /* osMajorType (2 bytes) */
	Stream_Write_UINT16(s, (UINT16)settings->OsMinorType); /* osMinorType (2 bytes) */
	Stream_Write_UINT16(s, settings->CapsProtocolVersion); /* protocolVersion (2 bytes) */
	Stream_Write_UINT16(s, 0);                             /* pad2OctetsA (2 bytes) */
	Stream_Write_UINT16(
	    s, settings->CapsGeneralCompressionTypes); /* generalCompressionTypes (2 bytes) */
	Stream_Write_UINT16(s, extraFlags);            /* extraFlags (2 bytes) */
	Stream_Write_UINT16(s, settings->CapsUpdateCapabilityFlag); /* updateCapabilityFlag (2 bytes) */
	Stream_Write_UINT16(s, settings->CapsRemoteUnshareFlag);    /* remoteUnshareFlag (2 bytes) */
	Stream_Write_UINT16(
	    s, settings->CapsGeneralCompressionLevel);           /* generalCompressionLevel (2 bytes) */
	Stream_Write_UINT8(s, settings->RefreshRect ? 1 : 0);    /* refreshRectSupport (1 byte) */
	Stream_Write_UINT8(s, settings->SuppressOutput ? 1 : 0); /* suppressOutputSupport (1 byte) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_GENERAL);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_general_capability_set(wStream* s)
{
	UINT16 osMajorType = 0;
	UINT16 osMinorType = 0;
	UINT16 protocolVersion = 0;
	UINT16 pad2OctetsA = 0;
	UINT16 generalCompressionTypes = 0;
	UINT16 extraFlags = 0;
	UINT16 updateCapabilityFlag = 0;
	UINT16 remoteUnshareFlag = 0;
	UINT16 generalCompressionLevel = 0;
	BYTE refreshRectSupport = 0;
	BYTE suppressOutputSupport = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return FALSE;

	WLog_VRB(TAG, "GeneralCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));
	Stream_Read_UINT16(s, osMajorType);             /* osMajorType (2 bytes) */
	Stream_Read_UINT16(s, osMinorType);             /* osMinorType (2 bytes) */
	Stream_Read_UINT16(s, protocolVersion);         /* protocolVersion (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA);             /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT16(s, generalCompressionTypes); /* generalCompressionTypes (2 bytes) */
	Stream_Read_UINT16(s, extraFlags);              /* extraFlags (2 bytes) */
	Stream_Read_UINT16(s, updateCapabilityFlag);    /* updateCapabilityFlag (2 bytes) */
	Stream_Read_UINT16(s, remoteUnshareFlag);       /* remoteUnshareFlag (2 bytes) */
	Stream_Read_UINT16(s, generalCompressionLevel); /* generalCompressionLevel (2 bytes) */
	Stream_Read_UINT8(s, refreshRectSupport);       /* refreshRectSupport (1 byte) */
	Stream_Read_UINT8(s, suppressOutputSupport);    /* suppressOutputSupport (1 byte) */
	WLog_VRB(TAG, "\tosMajorType: 0x%04" PRIX16 "", osMajorType);
	WLog_VRB(TAG, "\tosMinorType: 0x%04" PRIX16 "", osMinorType);
	WLog_VRB(TAG, "\tprotocolVersion: 0x%04" PRIX16 "", protocolVersion);
	WLog_VRB(TAG, "\tpad2OctetsA: 0x%04" PRIX16 "", pad2OctetsA);
	WLog_VRB(TAG, "\tgeneralCompressionTypes: 0x%04" PRIX16 "", generalCompressionTypes);
	WLog_VRB(TAG, "\textraFlags: 0x%04" PRIX16 "", extraFlags);
	WLog_VRB(TAG, "\tupdateCapabilityFlag: 0x%04" PRIX16 "", updateCapabilityFlag);
	WLog_VRB(TAG, "\tremoteUnshareFlag: 0x%04" PRIX16 "", remoteUnshareFlag);
	WLog_VRB(TAG, "\tgeneralCompressionLevel: 0x%04" PRIX16 "", generalCompressionLevel);
	WLog_VRB(TAG, "\trefreshRectSupport: 0x%02" PRIX8 "", refreshRectSupport);
	WLog_VRB(TAG, "\tsuppressOutputSupport: 0x%02" PRIX8 "", suppressOutputSupport);
	return TRUE;
}
#endif
static BOOL rdp_apply_bitmap_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	if (!settings->ServerMode)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth,
		                                 freerdp_settings_get_uint32(src, FreeRDP_ColorDepth)))
			return FALSE;
	}

	if (!src->DesktopResize)
		settings->DesktopResize = FALSE;

	if (!settings->ServerMode && settings->DesktopResize)
	{
		/* The server may request a different desktop size during Deactivation-Reactivation sequence
		 */
		settings->DesktopWidth = src->DesktopWidth;
		settings->DesktopHeight = src->DesktopHeight;
	}

	if (settings->DrawAllowSkipAlpha)
		settings->DrawAllowSkipAlpha = src->DrawAllowSkipAlpha;

	if (settings->DrawAllowDynamicColorFidelity)
		settings->DrawAllowDynamicColorFidelity = src->DrawAllowDynamicColorFidelity;

	if (settings->DrawAllowColorSubsampling)
		settings->DrawAllowColorSubsampling = src->DrawAllowColorSubsampling;

	return TRUE;
}

/*
 * Read bitmap capability set.
 * msdn{cc240554}
 */

static BOOL rdp_read_bitmap_capability_set(wStream* s, rdpSettings* settings)
{
	BYTE drawingFlags = 0;
	UINT16 desktopWidth = 0;
	UINT16 desktopHeight = 0;
	UINT16 desktopResizeFlag = 0;
	UINT16 preferredBitsPerPixel = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 24))
		return FALSE;

	Stream_Read_UINT16(s, preferredBitsPerPixel); /* preferredBitsPerPixel (2 bytes) */
	Stream_Seek_UINT16(s);                        /* receive1BitPerPixel (2 bytes) */
	Stream_Seek_UINT16(s);                        /* receive4BitsPerPixel (2 bytes) */
	Stream_Seek_UINT16(s);                        /* receive8BitsPerPixel (2 bytes) */
	Stream_Read_UINT16(s, desktopWidth);          /* desktopWidth (2 bytes) */
	Stream_Read_UINT16(s, desktopHeight);         /* desktopHeight (2 bytes) */
	Stream_Seek_UINT16(s);                        /* pad2Octets (2 bytes) */
	Stream_Read_UINT16(s, desktopResizeFlag);     /* desktopResizeFlag (2 bytes) */
	Stream_Seek_UINT16(s);                        /* bitmapCompressionFlag (2 bytes) */
	Stream_Seek_UINT8(s);                         /* highColorFlags (1 byte) */
	Stream_Read_UINT8(s, drawingFlags);           /* drawingFlags (1 byte) */
	Stream_Seek_UINT16(s);                        /* multipleRectangleSupport (2 bytes) */
	Stream_Seek_UINT16(s);                        /* pad2OctetsB (2 bytes) */

	if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, preferredBitsPerPixel))
		return FALSE;
	settings->DesktopResize = desktopResizeFlag;
	settings->DesktopWidth = desktopWidth;
	settings->DesktopHeight = desktopHeight;
	settings->DrawAllowSkipAlpha = (drawingFlags & DRAW_ALLOW_SKIP_ALPHA) ? TRUE : FALSE;
	settings->DrawAllowDynamicColorFidelity =
	    (drawingFlags & DRAW_ALLOW_DYNAMIC_COLOR_FIDELITY) ? TRUE : FALSE;
	settings->DrawAllowColorSubsampling =
	    (drawingFlags & DRAW_ALLOW_COLOR_SUBSAMPLING) ? TRUE : FALSE;

	return TRUE;
}

/*
 * Write bitmap capability set.
 * msdn{cc240554}
 */

static BOOL rdp_write_bitmap_capability_set(wStream* s, const rdpSettings* settings)
{
	BYTE drawingFlags = 0;
	UINT16 preferredBitsPerPixel = 0;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);

	WINPR_ASSERT(settings);
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

	if ((freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth) > UINT16_MAX) ||
	    (settings->DesktopWidth > UINT16_MAX) || (settings->DesktopHeight > UINT16_MAX))
		return FALSE;

	if (settings->RdpVersion >= RDP_VERSION_5_PLUS)
		preferredBitsPerPixel = (UINT16)freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth);
	else
		preferredBitsPerPixel = 8;

	Stream_Write_UINT16(s, preferredBitsPerPixel);           /* preferredBitsPerPixel (2 bytes) */
	Stream_Write_UINT16(s, 1);                               /* receive1BitPerPixel (2 bytes) */
	Stream_Write_UINT16(s, 1);                               /* receive4BitsPerPixel (2 bytes) */
	Stream_Write_UINT16(s, 1);                               /* receive8BitsPerPixel (2 bytes) */
	Stream_Write_UINT16(s, (UINT16)settings->DesktopWidth);  /* desktopWidth (2 bytes) */
	Stream_Write_UINT16(s, (UINT16)settings->DesktopHeight); /* desktopHeight (2 bytes) */
	Stream_Write_UINT16(s, 0);                               /* pad2Octets (2 bytes) */
	Stream_Write_UINT16(s, (UINT16)settings->DesktopResize); /* desktopResizeFlag (2 bytes) */
	Stream_Write_UINT16(s, 1);                               /* bitmapCompressionFlag (2 bytes) */
	Stream_Write_UINT8(s, 0);                                /* highColorFlags (1 byte) */
	Stream_Write_UINT8(s, drawingFlags);                     /* drawingFlags (1 byte) */
	Stream_Write_UINT16(s, 1); /* multipleRectangleSupport (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2OctetsB (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_bitmap_capability_set(wStream* s)
{
	UINT16 preferredBitsPerPixel = 0;
	UINT16 receive1BitPerPixel = 0;
	UINT16 receive4BitsPerPixel = 0;
	UINT16 receive8BitsPerPixel = 0;
	UINT16 desktopWidth = 0;
	UINT16 desktopHeight = 0;
	UINT16 pad2Octets = 0;
	UINT16 desktopResizeFlag = 0;
	UINT16 bitmapCompressionFlag = 0;
	BYTE highColorFlags = 0;
	BYTE drawingFlags = 0;
	UINT16 multipleRectangleSupport = 0;
	UINT16 pad2OctetsB = 0;
	WLog_VRB(TAG, "BitmapCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 24))
		return FALSE;

	Stream_Read_UINT16(s, preferredBitsPerPixel);    /* preferredBitsPerPixel (2 bytes) */
	Stream_Read_UINT16(s, receive1BitPerPixel);      /* receive1BitPerPixel (2 bytes) */
	Stream_Read_UINT16(s, receive4BitsPerPixel);     /* receive4BitsPerPixel (2 bytes) */
	Stream_Read_UINT16(s, receive8BitsPerPixel);     /* receive8BitsPerPixel (2 bytes) */
	Stream_Read_UINT16(s, desktopWidth);             /* desktopWidth (2 bytes) */
	Stream_Read_UINT16(s, desktopHeight);            /* desktopHeight (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets);               /* pad2Octets (2 bytes) */
	Stream_Read_UINT16(s, desktopResizeFlag);        /* desktopResizeFlag (2 bytes) */
	Stream_Read_UINT16(s, bitmapCompressionFlag);    /* bitmapCompressionFlag (2 bytes) */
	Stream_Read_UINT8(s, highColorFlags);            /* highColorFlags (1 byte) */
	Stream_Read_UINT8(s, drawingFlags);              /* drawingFlags (1 byte) */
	Stream_Read_UINT16(s, multipleRectangleSupport); /* multipleRectangleSupport (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsB);              /* pad2OctetsB (2 bytes) */
	WLog_VRB(TAG, "\tpreferredBitsPerPixel: 0x%04" PRIX16 "", preferredBitsPerPixel);
	WLog_VRB(TAG, "\treceive1BitPerPixel: 0x%04" PRIX16 "", receive1BitPerPixel);
	WLog_VRB(TAG, "\treceive4BitsPerPixel: 0x%04" PRIX16 "", receive4BitsPerPixel);
	WLog_VRB(TAG, "\treceive8BitsPerPixel: 0x%04" PRIX16 "", receive8BitsPerPixel);
	WLog_VRB(TAG, "\tdesktopWidth: 0x%04" PRIX16 "", desktopWidth);
	WLog_VRB(TAG, "\tdesktopHeight: 0x%04" PRIX16 "", desktopHeight);
	WLog_VRB(TAG, "\tpad2Octets: 0x%04" PRIX16 "", pad2Octets);
	WLog_VRB(TAG, "\tdesktopResizeFlag: 0x%04" PRIX16 "", desktopResizeFlag);
	WLog_VRB(TAG, "\tbitmapCompressionFlag: 0x%04" PRIX16 "", bitmapCompressionFlag);
	WLog_VRB(TAG, "\thighColorFlags: 0x%02" PRIX8 "", highColorFlags);
	WLog_VRB(TAG, "\tdrawingFlags: 0x%02" PRIX8 "", drawingFlags);
	WLog_VRB(TAG, "\tmultipleRectangleSupport: 0x%04" PRIX16 "", multipleRectangleSupport);
	WLog_VRB(TAG, "\tpad2OctetsB: 0x%04" PRIX16 "", pad2OctetsB);
	return TRUE;
}
#endif
static BOOL rdp_apply_order_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	BOOL BitmapCacheV3Enabled = FALSE;
	BOOL FrameMarkerCommandEnabled = FALSE;

	for (size_t i = 0; i < 32; i++)
	{
		if (!src->OrderSupport[i])
			settings->OrderSupport[i] = FALSE;
	}

	if (settings->OrderSupportFlags & ORDER_FLAGS_EXTRA_SUPPORT)
	{
		if (src->OrderSupportFlagsEx & CACHE_BITMAP_V3_SUPPORT)
			BitmapCacheV3Enabled = TRUE;

		if (src->OrderSupportFlagsEx & ALTSEC_FRAME_MARKER_SUPPORT)
			FrameMarkerCommandEnabled = TRUE;
	}

	if (BitmapCacheV3Enabled)
	{
		settings->BitmapCacheV3Enabled = src->BitmapCacheV3Enabled;
		settings->BitmapCacheVersion = src->BitmapCacheVersion;
	}
	else
		settings->BitmapCacheV3Enabled = FALSE;

	if (FrameMarkerCommandEnabled && !src->FrameMarkerCommandEnabled)
		settings->FrameMarkerCommandEnabled = FALSE;

	return TRUE;
}

/*
 * Read order capability set.
 * msdn{cc240556}
 */

static BOOL rdp_read_order_capability_set(wStream* s, rdpSettings* settings)
{
	char terminalDescriptor[17] = { 0 };
	BYTE orderSupport[32] = { 0 };
	BOOL BitmapCacheV3Enabled = FALSE;
	BOOL FrameMarkerCommandEnabled = FALSE;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 84))
		return FALSE;

	Stream_Read(s, terminalDescriptor, 16);               /* terminalDescriptor (16 bytes) */
	Stream_Seek_UINT32(s);                                /* pad4OctetsA (4 bytes) */
	Stream_Seek_UINT16(s);                                /* desktopSaveXGranularity (2 bytes) */
	Stream_Seek_UINT16(s);                                /* desktopSaveYGranularity (2 bytes) */
	Stream_Seek_UINT16(s);                                /* pad2OctetsA (2 bytes) */
	Stream_Seek_UINT16(s);                                /* maximumOrderLevel (2 bytes) */
	Stream_Seek_UINT16(s);                                /* numberFonts (2 bytes) */
	Stream_Read_UINT16(s, settings->OrderSupportFlags);   /* orderFlags (2 bytes) */
	Stream_Read(s, orderSupport, 32);                     /* orderSupport (32 bytes) */
	Stream_Seek_UINT16(s);                                /* textFlags (2 bytes) */
	Stream_Read_UINT16(s, settings->OrderSupportFlagsEx); /* orderSupportExFlags (2 bytes) */
	Stream_Seek_UINT32(s);                                /* pad4OctetsB (4 bytes) */
	Stream_Seek_UINT32(s);                                /* desktopSaveSize (4 bytes) */
	Stream_Seek_UINT16(s);                                /* pad2OctetsC (2 bytes) */
	Stream_Seek_UINT16(s);                                /* pad2OctetsD (2 bytes) */
	Stream_Read_UINT16(s, settings->TextANSICodePage);    /* textANSICodePage (2 bytes) */
	Stream_Seek_UINT16(s);                                /* pad2OctetsE (2 bytes) */

	if (!freerdp_settings_set_string(settings, FreeRDP_TerminalDescriptor, terminalDescriptor))
		return FALSE;

	for (size_t i = 0; i < ARRAYSIZE(orderSupport); i++)
		settings->OrderSupport[i] = orderSupport[i];

	if (settings->OrderSupportFlags & ORDER_FLAGS_EXTRA_SUPPORT)
	{
		BitmapCacheV3Enabled =
		    (settings->OrderSupportFlagsEx & CACHE_BITMAP_V3_SUPPORT) ? TRUE : FALSE;
		FrameMarkerCommandEnabled =
		    (settings->OrderSupportFlagsEx & ALTSEC_FRAME_MARKER_SUPPORT) ? TRUE : FALSE;
	}

	settings->BitmapCacheV3Enabled = BitmapCacheV3Enabled;
	if (BitmapCacheV3Enabled)
		settings->BitmapCacheVersion = 3;

	settings->FrameMarkerCommandEnabled = FrameMarkerCommandEnabled;

	return TRUE;
}

/*
 * Write order capability set.
 * msdn{cc240556}
 */

static BOOL rdp_write_order_capability_set(wStream* s, const rdpSettings* settings)
{
	char terminalDescriptor[16] = { 0 };

	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);

	UINT16 orderSupportExFlags = settings->OrderSupportFlagsEx;
	UINT16 orderFlags = settings->OrderSupportFlags;

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

	const char* dsc = freerdp_settings_get_string(settings, FreeRDP_TerminalDescriptor);
	if (dsc)
	{
		const size_t len = strnlen(dsc, ARRAYSIZE(terminalDescriptor));
		strncpy(terminalDescriptor, dsc, len);
	}
	Stream_Write(s, terminalDescriptor,
	             sizeof(terminalDescriptor));           /* terminalDescriptor (16 bytes) */
	Stream_Write_UINT32(s, 0);                          /* pad4OctetsA (4 bytes) */
	Stream_Write_UINT16(s, 1);                          /* desktopSaveXGranularity (2 bytes) */
	Stream_Write_UINT16(s, 20);                         /* desktopSaveYGranularity (2 bytes) */
	Stream_Write_UINT16(s, 0);                          /* pad2OctetsA (2 bytes) */
	Stream_Write_UINT16(s, 1);                          /* maximumOrderLevel (2 bytes) */
	Stream_Write_UINT16(s, 0);                          /* numberFonts (2 bytes) */
	Stream_Write_UINT16(s, orderFlags);                 /* orderFlags (2 bytes) */
	Stream_Write(s, settings->OrderSupport, 32);        /* orderSupport (32 bytes) */
	Stream_Write_UINT16(s, 0);                          /* textFlags (2 bytes) */
	Stream_Write_UINT16(s, orderSupportExFlags);        /* orderSupportExFlags (2 bytes) */
	Stream_Write_UINT32(s, 0);                          /* pad4OctetsB (4 bytes) */
	Stream_Write_UINT32(s, 230400);                     /* desktopSaveSize (4 bytes) */
	Stream_Write_UINT16(s, 0);                          /* pad2OctetsC (2 bytes) */
	Stream_Write_UINT16(s, 0);                          /* pad2OctetsD (2 bytes) */
	Stream_Write_UINT16(s, settings->TextANSICodePage); /* textANSICodePage (2 bytes) */
	Stream_Write_UINT16(s, 0);                          /* pad2OctetsE (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_ORDER);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_order_capability_set(wStream* s)
{
	BYTE terminalDescriptor[16];
	UINT32 pad4OctetsA = 0;
	UINT16 desktopSaveXGranularity = 0;
	UINT16 desktopSaveYGranularity = 0;
	UINT16 pad2OctetsA = 0;
	UINT16 maximumOrderLevel = 0;
	UINT16 numberFonts = 0;
	UINT16 orderFlags = 0;
	BYTE orderSupport[32];
	UINT16 textFlags = 0;
	UINT16 orderSupportExFlags = 0;
	UINT32 pad4OctetsB = 0;
	UINT32 desktopSaveSize = 0;
	UINT16 pad2OctetsC = 0;
	UINT16 pad2OctetsD = 0;
	UINT16 textANSICodePage = 0;
	UINT16 pad2OctetsE = 0;
	WLog_VRB(TAG, "OrderCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 84))
		return FALSE;

	Stream_Read(s, terminalDescriptor, 16);         /* terminalDescriptor (16 bytes) */
	Stream_Read_UINT32(s, pad4OctetsA);             /* pad4OctetsA (4 bytes) */
	Stream_Read_UINT16(s, desktopSaveXGranularity); /* desktopSaveXGranularity (2 bytes) */
	Stream_Read_UINT16(s, desktopSaveYGranularity); /* desktopSaveYGranularity (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA);             /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT16(s, maximumOrderLevel);       /* maximumOrderLevel (2 bytes) */
	Stream_Read_UINT16(s, numberFonts);             /* numberFonts (2 bytes) */
	Stream_Read_UINT16(s, orderFlags);              /* orderFlags (2 bytes) */
	Stream_Read(s, orderSupport, 32);               /* orderSupport (32 bytes) */
	Stream_Read_UINT16(s, textFlags);               /* textFlags (2 bytes) */
	Stream_Read_UINT16(s, orderSupportExFlags);     /* orderSupportExFlags (2 bytes) */
	Stream_Read_UINT32(s, pad4OctetsB);             /* pad4OctetsB (4 bytes) */
	Stream_Read_UINT32(s, desktopSaveSize);         /* desktopSaveSize (4 bytes) */
	Stream_Read_UINT16(s, pad2OctetsC);             /* pad2OctetsC (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsD);             /* pad2OctetsD (2 bytes) */
	Stream_Read_UINT16(s, textANSICodePage);        /* textANSICodePage (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsE);             /* pad2OctetsE (2 bytes) */
	WLog_VRB(TAG, "\tpad4OctetsA: 0x%08" PRIX32 "", pad4OctetsA);
	WLog_VRB(TAG, "\tdesktopSaveXGranularity: 0x%04" PRIX16 "", desktopSaveXGranularity);
	WLog_VRB(TAG, "\tdesktopSaveYGranularity: 0x%04" PRIX16 "", desktopSaveYGranularity);
	WLog_VRB(TAG, "\tpad2OctetsA: 0x%04" PRIX16 "", pad2OctetsA);
	WLog_VRB(TAG, "\tmaximumOrderLevel: 0x%04" PRIX16 "", maximumOrderLevel);
	WLog_VRB(TAG, "\tnumberFonts: 0x%04" PRIX16 "", numberFonts);
	WLog_VRB(TAG, "\torderFlags: 0x%04" PRIX16 "", orderFlags);
	WLog_VRB(TAG, "\torderSupport:");
	WLog_VRB(TAG, "\t\tDSTBLT: %" PRIu8 "", orderSupport[NEG_DSTBLT_INDEX]);
	WLog_VRB(TAG, "\t\tPATBLT: %" PRIu8 "", orderSupport[NEG_PATBLT_INDEX]);
	WLog_VRB(TAG, "\t\tSCRBLT: %" PRIu8 "", orderSupport[NEG_SCRBLT_INDEX]);
	WLog_VRB(TAG, "\t\tMEMBLT: %" PRIu8 "", orderSupport[NEG_MEMBLT_INDEX]);
	WLog_VRB(TAG, "\t\tMEM3BLT: %" PRIu8 "", orderSupport[NEG_MEM3BLT_INDEX]);
	WLog_VRB(TAG, "\t\tATEXTOUT: %" PRIu8 "", orderSupport[NEG_ATEXTOUT_INDEX]);
	WLog_VRB(TAG, "\t\tAEXTTEXTOUT: %" PRIu8 "", orderSupport[NEG_AEXTTEXTOUT_INDEX]);
	WLog_VRB(TAG, "\t\tDRAWNINEGRID: %" PRIu8 "", orderSupport[NEG_DRAWNINEGRID_INDEX]);
	WLog_VRB(TAG, "\t\tLINETO: %" PRIu8 "", orderSupport[NEG_LINETO_INDEX]);
	WLog_VRB(TAG, "\t\tMULTI_DRAWNINEGRID: %" PRIu8 "", orderSupport[NEG_MULTI_DRAWNINEGRID_INDEX]);
	WLog_VRB(TAG, "\t\tOPAQUE_RECT: %" PRIu8 "", orderSupport[NEG_OPAQUE_RECT_INDEX]);
	WLog_VRB(TAG, "\t\tSAVEBITMAP: %" PRIu8 "", orderSupport[NEG_SAVEBITMAP_INDEX]);
	WLog_VRB(TAG, "\t\tWTEXTOUT: %" PRIu8 "", orderSupport[NEG_WTEXTOUT_INDEX]);
	WLog_VRB(TAG, "\t\tMEMBLT_V2: %" PRIu8 "", orderSupport[NEG_MEMBLT_V2_INDEX]);
	WLog_VRB(TAG, "\t\tMEM3BLT_V2: %" PRIu8 "", orderSupport[NEG_MEM3BLT_V2_INDEX]);
	WLog_VRB(TAG, "\t\tMULTIDSTBLT: %" PRIu8 "", orderSupport[NEG_MULTIDSTBLT_INDEX]);
	WLog_VRB(TAG, "\t\tMULTIPATBLT: %" PRIu8 "", orderSupport[NEG_MULTIPATBLT_INDEX]);
	WLog_VRB(TAG, "\t\tMULTISCRBLT: %" PRIu8 "", orderSupport[NEG_MULTISCRBLT_INDEX]);
	WLog_VRB(TAG, "\t\tMULTIOPAQUERECT: %" PRIu8 "", orderSupport[NEG_MULTIOPAQUERECT_INDEX]);
	WLog_VRB(TAG, "\t\tFAST_INDEX: %" PRIu8 "", orderSupport[NEG_FAST_INDEX_INDEX]);
	WLog_VRB(TAG, "\t\tPOLYGON_SC: %" PRIu8 "", orderSupport[NEG_POLYGON_SC_INDEX]);
	WLog_VRB(TAG, "\t\tPOLYGON_CB: %" PRIu8 "", orderSupport[NEG_POLYGON_CB_INDEX]);
	WLog_VRB(TAG, "\t\tPOLYLINE: %" PRIu8 "", orderSupport[NEG_POLYLINE_INDEX]);
	WLog_VRB(TAG, "\t\tUNUSED23: %" PRIu8 "", orderSupport[NEG_UNUSED23_INDEX]);
	WLog_VRB(TAG, "\t\tFAST_GLYPH: %" PRIu8 "", orderSupport[NEG_FAST_GLYPH_INDEX]);
	WLog_VRB(TAG, "\t\tELLIPSE_SC: %" PRIu8 "", orderSupport[NEG_ELLIPSE_SC_INDEX]);
	WLog_VRB(TAG, "\t\tELLIPSE_CB: %" PRIu8 "", orderSupport[NEG_ELLIPSE_CB_INDEX]);
	WLog_VRB(TAG, "\t\tGLYPH_INDEX: %" PRIu8 "", orderSupport[NEG_GLYPH_INDEX_INDEX]);
	WLog_VRB(TAG, "\t\tGLYPH_WEXTTEXTOUT: %" PRIu8 "", orderSupport[NEG_GLYPH_WEXTTEXTOUT_INDEX]);
	WLog_VRB(TAG, "\t\tGLYPH_WLONGTEXTOUT: %" PRIu8 "", orderSupport[NEG_GLYPH_WLONGTEXTOUT_INDEX]);
	WLog_VRB(TAG, "\t\tGLYPH_WLONGEXTTEXTOUT: %" PRIu8 "",
	         orderSupport[NEG_GLYPH_WLONGEXTTEXTOUT_INDEX]);
	WLog_VRB(TAG, "\t\tUNUSED31: %" PRIu8 "", orderSupport[NEG_UNUSED31_INDEX]);
	WLog_VRB(TAG, "\ttextFlags: 0x%04" PRIX16 "", textFlags);
	WLog_VRB(TAG, "\torderSupportExFlags: 0x%04" PRIX16 "", orderSupportExFlags);
	WLog_VRB(TAG, "\tpad4OctetsB: 0x%08" PRIX32 "", pad4OctetsB);
	WLog_VRB(TAG, "\tdesktopSaveSize: 0x%08" PRIX32 "", desktopSaveSize);
	WLog_VRB(TAG, "\tpad2OctetsC: 0x%04" PRIX16 "", pad2OctetsC);
	WLog_VRB(TAG, "\tpad2OctetsD: 0x%04" PRIX16 "", pad2OctetsD);
	WLog_VRB(TAG, "\ttextANSICodePage: 0x%04" PRIX16 "", textANSICodePage);
	WLog_VRB(TAG, "\tpad2OctetsE: 0x%04" PRIX16 "", pad2OctetsE);
	return TRUE;
}
#endif

static BOOL rdp_apply_bitmap_cache_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);
	return TRUE;
}

/*
 * Read bitmap cache capability set.
 * msdn{cc240559}
 */

static BOOL rdp_read_bitmap_cache_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 36))
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

/*
 * Write bitmap cache capability set.
 * msdn{cc240559}
 */

static BOOL rdp_write_bitmap_cache_capability_set(wStream* s, const rdpSettings* settings)
{
	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	const UINT32 bpp = (freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth) + 7) / 8;
	if (bpp > UINT16_MAX)
		return FALSE;
	Stream_Write_UINT32(s, 0); /* pad1 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad2 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad3 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad4 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad5 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* pad6 (4 bytes) */
	UINT32 size = bpp * 256;
	if (size > UINT16_MAX)
		return FALSE;
	Stream_Write_UINT16(s, 200);          /* Cache0Entries (2 bytes) */
	Stream_Write_UINT16(s, (UINT16)size); /* Cache0MaximumCellSize (2 bytes) */
	size = bpp * 1024;
	if (size > UINT16_MAX)
		return FALSE;
	Stream_Write_UINT16(s, 600);          /* Cache1Entries (2 bytes) */
	Stream_Write_UINT16(s, (UINT16)size); /* Cache1MaximumCellSize (2 bytes) */
	size = bpp * 4096;
	if (size > UINT16_MAX)
		return FALSE;
	Stream_Write_UINT16(s, 1000);         /* Cache2Entries (2 bytes) */
	Stream_Write_UINT16(s, (UINT16)size); /* Cache2MaximumCellSize (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_bitmap_cache_capability_set(wStream* s)
{
	UINT32 pad1 = 0;
	UINT32 pad2 = 0;
	UINT32 pad3 = 0;
	UINT32 pad4 = 0;
	UINT32 pad5 = 0;
	UINT32 pad6 = 0;
	UINT16 Cache0Entries = 0;
	UINT16 Cache0MaximumCellSize = 0;
	UINT16 Cache1Entries = 0;
	UINT16 Cache1MaximumCellSize = 0;
	UINT16 Cache2Entries = 0;
	UINT16 Cache2MaximumCellSize = 0;
	WLog_VRB(TAG, "BitmapCacheCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 36))
		return FALSE;

	Stream_Read_UINT32(s, pad1);                  /* pad1 (4 bytes) */
	Stream_Read_UINT32(s, pad2);                  /* pad2 (4 bytes) */
	Stream_Read_UINT32(s, pad3);                  /* pad3 (4 bytes) */
	Stream_Read_UINT32(s, pad4);                  /* pad4 (4 bytes) */
	Stream_Read_UINT32(s, pad5);                  /* pad5 (4 bytes) */
	Stream_Read_UINT32(s, pad6);                  /* pad6 (4 bytes) */
	Stream_Read_UINT16(s, Cache0Entries);         /* Cache0Entries (2 bytes) */
	Stream_Read_UINT16(s, Cache0MaximumCellSize); /* Cache0MaximumCellSize (2 bytes) */
	Stream_Read_UINT16(s, Cache1Entries);         /* Cache1Entries (2 bytes) */
	Stream_Read_UINT16(s, Cache1MaximumCellSize); /* Cache1MaximumCellSize (2 bytes) */
	Stream_Read_UINT16(s, Cache2Entries);         /* Cache2Entries (2 bytes) */
	Stream_Read_UINT16(s, Cache2MaximumCellSize); /* Cache2MaximumCellSize (2 bytes) */
	WLog_VRB(TAG, "\tpad1: 0x%08" PRIX32 "", pad1);
	WLog_VRB(TAG, "\tpad2: 0x%08" PRIX32 "", pad2);
	WLog_VRB(TAG, "\tpad3: 0x%08" PRIX32 "", pad3);
	WLog_VRB(TAG, "\tpad4: 0x%08" PRIX32 "", pad4);
	WLog_VRB(TAG, "\tpad5: 0x%08" PRIX32 "", pad5);
	WLog_VRB(TAG, "\tpad6: 0x%08" PRIX32 "", pad6);
	WLog_VRB(TAG, "\tCache0Entries: 0x%04" PRIX16 "", Cache0Entries);
	WLog_VRB(TAG, "\tCache0MaximumCellSize: 0x%04" PRIX16 "", Cache0MaximumCellSize);
	WLog_VRB(TAG, "\tCache1Entries: 0x%04" PRIX16 "", Cache1Entries);
	WLog_VRB(TAG, "\tCache1MaximumCellSize: 0x%04" PRIX16 "", Cache1MaximumCellSize);
	WLog_VRB(TAG, "\tCache2Entries: 0x%04" PRIX16 "", Cache2Entries);
	WLog_VRB(TAG, "\tCache2MaximumCellSize: 0x%04" PRIX16 "", Cache2MaximumCellSize);
	return TRUE;
}
#endif

static BOOL rdp_apply_control_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	return TRUE;
}

/*
 * Read control capability set.
 * msdn{cc240568}
 */

static BOOL rdp_read_control_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Seek_UINT16(s); /* controlFlags (2 bytes) */
	Stream_Seek_UINT16(s); /* remoteDetachFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* controlInterest (2 bytes) */
	Stream_Seek_UINT16(s); /* detachInterest (2 bytes) */
	return TRUE;
}

/*
 * Write control capability set.
 * msdn{cc240568}
 */

static BOOL rdp_write_control_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT16(s, 0); /* controlFlags (2 bytes) */
	Stream_Write_UINT16(s, 0); /* remoteDetachFlag (2 bytes) */
	Stream_Write_UINT16(s, 2); /* controlInterest (2 bytes) */
	Stream_Write_UINT16(s, 2); /* detachInterest (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_CONTROL);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_control_capability_set(wStream* s)
{
	UINT16 controlFlags = 0;
	UINT16 remoteDetachFlag = 0;
	UINT16 controlInterest = 0;
	UINT16 detachInterest = 0;
	WLog_VRB(TAG, "ControlCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT16(s, controlFlags);     /* controlFlags (2 bytes) */
	Stream_Read_UINT16(s, remoteDetachFlag); /* remoteDetachFlag (2 bytes) */
	Stream_Read_UINT16(s, controlInterest);  /* controlInterest (2 bytes) */
	Stream_Read_UINT16(s, detachInterest);   /* detachInterest (2 bytes) */
	WLog_VRB(TAG, "\tcontrolFlags: 0x%04" PRIX16 "", controlFlags);
	WLog_VRB(TAG, "\tremoteDetachFlag: 0x%04" PRIX16 "", remoteDetachFlag);
	WLog_VRB(TAG, "\tcontrolInterest: 0x%04" PRIX16 "", controlInterest);
	WLog_VRB(TAG, "\tdetachInterest: 0x%04" PRIX16 "", detachInterest);
	return TRUE;
}
#endif

static BOOL rdp_apply_window_activation_capability_set(rdpSettings* settings,
                                                       const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	return TRUE;
}

/*
 * Read window activation capability set.
 * msdn{cc240569}
 */

static BOOL rdp_read_window_activation_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Seek_UINT16(s); /* helpKeyFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* helpKeyIndexFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* helpExtendedKeyFlag (2 bytes) */
	Stream_Seek_UINT16(s); /* windowManagerKeyFlag (2 bytes) */
	return TRUE;
}

/*
 * Write window activation capability set.
 * msdn{cc240569}
 */

static BOOL rdp_write_window_activation_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT16(s, 0); /* helpKeyFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* helpKeyIndexFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* helpExtendedKeyFlag (2 bytes) */
	Stream_Write_UINT16(s, 0); /* windowManagerKeyFlag (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_ACTIVATION);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_window_activation_capability_set(wStream* s)
{
	UINT16 helpKeyFlag = 0;
	UINT16 helpKeyIndexFlag = 0;
	UINT16 helpExtendedKeyFlag = 0;
	UINT16 windowManagerKeyFlag = 0;
	WLog_VRB(TAG,
	         "WindowActivationCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT16(s, helpKeyFlag);          /* helpKeyFlag (2 bytes) */
	Stream_Read_UINT16(s, helpKeyIndexFlag);     /* helpKeyIndexFlag (2 bytes) */
	Stream_Read_UINT16(s, helpExtendedKeyFlag);  /* helpExtendedKeyFlag (2 bytes) */
	Stream_Read_UINT16(s, windowManagerKeyFlag); /* windowManagerKeyFlag (2 bytes) */
	WLog_VRB(TAG, "\thelpKeyFlag: 0x%04" PRIX16 "", helpKeyFlag);
	WLog_VRB(TAG, "\thelpKeyIndexFlag: 0x%04" PRIX16 "", helpKeyIndexFlag);
	WLog_VRB(TAG, "\thelpExtendedKeyFlag: 0x%04" PRIX16 "", helpExtendedKeyFlag);
	WLog_VRB(TAG, "\twindowManagerKeyFlag: 0x%04" PRIX16 "", windowManagerKeyFlag);
	return TRUE;
}
#endif

static BOOL rdp_apply_pointer_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	const UINT32 pointerCacheSize = freerdp_settings_get_uint32(src, FreeRDP_PointerCacheSize);
	const UINT32 colorPointerCacheSize =
	    freerdp_settings_get_uint32(src, FreeRDP_ColorPointerCacheSize);
	const UINT32 dstPointerCacheSize =
	    freerdp_settings_get_uint32(settings, FreeRDP_PointerCacheSize);
	const UINT32 dstColorPointerCacheSize =
	    freerdp_settings_get_uint32(settings, FreeRDP_ColorPointerCacheSize);

	/* We want the minimum of our setting and the remote announced value. */
	const UINT32 actualPointerCacheSize = MIN(pointerCacheSize, dstPointerCacheSize);
	const UINT32 actualColorPointerCacheSize = MIN(colorPointerCacheSize, dstColorPointerCacheSize);

	if (!freerdp_settings_set_uint32(settings, FreeRDP_PointerCacheSize, actualPointerCacheSize) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ColorPointerCacheSize,
	                                 actualColorPointerCacheSize))
		return FALSE;

	return TRUE;
}

/*
 * Read pointer capability set.
 * msdn{cc240562}
 */

static BOOL rdp_read_pointer_capability_set(wStream* s, rdpSettings* settings)
{
	UINT16 colorPointerFlag = 0;
	UINT16 colorPointerCacheSize = 0;
	UINT16 pointerCacheSize = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, colorPointerFlag);      /* colorPointerFlag (2 bytes) */
	Stream_Read_UINT16(s, colorPointerCacheSize); /* colorPointerCacheSize (2 bytes) */

	if (colorPointerFlag == 0)
	{
		WLog_WARN(TAG, "[MS-RDPBCGR] 2.2.7.1.5 Pointer Capability Set "
		               "(TS_POINTER_CAPABILITYSET)::colorPointerFlag received is %" PRIu16
		               ". Vaue is ignored and always assumed to be TRUE");
	}

	/* pointerCacheSize is optional */
	if (Stream_GetRemainingLength(s) >= 2)
		Stream_Read_UINT16(s, pointerCacheSize); /* pointerCacheSize (2 bytes) */

	WINPR_ASSERT(settings);
	settings->PointerCacheSize = pointerCacheSize;
	settings->ColorPointerCacheSize = colorPointerCacheSize;

	return TRUE;
}

/*
 * Write pointer capability set.
 * msdn{cc240562}
 */

static BOOL rdp_write_pointer_capability_set(wStream* s, const rdpSettings* settings)
{
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	if (settings->PointerCacheSize > UINT16_MAX)
		return FALSE;
	if (settings->ColorPointerCacheSize > UINT16_MAX)
		return FALSE;

	WINPR_ASSERT(settings);
	const UINT32 colorPointerFlag =
	    1; /* [MS-RDPBCGR] 2.2.7.1.5 Pointer Capability Set (TS_POINTER_CAPABILITYSET)
	        * colorPointerFlag is ignored and always assumed to be TRUE */
	Stream_Write_UINT16(s, colorPointerFlag); /* colorPointerFlag (2 bytes) */
	Stream_Write_UINT16(
	    s, (UINT16)settings->ColorPointerCacheSize); /* colorPointerCacheSize (2 bytes) */
	Stream_Write_UINT16(s, (UINT16)settings->PointerCacheSize); /* pointerCacheSize (2 bytes) */

	return rdp_capability_set_finish(s, header, CAPSET_TYPE_POINTER);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_pointer_capability_set(wStream* s)
{
	UINT16 colorPointerFlag = 0;
	UINT16 colorPointerCacheSize = 0;
	UINT16 pointerCacheSize = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	WLog_VRB(TAG, "PointerCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));
	Stream_Read_UINT16(s, colorPointerFlag);      /* colorPointerFlag (2 bytes) */
	Stream_Read_UINT16(s, colorPointerCacheSize); /* colorPointerCacheSize (2 bytes) */
	Stream_Read_UINT16(s, pointerCacheSize);      /* pointerCacheSize (2 bytes) */
	WLog_VRB(TAG, "\tcolorPointerFlag: 0x%04" PRIX16 "", colorPointerFlag);
	WLog_VRB(TAG, "\tcolorPointerCacheSize: 0x%04" PRIX16 "", colorPointerCacheSize);
	WLog_VRB(TAG, "\tpointerCacheSize: 0x%04" PRIX16 "", pointerCacheSize);
	return TRUE;
}
#endif

static BOOL rdp_apply_share_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	return TRUE;
}

/*
 * Read share capability set.
 * msdn{cc240570}
 */

static BOOL rdp_read_share_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Seek_UINT16(s); /* nodeId (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */
	return TRUE;
}

/*
 * Write share capability set.
 * msdn{cc240570}
 */

static BOOL rdp_write_share_capability_set(wStream* s, const rdpSettings* settings)
{
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);

	WINPR_ASSERT(settings);
	const UINT16 nodeId = (settings->ServerMode) ? 0x03EA : 0;
	Stream_Write_UINT16(s, nodeId); /* nodeId (2 bytes) */
	Stream_Write_UINT16(s, 0);      /* pad2Octets (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_SHARE);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_share_capability_set(wStream* s)
{
	UINT16 nodeId = 0;
	UINT16 pad2Octets = 0;
	WLog_VRB(TAG, "ShareCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, nodeId);     /* nodeId (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */
	WLog_VRB(TAG, "\tnodeId: 0x%04" PRIX16 "", nodeId);
	WLog_VRB(TAG, "\tpad2Octets: 0x%04" PRIX16 "", pad2Octets);
	return TRUE;
}
#endif

static BOOL rdp_apply_color_cache_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);
	return TRUE;
}

/*
 * Read color cache capability set.
 * msdn{cc241564}
 */

static BOOL rdp_read_color_cache_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Seek_UINT16(s); /* colorTableCacheSize (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */
	return TRUE;
}

/*
 * Write color cache capability set.
 * msdn{cc241564}
 */

static BOOL rdp_write_color_cache_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT16(s, 6); /* colorTableCacheSize (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_COLOR_CACHE);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_color_cache_capability_set(wStream* s)
{
	UINT16 colorTableCacheSize = 0;
	UINT16 pad2Octets = 0;
	WLog_VRB(TAG, "ColorCacheCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, colorTableCacheSize); /* colorTableCacheSize (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets);          /* pad2Octets (2 bytes) */
	WLog_VRB(TAG, "\tcolorTableCacheSize: 0x%04" PRIX16 "", colorTableCacheSize);
	WLog_VRB(TAG, "\tpad2Octets: 0x%04" PRIX16 "", pad2Octets);
	return TRUE;
}
#endif

static BOOL rdp_apply_sound_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	settings->SoundBeepsEnabled = src->SoundBeepsEnabled;

	return TRUE;
}

/*
 * Read sound capability set.
 * msdn{cc240552}
 */

static BOOL rdp_read_sound_capability_set(wStream* s, rdpSettings* settings)
{
	UINT16 soundFlags = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, soundFlags); /* soundFlags (2 bytes) */
	Stream_Seek_UINT16(s);             /* pad2OctetsA (2 bytes) */
	settings->SoundBeepsEnabled = (soundFlags & SOUND_BEEPS_FLAG) ? TRUE : FALSE;
	return TRUE;
}

/*
 * Write sound capability set.
 * msdn{cc240552}
 */

static BOOL rdp_write_sound_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	const UINT16 soundFlags = (settings->SoundBeepsEnabled) ? SOUND_BEEPS_FLAG : 0;
	Stream_Write_UINT16(s, soundFlags); /* soundFlags (2 bytes) */
	Stream_Write_UINT16(s, 0);          /* pad2OctetsA (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_SOUND);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_sound_capability_set(wStream* s)
{
	UINT16 soundFlags = 0;
	UINT16 pad2OctetsA = 0;
	WLog_VRB(TAG, "SoundCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, soundFlags);  /* soundFlags (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA); /* pad2OctetsA (2 bytes) */
	WLog_VRB(TAG, "\tsoundFlags: 0x%04" PRIX16 "", soundFlags);
	WLog_VRB(TAG, "\tpad2OctetsA: 0x%04" PRIX16 "", pad2OctetsA);
	return TRUE;
}
#endif

static BOOL rdp_apply_input_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	if (settings->ServerMode)
	{
		settings->KeyboardLayout = src->KeyboardLayout;
		settings->KeyboardType = src->KeyboardType;
		settings->KeyboardSubType = src->KeyboardSubType;
		settings->KeyboardFunctionKey = src->KeyboardFunctionKey;
	}

	if (!freerdp_settings_set_string(settings, FreeRDP_ImeFileName, src->ImeFileName))
		return FALSE;

	if (!settings->ServerMode)
	{
		settings->FastPathInput = src->FastPathInput;

		/* Note: These settings have split functionality:
		 * 1. If disabled in client pre_connect, it can disable announcing the feature
		 * 2. If enabled in client pre_connect, override it with the server announced support flag.
		 */
		if (settings->HasHorizontalWheel)
			settings->HasHorizontalWheel = src->HasHorizontalWheel;
		const BOOL UnicodeInput = freerdp_settings_get_bool(settings, FreeRDP_UnicodeInput);
		if (UnicodeInput)
		{
			const BOOL srcVal = freerdp_settings_get_bool(settings, FreeRDP_UnicodeInput);
			if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, srcVal))
				return FALSE;
		}
		if (settings->HasExtendedMouseEvent)
			settings->HasExtendedMouseEvent = src->HasExtendedMouseEvent;
		if (settings->HasRelativeMouseEvent)
			settings->HasRelativeMouseEvent = src->HasRelativeMouseEvent;
		if (freerdp_settings_get_bool(settings, FreeRDP_HasQoeEvent))
			settings->HasQoeEvent = freerdp_settings_get_bool(settings, FreeRDP_HasQoeEvent);
	}
	return TRUE;
}

/*
 * Read input capability set.
 * msdn{cc240563}
 */

static BOOL rdp_read_input_capability_set(wStream* s, rdpSettings* settings)
{
	UINT16 inputFlags = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 84))
		return FALSE;

	Stream_Read_UINT16(s, inputFlags); /* inputFlags (2 bytes) */
	Stream_Seek_UINT16(s);             /* pad2OctetsA (2 bytes) */

	Stream_Read_UINT32(s, settings->KeyboardLayout);      /* keyboardLayout (4 bytes) */
	Stream_Read_UINT32(s, settings->KeyboardType);        /* keyboardType (4 bytes) */
	Stream_Read_UINT32(s, settings->KeyboardSubType);     /* keyboardSubType (4 bytes) */
	Stream_Read_UINT32(s, settings->KeyboardFunctionKey); /* keyboardFunctionKeys (4 bytes) */

	{
		WCHAR wstr[32] = { 0 };
		char str[65] = { 0 };

		/* Older windows versions report invalid UTF16
		 * [MS-RDPBCGR] <29> Section 2.2.7.1.6: Microsoft RDP 4.0, 5.0, 5.1, and 5.2 servers do not
		 * explicitly fill the imeFileName field with zeros.
		 */
		if (!Stream_Read_UTF16_String(s, wstr, ARRAYSIZE(wstr)))
			return FALSE;

		if (ConvertWCharNToUtf8(wstr, ARRAYSIZE(wstr), str, ARRAYSIZE(str)) < 0)
			memset(str, 0, sizeof(str));

		if (!freerdp_settings_set_string_len(settings, FreeRDP_ImeFileName, str, ARRAYSIZE(str)))
			return FALSE;
	}

	if (!freerdp_settings_set_bool(settings, FreeRDP_FastPathInput,
	                               inputFlags &
	                                   (INPUT_FLAG_FASTPATH_INPUT | INPUT_FLAG_FASTPATH_INPUT2)))
		return FALSE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_HasHorizontalWheel,
	                               (inputFlags & TS_INPUT_FLAG_MOUSE_HWHEEL) ? TRUE : FALSE))
		return FALSE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput,
	                               (inputFlags & INPUT_FLAG_UNICODE) ? TRUE : FALSE))
		return FALSE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_HasRelativeMouseEvent,
	                               (inputFlags & INPUT_FLAG_MOUSE_RELATIVE) ? TRUE : FALSE))
		return FALSE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_HasExtendedMouseEvent,
	                               (inputFlags & INPUT_FLAG_MOUSEX) ? TRUE : FALSE))
		return FALSE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_HasQoeEvent,
	                               (inputFlags & TS_INPUT_FLAG_QOE_TIMESTAMPS) ? TRUE : FALSE))
		return FALSE;

	return TRUE;
}

/*
 * Write input capability set.
 * msdn{cc240563}
 */

static BOOL rdp_write_input_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 128))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	UINT16 inputFlags = INPUT_FLAG_SCANCODES;

	if (settings->FastPathInput)
	{
		inputFlags |= INPUT_FLAG_FASTPATH_INPUT;
		inputFlags |= INPUT_FLAG_FASTPATH_INPUT2;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_HasRelativeMouseEvent))
		inputFlags |= INPUT_FLAG_MOUSE_RELATIVE;

	if (freerdp_settings_get_bool(settings, FreeRDP_HasHorizontalWheel))
		inputFlags |= TS_INPUT_FLAG_MOUSE_HWHEEL;

	if (freerdp_settings_get_bool(settings, FreeRDP_UnicodeInput))
		inputFlags |= INPUT_FLAG_UNICODE;

	if (freerdp_settings_get_bool(settings, FreeRDP_HasQoeEvent))
		inputFlags |= TS_INPUT_FLAG_QOE_TIMESTAMPS;

	if (settings->HasExtendedMouseEvent)
		inputFlags |= INPUT_FLAG_MOUSEX;

	Stream_Write_UINT16(s, inputFlags);                    /* inputFlags (2 bytes) */
	Stream_Write_UINT16(s, 0);                             /* pad2OctetsA (2 bytes) */
	Stream_Write_UINT32(s, settings->KeyboardLayout);      /* keyboardLayout (4 bytes) */
	Stream_Write_UINT32(s, settings->KeyboardType);        /* keyboardType (4 bytes) */
	Stream_Write_UINT32(s, settings->KeyboardSubType);     /* keyboardSubType (4 bytes) */
	Stream_Write_UINT32(s, settings->KeyboardFunctionKey); /* keyboardFunctionKeys (4 bytes) */
	Stream_Zero(s, 64);                                    /* imeFileName (64 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_INPUT);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_input_capability_set(wStream* s)
{
	UINT16 inputFlags = 0;
	UINT16 pad2OctetsA = 0;
	UINT32 keyboardLayout = 0;
	UINT32 keyboardType = 0;
	UINT32 keyboardSubType = 0;
	UINT32 keyboardFunctionKey = 0;
	WLog_VRB(TAG, "InputCapabilitySet (length %" PRIuz ")", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 84))
		return FALSE;

	Stream_Read_UINT16(s, inputFlags);          /* inputFlags (2 bytes) */
	Stream_Read_UINT16(s, pad2OctetsA);         /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT32(s, keyboardLayout);      /* keyboardLayout (4 bytes) */
	Stream_Read_UINT32(s, keyboardType);        /* keyboardType (4 bytes) */
	Stream_Read_UINT32(s, keyboardSubType);     /* keyboardSubType (4 bytes) */
	Stream_Read_UINT32(s, keyboardFunctionKey); /* keyboardFunctionKeys (4 bytes) */
	Stream_Seek(s, 64);                         /* imeFileName (64 bytes) */
	WLog_VRB(TAG, "\tinputFlags: 0x%04" PRIX16 "", inputFlags);
	WLog_VRB(TAG, "\tpad2OctetsA: 0x%04" PRIX16 "", pad2OctetsA);
	WLog_VRB(TAG, "\tkeyboardLayout: 0x%08" PRIX32 "", keyboardLayout);
	WLog_VRB(TAG, "\tkeyboardType: 0x%08" PRIX32 "", keyboardType);
	WLog_VRB(TAG, "\tkeyboardSubType: 0x%08" PRIX32 "", keyboardSubType);
	WLog_VRB(TAG, "\tkeyboardFunctionKey: 0x%08" PRIX32 "", keyboardFunctionKey);
	return TRUE;
}
#endif

static BOOL rdp_apply_font_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);
	return TRUE;
}

/*
 * Read font capability set.
 * msdn{cc240571}
 */

static BOOL rdp_read_font_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	if (Stream_GetRemainingLength(s) >= 2)
		Stream_Seek_UINT16(s); /* fontSupportFlags (2 bytes) */

	if (Stream_GetRemainingLength(s) >= 2)
		Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */

	return TRUE;
}

/*
 * Write font capability set.
 * msdn{cc240571}
 */

static BOOL rdp_write_font_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT16(s, FONTSUPPORT_FONTLIST); /* fontSupportFlags (2 bytes) */
	Stream_Write_UINT16(s, 0);                    /* pad2Octets (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_FONT);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_font_capability_set(wStream* s)
{
	UINT16 fontSupportFlags = 0;
	UINT16 pad2Octets = 0;
	WLog_VRB(TAG, "FontCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (Stream_GetRemainingLength(s) >= 2)
		Stream_Read_UINT16(s, fontSupportFlags); /* fontSupportFlags (2 bytes) */

	if (Stream_GetRemainingLength(s) >= 2)
		Stream_Read_UINT16(s, pad2Octets); /* pad2Octets (2 bytes) */

	WLog_VRB(TAG, "\tfontSupportFlags: 0x%04" PRIX16 "", fontSupportFlags);
	WLog_VRB(TAG, "\tpad2Octets: 0x%04" PRIX16 "", pad2Octets);
	return TRUE;
}
#endif

static BOOL rdp_apply_brush_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	// TODO: Minimum of what?
	settings->BrushSupportLevel = src->BrushSupportLevel;
	return TRUE;
}

/*
 * Read brush capability set.
 * msdn{cc240564}
 */

static BOOL rdp_read_brush_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;
	Stream_Read_UINT32(s, settings->BrushSupportLevel); /* brushSupportLevel (4 bytes) */
	return TRUE;
}

/*
 * Write brush capability set.
 * msdn{cc240564}
 */

static BOOL rdp_write_brush_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT32(s, settings->BrushSupportLevel); /* brushSupportLevel (4 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_BRUSH);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_brush_capability_set(wStream* s)
{
	UINT32 brushSupportLevel = 0;
	WLog_VRB(TAG, "BrushCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, brushSupportLevel); /* brushSupportLevel (4 bytes) */
	WLog_VRB(TAG, "\tbrushSupportLevel: 0x%08" PRIX32 "", brushSupportLevel);
	return TRUE;
}
#endif

/*
 * Read cache definition (glyph).
 * msdn{cc240566}
 */
static void rdp_read_cache_definition(wStream* s, GLYPH_CACHE_DEFINITION* cache_definition)
{
	WINPR_ASSERT(cache_definition);
	Stream_Read_UINT16(s, cache_definition->cacheEntries); /* cacheEntries (2 bytes) */
	Stream_Read_UINT16(s,
	                   cache_definition->cacheMaximumCellSize); /* cacheMaximumCellSize (2 bytes) */
}

/*
 * Write cache definition (glyph).
 * msdn{cc240566}
 */
static void rdp_write_cache_definition(wStream* s, GLYPH_CACHE_DEFINITION* cache_definition)
{
	WINPR_ASSERT(cache_definition);
	Stream_Write_UINT16(s, cache_definition->cacheEntries); /* cacheEntries (2 bytes) */
	Stream_Write_UINT16(
	    s, cache_definition->cacheMaximumCellSize); /* cacheMaximumCellSize (2 bytes) */
}

static BOOL rdp_apply_glyph_cache_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	WINPR_ASSERT(src->GlyphCache);
	WINPR_ASSERT(settings->GlyphCache);
	for (size_t x = 0; x < 10; x++)
		settings->GlyphCache[x] = src->GlyphCache[x];

	WINPR_ASSERT(src->FragCache);
	WINPR_ASSERT(settings->FragCache);
	settings->FragCache[0] = src->FragCache[0];
	settings->GlyphSupportLevel = src->GlyphSupportLevel;

	return TRUE;
}

/*
 * Read glyph cache capability set.
 * msdn{cc240565}
 */

static BOOL rdp_read_glyph_cache_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 48))
		return FALSE;

	/* glyphCache (40 bytes) */
	for (size_t x = 0; x < 10; x++)
		rdp_read_cache_definition(s, &(settings->GlyphCache[x])); /* glyphCache0 (4 bytes) */
	rdp_read_cache_definition(s, settings->FragCache);            /* fragCache (4 bytes) */
	Stream_Read_UINT16(s, settings->GlyphSupportLevel);           /* glyphSupportLevel (2 bytes) */
	Stream_Seek_UINT16(s);                                        /* pad2Octets (2 bytes) */
	return TRUE;
}

/*
 * Write glyph cache capability set.
 * msdn{cc240565}
 */

static BOOL rdp_write_glyph_cache_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	if (settings->GlyphSupportLevel > UINT16_MAX)
		return FALSE;
	/* glyphCache (40 bytes) */
	for (size_t x = 0; x < 10; x++)
		rdp_write_cache_definition(s, &(settings->GlyphCache[x])); /* glyphCache0 (4 bytes) */
	rdp_write_cache_definition(s, settings->FragCache);            /* fragCache (4 bytes) */
	Stream_Write_UINT16(s, (UINT16)settings->GlyphSupportLevel);   /* glyphSupportLevel (2 bytes) */
	Stream_Write_UINT16(s, 0);                                     /* pad2Octets (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_GLYPH_CACHE);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_glyph_cache_capability_set(wStream* s)
{
	GLYPH_CACHE_DEFINITION glyphCache[10] = { 0 };
	GLYPH_CACHE_DEFINITION fragCache = { 0 };
	UINT16 glyphSupportLevel = 0;
	UINT16 pad2Octets = 0;
	WLog_VRB(TAG, "GlyphCacheCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 48))
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
	rdp_read_cache_definition(s, &fragCache);     /* fragCache (4 bytes) */
	Stream_Read_UINT16(s, glyphSupportLevel);     /* glyphSupportLevel (2 bytes) */
	Stream_Read_UINT16(s, pad2Octets);            /* pad2Octets (2 bytes) */
	WLog_VRB(TAG, "\tglyphCache0: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[0].cacheEntries, glyphCache[0].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphCache1: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[1].cacheEntries, glyphCache[1].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphCache2: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[2].cacheEntries, glyphCache[2].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphCache3: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[3].cacheEntries, glyphCache[3].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphCache4: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[4].cacheEntries, glyphCache[4].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphCache5: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[5].cacheEntries, glyphCache[5].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphCache6: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[6].cacheEntries, glyphCache[6].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphCache7: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[7].cacheEntries, glyphCache[7].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphCache8: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[8].cacheEntries, glyphCache[8].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphCache9: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         glyphCache[9].cacheEntries, glyphCache[9].cacheMaximumCellSize);
	WLog_VRB(TAG, "\tfragCache: Entries: %" PRIu16 " MaximumCellSize: %" PRIu16 "",
	         fragCache.cacheEntries, fragCache.cacheMaximumCellSize);
	WLog_VRB(TAG, "\tglyphSupportLevel: 0x%04" PRIX16 "", glyphSupportLevel);
	WLog_VRB(TAG, "\tpad2Octets: 0x%04" PRIX16 "", pad2Octets);
	return TRUE;
}
#endif

static BOOL rdp_apply_offscreen_bitmap_cache_capability_set(rdpSettings* settings,
                                                            const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	settings->OffscreenCacheSize = src->OffscreenCacheSize;
	settings->OffscreenCacheEntries = src->OffscreenCacheEntries;
	settings->OffscreenSupportLevel = src->OffscreenSupportLevel;

	return TRUE;
}

/*
 * Read offscreen bitmap cache capability set.
 * msdn{cc240550}
 */

static BOOL rdp_read_offscreen_bitmap_cache_capability_set(wStream* s, rdpSettings* settings)
{
	UINT32 offscreenSupportLevel = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, offscreenSupportLevel);           /* offscreenSupportLevel (4 bytes) */
	Stream_Read_UINT16(s, settings->OffscreenCacheSize);    /* offscreenCacheSize (2 bytes) */
	Stream_Read_UINT16(s, settings->OffscreenCacheEntries); /* offscreenCacheEntries (2 bytes) */

	settings->OffscreenSupportLevel = offscreenSupportLevel & 0x01;

	return TRUE;
}

/*
 * Write offscreen bitmap cache capability set.
 * msdn{cc240550}
 */

static BOOL rdp_write_offscreen_bitmap_cache_capability_set(wStream* s, const rdpSettings* settings)
{
	UINT32 offscreenSupportLevel = 0x00;

	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	if (settings->OffscreenSupportLevel)
	{
		offscreenSupportLevel = 0x01;
		Stream_Write_UINT32(s, offscreenSupportLevel);        /* offscreenSupportLevel (4 bytes) */
		Stream_Write_UINT16(s, settings->OffscreenCacheSize); /* offscreenCacheSize (2 bytes) */
		Stream_Write_UINT16(s,
		                    settings->OffscreenCacheEntries); /* offscreenCacheEntries (2 bytes) */
	}
	else
		Stream_Zero(s, 8);

	return rdp_capability_set_finish(s, header, CAPSET_TYPE_OFFSCREEN_CACHE);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_offscreen_bitmap_cache_capability_set(wStream* s)
{
	UINT32 offscreenSupportLevel = 0;
	UINT16 offscreenCacheSize = 0;
	UINT16 offscreenCacheEntries = 0;
	WLog_VRB(TAG, "OffscreenBitmapCacheCapabilitySet (length %" PRIuz "):",
	         Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, offscreenSupportLevel); /* offscreenSupportLevel (4 bytes) */
	Stream_Read_UINT16(s, offscreenCacheSize);    /* offscreenCacheSize (2 bytes) */
	Stream_Read_UINT16(s, offscreenCacheEntries); /* offscreenCacheEntries (2 bytes) */
	WLog_VRB(TAG, "\toffscreenSupportLevel: 0x%08" PRIX32 "", offscreenSupportLevel);
	WLog_VRB(TAG, "\toffscreenCacheSize: 0x%04" PRIX16 "", offscreenCacheSize);
	WLog_VRB(TAG, "\toffscreenCacheEntries: 0x%04" PRIX16 "", offscreenCacheEntries);
	return TRUE;
}
#endif

static BOOL rdp_apply_bitmap_cache_host_support_capability_set(rdpSettings* settings,
                                                               const rdpSettings* src)
{
	const BOOL val = (freerdp_settings_get_bool(src, FreeRDP_BitmapCachePersistEnabled) &&
	                  freerdp_settings_get_bool(settings, FreeRDP_BitmapCachePersistEnabled));
	return freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled, val);
}

/*
 * Read bitmap cache host support capability set.
 * msdn{cc240557}
 */

static BOOL rdp_read_bitmap_cache_host_support_capability_set(wStream* s, rdpSettings* settings)
{
	BYTE cacheVersion = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT8(s, cacheVersion); /* cacheVersion (1 byte) */
	Stream_Seek_UINT8(s);               /* pad1 (1 byte) */
	Stream_Seek_UINT16(s);              /* pad2 (2 bytes) */

	return freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled,
	                                 cacheVersion & BITMAP_CACHE_V2);
}

/*
 * Write bitmap cache host support capability set.
 * msdn{cc240557}
 */

static BOOL rdp_write_bitmap_cache_host_support_capability_set(wStream* s,
                                                               const rdpSettings* settings)
{
	UINT8 cacheVersion = 0;

	if (freerdp_settings_get_bool(settings, FreeRDP_BitmapCacheEnabled))
		cacheVersion |= BITMAP_CACHE_V2;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT8(s, cacheVersion); /* cacheVersion (1 byte) */
	Stream_Write_UINT8(s, 0);            /* pad1 (1 byte) */
	Stream_Write_UINT16(s, 0);           /* pad2 (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_bitmap_cache_host_support_capability_set(wStream* s)
{
	BYTE cacheVersion = 0;
	BYTE pad1 = 0;
	UINT16 pad2 = 0;
	WLog_VRB(TAG, "BitmapCacheHostSupportCapabilitySet (length %" PRIuz "):",
	         Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT8(s, cacheVersion); /* cacheVersion (1 byte) */
	Stream_Read_UINT8(s, pad1);         /* pad1 (1 byte) */
	Stream_Read_UINT16(s, pad2);        /* pad2 (2 bytes) */
	WLog_VRB(TAG, "\tcacheVersion: 0x%02" PRIX8 "", cacheVersion);
	WLog_VRB(TAG, "\tpad1: 0x%02" PRIX8 "", pad1);
	WLog_VRB(TAG, "\tpad2: 0x%04" PRIX16 "", pad2);
	return TRUE;
}
#endif

static BOOL rdp_read_bitmap_cache_cell_info(wStream* s, BITMAP_CACHE_V2_CELL_INFO* cellInfo)
{
	UINT32 info = 0;

	WINPR_ASSERT(cellInfo);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	/*
	 * numEntries is in the first 31 bits, while the last bit (k)
	 * is used to indicate a persistent bitmap cache.
	 */
	Stream_Read_UINT32(s, info);
	cellInfo->numEntries = (info & 0x7FFFFFFF);
	cellInfo->persistent = (info & 0x80000000) ? 1 : 0;
	return TRUE;
}

static void rdp_write_bitmap_cache_cell_info(wStream* s, BITMAP_CACHE_V2_CELL_INFO* cellInfo)
{
	UINT32 info = 0;
	/*
	 * numEntries is in the first 31 bits, while the last bit (k)
	 * is used to indicate a persistent bitmap cache.
	 */
	WINPR_ASSERT(cellInfo);
	info = (cellInfo->numEntries | (cellInfo->persistent << 31));
	Stream_Write_UINT32(s, info);
}

static BOOL rdp_apply_bitmap_cache_v2_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	const FreeRDP_Settings_Keys_Bool keys[] = { FreeRDP_BitmapCacheEnabled,
		                                        FreeRDP_BitmapCachePersistEnabled };

	for (size_t x = 0; x < ARRAYSIZE(keys); x++)
	{
		const FreeRDP_Settings_Keys_Bool id = keys[x];
		const BOOL val = freerdp_settings_get_bool(src, id);
		if (!freerdp_settings_set_bool(settings, id, val))
			return FALSE;
	}

	{
		const UINT32 BitmapCacheV2NumCells =
		    freerdp_settings_get_uint32(src, FreeRDP_BitmapCacheV2NumCells);
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_BitmapCacheV2CellInfo, NULL,
		                                      BitmapCacheV2NumCells))
			return FALSE;

		for (size_t x = 0; x < BitmapCacheV2NumCells; x++)
		{
			const BITMAP_CACHE_V2_CELL_INFO* cdata =
			    freerdp_settings_get_pointer_array(src, FreeRDP_BitmapCacheV2CellInfo, x);
			if (!freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, x,
			                                        cdata))
				return FALSE;
		}
	}

	return TRUE;
}

/*
 * Read bitmap cache v2 capability set.
 * msdn{cc240560}
 */

static BOOL rdp_read_bitmap_cache_v2_capability_set(wStream* s, rdpSettings* settings)
{
	UINT16 cacheFlags = 0;
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 36))
		return FALSE;

	Stream_Read_UINT16(s, cacheFlags); /* cacheFlags (2 bytes) */

	if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, TRUE))
		return FALSE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled,
	                               cacheFlags & PERSISTENT_KEYS_EXPECTED_FLAG))
		return FALSE;

	Stream_Seek_UINT8(s);                                  /* pad2 (1 byte) */
	Stream_Read_UINT8(s, settings->BitmapCacheV2NumCells); /* numCellCaches (1 byte) */
	if (settings->BitmapCacheV2NumCells > 5)
	{
		WLog_ERR(TAG, "Invalid TS_BITMAPCACHE_CAPABILITYSET_REV2::numCellCaches %" PRIu32 " > 5",
		         settings->BitmapCacheV2NumCells);
		return FALSE;
	}

	for (size_t x = 0; x < settings->BitmapCacheV2NumCells; x++)
	{
		BITMAP_CACHE_V2_CELL_INFO* info =
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_BitmapCacheV2CellInfo, x);
		if (!rdp_read_bitmap_cache_cell_info(s, info))
			return FALSE;
	}

	/* Input must always have 5 BitmapCacheV2CellInfo values */
	for (size_t x = settings->BitmapCacheV2NumCells; x < 5; x++)
	{
		if (!Stream_SafeSeek(s, 4))
			return FALSE;
	}
	Stream_Seek(s, 12); /* pad3 (12 bytes) */
	return TRUE;
}

/*
 * Write bitmap cache v2 capability set.
 * msdn{cc240560}
 */

static BOOL rdp_write_bitmap_cache_v2_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	UINT16 cacheFlags = ALLOW_CACHE_WAITING_LIST_FLAG;

	if (freerdp_settings_get_bool(settings, FreeRDP_BitmapCachePersistEnabled))
	{
		cacheFlags |= PERSISTENT_KEYS_EXPECTED_FLAG;
		settings->BitmapCacheV2CellInfo[0].persistent = 1;
		settings->BitmapCacheV2CellInfo[1].persistent = 1;
		settings->BitmapCacheV2CellInfo[2].persistent = 1;
		settings->BitmapCacheV2CellInfo[3].persistent = 1;
		settings->BitmapCacheV2CellInfo[4].persistent = 1;
	}

	Stream_Write_UINT16(s, cacheFlags);                     /* cacheFlags (2 bytes) */
	Stream_Write_UINT8(s, 0);                               /* pad2 (1 byte) */
	Stream_Write_UINT8(s, settings->BitmapCacheV2NumCells); /* numCellCaches (1 byte) */
	rdp_write_bitmap_cache_cell_info(
	    s, &settings->BitmapCacheV2CellInfo[0]); /* bitmapCache0CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(
	    s, &settings->BitmapCacheV2CellInfo[1]); /* bitmapCache1CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(
	    s, &settings->BitmapCacheV2CellInfo[2]); /* bitmapCache2CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(
	    s, &settings->BitmapCacheV2CellInfo[3]); /* bitmapCache3CellInfo (4 bytes) */
	rdp_write_bitmap_cache_cell_info(
	    s, &settings->BitmapCacheV2CellInfo[4]); /* bitmapCache4CellInfo (4 bytes) */
	Stream_Zero(s, 12);                          /* pad3 (12 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_V2);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_bitmap_cache_v2_capability_set(wStream* s)
{
	BITMAP_CACHE_V2_CELL_INFO bitmapCacheV2CellInfo[5] = { 0 };
	WLog_VRB(TAG, "BitmapCacheV2CapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 36))
		return FALSE;

	const UINT16 cacheFlags = Stream_Get_UINT16(s);  /* cacheFlags (2 bytes) */
	const UINT8 pad2 = Stream_Get_UINT8(s);          /* pad2 (1 byte) */
	const UINT8 numCellCaches = Stream_Get_UINT8(s); /* numCellCaches (1 byte) */

	for (size_t x = 0; x < ARRAYSIZE(bitmapCacheV2CellInfo); x++)
	{
		if (!rdp_read_bitmap_cache_cell_info(
		        s, &bitmapCacheV2CellInfo[x])) /* bitmapCache0CellInfo (4 bytes) */
			return FALSE;
	}

	if (!Stream_SafeSeek(s, 12)) /* pad3 (12 bytes) */
		return FALSE;

	WLog_VRB(TAG, "\tcacheFlags: 0x%04" PRIX16 "", cacheFlags);
	WLog_VRB(TAG, "\tpad2: 0x%02" PRIX8 "", pad2);
	WLog_VRB(TAG, "\tnumCellCaches: 0x%02" PRIX8 "", numCellCaches);
	for (size_t x = 0; x < ARRAYSIZE(bitmapCacheV2CellInfo); x++)
	{
		const BITMAP_CACHE_V2_CELL_INFO* info = &bitmapCacheV2CellInfo[x];
		WLog_VRB(TAG,
		         "\tbitmapCache%" PRIuz "CellInfo: numEntries: %" PRIu32 " persistent: %" PRId32 "",
		         x, info->numEntries, info->persistent);
	}
	return TRUE;
}
#endif

static BOOL rdp_apply_virtual_channel_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	/* MS servers and clients disregard in advertising what is relevant for their own side */
	if (settings->ServerMode && (settings->VCFlags & VCCAPS_COMPR_SC) &&
	    (src->VCFlags & VCCAPS_COMPR_SC))
		settings->VCFlags |= VCCAPS_COMPR_SC;
	else
		settings->VCFlags &= ~VCCAPS_COMPR_SC;

	if (!settings->ServerMode && (settings->VCFlags & VCCAPS_COMPR_CS_8K) &&
	    (src->VCFlags & VCCAPS_COMPR_CS_8K))
		settings->VCFlags |= VCCAPS_COMPR_CS_8K;
	else
		settings->VCFlags &= ~VCCAPS_COMPR_CS_8K;

	/*
	 * When one peer does not write the VCChunkSize, the VCChunkSize must not be
	 * larger than CHANNEL_CHUNK_LENGTH (1600) bytes.
	 * Also prevent an invalid 0 size.
	 */
	if (!settings->ServerMode)
	{
		if ((src->VCChunkSize > CHANNEL_CHUNK_MAX_LENGTH) || (src->VCChunkSize == 0))
			settings->VCChunkSize = CHANNEL_CHUNK_LENGTH;
		else
		{
			settings->VCChunkSize = src->VCChunkSize;
		}
	}

	return TRUE;
}

/*
 * Read virtual channel capability set.
 * msdn{cc240551}
 */

static BOOL rdp_read_virtual_channel_capability_set(wStream* s, rdpSettings* settings)
{
	UINT32 flags = 0;
	UINT32 VCChunkSize = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, flags); /* flags (4 bytes) */

	if (Stream_GetRemainingLength(s) >= 4)
		Stream_Read_UINT32(s, VCChunkSize); /* VCChunkSize (4 bytes) */
	else
		VCChunkSize = UINT32_MAX; /* Use an invalid value to determine that value is not present */

	settings->VCFlags = flags;
	settings->VCChunkSize = VCChunkSize;

	return TRUE;
}

/*
 * Write virtual channel capability set.
 * msdn{cc240551}
 */

static BOOL rdp_write_virtual_channel_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT32(s, settings->VCFlags);     /* flags (4 bytes) */
	Stream_Write_UINT32(s, settings->VCChunkSize); /* VCChunkSize (4 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_VIRTUAL_CHANNEL);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_virtual_channel_capability_set(wStream* s)
{
	UINT32 flags = 0;
	UINT32 VCChunkSize = 0;
	WLog_VRB(TAG, "VirtualChannelCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, flags); /* flags (4 bytes) */

	if (Stream_GetRemainingLength(s) >= 4)
		Stream_Read_UINT32(s, VCChunkSize); /* VCChunkSize (4 bytes) */
	else
		VCChunkSize = 1600;

	WLog_VRB(TAG, "\tflags: 0x%08" PRIX32 "", flags);
	WLog_VRB(TAG, "\tVCChunkSize: 0x%08" PRIX32 "", VCChunkSize);
	return TRUE;
}
#endif

static BOOL rdp_apply_draw_nine_grid_cache_capability_set(rdpSettings* settings,
                                                          const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	settings->DrawNineGridCacheSize = src->DrawNineGridCacheSize;
	settings->DrawNineGridCacheEntries = src->DrawNineGridCacheEntries;
	settings->DrawNineGridEnabled = src->DrawNineGridEnabled;

	return TRUE;
}

/*
 * Read drawn nine grid cache capability set.
 * msdn{cc241565}
 */

static BOOL rdp_read_draw_nine_grid_cache_capability_set(wStream* s, rdpSettings* settings)
{
	UINT32 drawNineGridSupportLevel = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, drawNineGridSupportLevel);        /* drawNineGridSupportLevel (4 bytes) */
	Stream_Read_UINT16(s, settings->DrawNineGridCacheSize); /* drawNineGridCacheSize (2 bytes) */
	Stream_Read_UINT16(s,
	                   settings->DrawNineGridCacheEntries); /* drawNineGridCacheEntries (2 bytes) */

	settings->DrawNineGridEnabled =
	    (drawNineGridSupportLevel & (DRAW_NINEGRID_SUPPORTED | DRAW_NINEGRID_SUPPORTED_V2)) ? TRUE
	                                                                                        : FALSE;

	return TRUE;
}

/*
 * Write drawn nine grid cache capability set.
 * msdn{cc241565}
 */

static BOOL rdp_write_draw_nine_grid_cache_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	const UINT32 drawNineGridSupportLevel =
	    (settings->DrawNineGridEnabled) ? DRAW_NINEGRID_SUPPORTED_V2 : DRAW_NINEGRID_NO_SUPPORT;
	Stream_Write_UINT32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	Stream_Write_UINT16(s, settings->DrawNineGridCacheSize); /* drawNineGridCacheSize (2 bytes) */
	Stream_Write_UINT16(
	    s, settings->DrawNineGridCacheEntries); /* drawNineGridCacheEntries (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_DRAW_NINE_GRID_CACHE);
}

static void rdp_write_gdiplus_cache_entries(wStream* s, UINT16 gce, UINT16 bce, UINT16 pce,
                                            UINT16 ice, UINT16 ace)
{
	Stream_Write_UINT16(s, gce); /* gdipGraphicsCacheEntries (2 bytes) */
	Stream_Write_UINT16(s, bce); /* gdipBrushCacheEntries (2 bytes) */
	Stream_Write_UINT16(s, pce); /* gdipPenCacheEntries (2 bytes) */
	Stream_Write_UINT16(s, ice); /* gdipImageCacheEntries (2 bytes) */
	Stream_Write_UINT16(s, ace); /* gdipImageAttributesCacheEntries (2 bytes) */
}

static void rdp_write_gdiplus_cache_chunk_size(wStream* s, UINT16 gccs, UINT16 obccs, UINT16 opccs,
                                               UINT16 oiaccs)
{
	Stream_Write_UINT16(s, gccs);   /* gdipGraphicsCacheChunkSize (2 bytes) */
	Stream_Write_UINT16(s, obccs);  /* gdipObjectBrushCacheChunkSize (2 bytes) */
	Stream_Write_UINT16(s, opccs);  /* gdipObjectPenCacheChunkSize (2 bytes) */
	Stream_Write_UINT16(s, oiaccs); /* gdipObjectImageAttributesCacheChunkSize (2 bytes) */
}

static void rdp_write_gdiplus_image_cache_properties(wStream* s, UINT16 oiccs, UINT16 oicts,
                                                     UINT16 oicms)
{
	Stream_Write_UINT16(s, oiccs); /* gdipObjectImageCacheChunkSize (2 bytes) */
	Stream_Write_UINT16(s, oicts); /* gdipObjectImageCacheTotalSize (2 bytes) */
	Stream_Write_UINT16(s, oicms); /* gdipObjectImageCacheMaxSize (2 bytes) */
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_draw_nine_grid_cache_capability_set(wStream* s)
{
	UINT32 drawNineGridSupportLevel = 0;
	UINT16 DrawNineGridCacheSize = 0;
	UINT16 DrawNineGridCacheEntries = 0;
	WLog_VRB(TAG,
	         "DrawNineGridCacheCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, drawNineGridSupportLevel); /* drawNineGridSupportLevel (4 bytes) */
	Stream_Read_UINT16(s, DrawNineGridCacheSize);    /* drawNineGridCacheSize (2 bytes) */
	Stream_Read_UINT16(s, DrawNineGridCacheEntries); /* drawNineGridCacheEntries (2 bytes) */
	return TRUE;
}
#endif

static BOOL rdp_apply_draw_gdiplus_cache_capability_set(rdpSettings* settings,
                                                        const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	if (src->DrawGdiPlusEnabled)
		settings->DrawGdiPlusEnabled = TRUE;

	if (src->DrawGdiPlusCacheEnabled)
		settings->DrawGdiPlusCacheEnabled = TRUE;

	return TRUE;
}

/*
 * Read GDI+ cache capability set.
 * msdn{cc241566}
 */

static BOOL rdp_read_draw_gdiplus_cache_capability_set(wStream* s, rdpSettings* settings)
{
	UINT32 drawGDIPlusSupportLevel = 0;
	UINT32 drawGdiplusCacheLevel = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 36))
		return FALSE;

	Stream_Read_UINT32(s, drawGDIPlusSupportLevel); /* drawGDIPlusSupportLevel (4 bytes) */
	Stream_Seek_UINT32(s);                          /* GdipVersion (4 bytes) */
	Stream_Read_UINT32(s, drawGdiplusCacheLevel);   /* drawGdiplusCacheLevel (4 bytes) */
	Stream_Seek(s, 10);                             /* GdipCacheEntries (10 bytes) */
	Stream_Seek(s, 8);                              /* GdipCacheChunkSize (8 bytes) */
	Stream_Seek(s, 6);                              /* GdipImageCacheProperties (6 bytes) */

	settings->DrawGdiPlusEnabled =
	    (drawGDIPlusSupportLevel & DRAW_GDIPLUS_SUPPORTED) ? TRUE : FALSE;
	settings->DrawGdiPlusCacheEnabled =
	    (drawGdiplusCacheLevel & DRAW_GDIPLUS_CACHE_LEVEL_ONE) ? TRUE : FALSE;

	return TRUE;
}

/*
 * Write GDI+ cache capability set.
 * msdn{cc241566}
 */

static BOOL rdp_write_draw_gdiplus_cache_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	const UINT32 drawGDIPlusSupportLevel =
	    (settings->DrawGdiPlusEnabled) ? DRAW_GDIPLUS_SUPPORTED : DRAW_GDIPLUS_DEFAULT;
	const UINT32 drawGdiplusCacheLevel = (settings->DrawGdiPlusEnabled)
	                                         ? DRAW_GDIPLUS_CACHE_LEVEL_ONE
	                                         : DRAW_GDIPLUS_CACHE_LEVEL_DEFAULT;
	Stream_Write_UINT32(s, drawGDIPlusSupportLevel);     /* drawGDIPlusSupportLevel (4 bytes) */
	Stream_Write_UINT32(s, 0);                           /* GdipVersion (4 bytes) */
	Stream_Write_UINT32(s, drawGdiplusCacheLevel);       /* drawGdiplusCacheLevel (4 bytes) */
	rdp_write_gdiplus_cache_entries(s, 10, 5, 5, 10, 2); /* GdipCacheEntries (10 bytes) */
	rdp_write_gdiplus_cache_chunk_size(s, 512, 2048, 1024, 64); /* GdipCacheChunkSize (8 bytes) */
	rdp_write_gdiplus_image_cache_properties(s, 4096, 256,
	                                         128); /* GdipImageCacheProperties (6 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_DRAW_GDI_PLUS);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_draw_gdiplus_cache_capability_set(wStream* s)
{
	UINT32 drawGdiPlusSupportLevel = 0;
	UINT32 GdipVersion = 0;
	UINT32 drawGdiplusCacheLevel = 0;
	WLog_VRB(TAG,
	         "DrawGdiPlusCacheCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 36))
		return FALSE;

	Stream_Read_UINT32(s, drawGdiPlusSupportLevel); /* drawGdiPlusSupportLevel (4 bytes) */
	Stream_Read_UINT32(s, GdipVersion);             /* GdipVersion (4 bytes) */
	Stream_Read_UINT32(s, drawGdiplusCacheLevel);   /* drawGdiPlusCacheLevel (4 bytes) */
	Stream_Seek(s, 10);                             /* GdipCacheEntries (10 bytes) */
	Stream_Seek(s, 8);                              /* GdipCacheChunkSize (8 bytes) */
	Stream_Seek(s, 6);                              /* GdipImageCacheProperties (6 bytes) */
	return TRUE;
}
#endif

static BOOL rdp_apply_remote_programs_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	if (settings->RemoteApplicationMode)
		settings->RemoteApplicationMode = src->RemoteApplicationMode;

	/* 2.2.2.2.3 HandshakeEx PDU (TS_RAIL_ORDER_HANDSHAKE_EX)
	 * the handshake ex pdu is supported when both, client and server announce
	 * it OR if we are ready to begin enhanced remoteAPP mode. */
	UINT32 supportLevel = src->RemoteApplicationSupportLevel;
	if (settings->RemoteApplicationMode)
		supportLevel |= RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED;

	settings->RemoteApplicationSupportLevel = supportLevel & settings->RemoteApplicationSupportMask;

	return TRUE;
}

/*
 * Read remote programs capability set.
 * msdn{cc242518}
 */

static BOOL rdp_read_remote_programs_capability_set(wStream* s, rdpSettings* settings)
{
	UINT32 railSupportLevel = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, railSupportLevel); /* railSupportLevel (4 bytes) */

	settings->RemoteApplicationMode = (railSupportLevel & RAIL_LEVEL_SUPPORTED) ? TRUE : FALSE;
	settings->RemoteApplicationSupportLevel = railSupportLevel;
	return TRUE;
}

/*
 * Write remote programs capability set.
 * msdn{cc242518}
 */

static BOOL rdp_write_remote_programs_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	UINT32 railSupportLevel = RAIL_LEVEL_SUPPORTED;

	if (settings->RemoteApplicationSupportLevel & RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED)
	{
		if (settings->RemoteAppLanguageBarSupported)
			railSupportLevel |= RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED;
	}

	railSupportLevel |= RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED;
	railSupportLevel |= RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED;
	railSupportLevel |= RAIL_LEVEL_SERVER_TO_CLIENT_IME_SYNC_SUPPORTED;
	railSupportLevel |= RAIL_LEVEL_HIDE_MINIMIZED_APPS_SUPPORTED;
	railSupportLevel |= RAIL_LEVEL_WINDOW_CLOAKING_SUPPORTED;
	railSupportLevel |= RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED;
	/* Mask out everything the server does not support. */
	railSupportLevel &= settings->RemoteApplicationSupportLevel;
	Stream_Write_UINT32(s, railSupportLevel); /* railSupportLevel (4 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_RAIL);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_remote_programs_capability_set(wStream* s)
{
	UINT32 railSupportLevel = 0;
	WLog_VRB(TAG, "RemoteProgramsCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, railSupportLevel); /* railSupportLevel (4 bytes) */
	WLog_VRB(TAG, "\trailSupportLevel: 0x%08" PRIX32 "", railSupportLevel);
	return TRUE;
}
#endif

static BOOL rdp_apply_window_list_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	settings->RemoteWndSupportLevel = src->RemoteWndSupportLevel;
	settings->RemoteAppNumIconCaches = src->RemoteAppNumIconCaches;
	settings->RemoteAppNumIconCacheEntries = src->RemoteAppNumIconCacheEntries;

	return TRUE;
}

/*
 * Read window list capability set.
 * msdn{cc242564}
 */

static BOOL rdp_read_window_list_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 7))
		return FALSE;

	Stream_Read_UINT32(s, settings->RemoteWndSupportLevel); /* wndSupportLevel (4 bytes) */
	Stream_Read_UINT8(s, settings->RemoteAppNumIconCaches); /* numIconCaches (1 byte) */
	Stream_Read_UINT16(s,
	                   settings->RemoteAppNumIconCacheEntries); /* numIconCacheEntries (2 bytes) */
	return TRUE;
}

/*
 * Write window list capability set.
 * msdn{cc242564}
 */

static BOOL rdp_write_window_list_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT32(s, settings->RemoteWndSupportLevel); /* wndSupportLevel (4 bytes) */
	Stream_Write_UINT8(s, settings->RemoteAppNumIconCaches); /* numIconCaches (1 byte) */
	Stream_Write_UINT16(s,
	                    settings->RemoteAppNumIconCacheEntries); /* numIconCacheEntries (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_WINDOW);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_window_list_capability_set(wStream* s)
{
	UINT32 wndSupportLevel = 0;
	BYTE numIconCaches = 0;
	UINT16 numIconCacheEntries = 0;
	WLog_VRB(TAG, "WindowListCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 7))
		return FALSE;

	Stream_Read_UINT32(s, wndSupportLevel);     /* wndSupportLevel (4 bytes) */
	Stream_Read_UINT8(s, numIconCaches);        /* numIconCaches (1 byte) */
	Stream_Read_UINT16(s, numIconCacheEntries); /* numIconCacheEntries (2 bytes) */
	WLog_VRB(TAG, "\twndSupportLevel: 0x%08" PRIX32 "", wndSupportLevel);
	WLog_VRB(TAG, "\tnumIconCaches: 0x%02" PRIX8 "", numIconCaches);
	WLog_VRB(TAG, "\tnumIconCacheEntries: 0x%04" PRIX16 "", numIconCacheEntries);
	return TRUE;
}
#endif

static BOOL rdp_apply_desktop_composition_capability_set(rdpSettings* settings,
                                                         const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	settings->CompDeskSupportLevel = src->CompDeskSupportLevel;
	return TRUE;
}

/*
 * Read desktop composition capability set.
 * msdn{cc240855}
 */

static BOOL rdp_read_desktop_composition_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, settings->CompDeskSupportLevel); /* compDeskSupportLevel (2 bytes) */
	return TRUE;
}

/*
 * Write desktop composition capability set.
 * msdn{cc240855}
 */

static BOOL rdp_write_desktop_composition_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	const UINT16 compDeskSupportLevel =
	    (settings->AllowDesktopComposition) ? COMPDESK_SUPPORTED : COMPDESK_NOT_SUPPORTED;
	Stream_Write_UINT16(s, compDeskSupportLevel); /* compDeskSupportLevel (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_COMP_DESK);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_desktop_composition_capability_set(wStream* s)
{
	UINT16 compDeskSupportLevel = 0;
	WLog_VRB(TAG,
	         "DesktopCompositionCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, compDeskSupportLevel); /* compDeskSupportLevel (2 bytes) */
	WLog_VRB(TAG, "\tcompDeskSupportLevel: 0x%04" PRIX16 "", compDeskSupportLevel);
	return TRUE;
}
#endif

static BOOL rdp_apply_multifragment_update_capability_set(rdpSettings* settings,
                                                          const rdpSettings* src)
{
	UINT32 multifragMaxRequestSize = 0;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	multifragMaxRequestSize = src->MultifragMaxRequestSize;

	if (settings->ServerMode)
	{
		/*
		 * Special case: The client announces multifragment update support but sets the maximum
		 * request size to something smaller than maximum size for *one* fast-path PDU. In this case
		 * behave like no multifragment updates were supported and make sure no fragmentation
		 * happens by setting FASTPATH_FRAGMENT_SAFE_SIZE.
		 *
		 * This behaviour was observed with some windows ce rdp clients.
		 */
		if (multifragMaxRequestSize < FASTPATH_MAX_PACKET_SIZE)
			multifragMaxRequestSize = FASTPATH_FRAGMENT_SAFE_SIZE;

		if (settings->RemoteFxCodec)
		{
			/*
			 * If we are using RemoteFX the client MUST use a value greater
			 * than or equal to the value we've previously sent in the server to
			 * client multi-fragment update capability set (MS-RDPRFX 1.5)
			 */
			if (multifragMaxRequestSize < settings->MultifragMaxRequestSize)
			{
				/*
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
		/*
		 * In client mode we keep up with the server's capabilites.
		 * In RemoteFX mode we MUST do this but it might also be useful to
		 * receive larger related bitmap updates.
		 */
		if (multifragMaxRequestSize > settings->MultifragMaxRequestSize)
			settings->MultifragMaxRequestSize = multifragMaxRequestSize;
	}
	return TRUE;
}

/*
 * Read multifragment update capability set.
 * msdn{cc240649}
 */

static BOOL rdp_read_multifragment_update_capability_set(wStream* s, rdpSettings* settings)
{
	UINT32 multifragMaxRequestSize = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, multifragMaxRequestSize); /* MaxRequestSize (4 bytes) */
	settings->MultifragMaxRequestSize = multifragMaxRequestSize;

	return TRUE;
}

/*
 * Write multifragment update capability set.
 * msdn{cc240649}
 */

static BOOL rdp_write_multifragment_update_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (settings->ServerMode && settings->MultifragMaxRequestSize == 0)
	{
		/*
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
		UINT32 tileNumX = (settings->DesktopWidth + 63) / 64;
		UINT32 tileNumY = (settings->DesktopHeight + 63) / 64;
		settings->MultifragMaxRequestSize = tileNumX * tileNumY * 16384;
		/* and add room for headers, regions, frame markers, etc. */
		settings->MultifragMaxRequestSize += 16384;
	}

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;
	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT32(s, settings->MultifragMaxRequestSize); /* MaxRequestSize (4 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_MULTI_FRAGMENT_UPDATE);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_multifragment_update_capability_set(wStream* s)
{
	UINT32 maxRequestSize = 0;
	WLog_VRB(TAG,
	         "MultifragmentUpdateCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, maxRequestSize); /* maxRequestSize (4 bytes) */
	WLog_VRB(TAG, "\tmaxRequestSize: 0x%08" PRIX32 "", maxRequestSize);
	return TRUE;
}
#endif

static BOOL rdp_apply_large_pointer_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	settings->LargePointerFlag = src->LargePointerFlag;
	return TRUE;
}

/*
 * Read large pointer capability set.
 * msdn{cc240650}
 */

static BOOL rdp_read_large_pointer_capability_set(wStream* s, rdpSettings* settings)
{
	UINT16 largePointerSupportFlags = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, largePointerSupportFlags); /* largePointerSupportFlags (2 bytes) */
	settings->LargePointerFlag &= largePointerSupportFlags;
	if ((largePointerSupportFlags & ~(LARGE_POINTER_FLAG_96x96 | LARGE_POINTER_FLAG_384x384)) != 0)
	{
		WLog_WARN(
		    TAG,
		    "TS_LARGE_POINTER_CAPABILITYSET with unsupported flags %04X (all flags %04X) received",
		    largePointerSupportFlags & ~(LARGE_POINTER_FLAG_96x96 | LARGE_POINTER_FLAG_384x384),
		    largePointerSupportFlags);
	}
	return TRUE;
}

/*
 * Write large pointer capability set.
 * msdn{cc240650}
 */

static BOOL rdp_write_large_pointer_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	const UINT16 largePointerSupportFlags =
	    settings->LargePointerFlag & (LARGE_POINTER_FLAG_96x96 | LARGE_POINTER_FLAG_384x384);
	Stream_Write_UINT16(s, largePointerSupportFlags); /* largePointerSupportFlags (2 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_LARGE_POINTER);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_large_pointer_capability_set(wStream* s)
{
	UINT16 largePointerSupportFlags = 0;
	WLog_VRB(TAG, "LargePointerCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, largePointerSupportFlags); /* largePointerSupportFlags (2 bytes) */
	WLog_VRB(TAG, "\tlargePointerSupportFlags: 0x%04" PRIX16 "", largePointerSupportFlags);
	return TRUE;
}
#endif

static BOOL rdp_apply_surface_commands_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	/* [MS-RDPBCGR] 2.2.7.2.9 Surface Commands Capability Set (TS_SURFCMDS_CAPABILITYSET)
	 *
	 * disable surface commands if the remote does not support fastpath
	 */
	if (src->FastPathOutput)
	{
		settings->SurfaceCommandsSupported &= src->SurfaceCommandsSupported;
		settings->SurfaceCommandsEnabled = src->SurfaceCommandsEnabled;
		settings->SurfaceFrameMarkerEnabled = src->SurfaceFrameMarkerEnabled;
	}
	else
	{
		settings->SurfaceCommandsSupported = 0;
		settings->SurfaceCommandsEnabled = FALSE;
		settings->SurfaceFrameMarkerEnabled = FALSE;
	}

	return TRUE;
}

/*
 * Read surface commands capability set.
 * msdn{dd871563}
 */

static BOOL rdp_read_surface_commands_capability_set(wStream* s, rdpSettings* settings)
{
	UINT32 cmdFlags = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, cmdFlags); /* cmdFlags (4 bytes) */
	Stream_Seek_UINT32(s);           /* reserved (4 bytes) */
	settings->SurfaceCommandsSupported = cmdFlags;
	settings->SurfaceCommandsEnabled =
	    (cmdFlags & (SURFCMDS_SET_SURFACE_BITS | SURFCMDS_STREAM_SURFACE_BITS)) ? TRUE : FALSE;
	settings->SurfaceFrameMarkerEnabled = (cmdFlags & SURFCMDS_FRAME_MARKER) ? TRUE : FALSE;
	return TRUE;
}

/*
 * Write surface commands capability set.
 * msdn{dd871563}
 */

static BOOL rdp_write_surface_commands_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	// TODO: Make these configurable too
	UINT32 cmdFlags = freerdp_settings_get_uint32(settings, FreeRDP_SurfaceCommandsSupported);

	if (settings->SurfaceFrameMarkerEnabled)
		cmdFlags |= SURFCMDS_FRAME_MARKER;

	Stream_Write_UINT32(s, cmdFlags); /* cmdFlags (4 bytes) */
	Stream_Write_UINT32(s, 0);        /* reserved (4 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_SURFACE_COMMANDS);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_surface_commands_capability_set(wStream* s)
{
	UINT32 cmdFlags = 0;
	UINT32 reserved = 0;

	WLog_VRB(TAG,
	         "SurfaceCommandsCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, cmdFlags); /* cmdFlags (4 bytes) */
	Stream_Read_UINT32(s, reserved); /* reserved (4 bytes) */
	WLog_VRB(TAG, "\tcmdFlags: 0x%08" PRIX32 "", cmdFlags);
	WLog_VRB(TAG, "\treserved: 0x%08" PRIX32 "", reserved);
	return TRUE;
}

static void rdp_print_bitmap_codec_guid(const GUID* guid)
{
	WINPR_ASSERT(guid);
	WLog_VRB(TAG,
	         "%08" PRIX32 "%04" PRIX16 "%04" PRIX16 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8
	         "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "",
	         guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1], guid->Data4[2],
	         guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}

static char* rdp_get_bitmap_codec_guid_name(const GUID* guid)
{
	RPC_STATUS rpc_status = 0;

	WINPR_ASSERT(guid);
	if (UuidEqual(guid, &CODEC_GUID_REMOTEFX, &rpc_status))
		return "CODEC_GUID_REMOTEFX";
	else if (UuidEqual(guid, &CODEC_GUID_NSCODEC, &rpc_status))
		return "CODEC_GUID_NSCODEC";
	else if (UuidEqual(guid, &CODEC_GUID_IGNORE, &rpc_status))
		return "CODEC_GUID_IGNORE";
	else if (UuidEqual(guid, &CODEC_GUID_IMAGE_REMOTEFX, &rpc_status))
		return "CODEC_GUID_IMAGE_REMOTEFX";

#if defined(WITH_JPEG)
	else if (UuidEqual(guid, &CODEC_GUID_JPEG, &rpc_status))
		return "CODEC_GUID_JPEG";

#endif
	return "CODEC_GUID_UNKNOWN";
}
#endif

static BOOL rdp_read_bitmap_codec_guid(wStream* s, GUID* guid)
{
	BYTE g[16] = { 0 };

	WINPR_ASSERT(guid);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		return FALSE;
	Stream_Read(s, g, 16);
	guid->Data1 = ((UINT32)g[3] << 24U) | ((UINT32)g[2] << 16U) | (g[1] << 8U) | g[0];
	guid->Data2 = (g[5] << 8U) | g[4];
	guid->Data3 = (g[7] << 8U) | g[6];
	guid->Data4[0] = g[8];
	guid->Data4[1] = g[9];
	guid->Data4[2] = g[10];
	guid->Data4[3] = g[11];
	guid->Data4[4] = g[12];
	guid->Data4[5] = g[13];
	guid->Data4[6] = g[14];
	guid->Data4[7] = g[15];
	return TRUE;
}

static void rdp_write_bitmap_codec_guid(wStream* s, const GUID* guid)
{
	BYTE g[16] = { 0 };
	WINPR_ASSERT(guid);
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

static BOOL rdp_apply_bitmap_codecs_capability_set(rdpSettings* settings, const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	if (settings->ServerMode)
	{

		settings->RemoteFxCodecId = src->RemoteFxCodecId;
		settings->RemoteFxCaptureFlags = src->RemoteFxCaptureFlags;
		settings->RemoteFxOnly = src->RemoteFxOnly;
		settings->RemoteFxRlgrMode = src->RemoteFxRlgrMode;
		settings->RemoteFxCodecMode = src->RemoteFxCodecMode;
		settings->NSCodecId = src->NSCodecId;
		settings->NSCodecAllowDynamicColorFidelity = src->NSCodecAllowDynamicColorFidelity;
		settings->NSCodecAllowSubsampling = src->NSCodecAllowSubsampling;
		settings->NSCodecColorLossLevel = src->NSCodecColorLossLevel;

		/* only enable a codec if we've announced/enabled it before */
		settings->RemoteFxCodec = settings->RemoteFxCodec && src->RemoteFxCodecId;
		settings->RemoteFxImageCodec = settings->RemoteFxImageCodec && src->RemoteFxImageCodec;
		if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec,
		                               settings->NSCodec && src->NSCodec))
			return FALSE;
		settings->JpegCodec = src->JpegCodec;
	}
	return TRUE;
}

static BOOL rdp_read_codec_ts_rfx_icap(wStream* sub, rdpSettings* settings, UINT16 icapLen)
{
	UINT16 version = 0;
	UINT16 tileSize = 0;
	BYTE codecFlags = 0;
	BYTE colConvBits = 0;
	BYTE transformBits = 0;
	BYTE entropyBits = 0;
	/* TS_RFX_ICAP */
	if (icapLen != 8)
	{
		WLog_ERR(TAG,
		         "[MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP size %" PRIu16
		         " unsupported, expecting size %" PRIu16 " not supported",
		         icapLen, 8);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, sub, 8))
		return FALSE;

	Stream_Read_UINT16(sub, version);      /* version (2 bytes) */
	Stream_Read_UINT16(sub, tileSize);     /* tileSize (2 bytes) */
	Stream_Read_UINT8(sub, codecFlags);    /* flags (1 byte) */
	Stream_Read_UINT8(sub, colConvBits);   /* colConvBits (1 byte) */
	Stream_Read_UINT8(sub, transformBits); /* transformBits (1 byte) */
	Stream_Read_UINT8(sub, entropyBits);   /* entropyBits (1 byte) */

	if (version == 0x0009)
	{
		/* Version 0.9 */
		if (tileSize != 0x0080)
		{
			WLog_ERR(TAG,
			         "[MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP::version %" PRIu16 " tile size %" PRIu16
			         " not supported",
			         version, tileSize);
			return FALSE;
		}
	}
	else if (version == 0x0100)
	{
		/* Version 1.0 */
		if (tileSize != 0x0040)
		{
			WLog_ERR(TAG,
			         "[MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP::version %" PRIu16 " tile size %" PRIu16
			         " not supported",
			         version, tileSize);
			return FALSE;
		}
	}
	else
	{
		WLog_ERR(TAG, "[MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP::version %" PRIu16 " not supported",
		         version);
		return FALSE;
	}

	/* [MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP CLW_COL_CONV_ICT (0x1) */
	if (colConvBits != 1)
	{
		WLog_ERR(TAG,
		         "[MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP::colConvBits %" PRIu8
		         " not supported, must be CLW_COL_CONV_ICT (0x1)",
		         colConvBits);
		return FALSE;
	}

	/* [MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP CLW_XFORM_DWT_53_A (0x1) */
	if (transformBits != 1)
	{
		WLog_ERR(TAG,
		         "[MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP::transformBits %" PRIu8
		         " not supported, must be CLW_XFORM_DWT_53_A (0x1)",
		         colConvBits);
		return FALSE;
	}

	const UINT8 CODEC_MODE = 0x02; /* [MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP */
	if (!freerdp_settings_set_uint32(settings, FreeRDP_RemoteFxCodecMode, 0x00))
		return FALSE;

	if ((codecFlags & CODEC_MODE) != 0)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_RemoteFxCodecMode, 0x02))
			return FALSE;
	}
	else if ((codecFlags & ~CODEC_MODE) != 0)
		WLog_WARN(TAG,
		          "[MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP::flags unknown value "
		          "0x%02" PRIx8,
		          (codecFlags & ~CODEC_MODE));

	switch (entropyBits)
	{
		case CLW_ENTROPY_RLGR1:
			if (!freerdp_settings_set_uint32(settings, FreeRDP_RemoteFxRlgrMode, RLGR1))
				return FALSE;
			break;
		case CLW_ENTROPY_RLGR3:
			if (!freerdp_settings_set_uint32(settings, FreeRDP_RemoteFxRlgrMode, RLGR3))
				return FALSE;
			break;
		default:
			WLog_ERR(TAG,
			         "[MS-RDPRFX] 2.2.1.1.1.1.1 TS_RFX_ICAP::entropyBits "
			         "unsupported value 0x%02" PRIx8
			         ", must be CLW_ENTROPY_RLGR1 (0x01) or CLW_ENTROPY_RLGR3 "
			         "(0x04)",
			         entropyBits);
			return FALSE;
	}
	return TRUE;
}

static BOOL rdp_read_codec_ts_rfx_capset(wStream* s, rdpSettings* settings)
{
	UINT16 blockType = 0;
	UINT32 blockLen = 0;
	BYTE rfxCodecId = 0;
	UINT16 capsetType = 0;
	UINT16 numIcaps = 0;
	UINT16 icapLen = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	/* TS_RFX_CAPSET */
	Stream_Read_UINT16(s, blockType); /* blockType (2 bytes) */
	Stream_Read_UINT32(s, blockLen);  /* blockLen (4 bytes) */
	if (blockType != 0xCBC1)
	{
		WLog_ERR(TAG,
		         "[MS_RDPRFX] 2.2.1.1.1.1 TS_RFX_CAPSET::blockType[0x%04" PRIx16
		         "] != CBY_CAPSET (0xCBC1)",
		         blockType);
		return FALSE;
	}
	if (blockLen < 6ull)
	{
		WLog_ERR(TAG, "[MS_RDPRFX] 2.2.1.1.1.1 TS_RFX_CAPSET::blockLen[%" PRIu16 "] < 6", blockLen);
		return FALSE;
	}
	if (!Stream_CheckAndLogRequiredLength(TAG, s, blockLen - 6ull))
		return FALSE;

	wStream sbuffer = { 0 };
	wStream* sub = Stream_StaticConstInit(&sbuffer, Stream_Pointer(s), blockLen - 6ull);
	WINPR_ASSERT(sub);

	if (!Stream_CheckAndLogRequiredLength(TAG, sub, 7))
		return FALSE;

	Stream_Read_UINT8(sub, rfxCodecId);  /* codecId (1 byte) */
	Stream_Read_UINT16(sub, capsetType); /* capsetType (2 bytes) */
	Stream_Read_UINT16(sub, numIcaps);   /* numIcaps (2 bytes) */
	Stream_Read_UINT16(sub, icapLen);    /* icapLen (2 bytes) */

	if (rfxCodecId != 1)
	{
		WLog_ERR(TAG, "[MS_RDPRFX] 2.2.1.1.1.1 TS_RFX_CAPSET::codecId[%" PRIu16 "] != 1",
		         rfxCodecId);
		return FALSE;
	}

	if (capsetType != 0xCFC0)
	{
		WLog_ERR(TAG,
		         "[MS_RDPRFX] 2.2.1.1.1.1 TS_RFX_CAPSET::capsetType[0x%04" PRIx16
		         "] != CLY_CAPSET (0xCFC0)",
		         capsetType);
		return FALSE;
	}

	while (numIcaps--)
	{
		if (!rdp_read_codec_ts_rfx_icap(sub, settings, icapLen))
			return FALSE;
	}
	return TRUE;
}

static BOOL rdp_read_codec_ts_rfx_caps(wStream* sub, rdpSettings* settings)
{
	if (Stream_GetRemainingLength(sub) == 0)
		return TRUE;

	UINT16 blockType = 0;
	UINT32 blockLen = 0;
	UINT16 numCapsets = 0;

	/* TS_RFX_CAPS */
	if (!Stream_CheckAndLogRequiredLength(TAG, sub, 8))
		return FALSE;
	Stream_Read_UINT16(sub, blockType);  /* blockType (2 bytes) */
	Stream_Read_UINT32(sub, blockLen);   /* blockLen (4 bytes) */
	Stream_Read_UINT16(sub, numCapsets); /* numCapsets (2 bytes) */

	if (blockType != 0xCBC0)
	{
		WLog_ERR(TAG,
		         "[MS_RDPRFX] 2.2.1.1.1 TS_RFX_CAPS::blockType[0x%04" PRIx16
		         "] != CBY_CAPS (0xCBC0)",
		         blockType);
		return FALSE;
	}

	if (blockLen != 8)
	{
		WLog_ERR(TAG, "[MS_RDPRFX] 2.2.1.1.1 TS_RFX_CAPS::blockLen[%" PRIu16 "] != 8", blockLen);
		return FALSE;
	}

	if (numCapsets != 1)
	{
		WLog_ERR(TAG, "[MS_RDPRFX] 2.2.1.1.1.1 TS_RFX_CAPSET::numIcaps[" PRIu16 "] != 1",
		         numCapsets);
		return FALSE;
	}

	for (UINT16 x = 0; x < numCapsets; x++)
	{
		if (!rdp_read_codec_ts_rfx_capset(sub, settings))
			return FALSE;
	}

	return TRUE;
}

static BOOL rdp_read_codec_ts_rfx_clnt_caps_container(wStream* s, rdpSettings* settings)
{
	UINT32 rfxCapsLength = 0;
	UINT32 rfxPropsLength = 0;
	UINT32 captureFlags = 0;

	/* [MS_RDPRFX] 2.2.1.1 TS_RFX_CLNT_CAPS_CONTAINER */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;
	Stream_Read_UINT32(s, rfxPropsLength); /* length (4 bytes) */
	if (rfxPropsLength < 4)
	{
		WLog_ERR(TAG,
		         "[MS_RDPRFX] 2.2.1.1 TS_RFX_CLNT_CAPS_CONTAINER::length %" PRIu32
		         " too short, require at least 4 bytes",
		         rfxPropsLength);
		return FALSE;
	}
	if (!Stream_CheckAndLogRequiredLength(TAG, s, rfxPropsLength - 4ull))
		return FALSE;

	wStream sbuffer = { 0 };
	wStream* sub = Stream_StaticConstInit(&sbuffer, Stream_Pointer(s), rfxPropsLength - 4ull);
	WINPR_ASSERT(sub);

	Stream_Seek(s, rfxPropsLength - 4ull);

	if (!Stream_CheckAndLogRequiredLength(TAG, sub, 8))
		return FALSE;

	Stream_Read_UINT32(sub, captureFlags);  /* captureFlags (4 bytes) */
	Stream_Read_UINT32(sub, rfxCapsLength); /* capsLength (4 bytes) */
	if (!Stream_CheckAndLogRequiredLength(TAG, sub, rfxCapsLength))
		return FALSE;

	settings->RemoteFxCaptureFlags = captureFlags;
	settings->RemoteFxOnly = (captureFlags & CARDP_CAPS_CAPTURE_NON_CAC) ? FALSE : TRUE;

	/* [MS_RDPRFX] 2.2.1.1.1 TS_RFX_CAPS */
	wStream tsbuffer = { 0 };
	wStream* ts_sub = Stream_StaticConstInit(&tsbuffer, Stream_Pointer(sub), rfxCapsLength);
	WINPR_ASSERT(ts_sub);
	return rdp_read_codec_ts_rfx_caps(ts_sub, settings);
}

/*
 * Read bitmap codecs capability set.
 * msdn{dd891377}
 */

static BOOL rdp_read_bitmap_codecs_capability_set(wStream* s, rdpSettings* settings, BOOL isServer)
{
	BYTE codecId = 0;
	GUID codecGuid = { 0 };
	RPC_STATUS rpc_status = 0;
	BYTE bitmapCodecCount = 0;
	UINT16 codecPropertiesLength = 0;

	BOOL guidNSCodec = FALSE;
	BOOL guidRemoteFx = FALSE;
	BOOL guidRemoteFxImage = FALSE;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, bitmapCodecCount); /* bitmapCodecCount (1 byte) */

	while (bitmapCodecCount > 0)
	{
		wStream subbuffer = { 0 };

		if (!rdp_read_bitmap_codec_guid(s, &codecGuid)) /* codecGuid (16 bytes) */
			return FALSE;
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 3))
			return FALSE;
		Stream_Read_UINT8(s, codecId);                /* codecId (1 byte) */
		Stream_Read_UINT16(s, codecPropertiesLength); /* codecPropertiesLength (2 bytes) */

		wStream* sub = Stream_StaticInit(&subbuffer, Stream_Pointer(s), codecPropertiesLength);
		if (!Stream_SafeSeek(s, codecPropertiesLength))
			return FALSE;

		if (isServer)
		{
			if (UuidEqual(&codecGuid, &CODEC_GUID_REMOTEFX, &rpc_status))
			{
				guidRemoteFx = TRUE;
				settings->RemoteFxCodecId = codecId;
				if (!rdp_read_codec_ts_rfx_clnt_caps_container(sub, settings))
					return FALSE;
			}
			else if (UuidEqual(&codecGuid, &CODEC_GUID_IMAGE_REMOTEFX, &rpc_status))
			{
				/* Microsoft RDP servers ignore CODEC_GUID_IMAGE_REMOTEFX codec properties */
				guidRemoteFxImage = TRUE;
				if (!Stream_SafeSeek(sub, codecPropertiesLength)) /* codecProperties */
					return FALSE;
			}
			else if (UuidEqual(&codecGuid, &CODEC_GUID_NSCODEC, &rpc_status))
			{
				BYTE colorLossLevel = 0;
				BYTE fAllowSubsampling = 0;
				BYTE fAllowDynamicFidelity = 0;
				guidNSCodec = TRUE;
				settings->NSCodecId = codecId;
				if (!Stream_CheckAndLogRequiredLength(TAG, sub, 3))
					return FALSE;
				Stream_Read_UINT8(sub, fAllowDynamicFidelity); /* fAllowDynamicFidelity (1 byte) */
				Stream_Read_UINT8(sub, fAllowSubsampling);     /* fAllowSubsampling (1 byte) */
				Stream_Read_UINT8(sub, colorLossLevel);        /* colorLossLevel (1 byte) */

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
				if (!Stream_SafeSeek(sub, codecPropertiesLength)) /* codecProperties */
					return FALSE;
			}
			else
			{
				if (!Stream_SafeSeek(sub, codecPropertiesLength)) /* codecProperties */
					return FALSE;
			}
		}
		else
		{
			if (!Stream_SafeSeek(sub, codecPropertiesLength)) /* codecProperties */
				return FALSE;
		}

		const size_t rest = Stream_GetRemainingLength(sub);
		if (rest > 0)
		{
			WLog_ERR(TAG,
			         "error while reading codec properties: actual size: %" PRIuz
			         " expected size: %" PRIu32 "",
			         rest + codecPropertiesLength, codecPropertiesLength);
		}
		bitmapCodecCount--;

		/* only enable a codec if we've announced/enabled it before */
		if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, guidRemoteFx))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxImageCodec, guidRemoteFxImage))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, guidNSCodec))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_JpegCodec, FALSE))
			return FALSE;
	}

	return TRUE;
}

/*
 * Write RemoteFX Client Capability Container.
 */
static BOOL rdp_write_rfx_client_capability_container(wStream* s, const rdpSettings* settings)
{
	UINT32 captureFlags = 0;
	BYTE codecMode = 0;

	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	captureFlags = settings->RemoteFxOnly ? 0 : CARDP_CAPS_CAPTURE_NON_CAC;
	codecMode = settings->RemoteFxCodecMode;
	Stream_Write_UINT16(s, 49); /* codecPropertiesLength */
	/* TS_RFX_CLNT_CAPS_CONTAINER */
	Stream_Write_UINT32(s, 49);           /* length */
	Stream_Write_UINT32(s, captureFlags); /* captureFlags */
	Stream_Write_UINT32(s, 37);           /* capsLength */
	/* TS_RFX_CAPS */
	Stream_Write_UINT16(s, CBY_CAPS); /* blockType */
	Stream_Write_UINT32(s, 8);        /* blockLen */
	Stream_Write_UINT16(s, 1);        /* numCapsets */
	/* TS_RFX_CAPSET */
	Stream_Write_UINT16(s, CBY_CAPSET); /* blockType */
	Stream_Write_UINT32(s, 29);         /* blockLen */
	Stream_Write_UINT8(s, 0x01);        /* codecId (MUST be set to 0x01) */
	Stream_Write_UINT16(s, CLY_CAPSET); /* capsetType */
	Stream_Write_UINT16(s, 2);          /* numIcaps */
	Stream_Write_UINT16(s, 8);          /* icapLen */
	/* TS_RFX_ICAP (RLGR1) */
	Stream_Write_UINT16(s, CLW_VERSION_1_0);   /* version */
	Stream_Write_UINT16(s, CT_TILE_64x64);     /* tileSize */
	Stream_Write_UINT8(s, codecMode);          /* flags */
	Stream_Write_UINT8(s, CLW_COL_CONV_ICT);   /* colConvBits */
	Stream_Write_UINT8(s, CLW_XFORM_DWT_53_A); /* transformBits */
	Stream_Write_UINT8(s, CLW_ENTROPY_RLGR1);  /* entropyBits */
	/* TS_RFX_ICAP (RLGR3) */
	Stream_Write_UINT16(s, CLW_VERSION_1_0);   /* version */
	Stream_Write_UINT16(s, CT_TILE_64x64);     /* tileSize */
	Stream_Write_UINT8(s, codecMode);          /* flags */
	Stream_Write_UINT8(s, CLW_COL_CONV_ICT);   /* colConvBits */
	Stream_Write_UINT8(s, CLW_XFORM_DWT_53_A); /* transformBits */
	Stream_Write_UINT8(s, CLW_ENTROPY_RLGR3);  /* entropyBits */
	return TRUE;
}

/*
 * Write NSCODEC Client Capability Container.
 */
static BOOL rdp_write_nsc_client_capability_container(wStream* s, const rdpSettings* settings)
{
	BYTE colorLossLevel = 0;
	BYTE fAllowSubsampling = 0;
	BYTE fAllowDynamicFidelity = 0;

	WINPR_ASSERT(settings);

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
	Stream_Write_UINT8(s, fAllowDynamicFidelity); /* fAllowDynamicFidelity (1 byte) */
	Stream_Write_UINT8(s, fAllowSubsampling);     /* fAllowSubsampling (1 byte) */
	Stream_Write_UINT8(s, colorLossLevel);        /* colorLossLevel (1 byte) */
	return TRUE;
}

#if defined(WITH_JPEG)
static BOOL rdp_write_jpeg_client_capability_container(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, 1); /* codecPropertiesLength */
	Stream_Write_UINT8(s, settings->JpegQuality);
	return TRUE;
}
#endif

/*
 * Write RemoteFX Server Capability Container.
 */
static BOOL rdp_write_rfx_server_capability_container(wStream* s, const rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, 4); /* codecPropertiesLength */
	Stream_Write_UINT32(s, 0); /* reserved */
	return TRUE;
}

static BOOL rdp_write_jpeg_server_capability_container(wStream* s, const rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, 1); /* codecPropertiesLength */
	Stream_Write_UINT8(s, 75);
	return TRUE;
}

/*
 * Write NSCODEC Server Capability Container.
 */
static BOOL rdp_write_nsc_server_capability_container(wStream* s, const rdpSettings* settings)
{
	WINPR_UNUSED(settings);
	WINPR_ASSERT(settings);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, 4); /* codecPropertiesLength */
	Stream_Write_UINT32(s, 0); /* reserved */
	return TRUE;
}

/*
 * Write bitmap codecs capability set.
 * msdn{dd891377}
 */

static BOOL rdp_write_bitmap_codecs_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	BYTE bitmapCodecCount = 0;

	if (settings->RemoteFxCodec)
		bitmapCodecCount++;

	if (freerdp_settings_get_bool(settings, FreeRDP_NSCodec))
		bitmapCodecCount++;

#if defined(WITH_JPEG)

	if (settings->JpegCodec)
		bitmapCodecCount++;

#endif

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

	if (freerdp_settings_get_bool(settings, FreeRDP_NSCodec))
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

#if defined(WITH_JPEG)

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

#endif

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

	return rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CODECS);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_bitmap_codecs_capability_set(wStream* s)
{
	GUID codecGuid = { 0 };
	BYTE bitmapCodecCount = 0;
	BYTE codecId = 0;
	UINT16 codecPropertiesLength = 0;

	WLog_VRB(TAG, "BitmapCodecsCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, bitmapCodecCount); /* bitmapCodecCount (1 byte) */
	WLog_VRB(TAG, "\tbitmapCodecCount: %" PRIu8 "", bitmapCodecCount);

	while (bitmapCodecCount > 0)
	{
		if (!rdp_read_bitmap_codec_guid(s, &codecGuid)) /* codecGuid (16 bytes) */
			return FALSE;
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 3))
			return FALSE;
		Stream_Read_UINT8(s, codecId); /* codecId (1 byte) */
		WLog_VRB(TAG, "\tcodecGuid: 0x");
		rdp_print_bitmap_codec_guid(&codecGuid);
		WLog_VRB(TAG, " (%s)", rdp_get_bitmap_codec_guid_name(&codecGuid));
		WLog_VRB(TAG, "\tcodecId: %" PRIu8 "", codecId);
		Stream_Read_UINT16(s, codecPropertiesLength); /* codecPropertiesLength (2 bytes) */
		WLog_VRB(TAG, "\tcodecPropertiesLength: %" PRIu16 "", codecPropertiesLength);

		if (!Stream_SafeSeek(s, codecPropertiesLength)) /* codecProperties */
			return FALSE;
		bitmapCodecCount--;
	}

	return TRUE;
}
#endif

static BOOL rdp_apply_frame_acknowledge_capability_set(rdpSettings* settings,
                                                       const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	if (settings->ServerMode)
		settings->FrameAcknowledge = src->FrameAcknowledge;

	return TRUE;
}

/*
 * Read frame acknowledge capability set.
 */

static BOOL rdp_read_frame_acknowledge_capability_set(wStream* s, rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, settings->FrameAcknowledge); /* (4 bytes) */

	return TRUE;
}

/*
 * Write frame acknowledge capability set.
 */

static BOOL rdp_write_frame_acknowledge_capability_set(wStream* s, const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	Stream_Write_UINT32(s, settings->FrameAcknowledge); /* (4 bytes) */
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_FRAME_ACKNOWLEDGE);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_frame_acknowledge_capability_set(wStream* s)
{
	UINT32 frameAcknowledge = 0;
	WLog_VRB(TAG,
	         "FrameAcknowledgeCapabilitySet (length %" PRIuz "):", Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, frameAcknowledge); /* frameAcknowledge (4 bytes) */
	WLog_VRB(TAG, "\tframeAcknowledge: 0x%08" PRIX32 "", frameAcknowledge);
	return TRUE;
}
#endif

static BOOL rdp_apply_bitmap_cache_v3_codec_id_capability_set(rdpSettings* settings,
                                                              const rdpSettings* src)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(src);

	settings->BitmapCacheV3CodecId = src->BitmapCacheV3CodecId;
	return TRUE;
}

static BOOL rdp_read_bitmap_cache_v3_codec_id_capability_set(wStream* s, rdpSettings* settings)
{
	BYTE bitmapCacheV3CodecId = 0;

	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, bitmapCacheV3CodecId); /* bitmapCacheV3CodecId (1 byte) */
	settings->BitmapCacheV3CodecId = bitmapCacheV3CodecId;
	return TRUE;
}

static BOOL rdp_write_bitmap_cache_v3_codec_id_capability_set(wStream* s,
                                                              const rdpSettings* settings)
{
	WINPR_ASSERT(settings);
	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	const size_t header = rdp_capability_set_start(s);
	if (settings->BitmapCacheV3CodecId > UINT8_MAX)
		return FALSE;
	Stream_Write_UINT8(s, (UINT8)settings->BitmapCacheV3CodecId);
	return rdp_capability_set_finish(s, header, CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID);
}

#ifdef WITH_DEBUG_CAPABILITIES
static BOOL rdp_print_bitmap_cache_v3_codec_id_capability_set(wStream* s)
{
	BYTE bitmapCacheV3CodecId = 0;
	WLog_VRB(TAG, "BitmapCacheV3CodecIdCapabilitySet (length %" PRIuz "):",
	         Stream_GetRemainingLength(s));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, bitmapCacheV3CodecId); /* bitmapCacheV3CodecId (1 byte) */
	WLog_VRB(TAG, "\tbitmapCacheV3CodecId: 0x%02" PRIX8 "", bitmapCacheV3CodecId);
	return TRUE;
}

BOOL rdp_print_capability_sets(wStream* s, size_t start, BOOL receiving)
{
	BOOL rc = FALSE;
	UINT16 type = 0;
	UINT16 length = 0;
	UINT16 numberCapabilities = 0;

	size_t pos = Stream_GetPosition(s);

	Stream_SetPosition(s, start);
	if (receiving)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			goto fail;
	}
	else
	{
		if (!Stream_CheckAndLogRequiredCapacity(TAG, (s), 4))
			goto fail;
	}

	Stream_Read_UINT16(s, numberCapabilities);
	Stream_Seek(s, 2);

	while (numberCapabilities > 0)
	{
		size_t rest = 0;
		wStream subBuffer;
		wStream* sub = NULL;

		if (!rdp_read_capability_set_header(s, &length, &type))
			goto fail;

		WLog_VRB(TAG, "%s ", receiving ? "Receiving" : "Sending");
		sub = Stream_StaticInit(&subBuffer, Stream_Pointer(s), length - 4);
		if (!Stream_SafeSeek(s, length - 4))
			goto fail;

		switch (type)
		{
			case CAPSET_TYPE_GENERAL:
				if (!rdp_print_general_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_BITMAP:
				if (!rdp_print_bitmap_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_ORDER:
				if (!rdp_print_order_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_BITMAP_CACHE:
				if (!rdp_print_bitmap_cache_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_CONTROL:
				if (!rdp_print_control_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_ACTIVATION:
				if (!rdp_print_window_activation_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_POINTER:
				if (!rdp_print_pointer_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_SHARE:
				if (!rdp_print_share_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_COLOR_CACHE:
				if (!rdp_print_color_cache_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_SOUND:
				if (!rdp_print_sound_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_INPUT:
				if (!rdp_print_input_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_FONT:
				if (!rdp_print_font_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_BRUSH:
				if (!rdp_print_brush_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_GLYPH_CACHE:
				if (!rdp_print_glyph_cache_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_OFFSCREEN_CACHE:
				if (!rdp_print_offscreen_bitmap_cache_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT:
				if (!rdp_print_bitmap_cache_host_support_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_BITMAP_CACHE_V2:
				if (!rdp_print_bitmap_cache_v2_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_VIRTUAL_CHANNEL:
				if (!rdp_print_virtual_channel_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_DRAW_NINE_GRID_CACHE:
				if (!rdp_print_draw_nine_grid_cache_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_DRAW_GDI_PLUS:
				if (!rdp_print_draw_gdiplus_cache_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_RAIL:
				if (!rdp_print_remote_programs_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_WINDOW:
				if (!rdp_print_window_list_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_COMP_DESK:
				if (!rdp_print_desktop_composition_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_MULTI_FRAGMENT_UPDATE:
				if (!rdp_print_multifragment_update_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_LARGE_POINTER:
				if (!rdp_print_large_pointer_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_SURFACE_COMMANDS:
				if (!rdp_print_surface_commands_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_BITMAP_CODECS:
				if (!rdp_print_bitmap_codecs_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_FRAME_ACKNOWLEDGE:
				if (!rdp_print_frame_acknowledge_capability_set(sub))
					goto fail;

				break;

			case CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID:
				if (!rdp_print_bitmap_cache_v3_codec_id_capability_set(sub))
					goto fail;

				break;

			default:
				WLog_ERR(TAG, "unknown capability type %" PRIu16 "", type);
				break;
		}

		rest = Stream_GetRemainingLength(sub);
		if (rest > 0)
		{
			WLog_WARN(TAG,
			          "incorrect capability offset, type:0x%04" PRIX16 " %" PRIu16
			          " bytes expected, %" PRIuz "bytes remaining",
			          type, length, rest);
		}

		numberCapabilities--;
	}

	rc = TRUE;
fail:
	Stream_SetPosition(s, pos);
	return rc;
}
#endif

static BOOL rdp_apply_from_received(UINT16 type, rdpSettings* dst, const rdpSettings* src)
{
	switch (type)
	{
		case CAPSET_TYPE_GENERAL:
			return rdp_apply_general_capability_set(dst, src);
		case CAPSET_TYPE_BITMAP:
			return rdp_apply_bitmap_capability_set(dst, src);
		case CAPSET_TYPE_ORDER:
			return rdp_apply_order_capability_set(dst, src);
		case CAPSET_TYPE_POINTER:
			return rdp_apply_pointer_capability_set(dst, src);
		case CAPSET_TYPE_INPUT:
			return rdp_apply_input_capability_set(dst, src);
		case CAPSET_TYPE_VIRTUAL_CHANNEL:
			return rdp_apply_virtual_channel_capability_set(dst, src);
		case CAPSET_TYPE_SHARE:
			return rdp_apply_share_capability_set(dst, src);
		case CAPSET_TYPE_COLOR_CACHE:
			return rdp_apply_color_cache_capability_set(dst, src);
		case CAPSET_TYPE_FONT:
			return rdp_apply_font_capability_set(dst, src);
		case CAPSET_TYPE_DRAW_GDI_PLUS:
			return rdp_apply_draw_gdiplus_cache_capability_set(dst, src);
		case CAPSET_TYPE_RAIL:
			return rdp_apply_remote_programs_capability_set(dst, src);
		case CAPSET_TYPE_WINDOW:
			return rdp_apply_window_list_capability_set(dst, src);
		case CAPSET_TYPE_MULTI_FRAGMENT_UPDATE:
			return rdp_apply_multifragment_update_capability_set(dst, src);
		case CAPSET_TYPE_LARGE_POINTER:
			return rdp_apply_large_pointer_capability_set(dst, src);
		case CAPSET_TYPE_COMP_DESK:
			return rdp_apply_desktop_composition_capability_set(dst, src);
		case CAPSET_TYPE_SURFACE_COMMANDS:
			return rdp_apply_surface_commands_capability_set(dst, src);
		case CAPSET_TYPE_BITMAP_CODECS:
			return rdp_apply_bitmap_codecs_capability_set(dst, src);
		case CAPSET_TYPE_FRAME_ACKNOWLEDGE:
			return rdp_apply_frame_acknowledge_capability_set(dst, src);
		case CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID:
			return rdp_apply_bitmap_cache_v3_codec_id_capability_set(dst, src);
		case CAPSET_TYPE_BITMAP_CACHE:
			return rdp_apply_bitmap_cache_capability_set(dst, src);
		case CAPSET_TYPE_BITMAP_CACHE_V2:
			return rdp_apply_bitmap_cache_v2_capability_set(dst, src);
		case CAPSET_TYPE_BRUSH:
			return rdp_apply_brush_capability_set(dst, src);
		case CAPSET_TYPE_GLYPH_CACHE:
			return rdp_apply_glyph_cache_capability_set(dst, src);
		case CAPSET_TYPE_OFFSCREEN_CACHE:
			return rdp_apply_offscreen_bitmap_cache_capability_set(dst, src);
		case CAPSET_TYPE_SOUND:
			return rdp_apply_sound_capability_set(dst, src);
		case CAPSET_TYPE_CONTROL:
			return rdp_apply_control_capability_set(dst, src);
		case CAPSET_TYPE_ACTIVATION:
			return rdp_apply_window_activation_capability_set(dst, src);
		case CAPSET_TYPE_DRAW_NINE_GRID_CACHE:
			return rdp_apply_draw_nine_grid_cache_capability_set(dst, src);
		case CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT:
			return rdp_apply_bitmap_cache_host_support_capability_set(dst, src);
		default:
			return TRUE;
	}
}

BOOL rdp_read_capability_set(wStream* sub, UINT16 type, rdpSettings* settings, BOOL isServer)
{
	WINPR_ASSERT(settings);

	if (type <= CAPSET_TYPE_FRAME_ACKNOWLEDGE)
	{
		size_t size = Stream_Length(sub);

		WINPR_ASSERT(settings->ReceivedCapabilities);
		settings->ReceivedCapabilities[type] = TRUE;

		WINPR_ASSERT(settings->ReceivedCapabilityDataSizes);
		settings->ReceivedCapabilityDataSizes[type] = size;

		WINPR_ASSERT(settings->ReceivedCapabilityData);
		void* tmp = realloc(settings->ReceivedCapabilityData[type], size);
		if (!tmp && (size > 0))
			return FALSE;
		memcpy(tmp, Stream_Buffer(sub), size);
		settings->ReceivedCapabilityData[type] = tmp;
	}
	else
		WLog_WARN(TAG, "not handling capability type %" PRIu16 " yet", type);

	BOOL treated = TRUE;

	switch (type)
	{
		case CAPSET_TYPE_GENERAL:
			if (!rdp_read_general_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_BITMAP:
			if (!rdp_read_bitmap_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_ORDER:
			if (!rdp_read_order_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_POINTER:
			if (!rdp_read_pointer_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_INPUT:
			if (!rdp_read_input_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_VIRTUAL_CHANNEL:
			if (!rdp_read_virtual_channel_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_SHARE:
			if (!rdp_read_share_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_COLOR_CACHE:
			if (!rdp_read_color_cache_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_FONT:
			if (!rdp_read_font_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_DRAW_GDI_PLUS:
			if (!rdp_read_draw_gdiplus_cache_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_RAIL:
			if (!rdp_read_remote_programs_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_WINDOW:
			if (!rdp_read_window_list_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_MULTI_FRAGMENT_UPDATE:
			if (!rdp_read_multifragment_update_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_LARGE_POINTER:
			if (!rdp_read_large_pointer_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_COMP_DESK:
			if (!rdp_read_desktop_composition_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_SURFACE_COMMANDS:
			if (!rdp_read_surface_commands_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_BITMAP_CODECS:
			if (!rdp_read_bitmap_codecs_capability_set(sub, settings, isServer))
				return FALSE;

			break;

		case CAPSET_TYPE_FRAME_ACKNOWLEDGE:
			if (!rdp_read_frame_acknowledge_capability_set(sub, settings))
				return FALSE;

			break;

		case CAPSET_TYPE_BITMAP_CACHE_V3_CODEC_ID:
			if (!rdp_read_bitmap_cache_v3_codec_id_capability_set(sub, settings))
				return FALSE;

			break;

		default:
			treated = FALSE;
			break;
	}

	if (!treated)
	{
		if (isServer)
		{
			/* treating capabilities that are supposed to be send only from the client */
			switch (type)
			{
				case CAPSET_TYPE_BITMAP_CACHE:
					if (!rdp_read_bitmap_cache_capability_set(sub, settings))
						return FALSE;

					break;

				case CAPSET_TYPE_BITMAP_CACHE_V2:
					if (!rdp_read_bitmap_cache_v2_capability_set(sub, settings))
						return FALSE;

					break;

				case CAPSET_TYPE_BRUSH:
					if (!rdp_read_brush_capability_set(sub, settings))
						return FALSE;

					break;

				case CAPSET_TYPE_GLYPH_CACHE:
					if (!rdp_read_glyph_cache_capability_set(sub, settings))
						return FALSE;

					break;

				case CAPSET_TYPE_OFFSCREEN_CACHE:
					if (!rdp_read_offscreen_bitmap_cache_capability_set(sub, settings))
						return FALSE;

					break;

				case CAPSET_TYPE_SOUND:
					if (!rdp_read_sound_capability_set(sub, settings))
						return FALSE;

					break;

				case CAPSET_TYPE_CONTROL:
					if (!rdp_read_control_capability_set(sub, settings))
						return FALSE;

					break;

				case CAPSET_TYPE_ACTIVATION:
					if (!rdp_read_window_activation_capability_set(sub, settings))
						return FALSE;

					break;

				case CAPSET_TYPE_DRAW_NINE_GRID_CACHE:
					if (!rdp_read_draw_nine_grid_cache_capability_set(sub, settings))
						return FALSE;

					break;

				default:
					WLog_ERR(TAG, "capability %s(%" PRIu16 ") not expected from client",
					         get_capability_name(type), type);
					return FALSE;
			}
		}
		else
		{
			/* treating capabilities that are supposed to be send only from the server */
			switch (type)
			{
				case CAPSET_TYPE_BITMAP_CACHE_HOST_SUPPORT:
					if (!rdp_read_bitmap_cache_host_support_capability_set(sub, settings))
						return FALSE;

					break;

				default:
					WLog_ERR(TAG, "capability %s(%" PRIu16 ") not expected from server",
					         get_capability_name(type), type);
					return FALSE;
			}
		}
	}

	const size_t rest = Stream_GetRemainingLength(sub);
	if (rest > 0)
	{
		const size_t length = Stream_Capacity(sub);
		WLog_ERR(TAG,
		         "incorrect offset, type:0x%04" PRIx16 " actual:%" PRIuz " expected:%" PRIuz "",
		         type, length - rest, length);
	}
	return TRUE;
}

static BOOL rdp_read_capability_sets(wStream* s, rdpSettings* settings, rdpSettings* rcvSettings,
                                     UINT16 totalLength)
{
	BOOL rc = FALSE;
	size_t start = 0;
	size_t end = 0;
	size_t len = 0;
	UINT16 numberCapabilities = 0;
	UINT16 count = 0;

#ifdef WITH_DEBUG_CAPABILITIES
	const size_t capstart = Stream_GetPosition(s);
#endif

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, numberCapabilities); /* numberCapabilities (2 bytes) */
	Stream_Seek(s, 2);                         /* pad2Octets (2 bytes) */
	count = numberCapabilities;

	start = Stream_GetPosition(s);
	while (numberCapabilities > 0 && Stream_GetRemainingLength(s) >= 4)
	{
		UINT16 type = 0;
		UINT16 length = 0;
		wStream subbuffer;
		wStream* sub = NULL;

		if (!rdp_read_capability_set_header(s, &length, &type))
			goto fail;
		sub = Stream_StaticInit(&subbuffer, Stream_Pointer(s), length - 4);
		if (!Stream_SafeSeek(s, length - 4))
			goto fail;

		if (!rdp_read_capability_set(sub, type, rcvSettings, settings->ServerMode))
			goto fail;

		if (!rdp_apply_from_received(type, settings, rcvSettings))
			goto fail;
		numberCapabilities--;
	}

	end = Stream_GetPosition(s);
	len = end - start;

	if (numberCapabilities)
	{
		WLog_ERR(TAG,
		         "strange we haven't read the number of announced capacity sets, read=%d "
		         "expected=%" PRIu16 "",
		         count - numberCapabilities, count);
	}

#ifdef WITH_DEBUG_CAPABILITIES
	rdp_print_capability_sets(s, capstart, TRUE);
#endif

	if (len > totalLength)
	{
		WLog_ERR(TAG, "Capability length expected %" PRIu16 ", actual %" PRIdz, totalLength, len);
		goto fail;
	}
	rc = freerdp_capability_buffer_copy(settings, rcvSettings);
fail:
	return rc;
}

BOOL rdp_recv_get_active_header(rdpRdp* rdp, wStream* s, UINT16* pChannelId, UINT16* length)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->context);

	if (!rdp_read_header(rdp, s, length, pChannelId))
		return FALSE;

	if (freerdp_shall_disconnect_context(rdp->context))
		return TRUE;

	if (*pChannelId != MCS_GLOBAL_CHANNEL_ID)
	{
		UINT16 mcsMessageChannelId = rdp->mcs->messageChannelId;

		if ((mcsMessageChannelId == 0) || (*pChannelId != mcsMessageChannelId))
		{
			WLog_ERR(TAG, "unexpected MCS channel id %04" PRIx16 " received", *pChannelId);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL rdp_recv_demand_active(rdpRdp* rdp, wStream* s, UINT16 pduSource, UINT16 length)
{
	UINT16 lengthSourceDescriptor = 0;
	UINT16 lengthCombinedCapabilities = 0;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);
	WINPR_ASSERT(rdp->context);
	WINPR_ASSERT(s);

	rdp->settings->PduSource = pduSource;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, rdp->settings->ShareId);     /* shareId (4 bytes) */
	Stream_Read_UINT16(s, lengthSourceDescriptor);     /* lengthSourceDescriptor (2 bytes) */
	Stream_Read_UINT16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */

	if (!Stream_SafeSeek(s, lengthSourceDescriptor) ||
	    !Stream_CheckAndLogRequiredLength(TAG, s, 4)) /* sourceDescriptor */
		return FALSE;

	/* capabilitySets */
	if (!rdp_read_capability_sets(s, rdp->settings, rdp->remoteSettings,
	                              lengthCombinedCapabilities))
	{
		WLog_ERR(TAG, "rdp_read_capability_sets failed");
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	UINT32 SessionId = 0;
	Stream_Read_UINT32(s, SessionId); /* SessionId */

	{
		rdp_secondary_update_internal* secondary = secondary_update_cast(rdp->update->secondary);
		secondary->glyph_v2 = (rdp->settings->GlyphSupportLevel > GLYPH_SUPPORT_FULL);
	}

	return tpkt_ensure_stream_consumed(s, length);
}

static BOOL rdp_write_demand_active(wStream* s, rdpSettings* settings)
{
	size_t bm = 0;
	size_t em = 0;
	size_t lm = 0;
	UINT16 numberCapabilities = 0;
	size_t lengthCombinedCapabilities = 0;

	if (!Stream_EnsureRemainingCapacity(s, 64))
		return FALSE;

	Stream_Write_UINT32(s, settings->ShareId); /* shareId (4 bytes) */
	Stream_Write_UINT16(s, 4);                 /* lengthSourceDescriptor (2 bytes) */
	lm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s);     /* lengthCombinedCapabilities (2 bytes) */
	Stream_Write(s, "RDP", 4); /* sourceDescriptor */
	bm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s);     /* numberCapabilities (2 bytes) */
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

	if (freerdp_settings_get_bool(settings, FreeRDP_BitmapCachePersistEnabled))
	{
		numberCapabilities++;

		if (!rdp_write_bitmap_cache_host_support_capability_set(s, settings))
			return FALSE;
	}

	if (settings->RemoteApplicationMode)
	{
		numberCapabilities += 2;

		if (!rdp_write_remote_programs_capability_set(s, settings) ||
		    !rdp_write_window_list_capability_set(s, settings))
			return FALSE;
	}

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, lm); /* go back to lengthCombinedCapabilities */
	lengthCombinedCapabilities = (em - bm);
	if (lengthCombinedCapabilities > UINT16_MAX)
		return FALSE;
	Stream_Write_UINT16(
	    s, (UINT16)lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */
	Stream_SetPosition(s, bm);                  /* go back to numberCapabilities */
	Stream_Write_UINT16(s, numberCapabilities); /* numberCapabilities (2 bytes) */
#ifdef WITH_DEBUG_CAPABILITIES
	rdp_print_capability_sets(s, bm, FALSE);
#endif
	Stream_SetPosition(s, em);
	Stream_Write_UINT32(s, 0); /* sessionId */
	return TRUE;
}

BOOL rdp_send_demand_active(rdpRdp* rdp)
{
	wStream* s = rdp_send_stream_pdu_init(rdp);
	BOOL status = 0;

	if (!s)
		return FALSE;

	rdp->settings->ShareId = 0x10000 + rdp->mcs->userId;
	status = rdp_write_demand_active(s, rdp->settings) &&
	         rdp_send_pdu(rdp, s, PDU_TYPE_DEMAND_ACTIVE, rdp->mcs->userId);
	Stream_Release(s);
	return status;
}

BOOL rdp_recv_confirm_active(rdpRdp* rdp, wStream* s, UINT16 pduLength)
{
	rdpSettings* settings = NULL;
	UINT16 lengthSourceDescriptor = 0;
	UINT16 lengthCombinedCapabilities = 0;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);
	settings = rdp->settings;
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 10))
		return FALSE;

	Stream_Seek_UINT32(s);                             /* shareId (4 bytes) */
	Stream_Seek_UINT16(s);                             /* originatorId (2 bytes) */
	Stream_Read_UINT16(s, lengthSourceDescriptor);     /* lengthSourceDescriptor (2 bytes) */
	Stream_Read_UINT16(s, lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, lengthSourceDescriptor + 4U))
		return FALSE;

	Stream_Seek(s, lengthSourceDescriptor); /* sourceDescriptor */
	if (!rdp_read_capability_sets(s, rdp->settings, rdp->remoteSettings,
	                              lengthCombinedCapabilities))
		return FALSE;

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
		if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, FALSE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, FALSE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_JpegCodec, FALSE))
			return FALSE;
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

	return tpkt_ensure_stream_consumed(s, pduLength);
}

static BOOL rdp_write_confirm_active(wStream* s, rdpSettings* settings)
{
	size_t bm = 0;
	size_t em = 0;
	size_t lm = 0;
	UINT16 numberCapabilities = 0;
	UINT16 lengthSourceDescriptor = 0;
	size_t lengthCombinedCapabilities = 0;
	BOOL ret = 0;

	WINPR_ASSERT(settings);

	lengthSourceDescriptor = sizeof(SOURCE_DESCRIPTOR);
	Stream_Write_UINT32(s, settings->ShareId);      /* shareId (4 bytes) */
	Stream_Write_UINT16(s, 0x03EA);                 /* originatorId (2 bytes) */
	Stream_Write_UINT16(s, lengthSourceDescriptor); /* lengthSourceDescriptor (2 bytes) */
	lm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s); /* lengthCombinedCapabilities (2 bytes) */
	Stream_Write(s, SOURCE_DESCRIPTOR, lengthSourceDescriptor); /* sourceDescriptor */
	bm = Stream_GetPosition(s);
	Stream_Seek_UINT16(s);     /* numberCapabilities (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */
	/* Capability Sets */
	numberCapabilities = 15;

	if (!rdp_write_general_capability_set(s, settings) ||
	    !rdp_write_bitmap_capability_set(s, settings) ||
	    !rdp_write_order_capability_set(s, settings))
		return FALSE;

	if (settings->RdpVersion >= RDP_VERSION_5_PLUS)
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
	if (lengthCombinedCapabilities > UINT16_MAX)
		return FALSE;
	Stream_Write_UINT16(
	    s, (UINT16)lengthCombinedCapabilities); /* lengthCombinedCapabilities (2 bytes) */
	Stream_SetPosition(s, bm);                  /* go back to numberCapabilities */
	Stream_Write_UINT16(s, numberCapabilities); /* numberCapabilities (2 bytes) */
#ifdef WITH_DEBUG_CAPABILITIES
	rdp_print_capability_sets(s, bm, FALSE);
#endif
	Stream_SetPosition(s, em);

	return TRUE;
}

BOOL rdp_send_confirm_active(rdpRdp* rdp)
{
	wStream* s = rdp_send_stream_pdu_init(rdp);
	BOOL status = 0;

	if (!s)
		return FALSE;

	status = rdp_write_confirm_active(s, rdp->settings) &&
	         rdp_send_pdu(rdp, s, PDU_TYPE_CONFIRM_ACTIVE, rdp->mcs->userId);
	Stream_Release(s);
	return status;
}

const char* rdp_input_flag_string(UINT16 flags, char* buffer, size_t len)
{
	char prefix[16] = { 0 };

	(void)_snprintf(prefix, sizeof(prefix), "[0x%04" PRIx16 "][", flags);
	winpr_str_append(prefix, buffer, len, "");
	if ((flags & INPUT_FLAG_SCANCODES) != 0)
		winpr_str_append("INPUT_FLAG_SCANCODES", buffer, len, "|");
	if ((flags & INPUT_FLAG_MOUSEX) != 0)
		winpr_str_append("INPUT_FLAG_MOUSEX", buffer, len, "|");
	if ((flags & INPUT_FLAG_FASTPATH_INPUT) != 0)
		winpr_str_append("INPUT_FLAG_FASTPATH_INPUT", buffer, len, "|");
	if ((flags & INPUT_FLAG_UNICODE) != 0)
		winpr_str_append("INPUT_FLAG_UNICODE", buffer, len, "|");
	if ((flags & INPUT_FLAG_FASTPATH_INPUT2) != 0)
		winpr_str_append("INPUT_FLAG_FASTPATH_INPUT2", buffer, len, "|");
	if ((flags & INPUT_FLAG_UNUSED1) != 0)
		winpr_str_append("INPUT_FLAG_UNUSED1", buffer, len, "|");
	if ((flags & INPUT_FLAG_MOUSE_RELATIVE) != 0)
		winpr_str_append("INPUT_FLAG_MOUSE_RELATIVE", buffer, len, "|");
	if ((flags & TS_INPUT_FLAG_MOUSE_HWHEEL) != 0)
		winpr_str_append("TS_INPUT_FLAG_MOUSE_HWHEEL", buffer, len, "|");
	if ((flags & TS_INPUT_FLAG_QOE_TIMESTAMPS) != 0)
		winpr_str_append("TS_INPUT_FLAG_QOE_TIMESTAMPS", buffer, len, "|");
	winpr_str_append("]", buffer, len, "");
	return buffer;
}

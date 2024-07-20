/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * codec color
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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
#ifndef FREERDP_LIB_CODEC_COLOR_H
#define FREERDP_LIB_CODEC_COLOR_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <freerdp/codec/color.h>
#include <freerdp/log.h>

#define INT_COLOR_TAG FREERDP_TAG("codec.color.h")

static INLINE DWORD FreeRDPAreColorFormatsEqualNoAlpha_int(DWORD first, DWORD second)
{
	const DWORD mask = (DWORD) ~(8UL << 12UL);
	return (first & mask) == (second & mask);
}

static INLINE BOOL FreeRDPWriteColor_int(BYTE* WINPR_RESTRICT dst, UINT32 format, UINT32 color)
{
	switch (FreeRDPGetBitsPerPixel(format))
	{
		case 32:
			dst[0] = (BYTE)(color >> 24);
			dst[1] = (BYTE)(color >> 16);
			dst[2] = (BYTE)(color >> 8);
			dst[3] = (BYTE)color;
			break;

		case 24:
			dst[0] = (BYTE)(color >> 16);
			dst[1] = (BYTE)(color >> 8);
			dst[2] = (BYTE)color;
			break;

		case 16:
			dst[1] = (BYTE)(color >> 8);
			dst[0] = (BYTE)color;
			break;

		case 15:
			if (!FreeRDPColorHasAlpha(format))
				color = color & 0x7FFF;

			dst[1] = (BYTE)(color >> 8);
			dst[0] = (BYTE)color;
			break;

		case 8:
			dst[0] = (BYTE)color;
			break;

		default:
			WLog_ERR(INT_COLOR_TAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			return FALSE;
	}

	return TRUE;
}

static INLINE BOOL FreeRDPWriteColorIgnoreAlpha_int(BYTE* WINPR_RESTRICT dst, UINT32 format,
                                                    UINT32 color)
{
	switch (format)
	{
		case PIXEL_FORMAT_XBGR32:
		case PIXEL_FORMAT_XRGB32:
		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_ARGB32:
		{
			const UINT32 tmp = ((UINT32)dst[0] << 24ULL) | (color & 0x00FFFFFFULL);
			return FreeRDPWriteColor_int(dst, format, tmp);
		}
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_RGBX32:
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_RGBA32:
		{
			const UINT32 tmp = ((UINT32)dst[3]) | (color & 0xFFFFFF00ULL);
			return FreeRDPWriteColor_int(dst, format, tmp);
		}
		default:
			return FreeRDPWriteColor_int(dst, format, color);
	}
}

static INLINE UINT32 FreeRDPReadColor_int(const BYTE* WINPR_RESTRICT src, UINT32 format)
{
	UINT32 color = 0;

	switch (FreeRDPGetBitsPerPixel(format))
	{
		case 32:
			color =
			    ((UINT32)src[0] << 24) | ((UINT32)src[1] << 16) | ((UINT32)src[2] << 8) | src[3];
			break;

		case 24:
			color = ((UINT32)src[0] << 16) | ((UINT32)src[1] << 8) | src[2];
			break;

		case 16:
			color = ((UINT32)src[1] << 8) | src[0];
			break;

		case 15:
			color = ((UINT32)src[1] << 8) | src[0];

			if (!FreeRDPColorHasAlpha(format))
				color = color & 0x7FFF;

			break;

		case 8:
		case 4:
		case 1:
			color = *src;
			break;

		default:
			WLog_ERR(INT_COLOR_TAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			color = 0;
			break;
	}

	return color;
}

#endif

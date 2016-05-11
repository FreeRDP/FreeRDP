/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Color Conversion Routines
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <winpr/crt.h>

#include <freerdp/log.h>
#include <freerdp/freerdp.h>
#include <freerdp/primitives.h>

#define TAG FREERDP_TAG("color")

int freerdp_get_pixel(BYTE* data, int x, int y, int width, int height, int bpp)
{
	int start;
	int shift;
	UINT16* src16;
	UINT32* src32;
	int red, green, blue;

	switch (bpp)
	{
		case  1:
			width = (width + 7) / 8;
			start = (y * width) + x / 8;
			shift = x % 8;
			return (data[start] & (0x80 >> shift)) != 0;
		case 8:
			return data[y * width + x];
		case 15:
		case 16:
			src16 = (UINT16*) data;
			return src16[y * width + x];
		case 24:
			data += y * width * 3;
			data += x * 3;
			red = data[0];
			green = data[1];
			blue = data[2];
			return RGB24(red, green, blue);
		case 32:
			src32 = (UINT32*) data;
			return src32[y * width + x];
		default:
			break;
	}

	return 0;
}

void freerdp_set_pixel(BYTE* data, int x, int y, int width, int height, int bpp, int pixel)
{
	int start;
	int shift;
	int *dst32;

	if (bpp == 1)
	{
		width = (width + 7) / 8;
		start = (y * width) + x / 8;
		shift = x % 8;
		if (pixel)
			data[start] = data[start] | (0x80 >> shift);
		else
			data[start] = data[start] & ~(0x80 >> shift);
	}
	else if (bpp == 32)
	{
		dst32 = (int*) data;
		dst32[y * width + x] = pixel;
	}
}

static INLINE void freerdp_color_split_rgb(UINT32* color, int bpp, BYTE* red, BYTE* green, BYTE* blue, BYTE* alpha, HCLRCONV clrconv)
{
	*red = *green = *blue = 0;
	*alpha = (clrconv->alpha) ? 0xFF : 0x00;

	switch (bpp)
	{
		case 32:
			if (clrconv->alpha)
			{
				GetARGB32(*alpha, *red, *green, *blue, *color);
			}
			else
			{
				GetRGB32(*red, *green, *blue, *color);
			}
			break;

		case 24:
			GetRGB24(*red, *green, *blue, *color);
			break;

		case 16:
			GetRGB16(*red, *green, *blue, *color);
			break;

		case 15:
			GetRGB15(*red, *green, *blue, *color);
			break;

		case 8:
			*color &= 0xFF;
			*red = clrconv->palette->entries[*color].red;
			*green = clrconv->palette->entries[*color].green;
			*blue = clrconv->palette->entries[*color].blue;
			break;

		case 1:
			if (*color != 0)
			{
				*red = 0xFF;
				*green = 0xFF;
				*blue = 0xFF;
			}
			break;

		default:
			break;
	}
}

static INLINE void freerdp_color_split_bgr(UINT32* color, int bpp, BYTE* red, BYTE* green, BYTE* blue, BYTE* alpha, HCLRCONV clrconv)
{
	*red = *green = *blue = 0;
	*alpha = (clrconv->alpha) ? 0xFF : 0x00;

	switch (bpp)
	{
		case 32:
			if (clrconv->alpha)
			{
				GetABGR32(*alpha, *red, *green, *blue, *color);
			}
			else
			{
				GetBGR32(*red, *green, *blue, *color);
			}
			break;

		case 24:
			GetBGR24(*red, *green, *blue, *color);
			break;

		case 16:
			GetBGR16(*red, *green, *blue, *color);
			break;

		case 15:
			GetBGR15(*red, *green, *blue, *color);
			break;

		case 8:
			*color &= 0xFF;
			*red = clrconv->palette->entries[*color].red;
			*green = clrconv->palette->entries[*color].green;
			*blue = clrconv->palette->entries[*color].blue;
			break;

		case 1:
			if (*color != 0)
			{
				*red = 0xFF;
				*green = 0xFF;
				*blue = 0xFF;
			}
			break;

		default:
			break;
	}
}

static INLINE void freerdp_color_make_rgb(UINT32* color, int bpp, BYTE* red, BYTE* green, BYTE* blue, BYTE* alpha, HCLRCONV clrconv)
{
	switch (bpp)
	{
		case 32:
			*color = ARGB32(*alpha, *red, *green, *blue);
			break;

		case 24:
			*color = RGB24(*red, *green, *blue);
			break;

		case 16:
			if (clrconv->rgb555)
			{
				*color = RGB15(*red, *green, *blue);
			}
			else
			{
				*color = RGB16(*red, *green, *blue);
			}
			break;

		case 15:
			*color = RGB15(*red, *green, *blue);
			break;

		case 8:
			*color = RGB24(*red, *green, *blue);
			break;

		case 1:
			if ((*red != 0) || (*green != 0) || (*blue != 0))
				*color = 1;
			break;

		default:
			break;
	}
}

static INLINE void freerdp_color_make_bgr(UINT32* color, int bpp, BYTE* red, BYTE* green, BYTE* blue, BYTE* alpha, HCLRCONV clrconv)
{
	switch (bpp)
	{
		case 32:
			*color = ABGR32(*alpha, *red, *green, *blue);
			break;

		case 24:
			*color = BGR24(*red, *green, *blue);
			break;

		case 16:
			if (clrconv->rgb555)
			{
				*color = BGR15(*red, *green, *blue);
			}
			else
			{
				*color = BGR16(*red, *green, *blue);
			}
			break;

		case 15:
			*color = BGR15(*red, *green, *blue);
			break;

		case 8:
			*color = BGR24(*red, *green, *blue);
			break;

		case 1:
			if ((*red != 0) || (*green != 0) || (*blue != 0))
				*color = 1;
			break;

		default:
			break;
	}
}

UINT32 freerdp_color_convert_rgb(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	BYTE red = 0;
	BYTE green = 0;
	BYTE blue = 0;
	BYTE alpha = 0xFF;
	UINT32 dstColor = 0;

	freerdp_color_split_rgb(&srcColor, srcBpp, &red, &green, &blue, &alpha, clrconv);
	freerdp_color_make_rgb(&dstColor, dstBpp, &red, &green, &blue, &alpha, clrconv);

	return dstColor;
}

UINT32 freerdp_color_convert_bgr(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	BYTE red = 0;
	BYTE green = 0;
	BYTE blue = 0;
	BYTE alpha = 0xFF;
	UINT32 dstColor = 0;

	freerdp_color_split_bgr(&srcColor, srcBpp, &red, &green, &blue, &alpha, clrconv);
	freerdp_color_make_bgr(&dstColor, dstBpp, &red, &green, &blue, &alpha, clrconv);

	return dstColor;
}

UINT32 freerdp_color_convert_rgb_bgr(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	BYTE red = 0;
	BYTE green = 0;
	BYTE blue = 0;
	BYTE alpha = 0xFF;
	UINT32 dstColor = 0;

	freerdp_color_split_rgb(&srcColor, srcBpp, &red, &green, &blue, &alpha, clrconv);
	freerdp_color_make_bgr(&dstColor, dstBpp, &red, &green, &blue, &alpha, clrconv);

	return dstColor;
}

UINT32 freerdp_color_convert_bgr_rgb(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	BYTE red = 0;
	BYTE green = 0;
	BYTE blue = 0;
	BYTE alpha = 0xFF;
	UINT32 dstColor = 0;

	freerdp_color_split_bgr(&srcColor, srcBpp, &red, &green, &blue, &alpha, clrconv);
	freerdp_color_make_rgb(&dstColor, dstBpp, &red, &green, &blue, &alpha, clrconv);

	return dstColor;
}

UINT32 freerdp_color_convert_var(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	if (clrconv->invert)
		return freerdp_color_convert_var_bgr(srcColor, srcBpp, dstBpp, clrconv);
	else
		return freerdp_color_convert_var_rgb(srcColor, srcBpp, dstBpp, clrconv);
}

UINT32 freerdp_color_convert_var_rgb(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	if (srcBpp == 8)
	{
		BYTE alpha = 0xFF;
		UINT32 dstColor = 0;
		PALETTE_ENTRY* entry = &clrconv->palette->entries[srcColor & 0xFF];

		freerdp_color_make_bgr(&dstColor, dstBpp, &entry->red, &entry->green, &entry->blue, &alpha, clrconv);

		return dstColor;
	}

	if (srcBpp > 16)
		return freerdp_color_convert_bgr_rgb(srcColor, srcBpp, dstBpp, clrconv);
	else
		return freerdp_color_convert_rgb(srcColor, srcBpp, dstBpp, clrconv);
}

UINT32 freerdp_color_convert_var_bgr(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	if (srcBpp == 8)
	{
		BYTE alpha = 0xFF;
		UINT32 dstColor = 0;
		PALETTE_ENTRY* entry = &clrconv->palette->entries[srcColor & 0xFF];

		freerdp_color_make_rgb(&dstColor, dstBpp, &entry->red, &entry->green, &entry->blue, &alpha, clrconv);

		return dstColor;
	}

	if (srcBpp > 16)
		return freerdp_color_convert_bgr(srcColor, srcBpp, dstBpp, clrconv);
	else
		return freerdp_color_convert_rgb_bgr(srcColor, srcBpp, dstBpp, clrconv);
}

UINT32 freerdp_convert_gdi_order_color(UINT32 color, int bpp, UINT32 format, BYTE* palette)
{
	UINT32 r = 0;
	UINT32 g = 0;
	UINT32 b = 0;

	switch (bpp)
	{
		case 32:
			GetRGB32(r, g, b, color);
			break;

		case 24:
			GetRGB32(r, g, b, color);
			break;

		case 16:
			color = (color & (UINT32) 0xFF00) | ((color >> 16) & (UINT32) 0xFF);
			GetRGB16(r, g, b, color);
			break;

		case 15:
			color = (color & (UINT32) 0xFF00) | ((color >> 16) & (UINT32) 0xFF);
			GetRGB15(r, g, b, color);
			break;

		case 8:
			color = (color >> 16) & (UINT32) 0xFF;
			if (palette)
			{
				r = palette[(color * 4) + 2];
				g = palette[(color * 4) + 1];
				b = palette[(color * 4) + 0];
			}
			break;

		case 1:
			r = g = b = 0;
			if (color != 0)
				r = g = b = 0xFF;
			break;

		default:
			return color;
			break;
	}

	if (FREERDP_PIXEL_FORMAT_TYPE(format) == FREERDP_PIXEL_FORMAT_TYPE_ABGR)
		return BGR32(r, g, b);

	return RGB32(r, g, b);
}

BYTE* freerdp_image_convert_8bpp(BYTE* srcData, BYTE* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	int i;
	BYTE red;
	BYTE green;
	BYTE blue;
	UINT32 pixel;
	BYTE *src8;
	UINT16 *dst16;
	UINT32 *dst32;

	if (dstBpp == 8)
	{
		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height, 16);

		if (!dstData)
			return NULL;

		CopyMemory(dstData, srcData, width * height);

		return dstData;
	}
	else if (dstBpp == 15 || (dstBpp == 16 && clrconv->rgb555))
	{
		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 2, 16);

		if (!dstData)
			return NULL;

		dst16 = (UINT16*) dstData;

		for (i = width * height; i > 0; i--)
		{
			pixel = *srcData;
			srcData++;
			red = clrconv->palette->entries[pixel].red;
			green = clrconv->palette->entries[pixel].green;
			blue = clrconv->palette->entries[pixel].blue;
			pixel = (clrconv->invert) ? BGR15(red, green, blue) : RGB15(red, green, blue);
			*dst16 = pixel;
			dst16++;
		}

		return dstData;
	}
	else if (dstBpp == 16)
	{
		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 2, 16);

		if (!dstData)
			return NULL;

		dst16 = (UINT16*) dstData;

		for (i = width * height; i > 0; i--)
		{
			pixel = *srcData;
			srcData++;
			red = clrconv->palette->entries[pixel].red;
			green = clrconv->palette->entries[pixel].green;
			blue = clrconv->palette->entries[pixel].blue;
			pixel = (clrconv->invert) ? BGR16(red, green, blue) : RGB16(red, green, blue);
			*dst16 = pixel;
			dst16++;
		}

		return dstData;
	}
	else if (dstBpp == 32)
	{
		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 4, 16);

		if (!dstData)
			return NULL;

		src8 = (BYTE*) srcData;
		dst32 = (UINT32*) dstData;

		for (i = width * height; i > 0; i--)
		{
			pixel = *src8;
			src8++;

			red = clrconv->palette->entries[pixel].red;
			green = clrconv->palette->entries[pixel].green;
			blue = clrconv->palette->entries[pixel].blue;

			if (clrconv->alpha)
			{
				pixel = (clrconv->invert) ? ABGR32(0xFF, red, green, blue) : ARGB32(0xFF, red, green, blue);
			}
			else
			{
				pixel = (clrconv->invert) ? BGR32(red, green, blue) : RGB32(red, green, blue);
			}

			*dst32 = pixel;
			dst32++;
		}

		return dstData;
	}

	return srcData;
}

BYTE* freerdp_image_convert_15bpp(BYTE* srcData, BYTE* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	int i;
	BYTE red;
	BYTE green;
	BYTE blue;
	UINT32 pixel;
	UINT16 *src16;
	UINT16 *dst16;
	UINT32 *dst32;

	if (dstBpp == 15 || (dstBpp == 16 && clrconv->rgb555))
	{
		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 2, 16);

		if (!dstData)
			return NULL;

		CopyMemory(dstData, srcData, width * height * 2);

		return dstData;
	}
	else if (dstBpp == 32)
	{
		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 4, 16);

		if (!dstData)
			return NULL;

		src16 = (UINT16*) srcData;
		dst32 = (UINT32*) dstData;

		for (i = width * height; i > 0; i--)
		{
			pixel = *src16;
			src16++;
			GetBGR15(red, green, blue, pixel);
			if (clrconv->alpha)
			{
				pixel = (clrconv->invert) ? ARGB32(0xFF, red, green, blue) : ABGR32(0xFF, red, green, blue);
			}
			else
			{
				pixel = (clrconv->invert) ? RGB32(red, green, blue) : BGR32(red, green, blue);
			}
			*dst32 = pixel;
			dst32++;
		}

		return dstData;
	}
	else if (dstBpp == 16)
	{
		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 2, 16);

		if (!dstData)
			return NULL;

		src16 = (UINT16*) srcData;
		dst16 = (UINT16*) dstData;

		for (i = width * height; i > 0; i--)
		{
			pixel = *src16;
			src16++;
			GetRGB_555(red, green, blue, pixel);
			RGB_555_565(red, green, blue);
			pixel = (clrconv->invert) ? BGR565(red, green, blue) : RGB565(red, green, blue);
			*dst16 = pixel;
			dst16++;
		}

		return dstData;
	}

	return srcData;
}

BYTE* freerdp_image_convert_16bpp(BYTE* srcData, BYTE* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	if (srcBpp == 15)
		return freerdp_image_convert_15bpp(srcData, dstData, width, height, srcBpp, dstBpp, clrconv);

	if (dstBpp == 16)
	{
		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 2, 16);

		if (!dstData)
			return NULL;

		if (clrconv->rgb555)
		{
			int i;
			BYTE red, green, blue;
			UINT16* src16 = (UINT16 *) srcData;
			UINT16* dst16 = (UINT16 *) dstData;

			for (i = width * height; i > 0; i--)
			{
				GetRGB_565(red, green, blue, (*src16));
				RGB_565_555(red, green, blue);
				(*dst16) = (clrconv->invert) ? BGR555(red, green, blue) : RGB555(red, green, blue);
				src16++;
				dst16++;
			}
		}
		else
		{
			CopyMemory(dstData, srcData, width * height * 2);
		}

		return dstData;
	}
	else if (dstBpp == 24)
	{
		int i;
		BYTE *dst8;
		UINT16 *src16;
		BYTE red, green, blue;

		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 3, 16);

		if (!dstData)
			return NULL;

		dst8 = (BYTE*) dstData;
		src16 = (UINT16*) srcData;

		for (i = width * height; i > 0; i--)
		{
			GetBGR16(red, green, blue, *src16);
			src16++;

			if (clrconv->invert)
			{
				*dst8++ = blue;
				*dst8++ = green;
				*dst8++ = red;
			}
			else
			{
				*dst8++ = red;
				*dst8++ = green;
				*dst8++ = blue;
			}
		}

		return dstData;
	}
	else if (dstBpp == 32)
	{
		primitives_t* prims;

		if (!dstData)
			dstData = _aligned_malloc(width * height * sizeof(UINT32), 16);

		if (!dstData)
			return NULL;

		prims = primitives_get();
		prims->RGB565ToARGB_16u32u_C3C4(
			(const UINT16*) srcData, width * sizeof(UINT16),
			(UINT32*) dstData, width * sizeof(UINT32),
			width, height, clrconv->alpha, clrconv->invert);

		return dstData;
	}

	return srcData;
}

BYTE* freerdp_image_convert_24bpp(BYTE* srcData, BYTE* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	int i;

	if (dstBpp == 32)
	{
		UINT32 pixel, alpha_mask, temp;
		UINT32* srcp;
		UINT32* dstp;

		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 4, 16);

		if (!dstData)
			return NULL;

		alpha_mask = clrconv->alpha ? 0xFF000000 : 0;

		srcp = (UINT32*) srcData;
		dstp = (UINT32*) dstData;

		if (clrconv->invert)
		{
			/* Each iteration handles four pixels using 32-bit load and
			   store operations. */
			for (i = ((width * height) / 4); i > 0; i--)
			{
				temp = 0;

				pixel = temp;
				temp = *srcp++;
				pixel |= temp & 0x00FFFFFF;
				temp = temp >> 24;
				*dstp++ = alpha_mask | RGB32_to_BGR32(pixel);

				pixel = temp;
				temp = *srcp++;
				pixel |= (temp & 0x0000FFFF) << 8;
				temp = temp >> 16;
				*dstp++ = alpha_mask | RGB32_to_BGR32(pixel);

				pixel = temp;
				temp = *srcp++;
				pixel |= (temp & 0x000000FF) << 16;
				temp = temp >> 8;
				*dstp++ = alpha_mask | RGB32_to_BGR32(pixel);

				*dstp++ = alpha_mask | RGB32_to_BGR32(temp);
			}

			/* Handle any remainder. */
			for (i = (width * height) % 4; i > 0; i--)
			{
				pixel = ABGR32(alpha_mask, srcData[2], srcData[1], srcData[0]);
				*dstp++ = pixel;
				srcData += 3;
			}
		}
		else
		{
			for (i = ((width * height) / 4); i > 0; i--)
			{
				temp = 0;

				pixel = temp;
				temp = *srcp++;
				pixel |= temp & 0x00FFFFFF;
				temp = temp >> 24;
				*dstp++ = alpha_mask | pixel;

				pixel = temp;
				temp = *srcp++;
				pixel |= (temp & 0x0000FFFF) << 8;
				temp = temp >> 16;
				*dstp++ = alpha_mask | pixel;

				pixel = temp;
				temp = *srcp++;
				pixel |= (temp & 0x000000FF) << 16;
				temp = temp >> 8;
				*dstp++ = alpha_mask | pixel;

				*dstp++ = alpha_mask | temp;
			}

			for (i = (width * height) % 4; i > 0; i--)
			{
				pixel = ARGB32(alpha_mask, srcData[2], srcData[1], srcData[0]);
				*dstp++ = pixel;
				srcData += 3;
			}
		}

		return dstData;
	}

	return srcData;
}

BYTE* freerdp_image_convert_32bpp(BYTE* srcData, BYTE* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	if (dstBpp == 16)
	{
		int index;
		UINT16 *dst16;
		UINT32 *src32;
		BYTE red, green, blue;

		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 2, 16);

		if (!dstData)
			return NULL;

		dst16 = (UINT16*) dstData;
		src32 = (UINT32*) srcData;

		for (index = 0; index < width * height; index++)
		{
			GetBGR32(blue, green, red, *src32);
			*dst16 = (clrconv->invert) ? BGR16(red, green, blue) : RGB16(red, green, blue);
			src32++;
			dst16++;
		}

		return dstData;
	}
	else if (dstBpp == 24)
	{
		int index;
		BYTE* dstp;
		BYTE red, green, blue;

		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 3, 16);

		if (!dstData)
			return NULL;

		dstp = dstData;

		for (index = 0; index < width * height; index++)
		{
			red = *(srcData++);
			green = *(srcData++);
			blue = *(srcData++);

			if (clrconv->invert)
			{
				*dstp++ = blue;
				*dstp++ = green;
				*dstp++ = red;
			}
			else
			{
				*dstp++ = red;
				*dstp++ = green;
				*dstp++ = blue;
			}

			srcData++;
		}

		return dstData;
	}
	else if (dstBpp == 32)
	{
		int i;
		UINT32 pixel;
		UINT32 alpha_mask;
		UINT32* srcp;
		UINT32* dstp;
		BYTE red, green, blue;

		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 4, 16);

		if (!dstData)
			return NULL;

		alpha_mask = clrconv->alpha ? 0xFF000000 : 0;

		srcp = (UINT32*) srcData;
		dstp = (UINT32*) dstData;

		if (clrconv->invert)
		{
			for (i = width * height; i > 0; i--)
			{
				pixel = *srcp;
				srcp++;
				GetRGB32(red, green, blue, pixel);
				pixel = alpha_mask | BGR32(red, green, blue);
				*dstp = pixel;
				dstp++;
			}
		}
		else
		{
			for (i = width * height; i > 0; i--)
			{
				pixel = *srcp;
				srcp++;
				GetRGB32(red, green, blue, pixel);
				pixel = alpha_mask | RGB32(red, green, blue);
				*dstp = pixel;
				dstp++;
			}
		}

		return dstData;
	}

	return srcData;
}

p_freerdp_image_convert freerdp_image_convert_[5] =
{
	NULL,
	freerdp_image_convert_8bpp,
	freerdp_image_convert_16bpp,
	freerdp_image_convert_24bpp,
	freerdp_image_convert_32bpp
};

BYTE* freerdp_image_convert(BYTE* srcData, BYTE* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	p_freerdp_image_convert _p_freerdp_image_convert = freerdp_image_convert_[IBPP(srcBpp)];

	if (_p_freerdp_image_convert != NULL)
		return _p_freerdp_image_convert(srcData, dstData, width, height, srcBpp, dstBpp, clrconv);
	else
		return 0;
}

void freerdp_bitmap_flip(BYTE * src, BYTE * dst, int scanLineSz, int height)
{
	int i;

	BYTE * bottomLine = dst + (scanLineSz * (height - 1));
	BYTE * topLine = src;

	/* Special processing if called for flip-in-place. */
	if (src == dst)
	{
		/* Allocate a scanline buffer.
		 * (FIXME: malloc / xfree below should be replaced by "get/put
		 * scanline buffer from a pool/Q of fixed buffers" to reuse
		 * fixed size buffers (of max scanline size (or adaptative?) )
		 * -- would be much faster).
		 */
		BYTE* tmpBfr = _aligned_malloc(scanLineSz, 16);
		int half = height / 2;
		/* Flip buffer in place by line permutations through the temp
		 * scan line buffer.
		 * Note that if height has an odd number of line, we don't need
		 * to move the center scanline anyway.
		 * Also note that in place flipping takes three memcpy() calls
		 * to process two scanlines while src to distinct dest would
		 * only requires two memcpy() calls for two scanlines.
		 */
		height--;
		for (i = 0; i < half ; i++)
		{
			CopyMemory(tmpBfr, topLine, scanLineSz);
			CopyMemory(topLine, bottomLine, scanLineSz);
			CopyMemory(bottomLine, tmpBfr, scanLineSz);
			topLine += scanLineSz;
			bottomLine -= scanLineSz;
			height--;
		}
		_aligned_free(tmpBfr);
	}
	/* Flip from source buffer to destination buffer. */
	else
	{

		for (i = 0; i < height; i++)
		{
			CopyMemory(bottomLine, topLine, scanLineSz);
			topLine += scanLineSz;
			bottomLine -= scanLineSz;
		}
	}

}

BYTE* freerdp_image_flip(BYTE* srcData, BYTE* dstData, int width, int height, int bpp)
{
	int scanline;

	scanline = width * ((bpp + 7) / 8);

	if (!dstData)
		dstData = (BYTE*) _aligned_malloc(width * height * ((bpp + 7) / 8), 16);

	if (!dstData)
		return NULL;

	freerdp_bitmap_flip(srcData, dstData, scanline, height);

	return dstData;
}

BYTE* freerdp_icon_convert(BYTE* srcData, BYTE* dstData, BYTE* mask, int width, int height, int bpp, HCLRCONV clrconv)
{
	BYTE* data;
	BYTE bmask;
	UINT32* icon;
	int x, y, bit;
	int maskIndex;

	if (bpp == 16)
	{
		/* Server sends 16 bpp field, but data is usually 15-bit 555 */
		bpp = 15;
	}

	data = freerdp_image_flip(srcData, dstData, width, height, bpp);
	dstData = freerdp_image_convert(data, NULL, width, height, bpp, 32, clrconv);
	_aligned_free(data);

	/* Read the AND alpha plane */
	if (bpp < 32)
	{
		maskIndex = 0;
		icon = (UINT32*) dstData;

		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width-7; x+=8)
			{
				bmask = mask[maskIndex++];

				for (bit = 0; bit < 8; bit++)
					if ((bmask & (0x80 >> bit)) == 0)
					{
						UINT32 *tmp = (icon + (height - y - 1) * width + x + bit);
						if (tmp)
							*tmp |= 0xFF000000;
					}
			}

			if ((width % 8) != 0)
			{
				bmask = mask[maskIndex++];

				for (bit = 0; bit < width % 8; bit++)
					if ((bmask & (0x80 >> bit)) == 0)
					{
						UINT32 *tmp = (icon + (height - y - 1) * width + x + bit);
						if (tmp)
							*tmp |= 0xFF000000;
					}
			}

			/* Skip padding */
			if ((width % 32) != 0)
				maskIndex += (32 - (width % 32)) / 8;
		}
	}

	return dstData;
}

BYTE* freerdp_glyph_convert(int width, int height, BYTE* data)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
	BYTE* dstData;
	int scanline;

	/*
	 * converts a 1-bit-per-pixel glyph to a one-byte-per-pixel glyph:
	 * this approach uses a little more memory, but provides faster
	 * means of accessing individual pixels in blitting operations
	 */

	scanline = (width + 7) / 8;
	dstData = (BYTE*) _aligned_malloc(width * height, 16);

	if (!dstData)
		return NULL;

	ZeroMemory(dstData, width * height);
	dstp = dstData;

	for (y = 0; y < height; y++)
	{
		srcp = data + (y * scanline);

		for (x = 0; x < width; x++)
		{
			if ((*srcp & (0x80 >> (x % 8))) != 0)
				*dstp = 0xFF;
			dstp++;

			if (((x + 1) % 8 == 0) && x != 0)
				srcp++;
		}
	}

	return dstData;
}

BYTE* freerdp_mono_image_convert(BYTE* srcData, int width, int height, int srcBpp, int dstBpp, UINT32 bgcolor, UINT32 fgcolor, HCLRCONV clrconv)
{
	int index;
	UINT16* dst16;
	UINT32* dst32;
	BYTE* dstData;
	BYTE bitMask;
	int bitIndex;
	BYTE redBg, greenBg, blueBg;
	BYTE redFg, greenFg, blueFg;

	GetRGB32(redBg, greenBg, blueBg, bgcolor);
	GetRGB32(redFg, greenFg, blueFg, fgcolor);

	if (dstBpp == 16)
	{
		dstData = (BYTE*) _aligned_malloc(width * height * 2, 16);

		if (!dstData)
			return NULL;

		dst16 = (UINT16*) dstData;

		if (clrconv->rgb555)
		{
			bgcolor = clrconv->invert ? BGR15(redBg, greenBg, blueBg) : RGB15(redBg, greenBg, blueBg);
			fgcolor = clrconv->invert ? BGR15(redFg, greenFg, blueFg) : RGB15(redFg, greenFg, blueFg);
		}
		else
		{
			bgcolor = clrconv->invert ? BGR16(redBg, greenBg, blueBg) : RGB16(redBg, greenBg, blueBg);
			fgcolor = clrconv->invert ? BGR16(redFg, greenFg, blueFg) : RGB16(redFg, greenFg, blueFg);
		}

		for (index = height; index > 0; index--)
		{
			/* each bit encodes a pixel */
			bitMask = *srcData;
			for (bitIndex = 7; bitIndex >= 0; bitIndex--)
			{
				if ((bitMask >> bitIndex) & 0x01)
				{
					*dst16 = bgcolor;
				}
				else
				{
					*dst16 = fgcolor;
				}
				dst16++;
			}
			srcData++;
		}
		return dstData;
	}
	else if (dstBpp == 32)
	{
		dstData = (BYTE*) _aligned_malloc(width * height * 4, 16);

		if (!dstData)
			return NULL;

		dst32 = (UINT32*) dstData;

		for (index = height; index > 0; index--)
		{
			/* each bit encodes a pixel */
			bitMask = *srcData;

			for (bitIndex = 7; bitIndex >= 0; bitIndex--)
			{
				if ((bitMask >> bitIndex) & 0x01)
				{
					if (clrconv->alpha)
					{
						*dst32 = (clrconv->invert) ? ABGR32(0xFF, redBg, greenBg, blueBg) : ARGB32(0xFF, redBg, greenBg, blueBg);
					}
					else
					{
						*dst32 = (clrconv->invert) ? BGR32(redBg, greenBg, blueBg) : RGB32(redBg, greenBg, blueBg);
					}
				}
				else
				{
					if (clrconv->alpha)
					{
						*dst32 = (clrconv->invert) ? ABGR32(0xFF, redFg, greenFg, blueFg) : ARGB32(0xFF, redFg, greenFg, blueFg);
					}
					else
					{
						*dst32 = (clrconv->invert) ? BGR32(redFg, greenFg, blueFg) : RGB32(redFg, greenFg, blueFg);
					}
				}
				dst32++;
			}
			srcData++;
		}
		return dstData;
	}

	return srcData;
}

int freerdp_image_copy_from_monochrome(BYTE* pDstData, UINT32 DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, UINT32 backColor, UINT32 foreColor, BYTE* palette)
{
	int x, y;
	BOOL vFlip;
	BOOL invert;
	int srcFlip;
	int dstFlip;
	int nDstPad;
	int monoStep;
	UINT32 monoBit;
	BYTE* monoBits;
	UINT32 monoPixel;
	BYTE a, r, g, b;
	int dstBitsPerPixel;
	int dstBytesPerPixel;

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat);

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	srcFlip = FREERDP_PIXEL_FLIP_NONE;
	vFlip = (srcFlip != dstFlip) ? TRUE : FALSE;

	invert = (FREERDP_PIXEL_FORMAT_IS_ABGR(DstFormat)) ? TRUE : FALSE;

	backColor |= 0xFF000000;
	foreColor |= 0xFF000000;

	monoStep = (nWidth + 7) / 8;

	if (dstBytesPerPixel == 4)
	{
		UINT32* pDstPixel;

		if (invert)
		{
			GetARGB32(a, r, g, b, backColor);
			backColor = ABGR32(a, r, g, b);

			GetARGB32(a, r, g, b, foreColor);
			foreColor = ABGR32(a, r, g, b);
		}

		pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

		for (y = 0; y < nHeight; y++)
		{
			monoBit = 0x80;

			if (!vFlip)
				monoBits = &pSrcData[monoStep * y];
			else
				monoBits = &pSrcData[monoStep * (nHeight - y - 1)];

			for (x = 0; x < nWidth; x++)
			{
				monoPixel = (*monoBits & monoBit) ? 1 : 0;
				if (!(monoBit >>= 1)) { monoBits++; monoBit = 0x80; }

				if (monoPixel)
					*pDstPixel++ = backColor;
				else
					*pDstPixel++ = foreColor;
			}

			pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
		}

		return 1;
	}
	else if (dstBytesPerPixel == 2)
	{
		UINT16* pDstPixel;
		UINT16 backColor16;
		UINT16 foreColor16;

		if (!invert)
		{
			if (dstBitsPerPixel == 15)
			{
				GetRGB32(r, g, b, backColor);
				backColor16 = RGB15(r, g, b);

				GetRGB32(r, g, b, foreColor);
				foreColor16 = RGB15(r, g, b);
			}
			else
			{
				GetRGB32(r, g, b, backColor);
				backColor16 = RGB16(r, g, b);

				GetRGB32(r, g, b, foreColor);
				foreColor16 = RGB16(r, g, b);
			}
		}
		else
		{
			if (dstBitsPerPixel == 15)
			{
				GetRGB32(r, g, b, backColor);
				backColor16 = BGR15(r, g, b);

				GetRGB32(r, g, b, foreColor);
				foreColor16 = BGR15(r, g, b);
			}
			else
			{
				GetRGB32(r, g, b, backColor);
				backColor16 = BGR16(r, g, b);

				GetRGB32(r, g, b, foreColor);
				foreColor16 = BGR16(r, g, b);
			}
		}

		pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

		for (y = 0; y < nHeight; y++)
		{
			monoBit = 0x80;

			if (!vFlip)
				monoBits = &pSrcData[monoStep * y];
			else
				monoBits = &pSrcData[monoStep * (nHeight - y - 1)];

			for (x = 0; x < nWidth; x++)
			{
				monoPixel = (*monoBits & monoBit) ? 1 : 0;
				if (!(monoBit >>= 1)) { monoBits++; monoBit = 0x80; }

				if (monoPixel)
					*pDstPixel++ = backColor16;
				else
					*pDstPixel++ = foreColor16;
			}

			pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
		}

		return 1;
	}

	WLog_ERR(TAG, "failure: dstBytesPerPixel: %d dstBitsPerPixel: %d",
			dstBytesPerPixel, dstBitsPerPixel);

	return -1;
}

void freerdp_alpha_cursor_convert(BYTE* alphaData, BYTE* xorMask, BYTE* andMask, int width, int height, int bpp, HCLRCONV clrconv)
{
	UINT32 xorPixel;
	UINT32 andPixel;
	UINT32 x, y, jj;

	for (y = 0; y < height; y++)
	{
		jj = (bpp == 1) ? y : (height - 1) - y;

		for (x = 0; x < width; x++)
		{
			xorPixel = freerdp_get_pixel(xorMask, x, jj, width, height, bpp);
			xorPixel = freerdp_color_convert_rgb(xorPixel, bpp, 32, clrconv);
			andPixel = freerdp_get_pixel(andMask, x, jj, width, height, 1);

			if (andPixel)
			{
				if ((xorPixel & 0xFFFFFF) == 0xFFFFFF)
				{
					/* use pattern (not solid black) for xor area */
					xorPixel = (x & 1) == (y & 1);
					xorPixel = xorPixel ? 0xFFFFFF : 0;
					xorPixel |= 0xFF000000;
				}
				else if (xorPixel == 0xFF000000)
				{
					xorPixel = 0;
				}
			}

			freerdp_set_pixel(alphaData, x, y, width, height, 32, xorPixel);
		}
	}
}

static INLINE UINT32 freerdp_image_inverted_pointer_color(int x, int y)
{
#if 1
	/**
	 * Inverted pointer colors (where individual pixels can change their
	 * color to accommodate the background behind them) only seem to be
	 * supported on Windows.
	 * Using a static replacement color for these pixels (e.g. black)
	 * might result in invisible pointers depending on the background.
	 * This function returns either black or white, depending on the
	 * pixel's position.
	 */

	return (x + y) & 1 ? 0xFF000000 : 0xFFFFFFFF;
#else
	return 0xFF000000;
#endif
}

/**
 * Drawing Monochrome Pointers:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff556143/
 *
 * Drawing Color Pointers:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff556138/
 */

int freerdp_image_copy_from_pointer_data(BYTE* pDstData, UINT32 DstFormat,
					 int nDstStep, int nXDst, int nYDst,
					 int nWidth, int nHeight, BYTE* xorMask,
					 UINT32 xorMaskLength, BYTE* andMask,
					 UINT32 andMaskLength, UINT32 xorBpp,
					 BYTE* palette)
{
	int x, y;
	BOOL vFlip;
	BOOL invert;
	int srcFlip;
	int dstFlip;
	int nDstPad;
	int xorStep;
	int andStep;
	UINT32 xorBit;
	UINT32 andBit;
	BYTE* xorBits;
	BYTE* andBits;
	UINT32 xorPixel;
	UINT32 andPixel;
	int dstBitsPerPixel;
	int dstBytesPerPixel;

	xorBits = xorMask;
	andBits = andMask;

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat);

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	srcFlip = (xorBpp == 1) ? FREERDP_PIXEL_FLIP_NONE : FREERDP_PIXEL_FLIP_VERTICAL;

	vFlip = (srcFlip != dstFlip) ? TRUE : FALSE;
	invert = (FREERDP_PIXEL_FORMAT_IS_ABGR(DstFormat)) ? TRUE : FALSE;

	andStep = (nWidth + 7) / 8;
	andStep += (andStep % 2);

	if (!xorMask || (xorMaskLength == 0))
		return -1;

	if (dstBytesPerPixel == 4)
	{
		UINT32* pDstPixel;

		if (xorBpp == 1)
		{
			if (!andMask || (andMaskLength == 0))
				return -1;

			xorStep = (nWidth + 7) / 8;
			xorStep += (xorStep % 2);

			if (xorStep * nHeight > xorMaskLength)
				return -1;

			if (andStep * nHeight > andMaskLength)
				return -1;

			pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

			for (y = 0; y < nHeight; y++)
			{
				xorBit = andBit = 0x80;

				if (!vFlip)
				{
					xorBits = &xorMask[xorStep * y];
					andBits = &andMask[andStep * y];
				}
				else
				{
					xorBits = &xorMask[xorStep * (nHeight - y - 1)];
					andBits = &andMask[andStep * (nHeight - y - 1)];
				}

				for (x = 0; x < nWidth; x++)
				{
					xorPixel = (*xorBits & xorBit) ? 1 : 0;
					if (!(xorBit >>= 1)) { xorBits++; xorBit = 0x80; }

					andPixel = (*andBits & andBit) ? 1 : 0;
					if (!(andBit >>= 1)) { andBits++; andBit = 0x80; }

					if (!andPixel && !xorPixel)
						*pDstPixel++ = 0xFF000000; /* black */
					else if (!andPixel && xorPixel)
						*pDstPixel++ = 0xFFFFFFFF; /* white */
					else if (andPixel && !xorPixel)
						*pDstPixel++ = 0x00000000; /* transparent */
					else if (andPixel && xorPixel)
						*pDstPixel++ = freerdp_image_inverted_pointer_color(x, y); /* inverted */
				}

				pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
			}

			return 1;
		}
		else if (xorBpp == 24 || xorBpp == 32 || xorBpp == 16 || xorBpp == 8)
		{
			int xorBytesPerPixel = xorBpp >> 3;
			xorStep = nWidth * xorBytesPerPixel;
			pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

			if (xorBpp == 8 && !palette)
			{
				WLog_ERR(TAG, "null palette in conversion from %d bpp to %d bpp",
						 xorBpp, dstBitsPerPixel);
				return -1;
			}

			if (xorStep * nHeight > xorMaskLength)
				return -1;

			if (andMask)
			{
				if (andStep * nHeight > andMaskLength)
					return -1;
			}

			for (y = 0; y < nHeight; y++)
			{
				andBit = 0x80;

				if (!vFlip)
				{
					if (andMask)
						andBits = &andMask[andStep * y];
					xorBits = &xorMask[xorStep * y];
				}
				else
				{
					if (andMask)
						andBits = &andMask[andStep * (nHeight - y - 1)];
					xorBits = &xorMask[xorStep * (nHeight - y - 1)];
				}

				for (x = 0; x < nWidth; x++)
				{
					BOOL ignoreAndMask = FALSE;

					if (xorBpp == 32)
					{
						xorPixel = *((UINT32*) xorBits);
						if (xorPixel & 0xFF000000)
							ignoreAndMask = TRUE;
					}
					else if (xorBpp == 16)
					{
						UINT16 r, g, b;
						GetRGB16(r, g, b, *(UINT16*)xorBits);
						xorPixel = ARGB32(0xFF, r, g, b);
					}
					else if (xorBpp == 8)
					{
						xorPixel = 0xFF << 24 | ((UINT32*)palette)[xorBits[0]];
					}
					else
					{
						xorPixel = xorBits[0] | xorBits[1] << 8 | xorBits[2] << 16 | 0xFF << 24;
					}

					xorBits += xorBytesPerPixel;

					andPixel = 0;
					if (andMask)
					{
						andPixel = (*andBits & andBit) ? 1 : 0;
						if (!(andBit >>= 1)) { andBits++; andBit = 0x80; }
					}

					/* Ignore the AND mask, if the color format already supplies alpha data. */
					if (andPixel && !ignoreAndMask)
					{
						const UINT32 xorPixelMasked = xorPixel | 0xFF000000;

						if (xorPixelMasked == 0xFF000000) /* black -> transparent */
							*pDstPixel++ = 0x00000000;
						else if (xorPixelMasked == 0xFFFFFFFF) /* white -> inverted */
							*pDstPixel++ = freerdp_image_inverted_pointer_color(x, y);
						else
							*pDstPixel++ = xorPixel;
					}
					else
					{
						*pDstPixel++ = xorPixel;
					}
				}

				pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
			}

			return 1;
		}
	}

	WLog_ERR(TAG, "failed to convert from %d bpp to %d bpp",
			xorBpp, dstBitsPerPixel);

	return -1;
}

void freerdp_image_swap_color_order(BYTE* data, int width, int height)
{
	int x, y;
	UINT32* pixel;
	BYTE a, r, g, b;

	pixel = (UINT32*) data;

	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			GetARGB32(a, r, g, b, *pixel);
			*pixel = ABGR32(a, r, g, b);
			pixel++;
		}
	}
}

HCLRCONV freerdp_clrconv_new(UINT32 flags)
{
	HCLRCONV clrconv;

	clrconv = (CLRCONV*) calloc(1, sizeof(CLRCONV));

	if (!clrconv)
		return NULL;

	clrconv->alpha = (flags & CLRCONV_ALPHA) ? TRUE : FALSE;
	clrconv->invert = (flags & CLRCONV_INVERT) ? TRUE : FALSE;
	clrconv->rgb555 = (flags & CLRCONV_RGB555) ? TRUE : FALSE;

	clrconv->palette = (rdpPalette*) calloc(1, sizeof(rdpPalette));

	if (!clrconv->palette)
	{
		free (clrconv);
		return NULL;
	}

	return clrconv;
}

void freerdp_clrconv_free(HCLRCONV clrconv)
{
	if (clrconv)
	{
		free(clrconv->palette);
		free(clrconv);
	}
}

int freerdp_image1_copy(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD SrcFormat, int nSrcStep, int nXSrc, int nYSrc, BYTE* palette)
{
	int x, y;
	int nSrcPad;
	int nDstPad;
	int nAlignedWidth;
	int dstBitsPerPixel;
	int dstBytesPerPixel;
	BOOL vFlip = FALSE;
	BOOL invert = FALSE;

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);

	nAlignedWidth = nWidth + nWidth % 8;

	if (nSrcStep < 0)
		nSrcStep = nAlignedWidth / 8;

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nSrcPad = (nSrcStep - (nAlignedWidth / 8));
	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	if (FREERDP_PIXEL_FORMAT_IS_ABGR(DstFormat))
		invert = TRUE;

	if (FREERDP_PIXEL_FORMAT_FLIP(DstFormat) == FREERDP_PIXEL_FLIP_VERTICAL)
		vFlip = TRUE;

	if (dstBytesPerPixel == 4)
	{
		BYTE SrcPixel;
		BYTE* pSrcPixel;
		UINT32* pDstPixel;

		if (!invert)
		{
			if (!vFlip)
			{
				pSrcPixel = &pSrcData[nYSrc * nSrcStep];
				pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

				for (y = 0; y < nHeight; y++)
				{
					for (x = 0; x < nWidth / 8; x++)
					{
						SrcPixel = *pSrcPixel;
						pDstPixel[0] = (SrcPixel & 0x80) ? 0xFFFFFFFF : 0xFF000000;
						pDstPixel[1] = (SrcPixel & 0x40) ? 0xFFFFFFFF : 0xFF000000;
						pDstPixel[2] = (SrcPixel & 0x20) ? 0xFFFFFFFF : 0xFF000000;
						pDstPixel[3] = (SrcPixel & 0x10) ? 0xFFFFFFFF : 0xFF000000;
						pDstPixel[4] = (SrcPixel & 0x08) ? 0xFFFFFFFF : 0xFF000000;
						pDstPixel[5] = (SrcPixel & 0x04) ? 0xFFFFFFFF : 0xFF000000;
						pDstPixel[6] = (SrcPixel & 0x02) ? 0xFFFFFFFF : 0xFF000000;
						pDstPixel[7] = (SrcPixel & 0x01) ? 0xFFFFFFFF : 0xFF000000;
						pDstPixel += 8;
						pSrcPixel++;
					}

					if (nWidth % 8)
					{
						SrcPixel = *pSrcPixel;

						for (x = 0; x < nWidth % 8; x++)
						{
							*pDstPixel = (SrcPixel & 0x80) ? 0xFFFFFFFF : 0xFF000000;
							SrcPixel <<= 1;
							pDstPixel++;
						}

						pSrcPixel++;
					}

					pSrcPixel += nSrcPad;
					pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
				}
			}
		}

		return 1;
	}

	return 1;
}

int freerdp_image4_copy(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD SrcFormat, int nSrcStep, int nXSrc, int nYSrc, BYTE* palette)
{
	int x, y;
	int nSrcPad;
	int nDstPad;
	int nAlignedWidth;
	int dstBitsPerPixel;
	int dstBytesPerPixel;
	BOOL vFlip = FALSE;
	BOOL invert = FALSE;

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);

	nAlignedWidth = nWidth + (nWidth % 2);

	if (nSrcStep < 0)
		nSrcStep = nAlignedWidth / 2;

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nSrcPad = (nSrcStep - (nAlignedWidth / 2));
	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	if (FREERDP_PIXEL_FORMAT_IS_ABGR(DstFormat))
		invert = TRUE;

	if (FREERDP_PIXEL_FORMAT_FLIP(DstFormat) == FREERDP_PIXEL_FLIP_VERTICAL)
		vFlip = TRUE;

	if (dstBytesPerPixel == 4)
	{
		BYTE* pSrcPixel;
		UINT32* pDstPixel;
		UINT32* values = (UINT32*) palette;

		if (!invert)
		{
			if (!vFlip)
			{
				pSrcPixel = &pSrcData[nYSrc * nSrcStep];
				pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

				for (y = 0; y < nHeight; y++)
				{
					for (x = 0; x < nWidth / 2; x++)
					{
						pDstPixel[0] = values[*pSrcPixel >> 4];
						pDstPixel[1] = values[*pSrcPixel & 0xF];
						pDstPixel += 2;
						pSrcPixel++;
					}

					if (nWidth % 2)
					{
						pDstPixel[0] = values[*pSrcPixel >> 4];
						pDstPixel++;
						pSrcPixel++;
					}

					pSrcPixel += nSrcPad;
					pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
				}
			}
		}

		return 1;
	}

	return 1;
}

int freerdp_image8_copy(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD SrcFormat, int nSrcStep, int nXSrc, int nYSrc, BYTE* palette)
{
	BYTE* pe;
	int x, y;
	int srcFlip;
	int dstFlip;
	int nSrcPad;
	int nDstPad;
	int srcBitsPerPixel;
	int srcBytesPerPixel;
	int dstBitsPerPixel;
	int dstBytesPerPixel;
	BOOL vFlip = FALSE;
	BOOL invert = FALSE;

	if (!palette)
		return -1;

	srcBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(SrcFormat);
	srcBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(SrcFormat) / 8);
	srcFlip = FREERDP_PIXEL_FORMAT_FLIP(SrcFormat);

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat);

	if (nSrcStep < 0)
		nSrcStep = srcBytesPerPixel * nWidth;

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nSrcPad = (nSrcStep - (nWidth * srcBytesPerPixel));
	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	if (srcFlip != dstFlip)
		vFlip = TRUE;

	invert = FREERDP_PIXEL_FORMAT_IS_ABGR(DstFormat) ? TRUE : FALSE;

	if (dstBytesPerPixel == 4)
	{
		if ((dstBitsPerPixel == 32) || (dstBitsPerPixel == 24))
		{
			BYTE* pSrcPixel;
			UINT32* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + nXSrc];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = RGB32(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + nXSrc];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = RGB32(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + nXSrc];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = BGR32(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + nXSrc];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = BGR32(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
	}
	else if (dstBytesPerPixel == 3)
	{

	}
	else if (dstBytesPerPixel == 2)
	{
		if (dstBitsPerPixel == 16)
		{
			BYTE* pSrcPixel;
			UINT16* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + nXSrc];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = RGB16(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + nXSrc];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = RGB16(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + nXSrc];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = BGR16(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + nXSrc];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = BGR16(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
		else if (dstBitsPerPixel == 15)
		{
			BYTE* pSrcPixel;
			UINT16* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + nXSrc];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = RGB15(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + nXSrc];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = RGB15(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + nXSrc];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = BGR15(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + nXSrc];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pe = &palette[*pSrcPixel * 4];
							*pDstPixel++ = BGR15(pe[2], pe[1], pe[0]);
							pSrcPixel++;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
	}
	else if (dstBytesPerPixel == 1)
	{
		BYTE* pSrcPixel;
		BYTE* pDstPixel;

		if (!vFlip)
		{
			pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + nXSrc];
			pDstPixel = &pDstData[(nYDst * nDstStep) + nXDst];

			for (y = 0; y < nHeight; y++)
			{
				CopyMemory(pDstPixel, pSrcPixel, nWidth);
				pSrcPixel = &pSrcPixel[nSrcStep];
				pDstPixel = &pDstPixel[nDstStep];
			}
		}
		else
		{
			pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + nXSrc];
			pDstPixel = &pDstData[(nYDst * nDstStep) + nXDst];

			for (y = 0; y < nHeight; y++)
			{
				CopyMemory(pDstPixel, pSrcPixel, nWidth);
				pSrcPixel = &pSrcPixel[-nSrcStep];
				pDstPixel = &pDstPixel[nDstStep];
			}
		}

		return 1;
	}

	return -1;
}

int freerdp_image15_copy(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD SrcFormat, int nSrcStep, int nXSrc, int nYSrc, BYTE* palette)
{
	int x, y;
	int srcFlip;
	int dstFlip;
	int nSrcPad;
	int nDstPad;
	BYTE r, g, b;
	int srcBitsPerPixel;
	int srcBytesPerPixel;
	int dstBitsPerPixel;
	int dstBytesPerPixel;
	int srcType, dstType;
	BOOL vFlip = FALSE;
	BOOL invert = FALSE;

	srcBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(SrcFormat);
	srcBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(SrcFormat) / 8);
	srcFlip = FREERDP_PIXEL_FORMAT_FLIP(SrcFormat);
	srcType = FREERDP_PIXEL_FORMAT_TYPE(SrcFormat);

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat);
	dstType = FREERDP_PIXEL_FORMAT_TYPE(DstFormat);

	if (nSrcStep < 0)
		nSrcStep = srcBytesPerPixel * nWidth;

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nSrcPad = (nSrcStep - (nWidth * srcBytesPerPixel));
	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	if (srcFlip != dstFlip)
		vFlip = TRUE;

	if (srcType != dstType)
		invert = TRUE;

	if (dstBytesPerPixel == 4)
	{
		if ((dstBitsPerPixel == 32) || (dstBitsPerPixel == 24))
		{
			UINT16* pSrcPixel;
			UINT32* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = ARGB32(0xFF, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = ARGB32(0xFF, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = ABGR32(0xFF, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = ABGR32(0xFF, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
	}
	else if (dstBytesPerPixel == 2)
	{
		if (dstBitsPerPixel == 16)
		{
			UINT16* pSrcPixel;
			UINT16* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = RGB16(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = RGB16(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = BGR16(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = BGR16(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
		else if (dstBitsPerPixel == 15)
		{
			UINT16* pSrcPixel;
			UINT16* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = RGB15(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = RGB15(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = BGR15(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB15(r, g, b, *pSrcPixel);
							*pDstPixel = BGR15(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
	}

	return -1;
}

int freerdp_image16_copy(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD SrcFormat, int nSrcStep, int nXSrc, int nYSrc, BYTE* palette)
{
	int x, y;
	int srcFlip;
	int dstFlip;
	int nSrcPad;
	int nDstPad;
	BYTE r, g, b;
	int srcBitsPerPixel;
	int srcBytesPerPixel;
	int dstBitsPerPixel;
	int dstBytesPerPixel;
	int srcType, dstType;
	BOOL vFlip = FALSE;
	BOOL invert = FALSE;

	srcBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(SrcFormat);
	srcBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(SrcFormat) / 8);
	srcFlip = FREERDP_PIXEL_FORMAT_FLIP(SrcFormat);
	srcType = FREERDP_PIXEL_FORMAT_TYPE(SrcFormat);

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat);
	dstType = FREERDP_PIXEL_FORMAT_TYPE(DstFormat);

	if (nSrcStep < 0)
		nSrcStep = srcBytesPerPixel * nWidth;

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nSrcPad = (nSrcStep - (nWidth * srcBytesPerPixel));
	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	if (srcFlip != dstFlip)
		vFlip = TRUE;

	if (srcType != dstType)
		invert = TRUE;

	if (dstBytesPerPixel == 4)
	{
		if ((dstBitsPerPixel == 32) || (dstBitsPerPixel == 24))
		{
			UINT16* pSrcPixel;
			UINT32* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = ARGB32(0xFF, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = ARGB32(0xFF, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = ABGR32(0xFF, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = ABGR32(0xFF, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
	}
	else if (dstBytesPerPixel == 2)
	{
		if (dstBitsPerPixel == 16)
		{
			UINT16* pSrcPixel;
			UINT16* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						CopyMemory(pDstPixel, pSrcPixel, nWidth * 2);
						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcStep];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstStep];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						CopyMemory(pDstPixel, pSrcPixel, nWidth * 2);
						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-nSrcStep];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstStep];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = BGR16(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = BGR16(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
		else if (dstBitsPerPixel == 15)
		{
			UINT16* pSrcPixel;
			UINT16* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = RGB15(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = RGB15(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = (UINT16*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = BGR15(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = (UINT16*) &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 2)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetRGB16(r, g, b, *pSrcPixel);
							*pDstPixel = BGR15(r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT16*) &((BYTE*) pSrcPixel)[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
	}

	return -1;
}

int freerdp_image24_copy(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD SrcFormat, int nSrcStep, int nXSrc, int nYSrc, BYTE* palette)
{
	int x, y;
	int srcFlip;
	int dstFlip;
	int nSrcPad;
	int nDstPad;
	int srcBitsPerPixel;
	int srcBytesPerPixel;
	int dstBitsPerPixel;
	int dstBytesPerPixel;
	int srcType, dstType;
	BOOL vFlip = FALSE;
	BOOL invert = FALSE;

	srcBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(SrcFormat);
	srcBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(SrcFormat) / 8);
	srcFlip = FREERDP_PIXEL_FORMAT_FLIP(SrcFormat);
	srcType = FREERDP_PIXEL_FORMAT_TYPE(SrcFormat);

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat);
	dstType = FREERDP_PIXEL_FORMAT_TYPE(DstFormat);

	if (nSrcStep < 0)
		nSrcStep = srcBytesPerPixel * nWidth;

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nSrcPad = (nSrcStep - (nWidth * srcBytesPerPixel));
	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	if (srcFlip != dstFlip)
		vFlip = TRUE;

	if (srcType != dstType)
		invert = TRUE;

	if (dstBytesPerPixel == 4)
	{
		if ((dstBitsPerPixel == 32) || (dstBitsPerPixel == 24))
		{
			BYTE* pSrcPixel;
			BYTE* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 3)];
					pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = *pSrcPixel++;
							*pDstPixel++ = *pSrcPixel++;
							*pDstPixel++ = *pSrcPixel++;
							*pDstPixel++ = 0xFF;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = &pDstPixel[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 3)];
					pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = *pSrcPixel++;
							*pDstPixel++ = *pSrcPixel++;
							*pDstPixel++ = *pSrcPixel++;
							*pDstPixel++ = 0xFF;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = &pDstPixel[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 3)];
					pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pDstPixel[0] = pSrcPixel[2];
							pDstPixel[1] = pSrcPixel[1];
							pDstPixel[2] = pSrcPixel[0];
							pDstPixel[3] = 0xFF;

							pSrcPixel += 3;
							pDstPixel += 4;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = &pDstPixel[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 3)];
					pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							pDstPixel[0] = pSrcPixel[2];
							pDstPixel[1] = pSrcPixel[1];
							pDstPixel[2] = pSrcPixel[0];
							pDstPixel[3] = 0xFF;

							pSrcPixel += 3;
							pDstPixel += 4;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = &pDstPixel[nDstPad];
					}
				}
			}

			return 1;
		}
	}
	else if (dstBytesPerPixel == 3)
	{
		BYTE* pSrcPixel;
		BYTE* pDstPixel;

		if (!invert)
		{
			if (!vFlip)
			{
				pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 3)];
				pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 3)];

				for (y = 0; y < nHeight; y++)
				{
					CopyMemory(pDstPixel, pSrcPixel, nWidth * 3);
					pSrcPixel = &pSrcPixel[nSrcStep];
					pDstPixel = &pDstPixel[nDstStep];
				}
			}
			else
			{
				pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 3)];
				pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 3)];

				for (y = 0; y < nHeight; y++)
				{
					CopyMemory(pDstPixel, pSrcPixel, nWidth * 3);
					pSrcPixel = &pSrcPixel[-nSrcStep];
					pDstPixel = &pDstPixel[nDstStep];
				}
			}
		}
		else
		{
			if (!vFlip)
			{
				pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 3)];
				pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 3)];

				for (y = 0; y < nHeight; y++)
				{
					for (x = 0; x < nWidth; x++)
					{
						pDstPixel[0] = pSrcPixel[2];
						pDstPixel[1] = pSrcPixel[1];
						pDstPixel[2] = pSrcPixel[0];

						pSrcPixel += 3;
						pDstPixel += 3;
					}

					pSrcPixel = &pSrcPixel[nSrcPad];
					pDstPixel = &pDstPixel[nDstPad];
				}
			}
			else
			{
				pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 3)];
				pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 3)];

				for (y = 0; y < nHeight; y++)
				{
					for (x = 0; x < nWidth; x++)
					{
						pDstPixel[0] = pSrcPixel[2];
						pDstPixel[1] = pSrcPixel[1];
						pDstPixel[2] = pSrcPixel[0];

						pSrcPixel += 3;
						pDstPixel += 3;
					}

					pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
					pDstPixel = &pDstPixel[nDstPad];
				}
			}
		}

		return 1;
	}
	else if (dstBytesPerPixel == 2)
	{
		if (dstBitsPerPixel == 16)
		{
			BYTE* pSrcPixel;
			UINT16* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 3)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = RGB16(pSrcPixel[2], pSrcPixel[1], pSrcPixel[0]);
							pSrcPixel += 3;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = &((UINT16*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 3)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = RGB16(pSrcPixel[2], pSrcPixel[1], pSrcPixel[0]);
							pSrcPixel += 3;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = &((UINT16*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 3)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = RGB16(pSrcPixel[0], pSrcPixel[1], pSrcPixel[2]);
							pSrcPixel += 3;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = &((UINT16*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 3)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = RGB16(pSrcPixel[0], pSrcPixel[1], pSrcPixel[2]);
							pSrcPixel += 3;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = &((UINT16*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
		else if (dstBitsPerPixel == 15)
		{
			BYTE* pSrcPixel;
			UINT16* pDstPixel;

			if (!invert)
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 3)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = RGB15(pSrcPixel[2], pSrcPixel[1], pSrcPixel[0]);
							pSrcPixel += 3;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = &((UINT16*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 3)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = RGB15(pSrcPixel[2], pSrcPixel[1], pSrcPixel[0]);
							pSrcPixel += 3;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = &((UINT16*) pDstPixel)[nDstPad];
					}
				}
			}
			else
			{
				if (!vFlip)
				{
					pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 3)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = RGB15(pSrcPixel[0], pSrcPixel[1], pSrcPixel[2]);
							pSrcPixel += 3;
						}

						pSrcPixel = &pSrcPixel[nSrcPad];
						pDstPixel = &((UINT16*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 3)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = RGB15(pSrcPixel[0], pSrcPixel[1], pSrcPixel[2]);
							pSrcPixel += 3;
						}

						pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
						pDstPixel = &((UINT16*) pDstPixel)[nDstPad];
					}
				}
			}

			return 1;
		}
	}

	return -1;
}

int freerdp_image32_copy(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD SrcFormat, int nSrcStep, int nXSrc, int nYSrc, BYTE* palette)
{
	int x, y;
	int srcFlip;
	int dstFlip;
	int nSrcPad;
	int nDstPad;
	BYTE a, r, g, b;
	int srcBitsPerPixel;
	int srcBytesPerPixel;
	int dstBitsPerPixel;
	int dstBytesPerPixel;
	int srcType, dstType;
	BOOL vFlip = FALSE;
	BOOL invert = FALSE;

	srcBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(SrcFormat);
	srcBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(SrcFormat) / 8);
	srcFlip = FREERDP_PIXEL_FORMAT_FLIP(SrcFormat);
	srcType = FREERDP_PIXEL_FORMAT_TYPE(SrcFormat);

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat);
	dstType = FREERDP_PIXEL_FORMAT_TYPE(DstFormat);

	if (nSrcStep < 0)
		nSrcStep = srcBytesPerPixel * nWidth;

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nSrcPad = (nSrcStep - (nWidth * srcBytesPerPixel));
	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	if (srcFlip != dstFlip)
		vFlip = TRUE;

	if (srcType != dstType)
		invert = TRUE;

	if (srcBitsPerPixel == 24)
	{
		if (dstBytesPerPixel == 4) /* srcBytesPerPixel == dstBytesPerPixel */
		{
			if (dstBitsPerPixel == 32)
			{
				UINT32* pSrcPixel;
				UINT32* pDstPixel;

				pSrcPixel = (UINT32*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 4)];
				pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

				if (!invert)
				{
					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							*pDstPixel++ = *pSrcPixel++;
						}

						pSrcPixel = (UINT32*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}
				else
				{
					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetARGB32(a, r, g, b, *pSrcPixel);
							*pDstPixel = ABGR32(a, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT32*) &((BYTE*) pSrcPixel)[nSrcPad];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[nDstPad];
					}
				}

				return 1;
			}
			else if (dstBitsPerPixel == 24) /* srcBitsPerPixel == dstBitsPerPixel */
			{
				BYTE* pSrcPixel;
				BYTE* pDstPixel;

				if (!invert)
				{
					if (!vFlip)
					{
						pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 4)];
						pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

						for (y = 0; y < nHeight; y++)
						{
							MoveMemory(pDstPixel, pSrcPixel, nWidth * 4);
							pSrcPixel = &pSrcPixel[nSrcStep];
							pDstPixel = &pDstPixel[nDstStep];
						}
					}
					else
					{
						pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 4)];
						pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

						for (y = 0; y < nHeight; y++)
						{
							MoveMemory(pDstPixel, pSrcPixel, nWidth * 4);
							pSrcPixel = &pSrcPixel[-nSrcStep];
							pDstPixel = &pDstPixel[nDstStep];
						}
					}
				}
				else
				{
					if (!vFlip)
					{
						pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 4)];
						pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

						for (y = 0; y < nHeight; y++)
						{
							for (x = 0; x < nWidth; x++)
							{
								pDstPixel[0] = pSrcPixel[2];
								pDstPixel[1] = pSrcPixel[1];
								pDstPixel[2] = pSrcPixel[0];
								pDstPixel[3] = 0xFF;

								pSrcPixel += 4;
								pDstPixel += 4;
							}

							pSrcPixel = &pSrcPixel[nSrcPad];
							pDstPixel = &pDstPixel[nDstPad];
						}
					}
					else
					{
						pSrcPixel = &pSrcData[((nYSrc + nHeight - 1) * nSrcStep) + (nXSrc * 4)];
						pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

						for (y = 0; y < nHeight; y++)
						{
							for (x = 0; x < nWidth; x++)
							{
								pDstPixel[0] = pSrcPixel[2];
								pDstPixel[1] = pSrcPixel[1];
								pDstPixel[2] = pSrcPixel[0];
								pDstPixel[3] = 0xFF;

								pSrcPixel += 4;
								pDstPixel += 4;
							}

							pSrcPixel = &pSrcPixel[-((nSrcStep - nSrcPad) + nSrcStep)];
							pDstPixel = &pDstPixel[nDstPad];
						}
					}
				}

				return 1;
			}
		}
		else if (dstBytesPerPixel == 3)
		{
			UINT32* pSrcPixel;
			BYTE* pDstPixel;

			pSrcPixel = (UINT32*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 4)];
			pDstPixel = (BYTE*) &pDstData[(nYDst * nDstStep) + (nXDst * 3)];

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					GetRGB32(r, g, b, *pSrcPixel);

					*pDstPixel++ = r;
					*pDstPixel++ = g;
					*pDstPixel++ = b;

					pSrcPixel++;
				}

				pSrcPixel = (UINT32*) &((BYTE*) pSrcPixel)[nSrcPad];
				pDstPixel = (BYTE*) &((BYTE*) pDstPixel)[nDstPad];
			}

			return 1;
		}
		else if (dstBytesPerPixel == 2)
		{
			if (dstBitsPerPixel == 16)
			{
				UINT32* pSrcPixel;
				UINT16* pDstPixel;

				pSrcPixel = (UINT32*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 4)];
				pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

				for (y = 0; y < nHeight; y++)
				{
					for (x = 0; x < nWidth; x++)
					{
						GetRGB32(r, g, b, *pSrcPixel);
						RGB_888_565(r, g, b);
						*pDstPixel = RGB565(r, g, b);

						pSrcPixel++;
						pDstPixel++;
					}

					pSrcPixel = (UINT32*) &((BYTE*) pSrcPixel)[nSrcPad];
					pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
				}

				return 1;
			}
			else if (dstBitsPerPixel == 15)
			{
				UINT32* pSrcPixel;
				UINT16* pDstPixel;

				pSrcPixel = (UINT32*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 4)];
				pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * 2)];

				for (y = 0; y < nHeight; y++)
				{
					for (x = 0; x < nWidth; x++)
					{
						GetRGB32(r, g, b, *pSrcPixel);
						RGB_888_555(r, g, b);
						*pDstPixel = RGB555(r, g, b);

						pSrcPixel++;
						pDstPixel++;
					}

					pSrcPixel = (UINT32*) &((BYTE*) pSrcPixel)[nSrcPad];
					pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[nDstPad];
				}

				return 1;
			}
		}
	}

	return -1;
}

int freerdp_image_copy(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD SrcFormat, int nSrcStep, int nXSrc, int nYSrc, BYTE* palette)
{
	int status = -1;
	int srcBitsPerPixel;
	int srcBytesPerPixel;

	srcBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(SrcFormat);
	srcBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(SrcFormat) / 8);

	if (srcBytesPerPixel == 4)
	{
		status = freerdp_image32_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst,
				nWidth, nHeight, pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, palette);
	}
	else if (srcBytesPerPixel == 3)
	{
		status = freerdp_image24_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst,
				nWidth, nHeight, pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, palette);
	}
	else if (srcBytesPerPixel == 2)
	{
		if (srcBitsPerPixel == 16)
		{
			status = freerdp_image16_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst,
					nWidth, nHeight, pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, palette);
		}
		else if (srcBitsPerPixel == 15)
		{
			status = freerdp_image15_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst,
					nWidth, nHeight, pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, palette);
		}
	}
	else if (srcBytesPerPixel == 1)
	{
		status = freerdp_image8_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst,
				nWidth, nHeight, pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, palette);
	}
	else if (srcBitsPerPixel == 1)
	{
		status = freerdp_image1_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst,
				nWidth, nHeight, pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, palette);
	}
	else if (srcBitsPerPixel == 4)
	{
		status = freerdp_image4_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst,
				nWidth, nHeight, pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, palette);
	}

	if (status < 0)
	{
		int dstBitsPerPixel;
		int dstBytesPerPixel;

		dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
		dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);

		WLog_ERR(TAG, "failure: src: %d/%d dst: %d/%d",
				srcBitsPerPixel, srcBytesPerPixel, dstBitsPerPixel, dstBytesPerPixel);
	}

	return status;
}

int freerdp_image_move(BYTE* pData, DWORD Format, int nStep, int nXDst, int nYDst, int nWidth, int nHeight, int nXSrc, int nYSrc)
{
	int y;
	BOOL overlap;
	BYTE* pSrcPixel;
	BYTE* pDstPixel;
	int bytesPerPixel;

	bytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(Format) / 8);

	if (nStep < 0)
		nStep = nWidth * bytesPerPixel;

	overlap = (((nXDst + nWidth) > nXSrc) && (nXDst < (nXSrc + nWidth)) &&
		((nYDst + nHeight) > nYSrc) && (nYDst < (nYSrc + nHeight))) ? TRUE : FALSE;

	if (!overlap)
	{
		pSrcPixel = &pData[(nYSrc * nStep) + (nXSrc * bytesPerPixel)];
		pDstPixel = &pData[(nYDst * nStep) + (nXDst * bytesPerPixel)];

		for (y = 0; y < nHeight; y++)
		{
			CopyMemory(pDstPixel, pSrcPixel, nWidth * bytesPerPixel);
			pSrcPixel += nStep;
			pDstPixel += nStep;
		}

		return 1;
	}

	if (nYSrc < nYDst)
	{
		/* copy down */

		pSrcPixel = &pData[((nYSrc + nHeight - 1) * nStep) + (nXSrc * bytesPerPixel)];
		pDstPixel = &pData[((nYDst + nHeight - 1) * nStep) + (nXDst * bytesPerPixel)];

		for (y = 0; y < nHeight; y++)
		{
			CopyMemory(pDstPixel, pSrcPixel, nWidth * bytesPerPixel);
			pSrcPixel -= nStep;
			pDstPixel -= nStep;
		}
	}
	else if (nYSrc > nYDst)
	{
		/* copy up */

		pSrcPixel = &pData[(nYSrc * nStep) + (nXSrc * bytesPerPixel)];
		pDstPixel = &pData[(nYDst * nStep) + (nXDst * bytesPerPixel)];

		for (y = 0; y < nHeight; y++)
		{
			CopyMemory(pDstPixel, pSrcPixel, nWidth * bytesPerPixel);
			pSrcPixel += nStep;
			pDstPixel += nStep;
		}
	}
	else if (nXSrc > nXDst)
	{
		/* copy left */

		pSrcPixel = &pData[(nYSrc * nStep) + (nXSrc * bytesPerPixel)];
		pDstPixel = &pData[(nYDst * nStep) + (nXDst * bytesPerPixel)];

		for (y = 0; y < nHeight; y++)
		{
			MoveMemory(pDstPixel, pSrcPixel, nWidth * bytesPerPixel);
			pSrcPixel += nStep;
			pDstPixel += nStep;
		}
	}
	else
	{
		/* copy right */

		pSrcPixel = &pData[(nYSrc * nStep) + (nXSrc * bytesPerPixel)];
		pDstPixel = &pData[(nYDst * nStep) + (nXDst * bytesPerPixel)];

		for (y = 0; y < nHeight; y++)
		{
			MoveMemory(pDstPixel, pSrcPixel, nWidth * bytesPerPixel);
			pSrcPixel += nStep;
			pDstPixel += nStep;
		}
	}

	return 1;
}

void* freerdp_image_memset32(UINT32* ptr, UINT32 fill, size_t length)
{
	while (length--)
	{
		*ptr++ = fill;
	}

	return (void*) ptr;
}

int freerdp_image_fill(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, UINT32 color)
{
	int y;
	int dstBitsPerPixel;
	int dstBytesPerPixel;

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);

	if (dstBytesPerPixel == 4)
	{
		UINT32* pDstPixel;

		if (nDstStep < 0)
			nDstStep = dstBytesPerPixel * nWidth;

		for (y = 0; y < nHeight; y++)
		{
			pDstPixel = (UINT32*) &pDstData[((nYDst + y) * nDstStep) + (nXDst * dstBytesPerPixel)];
			freerdp_image_memset32(pDstPixel, color, nWidth);
		}
	}
	else if (dstBytesPerPixel == 3)
	{

	}
	else if (dstBytesPerPixel == 2)
	{
		if (dstBitsPerPixel == 16)
		{

		}
		else if (dstBitsPerPixel == 15)
		{

		}
	}

	return 0;
}

int freerdp_image_copy_from_retina(BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst,
				   int nWidth, int nHeight, BYTE* pSrcData, int nSrcStep, int nXSrc, int nYSrc)
{
	int x, y;
	int nSrcPad;
	int nDstPad;
	int srcBitsPerPixel;
	int srcBytesPerPixel;
	int dstBitsPerPixel;
	int dstBytesPerPixel;

	srcBitsPerPixel = 24;
	srcBytesPerPixel = 8;

	if (nSrcStep < 0)
		nSrcStep = srcBytesPerPixel * nWidth;

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nSrcPad = (nSrcStep - (nWidth * srcBytesPerPixel));
	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));

	if (dstBytesPerPixel == 4)
	{
		UINT32 R, G, B;
		BYTE* pSrcPixel;
		BYTE* pDstPixel;

		pSrcPixel = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 4)];
		pDstPixel = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x < nWidth; x++)
			{
				/* simple box filter scaling, could be improved with better algorithm */

				B = pSrcPixel[0] + pSrcPixel[4] + pSrcPixel[nSrcStep + 0] + pSrcPixel[nSrcStep + 4];
				G = pSrcPixel[1] + pSrcPixel[5] + pSrcPixel[nSrcStep + 1] + pSrcPixel[nSrcStep + 5];
				R = pSrcPixel[2] + pSrcPixel[6] + pSrcPixel[nSrcStep + 2] + pSrcPixel[nSrcStep + 6];
				pSrcPixel += 8;

				*pDstPixel++ = (BYTE) (B >> 2);
				*pDstPixel++ = (BYTE) (G >> 2);
				*pDstPixel++ = (BYTE) (R >> 2);
				*pDstPixel++ = 0xFF;
			}

			pSrcPixel = &pSrcPixel[nSrcPad + nSrcStep];
			pDstPixel = &pDstPixel[nDstPad];
		}
	}

	return 1;
}

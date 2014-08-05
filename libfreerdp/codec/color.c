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

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/primitives.h>
#include <freerdp/codec/color.h>

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
		BYTE* dstp;

		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 4, 16);

		if (!dstData)
			return NULL;

		dstp = dstData;

		for (i = width * height; i > 0; i--)
		{
			*(dstp++) = *(srcData++);
			*(dstp++) = *(srcData++);
			*(dstp++) = *(srcData++);
			*(dstp++) = 0xFF;
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
		if (!dstData)
			dstData = (BYTE*) _aligned_malloc(width * height * 4, 16);

		if (!dstData)
			return NULL;

		if (clrconv->alpha)
		{
			int x, y;
			BYTE* dstp;

			CopyMemory(dstData, srcData, width * height * 4);

			dstp = dstData;
			for (y = 0; y < height; y++)
			{
				for (x = 0; x < width * 4; x += 4)
				{
					dstp += 3;
					*dstp = 0xFF;
					dstp++;
				}
			}
		}
		else
		{
			CopyMemory(dstData, srcData, width * height * 4);
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

	switch (srcBpp)
	{
		case 8:
			bgcolor &= 0xFF;
			redBg = clrconv->palette->entries[bgcolor].red;
			greenBg = clrconv->palette->entries[bgcolor].green;
			blueBg = clrconv->palette->entries[bgcolor].blue;

			fgcolor &= 0xFF;
			redFg = clrconv->palette->entries[fgcolor].red;
			greenFg = clrconv->palette->entries[fgcolor].green;
			blueFg = clrconv->palette->entries[fgcolor].blue;
			break;

		case 16:
			GetRGB16(redBg, greenBg, blueBg, bgcolor);
			GetRGB16(redFg, greenFg, blueFg, fgcolor);
			break;

		case 15:
			GetRGB15(redBg, greenBg, blueBg, bgcolor);
			GetRGB15(redFg, greenFg, blueFg, fgcolor);
			break;

		default:
			GetRGB32(redBg, greenBg, blueBg, bgcolor);
			GetRGB32(redFg, greenFg, blueFg, fgcolor);
			break;
	}

	if (dstBpp == 16)
	{
		if (clrconv->rgb555)
		{
			if (srcBpp == 16)
			{
				/* convert 15-bit colors to 16-bit colors */
				RGB16_RGB15(redBg, greenBg, blueBg, bgcolor);
				RGB16_RGB15(redFg, greenFg, blueFg, fgcolor);
			}
		}
		else
		{
			if (srcBpp == 15)
			{
				/* convert 15-bit colors to 16-bit colors */
				RGB15_RGB16(redBg, greenBg, blueBg, bgcolor);
				RGB15_RGB16(redFg, greenFg, blueFg, fgcolor);
			}
		}

		dstData = (BYTE*) _aligned_malloc(width * height * 2, 16);

		if (!dstData)
			return NULL;

		dst16 = (UINT16*) dstData;

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
					*dst32 = (clrconv->invert) ? BGR32(redBg, greenBg, blueBg) : RGB32(redBg, greenBg, blueBg);
				}
				else
				{
					*dst32 = (clrconv->invert) ? BGR32(redFg, greenFg, blueFg) : RGB32(redFg, greenFg, blueFg);
				}
				dst32++;
			}
			srcData++;
		}
		return dstData;
	}

	return srcData;
}

void freerdp_alpha_cursor_convert(BYTE* alphaData, BYTE* xorMask, BYTE* andMask, int width, int height, int bpp, HCLRCONV clrconv)
{
	int xpixel;
	int apixel;
	int i, j, jj;

	for (j = 0; j < height; j++)
	{
		jj = (bpp == 1) ? j : (height - 1) - j;
		for (i = 0; i < width; i++)
		{
			xpixel = freerdp_get_pixel(xorMask, i, jj, width, height, bpp);
			xpixel = freerdp_color_convert_rgb(xpixel, bpp, 32, clrconv);
			apixel = freerdp_get_pixel(andMask, i, jj, width, height, 1);

			if (apixel != 0)
			{
				if ((xpixel & 0xffffff) == 0xffffff)
				{
					/* use pattern (not solid black) for xor area */
					xpixel = (i & 1) == (j & 1);
					xpixel = xpixel ? 0xFFFFFF : 0;
					xpixel |= 0xFF000000;
				}
				else if (xpixel == 0xFF000000)
				{
					xpixel = 0;
				}
			}

			freerdp_set_pixel(alphaData, i, j, width, height, 32, xpixel);
		}
	}
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
		return NULL;

	return clrconv;
}

void freerdp_clrconv_free(HCLRCONV clrconv)
{
	if (clrconv)
	{
		if (clrconv->palette)
			free(clrconv->palette);

		free(clrconv);
	}
}

int freerdp_image_copy(BYTE* pDstData, DWORD dwDstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD dwSrcFormat, int nSrcStep, int nXSrc, int nYSrc)
{
	int x, y;
	int srcFlip;
	int dstFlip;
	BYTE a, r, g, b;
	int beg, end, inc;
	int srcBitsPerPixel;
	int srcBytesPerPixel;
	int dstBitsPerPixel;
	int dstBytesPerPixel;
	BOOL overlap = FALSE;
	BOOL vFlip = FALSE;

	srcBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(dwSrcFormat);
	srcBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(dwSrcFormat) / 8);
	srcFlip = FREERDP_PIXEL_FORMAT_FLIP(dwSrcFormat);

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(dwDstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(dwDstFormat) / 8);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(dwDstFormat);

	if (srcFlip != dstFlip)
		vFlip = TRUE;

	if (pDstData == pSrcData)
	{
		overlap = (((nXDst + nWidth) > nXSrc) && (nXDst < (nXSrc + nWidth)) &&
			((nYDst + nHeight) > nYSrc) && (nYDst < (nYSrc + nHeight))) ? TRUE : FALSE;
	}

	if (srcBytesPerPixel == 4)
	{
		if (nSrcStep < 0)
			nSrcStep = srcBytesPerPixel * nWidth;

		if (srcBitsPerPixel == 24)
		{
			if (dstBytesPerPixel == 4) /* srcBytesPerPixel == dstBytesPerPixel */
			{
				if (dstBitsPerPixel == 32)
				{
					UINT32* pSrcPixel;
					UINT32* pDstPixel;

					if (nDstStep < 0)
						nDstStep = dstBytesPerPixel * nWidth;

					pSrcPixel = (UINT32*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * srcBytesPerPixel)];
					pDstPixel = (UINT32*) &pDstData[(nYDst * nDstStep) + (nXDst * dstBytesPerPixel)];

					for (y = 0; y < nHeight; y++)
					{
						for (x = 0; x < nWidth; x++)
						{
							GetARGB32(a, r, g, b, *pSrcPixel);
							*pDstPixel = ARGB32(a, r, g, b);

							pSrcPixel++;
							pDstPixel++;
						}

						pSrcPixel = (UINT32*) &((BYTE*) pSrcPixel)[(nSrcStep - (nWidth * srcBytesPerPixel))];
						pDstPixel = (UINT32*) &((BYTE*) pDstPixel)[(nDstStep - (nWidth * dstBytesPerPixel))];
					}
				}
				else if (dstBitsPerPixel == 24) /* srcBitsPerPixel == dstBitsPerPixel */
				{
					UINT32* pSrcPixel;
					UINT32* pDstPixel;

					if (nDstStep < 0)
						nDstStep = dstBytesPerPixel * nWidth;

					if (overlap && (nYSrc < nYDst))
					{
						beg = nHeight - 1;
						inc = -1; /* downward copy */
						end = -1;
					}
					else
					{
						beg = 0;
						inc = 1; /* upward copy */
						end = nHeight;
					}

					if (!vFlip)
					{
						for (y = beg; y != end; y += inc)
						{
							pSrcPixel = (UINT32*) &pSrcData[((nYSrc + y) * nSrcStep) + (nXSrc * srcBytesPerPixel)];
							pDstPixel = (UINT32*) &pDstData[((nYDst + y) * nDstStep) + (nXDst * dstBytesPerPixel)];
							MoveMemory(pDstPixel, pSrcPixel, nWidth * 4);
						}
					}
					else
					{
						for (y = beg; y != end; y += inc)
						{
							pSrcPixel = (UINT32*) &pSrcData[((nYSrc + y) * nSrcStep) + (nXSrc * srcBytesPerPixel)];
							pDstPixel = (UINT32*) &pDstData[((nYDst + (nHeight - y - 1)) * nDstStep) + (nXDst * dstBytesPerPixel)];
							MoveMemory(pDstPixel, pSrcPixel, nWidth * 4);
						}
					}
				}
			}
			else if (dstBytesPerPixel == 3)
			{
				UINT32* pSrcPixel;
				BYTE* pDstPixel;

				if (nDstStep < 0)
					nDstStep = dstBytesPerPixel * nWidth;

				pSrcPixel = (UINT32*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * srcBytesPerPixel)];
				pDstPixel = (BYTE*) &pDstData[(nYDst * nDstStep) + (nXDst * dstBytesPerPixel)];

				for (y = 0; y < nHeight; y++)
				{
					for (x = 0; x < nWidth; x++)
					{
						GetRGB32(r, g, b, *pSrcPixel);

						*(pDstPixel++) = r;
						*(pDstPixel++) = g;
						*(pDstPixel++) = b;

						pSrcPixel++;
					}

					pSrcPixel = (UINT32*) &((BYTE*) pSrcPixel)[(nSrcStep - (nWidth * srcBytesPerPixel))];
					pDstPixel = (BYTE*) &((BYTE*) pDstPixel)[(nDstStep - (nWidth * dstBytesPerPixel))];
				}
			}
			else if (dstBytesPerPixel == 2)
			{
				if (dstBitsPerPixel == 16)
				{
					UINT32* pSrcPixel;
					UINT16* pDstPixel;

					if (nDstStep < 0)
						nDstStep = dstBytesPerPixel * nWidth;

					pSrcPixel = (UINT32*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * srcBytesPerPixel)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * dstBytesPerPixel)];

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

						pSrcPixel = (UINT32*) &((BYTE*) pSrcPixel)[(nSrcStep - (nWidth * srcBytesPerPixel))];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[(nDstStep - (nWidth * dstBytesPerPixel))];
					}
				}
				else if (dstBitsPerPixel == 15)
				{
					UINT32* pSrcPixel;
					UINT16* pDstPixel;

					if (nDstStep < 0)
						nDstStep = dstBytesPerPixel * nWidth;

					pSrcPixel = (UINT32*) &pSrcData[(nYSrc * nSrcStep) + (nXSrc * srcBytesPerPixel)];
					pDstPixel = (UINT16*) &pDstData[(nYDst * nDstStep) + (nXDst * dstBytesPerPixel)];

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

						pSrcPixel = (UINT32*) &((BYTE*) pSrcPixel)[(nSrcStep - (nWidth * srcBytesPerPixel))];
						pDstPixel = (UINT16*) &((BYTE*) pDstPixel)[(nDstStep - (nWidth * dstBytesPerPixel))];
					}
				}
			}
		}
	}

	return 0;
}

void* freerdp_image_memset32(UINT32* ptr, UINT32 fill, size_t length)
{
	while (length--)
	{
		*ptr++ = fill;
	}

	return (void*) ptr;
}

int freerdp_image_fill(BYTE* pDstData, DWORD dwDstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, UINT32 color)
{
	int y;
	int dstBitsPerPixel;
	int dstBytesPerPixel;

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(dwDstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(dwDstFormat) / 8);

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


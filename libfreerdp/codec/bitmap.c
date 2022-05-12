/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Bitmap Compression
 *
 * Copyright 2004-2012 Jay Sorg <jay.sorg@gmail.com>
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

#include <freerdp/config.h>

#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/planar.h>

static INLINE UINT16 GETPIXEL16(const void* d, UINT32 x, UINT32 y, UINT32 w)
{
	const BYTE* src = (const BYTE*)d + ((y * w + x) * sizeof(UINT16));
	return (UINT16)(((UINT16)src[1] << 8) | (UINT16)src[0]);
}

static INLINE UINT32 GETPIXEL32(const void* d, UINT32 x, UINT32 y, UINT32 w)
{
	const BYTE* src = (const BYTE*)d + ((y * w + x) * sizeof(UINT32));
	return (((UINT32)src[3]) << 24) | (((UINT32)src[2]) << 16) | (((UINT32)src[1]) << 8) |
	       (src[0] & 0xFF);
}

/*****************************************************************************/
static INLINE UINT16 IN_PIXEL16(const void* in_ptr, UINT32 in_x, UINT32 in_y, UINT32 in_w,
                                UINT16 in_last_pixel)
{
	if (in_ptr == 0)
		return 0;
	else if (in_x < in_w)
		return GETPIXEL16(in_ptr, in_x, in_y, in_w);
	else
		return in_last_pixel;
}

/*****************************************************************************/
static INLINE UINT32 IN_PIXEL32(const void* in_ptr, UINT32 in_x, UINT32 in_y, UINT32 in_w,
                                UINT32 in_last_pixel)
{
	if (in_ptr == 0)
		return 0;
	else if (in_x < in_w)
		return GETPIXEL32(in_ptr, in_x, in_y, in_w);
	else
		return in_last_pixel;
}

/*****************************************************************************/
/* color */
static UINT16 out_color_count_2(UINT16 in_count, wStream* in_s, UINT16 in_data)
{
	if (in_count > 0)
	{
		if (in_count < 32)
		{
			const BYTE temp = ((0x3 << 5) | in_count) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count < 256 + 32)
		{
			const BYTE temp = (in_count - 32) & 0xFF;
			Stream_Write_UINT8(in_s, 0x60);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf3);
			Stream_Write_UINT16(in_s, in_count);
		}

		Stream_Write_UINT16(in_s, in_data);
	}

	return 0;
}
#define OUT_COLOR_COUNT2(in_count, in_s, in_data) \
	in_count = out_color_count_2(in_count, in_s, in_data)

/*****************************************************************************/
/* color */
static UINT16 out_color_count_3(UINT16 in_count, wStream* in_s, UINT32 in_data)
{
	if (in_count > 0)
	{
		if (in_count < 32)
		{
			const BYTE temp = ((0x3 << 5) | in_count) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count < 256 + 32)
		{
			const BYTE temp = (in_count - 32) & 0xFF;
			Stream_Write_UINT8(in_s, 0x60);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf3);
			Stream_Write_UINT16(in_s, in_count);
		}

		Stream_Write_UINT8(in_s, in_data & 0xFF);

		Stream_Write_UINT8(in_s, (in_data >> 8) & 0xFF);
		Stream_Write_UINT8(in_s, (in_data >> 16) & 0xFF);
	}

	return 0;
}

#define OUT_COLOR_COUNT3(in_count, in_s, in_data) \
	in_count = out_color_count_3(in_count, in_s, in_data)

/*****************************************************************************/
/* copy */
static INLINE UINT16 out_copy_count_2(UINT16 in_count, wStream* in_s, wStream* in_data)

{
	if (in_count > 0)
	{
		if (in_count < 32)
		{
			const BYTE temp = ((0x4 << 5) | in_count) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count < 256 + 32)
		{
			const BYTE temp = (in_count - 32) & 0xFF;
			Stream_Write_UINT8(in_s, 0x80);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf4);
			Stream_Write_UINT16(in_s, in_count);
		}

		Stream_Write(in_s, Stream_Buffer(in_data), in_count * 2);
	}

	Stream_SetPosition(in_data, 0);
	return 0;
}
#define OUT_COPY_COUNT2(in_count, in_s, in_data) \
	in_count = out_copy_count_2(in_count, in_s, in_data)
/*****************************************************************************/
/* copy */
static INLINE UINT16 out_copy_count_3(UINT16 in_count, wStream* in_s, wStream* in_data)
{
	if (in_count > 0)
	{
		if (in_count < 32)
		{
			const BYTE temp = ((0x4 << 5) | in_count) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count < 256 + 32)
		{
			const BYTE temp = (in_count - 32) & 0xFF;
			Stream_Write_UINT8(in_s, 0x80);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf4);
			Stream_Write_UINT16(in_s, in_count);
		}

		Stream_Write(in_s, Stream_Pointer(in_data), in_count * 3);
	}

	Stream_SetPosition(in_data, 0);
	return 0;
}
#define OUT_COPY_COUNT3(in_count, in_s, in_data) \
	in_count = out_copy_count_3(in_count, in_s, in_data)

/*****************************************************************************/
/* bicolor */
static INLINE UINT16 out_bicolor_count_2(UINT16 in_count, wStream* in_s, UINT16 in_color1,
                                         UINT16 in_color2)
{
	if (in_count > 0)
	{
		if (in_count / 2 < 16)
		{
			const BYTE temp = ((0xe << 4) | (in_count / 2)) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count / 2 < 256 + 16)
		{
			const BYTE temp = (in_count / 2 - 16) & 0xFF;
			Stream_Write_UINT8(in_s, 0xe0);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf8);
			Stream_Write_UINT16(in_s, in_count / 2);
		}

		Stream_Write_UINT16(in_s, in_color1);
		Stream_Write_UINT16(in_s, in_color2);
	}

	return 0;
}

#define OUT_BICOLOR_COUNT2(in_count, in_s, in_color1, in_color2) \
	in_count = out_bicolor_count_2(in_count, in_s, in_color1, in_color2)

/*****************************************************************************/
/* bicolor */
static INLINE UINT16 out_bicolor_count_3(UINT16 in_count, wStream* in_s, UINT32 in_color1,
                                         UINT32 in_color2)
{
	if (in_count > 0)
	{
		if (in_count / 2 < 16)
		{
			const BYTE temp = ((0xe << 4) | (in_count / 2)) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count / 2 < 256 + 16)
		{
			const BYTE temp = (in_count / 2 - 16) & 0xFF;
			Stream_Write_UINT8(in_s, 0xe0);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf8);
			Stream_Write_UINT16(in_s, in_count / 2);
		}

		Stream_Write_UINT8(in_s, in_color1 & 0xFF);
		Stream_Write_UINT8(in_s, (in_color1 >> 8) & 0xFF);
		Stream_Write_UINT8(in_s, (in_color1 >> 16) & 0xFF);
		Stream_Write_UINT8(in_s, in_color2 & 0xFF);
		Stream_Write_UINT8(in_s, (in_color2 >> 8) & 0xFF);
		Stream_Write_UINT8(in_s, (in_color2 >> 16) & 0xFF);
	}

	return 0;
}

#define OUT_BICOLOR_COUNT3(in_count, in_s, in_color1, in_color2) \
	in_count = out_bicolor_count_3(in_count, in_s, in_color1, in_color2)

/*****************************************************************************/
/* fill */
static INLINE UINT16 out_fill_count_2(UINT16 in_count, wStream* in_s)
{
	if (in_count > 0)
	{
		if (in_count < 32)
		{
			Stream_Write_UINT8(in_s, in_count & 0xFF);
		}
		else if (in_count < 256 + 32)
		{
			const BYTE temp = (in_count - 32) & 0xFF;
			Stream_Write_UINT8(in_s, 0x0);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf0);
			Stream_Write_UINT16(in_s, in_count);
		}
	}

	return 0;
}

#define OUT_FILL_COUNT2(in_count, in_s) in_count = out_fill_count_2(in_count, in_s)

/*****************************************************************************/
/* fill */
static INLINE UINT16 out_fill_count_3(UINT16 in_count, wStream* in_s)
{
	if (in_count > 0)
	{
		if (in_count < 32)
		{
			Stream_Write_UINT8(in_s, in_count & 0xFF);
		}
		else if (in_count < 256 + 32)
		{
			const BYTE temp = (in_count - 32) & 0xFF;
			Stream_Write_UINT8(in_s, 0x0);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf0);
			Stream_Write_UINT16(in_s, in_count);
		}
	}

	return 0;
}
#define OUT_FILL_COUNT3(in_count, in_s) in_count = out_fill_count_3(in_count, in_s)

/*****************************************************************************/
/* mix */
static INLINE UINT16 out_mix_count_2(UINT16 in_count, wStream* in_s)
{
	if (in_count > 0)
	{
		if (in_count < 32)
		{
			const BYTE temp = ((0x1 << 5) | in_count) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count < 256 + 32)
		{
			const BYTE temp = (in_count - 32) & 0xFF;
			Stream_Write_UINT8(in_s, 0x20);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf1);
			Stream_Write_UINT16(in_s, in_count);
		}
	}

	return 0;
}
#define OUT_MIX_COUNT2(in_count, in_s) in_count = out_mix_count_2(in_count, in_s)

/*****************************************************************************/
/* mix */
static INLINE UINT16 out_mix_count_3(UINT16 in_count, wStream* in_s)
{
	if (in_count > 0)
	{
		if (in_count < 32)
		{
			const BYTE temp = ((0x1 << 5) | in_count) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count < 256 + 32)
		{
			const BYTE temp = (in_count - 32) & 0xFF;
			Stream_Write_UINT8(in_s, 0x20);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf1);
			Stream_Write_UINT16(in_s, in_count);
		}
	}

	return 0;
}

#define OUT_MIX_COUNT3(in_count, in_s) in_count = out_mix_count_3(in_count, in_s)

/*****************************************************************************/
/* fom */
static INLINE UINT16 out_from_count_2(UINT16 in_count, wStream* in_s, const char* in_mask,
                                      size_t in_mask_len)
{
	if (in_count > 0)
	{
		if ((in_count % 8) == 0 && in_count < 249)
		{
			const BYTE temp = ((0x2 << 5) | (in_count / 8)) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count < 256)
		{
			const BYTE temp = (in_count - 1) & 0xFF;
			Stream_Write_UINT8(in_s, 0x40);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf2);
			Stream_Write_UINT16(in_s, in_count);
		}

		Stream_Write(in_s, in_mask, in_mask_len);
	}

	return 0;
}
#define OUT_FOM_COUNT2(in_count, in_s, in_mask, in_mask_len) \
	in_count = out_from_count_2(in_count, in_s, in_mask, in_mask_len)

/*****************************************************************************/
/* fill or mix (fom) */
static INLINE UINT16 out_from_count_3(UINT16 in_count, wStream* in_s, const char* in_mask,
                                      size_t in_mask_len)
{
	if (in_count > 0)
	{
		if ((in_count % 8) == 0 && in_count < 249)
		{
			const BYTE temp = ((0x2 << 5) | (in_count / 8)) & 0xFF;
			Stream_Write_UINT8(in_s, temp);
		}
		else if (in_count < 256)
		{
			const BYTE temp = (in_count - 1) & 0xFF;
			Stream_Write_UINT8(in_s, 0x40);
			Stream_Write_UINT8(in_s, temp);
		}
		else
		{
			Stream_Write_UINT8(in_s, 0xf2);
			Stream_Write_UINT16(in_s, in_count);
		}

		Stream_Write(in_s, in_mask, in_mask_len);
	}

	return 0;
}
#define OUT_FOM_COUNT3(in_count, in_s, in_mask, in_mask_len) \
	in_count = out_from_count_3(in_count, in_s, in_mask, in_mask_len)

#define TEST_FILL ((last_line == 0 && pixel == 0) || (last_line != 0 && pixel == ypixel))
#define TEST_MIX ((last_line == 0 && pixel == mix) || (last_line != 0 && pixel == (ypixel ^ mix)))
#define TEST_FOM TEST_FILL || TEST_MIX
#define TEST_COLOR pixel == last_pixel
#define TEST_BICOLOR                                                        \
	((pixel != last_pixel) &&                                               \
	 ((!bicolor_spin && (pixel == bicolor1) && (last_pixel == bicolor2)) || \
	  (bicolor_spin && (pixel == bicolor2) && (last_pixel == bicolor1))))
#define RESET_COUNTS          \
	do                        \
	{                         \
		bicolor_count = 0;    \
		fill_count = 0;       \
		color_count = 0;      \
		mix_count = 0;        \
		fom_count = 0;        \
		fom_mask_len = 0;     \
		bicolor_spin = FALSE; \
	} while (0)

static SSIZE_T freerdp_bitmap_compress_24(const void* srcData, UINT32 width, UINT32 height,
                                          wStream* s, UINT32 byte_limit, UINT32 start_line,
                                          wStream* temp_s, UINT32 e)
{
	char fom_mask[8192]; /* good for up to 64K bitmap */
	SSIZE_T lines_sent = 0;
	UINT16 count = 0;
	UINT16 color_count = 0;
	UINT32 last_pixel = 0;
	UINT32 last_ypixel = 0;
	UINT16 bicolor_count = 0;
	UINT32 bicolor1 = 0;
	UINT32 bicolor2 = 0;
	BOOL bicolor_spin = FALSE;
	UINT32 end = width + e;
	UINT32 out_count = end * 3;
	UINT16 fill_count = 0;
	UINT16 mix_count = 0;
	const UINT32 mix = 0xFFFFFF;
	UINT16 fom_count = 0;
	size_t fom_mask_len = 0;
	const char* start = (const char*)srcData;
	const char* line = start + width * start_line * 4;
	const char* last_line = NULL;

	while ((line >= start) && (out_count < 32768))
	{
		UINT32 j;
		size_t i = Stream_GetPosition(s) + count * 3U;

		if ((i - (color_count * 3) >= byte_limit) && (i - (bicolor_count * 3) >= byte_limit) &&
		    (i - (fill_count * 3) >= byte_limit) && (i - (mix_count * 3) >= byte_limit) &&
		    (i - (fom_count * 3) >= byte_limit))
		{
			break;
		}

		out_count += end * 3;

		for (j = 0; j < end; j++)
		{
			/* read next pixel */
			const UINT32 pixel = IN_PIXEL32(line, j, 0, width, last_pixel);
			const UINT32 ypixel = IN_PIXEL32(last_line, j, 0, width, last_ypixel);

			if (!TEST_FILL)
			{
				if (fill_count > 3 && fill_count >= color_count && fill_count >= bicolor_count &&
				    fill_count >= mix_count && fill_count >= fom_count)
				{
					if (fill_count > count)
						return -1;

					count -= fill_count;
					OUT_COPY_COUNT3(count, s, temp_s);
					OUT_FILL_COUNT3(fill_count, s);
					RESET_COUNTS;
				}

				fill_count = 0;
			}

			if (!TEST_MIX)
			{
				if (mix_count > 3 && mix_count >= fill_count && mix_count >= bicolor_count &&
				    mix_count >= color_count && mix_count >= fom_count)
				{
					if (mix_count > count)
						return -1;

					count -= mix_count;
					OUT_COPY_COUNT3(count, s, temp_s);
					OUT_MIX_COUNT3(mix_count, s);
					RESET_COUNTS;
				}

				mix_count = 0;
			}

			if (!(TEST_COLOR))
			{
				if (color_count > 3 && color_count >= fill_count && color_count >= bicolor_count &&
				    color_count >= mix_count && color_count >= fom_count)
				{
					if (color_count > count)
						return -1;

					count -= color_count;
					OUT_COPY_COUNT3(count, s, temp_s);
					OUT_COLOR_COUNT3(color_count, s, last_pixel);
					RESET_COUNTS;
				}

				color_count = 0;
			}

			if (!TEST_BICOLOR)
			{
				if (bicolor_count > 3 && bicolor_count >= fill_count &&
				    bicolor_count >= color_count && bicolor_count >= mix_count &&
				    bicolor_count >= fom_count)
				{
					if ((bicolor_count % 2) != 0)
						bicolor_count--;

					if (bicolor_count > count)
						return -1;

					count -= bicolor_count;
					OUT_COPY_COUNT3(count, s, temp_s);
					OUT_BICOLOR_COUNT3(bicolor_count, s, bicolor2, bicolor1);
					RESET_COUNTS;
				}

				bicolor_count = 0;
				bicolor1 = last_pixel;
				bicolor2 = pixel;
				bicolor_spin = FALSE;
			}

			if (!(TEST_FOM))
			{
				if (fom_count > 3 && fom_count >= fill_count && fom_count >= color_count &&
				    fom_count >= mix_count && fom_count >= bicolor_count)
				{
					if (fom_count > count)
						return -1;

					count -= fom_count;
					OUT_COPY_COUNT3(count, s, temp_s);
					OUT_FOM_COUNT3(fom_count, s, fom_mask, fom_mask_len);
					RESET_COUNTS;
				}

				fom_count = 0;
				fom_mask_len = 0;
			}

			if (TEST_FILL)
			{
				fill_count++;
			}

			if (TEST_MIX)
			{
				mix_count++;
			}

			if (TEST_COLOR)
			{
				color_count++;
			}

			if (TEST_BICOLOR)
			{
				bicolor_spin = !bicolor_spin;
				bicolor_count++;
			}

			if (TEST_FOM)
			{
				if ((fom_count % 8) == 0)
				{
					fom_mask[fom_mask_len] = 0;
					fom_mask_len++;
				}

				if (pixel == (ypixel ^ mix))
				{
					fom_mask[fom_mask_len - 1] |= (1 << (fom_count % 8));
				}

				fom_count++;
			}

			Stream_Write_UINT8(temp_s, pixel & 0xff);
			Stream_Write_UINT8(temp_s, (pixel >> 8) & 0xff);
			Stream_Write_UINT8(temp_s, (pixel >> 16) & 0xff);
			count++;
			last_pixel = pixel;
			last_ypixel = ypixel;
		}

		/* can't take fix, mix, or fom past first line */
		if (last_line == 0)
		{
			if (fill_count > 3 && fill_count >= color_count && fill_count >= bicolor_count &&
			    fill_count >= mix_count && fill_count >= fom_count)
			{
				if (fill_count > count)
					return -1;

				count -= fill_count;
				OUT_COPY_COUNT3(count, s, temp_s);
				OUT_FILL_COUNT3(fill_count, s);
				RESET_COUNTS;
			}

			fill_count = 0;

			if (mix_count > 3 && mix_count >= fill_count && mix_count >= bicolor_count &&
			    mix_count >= color_count && mix_count >= fom_count)
			{
				if (mix_count > count)
					return -1;

				count -= mix_count;
				OUT_COPY_COUNT3(count, s, temp_s);
				OUT_MIX_COUNT3(mix_count, s);
				RESET_COUNTS;
			}

			mix_count = 0;

			if (fom_count > 3 && fom_count >= fill_count && fom_count >= color_count &&
			    fom_count >= mix_count && fom_count >= bicolor_count)
			{
				if (fom_count > count)
					return -1;

				count -= fom_count;
				OUT_COPY_COUNT3(count, s, temp_s);
				OUT_FOM_COUNT3(fom_count, s, fom_mask, fom_mask_len);
				RESET_COUNTS;
			}

			fom_count = 0;
			fom_mask_len = 0;
		}

		last_line = line;
		line = line - width * 4;
		start_line--;
		lines_sent++;
	}

	Stream_SetPosition(temp_s, 0);

	if (fill_count > 3 && fill_count >= color_count && fill_count >= bicolor_count &&
	    fill_count >= mix_count && fill_count >= fom_count)
	{
		if (fill_count > count)
			return -1;

		count -= fill_count;
		OUT_COPY_COUNT3(count, s, temp_s);
		OUT_FILL_COUNT3(fill_count, s);
	}
	else if (mix_count > 3 && mix_count >= color_count && mix_count >= bicolor_count &&
	         mix_count >= fill_count && mix_count >= fom_count)
	{
		if (mix_count > count)
			return -1;

		count -= mix_count;
		OUT_COPY_COUNT3(count, s, temp_s);
		OUT_MIX_COUNT3(mix_count, s);
	}
	else if (color_count > 3 && color_count >= mix_count && color_count >= bicolor_count &&
	         color_count >= fill_count && color_count >= fom_count)
	{
		if (color_count > count)
			return -1;

		count -= color_count;
		OUT_COPY_COUNT3(count, s, temp_s);
		OUT_COLOR_COUNT3(color_count, s, last_pixel);
	}
	else if (bicolor_count > 3 && bicolor_count >= mix_count && bicolor_count >= color_count &&
	         bicolor_count >= fill_count && bicolor_count >= fom_count)
	{
		if ((bicolor_count % 2) != 0)
			bicolor_count--;

		if (bicolor_count > count)
			return -1;

		count -= bicolor_count;
		OUT_COPY_COUNT3(count, s, temp_s);
		OUT_BICOLOR_COUNT3(bicolor_count, s, bicolor2, bicolor1);

		if (bicolor_count > count)
			return -1;

		count -= bicolor_count;
		OUT_COPY_COUNT3(count, s, temp_s);
		OUT_BICOLOR_COUNT3(bicolor_count, s, bicolor1, bicolor2);
	}
	else if (fom_count > 3 && fom_count >= mix_count && fom_count >= color_count &&
	         fom_count >= fill_count && fom_count >= bicolor_count)
	{
		if (fom_count > count)
			return -1;

		count -= fom_count;
		OUT_COPY_COUNT3(count, s, temp_s);
		OUT_FOM_COUNT3(fom_count, s, fom_mask, fom_mask_len);
	}
	else
	{
		OUT_COPY_COUNT3(count, s, temp_s);
	}

	return lines_sent;
}

static SSIZE_T freerdp_bitmap_compress_16(const void* srcData, UINT32 width, UINT32 height,
                                          wStream* s, UINT32 bpp, UINT32 byte_limit,
                                          UINT32 start_line, wStream* temp_s, UINT32 e)
{
	char fom_mask[8192]; /* good for up to 64K bitmap */
	SSIZE_T lines_sent = 0;
	UINT16 count = 0;
	UINT16 color_count = 0;
	UINT16 last_pixel = 0;
	UINT16 last_ypixel = 0;
	UINT16 bicolor_count = 0;
	UINT16 bicolor1 = 0;
	UINT16 bicolor2 = 0;
	BOOL bicolor_spin = FALSE;
	UINT32 end = width + e;
	UINT32 out_count = end * 2;
	UINT16 fill_count = 0;
	UINT16 mix_count = 0;
	const UINT32 mix = (bpp == 15) ? 0xBA1F : 0xFFFF;
	UINT16 fom_count = 0;
	size_t fom_mask_len = 0;
	const char* start = (const char*)srcData;
	const char* line = start + width * start_line * 2;
	const char* last_line = NULL;

	while ((line >= start) && (out_count < 32768))
	{
		UINT32 j;
		size_t i = Stream_GetPosition(s) + count * 2;

		if ((i - (color_count * 2) >= byte_limit) && (i - (bicolor_count * 2) >= byte_limit) &&
		    (i - (fill_count * 2) >= byte_limit) && (i - (mix_count * 2) >= byte_limit) &&
		    (i - (fom_count * 2) >= byte_limit))
		{
			break;
		}

		out_count += end * 2;

		for (j = 0; j < end; j++)
		{
			/* read next pixel */
			const UINT16 pixel = IN_PIXEL16(line, j, 0, width, last_pixel);
			const UINT16 ypixel = IN_PIXEL16(last_line, j, 0, width, last_ypixel);

			if (!TEST_FILL)
			{
				if (fill_count > 3 && fill_count >= color_count && fill_count >= bicolor_count &&
				    fill_count >= mix_count && fill_count >= fom_count)
				{
					if (fill_count > count)
						return -1;

					count -= fill_count;
					OUT_COPY_COUNT2(count, s, temp_s);
					OUT_FILL_COUNT2(fill_count, s);
					RESET_COUNTS;
				}

				fill_count = 0;
			}

			if (!TEST_MIX)
			{
				if (mix_count > 3 && mix_count >= fill_count && mix_count >= bicolor_count &&
				    mix_count >= color_count && mix_count >= fom_count)
				{
					if (mix_count > count)
						return -1;

					count -= mix_count;
					OUT_COPY_COUNT2(count, s, temp_s);
					OUT_MIX_COUNT2(mix_count, s);
					RESET_COUNTS;
				}

				mix_count = 0;
			}

			if (!(TEST_COLOR))
			{
				if (color_count > 3 && color_count >= fill_count && color_count >= bicolor_count &&
				    color_count >= mix_count && color_count >= fom_count)
				{
					if (color_count > count)
						return -1;

					count -= color_count;
					OUT_COPY_COUNT2(count, s, temp_s);
					OUT_COLOR_COUNT2(color_count, s, last_pixel);
					RESET_COUNTS;
				}

				color_count = 0;
			}

			if (!TEST_BICOLOR)
			{
				if ((bicolor_count > 3) && (bicolor_count >= fill_count) &&
				    (bicolor_count >= color_count) && (bicolor_count >= mix_count) &&
				    (bicolor_count >= fom_count))
				{
					if ((bicolor_count % 2) != 0)
						bicolor_count--;

					if (bicolor_count > count)
						return -1;

					count -= bicolor_count;
					OUT_COPY_COUNT2(count, s, temp_s);
					OUT_BICOLOR_COUNT2(bicolor_count, s, bicolor2, bicolor1);
					RESET_COUNTS;
				}

				bicolor_count = 0;
				bicolor1 = last_pixel;
				bicolor2 = pixel;
				bicolor_spin = FALSE;
			}

			if (!(TEST_FOM))
			{
				if (fom_count > 3 && fom_count >= fill_count && fom_count >= color_count &&
				    fom_count >= mix_count && fom_count >= bicolor_count)
				{
					if (fom_count > count)
						return -1;

					count -= fom_count;
					OUT_COPY_COUNT2(count, s, temp_s);
					OUT_FOM_COUNT2(fom_count, s, fom_mask, fom_mask_len);
					RESET_COUNTS;
				}

				fom_count = 0;
				fom_mask_len = 0;
			}

			if (TEST_FILL)
			{
				fill_count++;
			}

			if (TEST_MIX)
			{
				mix_count++;
			}

			if (TEST_COLOR)
			{
				color_count++;
			}

			if (TEST_BICOLOR)
			{
				bicolor_spin = !bicolor_spin;
				bicolor_count++;
			}

			if (TEST_FOM)
			{
				if ((fom_count % 8) == 0)
				{
					fom_mask[fom_mask_len] = 0;
					fom_mask_len++;
				}

				if (pixel == (ypixel ^ mix))
				{
					fom_mask[fom_mask_len - 1] |= (1 << (fom_count % 8));
				}

				fom_count++;
			}

			Stream_Write_UINT16(temp_s, pixel);
			count++;
			last_pixel = pixel;
			last_ypixel = ypixel;
		}

		/* can't take fix, mix, or fom past first line */
		if (last_line == 0)
		{
			if (fill_count > 3 && fill_count >= color_count && fill_count >= bicolor_count &&
			    fill_count >= mix_count && fill_count >= fom_count)
			{
				if (fill_count > count)
					return -1;

				count -= fill_count;
				OUT_COPY_COUNT2(count, s, temp_s);
				OUT_FILL_COUNT2(fill_count, s);
				RESET_COUNTS;
			}

			fill_count = 0;

			if (mix_count > 3 && mix_count >= fill_count && mix_count >= bicolor_count &&
			    mix_count >= color_count && mix_count >= fom_count)
			{
				if (mix_count > count)
					return -1;

				count -= mix_count;
				OUT_COPY_COUNT2(count, s, temp_s);
				OUT_MIX_COUNT2(mix_count, s);
				RESET_COUNTS;
			}

			mix_count = 0;

			if (fom_count > 3 && fom_count >= fill_count && fom_count >= color_count &&
			    fom_count >= mix_count && fom_count >= bicolor_count)
			{
				if (fom_count > count)
					return -1;

				count -= fom_count;
				OUT_COPY_COUNT2(count, s, temp_s);
				OUT_FOM_COUNT2(fom_count, s, fom_mask, fom_mask_len);
				RESET_COUNTS;
			}

			fom_count = 0;
			fom_mask_len = 0;
		}

		last_line = line;
		line = line - width * 2;
		start_line--;
		lines_sent++;
	}

	Stream_SetPosition(temp_s, 0);

	if (fill_count > 3 && fill_count >= color_count && fill_count >= bicolor_count &&
	    fill_count >= mix_count && fill_count >= fom_count)
	{
		if (fill_count > count)
			return -1;

		count -= fill_count;
		OUT_COPY_COUNT2(count, s, temp_s);
		OUT_FILL_COUNT2(fill_count, s);
	}
	else if (mix_count > 3 && mix_count >= color_count && mix_count >= bicolor_count &&
	         mix_count >= fill_count && mix_count >= fom_count)
	{
		if (mix_count > count)
			return -1;

		count -= mix_count;
		OUT_COPY_COUNT2(count, s, temp_s);
		OUT_MIX_COUNT2(mix_count, s);
	}
	else if (color_count > 3 && color_count >= mix_count && color_count >= bicolor_count &&
	         color_count >= fill_count && color_count >= fom_count)
	{
		if (color_count > count)
			return -1;

		count -= color_count;
		OUT_COPY_COUNT2(count, s, temp_s);
		OUT_COLOR_COUNT2(color_count, s, last_pixel);
	}
	else if (bicolor_count > 3 && bicolor_count >= mix_count && bicolor_count >= color_count &&
	         bicolor_count >= fill_count && bicolor_count >= fom_count)
	{
		if ((bicolor_count % 2) != 0)
			bicolor_count--;

		if (bicolor_count > count)
			return -1;

		count -= bicolor_count;
		OUT_COPY_COUNT2(count, s, temp_s);
		OUT_BICOLOR_COUNT2(bicolor_count, s, bicolor2, bicolor1);

		if (bicolor_count > count)
			return -1;

		count -= bicolor_count;
		OUT_COPY_COUNT2(count, s, temp_s);
		OUT_BICOLOR_COUNT2(bicolor_count, s, bicolor1, bicolor2);
	}
	else if (fom_count > 3 && fom_count >= mix_count && fom_count >= color_count &&
	         fom_count >= fill_count && fom_count >= bicolor_count)
	{
		if (fom_count > count)
			return -1;

		count -= fom_count;
		OUT_COPY_COUNT2(count, s, temp_s);
		OUT_FOM_COUNT2(fom_count, s, fom_mask, fom_mask_len);
	}
	else
	{
		OUT_COPY_COUNT2(count, s, temp_s);
	}

	return lines_sent;
}

SSIZE_T freerdp_bitmap_compress(const void* srcData, UINT32 width, UINT32 height, wStream* s,
                                UINT32 bpp, UINT32 byte_limit, UINT32 start_line, wStream* temp_s,
                                UINT32 e)
{
	Stream_SetPosition(temp_s, 0);

	switch (bpp)
	{
		case 15:
		case 16:
			return freerdp_bitmap_compress_16(srcData, width, height, s, bpp, byte_limit,
			                                  start_line, temp_s, e);

		case 24:
			return freerdp_bitmap_compress_24(srcData, width, height, s, byte_limit, start_line,
			                                  temp_s, e);

		default:
			return -1;
	}
}

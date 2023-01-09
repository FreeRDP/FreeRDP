/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Color Conversion Routines
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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

#ifndef FREERDP_CODEC_COLOR_H
#define FREERDP_CODEC_COLOR_H

#include <freerdp/api.h>

#define FREERDP_PIXEL_FORMAT_TYPE_A 0
#define FREERDP_PIXEL_FORMAT_TYPE_ARGB 1
#define FREERDP_PIXEL_FORMAT_TYPE_ABGR 2
#define FREERDP_PIXEL_FORMAT_TYPE_RGBA 3
#define FREERDP_PIXEL_FORMAT_TYPE_BGRA 4

#define FREERDP_PIXEL_FORMAT_IS_ABGR(_format) \
	(FREERDP_PIXEL_FORMAT_TYPE(_format) == FREERDP_PIXEL_FORMAT_TYPE_ABGR)

enum FREERDP_IMAGE_FLAGS
{
	FREERDP_FLIP_NONE = 0,
	FREERDP_FLIP_VERTICAL = 1,
	FREERDP_FLIP_HORIZONTAL = 2,
	FREERDP_KEEP_DST_ALPHA = 4
};

#define FREERDP_PIXEL_FORMAT(_bpp, _type, _a, _r, _g, _b) \
	((_bpp << 24) | (_type << 16) | (_a << 12) | (_r << 8) | (_g << 4) | (_b))

#define FREERDP_PIXEL_FORMAT_TYPE(_format) (((_format) >> 16) & 0x07)

/*** Design considerations
 *
 * The format naming scheme is based on byte position in memory.
 * RGBA for example names a byte array with red on positon 0, green on 1 etc.
 *
 * To read and write the appropriate format from / to memory use FreeRDPReadColor and
 * FreeRDPWriteColor.
 *
 * The single pixel manipulation functions use an intermediate integer representation
 * that must not be interpreted outside the functions as it is platform dependent.
 *
 * X for alpha channel denotes unused (but existing) alpha channel data.
 */

/* 32bpp formats */
#define PIXEL_FORMAT_ARGB32 FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 8, 8, 8, 8)
#define PIXEL_FORMAT_XRGB32 FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 8, 8, 8)
#define PIXEL_FORMAT_ABGR32 FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 8, 8, 8, 8)
#define PIXEL_FORMAT_XBGR32 FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 8, 8, 8)
#define PIXEL_FORMAT_BGRA32 FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_BGRA, 8, 8, 8, 8)
#define PIXEL_FORMAT_BGRX32 FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_BGRA, 0, 8, 8, 8)
#define PIXEL_FORMAT_RGBA32 FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_RGBA, 8, 8, 8, 8)
#define PIXEL_FORMAT_RGBX32 FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_RGBA, 0, 8, 8, 8)
#define PIXEL_FORMAT_BGRX32_DEPTH30 \
	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_BGRA, 0, 10, 10, 10)
#define PIXEL_FORMAT_RGBX32_DEPTH30 \
	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_RGBA, 0, 10, 10, 10)

/* 24bpp formats */
#define PIXEL_FORMAT_RGB24 FREERDP_PIXEL_FORMAT(24, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 8, 8, 8)
#define PIXEL_FORMAT_BGR24 FREERDP_PIXEL_FORMAT(24, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 8, 8, 8)

/* 16bpp formats */
#define PIXEL_FORMAT_RGB16 FREERDP_PIXEL_FORMAT(16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 5, 6, 5)
#define PIXEL_FORMAT_BGR16 FREERDP_PIXEL_FORMAT(16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 5, 6, 5)
#define PIXEL_FORMAT_ARGB15 FREERDP_PIXEL_FORMAT(16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 1, 5, 5, 5)
#define PIXEL_FORMAT_RGB15 FREERDP_PIXEL_FORMAT(15, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 5, 5, 5)
#define PIXEL_FORMAT_ABGR15 FREERDP_PIXEL_FORMAT(16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 1, 5, 5, 5)
#define PIXEL_FORMAT_BGR15 FREERDP_PIXEL_FORMAT(15, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 5, 5, 5)

/* 8bpp formats */
#define PIXEL_FORMAT_RGB8 FREERDP_PIXEL_FORMAT(8, FREERDP_PIXEL_FORMAT_TYPE_A, 8, 0, 0, 0)

/* 4 bpp formats */
#define PIXEL_FORMAT_A4 FREERDP_PIXEL_FORMAT(4, FREERDP_PIXEL_FORMAT_TYPE_A, 4, 0, 0, 0)

/* 1bpp formats */
#define PIXEL_FORMAT_MONO FREERDP_PIXEL_FORMAT(1, FREERDP_PIXEL_FORMAT_TYPE_A, 1, 0, 0, 0)

struct gdi_palette
{
	UINT32 format;
	UINT32 palette[256];
};
typedef struct gdi_palette gdiPalette;

#ifdef __cplusplus
extern "C"
{
#endif

	/* Compare two color formats but ignore differences in alpha channel.
	 */
	FREERDP_API DWORD FreeRDPAreColorFormatsEqualNoAlpha(DWORD first, DWORD second);

	/* Color Space Conversions: http://msdn.microsoft.com/en-us/library/ff566496/ */

	/***
	 *
	 * Get a string representation of a color
	 *
	 * @param format The pixel color format
	 *
	 * @return A string representation of format
	 */
#if defined(WITH_FREERDP_DEPRECATED)
#define GetColorFormatName(...) FreeRDPGetColorFormatName(__VA_ARGS__)
#endif
	FREERDP_API const char* FreeRDPGetColorFormatName(UINT32 format);

	/***
	 *
	 * Converts a pixel color in internal representation to its red, green, blue
	 * and alpha components.
	 *
	 * @param color  The color in format internal representation
	 * @param format one of PIXEL_FORMAT_* color format defines
	 * @param _r      red color value
	 * @param _g      green color value
	 * @param _b      blue color value
	 * @param _a      alpha color value
	 * @param palette pallete to use (only used for 8 bit color!)
	 */
#if defined(WITH_FREERDP_DEPRECATED)
#define SplitColor(...) FreeRDPSplitColor(__VA_ARGS__)
#endif
	FREERDP_API void FreeRDPSplitColor(UINT32 color, UINT32 format, BYTE* _r, BYTE* _g, BYTE* _b,
	                                   BYTE* _a, const gdiPalette* palette);

	/***
	 *
	 * Converts red, green, blue and alpha values to internal representation.
	 *
	 * @param format one of PIXEL_FORMAT_* color format defines
	 * @param r      red color value
	 * @param g      green color value
	 * @param b      blue color value
	 * @param a      alpha color value
	 *
	 * @return       The pixel color in the desired format. Value is in internal
	 *               representation.
	 */
#if defined(WITH_FREERDP_DEPRECATED)
#define GetColor(...) FreeRDPGetColor(__VA_ARGS__)
#endif
	FREERDP_API UINT32 FreeRDPGetColor(UINT32 format, BYTE r, BYTE g, BYTE b, BYTE a);

	/***
	 *
	 * Returns the number of bits the format format uses.
	 *
	 * @param format One of PIXEL_FORMAT_* defines
	 *
	 * @return The number of bits the format requires per pixel.
	 */
#if defined(WITH_FREERDP_DEPRECATED)
#define GetBitsPerPixel(...) FreeRDPGetBitsPerPixel(__VA_ARGS__)
#endif
	static INLINE UINT32 FreeRDPGetBitsPerPixel(UINT32 format)
	{
		return (((format) >> 24) & 0x3F);
	}

	/***
	 * @param format one of PIXEL_FORMAT_* color format defines
	 *
	 * @return TRUE if the format has an alpha channel, FALSE otherwise.
	 */
#if defined(WITH_FREERDP_DEPRECATED)
#define ColorHasAlpha(...) FreeRDPColorHasAlpha(__VA_ARGS__)
#endif
	static INLINE BOOL FreeRDPColorHasAlpha(UINT32 format)
	{
		UINT32 alpha = (((format) >> 12) & 0x0F);

		if (alpha == 0)
			return FALSE;

		return TRUE;
	}

	/***
	 *
	 * Read a pixel from memory to internal representation
	 *
	 * @param src    The source buffer
	 * @param format The PIXEL_FORMAT_* define the source buffer uses for encoding
	 *
	 * @return The pixel color in internal representation
	 */
#if defined(WITH_FREERDP_DEPRECATED)
#define ReadColor(...) FreeRDPReadColor(__VA_ARGS__)
#endif
	FREERDP_API UINT32 FreeRDPReadColor(const BYTE* src, UINT32 format);

	/***
	 *
	 * Write a pixel from internal representation to memory
	 *
	 * @param dst    The destination buffer
	 * @param format The PIXEL_FORMAT_* define for encoding
	 * @param color  The pixel color in internal representation
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
#if defined(WITH_FREERDP_DEPRECATED)
#define WriteColor(...) FreeRDPWriteColor(__VA_ARGS__)
#define WriteColorIgnoreAlpha(...) FreeRDPWriteColorIgnoreAlpha(__VA_ARGS__)
#endif
	FREERDP_API BOOL FreeRDPWriteColor(BYTE* dst, UINT32 format, UINT32 color);
	FREERDP_API BOOL FreeRDPWriteColorIgnoreAlpha(BYTE* dst, UINT32 format, UINT32 color);

	/***
	 *
	 * Converts a pixel in internal representation format srcFormat to internal
	 * representation format dstFormat
	 *
	 * @param color      The pixel color in srcFormat representation
	 * @param srcFormat  The PIXEL_FORMAT_* of color
	 * @param dstFormat  The PIXEL_FORMAT_* of the return.
	 * @param palette    pallete to use (only used for 8 bit color!)
	 *
	 * @return           The converted pixel color in dstFormat representation
	 */
#if defined(WITH_FREERDP_DEPRECATED)
#define ConvertColor(...) FreeRDPConvertColor(__VA_ARGS__)
#endif
	static INLINE UINT32 FreeRDPConvertColor(UINT32 color, UINT32 srcFormat, UINT32 dstFormat,
	                                         const gdiPalette* palette)
	{
		BYTE r = 0;
		BYTE g = 0;
		BYTE b = 0;
		BYTE a = 0;
		FreeRDPSplitColor(color, srcFormat, &r, &g, &b, &a, palette);
		return FreeRDPGetColor(dstFormat, r, g, b, a);
	}

	/***
	 *
	 * Returns the number of bytes the format format uses.
	 *
	 * @param format One of PIXEL_FORMAT_* defines
	 *
	 * @return The number of bytes the format requires per pixel.
	 */
#if defined(WITH_FREERDP_DEPRECATED)
#define GetBytesPerPixel(...) FreeRDPGetBytesPerPixel(__VA_ARGS__)
#endif
	static INLINE UINT32 FreeRDPGetBytesPerPixel(UINT32 format)
	{
		return (FreeRDPGetBitsPerPixel(format) + 7) / 8;
	}

	/***
	 *
	 * @param width    width to copy in pixels
	 * @param height   height to copy in pixels
	 * @param data      source buffer, must be (nWidth + 7) / 8 bytes long
	 *
	 * @return          A buffer allocated with winpr_aligned_malloc(width * height, 16)
	 *                  if successful, NULL otherwise.
	 */
	FREERDP_API BYTE* freerdp_glyph_convert(UINT32 width, UINT32 height, const BYTE* data);

	/***
	 *
	 * @param pDstData  destination buffer
	 * @param DstFormat destination buffer format
	 * @param nDstStep  destination buffer stride (line in bytes) 0 for default
	 * @param nXDst     destination buffer offset x
	 * @param nYDst     destination buffer offset y
	 * @param nWidth    width to copy in pixels
	 * @param nHeight   height to copy in pixels
	 * @param pSrcData  source buffer, must be (nWidth + 7) / 8 bytes long
	 * @param backColor The background color in internal representation format
	 * @param foreColor The foreground color in internal representation format
	 * @param palette   palette to use (only used for 8 bit color!)
	 *
	 * @return          TRUE if success, FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_image_copy_from_monochrome(BYTE* pDstData, UINT32 DstFormat,
	                                                    UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
	                                                    UINT32 nWidth, UINT32 nHeight,
	                                                    const BYTE* pSrcData, UINT32 backColor,
	                                                    UINT32 foreColor,
	                                                    const gdiPalette* palette);

	/***
	 *
	 * @param pDstData      destination buffer
	 * @param DstFormat     destination buffer format
	 * @param nDstStep      destination buffer stride (line in bytes) 0 for default
	 * @param nXDst         destination buffer offset x
	 * @param nYDst         destination buffer offset y
	 * @param nWidth        width to copy in pixels
	 * @param nHeight       height to copy in pixels
	 * @param bitsColor     icon's image data buffer
	 * @param cbBitsColor   length of the image data buffer in bytes
	 * @param bitsMask      icon's 1bpp image mask buffer
	 * @param cbBitsMask    length of the image mask buffer in bytes
	 * @param colorTable    icon's image color table
	 * @param cbColorTable  length of the image color table buffer in bytes
	 * @param bpp           color image data bits per pixel
	 *
	 * @return              TRUE if success, FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_image_copy_from_icon_data(BYTE* pDstData, UINT32 DstFormat,
	                                                   UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
	                                                   UINT16 nWidth, UINT16 nHeight,
	                                                   const BYTE* bitsColor, UINT16 cbBitsColor,
	                                                   const BYTE* bitsMask, UINT16 cbBitsMask,
	                                                   const BYTE* colorTable, UINT16 cbColorTable,
	                                                   UINT32 bpp);

	/***
	 *
	 * @param pDstData      destination buffer
	 * @param DstFormat     destination buffer format
	 * @param nDstStep      destination buffer stride (line in bytes) 0 for default
	 * @param nXDst         destination buffer offset x
	 * @param nYDst         destination buffer offset y
	 * @param nWidth        width to copy in pixels
	 * @param nHeight       height to copy in pixels
	 * @param xorMask       XOR mask buffer
	 * @param xorMaskLength XOR mask length in bytes
	 * @param andMask       AND mask buffer
	 * @param andMaskLength AND mask length in bytes
	 * @param xorBpp        XOR bits per pixel
	 * @param palette       palette to use (only used for 8 bit color!)
	 *
	 * @return              TRUE if success, FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_image_copy_from_pointer_data(
	    BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
	    UINT32 nWidth, UINT32 nHeight, const BYTE* xorMask, UINT32 xorMaskLength,
	    const BYTE* andMask, UINT32 andMaskLength, UINT32 xorBpp, const gdiPalette* palette);

	/***
	 *
	 * @param pDstData  destination buffer
	 * @param DstFormat destination buffer format
	 * @param nDstStep  destination buffer stride (line in bytes) 0 for default
	 * @param nXDst     destination buffer offset x
	 * @param nYDst     destination buffer offset y
	 * @param nWidth    width to copy in pixels
	 * @param nHeight   height to copy in pixels
	 * @param pSrcData  source buffer
	 * @param SrcFormat source buffer format
	 * @param nSrcStep  source buffer stride (line in bytes) 0 for default
	 * @param nXSrc     source buffer x offset in pixels
	 * @param nYSrc     source buffer y offset in pixels
	 * @param palette   palette to use (only used for 8 bit color!)
	 * @param flags     Image flipping flags FREERDP_FLIP_NONE et al
	 *
	 * @return          TRUE if success, FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_image_copy(BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep,
	                                    UINT32 nXDst, UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
	                                    const BYTE* pSrcData, DWORD SrcFormat, UINT32 nSrcStep,
	                                    UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette,
	                                    UINT32 flags);

	/***
	 *
	 * @param pDstData   destination buffer
	 * @param DstFormat  destination buffer format
	 * @param nDstStep   destination buffer stride (line in bytes) 0 for default
	 * @param nXDst      destination buffer offset x
	 * @param nYDst      destination buffer offset y
	 * @param nDstWidth  width of destination in pixels
	 * @param nDstHeight height of destination in pixels
	 * @param pSrcData   source buffer
	 * @param SrcFormat  source buffer format
	 * @param nSrcStep   source buffer stride (line in bytes) 0 for default
	 * @param nXSrc      source buffer x offset in pixels
	 * @param nYSrc      source buffer y offset in pixels
	 * @param nSrcWidth  width of source in pixels
	 * @param nSrcHeight height of source in pixels
	 *
	 * @return          TRUE if success, FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_image_scale(BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep,
	                                     UINT32 nXDst, UINT32 nYDst, UINT32 nDstWidth,
	                                     UINT32 nDstHeight, const BYTE* pSrcData, DWORD SrcFormat,
	                                     UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
	                                     UINT32 nSrcWidth, UINT32 nSrcHeight);

	/***
	 *
	 * @param pDstData  destionation buffer
	 * @param DstFormat destionation buffer format
	 * @param nDstStep  destionation buffer stride (line in bytes) 0 for default
	 * @param nXDst     destination buffer offset x
	 * @param nYDst     destination buffer offset y
	 * @param nWidth    width to copy in pixels
	 * @param nHeight   height to copy in pixels
	 * @param color     Pixel color in DstFormat (internal representation format,
	 *                  use FreeRDPGetColor to create)
	 *
	 * @return          TRUE if success, FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_image_fill(BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep,
	                                    UINT32 nXDst, UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
	                                    UINT32 color);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_COLOR_H */

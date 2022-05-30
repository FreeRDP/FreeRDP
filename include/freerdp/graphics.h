/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphical Objects
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_GRAPHICS_H
#define FREERDP_GRAPHICS_H

typedef struct rdp_bitmap rdpBitmap;
typedef struct rdp_pointer rdpPointer;
typedef struct rdp_glyph rdpGlyph;

#include <stdlib.h>
#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/* Bitmap Class */
	typedef BOOL (*pBitmap_New)(rdpContext* context, rdpBitmap* bitmap);
	typedef void (*pBitmap_Free)(rdpContext* context, rdpBitmap* bitmap);
	typedef BOOL (*pBitmap_Paint)(rdpContext* context, rdpBitmap* bitmap);
	typedef BOOL (*pBitmap_Decompress)(rdpContext* context, rdpBitmap* bitmap, const BYTE* data,
	                                   UINT32 width, UINT32 height, UINT32 bpp, UINT32 length,
	                                   BOOL compressed, UINT32 codec_id);
	typedef BOOL (*pBitmap_SetSurface)(rdpContext* context, rdpBitmap* bitmap, BOOL primary);

	struct rdp_bitmap
	{
		size_t size;                   /* 0 */
		pBitmap_New New;               /* 1 */
		pBitmap_Free Free;             /* 2 */
		pBitmap_Paint Paint;           /* 3 */
		pBitmap_Decompress Decompress; /* 4 */
		pBitmap_SetSurface SetSurface; /* 5 */
		UINT32 paddingA[16 - 6];       /* 6 */

		UINT32 left;              /* 16 */
		UINT32 top;               /* 17 */
		UINT32 right;             /* 18 */
		UINT32 bottom;            /* 19 */
		UINT32 width;             /* 20 */
		UINT32 height;            /* 21 */
		UINT32 format;            /* 22 */
		UINT32 flags;             /* 23 */
		UINT32 length;            /* 24 */
		BYTE* data;               /* 25 */
		UINT64 key64;             /* 26 */
		UINT32 paddingB[32 - 27]; /* 27 */

		BOOL compressed;          /* 32 */
		BOOL ephemeral;           /* 33 */
		UINT32 paddingC[64 - 34]; /* 34 */
	};

	FREERDP_API rdpBitmap* Bitmap_Alloc(rdpContext* context);
	FREERDP_API BOOL Bitmap_SetRectangle(rdpBitmap* bitmap, UINT16 left, UINT16 top, UINT16 right,
	                                     UINT16 bottom);
	FREERDP_API BOOL Bitmap_SetDimensions(rdpBitmap* bitmap, UINT16 width, UINT16 height);

	/* Pointer Class */

	typedef BOOL (*pPointer_New)(rdpContext* context, rdpPointer* pointer);
	typedef void (*pPointer_Free)(rdpContext* context, rdpPointer* pointer);
	typedef BOOL (*pPointer_Set)(rdpContext* context, rdpPointer* pointer);
	typedef BOOL (*pPointer_SetNull)(rdpContext* context);
	typedef BOOL (*pPointer_SetDefault)(rdpContext* context);
	typedef BOOL (*pPointer_SetPosition)(rdpContext* context, UINT32 x, UINT32 y);

	struct rdp_pointer
	{
		size_t size;                      /* 0 */
		pPointer_New New;                 /* 1 */
		pPointer_Free Free;               /* 2 */
		pPointer_Set Set;                 /* 3 */
		pPointer_SetNull SetNull;         /* 4*/
		pPointer_SetDefault SetDefault;   /* 5 */
		pPointer_SetPosition SetPosition; /* 6 */
		UINT32 paddingA[16 - 7];          /* 7 */

		UINT32 xPos;              /* 16 */
		UINT32 yPos;              /* 17 */
		UINT32 width;             /* 18 */
		UINT32 height;            /* 19 */
		UINT32 xorBpp;            /* 20 */
		UINT32 lengthAndMask;     /* 21 */
		UINT32 lengthXorMask;     /* 22 */
		BYTE* xorMaskData;        /* 23 */
		BYTE* andMaskData;        /* 24 */
		UINT32 paddingB[32 - 25]; /* 25 */
	};

	FREERDP_API rdpPointer* Pointer_Alloc(rdpContext* context);

	/* Glyph Class */
	typedef BOOL (*pGlyph_New)(rdpContext* context, rdpGlyph* glyph);
	typedef void (*pGlyph_Free)(rdpContext* context, rdpGlyph* glyph);
	typedef BOOL (*pGlyph_Draw)(rdpContext* context, const rdpGlyph* glyph, INT32 x, INT32 y,
	                            INT32 w, INT32 h, INT32 sx, INT32 sy, BOOL fOpRedundant);
	typedef BOOL (*pGlyph_BeginDraw)(rdpContext* context, INT32 x, INT32 y, INT32 width,
	                                 INT32 height, UINT32 bgcolor, UINT32 fgcolor,
	                                 BOOL fOpRedundant);
	typedef BOOL (*pGlyph_EndDraw)(rdpContext* context, INT32 x, INT32 y, INT32 width, INT32 height,
	                               UINT32 bgcolor, UINT32 fgcolor);
	typedef BOOL (*pGlyph_SetBounds)(rdpContext* context, INT32 x, INT32 y, INT32 width,
	                                 INT32 height);

	struct rdp_glyph
	{
		size_t size;                /* 0 */
		pGlyph_New New;             /* 1 */
		pGlyph_Free Free;           /* 2 */
		pGlyph_Draw Draw;           /* 3 */
		pGlyph_BeginDraw BeginDraw; /* 4 */
		pGlyph_EndDraw EndDraw;     /* 5 */
		pGlyph_SetBounds SetBounds; /* 6 */
		UINT32 paddingA[16 - 7];    /* 7 */

		INT32 x;                  /* 16 */
		INT32 y;                  /* 17 */
		UINT32 cx;                /* 18 */
		UINT32 cy;                /* 19 */
		UINT32 cb;                /* 20 */
		BYTE* aj;                 /* 21 */
		UINT32 paddingB[32 - 22]; /* 22 */
	};

	FREERDP_API rdpGlyph* Glyph_Alloc(rdpContext* context, INT32 x, INT32 y, UINT32 cx, UINT32 cy,
	                                  UINT32 cb, const BYTE* aj);

	/* Graphics Module */

	struct rdp_graphics
	{
		rdpContext* context;           /* 0 */
		rdpBitmap* Bitmap_Prototype;   /* 1 */
		rdpPointer* Pointer_Prototype; /* 2 */
		rdpGlyph* Glyph_Prototype;     /* 3 */
		UINT32 paddingA[16 - 4];       /* 4 */
	};

	FREERDP_API void graphics_register_bitmap(rdpGraphics* graphics, const rdpBitmap* bitmap);
	FREERDP_API void graphics_register_pointer(rdpGraphics* graphics, const rdpPointer* pointer);
	FREERDP_API void graphics_register_glyph(rdpGraphics* graphics, const rdpGlyph* glyph);

	FREERDP_API rdpGraphics* graphics_new(rdpContext* context);
	FREERDP_API void graphics_free(rdpGraphics* graphics);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_GRAPHICS_H */

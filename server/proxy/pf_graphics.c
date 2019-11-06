/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#include <winpr/crt.h>

#include <freerdp/codec/bitmap.h>

#include "pf_graphics.h"
#include "pf_log.h"
#include "pf_gdi.h"
#include "pf_context.h"

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/shape.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/graphics.h>
#include <freerdp/log.h>
#define TAG PROXY_TAG("graphics")

/* Bitmap Class */
static BOOL pf_Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	return TRUE;
}

static void pf_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
}

static BOOL pf_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap)
{
	return TRUE;
}

static BOOL pf_Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap, BOOL primary)
{
	return TRUE;
}

/* Pointer Class */
static BOOL pf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	return TRUE;
}

static void pf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
}

static BOOL pf_Pointer_Set(rdpContext* context, const rdpPointer* pointer)
{
	return TRUE;
}

static BOOL pf_Pointer_SetNull(rdpContext* context)
{
	return TRUE;
}

static BOOL pf_Pointer_SetDefault(rdpContext* context)
{
	return TRUE;
}

static BOOL pf_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	return TRUE;
}

/* Glyph Class */
static BOOL pf_Glyph_New(rdpContext* context, const rdpGlyph* glyph)
{
	return TRUE;
}

static void pf_Glyph_Free(rdpContext* context, rdpGlyph* glyph)
{
}

static BOOL pf_Glyph_Draw(rdpContext* context, const rdpGlyph* glyph, INT32 x, INT32 y, INT32 w,
                          INT32 h, INT32 sx, INT32 sy, BOOL fOpRedundant)
{
	return TRUE;
}

static BOOL pf_Glyph_BeginDraw(rdpContext* context, INT32 x, INT32 y, INT32 width, INT32 height,
                               UINT32 bgcolor, UINT32 fgcolor, BOOL fOpRedundant)
{
	return TRUE;
}

static BOOL pf_Glyph_EndDraw(rdpContext* context, INT32 x, INT32 y, INT32 width, INT32 height,
                             UINT32 bgcolor, UINT32 fgcolor)
{
	return TRUE;
}

/* Graphics Module */
BOOL pf_register_pointer(rdpGraphics* graphics)
{
	rdpPointer* pointer = NULL;

	if (!(pointer = (rdpPointer*)calloc(1, sizeof(rdpPointer))))
		return FALSE;

	pointer->size = sizeof(rdpPointer);
	pointer->New = pf_Pointer_New;
	pointer->Free = pf_Pointer_Free;
	pointer->Set = pf_Pointer_Set;
	pointer->SetNull = pf_Pointer_SetNull;
	pointer->SetDefault = pf_Pointer_SetDefault;
	pointer->SetPosition = pf_Pointer_SetPosition;
	graphics_register_pointer(graphics, pointer);
	free(pointer);
	return TRUE;
}

BOOL pf_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap bitmap;
	rdpGlyph glyph;

	if (!graphics || !graphics->Bitmap_Prototype || !graphics->Glyph_Prototype)
		return FALSE;

	bitmap = *graphics->Bitmap_Prototype;
	glyph = *graphics->Glyph_Prototype;
	bitmap.size = sizeof(rdpBitmap);
	bitmap.New = pf_Bitmap_New;
	bitmap.Free = pf_Bitmap_Free;
	bitmap.Paint = pf_Bitmap_Paint;
	bitmap.SetSurface = pf_Bitmap_SetSurface;
	graphics_register_bitmap(graphics, &bitmap);
	glyph.size = sizeof(rdpGlyph);
	glyph.New = pf_Glyph_New;
	glyph.Free = pf_Glyph_Free;
	glyph.Draw = pf_Glyph_Draw;
	glyph.BeginDraw = pf_Glyph_BeginDraw;
	glyph.EndDraw = pf_Glyph_EndDraw;
	graphics_register_glyph(graphics, &glyph);
	return TRUE;
}
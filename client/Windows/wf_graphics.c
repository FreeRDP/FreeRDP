/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Graphical Objects
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/codecs.h>
#include <freerdp/log.h>

#include "wf_gdi.h"
#include "wf_graphics.h"

#define TAG CLIENT_TAG("windows")

HBITMAP wf_create_dib(wfContext* wfc, UINT32 width, UINT32 height,
                      UINT32 srcFormat, const BYTE* data, BYTE** pdata)
{
	HDC hdc;
	int negHeight;
	HBITMAP bitmap;
	BITMAPINFO bmi;
	BYTE* cdata = NULL;
	UINT32 dstFormat = srcFormat;
	/**
	 * See: http://msdn.microsoft.com/en-us/library/dd183376
	 * if biHeight is positive, the bitmap is bottom-up
	 * if biHeight is negative, the bitmap is top-down
	 * Since we get top-down bitmaps, let's keep it that way
	 */
	negHeight = (height < 0) ? height : height * (-1);
	hdc = GetDC(NULL);
	bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = negHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = GetBitsPerPixel(dstFormat);
	bmi.bmiHeader.biCompression = BI_RGB;
	bitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**) &cdata, NULL, 0);

	if (data)
		freerdp_image_copy(cdata, dstFormat, 0, 0, 0, width, height, data, srcFormat, 0,
		                   0, 0, &wfc->context.gdi->palette, FREERDP_FLIP_NONE);

	if (pdata)
		*pdata = cdata;

	ReleaseDC(NULL, hdc);
	GdiFlush();
	return bitmap;
}

wfBitmap* wf_image_new(wfContext* wfc, UINT32 width, UINT32 height,
                       UINT32 format, const BYTE* data)
{
	HDC hdc;
	wfBitmap* image;
	hdc = GetDC(NULL);
	image = (wfBitmap*) malloc(sizeof(wfBitmap));
	image->hdc = CreateCompatibleDC(hdc);
	image->bitmap = wf_create_dib(wfc, width, height, format, data,
	                              &(image->pdata));
	image->org_bitmap = (HBITMAP) SelectObject(image->hdc, image->bitmap);
	ReleaseDC(NULL, hdc);
	return image;
}

void wf_image_free(wfBitmap* image)
{
	if (image != 0)
	{
		SelectObject(image->hdc, image->org_bitmap);
		DeleteObject(image->bitmap);
		DeleteDC(image->hdc);
		free(image);
	}
}

/* Bitmap Class */

static BOOL wf_Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	HDC hdc;
	wfContext* wfc = (wfContext*)context;
	wfBitmap* wf_bitmap = (wfBitmap*) bitmap;

	if (!context || !bitmap)
		return FALSE;

	wf_bitmap = (wfBitmap*) bitmap;
	hdc = GetDC(NULL);
	wf_bitmap->hdc = CreateCompatibleDC(hdc);

	if (!bitmap->data)
		wf_bitmap->bitmap = CreateCompatibleBitmap(hdc, bitmap->width, bitmap->height);
	else
		wf_bitmap->bitmap = wf_create_dib(wfc, bitmap->width, bitmap->height,
		                                  bitmap->format, bitmap->data, NULL);

	wf_bitmap->org_bitmap = (HBITMAP) SelectObject(wf_bitmap->hdc,
	                        wf_bitmap->bitmap);
	ReleaseDC(NULL, hdc);
	return TRUE;
}

static void wf_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	wfBitmap* wf_bitmap = (wfBitmap*) bitmap;

	if (wf_bitmap != 0)
	{
		SelectObject(wf_bitmap->hdc, wf_bitmap->org_bitmap);
		DeleteObject(wf_bitmap->bitmap);
		DeleteDC(wf_bitmap->hdc);

		_aligned_free(wf_bitmap->_bitmap.data);
		wf_bitmap->_bitmap.data = NULL;
	}
}

static BOOL wf_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap)
{
	BOOL rc;
	UINT32 width, height;
	wfContext* wfc = (wfContext*)context;
	wfBitmap* wf_bitmap = (wfBitmap*) bitmap;

	if (!context || !bitmap)
		return FALSE;

	width = bitmap->right - bitmap->left + 1;
	height = bitmap->bottom - bitmap->top + 1;
	rc = BitBlt(wfc->primary->hdc, bitmap->left, bitmap->top,
	            width, height, wf_bitmap->hdc, 0, 0, SRCCOPY);
	wf_invalidate_region(wfc, bitmap->left, bitmap->top, width, height);
	return rc;
}

static BOOL wf_Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap,
                                 BOOL primary)
{
	wfContext* wfc = (wfContext*)context;
	wfBitmap* bmp = (wfBitmap*) bitmap;
	rdpGdi* gdi = context->gdi;

	if (!gdi || !wfc)
		return FALSE;

	if (primary)
		wfc->drawing = wfc->primary;
	else if (!bmp)
		return FALSE;
	else
		wfc->drawing = bmp;

	return TRUE;
}

/* Pointer Class */

static BOOL flip_bitmap(const BYTE* src, BYTE* dst, UINT32 scanline,
                        UINT32 nHeight)
{
	UINT32 x;
	BYTE* bottomLine = dst + scanline * (nHeight - 1);

	for (x = 0; x < nHeight; x++)
	{
		memcpy(bottomLine, src, scanline);
		src += scanline;
		bottomLine -= scanline;
	}

	return TRUE;
}

static BOOL wf_Pointer_New(rdpContext* context, const rdpPointer* pointer)
{
	HCURSOR hCur;
	ICONINFO info;
	rdpGdi* gdi;
	BOOL rc = FALSE;

	if (!context || !pointer)
		return FALSE;

	gdi = context->gdi;

	if (!gdi)
		return FALSE;

	info.fIcon = FALSE;
	info.xHotspot = pointer->xPos;
	info.yHotspot = pointer->yPos;

	if (pointer->xorBpp == 1)
	{
		BYTE* pdata = (BYTE*) _aligned_malloc(pointer->lengthAndMask +
		                                      pointer->lengthXorMask, 16);

		if (!pdata)
			goto fail;

		CopyMemory(pdata, pointer->andMaskData, pointer->lengthAndMask);
		CopyMemory(pdata + pointer->lengthAndMask, pointer->xorMaskData,
		           pointer->lengthXorMask);
		info.hbmMask = CreateBitmap(pointer->width, pointer->height * 2, 1, 1, pdata);
		_aligned_free(pdata);
		info.hbmColor = NULL;
	}
	else
	{
		BYTE* pdata = (BYTE*) _aligned_malloc(pointer->lengthAndMask, 16);

		if (!pdata)
			goto fail;

		flip_bitmap(pointer->andMaskData, pdata, (pointer->width + 7) / 8,
		            pointer->height);
		info.hbmMask = CreateBitmap(pointer->width, pointer->height, 1, 1, pdata);
		_aligned_free(pdata);
		pdata = (BYTE*) _aligned_malloc(pointer->width * pointer->height *
		                                GetBitsPerPixel(gdi->dstFormat), 16);

		if (!pdata)
			goto fail;

		if (!freerdp_image_copy_from_pointer_data(pdata, gdi->dstFormat, 0, 0, 0,
		        pointer->width, pointer->height,
		        pointer->xorMaskData, pointer->lengthXorMask,
		        pointer->andMaskData, pointer->lengthAndMask, pointer->xorBpp, &gdi->palette))
		{
			_aligned_free(pdata);
			goto fail;
		}

		info.hbmColor = CreateBitmap(pointer->width, pointer->height, 1,
		                             GetBitsPerPixel(gdi->dstFormat), pdata);
		_aligned_free(pdata);
	}

	hCur = CreateIconIndirect(&info);
	((wfPointer*) pointer)->cursor = hCur;
	rc = TRUE;
fail:

	if (info.hbmMask)
		DeleteObject(info.hbmMask);

	if (info.hbmColor)
		DeleteObject(info.hbmColor);

	return rc;
}

static BOOL wf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	HCURSOR hCur;

	if (!context || !pointer)
		return FALSE;

	hCur = ((wfPointer*) pointer)->cursor;

	if (hCur != 0)
		DestroyIcon(hCur);

	return TRUE;
}

static BOOL wf_Pointer_Set(rdpContext* context, const rdpPointer* pointer)
{
	HCURSOR hCur;
	wfContext* wfc = (wfContext*)context;

	if (!context || !pointer)
		return FALSE;

	hCur = ((wfPointer*) pointer)->cursor;

	if (hCur != NULL)
	{
		SetCursor(hCur);
		wfc->cursor = hCur;
	}

	return TRUE;
}

static BOOL wf_Pointer_SetNull(rdpContext* context)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL wf_Pointer_SetDefault(rdpContext* context)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL wf_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	if (!context)
		return FALSE;

	return TRUE;
}

BOOL wf_register_pointer(rdpGraphics* graphics)
{
	wfContext* wfc;
	rdpPointer pointer;

	if (!graphics)
		return FALSE;

	wfc = (wfContext*) graphics->context;
	ZeroMemory(&pointer, sizeof(rdpPointer));
	pointer.size = sizeof(wfPointer);
	pointer.New = wf_Pointer_New;
	pointer.Free = wf_Pointer_Free;
	pointer.Set = wf_Pointer_Set;
	pointer.SetNull = wf_Pointer_SetNull;
	pointer.SetDefault = wf_Pointer_SetDefault;
	pointer.SetPosition = wf_Pointer_SetPosition;
	graphics_register_pointer(graphics, &pointer);
	return TRUE;
}

/* Graphics Module */

BOOL wf_register_graphics(rdpGraphics* graphics)
{
	wfContext* wfc;
	rdpGlyph glyph;
	rdpBitmap bitmap;

	if (!graphics)
		return FALSE;

	wfc = (wfContext*) graphics->context;
	bitmap = *graphics->Bitmap_Prototype;
	bitmap.size = sizeof(wfBitmap);
	bitmap.New = wf_Bitmap_New;
	bitmap.Free = wf_Bitmap_Free;
	bitmap.Paint = wf_Bitmap_Paint;
	bitmap.SetSurface = wf_Bitmap_SetSurface;
	graphics_register_bitmap(graphics, &bitmap);
	glyph = *graphics->Glyph_Prototype;
	graphics_register_glyph(graphics, &glyph);
	return TRUE;
}

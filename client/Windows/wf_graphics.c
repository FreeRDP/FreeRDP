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

HBITMAP wf_create_dib(wfContext* wfc, int width, int height, int bpp, BYTE* data, BYTE** pdata)
{
	HDC hdc;
	int negHeight;
	HBITMAP bitmap;
	BITMAPINFO bmi;
	BYTE* cdata = NULL;

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
	bmi.bmiHeader.biBitCount = wfc->dstBpp;
	bmi.bmiHeader.biCompression = BI_RGB;

	bitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**) &cdata, NULL, 0);

	if (data)
		freerdp_image_convert(data, cdata, width, height, bpp, wfc->dstBpp, wfc->clrconv);

	if (pdata)
		*pdata = cdata;

	ReleaseDC(NULL, hdc);
	GdiFlush();

	return bitmap;
}

wfBitmap* wf_image_new(wfContext* wfc, int width, int height, int bpp, BYTE* data)
{
	HDC hdc;
	wfBitmap* image;

	hdc = GetDC(NULL);
	image = (wfBitmap*) malloc(sizeof(wfBitmap));
	image->hdc = CreateCompatibleDC(hdc);

	image->bitmap = wf_create_dib(wfc, width, height, bpp, data, &(image->pdata));

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

void wf_Bitmap_New(wfContext* wfc, rdpBitmap* bitmap)
{
	HDC hdc;
	wfBitmap* wf_bitmap = (wfBitmap*) bitmap;

	wf_bitmap = (wfBitmap*) bitmap;

	hdc = GetDC(NULL);
	wf_bitmap->hdc = CreateCompatibleDC(hdc);

	if (!bitmap->data)
		wf_bitmap->bitmap = CreateCompatibleBitmap(hdc, bitmap->width, bitmap->height);
	else
		wf_bitmap->bitmap = wf_create_dib(wfc, bitmap->width, bitmap->height, bitmap->bpp, bitmap->data, NULL);

	wf_bitmap->org_bitmap = (HBITMAP) SelectObject(wf_bitmap->hdc, wf_bitmap->bitmap);
	ReleaseDC(NULL, hdc);
}

void wf_Bitmap_Free(wfContext* wfc, rdpBitmap* bitmap)
{
	wfBitmap* wf_bitmap = (wfBitmap*) bitmap;

	if (wf_bitmap != 0)
	{
		SelectObject(wf_bitmap->hdc, wf_bitmap->org_bitmap);
		DeleteObject(wf_bitmap->bitmap);
		DeleteDC(wf_bitmap->hdc);
	}
}

void wf_Bitmap_Paint(wfContext* wfc, rdpBitmap* bitmap)
{
	int width, height;
	wfBitmap* wf_bitmap = (wfBitmap*) bitmap;

	width = bitmap->right - bitmap->left + 1;
	height = bitmap->bottom - bitmap->top + 1;

	BitBlt(wfc->primary->hdc, bitmap->left, bitmap->top,
		width, height, wf_bitmap->hdc, 0, 0, SRCCOPY);

	wf_invalidate_region(wfc, bitmap->left, bitmap->top, width, height);
}

void wf_Bitmap_Decompress(wfContext* wfc, rdpBitmap* bitmap,
		BYTE* data, int width, int height, int bpp, int length, BOOL compressed, int codecId)
{
	int status;
	UINT16 size;
	BYTE* pSrcData;
	BYTE* pDstData;
	UINT32 SrcSize;
	UINT32 SrcFormat;
	UINT32 bytesPerPixel;

	bytesPerPixel = (bpp + 7) / 8;
	size = width * height * 4;

	if (!bitmap->data)
		bitmap->data = (BYTE*) _aligned_malloc(size, 16);
	else
		bitmap->data = (BYTE*) _aligned_realloc(bitmap->data, size, 16);

	pSrcData = data;
	SrcSize = (UINT32) length;
	pDstData = bitmap->data;

	if (compressed)
	{
		if (bpp < 32)
		{
			if (!freerdp_client_codecs_prepare(wfc->codecs, FREERDP_CODEC_INTERLEAVED,
											   wfc->instance->settings->DesktopWidth,
											   wfc->instance->settings->DesktopHeight))
				return;

			status = interleaved_decompress(wfc->codecs->interleaved, pSrcData, SrcSize, bpp,
					&pDstData, PIXEL_FORMAT_XRGB32, width * 4, 0, 0, width, height, NULL);
		}
		else
		{
			if (!freerdp_client_codecs_prepare(wfc->codecs, FREERDP_CODEC_PLANAR,
											   wfc->instance->settings->DesktopWidth,
											   wfc->instance->settings->DesktopHeight))
				return;

			status = planar_decompress(wfc->codecs->planar, pSrcData, SrcSize, &pDstData,
					PIXEL_FORMAT_XRGB32, width * 4, 0, 0, width, height, TRUE);
		}

		if (status < 0)
		{
			WLog_ERR(TAG, "Bitmap Decompression Failed");
			return;
		}
	}
	else
	{
		SrcFormat = gdi_get_pixel_format(bpp, TRUE);

		status = freerdp_image_copy(pDstData, PIXEL_FORMAT_XRGB32, width * 4, 0, 0,
				width, height, pSrcData, SrcFormat, width * bytesPerPixel, 0, 0, NULL);
	}

	bitmap->compressed = FALSE;
	bitmap->length = size;
	bitmap->bpp = 32;
}

void wf_Bitmap_SetSurface(wfContext* wfc, rdpBitmap* bitmap, BOOL primary)
{
	if (primary)
		wfc->drawing = wfc->primary;
	else
		wfc->drawing = (wfBitmap*) bitmap;
}

/* Pointer Class */

void wf_Pointer_New(wfContext* wfc, rdpPointer* pointer)
{
	HCURSOR hCur;
	ICONINFO info;
	BYTE *data;

	info.fIcon = FALSE;
	info.xHotspot = pointer->xPos;
	info.yHotspot = pointer->yPos;
	if (pointer->xorBpp == 1)
	{
		data = (BYTE*) malloc(pointer->lengthAndMask + pointer->lengthXorMask);
		CopyMemory(data, pointer->andMaskData, pointer->lengthAndMask);
		CopyMemory(data + pointer->lengthAndMask, pointer->xorMaskData, pointer->lengthXorMask);
		info.hbmMask = CreateBitmap(pointer->width, pointer->height * 2, 1, 1, data);
		free(data);
		info.hbmColor = NULL;
	}
	else
	{
		data = (BYTE*) malloc(pointer->lengthAndMask);
		freerdp_bitmap_flip(pointer->andMaskData, data, (pointer->width + 7) / 8, pointer->height);
		info.hbmMask = CreateBitmap(pointer->width, pointer->height, 1, 1, data);
		free(data);
		data = (BYTE*) malloc(pointer->lengthXorMask);
		freerdp_image_flip(pointer->xorMaskData, data, pointer->width, pointer->height, pointer->xorBpp);
		info.hbmColor = CreateBitmap(pointer->width, pointer->height, 1, pointer->xorBpp, data);
		free(data);
	}
	hCur = CreateIconIndirect(&info);
	((wfPointer*) pointer)->cursor = hCur;
	if (info.hbmMask)
		DeleteObject(info.hbmMask);
	if (info.hbmColor)
		DeleteObject(info.hbmColor);
}

void wf_Pointer_Free(wfContext* wfc, rdpPointer* pointer)
{
	HCURSOR hCur;

	hCur = ((wfPointer*) pointer)->cursor;

	if (hCur != 0)
		DestroyIcon(hCur);
}

void wf_Pointer_Set(wfContext* wfc, rdpPointer* pointer)
{
	HCURSOR hCur;

	hCur = ((wfPointer*) pointer)->cursor;

	if (hCur != NULL)
	{
		SetCursor(hCur);
		wfc->cursor = hCur;
	}
}

void wf_Pointer_SetNull(wfContext* wfc)
{

}

void wf_Pointer_SetDefault(wfContext* wfc)
{

}

void wf_register_pointer(rdpGraphics* graphics)
{
	wfContext* wfc;
	rdpPointer pointer;

	wfc = (wfContext*) graphics->context;

	ZeroMemory(&pointer, sizeof(rdpPointer));
	pointer.size = sizeof(wfPointer);
	pointer.New = (pPointer_New) wf_Pointer_New;
	pointer.Free = (pPointer_Free) wf_Pointer_Free;
	pointer.Set = (pPointer_Set) wf_Pointer_Set;
	pointer.SetNull = (pPointer_SetNull) wf_Pointer_SetNull;
	pointer.SetDefault = (pPointer_SetDefault) wf_Pointer_SetDefault;

	graphics_register_pointer(graphics, &pointer);
}

/* Graphics Module */

void wf_register_graphics(rdpGraphics* graphics)
{
	wfContext* wfc;
	rdpBitmap bitmap;

	wfc = (wfContext*) graphics->context;

	ZeroMemory(&bitmap, sizeof(rdpBitmap));
	bitmap.size = sizeof(wfBitmap);
	bitmap.New = (pBitmap_New) wf_Bitmap_New;
	bitmap.Free = (pBitmap_Free) wf_Bitmap_Free;
	bitmap.Paint = (pBitmap_Paint) wf_Bitmap_Paint;
	bitmap.Decompress = (pBitmap_Decompress) wf_Bitmap_Decompress;
	bitmap.SetSurface = (pBitmap_SetSurface) wf_Bitmap_SetSurface;

	graphics_register_bitmap(graphics, &bitmap);
}

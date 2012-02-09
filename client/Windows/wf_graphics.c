/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <freerdp/utils/memory.h>
#include <freerdp/codec/bitmap.h>

#include "wf_gdi.h"
#include "wf_graphics.h"

HBITMAP wf_create_dib(wfInfo* wfi, int width, int height, int bpp, uint8* data, uint8** pdata)
{
	HDC hdc;
	int negHeight;
	HBITMAP bitmap;
	BITMAPINFO bmi;
	uint8* cdata = NULL;

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
	bmi.bmiHeader.biBitCount = bpp;
	bmi.bmiHeader.biCompression = BI_RGB;
	bitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**) &cdata, NULL, 0);

	if (data != NULL)
		freerdp_image_convert(data, cdata, width, height, bpp, bpp, wfi->clrconv);

	if (pdata != NULL)
		*pdata = cdata;

	ReleaseDC(NULL, hdc);
	GdiFlush();

	return bitmap;
}

wfBitmap* wf_image_new(wfInfo* wfi, int width, int height, int bpp, uint8* data)
{
	HDC hdc;
	wfBitmap* image;

	hdc = GetDC(NULL);
	image = (wfBitmap*) malloc(sizeof(wfBitmap));
	image->hdc = CreateCompatibleDC(hdc);

	if (data == NULL)
		image->bitmap = CreateCompatibleBitmap(hdc, width, height);
	else
		image->bitmap = wf_create_dib(wfi, width, height, bpp, data, &(image->pdata));

	image->org_bitmap = (HBITMAP) SelectObject(image->hdc, image->bitmap);
	ReleaseDC(NULL, hdc);
	
	return image;
}

wfBitmap* wf_bitmap_new(wfInfo* wfi, int width, int height, int bpp, uint8* data)
{
	HDC hdc;
	wfBitmap* bitmap;

	hdc = GetDC(NULL);
	bitmap = (wfBitmap*) malloc(sizeof(wfBitmap));
	bitmap->hdc = CreateCompatibleDC(hdc);
	bitmap->bitmap = wf_create_dib(wfi, width, height, bpp, data, &(bitmap->pdata));
	bitmap->org_bitmap = (HBITMAP) SelectObject(bitmap->hdc, bitmap->bitmap);
	ReleaseDC(NULL, hdc);
	
	return bitmap;
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

void wf_Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	HDC hdc;
	wfBitmap* wf_bitmap = (wfBitmap*) bitmap;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	wf_bitmap = (wfBitmap*) bitmap;

	hdc = GetDC(NULL);
	wf_bitmap->hdc = CreateCompatibleDC(hdc);

	if (bitmap->data == NULL)
		wf_bitmap->bitmap = CreateCompatibleBitmap(hdc, bitmap->width, bitmap->height);
	else
		wf_bitmap->bitmap = wf_create_dib(wfi, bitmap->width, bitmap->height, bitmap->bpp, bitmap->data, NULL);

	wf_bitmap->org_bitmap = (HBITMAP) SelectObject(wf_bitmap->hdc, wf_bitmap->bitmap);
	ReleaseDC(NULL, hdc);
}

void wf_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	wfBitmap* wf_bitmap = (wfBitmap*) bitmap;
	
	if (wf_bitmap != 0)
	{
		SelectObject(wf_bitmap->hdc, wf_bitmap->org_bitmap);
		DeleteObject(wf_bitmap->bitmap);
		DeleteDC(wf_bitmap->hdc);
	}
}

void wf_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap)
{
	int width, height;
	wfBitmap* wf_bitmap = (wfBitmap*) bitmap;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	width = bitmap->right - bitmap->left + 1;
	height = bitmap->bottom - bitmap->top + 1;

	BitBlt(wfi->primary->hdc, bitmap->left, bitmap->top,
		width, height, wf_bitmap->hdc, 0, 0, SRCCOPY);

	wf_invalidate_region(wfi, bitmap->left, bitmap->top, width, height);
}

void wf_Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed)
{
	uint16 size;

	size = width * height * (bpp / 8);

	if (bitmap->data == NULL)
		bitmap->data = (uint8*) xmalloc(size);
	else
		bitmap->data = (uint8*) xrealloc(bitmap->data, size);

	if (compressed)
	{
		boolean status;

		status = bitmap_decompress(data, bitmap->data, width, height, length, bpp, bpp);

		if (status != true)
		{
			printf("Bitmap Decompression Failed\n");
		}
	}
	else
	{
		freerdp_image_flip(data, bitmap->data, width, height, bpp);
	}

	bitmap->compressed = false;
	bitmap->length = size;
	bitmap->bpp = bpp;
}

void wf_Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap, boolean primary)
{
	wfInfo* wfi = ((wfContext*) context)->wfi;

	if (primary)
		wfi->drawing = wfi->primary;
	else
		wfi->drawing = (wfBitmap*) bitmap;
}

/* Pointer Class */

void wf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{

}

void wf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{

}

void wf_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{

}

/* Graphics Module */

void wf_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap bitmap;
	rdpPointer pointer;

	memset(&bitmap, 0, sizeof(rdpBitmap));
	bitmap.size = sizeof(wfBitmap);
	bitmap.New = wf_Bitmap_New;
	bitmap.Free = wf_Bitmap_Free;
	bitmap.Paint = wf_Bitmap_Paint;
	bitmap.Decompress = wf_Bitmap_Decompress;
	bitmap.SetSurface = wf_Bitmap_SetSurface;

	memset(&pointer, 0, sizeof(rdpPointer));
	pointer.size = sizeof(wfPointer);
	pointer.New = wf_Pointer_New;
	pointer.Free = wf_Pointer_Free;
	pointer.Set = wf_Pointer_Set;

	graphics_register_bitmap(graphics, &bitmap);
	graphics_register_pointer(graphics, &pointer);
}

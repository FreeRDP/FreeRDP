/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Windows GDI
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/common/color.h>

#include "wfreerdp.h"

HBITMAP wf_create_dib(wfInfo* wfi, int width, int height, int bpp, uint8* data)
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
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;
	bitmap = CreateDIBSection (hdc, &bmi, DIB_RGB_COLORS, (void**) &cdata, NULL, 0);

	if (data != NULL)
		freerdp_image_convert(data, cdata, width, height, bpp, 24, wfi->clrconv);

	ReleaseDC(NULL, hdc);
	GdiFlush();

	return bitmap;
}

WF_IMAGE* wf_image_new(wfInfo* wfi, int width, int height, int bpp, uint8* data)
{
	HDC hdc;
	WF_IMAGE* image;

	hdc = GetDC(NULL);
	image = (WF_IMAGE*) malloc(sizeof(WF_IMAGE));
	image->hdc = CreateCompatibleDC(hdc);

	if (data == NULL)
		image->bitmap = CreateCompatibleBitmap(hdc, width, height);
	else
		image->bitmap = wf_create_dib(wfi, width, height, bpp, data);

	image->org_bitmap = (HBITMAP) SelectObject(image->hdc, image->bitmap);
	ReleaseDC(NULL, hdc);
	
	return image;
}

void wf_image_free(WF_IMAGE* image)
{
	if (image != 0)
	{
		SelectObject(image->hdc, image->org_bitmap);
		DeleteObject(image->bitmap);
		DeleteDC(image->hdc);
		free(image);
	}
}

void wf_toggle_fullscreen(wfInfo* wfi)
{
	ShowWindow(wfi->hwnd, SW_HIDE);
	wfi->fullscreen = !wfi->fullscreen;
	SetForegroundWindow(wfi->hwnd);
}

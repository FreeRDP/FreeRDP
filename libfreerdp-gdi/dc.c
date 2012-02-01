/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Device Context Functions
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

/* Device Context Functions: http://msdn.microsoft.com/en-us/library/dd183554 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/color.h>

#include <freerdp/gdi/region.h>

#include <freerdp/gdi/dc.h>

/**
 * Get the current device context (a new one is created each time).\n
 * @msdn{dd144871}
 * @return current device context
 */

HGDI_DC gdi_GetDC()
{
	HGDI_DC hDC = (HGDI_DC) malloc(sizeof(GDI_DC));
	hDC->bytesPerPixel = 4;
	hDC->bitsPerPixel = 32;
	hDC->drawMode = GDI_R2_BLACK;
	hDC->clip = gdi_CreateRectRgn(0, 0, 0, 0);
	hDC->clip->null = 1;
	hDC->hwnd = NULL;
	return hDC;
}

/**
 * Create a device context.\n
 * @msdn{dd144871}
 * @return new device context
 */

HGDI_DC gdi_CreateDC(HCLRCONV clrconv, int bpp)
{
	HGDI_DC hDC = (HGDI_DC) malloc(sizeof(GDI_DC));

	hDC->drawMode = GDI_R2_BLACK;
	hDC->clip = gdi_CreateRectRgn(0, 0, 0, 0);
	hDC->clip->null = 1;
	hDC->hwnd = NULL;

	hDC->bitsPerPixel = bpp;
	hDC->bytesPerPixel = bpp / 8;

	hDC->alpha = clrconv->alpha;
	hDC->invert = clrconv->invert;
	hDC->rgb555 = clrconv->rgb555;

	hDC->hwnd = (HGDI_WND) malloc(sizeof(GDI_WND));
	hDC->hwnd->invalid = gdi_CreateRectRgn(0, 0, 0, 0);
	hDC->hwnd->invalid->null = 1;

	hDC->hwnd->count = 32;
	hDC->hwnd->cinvalid = (HGDI_RGN) malloc(sizeof(GDI_RGN) * hDC->hwnd->count);
	hDC->hwnd->ninvalid = 0;

	return hDC;
}

/**
 * Create a new device context compatible with the given device context.\n
 * @msdn{dd183489}
 * @param hdc device context
 * @return new compatible device context
 */

HGDI_DC gdi_CreateCompatibleDC(HGDI_DC hdc)
{
	HGDI_DC hDC = (HGDI_DC) malloc(sizeof(GDI_DC));
	hDC->bytesPerPixel = hdc->bytesPerPixel;
	hDC->bitsPerPixel = hdc->bitsPerPixel;
	hDC->drawMode = hdc->drawMode;
	hDC->clip = gdi_CreateRectRgn(0, 0, 0, 0);
	hDC->clip->null = 1;
	hDC->hwnd = NULL;
	hDC->alpha = hdc->alpha;
	hDC->invert = hdc->invert;
	hDC->rgb555 = hdc->rgb555;
	return hDC;
}

/**
 * Select a GDI object in the current device context.\n
 * @msdn{dd162957}
 * @param hdc device context
 * @param hgdiobject new selected GDI object
 * @return previous selected GDI object
 */

HGDIOBJECT gdi_SelectObject(HGDI_DC hdc, HGDIOBJECT hgdiobject)
{
	HGDIOBJECT previousSelectedObject = hdc->selectedObject;

	if (hgdiobject == NULL)
		return NULL;

	if (hgdiobject->objectType == GDIOBJECT_BITMAP)
	{
		hdc->selectedObject = hgdiobject;
	}
	else if (hgdiobject->objectType == GDIOBJECT_PEN)
	{
		previousSelectedObject = (HGDIOBJECT) hdc->pen;
		hdc->pen = (HGDI_PEN) hgdiobject;
	}
	else if (hgdiobject->objectType == GDIOBJECT_BRUSH)
	{
		previousSelectedObject = (HGDIOBJECT) hdc->brush;
		hdc->brush = (HGDI_BRUSH) hgdiobject;
	}
	else if (hgdiobject->objectType == GDIOBJECT_REGION)
	{
		hdc->selectedObject = hgdiobject;
	}
	else if (hgdiobject->objectType == GDIOBJECT_RECT)
	{
		hdc->selectedObject = hgdiobject;
	}
	else
	{
		/* Unknown GDI Object Type */
		return 0;
	}

	return previousSelectedObject;
}

/**
 * Delete a GDI object.\n
 * @msdn{dd183539}
 * @param hgdiobject GDI object
 * @return 1 if successful, 0 otherwise
 */

int gdi_DeleteObject(HGDIOBJECT hgdiobject)
{
	if (hgdiobject == NULL)
		return 0;

	if (hgdiobject->objectType == GDIOBJECT_BITMAP)
	{
		HGDI_BITMAP hBitmap = (HGDI_BITMAP) hgdiobject;

		if (hBitmap->data != NULL)
			free(hBitmap->data);

		free(hBitmap);
	}
	else if (hgdiobject->objectType == GDIOBJECT_PEN)
	{
		HGDI_PEN hPen = (HGDI_PEN) hgdiobject;
		free(hPen);
	}
	else if (hgdiobject->objectType == GDIOBJECT_BRUSH)
	{
		HGDI_BRUSH hBrush = (HGDI_BRUSH) hgdiobject;

		if(hBrush->style == GDI_BS_PATTERN)
		{
			if (hBrush->pattern != NULL)
				gdi_DeleteObject((HGDIOBJECT) hBrush->pattern);
		}

		free(hBrush);
	}
	else if (hgdiobject->objectType == GDIOBJECT_REGION)
	{
		free(hgdiobject);
	}
	else if (hgdiobject->objectType == GDIOBJECT_RECT)
	{
		free(hgdiobject);
	}
	else
	{
		/* Unknown GDI Object Type */
		free(hgdiobject);
		return 0;
	}

	return 1;
}

/**
 * Delete device context.\n
 * @msdn{dd183533}
 * @param hdc device context
 * @return 1 if successful, 0 otherwise
 */

int gdi_DeleteDC(HGDI_DC hdc)
{
	if (hdc->hwnd)
	{
		if (hdc->hwnd->cinvalid != NULL)
			free(hdc->hwnd->cinvalid);

		if (hdc->hwnd->invalid != NULL)
			free(hdc->hwnd->invalid);

		free(hdc->hwnd);
	}

	free(hdc->clip);
	free(hdc);

	return 1;
}

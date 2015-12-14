/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

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
	if (!hDC)
		return NULL;

	hDC->bytesPerPixel = 4;
	hDC->bitsPerPixel = 32;
	hDC->drawMode = GDI_R2_BLACK;
	hDC->clip = gdi_CreateRectRgn(0, 0, 0, 0);
	if (!hDC->clip)
	{
		free(hDC);
		return NULL;
	}
	hDC->clip->null = 1;
	hDC->hwnd = NULL;
	return hDC;
}

/**
 * Create a device context.\n
 * @msdn{dd144871}
 * @return new device context
 */

HGDI_DC gdi_CreateDC(UINT32 flags, int bpp)
{
	HGDI_DC hDC;

	if (!(hDC = (HGDI_DC) calloc(1, sizeof(GDI_DC))))
		return NULL;

	hDC->drawMode = GDI_R2_BLACK;

	if (!(hDC->clip = gdi_CreateRectRgn(0, 0, 0, 0)))
		goto fail;

	hDC->clip->null = 1;
	hDC->hwnd = NULL;

	hDC->bitsPerPixel = bpp;
	hDC->bytesPerPixel = bpp / 8;

	hDC->alpha = (flags & CLRCONV_ALPHA) ? TRUE : FALSE;
	hDC->invert = (flags & CLRCONV_INVERT) ? TRUE : FALSE;
	hDC->rgb555 = (flags & CLRCONV_RGB555) ? TRUE : FALSE;

	if (!(hDC->hwnd = (HGDI_WND) calloc(1, sizeof(GDI_WND))))
		goto fail;

	if (!(hDC->hwnd->invalid = gdi_CreateRectRgn(0, 0, 0, 0)))
		goto fail;

	hDC->hwnd->invalid->null = 1;

	hDC->hwnd->count = 32;
	if (!(hDC->hwnd->cinvalid = (HGDI_RGN) calloc(hDC->hwnd->count, sizeof(GDI_RGN))))
		goto fail;

	hDC->hwnd->ninvalid = 0;

	return hDC;

fail:
	gdi_DeleteDC(hDC);
	return NULL;
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
	if (!hDC)
		return NULL;

	if (!(hDC->clip = gdi_CreateRectRgn(0, 0, 0, 0)))
	{
		free(hDC);
		return NULL;
	}
	hDC->clip->null = 1;
	hDC->bytesPerPixel = hdc->bytesPerPixel;
	hDC->bitsPerPixel = hdc->bitsPerPixel;
	hDC->drawMode = hdc->drawMode;
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
		previousSelectedObject = (HGDIOBJECT) COMPLEXREGION;
	}
	else if (hgdiobject->objectType == GDIOBJECT_RECT)
	{
		hdc->selectedObject = hgdiobject;
		previousSelectedObject = (HGDIOBJECT) SIMPLEREGION;
	}
	else
	{
		/* Unknown GDI Object Type */
		return NULL;
	}

	return previousSelectedObject;
}

/**
 * Delete a GDI object.\n
 * @msdn{dd183539}
 * @param hgdiobject GDI object
 * @return nonzero if successful, 0 otherwise
 */

BOOL gdi_DeleteObject(HGDIOBJECT hgdiobject)
{
	if (!hgdiobject)
		return FALSE;

	if (hgdiobject->objectType == GDIOBJECT_BITMAP)
	{
		HGDI_BITMAP hBitmap = (HGDI_BITMAP) hgdiobject;

		if (hBitmap->data && hBitmap->free)
		{
			hBitmap->free(hBitmap->data);
			hBitmap->data = NULL;
		}

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

		if (hBrush->style == GDI_BS_PATTERN || hBrush->style == GDI_BS_HATCHED)
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
		return FALSE;
	}

	return TRUE;
}

/**
 * Delete device context.\n
 * @msdn{dd183533}
 * @param hdc device context
 * @return nonzero if successful, 0 otherwise
 */

BOOL gdi_DeleteDC(HGDI_DC hdc)
{
	if (hdc)
	{
		if (hdc->hwnd)
		{
			free(hdc->hwnd->cinvalid);
			free(hdc->hwnd->invalid);
			free(hdc->hwnd);
		}
		free(hdc->clip);
		free(hdc);
	}

	return TRUE;
}

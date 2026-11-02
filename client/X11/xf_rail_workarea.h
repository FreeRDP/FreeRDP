/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#ifndef FREERDP_CLIENT_X11_RAIL_WORKAREA_H
#define FREERDP_CLIENT_X11_RAIL_WORKAREA_H

#include <freerdp/types.h>

/* Intersect X11 root coordinates before serializing unsigned 16-bit RAIL edges. */
static inline BOOL xf_rail_workarea_clip(INT32 x, INT32 y, UINT32 width, UINT32 height,
                                         UINT32 desktopWidth, UINT32 desktopHeight,
                                         RECTANGLE_16* area)
{
	if (!area || !width || !height || !desktopWidth || !desktopHeight ||
	    desktopWidth > UINT16_MAX || desktopHeight > UINT16_MAX)
		return FALSE;

	const INT64 left = (x > 0) ? x : 0;
	const INT64 top = (y > 0) ? y : 0;
	const INT64 rawRight = (INT64)x + width;
	const INT64 rawBottom = (INT64)y + height;
	const INT64 right = (rawRight < desktopWidth) ? rawRight : desktopWidth;
	const INT64 bottom = (rawBottom < desktopHeight) ? rawBottom : desktopHeight;
	if (right <= left || bottom <= top)
		return FALSE;

	area->left = (UINT16)left;
	area->top = (UINT16)top;
	area->right = (UINT16)right;
	area->bottom = (UINT16)bottom;
	return TRUE;
}

#endif /* FREERDP_CLIENT_X11_RAIL_WORKAREA_H */

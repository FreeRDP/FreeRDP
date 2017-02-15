/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Drawing Functions
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

/* GDI Drawing Functions: http://msdn.microsoft.com/en-us/library/dd162760/ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include "drawing.h"

/**
 * Set current foreground draw mode.\n
 * @msdn{dd144922}
 * @param hdc device context
 * @return draw mode
 */

UINT32 gdi_GetROP2(HGDI_DC hdc)
{
	return hdc->drawMode;
}

/**
 * Set current foreground draw mode.\n
 * @msdn{dd145088}
 * @param hdc device context
 * @param fnDrawMode draw mode
 * @return previous draw mode
 */

UINT32 gdi_SetROP2(HGDI_DC hdc, int fnDrawMode)
{
	UINT32 prevDrawMode = hdc->drawMode;

	if (fnDrawMode > 0 && fnDrawMode <= 16)
		hdc->drawMode = fnDrawMode;

	return prevDrawMode;
}

/**
 * Get the current background color.\n
 * @msdn{dd144852}
 * @param hdc device context
 * @return background color
 */

UINT32 gdi_GetBkColor(HGDI_DC hdc)
{
	return hdc->bkColor;
}

/**
 * Set the current background color.\n
 * @msdn{dd162964}
 * @param hdc device color
 * @param crColor new background color
 * @return previous background color
 */

UINT32 gdi_SetBkColor(HGDI_DC hdc, UINT32 crColor)
{
	UINT32 previousBkColor = hdc->bkColor;
	hdc->bkColor = crColor;
	return previousBkColor;
}

/**
 * Get the current background mode.\n
 * @msdn{dd144853}
 * @param hdc device context
 * @return background mode
 */

UINT32 gdi_GetBkMode(HGDI_DC hdc)
{
	return hdc->bkMode;
}

/**
 * Set the current background mode.\n
 * @msdn{dd162965}
 * @param hdc device context
 * @param iBkMode background mode
 * @return previous background mode on success, 0 on failure
 */


BOOL gdi_SetBkMode(HGDI_DC hdc, int iBkMode)
{
	if (iBkMode == GDI_OPAQUE || iBkMode == GDI_TRANSPARENT)
	{
		int previousBkMode = hdc->bkMode;
		hdc->bkMode = iBkMode;
		return previousBkMode;
	}

	return TRUE;
}

/**
 * Set the current text color.\n
 * @msdn{dd145093}
 * @param hdc device context
 * @param crColor new text color
 * @return previous text color
 */

UINT32 gdi_SetTextColor(HGDI_DC hdc, UINT32 crColor)
{
	UINT32 previousTextColor = hdc->textColor;
	hdc->textColor = crColor;
	return previousTextColor;
}

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Brush Functions
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

/* GDI Brush Functions: http://msdn.microsoft.com/en-us/library/dd183395/ */

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/region.h>
#include <freerdp/log.h>

#include "brush.h"
#include "clipping.h"

#define TAG FREERDP_TAG("gdi.brush")

const char* gdi_rop_to_string(UINT32 code)
{
	switch (code)
	{
		case GDI_BLACKNESS:
			return "0";

		case GDI_DPSoon:
			return "DPSoon";

		case GDI_DPSona:
			return "DPSona";

		case GDI_PSon:
			return "PSon";

		case GDI_SDPona:
			return "SDPona";

		case GDI_DPon:
			return "DPon";

		case GDI_PDSxnon:
			return "PDSxnon";

		case GDI_PDSaon:
			return "PDSaon";

		case GDI_SDPnaa:
			return "SDPnaa";

		case GDI_PDSxon:
			return "PDSxon";

		case GDI_DPna:
			return "DPna";

		case GDI_PSDnaon:
			return "PSDnaon";

		case GDI_SPna:
			return "SPna";

		case GDI_PDSnaon:
			return "PDSnaon";

		case GDI_PDSonon:
			return "PDSonon";

		case GDI_Pn:
			return "Pn";

		case GDI_PDSona:
			return "PDSona";

		case GDI_NOTSRCERASE:
			return "DSon";

		case GDI_SDPxnon:
			return "SDPxnon";

		case GDI_SDPaon:
			return "SDPaon";

		case GDI_DPSxnon:
			return "DPSxnon";

		case GDI_DPSaon:
			return "DPSaon";

		case GDI_PSDPSanaxx:
			return "PSDPSanaxx";

		case GDI_SSPxDSxaxn:
			return "SSPxDSxaxn";

		case GDI_SPxPDxa:
			return "SPxPDxa";

		case GDI_SDPSanaxn:
			return "SDPSanaxn";

		case GDI_PDSPaox:
			return "PDSPaox";

		case GDI_SDPSxaxn:
			return "SDPSxaxn";

		case GDI_PSDPaox:
			return "PSDPaox";

		case GDI_DSPDxaxn:
			return "DSPDxaxn";

		case GDI_PDSox:
			return "PDSox";

		case GDI_PDSoan:
			return "PDSoan";

		case GDI_DPSnaa:
			return "DPSnaa";

		case GDI_SDPxon:
			return "SDPxon";

		case GDI_DSna:
			return "DSna";

		case GDI_SPDnaon:
			return "SPDnaon";

		case GDI_SPxDSxa:
			return "SPxDSxa";

		case GDI_PDSPanaxn:
			return "PDSPanaxn";

		case GDI_SDPSaox:
			return "SDPSaox";

		case GDI_SDPSxnox:
			return "SDPSxnox";

		case GDI_DPSxa:
			return "DPSxa";

		case GDI_PSDPSaoxxn:
			return "PSDPSaoxxn";

		case GDI_DPSana:
			return "DPSana";

		case GDI_SSPxPDxaxn:
			return "SSPxPDxaxn";

		case GDI_SPDSoax:
			return "SPDSoax";

		case GDI_PSDnox:
			return "PSDnox";

		case GDI_PSDPxox:
			return "PSDPxox";

		case GDI_PSDnoan:
			return "PSDnoan";

		case GDI_PSna:
			return "PSna";

		case GDI_SDPnaon:
			return "SDPnaon";

		case GDI_SDPSoox:
			return "SDPSoox";

		case GDI_NOTSRCCOPY:
			return "Sn";

		case GDI_SPDSaox:
			return "SPDSaox";

		case GDI_SPDSxnox:
			return "SPDSxnox";

		case GDI_SDPox:
			return "SDPox";

		case GDI_SDPoan:
			return "SDPoan";

		case GDI_PSDPoax:
			return "PSDPoax";

		case GDI_SPDnox:
			return "SPDnox";

		case GDI_SPDSxox:
			return "SPDSxox";

		case GDI_SPDnoan:
			return "SPDnoan";

		case GDI_PSx:
			return "PSx";

		case GDI_SPDSonox:
			return "SPDSonox";

		case GDI_SPDSnaox:
			return "SPDSnaox";

		case GDI_PSan:
			return "PSan";

		case GDI_PSDnaa:
			return "PSDnaa";

		case GDI_DPSxon:
			return "DPSxon";

		case GDI_SDxPDxa:
			return "SDxPDxa";

		case GDI_SPDSanaxn:
			return "SPDSanaxn";

		case GDI_SRCERASE:
			return "SDna";

		case GDI_DPSnaon:
			return "DPSnaon";

		case GDI_DSPDaox:
			return "DSPDaox";

		case GDI_PSDPxaxn:
			return "PSDPxaxn";

		case GDI_SDPxa:
			return "SDPxa";

		case GDI_PDSPDaoxxn:
			return "PDSPDaoxxn";

		case GDI_DPSDoax:
			return "DPSDoax";

		case GDI_PDSnox:
			return "PDSnox";

		case GDI_SDPana:
			return "SDPana";

		case GDI_SSPxDSxoxn:
			return "SSPxDSxoxn";

		case GDI_PDSPxox:
			return "PDSPxox";

		case GDI_PDSnoan:
			return "PDSnoan";

		case GDI_PDna:
			return "PDna";

		case GDI_DSPnaon:
			return "DSPnaon";

		case GDI_DPSDaox:
			return "DPSDaox";

		case GDI_SPDSxaxn:
			return "SPDSxaxn";

		case GDI_DPSonon:
			return "DPSonon";

		case GDI_DSTINVERT:
			return "Dn";

		case GDI_DPSox:
			return "DPSox";

		case GDI_DPSoan:
			return "DPSoan";

		case GDI_PDSPoax:
			return "PDSPoax";

		case GDI_DPSnox:
			return "DPSnox";

		case GDI_PATINVERT:
			return "DPx";

		case GDI_DPSDonox:
			return "DPSDonox";

		case GDI_DPSDxox:
			return "DPSDxox";

		case GDI_DPSnoan:
			return "DPSnoan";

		case GDI_DPSDnaox:
			return "DPSDnaox";

		case GDI_DPan:
			return "DPan";

		case GDI_PDSxa:
			return "PDSxa";

		case GDI_DSPDSaoxxn:
			return "DSPDSaoxxn";

		case GDI_DSPDoax:
			return "DSPDoax";

		case GDI_SDPnox:
			return "SDPnox";

		case GDI_SDPSoax:
			return "SDPSoax";

		case GDI_DSPnox:
			return "DSPnox";

		case GDI_SRCINVERT:
			return "DSx";

		case GDI_SDPSonox:
			return "SDPSonox";

		case GDI_DSPDSonoxxn:
			return "DSPDSonoxxn";

		case GDI_PDSxxn:
			return "PDSxxn";

		case GDI_DPSax:
			return "DPSax";

		case GDI_PSDPSoaxxn:
			return "PSDPSoaxxn";

		case GDI_SDPax:
			return "SDPax";

		case GDI_PDSPDoaxxn:
			return "PDSPDoaxxn";

		case GDI_SDPSnoax:
			return "SDPSnoax";

		case GDI_PDSxnan:
			return "PDSxnan";

		case GDI_PDSana:
			return "PDSana";

		case GDI_SSDxPDxaxn:
			return "SSDxPDxaxn";

		case GDI_SDPSxox:
			return "SDPSxox";

		case GDI_SDPnoan:
			return "SDPnoan";

		case GDI_DSPDxox:
			return "DSPDxox";

		case GDI_DSPnoan:
			return "DSPnoan";

		case GDI_SDPSnaox:
			return "SDPSnaox";

		case GDI_DSan:
			return "DSan";

		case GDI_PDSax:
			return "PDSax";

		case GDI_DSPDSoaxxn:
			return "DSPDSoaxxn";

		case GDI_DPSDnoax:
			return "DPSDnoax";

		case GDI_SDPxnan:
			return "SDPxnan";

		case GDI_SPDSnoax:
			return "SPDSnoax";

		case GDI_DPSxnan:
			return "DPSxnan";

		case GDI_SPxDSxo:
			return "SPxDSxo";

		case GDI_DPSaan:
			return "DPSaan";

		case GDI_DPSaa:
			return "DPSaa";

		case GDI_SPxDSxon:
			return "SPxDSxon";

		case GDI_DPSxna:
			return "DPSxna";

		case GDI_SPDSnoaxn:
			return "SPDSnoaxn";

		case GDI_SDPxna:
			return "SDPxna";

		case GDI_PDSPnoaxn:
			return "PDSPnoaxn";

		case GDI_DSPDSoaxx:
			return "DSPDSoaxx";

		case GDI_PDSaxn:
			return "PDSaxn";

		case GDI_SRCAND:
			return "DSa";

		case GDI_SDPSnaoxn:
			return "SDPSnaoxn";

		case GDI_DSPnoa:
			return "DSPnoa";

		case GDI_DSPDxoxn:
			return "DSPDxoxn";

		case GDI_SDPnoa:
			return "SDPnoa";

		case GDI_SDPSxoxn:
			return "SDPSxoxn";

		case GDI_SSDxPDxax:
			return "SSDxPDxax";

		case GDI_PDSanan:
			return "PDSanan";

		case GDI_PDSxna:
			return "PDSxna";

		case GDI_SDPSnoaxn:
			return "SDPSnoaxn";

		case GDI_DPSDPoaxx:
			return "DPSDPoaxx";

		case GDI_SPDaxn:
			return "SPDaxn";

		case GDI_PSDPSoaxx:
			return "PSDPSoaxx";

		case GDI_DPSaxn:
			return "DPSaxn";

		case GDI_DPSxx:
			return "DPSxx";

		case GDI_PSDPSonoxx:
			return "PSDPSonoxx";

		case GDI_SDPSonoxn:
			return "SDPSonoxn";

		case GDI_DSxn:
			return "DSxn";

		case GDI_DPSnax:
			return "DPSnax";

		case GDI_SDPSoaxn:
			return "SDPSoaxn";

		case GDI_SPDnax:
			return "SPDnax";

		case GDI_DSPDoaxn:
			return "DSPDoaxn";

		case GDI_DSPDSaoxx:
			return "DSPDSaoxx";

		case GDI_PDSxan:
			return "PDSxan";

		case GDI_DPa:
			return "DPa";

		case GDI_PDSPnaoxn:
			return "PDSPnaoxn";

		case GDI_DPSnoa:
			return "DPSnoa";

		case GDI_DPSDxoxn:
			return "DPSDxoxn";

		case GDI_PDSPonoxn:
			return "PDSPonoxn";

		case GDI_PDxn:
			return "PDxn";

		case GDI_DSPnax:
			return "DSPnax";

		case GDI_PDSPoaxn:
			return "PDSPoaxn";

		case GDI_DPSoa:
			return "DPSoa";

		case GDI_DPSoxn:
			return "DPSoxn";

		case GDI_DSTCOPY:
			return "D";

		case GDI_DPSono:
			return "DPSono";

		case GDI_SPDSxax:
			return "SPDSxax";

		case GDI_DPSDaoxn:
			return "DPSDaoxn";

		case GDI_DSPnao:
			return "DSPnao";

		case GDI_DPno:
			return "DPno";

		case GDI_PDSnoa:
			return "PDSnoa";

		case GDI_PDSPxoxn:
			return "PDSPxoxn";

		case GDI_SSPxDSxox:
			return "SSPxDSxox";

		case GDI_SDPanan:
			return "SDPanan";

		case GDI_PSDnax:
			return "PSDnax";

		case GDI_DPSDoaxn:
			return "DPSDoaxn";

		case GDI_DPSDPaoxx:
			return "DPSDPaoxx";

		case GDI_SDPxan:
			return "SDPxan";

		case GDI_PSDPxax:
			return "PSDPxax";

		case GDI_DSPDaoxn:
			return "DSPDaoxn";

		case GDI_DPSnao:
			return "DPSnao";

		case GDI_MERGEPAINT:
			return "DSno";

		case GDI_SPDSanax:
			return "SPDSanax";

		case GDI_SDxPDxan:
			return "SDxPDxan";

		case GDI_DPSxo:
			return "DPSxo";

		case GDI_DPSano:
			return "DPSano";

		case GDI_MERGECOPY:
			return "PSa";

		case GDI_SPDSnaoxn:
			return "SPDSnaoxn";

		case GDI_SPDSonoxn:
			return "SPDSonoxn";

		case GDI_PSxn:
			return "PSxn";

		case GDI_SPDnoa:
			return "SPDnoa";

		case GDI_SPDSxoxn:
			return "SPDSxoxn";

		case GDI_SDPnax:
			return "SDPnax";

		case GDI_PSDPoaxn:
			return "PSDPoaxn";

		case GDI_SDPoa:
			return "SDPoa";

		case GDI_SPDoxn:
			return "SPDoxn";

		case GDI_DPSDxax:
			return "DPSDxax";

		case GDI_SPDSaoxn:
			return "SPDSaoxn";

		case GDI_SRCCOPY:
			return "S";

		case GDI_SDPono:
			return "SDPono";

		case GDI_SDPnao:
			return "SDPnao";

		case GDI_SPno:
			return "SPno";

		case GDI_PSDnoa:
			return "PSDnoa";

		case GDI_PSDPxoxn:
			return "PSDPxoxn";

		case GDI_PDSnax:
			return "PDSnax";

		case GDI_SPDSoaxn:
			return "SPDSoaxn";

		case GDI_SSPxPDxax:
			return "SSPxPDxax";

		case GDI_DPSanan:
			return "DPSanan";

		case GDI_PSDPSaoxx:
			return "PSDPSaoxx";

		case GDI_DPSxan:
			return "DPSxan";

		case GDI_PDSPxax:
			return "PDSPxax";

		case GDI_SDPSaoxn:
			return "SDPSaoxn";

		case GDI_DPSDanax:
			return "DPSDanax";

		case GDI_SPxDSxan:
			return "SPxDSxan";

		case GDI_SPDnao:
			return "SPDnao";

		case GDI_SDno:
			return "SDno";

		case GDI_SDPxo:
			return "SDPxo";

		case GDI_SDPano:
			return "SDPano";

		case GDI_PDSoa:
			return "PDSoa";

		case GDI_PDSoxn:
			return "PDSoxn";

		case GDI_DSPDxax:
			return "DSPDxax";

		case GDI_PSDPaoxn:
			return "PSDPaoxn";

		case GDI_SDPSxax:
			return "SDPSxax";

		case GDI_PDSPaoxn:
			return "PDSPaoxn";

		case GDI_SDPSanax:
			return "SDPSanax";

		case GDI_SPxPDxan:
			return "SPxPDxan";

		case GDI_SSPxDSxax:
			return "SSPxDSxax";

		case GDI_DSPDSanaxxn:
			return "DSPDSanaxxn";

		case GDI_DPSao:
			return "DPSao";

		case GDI_DPSxno:
			return "DPSxno";

		case GDI_SDPao:
			return "SDPao";

		case GDI_SDPxno:
			return "SDPxno";

		case GDI_SRCPAINT:
			return "DSo";

		case GDI_SDPnoo:
			return "SDPnoo";

		case GDI_PATCOPY:
			return "P";

		case GDI_PDSono:
			return "PDSono";

		case GDI_PDSnao:
			return "PDSnao";

		case GDI_PSno:
			return "PSno";

		case GDI_PSDnao:
			return "PSDnao";

		case GDI_PDno:
			return "PDno";

		case GDI_PDSxo:
			return "PDSxo";

		case GDI_PDSano:
			return "PDSano";

		case GDI_PDSao:
			return "PDSao";

		case GDI_PDSxno:
			return "PDSxno";

		case GDI_DPo:
			return "DPo";

		case GDI_PATPAINT:
			return "DPSnoo";

		case GDI_PSo:
			return "PSo";

		case GDI_PSDnoo:
			return "PSDnoo";

		case GDI_DPSoo:
			return "DPSoo";

		case GDI_WHITENESS:
			return "1";

		case GDI_GLYPH_ORDER:
			return "SPaDSnao";

		default:
			return "";
	}
}

/**
 * Create a new solid brush.\n
 * @msdn{dd183518}
 * @param crColor brush color
 * @return new brush
 */
HGDI_BRUSH gdi_CreateSolidBrush(UINT32 crColor)
{
	HGDI_BRUSH hBrush = (HGDI_BRUSH)calloc(1, sizeof(GDI_BRUSH));

	if (!hBrush)
		return NULL;

	hBrush->objectType = GDIOBJECT_BRUSH;
	hBrush->style = GDI_BS_SOLID;
	hBrush->color = crColor;
	return hBrush;
}
/**
 * Create a new pattern brush.\n
 * @msdn{dd183508}
 * @param hbmp pattern bitmap
 * @return new brush
 */
HGDI_BRUSH gdi_CreatePatternBrush(HGDI_BITMAP hbmp)
{
	HGDI_BRUSH hBrush = (HGDI_BRUSH)calloc(1, sizeof(GDI_BRUSH));

	if (!hBrush)
		return NULL;

	hBrush->objectType = GDIOBJECT_BRUSH;
	hBrush->style = GDI_BS_PATTERN;
	hBrush->pattern = hbmp;
	return hBrush;
}
HGDI_BRUSH gdi_CreateHatchBrush(HGDI_BITMAP hbmp)
{
	HGDI_BRUSH hBrush = (HGDI_BRUSH)calloc(1, sizeof(GDI_BRUSH));

	if (!hBrush)
		return NULL;

	hBrush->objectType = GDIOBJECT_BRUSH;
	hBrush->style = GDI_BS_HATCHED;
	hBrush->pattern = hbmp;
	return hBrush;
}

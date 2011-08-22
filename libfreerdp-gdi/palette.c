/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Palette Functions
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

/* GDI Palette Functions: http://msdn.microsoft.com/en-us/library/dd183454/ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/palette.h>

static HGDI_PALETTE hSystemPalette = NULL;

static const GDI_PALETTEENTRY default_system_palette[20] =
{
	/* First 10 entries */
	{ 0x00, 0x00, 0x00 },
	{ 0x80, 0x00, 0x00 },
	{ 0x00, 0x80, 0x00 },
	{ 0x80, 0x80, 0x00 },
	{ 0x00, 0x00, 0x80 },
	{ 0x80, 0x00, 0x80 },
	{ 0x00, 0x80, 0x80 },
	{ 0xC0, 0xC0, 0xC0 },
	{ 0xC0, 0xDC, 0xC0 },
	{ 0xA6, 0xCA, 0xF0 },

	/* Last 10 entries */
	{ 0xFF, 0xFB, 0xF0 },
	{ 0xA0, 0xA0, 0xA4 },
	{ 0x80, 0x80, 0x80 },
	{ 0xFF, 0x00, 0x00 },
	{ 0x00, 0xFF, 0x00 },
	{ 0xFF, 0xFF, 0x00 },
	{ 0x00, 0x00, 0xFF },
	{ 0xFF, 0x00, 0xFF },
	{ 0x00, 0xFF, 0xFF },
	{ 0xFF, 0xFF, 0xFF }
};

/**
 * Create a new palette.\n
 * @msdn{dd183507}
 * @param original palette
 * @return new palette
 */

HGDI_PALETTE gdi_CreatePalette(HGDI_PALETTE palette)
{
	HGDI_PALETTE hPalette = (HGDI_PALETTE) malloc(sizeof(GDI_PALETTE));
	hPalette->count = palette->count;
	hPalette->entries = (GDI_PALETTEENTRY*) malloc(sizeof(GDI_PALETTEENTRY) * hPalette->count);
	memcpy(hPalette->entries, palette->entries, sizeof(GDI_PALETTEENTRY) * hPalette->count);
	return hPalette;
}

/**
 * Create system palette\n
 * @return system palette
 */

HGDI_PALETTE CreateSystemPalette()
{
	HGDI_PALETTE palette = (HGDI_PALETTE) malloc(sizeof(GDI_PALETTE));

	palette->count = 256;
	palette->entries = (GDI_PALETTEENTRY*) malloc(sizeof(GDI_PALETTEENTRY) * 256);
	memset(palette->entries, 0, sizeof(GDI_PALETTEENTRY) * 256);

	memcpy(&palette->entries[0], &default_system_palette[0], 10 * sizeof(GDI_PALETTEENTRY));
	memcpy(&palette->entries[256 - 10], &default_system_palette[10], 10 * sizeof(GDI_PALETTEENTRY));

	return palette;
}

/**
 * Get system palette\n
 * @return system palette
 */

HGDI_PALETTE gdi_GetSystemPalette()
{
	if (hSystemPalette == NULL)
		hSystemPalette = CreateSystemPalette();

	return hSystemPalette;
}

/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Graphical Objects
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __GDI_GRAPHICS_H
#define __GDI_GRAPHICS_H

#include <freerdp/gdi/gdi.h>
#include <freerdp/graphics.h>

HGDI_BITMAP gdi_create_bitmap(rdpGdi* gdi, int width, int height, int bpp, uint8* data);

void gdi_Bitmap_New(rdpContext* context, rdpBitmap* bitmap);
void gdi_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap);
void gdi_Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed);
void gdi_register_graphics(rdpGraphics* graphics);

#endif /* __GDI_GRAPHICS_H */

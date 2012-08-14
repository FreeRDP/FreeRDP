/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Library
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __GDI_CORE_H
#define __GDI_CORE_H

#include "graphics.h"

gdiBitmap* gdi_bitmap_new_ex(rdpGdi* gdi, int width, int height, int bpp, uint8* data);
void gdi_bitmap_free_ex(gdiBitmap* gdi_bmp);

#endif /* __GDI_CORE_H */

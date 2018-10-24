/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Graphical Objects
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

#ifndef __DF_GRAPHICS_H
#define __DF_GRAPHICS_H

#include "dfreerdp.h"

void df_create_temp_surface(dfInfo *dfi, int width, int height, int bpp, IDirectFBSurface **ppsurf);

void df_fullscreen_cursor_bounds(rdpGdi* gdi, dfInfo* dfi, int *cursor_left, int *cursor_top, int *cursor_right, int *cursor_bottom);
void df_fullscreen_cursor_unpaint(uint8 *surface, int pitch, dfContext *context, boolean update_pos);
void df_fullscreen_cursor_save_image_under(uint8 *surface, int pitch, dfContext *context);
void df_fullscreen_cursor_paint(uint8 *surface, int pitch, dfContext *context);

void df_register_graphics(rdpGraphics* graphics);

#endif /* __DF_GRAPHICS_H */

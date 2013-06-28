/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef __WF_GDI_H
#define __WF_GDI_H

#include "wf_interface.h"

void wf_invalidate_region(wfContext* wfc, int x, int y, int width, int height);
wfBitmap* wf_image_new(wfContext* wfc, int width, int height, int bpp, BYTE* data);
void wf_image_free(wfBitmap* image);
void wf_update_offset(wfContext* wfc);
void wf_resize_window(wfContext* wfc);
void wf_toggle_fullscreen(wfContext* wfc);

void wf_gdi_register_update_callbacks(rdpUpdate* update);

void wf_update_canvas_diff(wfContext* wfc);

#endif /* __WF_GDI_H */

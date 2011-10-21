/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "wfreerdp.h"

void wf_invalidate_region(wfInfo* wfi, int x, int y, int width, int height);
wfBitmap* wf_image_new(wfInfo* wfi, int width, int height, int bpp, uint8* data);
void wf_image_free(wfBitmap* image);
void wf_toggle_fullscreen(wfInfo* wfi);

void wf_gdi_register_update_callbacks(rdpUpdate* update);

#endif /* __WF_GDI_H */

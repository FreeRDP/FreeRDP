/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Graphical Objects
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

#ifndef FREERDP_CLIENT_WIN_GRAPHICS_H
#define FREERDP_CLIENT_WIN_GRAPHICS_H

#include "wf_client.h"

HBITMAP wf_create_dib(wfContext* wfc, UINT32 width, UINT32 height, UINT32 format, const BYTE* data,
                      BYTE** pdata);
wfBitmap* wf_image_new(wfContext* wfc, UINT32 width, UINT32 height, UINT32 format,
                       const BYTE* data);
void wf_image_free(wfBitmap* image);

BOOL wf_register_pointer(rdpGraphics* graphics);
BOOL wf_register_graphics(rdpGraphics* graphics);

#endif /* FREERDP_CLIENT_WIN_GRAPHICS_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_SHADOW_SERVER_SURFACE_H
#define FREERDP_SHADOW_SERVER_SURFACE_H

#include <freerdp/server/shadow.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#ifdef __cplusplus
extern "C" {
#endif

rdpShadowSurface* shadow_surface_new(rdpShadowServer* server, int x, int y, int width, int height);
void shadow_surface_free(rdpShadowSurface* surface);
BOOL shadow_surface_resize(rdpShadowSurface *surface, int x, int y, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SHADOW_SERVER_SURFACE_H */

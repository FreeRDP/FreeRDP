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

#ifndef FREERDP_SERVER_SHADOW_SCREEN_H
#define FREERDP_SERVER_SHADOW_SCREEN_H

#include <freerdp/server/shadow.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

struct rdp_shadow_screen
{
	rdpShadowServer* server;

	UINT32 width;
	UINT32 height;

	CRITICAL_SECTION lock;
	REGION16 invalidRegion;

	rdpShadowSurface* primary;
	rdpShadowSurface* lobby;
};

#ifdef __cplusplus
extern "C"
{
#endif

	rdpShadowScreen* shadow_screen_new(rdpShadowServer* server);
	void shadow_screen_free(rdpShadowScreen* screen);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_SCREEN_H */

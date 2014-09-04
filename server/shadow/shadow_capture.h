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

#ifndef FREERDP_SHADOW_SERVER_CAPTURE_H
#define FREERDP_SHADOW_SERVER_CAPTURE_H

#include <freerdp/server/shadow.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

struct rdp_shadow_capture
{
	rdpShadowServer* server;

	int width;
	int height;

	CRITICAL_SECTION lock;
};

#ifdef __cplusplus
extern "C" {
#endif

int shadow_capture_align_clip_rect(RECTANGLE_16* rect, RECTANGLE_16* clip);
int shadow_capture_compare(BYTE* pData1, int nStep1, int nWidth, int nHeight, BYTE* pData2, int nStep2, RECTANGLE_16* rect);

rdpShadowCapture* shadow_capture_new(rdpShadowServer* server);
void shadow_capture_free(rdpShadowCapture* capture);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SHADOW_SERVER_CAPTURE_H */

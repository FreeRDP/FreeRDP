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

#ifndef FREERDP_SHADOW_SERVER_SUBSYSTEM_H
#define FREERDP_SHADOW_SERVER_SUBSYSTEM_H

#include <freerdp/server/shadow.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#define SHADOW_MSG_IN_REFRESH_OUTPUT_ID			1001
#define SHADOW_MSG_IN_SUPPRESS_OUTPUT_ID		1002

struct _SHADOW_MSG_IN_REFRESH_OUTPUT
{
	UINT32 numRects;
	RECTANGLE_16* rects;
};
typedef struct _SHADOW_MSG_IN_REFRESH_OUTPUT SHADOW_MSG_IN_REFRESH_OUTPUT;

struct _SHADOW_MSG_IN_SUPPRESS_OUTPUT
{
	BOOL allow;
	RECTANGLE_16 rect;
};
typedef struct _SHADOW_MSG_IN_SUPPRESS_OUTPUT SHADOW_MSG_IN_SUPPRESS_OUTPUT;

#define SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID		2001
#define SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID			2002

struct _SHADOW_MSG_OUT_POINTER_POSITION_UPDATE
{
	UINT32 xPos;
	UINT32 yPos;
};
typedef struct _SHADOW_MSG_OUT_POINTER_POSITION_UPDATE SHADOW_MSG_OUT_POINTER_POSITION_UPDATE;

struct _SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE
{
	UINT32 xHot;
	UINT32 yHot;
	UINT32 width;
	UINT32 height;
	BYTE* pixels;
	int scanline;
	BOOL premultiplied;
};
typedef struct _SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE;

#ifdef __cplusplus
extern "C" {
#endif

rdpShadowSubsystem* shadow_subsystem_new(const char* name);
void shadow_subsystem_free(rdpShadowSubsystem* subsystem);

int shadow_subsystem_init(rdpShadowSubsystem* subsystem, rdpShadowServer* server);
void shadow_subsystem_uninit(rdpShadowSubsystem* subsystem);

int shadow_subsystem_start(rdpShadowSubsystem* subsystem);
int shadow_subsystem_stop(rdpShadowSubsystem* subsystem);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SHADOW_SERVER_SUBSYSTEM_H */


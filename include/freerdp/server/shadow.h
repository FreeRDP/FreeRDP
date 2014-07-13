/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Session Shadowing
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

#ifndef FREERDP_SERVER_SHADOW_H
#define FREERDP_SERVER_SHADOW_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/settings.h>
#include <freerdp/listener.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/collections.h>

typedef struct rdp_shadow_client rdpShadowClient;
typedef struct rdp_shadow_server rdpShadowServer;
typedef struct rdp_shadow_screen rdpShadowScreen;
typedef struct rdp_shadow_surface rdpShadowSurface;
typedef struct rdp_shadow_encoder rdpShadowEncoder;
typedef struct rdp_shadow_subsystem rdpShadowSubsystem;

struct rdp_shadow_client
{
	rdpContext context;

	HANDLE thread;
	BOOL activated;
	HANDLE StopEvent;
	rdpShadowServer* server;
};

struct rdp_shadow_server
{
	void* ext;
	HANDLE thread;
	HANDLE StopEvent;
	rdpShadowScreen* screen;
	rdpShadowSurface* surface;
	rdpShadowEncoder* encoder;
	rdpShadowSubsystem* subsystem;

	DWORD port;
	freerdp_listener* listener;
};

#define RDP_SHADOW_SUBSYSTEM_COMMON() \
	rdpShadowServer* server; \
	HANDLE event; \
	int monitorCount; \
	MONITOR_DEF monitors[16]

struct rdp_shadow_subsystem
{
	RDP_SHADOW_SUBSYSTEM_COMMON();
};

#endif /* FREERDP_SERVER_SHADOW_H */


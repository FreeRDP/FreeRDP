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

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>

#include <freerdp/server/encomsp.h>
#include <freerdp/server/remdesk.h>

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
typedef struct rdp_shadow_capture rdpShadowCapture;
typedef struct rdp_shadow_subsystem rdpShadowSubsystem;

typedef rdpShadowSubsystem* (*pfnShadowCreateSubsystem)(rdpShadowServer* server);

typedef int (*pfnShadowSubsystemInit)(rdpShadowSubsystem* subsystem);
typedef int (*pfnShadowSubsystemUninit)(rdpShadowSubsystem* subsystem);
typedef int (*pfnShadowSubsystemStart)(rdpShadowSubsystem* subsystem);
typedef int (*pfnShadowSubsystemStop)(rdpShadowSubsystem* subsystem);
typedef void (*pfnShadowSubsystemFree)(rdpShadowSubsystem* subsystem);

typedef int (*pfnShadowSurfaceCopy)(rdpShadowSubsystem* subsystem);
typedef int (*pfnShadowSurfaceUpdate)(rdpShadowSubsystem* subsystem, REGION16* region);

typedef int (*pfnShadowSynchronizeEvent)(rdpShadowSubsystem* subsystem, UINT32 flags);
typedef int (*pfnShadowKeyboardEvent)(rdpShadowSubsystem* subsystem, UINT16 flags, UINT16 code);
typedef int (*pfnShadowUnicodeKeyboardEvent)(rdpShadowSubsystem* subsystem, UINT16 flags, UINT16 code);
typedef int (*pfnShadowMouseEvent)(rdpShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y);
typedef int (*pfnShadowExtendedMouseEvent)(rdpShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y);

struct rdp_shadow_client
{
	rdpContext context;

	HANDLE thread;
	BOOL activated;
	BOOL inLobby;
	BOOL mayView;
	BOOL mayInteract;
	HANDLE StopEvent;
	CRITICAL_SECTION lock;
	REGION16 invalidRegion;
	rdpShadowServer* server;
	rdpShadowSurface* lobby;
	rdpShadowEncoder* encoder;

	HANDLE vcm;
	EncomspServerContext* encomsp;
	RemdeskServerContext* remdesk;
};

struct rdp_shadow_server
{
	void* ext;
	HANDLE thread;
	HANDLE StopEvent;
	wArrayList* clients;
	rdpShadowScreen* screen;
	rdpShadowSurface* surface;
	rdpShadowCapture* capture;
	rdpShadowSubsystem* subsystem;

	DWORD port;
	BOOL mayView;
	BOOL mayInteract;
	char* ipcSocket;
	char* ConfigPath;
	char* CertificateFile;
	char* PrivateKeyFile;
	CRITICAL_SECTION lock;
	freerdp_listener* listener;
	pfnShadowCreateSubsystem CreateSubsystem;
};

#define RDP_SHADOW_SUBSYSTEM_COMMON() \
	HANDLE event; \
	int monitorCount; \
	MONITOR_DEF monitors[16]; \
	MONITOR_DEF virtualScreen; \
	HANDLE updateEvent; \
	REGION16 invalidRegion; \
	SYNCHRONIZATION_BARRIER barrier; \
	\
	pfnShadowSubsystemInit Init; \
	pfnShadowSubsystemUninit Uninit; \
	pfnShadowSubsystemStart Start; \
	pfnShadowSubsystemStop Stop; \
	pfnShadowSubsystemFree Free; \
	\
	pfnShadowSurfaceCopy SurfaceCopy; \
	pfnShadowSurfaceUpdate SurfaceUpdate; \
	\
	pfnShadowSynchronizeEvent SynchronizeEvent; \
	pfnShadowKeyboardEvent KeyboardEvent; \
	pfnShadowUnicodeKeyboardEvent UnicodeKeyboardEvent; \
	pfnShadowMouseEvent MouseEvent; \
	pfnShadowExtendedMouseEvent ExtendedMouseEvent; \
	\
	rdpShadowServer* server

struct rdp_shadow_subsystem
{
	RDP_SHADOW_SUBSYSTEM_COMMON();
};

#endif /* FREERDP_SERVER_SHADOW_H */


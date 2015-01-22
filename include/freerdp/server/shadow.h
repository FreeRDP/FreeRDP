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

typedef struct _RDP_SHADOW_ENTRY_POINTS RDP_SHADOW_ENTRY_POINTS;
typedef int (*pfnShadowSubsystemEntry)(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);

typedef rdpShadowSubsystem* (*pfnShadowSubsystemNew)(void);
typedef void (*pfnShadowSubsystemFree)(rdpShadowSubsystem* subsystem);

typedef int (*pfnShadowSubsystemInit)(rdpShadowSubsystem* subsystem);
typedef int (*pfnShadowSubsystemUninit)(rdpShadowSubsystem* subsystem);

typedef int (*pfnShadowSubsystemStart)(rdpShadowSubsystem* subsystem);
typedef int (*pfnShadowSubsystemStop)(rdpShadowSubsystem* subsystem);

typedef int (*pfnShadowEnumMonitors)(MONITOR_DEF* monitors, int maxMonitors);

typedef int (*pfnShadowAuthenticate)(rdpShadowSubsystem* subsystem,
		const char* user, const char* domain, const char* password);

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
	rdpShadowSubsystem* subsystem;

	UINT32 pointerX;
	UINT32 pointerY;

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
	BOOL shareSubRect;
	BOOL authentication;
	int selectedMonitor;
	RECTANGLE_16 subRect;
	char* ipcSocket;
	char* ConfigPath;
	char* CertificateFile;
	char* PrivateKeyFile;
	CRITICAL_SECTION lock;
	freerdp_listener* listener;
};

struct _RDP_SHADOW_ENTRY_POINTS
{
	pfnShadowSubsystemNew New;
	pfnShadowSubsystemFree Free;

	pfnShadowSubsystemInit Init;
	pfnShadowSubsystemUninit Uninit;

	pfnShadowSubsystemStart Start;
	pfnShadowSubsystemStop Stop;

	pfnShadowEnumMonitors EnumMonitors;
};

#define RDP_SHADOW_SUBSYSTEM_COMMON() \
	RDP_SHADOW_ENTRY_POINTS ep; \
	HANDLE event; \
	int numMonitors; \
	int captureFrameRate; \
	int selectedMonitor; \
	MONITOR_DEF monitors[16]; \
	MONITOR_DEF virtualScreen; \
	HANDLE updateEvent; \
	BOOL suppressOutput; \
	REGION16 invalidRegion; \
	wMessagePipe* MsgPipe; \
	SYNCHRONIZATION_BARRIER barrier; \
	UINT32 pointerX; \
	UINT32 pointerY; \
	\
	pfnShadowSynchronizeEvent SynchronizeEvent; \
	pfnShadowKeyboardEvent KeyboardEvent; \
	pfnShadowUnicodeKeyboardEvent UnicodeKeyboardEvent; \
	pfnShadowMouseEvent MouseEvent; \
	pfnShadowExtendedMouseEvent ExtendedMouseEvent; \
	\
	pfnShadowAuthenticate Authenticate; \
	\
	rdpShadowServer* server

struct rdp_shadow_subsystem
{
	RDP_SHADOW_SUBSYSTEM_COMMON();
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int shadow_server_parse_command_line(rdpShadowServer* server, int argc, char** argv);
FREERDP_API int shadow_server_command_line_status_print(rdpShadowServer* server, int argc, char** argv, int status);

FREERDP_API int shadow_server_start(rdpShadowServer* server);
FREERDP_API int shadow_server_stop(rdpShadowServer* server);

FREERDP_API int shadow_server_init(rdpShadowServer* server);
FREERDP_API int shadow_server_uninit(rdpShadowServer* server);

FREERDP_API int shadow_enum_monitors(MONITOR_DEF* monitors, int maxMonitors, const char* name);

FREERDP_API rdpShadowServer* shadow_server_new();
FREERDP_API void shadow_server_free(rdpShadowServer* server);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_H */


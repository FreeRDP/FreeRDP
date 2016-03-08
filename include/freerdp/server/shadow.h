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
#include <freerdp/server/rdpsnd.h>
#include <freerdp/server/audin.h>

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
typedef struct rdp_shadow_multiclient_event rdpShadowMultiClientEvent;

typedef struct _RDP_SHADOW_ENTRY_POINTS RDP_SHADOW_ENTRY_POINTS;
typedef int (*pfnShadowSubsystemEntry)(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);

typedef rdpShadowSubsystem* (*pfnShadowSubsystemNew)(void);
typedef void (*pfnShadowSubsystemFree)(rdpShadowSubsystem* subsystem);

typedef int (*pfnShadowSubsystemInit)(rdpShadowSubsystem* subsystem);
typedef int (*pfnShadowSubsystemUninit)(rdpShadowSubsystem* subsystem);

typedef int (*pfnShadowSubsystemStart)(rdpShadowSubsystem* subsystem);
typedef int (*pfnShadowSubsystemStop)(rdpShadowSubsystem* subsystem);

typedef int (*pfnShadowEnumMonitors)(MONITOR_DEF* monitors, int maxMonitors);

typedef int (*pfnShadowAuthenticate)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
		const char* user, const char* domain, const char* password);
typedef BOOL (*pfnShadowClientConnect)(rdpShadowSubsystem* subsystem, rdpShadowClient* client);
typedef void (*pfnShadowClientDisconnect)(rdpShadowSubsystem* subsystem, rdpShadowClient* client);
typedef BOOL (*pfnShadowClientCapabilities)(rdpShadowSubsystem* subsystem, rdpShadowClient* client);

typedef int (*pfnShadowSynchronizeEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client, UINT32 flags);
typedef int (*pfnShadowKeyboardEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 code);
typedef int (*pfnShadowUnicodeKeyboardEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 code);
typedef int (*pfnShadowMouseEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 x, UINT16 y);
typedef int (*pfnShadowExtendedMouseEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client, UINT16 flags, UINT16 x, UINT16 y);

typedef void (*pfnShadowChannelAudinServerReceiveSamples)(rdpShadowSubsystem* subsystem, rdpShadowClient* client, const void* buf, int nframes);

struct rdp_shadow_client
{
	rdpContext context;

	HANDLE thread;
	BOOL activated;
	BOOL inLobby;
	BOOL mayView;
	BOOL mayInteract;
	wMessageQueue* MsgQueue;
	CRITICAL_SECTION lock;
	REGION16 invalidRegion;
	rdpShadowServer* server;
	rdpShadowEncoder* encoder;
	rdpShadowSubsystem* subsystem;

	UINT32 pointerX;
	UINT32 pointerY;

	HANDLE vcm;
	EncomspServerContext* encomsp;
	RemdeskServerContext* remdesk;
	RdpsndServerContext* rdpsnd;
	audin_server_context* audin;
};

struct rdp_shadow_server
{
	void* ext;
	HANDLE thread;
	HANDLE StopEvent;
	wArrayList* clients;
	rdpShadowScreen* screen;
	rdpShadowSurface* surface;
	rdpShadowSurface* lobby;
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

struct rdp_shadow_surface
{
	rdpShadowServer* server;

	int x;
	int y;
	int width;
	int height;
	int scanline;
	BYTE* data;

	CRITICAL_SECTION lock;
	REGION16 invalidRegion;
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
	rdpShadowMultiClientEvent* updateEvent; \
	BOOL suppressOutput; \
	REGION16 invalidRegion; \
	wMessagePipe* MsgPipe; \
	UINT32 pointerX; \
	UINT32 pointerY; \
	\
	const AUDIO_FORMAT* rdpsndFormats; \
	int nRdpsndFormats; \
	const AUDIO_FORMAT* audinFormats; \
	int nAudinFormats; \
	\
	pfnShadowSynchronizeEvent SynchronizeEvent; \
	pfnShadowKeyboardEvent KeyboardEvent; \
	pfnShadowUnicodeKeyboardEvent UnicodeKeyboardEvent; \
	pfnShadowMouseEvent MouseEvent; \
	pfnShadowExtendedMouseEvent ExtendedMouseEvent; \
	pfnShadowChannelAudinServerReceiveSamples AudinServerReceiveSamples; \
	\
	pfnShadowAuthenticate Authenticate; \
	pfnShadowClientConnect ClientConnect; \
	pfnShadowClientDisconnect ClientDisconnect; \
	pfnShadowClientCapabilities ClientCapabilities; \
	\
	rdpShadowServer* server

struct rdp_shadow_subsystem
{
	RDP_SHADOW_SUBSYSTEM_COMMON();
};

/* Definition of message between subsystem and clients */
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

typedef struct _SHADOW_MSG_OUT SHADOW_MSG_OUT;
typedef void (*MSG_OUT_FREE_FN)(UINT32 id, SHADOW_MSG_OUT* msg); /* function to free SHADOW_MSG_OUT */
#define RDP_SHADOW_MSG_OUT_COMMON() \
	int refCount; \
	MSG_OUT_FREE_FN Free

struct _SHADOW_MSG_OUT
{
	RDP_SHADOW_MSG_OUT_COMMON();
};

#define SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID		2001
#define SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID			2002
#define SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES_ID				2003
#define SHADOW_MSG_OUT_AUDIO_OUT_VOLUME_ID				2004

struct _SHADOW_MSG_OUT_POINTER_POSITION_UPDATE
{
	RDP_SHADOW_MSG_OUT_COMMON();
	UINT32 xPos;
	UINT32 yPos;
};
typedef struct _SHADOW_MSG_OUT_POINTER_POSITION_UPDATE SHADOW_MSG_OUT_POINTER_POSITION_UPDATE;

struct _SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE
{
	RDP_SHADOW_MSG_OUT_COMMON();
	UINT32 xHot;
	UINT32 yHot;
	UINT32 width;
	UINT32 height;
	UINT32 lengthAndMask;
	UINT32 lengthXorMask;
	BYTE* xorMaskData;
	BYTE* andMaskData;
};
typedef struct _SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE;

struct _SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES
{
	RDP_SHADOW_MSG_OUT_COMMON();
	AUDIO_FORMAT audio_format;
	void* buf;
	int nFrames;
	UINT16 wTimestamp;
};
typedef struct _SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES;

struct _SHADOW_MSG_OUT_AUDIO_OUT_VOLUME
{
	RDP_SHADOW_MSG_OUT_COMMON();
	int left;
	int right;
};
typedef struct _SHADOW_MSG_OUT_AUDIO_OUT_VOLUME SHADOW_MSG_OUT_AUDIO_OUT_VOLUME;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API void shadow_subsystem_set_entry_builtin(const char* name);
FREERDP_API void shadow_subsystem_set_entry(pfnShadowSubsystemEntry pEntry);

FREERDP_API int shadow_subsystem_pointer_convert_alpha_pointer_data(BYTE* pixels, BOOL premultiplied,
		UINT32 width, UINT32 height, SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* pointerColor);

FREERDP_API int shadow_server_parse_command_line(rdpShadowServer* server, int argc, char** argv);
FREERDP_API int shadow_server_command_line_status_print(rdpShadowServer* server, int argc, char** argv, int status);

FREERDP_API int shadow_server_start(rdpShadowServer* server);
FREERDP_API int shadow_server_stop(rdpShadowServer* server);

FREERDP_API int shadow_server_init(rdpShadowServer* server);
FREERDP_API int shadow_server_uninit(rdpShadowServer* server);

FREERDP_API int shadow_enum_monitors(MONITOR_DEF* monitors, int maxMonitors);

FREERDP_API rdpShadowServer* shadow_server_new();
FREERDP_API void shadow_server_free(rdpShadowServer* server);

FREERDP_API int shadow_capture_align_clip_rect(RECTANGLE_16* rect, RECTANGLE_16* clip);
FREERDP_API int shadow_capture_compare(BYTE* pData1, int nStep1, int nWidth, int nHeight, BYTE* pData2, int nStep2, RECTANGLE_16* rect);

FREERDP_API void shadow_subsystem_frame_update(rdpShadowSubsystem* subsystem);

FREERDP_API BOOL shadow_client_post_msg(rdpShadowClient* client, void* context, UINT32 type, SHADOW_MSG_OUT* msg, void* lParam);
FREERDP_API int shadow_client_boardcast_msg(rdpShadowServer* server, void* context, UINT32 type, SHADOW_MSG_OUT* msg, void* lParam);
FREERDP_API int shadow_client_boardcast_quit(rdpShadowServer* server, int nExitCode);

FREERDP_API int shadow_encoder_preferred_fps(rdpShadowEncoder* encoder);
FREERDP_API UINT32 shadow_encoder_inflight_frames(rdpShadowEncoder* encoder);

FREERDP_API BOOL shadow_screen_resize(rdpShadowScreen* screen);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_H */


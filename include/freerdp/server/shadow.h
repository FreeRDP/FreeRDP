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
#include <freerdp/server/rdpgfx.h>

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

typedef UINT32 (*pfnShadowEnumMonitors)(MONITOR_DEF* monitors, UINT32 maxMonitors);

typedef int (*pfnShadowAuthenticate)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                     const char* user, const char* domain, const char* password);
typedef BOOL (*pfnShadowClientConnect)(rdpShadowSubsystem* subsystem, rdpShadowClient* client);
typedef void (*pfnShadowClientDisconnect)(rdpShadowSubsystem* subsystem, rdpShadowClient* client);
typedef BOOL (*pfnShadowClientCapabilities)(rdpShadowSubsystem* subsystem, rdpShadowClient* client);

typedef BOOL (*pfnShadowSynchronizeEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                          UINT32 flags);
typedef BOOL (*pfnShadowKeyboardEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                       UINT16 flags, UINT16 code);
typedef BOOL (*pfnShadowUnicodeKeyboardEvent)(rdpShadowSubsystem* subsystem,
                                              rdpShadowClient* client, UINT16 flags, UINT16 code);
typedef BOOL (*pfnShadowMouseEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                    UINT16 flags, UINT16 x, UINT16 y);
typedef BOOL (*pfnShadowExtendedMouseEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                            UINT16 flags, UINT16 x, UINT16 y);

typedef BOOL (*pfnShadowChannelAudinServerReceiveSamples)(rdpShadowSubsystem* subsystem,
                                                          rdpShadowClient* client,
                                                          const AUDIO_FORMAT* format, wStream* buf,
                                                          size_t nframes);

struct rdp_shadow_client
{
	rdpContext context;

	HANDLE thread;
	BOOL activated;
	BOOL inLobby;
	BOOL mayView;
	BOOL mayInteract;
	BOOL suppressOutput;
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
	RdpgfxServerContext* rdpgfx;
};

struct rdp_shadow_server
{
	void* ext;
	HANDLE thread;
	HANDLE StopEvent;
	wArrayList* clients;
	rdpSettings* settings;
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

	/* Codec settings */
	RLGR_MODE rfxMode;
	H264_RATECONTROL_MODE h264RateControlMode;
	UINT32 h264BitRate;
	FLOAT h264FrameRate;
	UINT32 h264QP;

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
	DWORD format;
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

struct rdp_shadow_subsystem
{
	RDP_SHADOW_ENTRY_POINTS ep;
	HANDLE event;
	int numMonitors;
	int captureFrameRate;
	int selectedMonitor;
	MONITOR_DEF monitors[16];
	MONITOR_DEF virtualScreen;

	/* This event indicates that we have graphic change */
	/* such as screen update and resize. It should not be */
	/* used by subsystem implementation directly */
	rdpShadowMultiClientEvent* updateEvent;

	wMessagePipe* MsgPipe;
	UINT32 pointerX;
	UINT32 pointerY;

	AUDIO_FORMAT* rdpsndFormats;
	size_t nRdpsndFormats;
	AUDIO_FORMAT* audinFormats;
	size_t nAudinFormats;

	pfnShadowSynchronizeEvent SynchronizeEvent;
	pfnShadowKeyboardEvent KeyboardEvent;
	pfnShadowUnicodeKeyboardEvent UnicodeKeyboardEvent;
	pfnShadowMouseEvent MouseEvent;
	pfnShadowExtendedMouseEvent ExtendedMouseEvent;
	pfnShadowChannelAudinServerReceiveSamples AudinServerReceiveSamples;

	pfnShadowAuthenticate Authenticate;
	pfnShadowClientConnect ClientConnect;
	pfnShadowClientDisconnect ClientDisconnect;
	pfnShadowClientCapabilities ClientCapabilities;

	rdpShadowServer* server;
};

/* Definition of message between subsystem and clients */
#define SHADOW_MSG_IN_REFRESH_REQUEST_ID 1001

typedef struct _SHADOW_MSG_OUT SHADOW_MSG_OUT;
typedef void (*MSG_OUT_FREE_FN)(UINT32 id,
                                SHADOW_MSG_OUT* msg); /* function to free SHADOW_MSG_OUT */

struct _SHADOW_MSG_OUT
{
	int refCount;
	MSG_OUT_FREE_FN Free;
};

#define SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID 2001
#define SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID 2002
#define SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES_ID 2003
#define SHADOW_MSG_OUT_AUDIO_OUT_VOLUME_ID 2004

struct _SHADOW_MSG_OUT_POINTER_POSITION_UPDATE
{
	SHADOW_MSG_OUT common;
	UINT32 xPos;
	UINT32 yPos;
};
typedef struct _SHADOW_MSG_OUT_POINTER_POSITION_UPDATE SHADOW_MSG_OUT_POINTER_POSITION_UPDATE;

struct _SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE
{
	SHADOW_MSG_OUT common;
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
	SHADOW_MSG_OUT common;
	AUDIO_FORMAT* audio_format;
	void* buf;
	int nFrames;
	UINT16 wTimestamp;
};
typedef struct _SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES;

struct _SHADOW_MSG_OUT_AUDIO_OUT_VOLUME
{
	SHADOW_MSG_OUT common;
	int left;
	int right;
};
typedef struct _SHADOW_MSG_OUT_AUDIO_OUT_VOLUME SHADOW_MSG_OUT_AUDIO_OUT_VOLUME;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API void shadow_subsystem_set_entry_builtin(const char* name);
	FREERDP_API void shadow_subsystem_set_entry(pfnShadowSubsystemEntry pEntry);

	FREERDP_API int shadow_subsystem_pointer_convert_alpha_pointer_data(
	    BYTE* pixels, BOOL premultiplied, UINT32 width, UINT32 height,
	    SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* pointerColor);

	FREERDP_API int shadow_server_parse_command_line(rdpShadowServer* server, int argc,
	                                                 char** argv);
	FREERDP_API int shadow_server_command_line_status_print(rdpShadowServer* server, int argc,
	                                                        char** argv, int status);

	FREERDP_API int shadow_server_start(rdpShadowServer* server);
	FREERDP_API int shadow_server_stop(rdpShadowServer* server);

	FREERDP_API int shadow_server_init(rdpShadowServer* server);
	FREERDP_API int shadow_server_uninit(rdpShadowServer* server);

	FREERDP_API UINT32 shadow_enum_monitors(MONITOR_DEF* monitors, UINT32 maxMonitors);

	FREERDP_API rdpShadowServer* shadow_server_new(void);
	FREERDP_API void shadow_server_free(rdpShadowServer* server);

	FREERDP_API int shadow_capture_align_clip_rect(RECTANGLE_16* rect, RECTANGLE_16* clip);
	FREERDP_API int shadow_capture_compare(BYTE* pData1, UINT32 nStep1, UINT32 nWidth,
	                                       UINT32 nHeight, BYTE* pData2, UINT32 nStep2,
	                                       RECTANGLE_16* rect);

	FREERDP_API void shadow_subsystem_frame_update(rdpShadowSubsystem* subsystem);

	FREERDP_API BOOL shadow_client_post_msg(rdpShadowClient* client, void* context, UINT32 type,
	                                        SHADOW_MSG_OUT* msg, void* lParam);
	FREERDP_API int shadow_client_boardcast_msg(rdpShadowServer* server, void* context, UINT32 type,
	                                            SHADOW_MSG_OUT* msg, void* lParam);
	FREERDP_API int shadow_client_boardcast_quit(rdpShadowServer* server, int nExitCode);

	FREERDP_API int shadow_encoder_preferred_fps(rdpShadowEncoder* encoder);
	FREERDP_API UINT32 shadow_encoder_inflight_frames(rdpShadowEncoder* encoder);

	FREERDP_API BOOL shadow_screen_resize(rdpShadowScreen* screen);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_H */

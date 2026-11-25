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

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/listener.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>

#include <freerdp/server/encomsp.h>
#include <freerdp/server/remdesk.h>
#include <freerdp/server/rdpsnd.h>
#if defined(CHANNEL_AUDIN_SERVER)
#include <freerdp/server/audin.h>
#endif
#include <freerdp/server/disp.h>
#if defined(CHANNEL_CLIPRDR_SERVER)
#include <freerdp/server/cliprdr.h>
#endif
#include <freerdp/server/rdpgfx.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/collections.h>
#include <winpr/cmdline.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rdp_shadow_client rdpShadowClient;
	typedef struct rdp_shadow_server rdpShadowServer;
	typedef struct rdp_shadow_screen rdpShadowScreen;
	typedef struct rdp_shadow_surface rdpShadowSurface;
	typedef struct rdp_shadow_encoder rdpShadowEncoder;
	typedef struct rdp_shadow_capture rdpShadowCapture;
	typedef struct rdp_shadow_subsystem rdpShadowSubsystem;
	typedef struct rdp_shadow_multiclient_event rdpShadowMultiClientEvent;
	typedef struct rdp_shadow_clipboard rdpShadowClipboard; /** @since version 3.34.0 */

	typedef struct S_RDP_SHADOW_ENTRY_POINTS RDP_SHADOW_ENTRY_POINTS;
	typedef int (*pfnShadowSubsystemEntry)(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);

	typedef rdpShadowSubsystem* (*pfnShadowSubsystemNew)(void);
	typedef void (*pfnShadowSubsystemFree)(rdpShadowSubsystem* subsystem);

	typedef int (*pfnShadowSubsystemInit)(rdpShadowSubsystem* subsystem);
	typedef int (*pfnShadowSubsystemUninit)(rdpShadowSubsystem* subsystem);

	typedef int (*pfnShadowSubsystemStart)(rdpShadowSubsystem* subsystem);
	typedef int (*pfnShadowSubsystemStop)(rdpShadowSubsystem* subsystem);

	typedef UINT32 (*pfnShadowEnumMonitors)(MONITOR_DEF* monitors, UINT32 maxMonitors);

	typedef int (*pfnShadowAuthenticate)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
	                                     const char* user, const char* domain,
	                                     const char* password);
	typedef BOOL (*pfnShadowClientConnect)(rdpShadowSubsystem* subsystem, rdpShadowClient* client);
	typedef void (*pfnShadowClientDisconnect)(rdpShadowSubsystem* subsystem,
	                                          rdpShadowClient* client);
	typedef BOOL (*pfnShadowClientCapabilities)(rdpShadowSubsystem* subsystem,
	                                            rdpShadowClient* client);
	/** The client asks for another desktop size. A subsystem that sets this offers Display
	 *  Control to its clients. Runs on the channel's thread and must not block.
	 *  @since version 3.34.0 */
	typedef BOOL (*pfnShadowClientResize)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
	                                      UINT32 width, UINT32 height);

	typedef BOOL (*pfnShadowSynchronizeEvent)(rdpShadowSubsystem* subsystem,
	                                          rdpShadowClient* client, UINT32 flags);
	typedef BOOL (*pfnShadowKeyboardEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
	                                       UINT16 flags, UINT8 code);
	typedef BOOL (*pfnShadowUnicodeKeyboardEvent)(rdpShadowSubsystem* subsystem,
	                                              rdpShadowClient* client, UINT16 flags,
	                                              UINT16 code);
	typedef BOOL (*pfnShadowMouseEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
	                                    UINT16 flags, UINT16 x, UINT16 y);
	typedef BOOL (*pfnShadowRelMouseEvent)(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
	                                       UINT16 flags, INT16 xDelta,
	                                       INT16 yDelta); /** @since version 3.15.0 */

	typedef BOOL (*pfnShadowExtendedMouseEvent)(rdpShadowSubsystem* subsystem,
	                                            rdpShadowClient* client, UINT16 flags, UINT16 x,
	                                            UINT16 y);

	typedef BOOL (*pfnShadowChannelAudinServerReceiveSamples)(rdpShadowSubsystem* subsystem,
	                                                          rdpShadowClient* client,
	                                                          const AUDIO_FORMAT* format,
	                                                          wStream* data);

	/** The clipboard of the session, for a subsystem that has one: see
	 *  shadow_clipboard_session_changed(). Each hook may run on any thread, must not block,
	 *  and must not call the shadow_clipboard_ functions before it returned: the work goes
	 *  to the subsystem's own thread.
	 *
	 *  @since version 3.34.0
	 */
#define SHADOW_CLIPBOARD_MAX_SIZE (256ull * 1024ull * 1024ull)

	/** A client copied: the session offers its data from now on, in these mime types. With
	 *  none, that client left or has nothing to offer: the session drops the offer. */
	typedef BOOL (*pfnShadowClipboardOffer)(rdpShadowSubsystem* subsystem,
	                                        const char* const* mimeTypes, size_t count);

	/** A client pastes: read the session's clipboard, and hand the data, or a failure, to
	 *  shadow_clipboard_session_data(). One read at a time. */
	typedef BOOL (*pfnShadowClipboardRead)(rdpShadowSubsystem* subsystem, const char* mimeType);

	/** The answer to shadow_clipboard_session_request(): the data, or nullptr on a failure.
	 *  It is only valid during the call. */
	typedef BOOL (*pfnShadowClipboardProvide)(rdpShadowSubsystem* subsystem, UINT32 request,
	                                          const BYTE* data, size_t size);

	struct rdp_shadow_client
	{
		rdpContext context;

		HANDLE thread;
		BOOL activated;
		BOOL first_frame;
		BOOL inLobby;
		BOOL mayView;
		BOOL mayInteract;
		BOOL suppressOutput;
		UINT16 surfaceId;
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
#if defined(CHANNEL_AUDIN_SERVER)
		audin_server_context* audin;
#endif
		RdpgfxServerContext* rdpgfx;
#if defined(CHANNEL_CLIPRDR_SERVER)
		CliprdrServerContext* cliprdr; /** @since version 3.33.1 */
#endif

		BOOL resizeRequested;
		UINT32 resizeWidth;
		UINT32 resizeHeight;
		BOOL areGfxCapsReady; /** @since version 3.3.0 */
		RDPGFX_CAPSET confirmedCaps; /** @since version 3.25.0 */
		/** sound samples and volume: a thread of their own sends them, so that a frame being
		 *  encoded does not hold the sound back
		 *  @since version 3.34.0 */
		wMessageQueue* SoundQueue;
		HANDLE soundThread;       /** @since version 3.34.0 */
		CRITICAL_SECTION vcmLock; /** one drain of the channel queue at a time
		                           *  @since version 3.34.0 */
		/** the keys this client pressed and has not released, one bit per scan code with its
		 *  KBD_FLAGS_EXTENDED (0x100) and KBD_FLAGS_EXTENDED1 (0x200) flags
		 *  @since version 3.34.0 */
		BYTE keysDown[128];
		DispServerContext* disp; /** @since version 3.34.0 */
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
		UINT32 selectedMonitor;
		RECTANGLE_16 subRect;

		/* Codec settings */
		RLGR_MODE rfxMode; /* unused */
		H264_RATECONTROL_MODE h264RateControlMode;
		UINT32 h264BitRate;
		UINT32 h264FrameRate;
		UINT32 h264QP;

		char* ipcSocket;
		char* ConfigPath;
		char* CertificateFile;
		char* PrivateKeyFile;
		CRITICAL_SECTION lock;
		freerdp_listener* listener;

		size_t maxClientsConnected;
		BOOL SupportMultiRectBitmapUpdates; /** @since version 3.13.0 */
		BOOL ShowMouseCursor;               /** @since version 3.15.0 */
#if defined(WITH_GFX_AV1)
		FREERDP_AV1_RATECONTROL AV1RateControlMode; /** @since version 3.15.0 */
		UINT32 AV1BitRate;                          /** @since version 3.15.0 */
#else
	    UINT32 reservedAV1[2];
#endif
		BOOL h264HwAccel; /** @since version 3.32.0 */
		/** how many clients hold each key of rdpShadowClient::keysDown, guarded by lock
		 *  @since version 3.34.0 */
		UINT16 keysHeld[0x400];
#if defined(WITH_GFX_AV2)
		FREERDP_AV2_RATECONTROL AV2RateControlMode; /** @since version 3.34.0 */
		UINT32 AV2BitRate;                          /** @since version 3.34.0 */
#else
	    UINT32 reservedAV2[2];
#endif
		rdpShadowClipboard* clipboard; /** @since version 3.34.0 */
#if defined(WITH_GFX_AZURE)
		FREERDP_HEVC_RATECONTROL HEVCRateControlMode; /** @since version 3.34.0 */
		UINT32 HEVCBitRate;                           /** @since version 3.34.0 */
#else
	    UINT32 reservedHEVC[2];
#endif
	};

	struct rdp_shadow_surface
	{
		rdpShadowServer* server;

		UINT16 x;
		UINT16 y;
		UINT32 width;
		UINT32 height;
		UINT32 scanline;
		DWORD format;
		BYTE* data;

		CRITICAL_SECTION lock;
		REGION16 invalidRegion;
	};

	struct S_RDP_SHADOW_ENTRY_POINTS
	{
		WINPR_ATTR_NODISCARD pfnShadowSubsystemNew New;
		pfnShadowSubsystemFree Free;

		WINPR_ATTR_NODISCARD pfnShadowSubsystemInit Init;
		pfnShadowSubsystemUninit Uninit;

		WINPR_ATTR_NODISCARD pfnShadowSubsystemStart Start;
		pfnShadowSubsystemStop Stop;

		WINPR_ATTR_NODISCARD pfnShadowEnumMonitors EnumMonitors;
	};

	struct rdp_shadow_subsystem
	{
		RDP_SHADOW_ENTRY_POINTS ep;
		HANDLE event;
		UINT32 numMonitors;
		UINT32 captureFrameRate;
		UINT32 selectedMonitor;
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

		WINPR_ATTR_NODISCARD pfnShadowSynchronizeEvent SynchronizeEvent;
		WINPR_ATTR_NODISCARD pfnShadowKeyboardEvent KeyboardEvent;
		WINPR_ATTR_NODISCARD pfnShadowUnicodeKeyboardEvent UnicodeKeyboardEvent;
		WINPR_ATTR_NODISCARD pfnShadowMouseEvent MouseEvent;
		WINPR_ATTR_NODISCARD pfnShadowExtendedMouseEvent ExtendedMouseEvent;
		WINPR_ATTR_NODISCARD pfnShadowChannelAudinServerReceiveSamples AudinServerReceiveSamples;

		WINPR_ATTR_NODISCARD pfnShadowAuthenticate Authenticate;
		WINPR_ATTR_NODISCARD pfnShadowClientConnect ClientConnect;
		pfnShadowClientDisconnect ClientDisconnect;
		WINPR_ATTR_NODISCARD pfnShadowClientCapabilities ClientCapabilities;

		rdpShadowServer* server;

		WINPR_ATTR_NODISCARD pfnShadowRelMouseEvent RelMouseEvent; /** @since version 3.15.0 */

		/* all three, or a client gets no clipboard channel from the core */
		WINPR_ATTR_NODISCARD pfnShadowClipboardOffer ClipboardOffer; /** @since version 3.34.0 */
		WINPR_ATTR_NODISCARD pfnShadowClipboardRead ClipboardRead;   /** @since version 3.34.0 */
		WINPR_ATTR_NODISCARD pfnShadowClipboardProvide
		    ClipboardProvide; /** @since version 3.34.0 */

		WINPR_ATTR_NODISCARD pfnShadowClientResize ClientResize; /** @since version 3.34.0 */
	};

/* Definition of message between subsystem and clients */
#define SHADOW_MSG_IN_REFRESH_REQUEST_ID 1001

	typedef struct S_SHADOW_MSG_OUT SHADOW_MSG_OUT;
	typedef void (*MSG_OUT_FREE_FN)(UINT32 id,
	                                SHADOW_MSG_OUT* msg); /* function to free SHADOW_MSG_OUT */

	struct S_SHADOW_MSG_OUT
	{
		LONG refCount; /* InterlockedIncrement() and InterlockedDecrement() take a LONG* */
		MSG_OUT_FREE_FN Free;
	};

#define SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID 2001
#define SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID 2002
#define SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES_ID 2003
#define SHADOW_MSG_OUT_AUDIO_OUT_VOLUME_ID 2004

	typedef struct
	{
		SHADOW_MSG_OUT common;
		UINT32 xPos;
		UINT32 yPos;
	} SHADOW_MSG_OUT_POINTER_POSITION_UPDATE;

	typedef struct
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
	} SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE;

	typedef struct
	{
		SHADOW_MSG_OUT common;
		AUDIO_FORMAT* audio_format;
		void* buf;
		size_t nFrames;
		UINT16 wTimestamp;
	} SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES;

	typedef struct
	{
		SHADOW_MSG_OUT common;
		UINT16 left;
		UINT16 right;
	} SHADOW_MSG_OUT_AUDIO_OUT_VOLUME;

	FREERDP_API void shadow_subsystem_set_entry_builtin(const char* name);
	FREERDP_API void shadow_subsystem_set_entry(pfnShadowSubsystemEntry pEntry);

	/** @brief Add a subsystem to the ones the server may use.
	 *
	 *  When more than one is added (and none is set with \b shadow_subsystem_set_entry), the
	 *  server uses the first that finds a monitor, in the order they were added. The command
	 *  line option \b /subsystem changes that order.
	 *
	 *  @param name The name \b /subsystem knows it by
	 *  @param pEntry The subsystem's entry function
	 *
	 *  @return \b TRUE for success, \b FALSE for an invalid argument or too many subsystems
	 *
	 *  @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL shadow_subsystem_add_entry(const char* name, pfnShadowSubsystemEntry pEntry);

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
	WINPR_DEPRECATED_VAR(
	    "[since 3.4.0] Use shadow_subsystem_pointer_convert_alpha_pointer_data_to_format instead",
	    WINPR_ATTR_NODISCARD FREERDP_API int shadow_subsystem_pointer_convert_alpha_pointer_data(
	        const BYTE* WINPR_RESTRICT pixels, BOOL premultiplied, UINT32 width, UINT32 height,
	        SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* WINPR_RESTRICT pointerColor));
#endif

	/** @brief Convert a pointer image from input format to RDP specific encoding
	 *
	 *  @param pixels A pointer to the pixel data
	 *  @param format The pixel format of the pointer image
	 *  @param premultiplied Premultiplied format, requires scaling of pixel colors
	 *  @param width The width in pixels of the pointer
	 *  @param height The height of the pointer
	 *  @param pointerColor A pointer to the struct that can hold the encoded data
	 *
	 *  @return \b >=0 for success, \b <0 for any failure
	 *
	 *  @since version 3.4.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API int shadow_subsystem_pointer_convert_alpha_pointer_data_to_format(
	    const BYTE* WINPR_RESTRICT pixels, UINT32 format, BOOL premultiplied, UINT32 width,
	    UINT32 height, SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* WINPR_RESTRICT pointerColor);

	WINPR_ATTR_NODISCARD
	FREERDP_API int shadow_server_parse_command_line(rdpShadowServer* server, int argc, char** argv,
	                                                 COMMAND_LINE_ARGUMENT_A* cargs);

	WINPR_ATTR_NODISCARD
	FREERDP_API int shadow_server_command_line_status_print(rdpShadowServer* server, int argc,
	                                                        char** argv, int status,
	                                                        const COMMAND_LINE_ARGUMENT_A* cargs);

	WINPR_ATTR_NODISCARD
	FREERDP_API int shadow_server_start(rdpShadowServer* server);

	FREERDP_API int shadow_server_stop(rdpShadowServer* server);

	WINPR_ATTR_NODISCARD
	FREERDP_API int shadow_server_init(rdpShadowServer* server);

	FREERDP_API int shadow_server_uninit(rdpShadowServer* server);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT32 shadow_enum_monitors(MONITOR_DEF* monitors, UINT32 maxMonitors);

	FREERDP_API void shadow_server_free(rdpShadowServer* server);

	WINPR_ATTR_MALLOC(shadow_server_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API rdpShadowServer* shadow_server_new(void);

	WINPR_ATTR_NODISCARD
	FREERDP_API int shadow_capture_align_clip_rect(RECTANGLE_16* rect, const RECTANGLE_16* clip);

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
	WINPR_DEPRECATED_VAR("[since 3.4.0] Use shadow_capture_compare_with_format",
	                     WINPR_ATTR_NODISCARD FREERDP_API int shadow_capture_compare(
	                         const BYTE* WINPR_RESTRICT pData1, UINT32 nStep1, UINT32 nWidth,
	                         UINT32 nHeight, const BYTE* WINPR_RESTRICT pData2, UINT32 nStep2,
	                         RECTANGLE_16* WINPR_RESTRICT rect));
#endif

	/** @brief Compare two framebuffer images of possibly different formats with each other
	 *
	 *  @param pData1  A pointer to the data of image 1
	 *  @param format1 The format of image 1
	 *  @param nStep1  The line width in bytes of image 1
	 *  @param nWidth  The line width in pixels of image 1
	 *  @param nHeight The height of image 1
	 *  @param pData2  A pointer to the data of image 2
	 *  @param format2 The format of image 2
	 *  @param nStep2  The line width in bytes of image 2
	 *  @param rect A pointer to the rectangle of the images to compare
	 *
	 *  @return \b 0 if equal, \b >0 if not equal and \b <0 for any error
	 *
	 *  @since version 3.4.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API int shadow_capture_compare_with_format(const BYTE* WINPR_RESTRICT pData1,
	                                                   UINT32 format1, UINT32 nStep1, UINT32 nWidth,
	                                                   UINT32 nHeight,
	                                                   const BYTE* WINPR_RESTRICT pData2,
	                                                   UINT32 format2, UINT32 nStep2,
	                                                   RECTANGLE_16* WINPR_RESTRICT rect);

	FREERDP_API void shadow_subsystem_frame_update(rdpShadowSubsystem* subsystem);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL shadow_client_post_msg(rdpShadowClient* client, void* context, UINT32 type,
	                                        SHADOW_MSG_OUT* msg, void* lParam);

	WINPR_ATTR_NODISCARD
	FREERDP_API int shadow_client_boardcast_msg(rdpShadowServer* server, void* context, UINT32 type,
	                                            SHADOW_MSG_OUT* msg, void* lParam);

	WINPR_ATTR_NODISCARD
	FREERDP_API int shadow_client_boardcast_quit(rdpShadowServer* server, int nExitCode);

	/** The session's clipboard changed, and not through ClipboardOffer.
	 *
	 *  @param server The server
	 *  @param mimeTypes What the session offers now
	 *  @param count The number of mime types, 0 for an empty clipboard
	 *
	 *  @return \b TRUE for success, \b FALSE otherwise
	 *  @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL shadow_clipboard_session_changed(rdpShadowServer* server,
	                                                  const char* const* mimeTypes, size_t count);

	/** The answer to ClipboardRead.
	 *
	 *  @param server The server
	 *  @param data What the session holds in that mime type, nullptr on a failure
	 *  @param size The size of \b data
	 *
	 *  @return \b TRUE for success, \b FALSE when no read was waiting
	 *  @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL shadow_clipboard_session_data(rdpShadowServer* server, const BYTE* data,
	                                               size_t size);

	/** The session pastes what a client offered: ClipboardProvide gets the answer.
	 *
	 *  @param server The server
	 *  @param request Any number of the subsystem's, ClipboardProvide gets it back
	 *  @param mimeType One of the mime types of ClipboardOffer
	 *
	 *  @return \b TRUE for success, \b FALSE otherwise
	 *  @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL shadow_clipboard_session_request(rdpShadowServer* server, UINT32 request,
	                                                  const char* mimeType);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT32 shadow_encoder_preferred_fps(rdpShadowEncoder* encoder);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT32 shadow_encoder_inflight_frames(rdpShadowEncoder* encoder);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL shadow_screen_resize(rdpShadowScreen* screen);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_H */

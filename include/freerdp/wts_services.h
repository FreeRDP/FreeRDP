/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client services for virtual channel plugins (Microsoft COM interfaces)
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_WTS_SERVICES_H
#define FREERDP_WTS_SERVICES_H

#include <winpr/com.h>

/** @file
 *  The services Microsoft's clients (mstscax.dll, rdclientax.dll) offer to virtual channel
 *  plugins through IWTSPluginServiceProvider::GetService(serviceKey), with their exact binary
 *  layout, so that the same objects serve FreeRDP's own channels and native plugins on Windows.
 *
 *  The names follow the Windows SDK (tsvirtualchannels.h) where it has them. That header is not
 *  included anywhere: it also defines IWTSVirtualChannelManager & co, which clash with
 *  freerdp/dvc.h.
 *
 *  The interfaces are only declared in C form (see winpr/com.h), in C and in C++: their vtables
 *  start with a wIUnknownVtbl `iunknown`, called with the WINPR_COM_* macros, and the other
 *  methods are called with ->lpVtbl->Method(This, ...). The objects GetService
 *  hands out are wIUnknown*: in C++ that is the C++ form, called directly.
 *
 *  Layouts are x64 ones. IWTSWindowInfoService is undocumented and only exists in rdclientax.dll
 *  (Windows App / AVD client), not in mstscax.dll.
 *
 *  Where they live (rdp_context in freerdp/freerdp.h):
 *  - the client implements IWTSWindowInfoService and IWTSBitmapRenderService with the behaviour
 *    described below, and stores one of each in rdpContext::windowInfoService and
 *    rdpContext::bitmapRenderService (holding one reference) before connecting. It releases them
 *    and resets the fields to nullptr in its ContextFree. Channels and plugins may hold objects
 *    of it after that: they must then answer E_UNEXPECTED instead of using the client's state.
 *    The core is the only subscriber of the client's window info service.
 *  - consumers are virtual channel plugins: FreeRDP's own channels, or channels built and shipped
 *    separately against the installed headers. They get the provider with
 *    freerdp_get_plugin_service_provider() from the rdpContext of their entry points, call
 *    GetService() and QueryInterface() the result, as with Microsoft's clients. Only these
 *    interfaces and the provider are needed: a plugin needs no access to the client's
 *    internals. A service the client didn't set answers E_NOINTERFACE. Each GetService of the
 *    window info service gives a new object with its own subscriber (see below).
 *  - native Windows plugins expect the provider by QueryInterface(IID_IWTSPluginServiceProvider)
 *    on the IWTSVirtualChannelManager given to IWTSPlugin::Initialize: a loader for them can hand
 *    out the same object.
 *
 *  Threads: the services may be called from any thread (a channel's own, a decoder's). The
 *  client's implementations must be thread safe and must not block for long in Render().
 *
 *  @since version 3.34.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct IWTSPluginServiceProvider IWTSPluginServiceProvider;
	typedef struct IWTSBitmapRenderer IWTSBitmapRenderer;
	typedef struct IWTSBitmapRendererCallback IWTSBitmapRendererCallback;
	typedef struct IWTSBitmapRenderService IWTSBitmapRenderService;
	typedef struct IWTSWindowInfoService IWTSWindowInfoService;
	typedef struct IWTSWindowChangedCallback IWTSWindowChangedCallback;

	/* ======================= IWTSPluginServiceProvider ======================= */

	/* obtained by QueryInterface on the IWTSVirtualChannelManager given to IWTSPlugin::Initialize
	 */
	WINPR_COM_DEFINE_GUID(IID_IWTSPluginServiceProvider, 0xd3e07363, 0x087c, 0x476c, 0x86, 0xa7,
	                      0xdb, 0xb1, 0x5f, 0x46, 0xdd, 0xb4);

	typedef struct IWTSPluginServiceProviderVtbl
	{
		wIUnknownVtbl iunknown;

		/* the service key is passed BY VALUE (SDK and mstscax agree; on x64 it ends up behind a
		 * pointer anyway, on x86 the 16 bytes are on the stack). The service returned is not
		 * necessarily of the type the key suggests: callers QueryInterface it */
		HRESULT(WINPR_COM_CALL* GetService)
		(IWTSPluginServiceProvider* This, GUID ServiceId, wIUnknown** ppunkObject);
	} IWTSPluginServiceProviderVtbl;

	struct IWTSPluginServiceProvider
	{
		const IWTSPluginServiceProviderVtbl* lpVtbl;
	};

	/* ======================= bitmap render service ======================= */

	/* GetService key (SDK) */
	WINPR_COM_DEFINE_GUID(RDCLIENT_BITMAP_RENDER_SERVICE, 0xe4cc08cb, 0x942e, 0x4b19, 0x85, 0x04,
	                      0xbd, 0x5a, 0x89, 0xa7, 0x47, 0xf5);

	WINPR_COM_DEFINE_GUID(IID_IWTSBitmapRenderService, 0xea326091, 0x05fe, 0x40c1, 0xb4, 0x9c, 0x3d,
	                      0x2e, 0xf4, 0x62, 0x6a, 0x0e);
	WINPR_COM_DEFINE_GUID(IID_IWTSBitmapRenderer, 0x5b7acc97, 0xf3c9, 0x46f7, 0x8c, 0x5b, 0xfa,
	                      0x68, 0x5d, 0x34, 0x41, 0xb1);
	WINPR_COM_DEFINE_GUID(IID_IWTSBitmapRendererCallback, 0xd782928e, 0xfe4e, 0x4e77, 0xae, 0x90,
	                      0x9c, 0xd0, 0xb3, 0xe3, 0xb3, 0x53);

	typedef struct
	{
		DWORD dwFramesDelivered;
		DWORD dwFramesDropped;
	} BITMAP_RENDERER_STATISTICS;

	/** @brief same layout as Win32's RECT, which winpr doesn't have on POSIX */
	typedef struct
	{
		LONG left;
		LONG top;
		LONG right;
		LONG bottom;
	} WTS_RECT;

	typedef struct IWTSBitmapRendererVtbl
	{
		wIUnknownVtbl iunknown;

		/* imageFormat: a Media Foundation subtype; GUID_NULL for BGRA32 */
		HRESULT(WINPR_COM_CALL* Render)
		(IWTSBitmapRenderer* This, GUID imageFormat, DWORD dwWidth, DWORD dwHeight, LONG cbStride,
		 DWORD cbImageBuffer, BYTE* pImageBuffer);
		HRESULT(WINPR_COM_CALL* GetRendererStatistics)
		(IWTSBitmapRenderer* This, BITMAP_RENDERER_STATISTICS* pStatistics);
		HRESULT(WINPR_COM_CALL* RemoveMapping)(IWTSBitmapRenderer* This);
	} IWTSBitmapRendererVtbl;

	struct IWTSBitmapRenderer
	{
		const IWTSBitmapRendererVtbl* lpVtbl;
	};

	typedef struct IWTSBitmapRendererCallbackVtbl
	{
		wIUnknownVtbl iunknown;

		/* RECT by value (SDK), left/top/right/bottom */
		HRESULT(WINPR_COM_CALL* OnTargetSizeChanged)
		(IWTSBitmapRendererCallback* This, WTS_RECT rcNewSize);
	} IWTSBitmapRendererCallbackVtbl;

	struct IWTSBitmapRendererCallback
	{
		const IWTSBitmapRendererCallbackVtbl* lpVtbl;
	};

	typedef struct IWTSBitmapRenderServiceVtbl
	{
		wIUnknownVtbl iunknown;

		/* mappingId: a MS-RDPEGT geometry mapping id, given by the server */
		HRESULT(WINPR_COM_CALL* GetMappedRenderer)
		(IWTSBitmapRenderService* This, UINT64 mappingId,
		 IWTSBitmapRendererCallback* pMappedRendererCallback,
		 IWTSBitmapRenderer** ppMappedRenderer);
	} IWTSBitmapRenderServiceVtbl;

	struct IWTSBitmapRenderService
	{
		const IWTSBitmapRenderServiceVtbl* lpVtbl;
	};

	/* ======================= window info service (rdclientax.dll only) =======================
	 *
	 * Implemented in rdclientax.dll (WTSWindowInfoService). Behaviour of Microsoft's
	 * implementation, which a compatible one should keep:
	 * - GetService creates a new service object on each call;
	 * - one subscriber per service object: a second Subscribe fails with
	 *   HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), Unsubscribe without one with
	 *   HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	 * - Subscribe/Unsubscribe are synchronous (run on the client's core thread, waited for);
	 * - OnWindowChanged gets a copy of the new WTS_WINDOW_INFO, valid during the call only. */

	/* GetService key (named by us, undocumented) */
	WINPR_COM_DEFINE_GUID(RDCLIENT_WINDOW_INFO_SERVICE, 0xa9a17a6b, 0xcdb3, 0x4e81, 0xb5, 0xc8,
	                      0xbd, 0x63, 0x82, 0x2a, 0x8d, 0x6c);
	/* what the service is then queried for */
	WINPR_COM_DEFINE_GUID(IID_IWTSWindowInfoService, 0x2a002c7b, 0x0120, 0x4d04, 0xbf, 0xb1, 0x3c,
	                      0x73, 0x11, 0x0c, 0x85, 0x81);

	typedef enum
	{
		WTS_RDP_SESSION_TYPE_DESKTOP = 0,
		WTS_RDP_SESSION_TYPE_REMOTEAPP = 1, /* the client's "RailMode" property */
	} WTS_RDP_SESSION_TYPE;

	/** @brief 40 bytes on x64 */
	typedef struct
	{
		/* desktop: the client window showing the session; RemoteApp: the local window of the
		 * remote window asked for */
		HWND hwnd;
		/* desktop: size of the remote desktop; RemoteApp: client size of the local window */
		INT32 remoteHeight;
		INT32 remoteWidth;
		/* client area of hwnd */
		INT32 viewWidth;
		INT32 viewHeight;
		/* scrollbar positions (0 if none, always 0 in RemoteApp) */
		INT32 scrollX;
		INT32 scrollY;
		/* desktop: viewWidth / remoteWidth, 1.0 when scrollbars are shown; RemoteApp: 0 */
		float scale;
	} WTS_WINDOW_INFO;

	typedef struct IWTSWindowChangedCallbackVtbl
	{
		wIUnknownVtbl iunknown;

		/* windowInfo: the new window info, valid during the call only */
		HRESULT(WINPR_COM_CALL* OnWindowChanged)
		(IWTSWindowChangedCallback* This, const WTS_WINDOW_INFO* windowInfo);
	} IWTSWindowChangedCallbackVtbl;

	struct IWTSWindowChangedCallback
	{
		const IWTSWindowChangedCallbackVtbl* lpVtbl;
	};

	typedef struct IWTSWindowInfoServiceVtbl
	{
		wIUnknownVtbl iunknown;

		/* remoteHwnd: the remote window, only used in RemoteApp sessions */
		HRESULT(WINPR_COM_CALL* GetWindowInfo)
		(IWTSWindowInfoService* This, UINT64 remoteHwnd, WTS_WINDOW_INFO* windowInfo);
		/* the client's own process id */
		HRESULT(WINPR_COM_CALL* GetProcessId)(IWTSWindowInfoService* This, DWORD* processId);
		/* a WTS_RDP_SESSION_TYPE */
		HRESULT(WINPR_COM_CALL* GetRdpSessionType)
		(IWTSWindowInfoService* This, UINT32* sessionType);
		/* remoteHwnd (0 for a desktop session) is not used by Microsoft's client */
		HRESULT(WINPR_COM_CALL* SubscribeWindowChanged)
		(IWTSWindowInfoService* This, UINT64 remoteHwnd, IWTSWindowChangedCallback* callback);
		HRESULT(WINPR_COM_CALL* UnsubscribeWindowChanged)(IWTSWindowInfoService* This);
	} IWTSWindowInfoServiceVtbl;

	struct IWTSWindowInfoService
	{
		const IWTSWindowInfoServiceVtbl* lpVtbl;
	};

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_WTS_SERVICES_H */

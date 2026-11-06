/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Services for virtual channel plugins
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

#include <freerdp/config.h>

#include <stddef.h>

#include <winpr/assert.h>
#include <winpr/error.h>
#include <winpr/synch.h>
#include <winpr/wlog.h>

#include <freerdp/log.h>
#include <freerdp/wts_services.h>

#include "plugin_services.h"

#define TAG FREERDP_TAG("core.services")

/** @file
 *  rdpContext::pluginServiceProvider: GetService() as Microsoft's clients answer it, on top of
 *  the services the client put in the context.
 *
 *  Microsoft's GetService(RDCLIENT_WINDOW_INFO_SERVICE) creates a new IWTSWindowInfoService on
 *  each call, with one change subscriber per object (UnsubscribeWindowChanged() says nobody's).
 *  The client implements a single object with these rules; each caller gets a proxy here, and
 *  the proxies' subscriptions share one subscription to the client's object, made while at least
 *  one proxy is subscribed.
 *
 *  Locking: `lock` protects the context pointer and the subscriber list, and is never held while
 *  calling out. `subscribeLock` serializes the calls to the client's Subscribe/Unsubscribe
 *  (which Microsoft's client runs synchronously on its core thread); change notifications don't
 *  take it, so a client may notify from any thread while subscribing. */

typedef struct s_plugin_services CPluginServiceProvider;
typedef struct s_window_info_proxy CWindowInfoServiceProxy;
typedef struct s_window_changed_relay CWindowChangedRelay;

struct s_plugin_services
{
	wComBase com; /* IWTSPluginServiceProvider */
	CRITICAL_SECTION lock;
	CRITICAL_SECTION subscribeLock;
	rdpContext* context; /* nullptr once detached */

	/* the subscription to the client's window info service, while a proxy is subscribed */
	IWTSWindowInfoService* subscribed; /* a reference */
	CWindowChangedRelay* relay;        /* a reference */
	CWindowInfoServiceProxy* subscribers;
};

struct s_window_info_proxy
{
	wComBase com;                        /* IWTSWindowInfoService */
	CPluginServiceProvider* provider;    /* a reference */
	IWTSWindowChangedCallback* callback; /* a reference, while subscribed */
	CWindowInfoServiceProxy* next;       /* in provider->subscribers */
};

/* what the provider subscribes to the client's service with */
struct s_window_changed_relay
{
	wComBase com;                     /* IWTSWindowChangedCallback */
	CPluginServiceProvider* provider; /* a reference: the relay only lives while subscribed */
};

/* ============================ helpers ============================ */

/** @brief the client's window info service, a reference for the caller; nullptr if it has none
 *  or the provider is detached */
WINPR_ATTR_NODISCARD
static IWTSWindowInfoService* client_window_info(CPluginServiceProvider* sp)
{
	IWTSWindowInfoService* svc = nullptr;
	EnterCriticalSection(&sp->lock);
	if (sp->context && sp->context->windowInfoService)
	{
		svc = sp->context->windowInfoService;
		WINPR_COM_ADDREF(svc);
	}
	LeaveCriticalSection(&sp->lock);
	return svc;
}

/* ============================ CWindowChangedRelay ============================ */

WINPR_COM_IUNKNOWN_THUNKS(CWindowChangedRelay, nullptr)

static HRESULT WINPR_COM_CALL CWindowChangedRelay_OnWindowChanged(IWTSWindowChangedCallback* This,
                                                                  const WTS_WINDOW_INFO* windowInfo)
{
	CWindowChangedRelay* relay = (CWindowChangedRelay*)This;
	CPluginServiceProvider* sp = relay->provider;

	/* the callbacks are called without the lock: they may call the service back */
	size_t count = 0;
	EnterCriticalSection(&sp->lock);
	for (CWindowInfoServiceProxy* p = sp->subscribers; p; p = p->next)
		count++;
	IWTSWindowChangedCallback** callbacks =
	    count ? (IWTSWindowChangedCallback**)calloc(count, sizeof(IWTSWindowChangedCallback*))
	          : nullptr;
	if (callbacks)
	{
		size_t i = 0;
		for (CWindowInfoServiceProxy* p = sp->subscribers; p; p = p->next)
		{
			WINPR_COM_ADDREF(p->callback);
			callbacks[i++] = p->callback;
		}
	}
	LeaveCriticalSection(&sp->lock);
	if (count && !callbacks)
		return E_OUTOFMEMORY;

	for (size_t i = 0; i < count; i++)
	{
		(void)callbacks[i]->lpVtbl->OnWindowChanged(callbacks[i], windowInfo);
		WINPR_COM_RELEASE(callbacks[i]);
	}
	free((void*)callbacks);
	return S_OK;
}

static const IWTSWindowChangedCallbackVtbl CWindowChangedRelay_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(CWindowChangedRelay),
	.OnWindowChanged = CWindowChangedRelay_OnWindowChanged,
};

static void CWindowChangedRelay_free(wComBase* object)
{
	CWindowChangedRelay* relay = (CWindowChangedRelay*)object;
	winpr_com_base_release(&relay->provider->com);
	free(relay);
}

WINPR_ATTR_NODISCARD
static CWindowChangedRelay* CWindowChangedRelay_new(CPluginServiceProvider* sp)
{
	CWindowChangedRelay* relay = calloc(1, sizeof(*relay));
	if (!relay)
		return nullptr;
	winpr_com_base_init(&relay->com, "CWindowChangedRelay", &CWindowChangedRelay_vtbl.iunknown,
	                    CWindowChangedRelay_free);
	winpr_com_base_add_ref(&sp->com);
	relay->provider = sp;
	return relay;
}

/* ============================ shared subscription ============================ */

/** @brief subscribes to the client's service; subscribeLock held */
WINPR_ATTR_NODISCARD
static HRESULT shared_subscribe(CPluginServiceProvider* sp, UINT64 remoteHwnd)
{
	if (sp->subscribed)
		return S_OK;

	IWTSWindowInfoService* svc = client_window_info(sp);
	if (!svc)
		return E_UNEXPECTED;

	CWindowChangedRelay* relay = CWindowChangedRelay_new(sp);
	if (!relay)
	{
		WINPR_COM_RELEASE(svc);
		return E_OUTOFMEMORY;
	}

	/* Microsoft's client ignores remoteHwnd: the first subscriber's is as good as any */
	const HRESULT hr =
	    svc->lpVtbl->SubscribeWindowChanged(svc, remoteHwnd, (IWTSWindowChangedCallback*)relay);
	if (FAILED(hr))
	{
		WLog_WARN(TAG, "the client's SubscribeWindowChanged failed: 0x%08" PRIX32, (UINT32)hr);
		winpr_com_base_release(&relay->com);
		WINPR_COM_RELEASE(svc);
		return hr;
	}
	sp->subscribed = svc;
	sp->relay = relay;
	return S_OK;
}

/** @brief ends the subscription to the client's service, if any; subscribeLock held */
static void shared_unsubscribe(CPluginServiceProvider* sp)
{
	IWTSWindowInfoService* svc = sp->subscribed;
	CWindowChangedRelay* relay = sp->relay;
	sp->subscribed = nullptr;
	sp->relay = nullptr;
	if (svc)
	{
		const HRESULT hr = svc->lpVtbl->UnsubscribeWindowChanged(svc);
		if (FAILED(hr))
			WLog_WARN(TAG, "the client's UnsubscribeWindowChanged failed: 0x%08" PRIX32,
			          (UINT32)hr);
		WINPR_COM_RELEASE(svc);
	}
	if (relay)
		winpr_com_base_release(&relay->com);
}

/** @brief removes p from the subscribers; its callback is returned (the reference with it).
 *  subscribeLock held */
WINPR_ATTR_NODISCARD
static IWTSWindowChangedCallback* proxy_unlink(CWindowInfoServiceProxy* p, BOOL* last)
{
	CPluginServiceProvider* sp = p->provider;
	EnterCriticalSection(&sp->lock);
	IWTSWindowChangedCallback* callback = p->callback;
	p->callback = nullptr;
	for (CWindowInfoServiceProxy** it = &sp->subscribers; *it; it = &(*it)->next)
	{
		if (*it == p)
		{
			*it = p->next;
			break;
		}
	}
	p->next = nullptr;
	*last = (sp->subscribers == nullptr);
	LeaveCriticalSection(&sp->lock);
	return callback;
}

/* ============================ CWindowInfoServiceProxy ============================ */

WINPR_COM_IUNKNOWN_THUNKS(CWindowInfoServiceProxy, &IID_IWTSWindowInfoService)

static HRESULT WINPR_COM_CALL CWindowInfoServiceProxy_GetWindowInfo(IWTSWindowInfoService* This,
                                                                    UINT64 remoteHwnd,
                                                                    WTS_WINDOW_INFO* windowInfo)
{
	CWindowInfoServiceProxy* p = (CWindowInfoServiceProxy*)This;
	IWTSWindowInfoService* svc = client_window_info(p->provider);
	if (!svc)
		return E_UNEXPECTED;
	const HRESULT hr = svc->lpVtbl->GetWindowInfo(svc, remoteHwnd, windowInfo);
	WINPR_COM_RELEASE(svc);
	return hr;
}

static HRESULT WINPR_COM_CALL CWindowInfoServiceProxy_GetProcessId(IWTSWindowInfoService* This,
                                                                   DWORD* processId)
{
	CWindowInfoServiceProxy* p = (CWindowInfoServiceProxy*)This;
	IWTSWindowInfoService* svc = client_window_info(p->provider);
	if (!svc)
		return E_UNEXPECTED;
	const HRESULT hr = svc->lpVtbl->GetProcessId(svc, processId);
	WINPR_COM_RELEASE(svc);
	return hr;
}

static HRESULT WINPR_COM_CALL CWindowInfoServiceProxy_GetRdpSessionType(IWTSWindowInfoService* This,
                                                                        UINT32* sessionType)
{
	CWindowInfoServiceProxy* p = (CWindowInfoServiceProxy*)This;
	IWTSWindowInfoService* svc = client_window_info(p->provider);
	if (!svc)
		return E_UNEXPECTED;
	const HRESULT hr = svc->lpVtbl->GetRdpSessionType(svc, sessionType);
	WINPR_COM_RELEASE(svc);
	return hr;
}

static HRESULT WINPR_COM_CALL CWindowInfoServiceProxy_SubscribeWindowChanged(
    IWTSWindowInfoService* This, UINT64 remoteHwnd, IWTSWindowChangedCallback* callback)
{
	CWindowInfoServiceProxy* p = (CWindowInfoServiceProxy*)This;
	CPluginServiceProvider* sp = p->provider;
	if (!callback)
		return E_POINTER;

	EnterCriticalSection(&sp->subscribeLock);
	EnterCriticalSection(&sp->lock);
	const BOOL already = (p->callback != nullptr);
	const BOOL detached = (sp->context == nullptr);
	LeaveCriticalSection(&sp->lock);

	HRESULT hr = S_OK;
	if (already)
		hr = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
	else if (detached)
		hr = E_UNEXPECTED;
	else
		hr = shared_subscribe(sp, remoteHwnd);

	if (SUCCEEDED(hr))
	{
		WINPR_COM_ADDREF(callback);
		EnterCriticalSection(&sp->lock);
		p->callback = callback;
		p->next = sp->subscribers;
		sp->subscribers = p;
		LeaveCriticalSection(&sp->lock);
	}
	LeaveCriticalSection(&sp->subscribeLock);
	return hr;
}

static HRESULT unsubscribe(CWindowInfoServiceProxy* p)
{
	CPluginServiceProvider* sp = p->provider;

	EnterCriticalSection(&sp->subscribeLock);
	EnterCriticalSection(&sp->lock);
	const BOOL subscribed = (p->callback != nullptr);
	LeaveCriticalSection(&sp->lock);

	HRESULT hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	if (subscribed)
	{
		BOOL last = FALSE;
		IWTSWindowChangedCallback* callback = proxy_unlink(p, &last);
		if (last)
			shared_unsubscribe(sp);
		WINPR_COM_RELEASE(callback);
		hr = S_OK;
	}
	LeaveCriticalSection(&sp->subscribeLock);
	return hr;
}

static HRESULT WINPR_COM_CALL
CWindowInfoServiceProxy_UnsubscribeWindowChanged(IWTSWindowInfoService* This)
{
	return unsubscribe((CWindowInfoServiceProxy*)This);
}

static const IWTSWindowInfoServiceVtbl CWindowInfoServiceProxy_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(CWindowInfoServiceProxy),
	.GetWindowInfo = CWindowInfoServiceProxy_GetWindowInfo,
	.GetProcessId = CWindowInfoServiceProxy_GetProcessId,
	.GetRdpSessionType = CWindowInfoServiceProxy_GetRdpSessionType,
	.SubscribeWindowChanged = CWindowInfoServiceProxy_SubscribeWindowChanged,
	.UnsubscribeWindowChanged = CWindowInfoServiceProxy_UnsubscribeWindowChanged,
};

static void CWindowInfoServiceProxy_free(wComBase* object)
{
	CWindowInfoServiceProxy* p = (CWindowInfoServiceProxy*)object;
	/* released while subscribed: the subscription goes with it */
	(void)unsubscribe(p);
	winpr_com_base_release(&p->provider->com);
	free(p);
}

WINPR_ATTR_NODISCARD
static CWindowInfoServiceProxy* CWindowInfoServiceProxy_new(CPluginServiceProvider* sp)
{
	CWindowInfoServiceProxy* p = calloc(1, sizeof(*p));
	if (!p)
		return nullptr;
	winpr_com_base_init(&p->com, "CWindowInfoServiceProxy", &CWindowInfoServiceProxy_vtbl.iunknown,
	                    CWindowInfoServiceProxy_free);
	winpr_com_base_add_ref(&sp->com);
	p->provider = sp;
	return p;
}

/* ============================ CPluginServiceProvider ============================ */

WINPR_COM_IUNKNOWN_THUNKS(CPluginServiceProvider, &IID_IWTSPluginServiceProvider)

static HRESULT WINPR_COM_CALL CPluginServiceProvider_GetService(IWTSPluginServiceProvider* This,
                                                                GUID ServiceId,
                                                                wIUnknown** ppunkObject)
{
	CPluginServiceProvider* sp = (CPluginServiceProvider*)This;
	if (!ppunkObject)
		return E_POINTER;
	*ppunkObject = nullptr;

	HRESULT hr = E_NOINTERFACE;
	if (winpr_com_guid_equal(&ServiceId, &RDCLIENT_WINDOW_INFO_SERVICE))
	{
		IWTSWindowInfoService* svc = client_window_info(sp);
		if (svc)
		{
			WINPR_COM_RELEASE(svc);
			/* a new object for each caller, as Microsoft's client does */
			CWindowInfoServiceProxy* p = CWindowInfoServiceProxy_new(sp);
			if (p)
			{
				*ppunkObject = (wIUnknown*)p;
				hr = S_OK;
			}
			else
				hr = E_OUTOFMEMORY;
		}
	}
	else if (winpr_com_guid_equal(&ServiceId, &RDCLIENT_BITMAP_RENDER_SERVICE))
	{
		/* stateless: every renderer comes from GetMappedRenderer, the client's object is
		 * shared */
		EnterCriticalSection(&sp->lock);
		if (sp->context && sp->context->bitmapRenderService)
		{
			IWTSBitmapRenderService* svc = sp->context->bitmapRenderService;
			WINPR_COM_ADDREF(svc);
			*ppunkObject = (wIUnknown*)svc;
			hr = S_OK;
		}
		LeaveCriticalSection(&sp->lock);
	}

	WLog_DBG(TAG, "GetService(" WINPR_COM_GUID_FMT ") -> 0x%08" PRIX32,
	         WINPR_COM_GUID_ARGS(&ServiceId), (UINT32)hr);
	return hr;
}

static const IWTSPluginServiceProviderVtbl CPluginServiceProvider_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(CPluginServiceProvider),
	.GetService = CPluginServiceProvider_GetService,
};

static void CPluginServiceProvider_free(wComBase* object)
{
	CPluginServiceProvider* sp = (CPluginServiceProvider*)object;
	/* subscribed proxies and the relay hold references: nothing is left here */
	WINPR_ASSERT(!sp->subscribed);
	WINPR_ASSERT(!sp->subscribers);
	DeleteCriticalSection(&sp->subscribeLock);
	DeleteCriticalSection(&sp->lock);
	free(sp);
}

IWTSPluginServiceProvider* plugin_services_new(rdpContext* context)
{
	WINPR_ASSERT(context);

	/* the rdp_context slots are ABI: 64 bit each, whatever the pointer size */
	WINPR_STATIC_ASSERT(offsetof(rdpContext, windowInfoService) == 46ull * 8ull);
	WINPR_STATIC_ASSERT(offsetof(rdpContext, bitmapRenderService) == 47ull * 8ull);
	WINPR_STATIC_ASSERT(offsetof(rdpContext, pluginServiceProvider) == 48ull * 8ull);
	WINPR_STATIC_ASSERT(offsetof(rdpContext, dump) == 64ull * 8ull);

	/* the vtables have the flat layout of MIDL's: IUnknown's three entries, then the others */
	WINPR_STATIC_ASSERT(offsetof(IWTSPluginServiceProviderVtbl, GetService) == 3 * sizeof(void*));
	WINPR_STATIC_ASSERT(offsetof(IWTSBitmapRendererVtbl, Render) == 3 * sizeof(void*));
	WINPR_STATIC_ASSERT(offsetof(IWTSBitmapRendererCallbackVtbl, OnTargetSizeChanged) ==
	                    3 * sizeof(void*));
	WINPR_STATIC_ASSERT(offsetof(IWTSBitmapRenderServiceVtbl, GetMappedRenderer) ==
	                    3 * sizeof(void*));
	WINPR_STATIC_ASSERT(offsetof(IWTSWindowChangedCallbackVtbl, OnWindowChanged) ==
	                    3 * sizeof(void*));
	WINPR_STATIC_ASSERT(offsetof(IWTSWindowInfoServiceVtbl, UnsubscribeWindowChanged) ==
	                    7 * sizeof(void*));

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_ARM64) || defined(__aarch64__)
	WINPR_STATIC_ASSERT(sizeof(WTS_WINDOW_INFO) == 40);
	WINPR_STATIC_ASSERT(offsetof(WTS_WINDOW_INFO, viewWidth) == 0x10);
	WINPR_STATIC_ASSERT(offsetof(WTS_WINDOW_INFO, scale) == 0x20);
#endif

	CPluginServiceProvider* sp = calloc(1, sizeof(*sp));
	if (!sp)
		return nullptr;
	if (!InitializeCriticalSectionAndSpinCount(&sp->lock, 4000))
	{
		free(sp);
		return nullptr;
	}
	if (!InitializeCriticalSectionAndSpinCount(&sp->subscribeLock, 4000))
	{
		DeleteCriticalSection(&sp->lock);
		free(sp);
		return nullptr;
	}
	winpr_com_base_init(&sp->com, "CPluginServiceProvider", &CPluginServiceProvider_vtbl.iunknown,
	                    CPluginServiceProvider_free);
	sp->context = context;
	return (IWTSPluginServiceProvider*)sp;
}

void plugin_services_detach(IWTSPluginServiceProvider* provider)
{
	CPluginServiceProvider* sp = (CPluginServiceProvider*)provider;
	if (!sp)
		return;

	EnterCriticalSection(&sp->subscribeLock);
	EnterCriticalSection(&sp->lock);
	sp->context = nullptr;
	LeaveCriticalSection(&sp->lock);
	/* proxies stay subscribed (their Unsubscribe still answers S_OK), without notifications */
	shared_unsubscribe(sp);
	LeaveCriticalSection(&sp->subscribeLock);
}

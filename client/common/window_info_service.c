/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * IWTSWindowInfoService for clients
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

#include <winpr/assert.h>
#include <winpr/error.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/wlog.h>

#include <freerdp/log.h>
#include <freerdp/client/window_info_service.h>

#define TAG CLIENT_TAG("common.windowinfo")

typedef struct
{
	wComBase com; /* IWTSWindowInfoService */
	CRITICAL_SECTION lock;
	WINDOW_INFO_SOURCE source;
	BOOL detached;
	IWTSWindowChangedCallback* subscriber; /* a reference */
} CWindowInfoService;

WINPR_COM_IUNKNOWN_THUNKS(CWindowInfoService, &IID_IWTSWindowInfoService)

/** @brief the source, if not detached; lock held */
WINPR_ATTR_NODISCARD
static const WINDOW_INFO_SOURCE* source_locked(CWindowInfoService* s)
{
	return s->detached ? nullptr : &s->source;
}

static HRESULT WINPR_COM_CALL CWindowInfoService_GetWindowInfo(IWTSWindowInfoService* This,
                                                               UINT64 remoteHwnd,
                                                               WTS_WINDOW_INFO* windowInfo)
{
	CWindowInfoService* s = (CWindowInfoService*)This;
	if (!windowInfo)
		return E_POINTER;

	/* the source is called with the lock held: detach() waits for running calls */
	HRESULT hr = E_UNEXPECTED;
	EnterCriticalSection(&s->lock);
	const WINDOW_INFO_SOURCE* src = source_locked(s);
	if (src)
	{
		memset(windowInfo, 0, sizeof(*windowInfo));
		hr = src->GetWindowInfo(src->custom, remoteHwnd, windowInfo);
	}
	LeaveCriticalSection(&s->lock);
	return hr;
}

static HRESULT WINPR_COM_CALL CWindowInfoService_GetProcessId(IWTSWindowInfoService* This,
                                                              DWORD* processId)
{
	WINPR_UNUSED(This);
	if (!processId)
		return E_POINTER;
	*processId = GetCurrentProcessId();
	return S_OK;
}

static HRESULT WINPR_COM_CALL CWindowInfoService_GetRdpSessionType(IWTSWindowInfoService* This,
                                                                   UINT32* sessionType)
{
	CWindowInfoService* s = (CWindowInfoService*)This;
	if (!sessionType)
		return E_POINTER;

	HRESULT hr = E_UNEXPECTED;
	EnterCriticalSection(&s->lock);
	const WINDOW_INFO_SOURCE* src = source_locked(s);
	if (src)
	{
		*sessionType = src->GetSessionType(src->custom);
		hr = S_OK;
	}
	LeaveCriticalSection(&s->lock);
	return hr;
}

static HRESULT WINPR_COM_CALL CWindowInfoService_SubscribeWindowChanged(
    IWTSWindowInfoService* This, UINT64 remoteHwnd, IWTSWindowChangedCallback* callback)
{
	CWindowInfoService* s = (CWindowInfoService*)This;
	WINPR_UNUSED(remoteHwnd); /* Microsoft's client doesn't use it either */
	if (!callback)
		return E_POINTER;

	HRESULT hr = S_OK;
	EnterCriticalSection(&s->lock);
	if (s->detached)
		hr = E_UNEXPECTED;
	else if (s->subscriber)
		hr = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
	else
	{
		WINPR_COM_ADDREF(callback);
		s->subscriber = callback;
	}
	LeaveCriticalSection(&s->lock);
	return hr;
}

static HRESULT WINPR_COM_CALL
CWindowInfoService_UnsubscribeWindowChanged(IWTSWindowInfoService* This)
{
	CWindowInfoService* s = (CWindowInfoService*)This;

	EnterCriticalSection(&s->lock);
	IWTSWindowChangedCallback* callback = s->subscriber;
	s->subscriber = nullptr;
	LeaveCriticalSection(&s->lock);

	if (!callback)
		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	WINPR_COM_RELEASE(callback);
	return S_OK;
}

static const IWTSWindowInfoServiceVtbl CWindowInfoService_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(CWindowInfoService),
	.GetWindowInfo = CWindowInfoService_GetWindowInfo,
	.GetProcessId = CWindowInfoService_GetProcessId,
	.GetRdpSessionType = CWindowInfoService_GetRdpSessionType,
	.SubscribeWindowChanged = CWindowInfoService_SubscribeWindowChanged,
	.UnsubscribeWindowChanged = CWindowInfoService_UnsubscribeWindowChanged,
};

static void CWindowInfoService_free(wComBase* object)
{
	CWindowInfoService* s = (CWindowInfoService*)object;
	if (s->subscriber)
		WINPR_COM_RELEASE(s->subscriber);
	DeleteCriticalSection(&s->lock);
	free(s);
}

IWTSWindowInfoService* freerdp_client_window_info_service_new(const WINDOW_INFO_SOURCE* source)
{
	WINPR_ASSERT(source);
	WINPR_ASSERT(source->GetWindowInfo);
	WINPR_ASSERT(source->GetSessionType);

	CWindowInfoService* s = calloc(1, sizeof(*s));
	if (!s)
		return nullptr;
	if (!InitializeCriticalSectionAndSpinCount(&s->lock, 4000))
	{
		free(s);
		return nullptr;
	}
	winpr_com_base_init(&s->com, "CWindowInfoService", &CWindowInfoService_vtbl.iunknown,
	                    CWindowInfoService_free);
	s->source = *source;
	return (IWTSWindowInfoService*)s;
}

void freerdp_client_window_info_service_changed(IWTSWindowInfoService* service, UINT64 remoteHwnd)
{
	CWindowInfoService* s = (CWindowInfoService*)service;
	if (!s)
		return;

	WTS_WINDOW_INFO info = WINPR_C_ARRAY_INIT;
	IWTSWindowChangedCallback* callback = nullptr;
	EnterCriticalSection(&s->lock);
	const WINDOW_INFO_SOURCE* src = source_locked(s);
	if (src && s->subscriber && SUCCEEDED(src->GetWindowInfo(src->custom, remoteHwnd, &info)))
	{
		callback = s->subscriber;
		WINPR_COM_ADDREF(callback);
	}
	LeaveCriticalSection(&s->lock);

	if (!callback)
		return;
	/* outside the lock: the subscriber may call the service back */
	const HRESULT hr = callback->lpVtbl->OnWindowChanged(callback, &info);
	if (FAILED(hr))
		WLog_DBG(TAG, "OnWindowChanged failed: 0x%08" PRIX32, (UINT32)hr);
	WINPR_COM_RELEASE(callback);
}

void freerdp_client_window_info_service_detach(IWTSWindowInfoService* service)
{
	CWindowInfoService* s = (CWindowInfoService*)service;
	if (!s)
		return;
	EnterCriticalSection(&s->lock);
	s->detached = TRUE;
	LeaveCriticalSection(&s->lock);
}

#include <stdio.h>

#include <winpr/error.h>
#include <winpr/thread.h>

#include <freerdp/freerdp.h>
#include <freerdp/client/window_info_service.h>

/* the client helper (freerdp/client/window_info_service.h), on its own and behind the context's
 * plugin service provider, as a channel uses it */

#define CHECK(cond)                                                          \
	do                                                                       \
	{                                                                        \
		if (!(cond))                                                         \
		{                                                                    \
			(void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); \
			goto fail;                                                       \
		}                                                                    \
	} while (0)

typedef struct
{
	INT32 width;
	UINT32 sessionType;
	int calls;
} Source;

static HRESULT source_get(void* custom, UINT64 remoteHwnd, WTS_WINDOW_INFO* info)
{
	Source* s = custom;
	s->calls++;
	if ((s->sessionType == WTS_RDP_SESSION_TYPE_REMOTEAPP) && (remoteHwnd != 7))
		return E_INVALIDARG;
	info->hwnd = (HWND)(UINT_PTR)0xabc;
	info->remoteWidth = info->viewWidth = s->width;
	info->remoteHeight = info->viewHeight = 600;
	info->scale = 1.0f;
	return S_OK;
}

static UINT32 source_type(void* custom)
{
	return ((Source*)custom)->sessionType;
}

typedef struct
{
	wComBase com; /* IWTSWindowChangedCallback */
	int calls;
	INT32 lastWidth;
} Listener;

WINPR_COM_IUNKNOWN_THUNKS(Listener, nullptr)

static HRESULT WINPR_COM_CALL Listener_OnWindowChanged(IWTSWindowChangedCallback* This,
                                                       const WTS_WINDOW_INFO* info)
{
	Listener* l = (Listener*)This;
	l->calls++;
	l->lastWidth = info->viewWidth;
	return S_OK;
}

static const IWTSWindowChangedCallbackVtbl Listener_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(Listener),
	.OnWindowChanged = Listener_OnWindowChanged,
};

static void Listener_free(wComBase* object)
{
	WINPR_UNUSED(object);
}

static Source source = { 800, WTS_RDP_SESSION_TYPE_DESKTOP, 0 };

static void client_context_free(freerdp* instance, rdpContext* context)
{
	WINPR_UNUSED(instance);
	freerdp_client_window_info_service_detach(context->windowInfoService);
	WINPR_COM_RELEASE(context->windowInfoService);
	context->windowInfoService = nullptr;
}

int TestClientWindowInfoService(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;
	IWTSWindowInfoService* svc = nullptr;
	IWTSWindowInfoService* channel = nullptr;
	freerdp* instance = nullptr;
	Listener l1 = WINPR_C_ARRAY_INIT;
	Listener l2 = WINPR_C_ARRAY_INIT;
	winpr_com_base_init(&l1.com, "L1", &Listener_vtbl.iunknown, Listener_free);
	winpr_com_base_init(&l2.com, "L2", &Listener_vtbl.iunknown, Listener_free);
	IWTSWindowChangedCallback* cb1 = (IWTSWindowChangedCallback*)&l1;
	IWTSWindowChangedCallback* cb2 = (IWTSWindowChangedCallback*)&l2;
	const WINDOW_INFO_SOURCE src = { &source, source_get, source_type };

	/* ---- the helper on its own: Microsoft's behaviour ---- */
	svc = freerdp_client_window_info_service_new(&src);
	CHECK(svc);
	WTS_WINDOW_INFO info = WINPR_C_ARRAY_INIT;
	CHECK(SUCCEEDED(svc->lpVtbl->GetWindowInfo(svc, 0, &info)));
	CHECK(info.hwnd == (HWND)(UINT_PTR)0xabc && info.viewWidth == 800);
	DWORD pid = 0;
	CHECK(SUCCEEDED(svc->lpVtbl->GetProcessId(svc, &pid)) && pid == GetCurrentProcessId());
	UINT32 type = 99;
	CHECK(SUCCEEDED(svc->lpVtbl->GetRdpSessionType(svc, &type)) && type == 0);

	/* nobody subscribed: changes go nowhere */
	freerdp_client_window_info_service_changed(svc, 0);
	CHECK(svc->lpVtbl->UnsubscribeWindowChanged(svc) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	CHECK(SUCCEEDED(svc->lpVtbl->SubscribeWindowChanged(svc, 0, cb1)));
	CHECK(svc->lpVtbl->SubscribeWindowChanged(svc, 0, cb2) ==
	      HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	source.width = 1024;
	freerdp_client_window_info_service_changed(svc, 0);
	CHECK(l1.calls == 1 && l1.lastWidth == 1024 && l2.calls == 0);
	CHECK(SUCCEEDED(svc->lpVtbl->UnsubscribeWindowChanged(svc)));
	freerdp_client_window_info_service_changed(svc, 0);
	CHECK(l1.calls == 1);

	/* detached: the client's state is gone */
	freerdp_client_window_info_service_detach(svc);
	const int calls = source.calls;
	CHECK(svc->lpVtbl->GetWindowInfo(svc, 0, &info) == E_UNEXPECTED);
	CHECK(svc->lpVtbl->GetRdpSessionType(svc, &type) == E_UNEXPECTED);
	CHECK(svc->lpVtbl->SubscribeWindowChanged(svc, 0, cb1) == E_UNEXPECTED);
	freerdp_client_window_info_service_changed(svc, 0);
	CHECK(source.calls == calls);
	WINPR_COM_RELEASE(svc);
	svc = nullptr;

	/* ---- in the context: what a channel gets ---- */
	instance = freerdp_new();
	CHECK(instance);
	instance->ContextFree = client_context_free;
	CHECK(freerdp_context_new(instance));
	rdpContext* context = instance->context;
	context->windowInfoService = freerdp_client_window_info_service_new(&src);
	CHECK(context->windowInfoService);

	IWTSPluginServiceProvider* sp = context->pluginServiceProvider;
	wIUnknown* unk = nullptr;
	CHECK(SUCCEEDED(sp->lpVtbl->GetService(sp, RDCLIENT_WINDOW_INFO_SERVICE, &unk)));
	CHECK(SUCCEEDED(
	    WINPR_COM_QUERY_INTERFACE((wIUnknown*)unk, &IID_IWTSWindowInfoService, &channel)));
	WINPR_COM_RELEASE((wIUnknown*)unk);
	CHECK(SUCCEEDED(channel->lpVtbl->SubscribeWindowChanged(channel, 0, cb2)));
	source.width = 1280;
	freerdp_client_window_info_service_changed(context->windowInfoService, 0);
	CHECK(l2.calls == 1 && l2.lastWidth == 1280);

	/* RemoteApp: the remote window's handle */
	source.sessionType = WTS_RDP_SESSION_TYPE_REMOTEAPP;
	CHECK(SUCCEEDED(channel->lpVtbl->GetRdpSessionType(channel, &type)) && type == 1);
	CHECK(channel->lpVtbl->GetWindowInfo(channel, 0, &info) == E_INVALIDARG);
	CHECK(SUCCEEDED(channel->lpVtbl->GetWindowInfo(channel, 7, &info)));
	freerdp_client_window_info_service_changed(context->windowInfoService, 7);
	CHECK(l2.calls == 2);

	/* the client goes first, the channel after (channels are freed after ContextFree) */
	freerdp_context_free(instance);
	freerdp_free(instance);
	instance = nullptr;
	CHECK(channel->lpVtbl->GetWindowInfo(channel, 7, &info) == E_UNEXPECTED);
	CHECK(SUCCEEDED(channel->lpVtbl->UnsubscribeWindowChanged(channel)));
	WINPR_COM_RELEASE(channel);
	channel = nullptr;

	CHECK(l1.com.refCount == 1 && l2.com.refCount == 1);
	rc = 0;
fail:
	if (svc)
		WINPR_COM_RELEASE(svc);
	if (channel)
		WINPR_COM_RELEASE(channel);
	if (instance)
	{
		freerdp_context_free(instance);
		freerdp_free(instance);
	}
	return rc;
}

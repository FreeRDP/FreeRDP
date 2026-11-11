#include <stdio.h>

#include <winpr/error.h>

#include <freerdp/freerdp.h>
#include <freerdp/wts_services.h>

/* rdpContext::pluginServiceProvider over a fake client window info service with Microsoft's
 * rules (one subscriber), and a fake bitmap render service */

#define CHECK(cond)                                                          \
	do                                                                       \
	{                                                                        \
		if (!(cond))                                                         \
		{                                                                    \
			(void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); \
			goto fail;                                                       \
		}                                                                    \
	} while (0)

/* ---- the client's window info service ---- */

typedef struct
{
	wComBase com; /* IWTSWindowInfoService */
	IWTSWindowChangedCallback* subscriber;
	int subscribes;
	int unsubscribes;
	BOOL freed;
} FakeWindowInfo;

static FakeWindowInfo fakeWindowInfo;

WINPR_COM_IUNKNOWN_THUNKS(FakeWindowInfo, &IID_IWTSWindowInfoService)

static HRESULT WINPR_COM_CALL FakeWindowInfo_GetWindowInfo(IWTSWindowInfoService* This,
                                                           UINT64 remoteHwnd, WTS_WINDOW_INFO* info)
{
	WINPR_UNUSED(This);
	WINPR_UNUSED(remoteHwnd);
	if (!info)
		return E_POINTER;
	memset(info, 0, sizeof(*info));
	info->hwnd = (HWND)(UINT_PTR)0x1234;
	info->remoteWidth = info->viewWidth = 1920;
	info->remoteHeight = info->viewHeight = 1080;
	info->scale = 1.0f;
	return S_OK;
}

static HRESULT WINPR_COM_CALL FakeWindowInfo_GetProcessId(IWTSWindowInfoService* This,
                                                          DWORD* processId)
{
	WINPR_UNUSED(This);
	*processId = 42;
	return S_OK;
}

static HRESULT WINPR_COM_CALL FakeWindowInfo_GetRdpSessionType(IWTSWindowInfoService* This,
                                                               UINT32* sessionType)
{
	WINPR_UNUSED(This);
	*sessionType = WTS_RDP_SESSION_TYPE_DESKTOP;
	return S_OK;
}

static HRESULT WINPR_COM_CALL FakeWindowInfo_Subscribe(IWTSWindowInfoService* This,
                                                       UINT64 remoteHwnd,
                                                       IWTSWindowChangedCallback* callback)
{
	FakeWindowInfo* f = (FakeWindowInfo*)This;
	WINPR_UNUSED(remoteHwnd);
	if (f->subscriber)
		return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
	WINPR_COM_ADDREF(callback);
	f->subscriber = callback;
	f->subscribes++;
	return S_OK;
}

static HRESULT WINPR_COM_CALL FakeWindowInfo_Unsubscribe(IWTSWindowInfoService* This)
{
	FakeWindowInfo* f = (FakeWindowInfo*)This;
	if (!f->subscriber)
		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	WINPR_COM_RELEASE(f->subscriber);
	f->subscriber = nullptr;
	f->unsubscribes++;
	return S_OK;
}

static const IWTSWindowInfoServiceVtbl FakeWindowInfo_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(FakeWindowInfo),
	.GetWindowInfo = FakeWindowInfo_GetWindowInfo,
	.GetProcessId = FakeWindowInfo_GetProcessId,
	.GetRdpSessionType = FakeWindowInfo_GetRdpSessionType,
	.SubscribeWindowChanged = FakeWindowInfo_Subscribe,
	.UnsubscribeWindowChanged = FakeWindowInfo_Unsubscribe,
};

static void FakeWindowInfo_free(wComBase* object)
{
	((FakeWindowInfo*)object)->freed = TRUE;
}

static void fire_window_changed(void)
{
	WTS_WINDOW_INFO info = WINPR_C_ARRAY_INIT;
	IWTSWindowChangedCallback* cb = fakeWindowInfo.subscriber;
	if (cb)
		(void)cb->lpVtbl->OnWindowChanged(cb, &info);
}

/* ---- the client's bitmap render service (only its identity matters) ---- */

typedef struct
{
	wComBase com; /* IWTSBitmapRenderService */
	BOOL freed;
} FakeBitmapRender;

static FakeBitmapRender fakeBitmapRender;

WINPR_COM_IUNKNOWN_THUNKS(FakeBitmapRender, &IID_IWTSBitmapRenderService)

static HRESULT WINPR_COM_CALL FakeBitmapRender_GetMappedRenderer(
    IWTSBitmapRenderService* This, UINT64 mappingId, IWTSBitmapRendererCallback* callback,
    IWTSBitmapRenderer** renderer)
{
	WINPR_UNUSED(This);
	WINPR_UNUSED(mappingId);
	WINPR_UNUSED(callback);
	WINPR_UNUSED(renderer);
	return E_NOTIMPL;
}

static const IWTSBitmapRenderServiceVtbl FakeBitmapRender_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(FakeBitmapRender),
	.GetMappedRenderer = FakeBitmapRender_GetMappedRenderer,
};

static void FakeBitmapRender_free(wComBase* object)
{
	((FakeBitmapRender*)object)->freed = TRUE;
}

/* ---- a plugin's change callback ---- */

typedef struct
{
	wComBase com; /* IWTSWindowChangedCallback */
	int calls;
} Listener;

WINPR_COM_IUNKNOWN_THUNKS(Listener, nullptr)

static HRESULT WINPR_COM_CALL Listener_OnWindowChanged(IWTSWindowChangedCallback* This,
                                                       const WTS_WINDOW_INFO* info)
{
	WINPR_UNUSED(info);
	((Listener*)This)->calls++;
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

/* ---- the client's ContextFree: releases its services ---- */

static BOOL subscribedAtContextFree = TRUE;

static void client_context_free(freerdp* instance, rdpContext* context)
{
	WINPR_UNUSED(instance);
	/* the core dropped its subscription before */
	subscribedAtContextFree = (fakeWindowInfo.subscriber != nullptr);
	WINPR_COM_RELEASE(context->windowInfoService);
	context->windowInfoService = nullptr;
	WINPR_COM_RELEASE(context->bitmapRenderService);
	context->bitmapRenderService = nullptr;
}

static IWTSWindowInfoService* get_window_info(IWTSPluginServiceProvider* sp)
{
	wIUnknown* unk = nullptr;
	IWTSWindowInfoService* svc = nullptr;
	if (FAILED(sp->lpVtbl->GetService(sp, RDCLIENT_WINDOW_INFO_SERVICE, &unk)))
		return nullptr;
	const HRESULT hr = WINPR_COM_QUERY_INTERFACE((wIUnknown*)unk, &IID_IWTSWindowInfoService, &svc);
	WINPR_COM_RELEASE((wIUnknown*)unk);
	return SUCCEEDED(hr) ? svc : nullptr;
}

int TestPluginServices(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;
	IWTSWindowInfoService* a = nullptr;
	IWTSWindowInfoService* b = nullptr;
	Listener la = WINPR_C_ARRAY_INIT;
	Listener lb = WINPR_C_ARRAY_INIT;
	winpr_com_base_init(&la.com, "ListenerA", &Listener_vtbl.iunknown, Listener_free);
	winpr_com_base_init(&lb.com, "ListenerB", &Listener_vtbl.iunknown, Listener_free);
	IWTSWindowChangedCallback* cba = (IWTSWindowChangedCallback*)&la;
	IWTSWindowChangedCallback* cbb = (IWTSWindowChangedCallback*)&lb;
	IWTSPluginServiceProvider* sp = nullptr;

	freerdp* instance = freerdp_new();
	CHECK(instance);
	instance->ContextFree = client_context_free;
	CHECK(freerdp_context_new(instance));
	rdpContext* context = instance->context;
	/* what a channel does from its entry points */
	sp = freerdp_get_plugin_service_provider(context);
	CHECK(sp && (sp == context->pluginServiceProvider));

	/* nothing set by the client yet */
	wIUnknown* unk = nullptr;
	CHECK(sp->lpVtbl->GetService(sp, RDCLIENT_WINDOW_INFO_SERVICE, &unk) == E_NOINTERFACE);
	CHECK(sp->lpVtbl->GetService(sp, RDCLIENT_BITMAP_RENDER_SERVICE, &unk) == E_NOINTERFACE);
	CHECK(sp->lpVtbl->GetService(sp, IID_IWTSPluginServiceProvider, &unk) == E_NOINTERFACE);
	CHECK(!unk);

	/* the client sets its services */
	winpr_com_base_init(&fakeWindowInfo.com, "FakeWindowInfo", &FakeWindowInfo_vtbl.iunknown,
	                    FakeWindowInfo_free);
	winpr_com_base_init(&fakeBitmapRender.com, "FakeBitmapRender", &FakeBitmapRender_vtbl.iunknown,
	                    FakeBitmapRender_free);
	context->windowInfoService = (IWTSWindowInfoService*)&fakeWindowInfo;
	context->bitmapRenderService = (IWTSBitmapRenderService*)&fakeBitmapRender;

	/* the bitmap render service is the client's own */
	CHECK(SUCCEEDED(sp->lpVtbl->GetService(sp, RDCLIENT_BITMAP_RENDER_SERVICE, &unk)));
	CHECK(unk == (wIUnknown*)&fakeBitmapRender);
	WINPR_COM_RELEASE((wIUnknown*)unk);

	/* a window info service per caller, which passes the getters through */
	a = get_window_info(sp);
	b = get_window_info(sp);
	CHECK(a && b && (a != b));
	CHECK(a != (IWTSWindowInfoService*)&fakeWindowInfo);
	WTS_WINDOW_INFO info = WINPR_C_ARRAY_INIT;
	CHECK(SUCCEEDED(a->lpVtbl->GetWindowInfo(a, 0, &info)));
	CHECK(info.hwnd == (HWND)(UINT_PTR)0x1234 && info.remoteWidth == 1920);
	DWORD pid = 0;
	UINT32 type = 99;
	CHECK(SUCCEEDED(b->lpVtbl->GetProcessId(b, &pid)) && pid == 42);
	CHECK(SUCCEEDED(b->lpVtbl->GetRdpSessionType(b, &type)) && type == 0);

	/* Microsoft's rules per object, one subscription to the client */
	CHECK(a->lpVtbl->UnsubscribeWindowChanged(a) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	CHECK(a->lpVtbl->SubscribeWindowChanged(a, 0, nullptr) == E_POINTER);
	CHECK(SUCCEEDED(a->lpVtbl->SubscribeWindowChanged(a, 0, cba)));
	CHECK(a->lpVtbl->SubscribeWindowChanged(a, 0, cbb) == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	CHECK(SUCCEEDED(b->lpVtbl->SubscribeWindowChanged(b, 0, cbb)));
	CHECK(fakeWindowInfo.subscribes == 1);

	fire_window_changed();
	CHECK(la.calls == 1 && lb.calls == 1);

	CHECK(SUCCEEDED(a->lpVtbl->UnsubscribeWindowChanged(a)));
	CHECK(a->lpVtbl->UnsubscribeWindowChanged(a) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	CHECK(fakeWindowInfo.unsubscribes == 0);
	fire_window_changed();
	CHECK(la.calls == 1 && lb.calls == 2);

	/* the last one out ends the client's subscription, the next one in makes a new one */
	CHECK(SUCCEEDED(b->lpVtbl->UnsubscribeWindowChanged(b)));
	CHECK(fakeWindowInfo.unsubscribes == 1 && !fakeWindowInfo.subscriber);
	CHECK(SUCCEEDED(b->lpVtbl->SubscribeWindowChanged(b, 0, cbb)));
	CHECK(fakeWindowInfo.subscribes == 2);

	/* releasing a subscribed object unsubscribes it */
	WINPR_COM_RELEASE(b);
	b = nullptr;
	CHECK(fakeWindowInfo.unsubscribes == 2);

	/* the context goes while a plugin still holds a subscribed service */
	CHECK(SUCCEEDED(a->lpVtbl->SubscribeWindowChanged(a, 0, cba)));
	CHECK(fakeWindowInfo.subscribes == 3);
	freerdp_context_free(instance);
	freerdp_free(instance);
	instance = nullptr;
	CHECK(!subscribedAtContextFree);
	CHECK(fakeWindowInfo.freed && fakeBitmapRender.freed);
	CHECK(a->lpVtbl->GetWindowInfo(a, 0, &info) == E_UNEXPECTED);
	CHECK(a->lpVtbl->SubscribeWindowChanged(a, 0, cbb) == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	CHECK(SUCCEEDED(a->lpVtbl->UnsubscribeWindowChanged(a)));
	CHECK(a->lpVtbl->SubscribeWindowChanged(a, 0, cbb) == E_UNEXPECTED);
	WINPR_COM_RELEASE(a);
	a = nullptr;

	/* the channel's provider outlives the context, and has nothing to give */
	CHECK(sp->lpVtbl->GetService(sp, RDCLIENT_WINDOW_INFO_SERVICE, &unk) == E_NOINTERFACE);
	CHECK(sp->lpVtbl->GetService(sp, RDCLIENT_BITMAP_RENDER_SERVICE, &unk) == E_NOINTERFACE);
	WINPR_COM_RELEASE(sp);
	sp = nullptr;

	/* every reference given back */
	CHECK(la.com.refCount == 1 && lb.com.refCount == 1);
	rc = 0;
fail:
	if (sp)
		WINPR_COM_RELEASE(sp);
	if (a)
		WINPR_COM_RELEASE(a);
	if (b)
		WINPR_COM_RELEASE(b);
	if (instance)
	{
		freerdp_context_free(instance);
		freerdp_free(instance);
	}
	return rc;
}

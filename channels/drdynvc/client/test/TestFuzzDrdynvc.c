/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for the drdynvc client channel (drdynvc_main.c)
 *
 * The built-in drdynvc channel is loaded through its VirtualChannelEntryEx with
 * stubbed virtual channel entry points, connected, and fed server PDUs through
 * the channel's CHANNEL_EVENT_DATA_RECEIVED handler. A listener named
 * FUZZ_LISTENER_NAME accepts dynamic channels and echoes received data back, so
 * channel creation, (compressed) data, fragmentation and close paths are reached.
 *
 * Input: a sequence of records [flags:1][totalLength:2 LE][length:2 LE][data]
 * where flags carries CHANNEL_FLAG_FIRST / CHANNEL_FLAG_LAST.
 */

#include <stddef.h>
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>
#include <winpr/wtsapi.h>

#include <freerdp/addin.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/drdynvc.h>
#include <freerdp/dvc.h>
#include <freerdp/svc.h>

#include "../drdynvc_main.h"

#define FUZZ_LISTENER_NAME "fuzz"
#define FUZZ_OPEN_HANDLE 1
#define FUZZ_MAX_RECORDS 256

typedef struct
{
	PCHANNEL_INIT_EVENT_EX_FN initEvent;
	PCHANNEL_OPEN_EVENT_EX_FN openEvent;
	LPVOID userParam;
} fuzz_vc;

typedef struct
{
	IWTSVirtualChannelCallback iface;
	IWTSVirtualChannel* channel;
} fuzz_dvc_callback;

static fuzz_vc g_vc = WINPR_C_ARRAY_INIT;
static int g_initHandle = 0;

static UINT VCAPITYPE fuzz_init_ex(LPVOID lpUserParam, WINPR_ATTR_UNUSED LPVOID clientContext,
                                   WINPR_ATTR_UNUSED LPVOID pInitHandle,
                                   WINPR_ATTR_UNUSED PCHANNEL_DEF pChannel,
                                   WINPR_ATTR_UNUSED INT channelCount,
                                   WINPR_ATTR_UNUSED ULONG versionRequested,
                                   PCHANNEL_INIT_EVENT_EX_FN pChannelInitEventProcEx)
{
	g_vc.userParam = lpUserParam;
	g_vc.initEvent = pChannelInitEventProcEx;
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE fuzz_open_ex(WINPR_ATTR_UNUSED LPVOID pInitHandle, LPDWORD pOpenHandle,
                                   WINPR_ATTR_UNUSED PCHAR pChannelName,
                                   PCHANNEL_OPEN_EVENT_EX_FN pChannelOpenEventProcEx)
{
	*pOpenHandle = FUZZ_OPEN_HANDLE;
	g_vc.openEvent = pChannelOpenEventProcEx;
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE fuzz_close_ex(WINPR_ATTR_UNUSED LPVOID pInitHandle,
                                    WINPR_ATTR_UNUSED DWORD openHandle)
{
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE fuzz_write_ex(WINPR_ATTR_UNUSED LPVOID pInitHandle, DWORD openHandle,
                                    WINPR_ATTR_UNUSED LPVOID pData,
                                    WINPR_ATTR_UNUSED ULONG dataLength, LPVOID pUserData)
{
	/* the channel releases the written stream on CHANNEL_EVENT_WRITE_COMPLETE */
	if (g_vc.openEvent)
		g_vc.openEvent(g_vc.userParam, openHandle, CHANNEL_EVENT_WRITE_COMPLETE, pUserData, 0, 0,
		               0);
	else
		Stream_Release((wStream*)pUserData);
	return CHANNEL_RC_OK;
}

static UINT fuzz_dvc_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	fuzz_dvc_callback* callback = (fuzz_dvc_callback*)pChannelCallback;
	const size_t length = Stream_GetRemainingLength(data);

	/* echo back to exercise the client write and fragmentation path */
	if ((length == 0) || (length > UINT32_MAX) || !callback->channel)
		return CHANNEL_RC_OK;
	return callback->channel->Write(callback->channel, (ULONG)length,
	                                Stream_ConstPointer(data), nullptr);
}

static UINT fuzz_dvc_on_open(WINPR_ATTR_UNUSED IWTSVirtualChannelCallback* pChannelCallback)
{
	return CHANNEL_RC_OK;
}

static UINT fuzz_dvc_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
	return CHANNEL_RC_OK;
}

static UINT fuzz_on_new_channel_connection(WINPR_ATTR_UNUSED IWTSListenerCallback* pListenerCallback,
                                           IWTSVirtualChannel* pChannel,
                                           WINPR_ATTR_UNUSED BYTE* Data, BOOL* pbAccept,
                                           IWTSVirtualChannelCallback** ppCallback)
{
	fuzz_dvc_callback* callback = (fuzz_dvc_callback*)calloc(1, sizeof(fuzz_dvc_callback));
	if (!callback)
		return CHANNEL_RC_NO_MEMORY;

	callback->iface.OnDataReceived = fuzz_dvc_on_data_received;
	callback->iface.OnOpen = fuzz_dvc_on_open;
	callback->iface.OnClose = fuzz_dvc_on_close;
	callback->channel = pChannel;

	*pbAccept = TRUE;
	*ppCallback = &callback->iface;
	return CHANNEL_RC_OK;
}

static IWTSListenerCallback g_listener = { fuzz_on_new_channel_connection, nullptr };

static void fuzz_feed(const uint8_t* data, size_t size)
{
	wStream buffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&buffer, data, size);

	for (size_t index = 0; index < FUZZ_MAX_RECORDS; index++)
	{
		if (Stream_GetRemainingLength(s) < 5)
			break;

		const UINT8 flags = Stream_Get_UINT8(s);
		const UINT16 totalLength = Stream_Get_UINT16(s);
		UINT16 length = Stream_Get_UINT16(s);
		if (length > Stream_GetRemainingLength(s))
			length = (UINT16)Stream_GetRemainingLength(s);

		g_vc.openEvent(g_vc.userParam, FUZZ_OPEN_HANDLE, CHANNEL_EVENT_DATA_RECEIVED,
		               Stream_Pointer(s), length, totalLength,
		               flags & (CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST));
		Stream_Seek(s, length);
	}
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (1u << 20))
		return 0;

	wLog* root = WLog_GetRoot();
	(void)WLog_SetLogLevel(root, WLOG_TRACE);
	(void)WLog_SetLogAppenderType(root, WLOG_APPENDER_CALLBACK);

	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return 0;

	/* process PDUs on the calling thread instead of the channel worker thread */
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_SynchronousDynamicChannels, TRUE))
		goto fail;

	{
		PVIRTUALCHANNELENTRYEX entryEx = WINPR_FUNC_PTR_CAST(
		    freerdp_channels_load_static_addin_entry(
		        DRDYNVC_SVC_CHANNEL_NAME, nullptr, nullptr,
		        FREERDP_ADDIN_CHANNEL_STATIC | FREERDP_ADDIN_CHANNEL_ENTRYEX),
		    PVIRTUALCHANNELENTRYEX);
		if (!entryEx)
			goto fail;

		CHANNEL_ENTRY_POINTS_FREERDP_EX entryPoints = WINPR_C_ARRAY_INIT;
		entryPoints.cbSize = sizeof(entryPoints);
		entryPoints.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
		entryPoints.pVirtualChannelInitEx = fuzz_init_ex;
		entryPoints.pVirtualChannelOpenEx = fuzz_open_ex;
		entryPoints.pVirtualChannelCloseEx = fuzz_close_ex;
		entryPoints.pVirtualChannelWriteEx = fuzz_write_ex;
		entryPoints.MagicNumber = FREERDP_CHANNEL_MAGIC_NUMBER;
		entryPoints.context = context;

		const fuzz_vc empty = WINPR_C_ARRAY_INIT;
		g_vc = empty;
		if (!entryEx((PCHANNEL_ENTRY_POINTS_EX)&entryPoints, &g_initHandle) || !g_vc.initEvent)
			goto fail;
	}

	g_vc.initEvent(g_vc.userParam, &g_initHandle, CHANNEL_EVENT_INITIALIZED, nullptr, 0);
	g_vc.initEvent(g_vc.userParam, &g_initHandle, CHANNEL_EVENT_CONNECTED, nullptr, 0);

	{
		drdynvcPlugin* drdynvc = (drdynvcPlugin*)g_vc.userParam;
		IWTSVirtualChannelManager* mgr = drdynvc->channel_mgr;
		if (mgr && g_vc.openEvent &&
		    (mgr->CreateListener(mgr, FUZZ_LISTENER_NAME, 0, &g_listener, nullptr) ==
		     CHANNEL_RC_OK))
			fuzz_feed(data, size);
	}

	g_vc.initEvent(g_vc.userParam, &g_initHandle, CHANNEL_EVENT_DISCONNECTED, nullptr, 0);
	/* frees the plugin */
	g_vc.initEvent(g_vc.userParam, &g_initHandle, CHANNEL_EVENT_TERMINATED, nullptr, 0);

fail:
	freerdp_client_context_free(context);
	return 0;
}


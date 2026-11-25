/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Regression test: a dynamic channel request without a listener must not leave
 * state behind, so the server can reuse that ChannelId for a supported channel.
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

#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include <freerdp/addin.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/drdynvc.h>
#include <freerdp/dvc.h>
#include <freerdp/svc.h>

#include "../drdynvc_main.h"

#define TEST_LISTENER_NAME "test"
#define TEST_OPEN_HANDLE 1

typedef struct
{
	PCHANNEL_INIT_EVENT_EX_FN initEvent;
	PCHANNEL_OPEN_EVENT_EX_FN openEvent;
	LPVOID userParam;
	INT32 lastCreateStatus;
	BOOL haveCreateStatus;
} test_vc;

static test_vc g_vc = WINPR_C_ARRAY_INIT;
static int g_initHandle = 0;

static UINT VCAPITYPE test_init_ex(LPVOID lpUserParam, WINPR_ATTR_UNUSED LPVOID clientContext,
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

static UINT VCAPITYPE test_open_ex(WINPR_ATTR_UNUSED LPVOID pInitHandle, LPDWORD pOpenHandle,
                                   WINPR_ATTR_UNUSED PCHAR pChannelName,
                                   PCHANNEL_OPEN_EVENT_EX_FN pChannelOpenEventProcEx)
{
	*pOpenHandle = TEST_OPEN_HANDLE;
	g_vc.openEvent = pChannelOpenEventProcEx;
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE test_close_ex(WINPR_ATTR_UNUSED LPVOID pInitHandle,
                                    WINPR_ATTR_UNUSED DWORD openHandle)
{
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE test_write_ex(WINPR_ATTR_UNUSED LPVOID pInitHandle, DWORD openHandle,
                                    LPVOID pData, ULONG dataLength, LPVOID pUserData)
{
	/* CREATE_RESPONSE_PDU with a 1 byte ChannelId: header, ChannelId, CreationStatus */
	const BYTE* data = pData;
	if (data && (dataLength == 6) && ((data[0] >> 4) == CREATE_REQUEST_PDU))
	{
		g_vc.lastCreateStatus =
		    (INT32)(data[2] | (data[3] << 8) | (data[4] << 16) | ((UINT32)data[5] << 24));
		g_vc.haveCreateStatus = TRUE;
	}

	/* the channel releases the written stream on CHANNEL_EVENT_WRITE_COMPLETE */
	if (g_vc.openEvent)
		g_vc.openEvent(g_vc.userParam, openHandle, CHANNEL_EVENT_WRITE_COMPLETE, pUserData, 0, 0,
		               0);
	else
		Stream_Release((wStream*)pUserData);
	return CHANNEL_RC_OK;
}

static UINT test_dvc_on_data_received(WINPR_ATTR_UNUSED IWTSVirtualChannelCallback* pCallback,
                                      WINPR_ATTR_UNUSED wStream* data)
{
	return CHANNEL_RC_OK;
}

static UINT test_dvc_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
	return CHANNEL_RC_OK;
}

static UINT test_on_new_channel_connection(WINPR_ATTR_UNUSED IWTSListenerCallback* pListener,
                                           WINPR_ATTR_UNUSED IWTSVirtualChannel* pChannel,
                                           WINPR_ATTR_UNUSED BYTE* Data, BOOL* pbAccept,
                                           IWTSVirtualChannelCallback** ppCallback)
{
	IWTSVirtualChannelCallback* callback = calloc(1, sizeof(IWTSVirtualChannelCallback));
	if (!callback)
		return CHANNEL_RC_NO_MEMORY;

	callback->OnDataReceived = test_dvc_on_data_received;
	callback->OnClose = test_dvc_on_close;
	*pbAccept = TRUE;
	*ppCallback = callback;
	return CHANNEL_RC_OK;
}

static IWTSListenerCallback g_listener = { test_on_new_channel_connection, nullptr };

/* Sends a CREATE_REQUEST_PDU (1 byte ChannelId) and returns the client's CreationStatus. */
static BOOL send_create_request(BYTE channelId, const char* name, INT32* status)
{
	BYTE pdu[64] = WINPR_C_ARRAY_INIT;
	const size_t nameLength = strlen(name) + 1;
	if (nameLength + 2 > sizeof(pdu))
		return FALSE;

	pdu[0] = CREATE_REQUEST_PDU << 4;
	pdu[1] = channelId;
	memcpy(&pdu[2], name, nameLength);

	g_vc.haveCreateStatus = FALSE;
	g_vc.openEvent(g_vc.userParam, TEST_OPEN_HANDLE, CHANNEL_EVENT_DATA_RECEIVED, pdu,
	               (UINT32)(nameLength + 2), (UINT32)(nameLength + 2),
	               CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST);
	if (!g_vc.haveCreateStatus)
		return FALSE;
	*status = g_vc.lastCreateStatus;
	return TRUE;
}

int TestDrdynvcUnknownChannel(int argc, char* argv[])
{
	int rc = -1;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return -1;

	/* process PDUs on the calling thread instead of the channel worker thread */
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_SynchronousDynamicChannels, TRUE))
		goto fail;

	{
		PVIRTUALCHANNELENTRYEX entryEx =
		    WINPR_FUNC_PTR_CAST(freerdp_channels_load_static_addin_entry(
		                            DRDYNVC_SVC_CHANNEL_NAME, nullptr, nullptr,
		                            FREERDP_ADDIN_CHANNEL_STATIC | FREERDP_ADDIN_CHANNEL_ENTRYEX),
		                        PVIRTUALCHANNELENTRYEX);
		if (!entryEx)
			goto fail;

		CHANNEL_ENTRY_POINTS_FREERDP_EX entryPoints = WINPR_C_ARRAY_INIT;
		entryPoints.cbSize = sizeof(entryPoints);
		entryPoints.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
		entryPoints.pVirtualChannelInitEx = test_init_ex;
		entryPoints.pVirtualChannelOpenEx = test_open_ex;
		entryPoints.pVirtualChannelCloseEx = test_close_ex;
		entryPoints.pVirtualChannelWriteEx = test_write_ex;
		entryPoints.MagicNumber = FREERDP_CHANNEL_MAGIC_NUMBER;
		entryPoints.context = context;

		if (!entryEx((PCHANNEL_ENTRY_POINTS_EX)&entryPoints, &g_initHandle) || !g_vc.initEvent)
			goto fail;
	}

	g_vc.initEvent(g_vc.userParam, &g_initHandle, CHANNEL_EVENT_INITIALIZED, nullptr, 0);
	g_vc.initEvent(g_vc.userParam, &g_initHandle, CHANNEL_EVENT_CONNECTED, nullptr, 0);

	{
		drdynvcPlugin* drdynvc = (drdynvcPlugin*)g_vc.userParam;
		IWTSVirtualChannelManager* mgr = drdynvc->channel_mgr;
		INT32 status = 0;

		if (!mgr || !g_vc.openEvent ||
		    (mgr->CreateListener(mgr, TEST_LISTENER_NAME, 0, &g_listener, nullptr) !=
		     CHANNEL_RC_OK))
			goto disconnect;

		/* Unsupported channel: refused, repeatedly, with the same answer. */
		for (size_t x = 0; x < 2; x++)
		{
			if (!send_create_request(5, "unsupported", &status) || (status != STATUS_NOT_FOUND))
			{
				(void)fprintf(stderr, "unsupported channel #%" PRIuz ": status 0x%08" PRIx32 "\n",
				              x, (UINT32)status);
				goto disconnect;
			}
		}

		/* The server may reuse that ChannelId for a channel the client supports. */
		if (!send_create_request(5, TEST_LISTENER_NAME, &status) || (status != 0))
		{
			(void)fprintf(stderr, "supported channel after reuse: status 0x%08" PRIx32 "\n",
			              (UINT32)status);
			goto disconnect;
		}
		rc = 0;
	}

disconnect:
	g_vc.initEvent(g_vc.userParam, &g_initHandle, CHANNEL_EVENT_DISCONNECTED, nullptr, 0);
	/* frees the plugin */
	g_vc.initEvent(g_vc.userParam, &g_initHandle, CHANNEL_EVENT_TERMINATED, nullptr, 0);

fail:
	freerdp_client_context_free(context);
	return rc;
}

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2021 Alexandru Bagu <alexandru.bagu@gmail.com>
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

#include "pf_dynamic_passthrough.h"
#include "pf_log.h"

#define TAG PROXY_TAG("dynamic_passthrough")

void server_dynamic_passthrough_free(DynamicPassthroughServerContext* context);
void client_dynamic_passthrough_free(DynamicPassthroughClientContext* context);

DWORD get_server_session_id(pServerContext* context)
{
	PULONG pSessionId = NULL;
	DWORD BytesReturned = 0;
	DWORD SessionId = WTS_CURRENT_SESSION;

	if (WTSQuerySessionInformationA(context->vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		return FALSE;
	}

	SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);
	return SessionId;
}

BOOL get_channel_event_handle(DynamicPassthroughServerPrivate* priv)
{
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;
	void* buffer;
	buffer = NULL;

	/* Query for channel event handle */
	if (!WTSVirtualChannelQuery(priv->channel, WTSVirtualEventHandle, &buffer, &BytesReturned) ||
	    (BytesReturned != sizeof(HANDLE)))
	{
		WLog_ERR(TAG,
		         "WTSVirtualChannelQuery failed "
		         "or invalid returned size(%" PRIu32 ")",
		         BytesReturned);

		if (buffer)
			WTSFreeMemory(buffer);

		return FALSE;
	}

	CopyMemory(&priv->channelEvent, buffer, sizeof(HANDLE));
	WTSFreeMemory(buffer);

	return TRUE;
}

static UINT dynamic_passthrough_server_handle_messages(DynamicPassthroughServerContext* context)
{
	DWORD BytesReturned;
	void* buffer;
	UINT ret = CHANNEL_RC_OK;
	DynamicPassthroughServerPrivate* priv = context->priv;

	/* Check whether the dynamic channel is ready */
	if (!priv->isReady)
	{
		if (WTSVirtualChannelQuery(priv->channel, WTSVirtualChannelReady, &buffer,
		                           &BytesReturned) == FALSE)
		{
			if (GetLastError() == ERROR_NO_DATA)
				return ERROR_NO_DATA;
			return ERROR_INTERNAL_ERROR;
		}

		priv->isReady = *((BOOL*)buffer);
		WTSFreeMemory(buffer);
	}

	/* Consume channel event only after the dynamic channel is ready */

	if (!WTSVirtualChannelRead(priv->channel, 0, NULL, 0, &BytesReturned))
	{
		if (GetLastError() == ERROR_NO_DATA)
			return ERROR_NO_DATA;

		return ERROR_INTERNAL_ERROR;
	}

	if (BytesReturned < 1)
		return CHANNEL_RC_OK;

	wStream* s = Stream_New(NULL, BytesReturned);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (WTSVirtualChannelRead(priv->channel, 0, (PCHAR)Stream_Buffer(s), Stream_Capacity(s),
	                          &BytesReturned) == FALSE)
	{
		Stream_Free(s, TRUE);
		return ERROR_INTERNAL_ERROR;
	}

	Stream_SetLength(s, BytesReturned);
	Stream_SetPosition(s, 0);
	if (context->OnReceive)
		ret = context->OnReceive(context, s);
	Stream_Free(s, TRUE);
	return ret;
}

static DWORD WINAPI dynamic_passthrough_server_thread_func(LPVOID arg)
{
	DynamicPassthroughServerContext* context = (DynamicPassthroughServerContext*)arg;
	DynamicPassthroughServerPrivate* priv = context->priv;
	DWORD status;
	DWORD nCount;
	HANDLE events[8];
	UINT error = CHANNEL_RC_OK;
	nCount = 0;
	events[nCount++] = priv->stopEvent;
	events[nCount++] = priv->channelEvent;

	/* Main virtual channel loop. */
	while (TRUE)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}

		/* Stop Event */
		if (status == WAIT_OBJECT_0)
			break;

		if ((error = dynamic_passthrough_server_handle_messages(context)))
		{
			break;
		}
	}

	DynamicPassthroughClientContext* client = (DynamicPassthroughClientContext*)context->client;
	if (client && client->dvcman_channel)
	{
		client->dvcman_channel->disconnect(client->dvcman_channel);
	}
	else
	{
		pServerContext* server = context->custom;
		server_dynamic_passthrough_free(context);
		ArrayList_Remove(server->dynamic_passthrough_channels, context);
	}

	ExitThread(error);
	return error;
}

UINT server_dynamic_passthrough_send(DynamicPassthroughServerContext* context,
                                     const wStream* stream)
{
	UINT ret;
	ULONG written;
	ULONG length = Stream_Length(stream) - Stream_GetPosition(stream);
	if (!WTSVirtualChannelWrite(context->priv->channel, (PCHAR)Stream_Pointer(stream), length,
	                            &written))
	{
		WLog_ERR(TAG, "server_dynamic_passthrough_send::WTSVirtualChannelWrite failed!");
		ret = ERROR_INTERNAL_ERROR;
		goto out;
	}

	if (written < length)
	{
		WLog_WARN(TAG,
		          "server_dynamic_passthrough_send::Unexpected bytes written: %" PRIu32 "/%" PRIuz
		          "",
		          written, length);
	}

	ret = CHANNEL_RC_OK;
out:
	return ret;
}

UINT client_dynamic_passthrough_send(DynamicPassthroughClientContext* context,
                                     const wStream* stream)
{
	DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL* channel = context->dvcman_channel;
	return channel->iface->Write(channel->iface, Stream_Length(stream),
	                             (PCHAR)Stream_Buffer(stream), NULL);
}

UINT server_dynamic_passthrough_on_receive(DynamicPassthroughServerContext* context,
                                           const wStream* stream)
{
	DynamicPassthroughClientContext* client = (DynamicPassthroughClientContext*)context->client;
	if (client && client->Send)
	{
		return client->Send(client, stream);
	}
	return ERROR_NO_DATA;
}

UINT client_dynamic_passthrough_on_receive(DynamicPassthroughClientContext* context,
                                           const wStream* stream)
{
	DynamicPassthroughServerContext* server = (DynamicPassthroughServerContext*)context->server;
	Stream_SetPosition(stream, 2);
	BYTE* buffer = Stream_Pointer(stream);
	size_t len = Stream_Length(stream) - Stream_GetPosition(stream);
	if (server && server->Send)
	{
		return server->Send(server, stream);
	}
	return ERROR_NO_DATA;
}

void server_dynamic_passthrough_free(DynamicPassthroughServerContext* context)
{
	if (context)
	{
		context->OnReceive = NULL;
		context->Send = NULL;
		if (context->priv)
		{
			if (context->priv->channel)
				WTSVirtualChannelClose(context->priv->channel);
			free(context->priv);
		}
		if (context->channelname)
			free(context->channelname);
		if (context->client)
		{
			DynamicPassthroughClientContext* client = context->client;
			client->server = NULL;
		}
		free(context);
	}
}

BOOL server_open_dynamic_passthrough(DynamicPassthroughServerContext* context)
{
	if (!(context->priv->thread = CreateThread(NULL, 0, dynamic_passthrough_server_thread_func,
	                                           (void*)context, 0, NULL)))
	{
		WLog_ERR(TAG, "server_open_dynamic_passthrough::CreateEvent failed!");
		CloseHandle(context->priv->stopEvent);
		context->priv->stopEvent = NULL;
		return FALSE;
	}
	return TRUE;
}

DynamicPassthroughServerContext* server_init_dynamic_passthrough(proxyData* pdata,
                                                                 const char* channelname)
{
	pServerContext* server = (pServerContext*)pdata->ps;

	DWORD SessionId = get_server_session_id(server);

	DynamicPassthroughServerContext* dpctx =
	    (DynamicPassthroughServerContext*)calloc(1, sizeof(DynamicPassthroughServerContext));

	if (!dpctx)
	{
		WLog_ERR(TAG, "pf_server_init_dynamic_passthrough::dpctx calloc failed");
		goto failed;
	}

	dpctx->custom = server;
	dpctx->Send = server_dynamic_passthrough_send;
	dpctx->OnReceive = server_dynamic_passthrough_on_receive;
	dpctx->channelname = _strdup(channelname);

	if (!dpctx->channelname)
	{
		WLog_ERR(TAG, "pf_server_init_dynamic_passthrough::channelname _strdup failed");
		goto failed;
	}

	dpctx->priv =
	    (DynamicPassthroughServerPrivate*)calloc(1, sizeof(DynamicPassthroughServerPrivate));

	if (!dpctx->priv)
	{
		WLog_ERR(TAG, "pf_server_init_dynamic_passthrough::priv calloc failed");
		goto failed;
	}

	dpctx->priv->channel =
	    (HANDLE)WTSVirtualChannelOpenEx(SessionId, dpctx->channelname, WTS_CHANNEL_OPTION_DYNAMIC);

	if (!dpctx->priv->channel)
	{
		WLog_ERR(TAG, "pf_server_init_dynamic_passthrough::WTSVirtualChannelOpenEx failed!");
		goto failed;
	}

	if (!get_channel_event_handle(dpctx->priv))
	{
		goto failed;
	}

	if (!(dpctx->priv->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "pf_server_init_dynamic_passthrough::CreateEvent failed!");
		goto failed;
	}

	ArrayList_Lock(server->dynamic_passthrough_channels);
	ArrayList_Append(server->dynamic_passthrough_channels, dpctx);
	ArrayList_Unlock(server->dynamic_passthrough_channels);
	return dpctx;
failed:
	server_dynamic_passthrough_free(dpctx);
	return NULL;
}

UINT client_dynamic_passthrough_on_data_received(IWTSVirtualChannelCallback* pChannelCallback,
                                                 wStream* data)
{
	DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL_CALLLBACK* callback =
	    (DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL_CALLLBACK*)pChannelCallback;

	DynamicPassthroughClientContext* dpctx = (DynamicPassthroughClientContext*)callback->custom;
	return client_dynamic_passthrough_on_receive(dpctx, data);
}

void client_dynamic_passthrough_free(DynamicPassthroughClientContext* context)
{
	if (context)
	{
		context->Send = NULL;
		if (context->dvcman_channel && context->dvcman_channel->channel_callback)
			context->dvcman_channel->channel_callback->OnDataReceived = NULL;
		if (context->channelname)
			free(context->channelname);
		if (context->server)
		{
			DynamicPassthroughServerContext* server = context->server;
			server->client = NULL;
		}
		if (context->dvcman_channel)
		{
			context->dvcman_channel->iface->Close(context->dvcman_channel->iface);
			context->dvcman_channel = NULL;
		}
		free(context);
	}
}

void client_dynamic_passthrough_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
}

BOOL pf_init_dynamic_passthrough(proxyData* pdata, const char* channelname,
                                 DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL* channel)
{
	pServerContext* server = (pServerContext*)pdata->ps;
	pClientContext* client = (pClientContext*)pdata->pc;
	DynamicPassthroughServerContext* sdpctx = NULL;

	DynamicPassthroughClientContext* dpctx =
	    (DynamicPassthroughClientContext*)calloc(1, sizeof(DynamicPassthroughClientContext));

	if (!dpctx)
	{
		WLog_ERR(TAG, "pf_init_dynamic_passthrough::dpctx calloc failed");
		goto failed;
	}
	dpctx->channelname = _strdup(channelname);
	if (!dpctx->channelname)
	{
		WLog_ERR(TAG, "pf_init_dynamic_passthrough::channelname _strdup failed");
		goto failed;
	}

	dpctx->custom = client;
	dpctx->dvcman_channel = channel;
	dpctx->Send = client_dynamic_passthrough_send;

	DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL_CALLLBACK* callback =
	    (DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL_CALLLBACK*)channel->channel_callback;
	callback->custom = dpctx;
	callback->iface.OnDataReceived = client_dynamic_passthrough_on_data_received;
	callback->iface.OnClose = client_dynamic_passthrough_on_close;
	dpctx->channelname = _strdup(channelname);

	sdpctx = server_init_dynamic_passthrough(pdata, channelname);
	if (sdpctx)
	{

		sdpctx->client = dpctx;
		dpctx->server = sdpctx;

		if (!server_open_dynamic_passthrough(sdpctx))
			goto failed;

		ArrayList_Lock(client->dynamic_passthrough_channels);
		ArrayList_Append(client->dynamic_passthrough_channels, dpctx);
		ArrayList_Unlock(client->dynamic_passthrough_channels);
		return TRUE;
	}

failed:
	if (sdpctx)
	{
		server_dynamic_passthrough_free(sdpctx);
	}
	if (dpctx)
	{
		client_dynamic_passthrough_free(dpctx);
	}
	return FALSE;
}

void pf_free_dynamic_passthrough(proxyData* pdata, const char* channelname,
                                 DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL* channel)
{
	pClientContext* client = (pClientContext*)pdata->pc;
	ArrayList_Lock(client->dynamic_passthrough_channels);
	int dpccount = ArrayList_Count(client->dynamic_passthrough_channels);
	for (int i = 0; i < dpccount; i++)
	{
		DynamicPassthroughClientContext* dpctx =
		    ArrayList_GetItem(client->dynamic_passthrough_channels, i);
		if (dpctx->dvcman_channel == channel && strcmp(dpctx->channelname, channelname) == 0)
		{
			ArrayList_RemoveAt(client->dynamic_passthrough_channels, i);

			if (dpctx->server)
			{
				DynamicPassthroughServerContext* sdpctx = dpctx->server;
				pServerContext* server = sdpctx->custom;
				if (server)
				{
					ArrayList_Remove(server->dynamic_passthrough_channels, sdpctx);
					server_dynamic_passthrough_free(sdpctx);
				}
			}

			client_dynamic_passthrough_free(dpctx);
		}
	}
	ArrayList_Unlock(client->dynamic_passthrough_channels);
}

void pf_server_clear_dynamic_passthrough(proxyData* pdata)
{
	pServerContext* server = (pServerContext*)pdata->ps;
	ArrayList_Lock(server->dynamic_passthrough_channels);
	int dpccount = ArrayList_Count(server->dynamic_passthrough_channels);
	for (int i = 0; i < dpccount; i++)
	{
		DynamicPassthroughServerContext* dpctx =
		    ArrayList_GetItem(server->dynamic_passthrough_channels, i);
		ArrayList_RemoveAt(server->dynamic_passthrough_channels, i);
		server_dynamic_passthrough_free(dpctx);
	}
	ArrayList_Unlock(server->dynamic_passthrough_channels);
}
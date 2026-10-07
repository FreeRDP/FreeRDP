/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * WebAuthn Virtual Channel Extension [MS-RDPEWA]
 * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpewa/68f2df2e-7c40-4a93-9bb0-517e4283a991
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

#include <stdio.h>
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>
#include <winpr/print.h>
#include <winpr/thread.h>
#include <winpr/synch.h>

#include "rdpewa_main.h"
#include "rdpewa_cbor.h"
#include "rdpewa_fido.h"
#include <rdpewa-common.h>

#include <freerdp/client/channels.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpewa.h>

#define TAG CHANNELS_TAG("rdpewa.client")

/** @brief Context for async FIDO operations (command 5) */
typedef struct
{
	rdpContext* rdpContext;
	RDPEWA_REQUEST request;
	IWTSVirtualChannel* channel;
} RDPEWA_ASYNC_WORK;

static DWORD WINAPI rdpewa_async_webauthn_thread(LPVOID arg)
{
	RDPEWA_ASYNC_WORK* work = (RDPEWA_ASYNC_WORK*)arg;
	wStream* s = nullptr;

	switch (work->request.command)
	{
		case CTAPCBOR_RPC_COMMAND_WEB_AUTHN:
			s = rdpewa_fido_webauthn(work->rdpContext, &work->request);
			break;
		default:
			break;
	}

	if (s)
	{
		const size_t len = Stream_GetPosition(s);
		WLog_DBG(TAG, "Async: sending %" PRIuz " byte response for command %" PRIu32, len,
		         work->request.command);
		winpr_HexDump(TAG, WLOG_TRACE, Stream_Buffer(s), len > 512 ? 512 : len);

		if (!freerdp_shall_disconnect_context(work->rdpContext))
		{
			UINT status =
			    work->channel->Write(work->channel, (ULONG)len, Stream_Buffer(s), nullptr);
			if (status != CHANNEL_RC_OK)
				WLog_ERR(TAG, "Async: Write failed with 0x%08" PRIx32, status);
		}
	}
	else
	{
		WLog_ERR(TAG, "Async: FIDO operation failed for command %" PRIu32, work->request.command);
	}

	Stream_Free(s, TRUE);
	free(work->request.request);
	free(work);
	return 0;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpewa_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	RDPEWA_CHANNEL_CALLBACK* ecb = (RDPEWA_CHANNEL_CALLBACK*)pChannelCallback;
	GENERIC_CHANNEL_CALLBACK* callback = &ecb->base;
	const BYTE* pBuffer = Stream_ConstPointer(data);
	const size_t cbSize = Stream_GetRemainingLength(data);
	wStream* response = nullptr;
	RDPEWA_REQUEST request = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(callback);
	WINPR_ASSERT(callback->channel);
	WINPR_ASSERT(callback->channel->Write);

	WLog_DBG(TAG, "Received %" PRIuz " bytes from server", cbSize);
	winpr_HexDump(TAG, WLOG_TRACE, pBuffer, cbSize > 2048 ? 2048 : cbSize);

	if (!rdpewa_cbor_decode_request(pBuffer, cbSize, &request))
	{
		WLog_ERR(TAG, "Failed to decode CBOR request");
		free(request.request);
		return ERROR_INVALID_DATA;
	}

	WLog_DBG(TAG,
	         "Received command %" PRIu32 " flags 0x%08" PRIx32 " requestLen=%" PRIuz
	         " timeout=%" PRIu32,
	         request.command, request.flags, request.requestLen, request.timeout);

	switch (request.command)
	{
		case CTAPCBOR_RPC_COMMAND_WEB_AUTHN:
		{
			/* Run FIDO operations on a worker thread to avoid blocking
			 * the DVC callback thread (which would freeze RDP rendering). */
			RDPEWA_PLUGIN* rdpewa = (RDPEWA_PLUGIN*)callback->plugin;
			RDPEWA_ASYNC_WORK* work = calloc(1, sizeof(*work));
			if (!work)
			{
				free(request.request);
				return ERROR_INTERNAL_ERROR;
			}
			work->rdpContext = rdpewa->rdp_context;
			work->request = request;
			work->channel = callback->channel;
			/* Transfer ownership of request.request to the worker */
			request.request = nullptr;

			/* Wait for any previous in-flight operation to finish before
			 * starting a new one, to avoid use-after-free on the channel. */
			if (ecb->workerThread)
			{
				WaitForSingleObject(ecb->workerThread, INFINITE);
				CloseHandle(ecb->workerThread);
				ecb->workerThread = nullptr;
			}

			ecb->workerThread =
			    CreateThread(nullptr, 0, rdpewa_async_webauthn_thread, work, 0, nullptr);
			if (!ecb->workerThread)
			{
				free(work->request.request);
				free(work);
				return ERROR_INTERNAL_ERROR;
			}
			return CHANNEL_RC_OK;
		}

		case CTAPCBOR_RPC_COMMAND_IUVPAA:
			response = rdpewa_fido_is_uvpaa();
			break;

		case CTAPCBOR_RPC_COMMAND_CANCEL_CUR_OP:
			/* Cancel is best-effort; we don't track in-flight operations yet */
			response = rdpewa_cbor_encode_hresult_response(S_OK);
			break;

		case CTAPCBOR_RPC_COMMAND_API_VERSION:
			response = rdpewa_fido_api_version();
			break;

		case CTAPCBOR_RPC_COMMAND_GET_CREDENTIALS:
		{
			RDPEWA_PLUGIN* rdpewa = (RDPEWA_PLUGIN*)callback->plugin;
			response = rdpewa_fido_get_credentials(rdpewa->rdp_context, request.rpId);
			break;
		}

		case CTAPCBOR_RPC_COMMAND_GET_AUTHENTICATOR_LIST:
			response = rdpewa_fido_get_authenticator_list();
			break;

		default:
			WLog_WARN(TAG, "Unsupported command %" PRIu32, request.command);
			response = rdpewa_cbor_encode_hresult_response(E_NOTIMPL);
			break;
	}

	if (!response)
	{
		free(request.request);
		return ERROR_INTERNAL_ERROR;
	}

	const size_t responseLen = Stream_GetPosition(response);
	WLog_DBG(TAG, "Sending %" PRIuz " byte response for command %" PRIu32, responseLen,
	         request.command);
	winpr_HexDump(TAG, WLOG_TRACE, Stream_Buffer(response), responseLen > 512 ? 512 : responseLen);
	UINT status = callback->channel->Write(callback->channel, (ULONG)responseLen,
	                                       Stream_Buffer(response), nullptr);
	if (status != CHANNEL_RC_OK)
		WLog_ERR(TAG, "Write failed with 0x%08" PRIx32, status);
	Stream_Free(response, TRUE);
	free(request.request);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpewa_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	WINPR_UNUSED(pChannelCallback);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpewa_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPEWA_CHANNEL_CALLBACK* ecb = (RDPEWA_CHANNEL_CALLBACK*)pChannelCallback;

	/* Wait for any in-flight FIDO operation to complete */
	if (ecb->workerThread)
	{
		WLog_DBG(TAG, "Waiting for worker thread to finish...");
		WaitForSingleObject(ecb->workerThread, INFINITE);
		CloseHandle(ecb->workerThread);
		ecb->workerThread = nullptr;
	}

	free(ecb);

	return CHANNEL_RC_OK;
}

static const IWTSVirtualChannelCallback rdpewa_callbacks = { rdpewa_on_data_received,
	                                                         rdpewa_on_open, rdpewa_on_close,
	                                                         nullptr };

static UINT rdpewa_init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, rdpContext* rcontext,
                                  rdpSettings* settings)
{
	WINPR_ASSERT(base);
	WINPR_UNUSED(settings);

	RDPEWA_PLUGIN* rdpewa = (RDPEWA_PLUGIN*)base;
	rdpewa->rdp_context = rcontext;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT VCAPITYPE rdpewa_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, RDPEWA_DVC_CHANNEL_NAME,
	                                      sizeof(RDPEWA_PLUGIN), sizeof(RDPEWA_CHANNEL_CALLBACK),
	                                      &rdpewa_callbacks, rdpewa_init_plugin_cb, nullptr);
}

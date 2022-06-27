/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Core
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <stdarg.h>

#include "rdp.h"
#include "input.h"
#include "update.h"
#include "surface.h"
#include "transport.h"
#include "connection.h"
#include "message.h"
#include <freerdp/buildflags.h>
#include "gateway/rpc_fault.h"

#include <winpr/assert.h>

#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/stream.h>
#include <winpr/wtsapi.h>
#include <winpr/ssl.h>
#include <winpr/debug.h>

#include <freerdp/freerdp.h>
#include <freerdp/streamdump.h>
#include <freerdp/error.h>
#include <freerdp/event.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/channels/channels.h>
#include <freerdp/version.h>
#include <freerdp/log.h>
#include <freerdp/cache/pointer.h>

#include "settings.h"
#include "utils.h"

#define TAG FREERDP_TAG("core")

/** Creates a new connection based on the settings found in the "instance" parameter
 *  It will use the callbacks registered on the structure to process the pre/post connect operations
 *  that the caller requires.
 *  @see struct rdp_freerdp in freerdp.h
 *
 *  @param instance - pointer to a rdp_freerdp structure that contains base information to establish
 * the connection. On return, this function will be initialized with the new connection's settings.
 *
 *  @return TRUE if successful. FALSE otherwise.
 *
 */
BOOL freerdp_connect(freerdp* instance)
{
	UINT status2 = CHANNEL_RC_OK;
	rdpRdp* rdp;
	BOOL status = TRUE;
	rdpSettings* settings;
	ConnectionResultEventArgs e;

	if (!instance)
		return FALSE;

	WINPR_ASSERT(instance->context);

	/* We always set the return code to 0 before we start the connect sequence*/
	instance->ConnectionCallbackState = CLIENT_STATE_INITIAL;
	freerdp_set_last_error_log(instance->context, FREERDP_ERROR_SUCCESS);
	clearChannelError(instance->context);
	if (!utils_reset_abort(instance->context->rdp))
		return FALSE;

	rdp = instance->context->rdp;
	WINPR_ASSERT(rdp);

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	freerdp_channels_register_instance(instance->context->channels, instance);

	if (!freerdp_settings_set_default_order_support(settings))
		return FALSE;

	IFCALLRET(instance->PreConnect, status, instance);
	instance->ConnectionCallbackState = CLIENT_STATE_PRECONNECT_PASSED;

	if (status)
	{
		freerdp_settings_free(rdp->originalSettings);
		rdp->originalSettings = freerdp_settings_clone(settings);
		if (!rdp->originalSettings)
			return 0;

		BOOL ok = IFCALLRESULT(TRUE, instance->LoadChannels, instance);
		if (!ok)
			return 0;

		status2 = freerdp_channels_pre_connect(instance->context->channels, instance);
	}

	if (settings->KeyboardLayout == KBD_JAPANESE ||
	    settings->KeyboardLayout == KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002)
	{
		settings->KeyboardType = KBD_TYPE_JAPANESE;
		settings->KeyboardSubType = 2;
		settings->KeyboardFunctionKey = 12;
	}

	if (!status || (status2 != CHANNEL_RC_OK))
	{
		freerdp_set_last_error_if_not(instance->context, FREERDP_ERROR_PRE_CONNECT_FAILED);

		WLog_ERR(TAG, "freerdp_pre_connect failed");
		goto freerdp_connect_finally;
	}

	status = rdp_client_connect(rdp);

	/* Pointers might have changed inbetween */
	if (rdp && rdp->settings)
	{
		rdp_update_internal* up = update_cast(rdp->update);

		/* --authonly tests the connection without a UI */
		if (rdp->settings->AuthenticationOnly)
		{
			WLog_ERR(TAG, "Authentication only, exit status %" PRId32 "", status);
			goto freerdp_connect_finally;
		}

		if (rdp->settings->DumpRemoteFx)
		{
			up->pcap_rfx = pcap_open(rdp->settings->DumpRemoteFxFile, TRUE);

			if (up->pcap_rfx)
				up->dump_rfx = TRUE;
		}
	}

	if (status)
	{
		pointer_cache_register_callbacks(instance->context->update);
		IFCALLRET(instance->PostConnect, status, instance);
		instance->ConnectionCallbackState = CLIENT_STATE_POSTCONNECT_PASSED;

		if (status)
			status2 = freerdp_channels_post_connect(instance->context->channels, instance);
	}
	else
	{
		if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_CONNECT_TRANSPORT_FAILED)
			status = freerdp_reconnect(instance);
		else
			goto freerdp_connect_finally;
	}

	if (!status || (status2 != CHANNEL_RC_OK) || !update_post_connect(instance->context->update))
	{
		WLog_ERR(TAG, "freerdp_post_connect failed");

		freerdp_set_last_error_if_not(instance->context, FREERDP_ERROR_POST_CONNECT_FAILED);

		status = FALSE;
		goto freerdp_connect_finally;
	}

	if (settings->PlayRemoteFx)
	{
		wStream* s;
		rdp_update_internal* update = update_cast(instance->context->update);
		pcap_record record = { 0 };

		WINPR_ASSERT(update);
		update->pcap_rfx = pcap_open(settings->PlayRemoteFxFile, FALSE);
		status = FALSE;

		if (!update->pcap_rfx)
			goto freerdp_connect_finally;
		else
			update->play_rfx = TRUE;

		status = TRUE;

		while (pcap_has_next_record(update->pcap_rfx) && status)
		{
			pcap_get_next_record_header(update->pcap_rfx, &record);

			s = transport_take_from_pool(rdp->transport, record.length);
			if (!s)
				break;

			record.data = Stream_Buffer(s);
			pcap_get_next_record_content(update->pcap_rfx, &record);
			Stream_SetLength(s, record.length);
			Stream_SetPosition(s, 0);

			if (!update_begin_paint(&update->common))
				status = FALSE;
			else
			{
				if (update_recv_surfcmds(&update->common, s) < 0)
					status = FALSE;

				if (!update_end_paint(&update->common))
					status = FALSE;
			}

			Stream_Release(s);
		}

		pcap_close(update->pcap_rfx);
		update->pcap_rfx = NULL;
		goto freerdp_connect_finally;
	}

	if (rdp->errorInfo == ERRINFO_SERVER_INSUFFICIENT_PRIVILEGES)
		freerdp_set_last_error_log(instance->context, FREERDP_ERROR_INSUFFICIENT_PRIVILEGES);

	transport_set_connected_event(rdp->transport);

freerdp_connect_finally:
	EventArgsInit(&e, "freerdp");
	e.result = status ? 0 : -1;
	PubSub_OnConnectionResult(instance->context->pubSub, instance->context, &e);

	if (!status)
		freerdp_disconnect(instance);

	return status;
}

BOOL freerdp_abort_connect(freerdp* instance)
{
	if (!instance)
		return FALSE;

	return freerdp_abort_connect_context(instance->context);
}

BOOL freerdp_abort_connect_context(rdpContext* context)
{
	if (!context)
		return FALSE;

	freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_CANCELLED);

	return utils_abort_connect(context->rdp);
}

#if defined(WITH_FREERDP_DEPRECATED)
BOOL freerdp_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	rdpRdp* rdp;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	rdp = instance->context->rdp;
	WINPR_ASSERT(rdp);

	transport_get_fds(rdp->transport, rfds, rcount);
	return TRUE;
}
#endif

BOOL freerdp_check_fds(freerdp* instance)
{
	int status;
	rdpRdp* rdp;

	if (!instance)
		return FALSE;

	if (!instance->context)
		return FALSE;

	if (!instance->context->rdp)
		return FALSE;

	rdp = instance->context->rdp;
	status = rdp_check_fds(rdp);

	if (status < 0)
	{
		TerminateEventArgs e;
		rdpContext* context = instance->context;
		WLog_DBG(TAG, "rdp_check_fds() - %i", status);
		EventArgsInit(&e, "freerdp");
		e.code = 0;
		PubSub_OnTerminate(context->pubSub, context, &e);
		return FALSE;
	}

	return TRUE;
}

DWORD freerdp_get_event_handles(rdpContext* context, HANDLE* events, DWORD count)
{
	DWORD nCount = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);
	WINPR_ASSERT(events || (count == 0));

	nCount += transport_get_event_handles(context->rdp->transport, events, count);

	if (nCount == 0)
		return 0;

	if (events && (nCount < count + 2))
	{
		events[nCount++] = freerdp_channels_get_event_handle(context->instance);
		events[nCount++] = getChannelErrorEventHandle(context);
		events[nCount++] = utils_get_abort_event(context->rdp);
	}
	else
		return 0;

	return nCount;
}

BOOL freerdp_check_event_handles(rdpContext* context)
{
	BOOL status;

	WINPR_ASSERT(context);

	status = freerdp_check_fds(context->instance);

	if (!status)
	{
		if (freerdp_get_last_error(context) == FREERDP_ERROR_SUCCESS)
			WLog_ERR(TAG, "freerdp_check_fds() failed - %" PRIi32 "", status);

		return FALSE;
	}

	status = freerdp_channels_check_fds(context->channels, context->instance);

	if (!status)
	{
		if (freerdp_get_last_error(context) == FREERDP_ERROR_SUCCESS)
			WLog_ERR(TAG, "freerdp_channels_check_fds() failed - %" PRIi32 "", status);

		return FALSE;
	}

	status = checkChannelErrorEvent(context);

	if (!status)
	{
		if (freerdp_get_last_error(context) == FREERDP_ERROR_SUCCESS)
			WLog_ERR(TAG, "checkChannelErrorEvent() failed - %" PRIi32 "", status);

		return FALSE;
	}

	return status;
}

wMessageQueue* freerdp_get_message_queue(freerdp* instance, DWORD id)
{
	wMessageQueue* queue = NULL;
	rdpContext* context;

	WINPR_ASSERT(instance);

	context = instance->context;
	WINPR_ASSERT(context);

	switch (id)
	{
		case FREERDP_UPDATE_MESSAGE_QUEUE:
		{
			rdp_update_internal* update = update_cast(context->update);
			queue = update->queue;
		}
		break;

		case FREERDP_INPUT_MESSAGE_QUEUE:
		{
			rdp_input_internal* input = input_cast(context->input);
			queue = input->queue;
		}
		break;
	}

	return queue;
}

HANDLE freerdp_get_message_queue_event_handle(freerdp* instance, DWORD id)
{
	HANDLE event = NULL;
	wMessageQueue* queue = freerdp_get_message_queue(instance, id);

	if (queue)
		event = MessageQueue_Event(queue);

	return event;
}

int freerdp_message_queue_process_message(freerdp* instance, DWORD id, wMessage* message)
{
	int status = -1;
	rdpContext* context;

	WINPR_ASSERT(instance);

	context = instance->context;
	WINPR_ASSERT(context);

	switch (id)
	{
		case FREERDP_UPDATE_MESSAGE_QUEUE:
			status = update_message_queue_process_message(context->update, message);
			break;

		case FREERDP_INPUT_MESSAGE_QUEUE:
			status = input_message_queue_process_message(context->input, message);
			break;
	}

	return status;
}

int freerdp_message_queue_process_pending_messages(freerdp* instance, DWORD id)
{
	int status = -1;
	rdpContext* context;

	WINPR_ASSERT(instance);

	context = instance->context;
	WINPR_ASSERT(context);

	switch (id)
	{
		case FREERDP_UPDATE_MESSAGE_QUEUE:
			status = update_message_queue_process_pending_messages(context->update);
			break;

		case FREERDP_INPUT_MESSAGE_QUEUE:
			status = input_message_queue_process_pending_messages(context->input);
			break;
	}

	return status;
}

static BOOL freerdp_send_channel_data(freerdp* instance, UINT16 channelId, const BYTE* data,
                                      size_t size)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->rdp);
	return rdp_send_channel_data(instance->context->rdp, channelId, data, size);
}

static BOOL freerdp_send_channel_packet(freerdp* instance, UINT16 channelId, size_t totalSize,
                                        UINT32 flags, const BYTE* data, size_t chunkSize)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->rdp);
	return rdp_channel_send_packet(instance->context->rdp, channelId, totalSize, flags, data,
	                               chunkSize);
}

BOOL freerdp_disconnect(freerdp* instance)
{
	BOOL rc = TRUE;
	rdpRdp* rdp;
	rdp_update_internal* up;

	if (!instance || !instance->context)
		return FALSE;

	rdp = instance->context->rdp;
	utils_abort_connect(rdp);

	if (!rdp_client_disconnect(rdp))
		rc = FALSE;

	up = update_cast(rdp->update);

	update_post_disconnect(rdp->update);

	IFCALL(instance->PostDisconnect, instance);

	if (up->pcap_rfx)
	{
		up->dump_rfx = FALSE;
		pcap_close(up->pcap_rfx);
		up->pcap_rfx = NULL;
	}

	freerdp_channels_close(instance->context->channels, instance);
	return rc;
}

BOOL freerdp_disconnect_before_reconnect(freerdp* instance)
{
	WINPR_ASSERT(instance);
	return freerdp_disconnect_before_reconnect_context(instance->context);
}

BOOL freerdp_disconnect_before_reconnect_context(rdpContext* context)
{
	rdpRdp* rdp;

	WINPR_ASSERT(context);

	rdp = context->rdp;
	return rdp_client_disconnect_and_clear(rdp);
}

BOOL freerdp_reconnect(freerdp* instance)
{
	rdpRdp* rdp;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_CONNECT_CANCELLED)
		return FALSE;

	rdp = instance->context->rdp;

	if (!utils_reset_abort(instance->context->rdp))
		return FALSE;
	return rdp_client_reconnect(rdp);
}

BOOL freerdp_shall_disconnect(freerdp* instance)
{
	if (!instance)
		return FALSE;

	return freerdp_shall_disconnect_context(instance->context);
}

BOOL freerdp_shall_disconnect_context(rdpContext* context)
{
	if (!context)
		return FALSE;

	return utils_abort_event_is_set(context->rdp);
}

BOOL freerdp_focus_required(freerdp* instance)
{
	rdpRdp* rdp;
	BOOL bRetCode = FALSE;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	rdp = instance->context->rdp;
	WINPR_ASSERT(rdp);

	if (rdp->resendFocus)
	{
		bRetCode = TRUE;
		rdp->resendFocus = FALSE;
	}

	return bRetCode;
}

void freerdp_set_focus(freerdp* instance)
{
	rdpRdp* rdp;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	rdp = instance->context->rdp;
	WINPR_ASSERT(rdp);

	rdp->resendFocus = TRUE;
}

void freerdp_get_version(int* major, int* minor, int* revision)
{
	if (major != NULL)
		*major = FREERDP_VERSION_MAJOR;

	if (minor != NULL)
		*minor = FREERDP_VERSION_MINOR;

	if (revision != NULL)
		*revision = FREERDP_VERSION_REVISION;
}

const char* freerdp_get_version_string(void)
{
	return FREERDP_VERSION_FULL;
}

const char* freerdp_get_build_config(void)
{
	static const char build_config[] =
	    "Build configuration: " FREERDP_BUILD_CONFIG "\n"
	    "Build type:          " FREERDP_BUILD_TYPE "\n"
	    "CFLAGS:              " FREERDP_CFLAGS "\n"
	    "Compiler:            " FREERDP_COMPILER_ID ", " FREERDP_COMPILER_VERSION "\n"
	    "Target architecture: " FREERDP_TARGET_ARCH "\n";
	return build_config;
}

const char* freerdp_get_build_revision(void)
{
	return FREERDP_GIT_REVISION;
}

static wEventType FreeRDP_Events[] = {
	DEFINE_EVENT_ENTRY(WindowStateChange),   DEFINE_EVENT_ENTRY(ResizeWindow),
	DEFINE_EVENT_ENTRY(LocalResizeWindow),   DEFINE_EVENT_ENTRY(EmbedWindow),
	DEFINE_EVENT_ENTRY(PanningChange),       DEFINE_EVENT_ENTRY(ZoomingChange),
	DEFINE_EVENT_ENTRY(ErrorInfo),           DEFINE_EVENT_ENTRY(Terminate),
	DEFINE_EVENT_ENTRY(ConnectionResult),    DEFINE_EVENT_ENTRY(ChannelConnected),
	DEFINE_EVENT_ENTRY(ChannelDisconnected), DEFINE_EVENT_ENTRY(MouseEvent),
	DEFINE_EVENT_ENTRY(Activated),           DEFINE_EVENT_ENTRY(Timer),
	DEFINE_EVENT_ENTRY(GraphicsReset)
};

/** Allocator function for a rdp context.
 *  The function will allocate a rdpRdp structure using rdp_new(), then copy
 *  its contents to the appropriate fields in the rdp_freerdp structure given in parameters.
 *  It will also initialize the 'context' field in the rdp_freerdp structure as needed.
 *  If the caller has set the ContextNew callback in the 'instance' parameter, it will be called at
 * the end of the function.
 *
 *  @param instance - Pointer to the rdp_freerdp structure that will be initialized with the new
 * context.
 */
BOOL freerdp_context_new(freerdp* instance)
{
	return freerdp_context_new_ex(instance, NULL);
}

BOOL freerdp_context_new_ex(freerdp* instance, rdpSettings* settings)
{
	rdpRdp* rdp;
	rdpContext* context;
	BOOL ret = TRUE;

	WINPR_ASSERT(instance);

	instance->context = context = (rdpContext*)calloc(1, instance->ContextSize);

	if (!context)
		return FALSE;

	/* Set to external settings, prevents rdp_new from creating its own instance */
	context->settings = settings;
	context->instance = instance;
	context->ServerMode = FALSE;
	context->disconnectUltimatum = 0;
	context->pubSub = PubSub_New(TRUE);

	if (!context->pubSub)
		goto fail;

	PubSub_AddEventTypes(context->pubSub, FreeRDP_Events, ARRAYSIZE(FreeRDP_Events));
	context->metrics = metrics_new(context);

	if (!context->metrics)
		goto fail;

	rdp = rdp_new(context);

	if (!rdp)
		goto fail;

	context->rdp = rdp;
#if defined(WITH_FREERDP_DEPRECATED)
	instance->input = rdp->input;
	instance->update = rdp->update;
	instance->settings = rdp->settings;
	instance->autodetect = rdp->autodetect;
#endif

	instance->heartbeat = rdp->heartbeat;
	context->graphics = graphics_new(context);

	if (!context->graphics)
		goto fail;

	context->input = rdp->input;
	context->update = rdp->update;
	context->settings = rdp->settings;
	context->autodetect = rdp->autodetect;

	if (!(context->errorDescription = calloc(1, 500)))
	{
		WLog_ERR(TAG, "calloc failed!");
		goto fail;
	}

	if (!(context->channelErrorEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		goto fail;
	}

	update_register_client_callbacks(rdp->update);

	if (!(context->channels = freerdp_channels_new(instance)))
		goto fail;

	context->dump = stream_dump_new();
	if (!context->dump)
		goto fail;

	IFCALLRET(instance->ContextNew, ret, instance, context);

	if (ret)
		return TRUE;

fail:
	freerdp_context_free(instance);
	return FALSE;
}

/** Deallocator function for a rdp context.
 *  The function will deallocate the resources from the 'instance' parameter that were allocated
 * from a call to freerdp_context_new(). If the ContextFree callback is set in the 'instance'
 * parameter, it will be called before deallocation occurs.
 *
 *  @param instance - Pointer to the rdp_freerdp structure that was initialized by a call to
 * freerdp_context_new(). On return, the fields associated to the context are invalid.
 */
void freerdp_context_free(freerdp* instance)
{
	rdpContext* ctx;

	if (!instance)
		return;

	if (!instance->context)
		return;

	ctx = instance->context;

	IFCALL(instance->ContextFree, instance, ctx);
	rdp_free(ctx->rdp);
	ctx->rdp = NULL;
	ctx->settings = NULL; /* owned by rdpRdp */

	graphics_free(ctx->graphics);
	ctx->graphics = NULL;

	PubSub_Free(ctx->pubSub);
	ctx->pubSub = NULL;

	metrics_free(ctx->metrics);
	ctx->metrics = NULL;

	if (ctx->channelErrorEvent)
		CloseHandle(ctx->channelErrorEvent);
	ctx->channelErrorEvent = NULL;

	free(ctx->errorDescription);
	ctx->errorDescription = NULL;

	freerdp_channels_free(ctx->channels);
	ctx->channels = NULL;

	codecs_free(ctx->codecs);
	ctx->codecs = NULL;

	stream_dump_free(ctx->dump);
	ctx->dump = NULL;

	ctx->input = NULL;      /* owned by rdpRdp */
	ctx->update = NULL;     /* owned by rdpRdp */
	ctx->settings = NULL;   /* owned by rdpRdp */
	ctx->autodetect = NULL; /* owned by rdpRdp */

	free(ctx);
	instance->context = NULL;
#if defined(WITH_FREERDP_DEPRECATED)
	instance->input = NULL;      /* owned by rdpRdp */
	instance->update = NULL;     /* owned by rdpRdp */
	instance->settings = NULL;   /* owned by rdpRdp */
	instance->autodetect = NULL; /* owned by rdpRdp */
#endif
	instance->heartbeat = NULL; /* owned by rdpRdp */
}

int freerdp_get_disconnect_ultimatum(rdpContext* context)
{
	WINPR_ASSERT(context);
	return context->disconnectUltimatum;
}

UINT32 freerdp_error_info(freerdp* instance)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->rdp);
	return instance->context->rdp->errorInfo;
}

void freerdp_set_error_info(rdpRdp* rdp, UINT32 error)
{
	if (!rdp)
		return;

	rdp_set_error_info(rdp, error);
}

BOOL freerdp_send_error_info(rdpRdp* rdp)
{
	if (!rdp)
		return FALSE;

	return rdp_send_error_info(rdp);
}

UINT32 freerdp_get_last_error(rdpContext* context)
{
	WINPR_ASSERT(context);
	return context->LastError;
}

const char* freerdp_get_last_error_name(UINT32 code)
{
	const char* name = NULL;
	const UINT32 cls = GET_FREERDP_ERROR_CLASS(code);
	const UINT32 type = GET_FREERDP_ERROR_TYPE(code);

	switch (cls)
	{
		case FREERDP_ERROR_ERRBASE_CLASS:
			name = freerdp_get_error_base_name(type);
			break;

		case FREERDP_ERROR_ERRINFO_CLASS:
			name = freerdp_get_error_info_name(type);
			break;

		case FREERDP_ERROR_CONNECT_CLASS:
			name = freerdp_get_error_connect_name(type);
			break;

		default:
			name = rpc_error_to_string(code);
			break;
	}

	return name;
}

const char* freerdp_get_last_error_string(UINT32 code)
{
	const char* string = NULL;
	const UINT32 cls = GET_FREERDP_ERROR_CLASS(code);
	const UINT32 type = GET_FREERDP_ERROR_TYPE(code);

	switch (cls)
	{
		case FREERDP_ERROR_ERRBASE_CLASS:
			string = freerdp_get_error_base_string(type);
			break;

		case FREERDP_ERROR_ERRINFO_CLASS:
			string = freerdp_get_error_info_string(type);
			break;

		case FREERDP_ERROR_CONNECT_CLASS:
			string = freerdp_get_error_connect_string(type);
			break;

		default:
			string = rpc_error_to_string(code);
			break;
	}

	return string;
}

const char* freerdp_get_last_error_category(UINT32 code)
{
	const char* string = NULL;
	const UINT32 cls = GET_FREERDP_ERROR_CLASS(code);
	const UINT32 type = GET_FREERDP_ERROR_TYPE(code);

	switch (cls)
	{
		case FREERDP_ERROR_ERRBASE_CLASS:
			string = freerdp_get_error_base_category(type);
			break;

		case FREERDP_ERROR_ERRINFO_CLASS:
			string = freerdp_get_error_info_category(type);
			break;

		case FREERDP_ERROR_CONNECT_CLASS:
			string = freerdp_get_error_connect_category(type);
			break;

		default:
			string = rpc_error_to_category(code);
			break;
	}

	return string;
}

void freerdp_set_last_error(rdpContext* context, UINT32 lastError)
{
	freerdp_set_last_error_ex(context, lastError, NULL, NULL, -1);
}

void freerdp_set_last_error_ex(rdpContext* context, UINT32 lastError, const char* fkt,
                               const char* file, int line)
{
	WINPR_ASSERT(context);

	if (lastError)
		WLog_ERR(TAG, "%s:%s %s [0x%08" PRIX32 "]", fkt, __FUNCTION__,
		         freerdp_get_last_error_name(lastError), lastError);

	if (lastError == FREERDP_ERROR_SUCCESS)
	{
		WLog_DBG(TAG, "%s:%s resetting error state", fkt, __FUNCTION__);
	}
	else if (context->LastError != FREERDP_ERROR_SUCCESS)
	{
		WLog_ERR(TAG, "%s: TODO: Trying to set error code %s, but %s already set!", fkt,
		         freerdp_get_last_error_name(lastError),
		         freerdp_get_last_error_name(context->LastError));
	}

	context->LastError = lastError;
}

const char* freerdp_get_logon_error_info_type(UINT32 type)
{
	switch (type)
	{
		case LOGON_MSG_DISCONNECT_REFUSED:
			return "LOGON_MSG_DISCONNECT_REFUSED";

		case LOGON_MSG_NO_PERMISSION:
			return "LOGON_MSG_NO_PERMISSION";

		case LOGON_MSG_BUMP_OPTIONS:
			return "LOGON_MSG_BUMP_OPTIONS";

		case LOGON_MSG_RECONNECT_OPTIONS:
			return "LOGON_MSG_RECONNECT_OPTIONS";

		case LOGON_MSG_SESSION_TERMINATE:
			return "LOGON_MSG_SESSION_TERMINATE";

		case LOGON_MSG_SESSION_CONTINUE:
			return "LOGON_MSG_SESSION_CONTINUE";

		default:
			return "UNKNOWN";
	}
}

const char* freerdp_get_logon_error_info_data(UINT32 data)
{
	switch (data)
	{
		case LOGON_FAILED_BAD_PASSWORD:
			return "LOGON_FAILED_BAD_PASSWORD";

		case LOGON_FAILED_UPDATE_PASSWORD:
			return "LOGON_FAILED_UPDATE_PASSWORD";

		case LOGON_FAILED_OTHER:
			return "LOGON_FAILED_OTHER";

		case LOGON_WARNING:
			return "LOGON_WARNING";

		default:
			return "SESSION_ID";
	}
}

/** Allocator function for the rdp_freerdp structure.
 *  @return an allocated structure filled with 0s. Need to be deallocated using freerdp_free()
 */
freerdp* freerdp_new(void)
{
	freerdp* instance;
	instance = (freerdp*)calloc(1, sizeof(freerdp));

	if (!instance)
		return NULL;

	instance->ContextSize = sizeof(rdpContext);
	instance->SendChannelData = freerdp_send_channel_data;
	instance->SendChannelPacket = freerdp_send_channel_packet;
	instance->ReceiveChannelData = freerdp_channels_data;
	return instance;
}

/** Deallocator function for the rdp_freerdp structure.
 *  @param instance - pointer to the rdp_freerdp structure to deallocate.
 *                    On return, this pointer is not valid anymore.
 */
void freerdp_free(freerdp* instance)
{
	free(instance);
}

ULONG freerdp_get_transport_sent(rdpContext* context, BOOL resetCount)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);
	return transport_get_bytes_sent(context->rdp->transport, resetCount);
}

BOOL freerdp_nla_impersonate(rdpContext* context)
{
	rdpNla* nla;

	if (!context)
		return FALSE;

	if (!context->rdp)
		return FALSE;

	if (!context->rdp->transport)
		return FALSE;

	nla = transport_get_nla(context->rdp->transport);
	return nla_impersonate(nla);
}

BOOL freerdp_nla_revert_to_self(rdpContext* context)
{
	rdpNla* nla;

	if (!context)
		return FALSE;

	if (!context->rdp)
		return FALSE;

	if (!context->rdp->transport)
		return FALSE;

	nla = transport_get_nla(context->rdp->transport);
	return nla_revert_to_self(nla);
}

HANDLE getChannelErrorEventHandle(rdpContext* context)
{
	WINPR_ASSERT(context);
	return context->channelErrorEvent;
}

BOOL checkChannelErrorEvent(rdpContext* context)
{
	WINPR_ASSERT(context);

	if (WaitForSingleObject(context->channelErrorEvent, 0) == WAIT_OBJECT_0)
	{
		WLog_ERR(TAG, "%s. Error was %" PRIu32 "", context->errorDescription,
		         context->channelErrorNum);
		return FALSE;
	}

	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT getChannelError(rdpContext* context)
{
	WINPR_ASSERT(context);
	return context->channelErrorNum;
}

const char* getChannelErrorDescription(rdpContext* context)
{
	WINPR_ASSERT(context);
	return context->errorDescription;
}

void clearChannelError(rdpContext* context)
{
	WINPR_ASSERT(context);
	context->channelErrorNum = 0;
	memset(context->errorDescription, 0, 500);
	ResetEvent(context->channelErrorEvent);
}

void setChannelError(rdpContext* context, UINT errorNum, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);

	WINPR_ASSERT(context);

	context->channelErrorNum = errorNum;
	vsnprintf(context->errorDescription, 499, format, ap);
	va_end(ap);
	SetEvent(context->channelErrorEvent);
}

const char* freerdp_nego_get_routing_token(rdpContext* context, DWORD* length)
{
	if (!context || !context->rdp)
		return NULL;

	return (const char*)nego_get_routing_token(context->rdp->nego, length);
}

const rdpTransportIo* freerdp_get_io_callbacks(rdpContext* context)
{
	WINPR_ASSERT(context);
	return rdp_get_io_callbacks(context->rdp);
}

BOOL freerdp_set_io_callbacks(rdpContext* context, const rdpTransportIo* io_callbacks)
{
	WINPR_ASSERT(context);
	return rdp_set_io_callbacks(context->rdp, io_callbacks);
}

BOOL freerdp_set_io_callback_context(rdpContext* context, void* usercontext)
{
	WINPR_ASSERT(context);
	return rdp_set_io_callback_context(context->rdp, usercontext);
}

void* freerdp_get_io_callback_context(rdpContext* context)
{
	WINPR_ASSERT(context);
	return rdp_get_io_callback_context(context->rdp);
}

CONNECTION_STATE freerdp_get_state(const rdpContext* context)
{
	WINPR_ASSERT(context);
	return rdp_get_state(context->rdp);
}

const char* freerdp_state_string(CONNECTION_STATE state)
{
	return rdp_state_string(state);
}

BOOL freerdp_channels_from_mcs(rdpSettings* settings, const rdpContext* context)
{
	WINPR_ASSERT(context);
	return rdp_channels_from_mcs(settings, context->rdp);
}

HANDLE freerdp_abort_event(rdpContext* context)
{
	WINPR_ASSERT(context);
	return utils_get_abort_event(context->rdp);
}

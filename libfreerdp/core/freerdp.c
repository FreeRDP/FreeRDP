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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rdp.h"
#include "input.h"
#include "update.h"
#include "surface.h"
#include "transport.h"
#include "connection.h"
#include "message.h"

#include <assert.h>

#include <winpr/string.h>
#include <winpr/stream.h>
#include <winpr/wtsapi.h>

#include <freerdp/freerdp.h>
#include <freerdp/error.h>
#include <freerdp/event.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/channels/channels.h>
#include <freerdp/version.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("core")

/* connectErrorCode is 'extern' in error.h. See comment there.*/

/** Creates a new connection based on the settings found in the "instance" parameter
 *  It will use the callbacks registered on the structure to process the pre/post connect operations
 *  that the caller requires.
 *  @see struct rdp_freerdp in freerdp.h
 *
 *  @param instance - pointer to a rdp_freerdp structure that contains base information to establish the connection.
 *  				  On return, this function will be initialized with the new connection's settings.
 *
 *  @return TRUE if successful. FALSE otherwise.
 *
 */
BOOL freerdp_connect(freerdp* instance)
{
	rdpRdp* rdp;
	BOOL status = TRUE;
	rdpSettings* settings;
	ConnectionResultEventArgs e;

	/* We always set the return code to 0 before we start the connect sequence*/
	connectErrorCode = 0;
	freerdp_set_last_error(instance->context, FREERDP_ERROR_SUCCESS);
	clearChannelError(instance->context);

	rdp = instance->context->rdp;
	settings = instance->settings;

	instance->context->codecs = codecs_new(instance->context);
	IFCALLRET(instance->PreConnect, status, instance);

	if (settings->KeyboardLayout == KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002)
	{
		settings->KeyboardType = 7;
		settings->KeyboardSubType = 2;
		settings->KeyboardFunctionKey = 12;
	}

	if (!status)
	{
		if (!freerdp_get_last_error(rdp->context))
			freerdp_set_last_error(instance->context, FREERDP_ERROR_PRE_CONNECT_FAILED);

		WLog_ERR(TAG, "freerdp_pre_connect failed");
		goto freerdp_connect_finally;
	}

	status = rdp_client_connect(rdp);

	/* --authonly tests the connection without a UI */
	if (instance->settings->AuthenticationOnly)
	{
		WLog_ERR(TAG, "Authentication only, exit status %d", !status);
		goto freerdp_connect_finally;
	}

	if (!status)
		goto freerdp_connect_finally;

	if (status)
	{
		if (instance->settings->DumpRemoteFx)
		{
			instance->update->pcap_rfx = pcap_open(instance->settings->DumpRemoteFxFile, TRUE);
			if (instance->update->pcap_rfx)
				instance->update->dump_rfx = TRUE;
		}

		IFCALLRET(instance->PostConnect, status, instance);

		if (!status || !update_post_connect(instance->update))
		{
			WLog_ERR(TAG, "freerdp_post_connect failed");

			if (!freerdp_get_last_error(rdp->context))
				freerdp_set_last_error(instance->context, FREERDP_ERROR_POST_CONNECT_FAILED);

			goto freerdp_connect_finally;
		}

		if (instance->settings->PlayRemoteFx)
		{
			wStream* s;
			rdpUpdate* update;
			pcap_record record;

			update = instance->update;

			update->pcap_rfx = pcap_open(settings->PlayRemoteFxFile, FALSE);

			if (!update->pcap_rfx)
			{
				status = FALSE;
				goto freerdp_connect_finally;
			}
			else
			{
				update->play_rfx = TRUE;
			}

			while (pcap_has_next_record(update->pcap_rfx))
			{

				pcap_get_next_record_header(update->pcap_rfx, &record);

				if (!(s = StreamPool_Take(rdp->transport->ReceivePool, record.length)))
					break;

				record.data = Stream_Buffer(s);

				pcap_get_next_record_content(update->pcap_rfx, &record);
				Stream_SetLength(s,record.length);
				Stream_SetPosition(s, 0);

				update->BeginPaint(update->context);
				update_recv_surfcmds(update, Stream_Length(s) , s);
				update->EndPaint(update->context);
				Stream_Release(s);
			}

			pcap_close(update->pcap_rfx);
			update->pcap_rfx = NULL;
			status = TRUE;
			goto freerdp_connect_finally;
		}
	}

	if (rdp->errorInfo == ERRINFO_SERVER_INSUFFICIENT_PRIVILEGES)
		freerdp_set_last_error(instance->context, FREERDP_ERROR_INSUFFICIENT_PRIVILEGES);

	SetEvent(rdp->transport->connectedEvent);
freerdp_connect_finally:
	EventArgsInit(&e, "freerdp");
	e.result = status ? 0 : -1;
	PubSub_OnConnectionResult(instance->context->pubSub, instance->context, &e);

	return status;
}

BOOL freerdp_abort_connect(freerdp* instance)
{
	if (!instance || !instance->context)
		return FALSE;

	return SetEvent(instance->context->abortEvent);
}

BOOL freerdp_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	rdpRdp* rdp = instance->context->rdp;
	transport_get_fds(rdp->transport, rfds, rcount);
	return TRUE;
}

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

	nCount += transport_get_event_handles(context->rdp->transport, events, count);

	if (nCount == 0)
		return 0;

	if (events && (nCount < count + 1))
	{
		events[nCount++] = freerdp_channels_get_event_handle(context->instance);
		events[nCount++] = getChannelErrorEventHandle(context);
	}
	else
		return 0;

	return nCount;
}

BOOL freerdp_check_event_handles(rdpContext* context)
{
	BOOL status;

	status = freerdp_check_fds(context->instance);

	if (!status)
	{
		WLog_ERR(TAG, "freerdp_check_fds() failed - %i", status);
		return FALSE;
	}

	status = freerdp_channels_check_fds(context->channels, context->instance);
	if (!status)
	{
		WLog_ERR(TAG, "freerdp_channels_check_fds() failed - %i", status);
		return FALSE;
	}

	if (!status)
		return FALSE;

	status = checkChannelErrorEvent(context);

	return status;
}

wMessageQueue* freerdp_get_message_queue(freerdp* instance, DWORD id)
{
	wMessageQueue* queue = NULL;

	switch (id)
	{
		case FREERDP_UPDATE_MESSAGE_QUEUE:
			queue = instance->update->queue;
			break;

		case FREERDP_INPUT_MESSAGE_QUEUE:
			queue = instance->input->queue;
			break;
	}

	return queue;
}

HANDLE freerdp_get_message_queue_event_handle(freerdp* instance, DWORD id)
{
	HANDLE event = NULL;
	wMessageQueue* queue = NULL;

	queue = freerdp_get_message_queue(instance, id);

	if (queue)
		event = MessageQueue_Event(queue);

	return event;
}

int freerdp_message_queue_process_message(freerdp* instance, DWORD id, wMessage* message)
{
	int status = -1;

	switch (id)
	{
		case FREERDP_UPDATE_MESSAGE_QUEUE:
			status = update_message_queue_process_message(instance->update, message);
			break;

		case FREERDP_INPUT_MESSAGE_QUEUE:
			status = input_message_queue_process_message(instance->input, message);
			break;
	}

	return status;
}

int freerdp_message_queue_process_pending_messages(freerdp* instance, DWORD id)
{
	int status = -1;

	switch (id)
	{
		case FREERDP_UPDATE_MESSAGE_QUEUE:
			status = update_message_queue_process_pending_messages(instance->update);
			break;

		case FREERDP_INPUT_MESSAGE_QUEUE:
			status = input_message_queue_process_pending_messages(instance->input);
			break;
	}

	return status;
}

static int freerdp_send_channel_data(freerdp* instance, UINT16 channelId, BYTE* data, int size)
{
	return rdp_send_channel_data(instance->context->rdp, channelId, data, size);
}

BOOL freerdp_disconnect(freerdp* instance)
{
	rdpRdp* rdp;

	rdp = instance->context->rdp;

	rdp_client_disconnect(rdp);
	update_post_disconnect(instance->update);

	IFCALL(instance->PostDisconnect, instance);

	if (instance->update->pcap_rfx)
	{
		instance->update->dump_rfx = FALSE;
		pcap_close(instance->update->pcap_rfx);
		instance->update->pcap_rfx = NULL;
	}

	codecs_free(instance->context->codecs);
	return TRUE;
}

BOOL freerdp_reconnect(freerdp* instance)
{
	BOOL status;
	rdpRdp* rdp = instance->context->rdp;

	status = rdp_client_reconnect(rdp);

	return status;
}

BOOL freerdp_shall_disconnect(freerdp* instance)
{
	if (!instance || !instance->context)
		return FALSE;
	if (WaitForSingleObject(instance->context->abortEvent, 0) != WAIT_OBJECT_0)
		return FALSE;
	return TRUE;
}

FREERDP_API BOOL freerdp_focus_required(freerdp* instance)
{
	rdpRdp* rdp;
	BOOL bRetCode = FALSE;

	rdp = instance->context->rdp;

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

	rdp = instance->context->rdp;
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

const char* freerdp_get_build_date(void)
{
	static char build_date[64];

	sprintf_s(build_date, sizeof(build_date), "%s %s", __DATE__, __TIME__);

	return build_date;
}

const char* freerdp_get_build_revision(void)
{
	return GIT_REVISION;
}

static wEventType FreeRDP_Events[] =
{
	DEFINE_EVENT_ENTRY(WindowStateChange)
	DEFINE_EVENT_ENTRY(ResizeWindow)
	DEFINE_EVENT_ENTRY(LocalResizeWindow)
	DEFINE_EVENT_ENTRY(EmbedWindow)
	DEFINE_EVENT_ENTRY(PanningChange)
	DEFINE_EVENT_ENTRY(ZoomingChange)
	DEFINE_EVENT_ENTRY(ErrorInfo)
	DEFINE_EVENT_ENTRY(Terminate)
	DEFINE_EVENT_ENTRY(ConnectionResult)
	DEFINE_EVENT_ENTRY(ChannelConnected)
	DEFINE_EVENT_ENTRY(ChannelDisconnected)
	DEFINE_EVENT_ENTRY(MouseEvent)
};

/** Allocator function for a rdp context.
 *  The function will allocate a rdpRdp structure using rdp_new(), then copy
 *  its contents to the appropriate fields in the rdp_freerdp structure given in parameters.
 *  It will also initialize the 'context' field in the rdp_freerdp structure as needed.
 *  If the caller has set the ContextNew callback in the 'instance' parameter, it will be called at the end of the function.
 *
 *  @param instance - Pointer to the rdp_freerdp structure that will be initialized with the new context.
 */
BOOL freerdp_context_new(freerdp* instance)
{
	rdpRdp* rdp;
	rdpContext* context;
	BOOL ret = TRUE;

	instance->context = (rdpContext*) calloc(1, instance->ContextSize);
	if (!instance->context)
		return FALSE;

	context = instance->context;
	context->instance = instance;

	context->ServerMode = FALSE;
	context->settings = instance->settings;

	context->pubSub = PubSub_New(TRUE);
	if(!context->pubSub)
		goto out_error_pubsub;
	PubSub_AddEventTypes(context->pubSub, FreeRDP_Events, sizeof(FreeRDP_Events) / sizeof(wEventType));

	context->metrics = metrics_new(context);
	if (!context->metrics)
		goto out_error_metrics_new;

	rdp = rdp_new(context);
	if (!rdp)
		goto out_error_rdp_new;

	instance->input = rdp->input;
	instance->update = rdp->update;
	instance->settings = rdp->settings;
	instance->autodetect = rdp->autodetect;

	context->graphics = graphics_new(context);
	if(!context->graphics)
		goto out_error_graphics_new;

	context->rdp = rdp;

	context->input = instance->input;
	context->update = instance->update;
	context->settings = instance->settings;
	context->autodetect = instance->autodetect;

	instance->update->context = instance->context;
	instance->update->pointer->context = instance->context;
	instance->update->primary->context = instance->context;
	instance->update->secondary->context = instance->context;
	instance->update->altsec->context = instance->context;

	instance->input->context = context;

	instance->autodetect->context = context;

	if (!(context->errorDescription = calloc(1, 500)))
	{
		WLog_ERR(TAG, "calloc failed!");
		goto out_error_description;
	}

	if (!(context->channelErrorEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		goto out_error_create_event;
	}

	update_register_client_callbacks(rdp->update);

	instance->context->abortEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!instance->context->abortEvent)
		goto out_error_abort_event;

	IFCALLRET(instance->ContextNew, ret, instance, instance->context);

	if (ret)
		return TRUE;

	CloseHandle(context->abortEvent);
out_error_abort_event:
	CloseHandle(context->channelErrorEvent);
out_error_create_event:
	free(context->errorDescription);
out_error_description:
	graphics_free(context->graphics);
out_error_graphics_new:
	rdp_free(rdp);
out_error_rdp_new:
	metrics_free(context->metrics);
out_error_metrics_new:
	PubSub_Free(context->pubSub);
out_error_pubsub:
	free(instance->context);
	return FALSE;
}

/** Deallocator function for a rdp context.
 *  The function will deallocate the resources from the 'instance' parameter that were allocated from a call
 *  to freerdp_context_new().
 *  If the ContextFree callback is set in the 'instance' parameter, it will be called before deallocation occurs.
 *
 *  @param instance - Pointer to the rdp_freerdp structure that was initialized by a call to freerdp_context_new().
 *  				  On return, the fields associated to the context are invalid.
 */
void freerdp_context_free(freerdp* instance)
{
	if (!instance)
		return;

	if (!instance->context)
		return;

	IFCALL(instance->ContextFree, instance, instance->context);

	rdp_free(instance->context->rdp);
	instance->context->rdp = NULL;

	graphics_free(instance->context->graphics);
	instance->context->graphics = NULL;

	PubSub_Free(instance->context->pubSub);

	metrics_free(instance->context->metrics);

	CloseHandle(instance->context->channelErrorEvent);
	free(instance->context->errorDescription);

	CloseHandle(instance->context->abortEvent);
	instance->context->abortEvent = NULL;

	free(instance->context);
	instance->context = NULL;

}

UINT32 freerdp_error_info(freerdp* instance)
{
	return instance->context->rdp->errorInfo;
}

void freerdp_set_error_info(rdpRdp* rdp, UINT32 error) {
	rdp->errorInfo = error;
}

UINT32 freerdp_get_last_error(rdpContext* context)
{
	return context->LastError;
}

const char* freerdp_get_last_error_name(UINT32 code)
{
	const char *name = NULL;
	const UINT32 cls = GET_FREERDP_ERROR_CLASS(code);
	const UINT32 type = GET_FREERDP_ERROR_TYPE(code);

	switch(cls)
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
			name = "Unknown error class";
			break;
	}

	return name;
}

const char* freerdp_get_last_error_string(UINT32 code)
{
	const char* string = NULL;
	const UINT32 cls = GET_FREERDP_ERROR_CLASS(code);
	const UINT32 type = GET_FREERDP_ERROR_TYPE(code);

	switch(cls)
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
			string = "Unknown error class";
			break;
	}

	return string;
}

void freerdp_set_last_error(rdpContext* context, UINT32 lastError)
{
	if (lastError)
		WLog_ERR(TAG, "freerdp_set_last_error %s [0x%04X]",
			freerdp_get_last_error_name(lastError), lastError);

	context->LastError = lastError;

	switch (lastError)
	{
		case FREERDP_ERROR_PRE_CONNECT_FAILED:
			connectErrorCode = PREECONNECTERROR;
			break;

		case FREERDP_ERROR_CONNECT_UNDEFINED:
			connectErrorCode = UNDEFINEDCONNECTERROR;
			break;

		case FREERDP_ERROR_POST_CONNECT_FAILED:
			connectErrorCode = POSTCONNECTERROR;
			break;

		case FREERDP_ERROR_DNS_ERROR:
			connectErrorCode = DNSERROR;
			break;

		case FREERDP_ERROR_DNS_NAME_NOT_FOUND:
			connectErrorCode = DNSNAMENOTFOUND;
			break;

		case FREERDP_ERROR_CONNECT_FAILED:
			connectErrorCode = CONNECTERROR;
			break;

		case FREERDP_ERROR_MCS_CONNECT_INITIAL_ERROR:
			connectErrorCode = MCSCONNECTINITIALERROR;
			break;

		case FREERDP_ERROR_TLS_CONNECT_FAILED:
			connectErrorCode = TLSCONNECTERROR;
			break;

		case FREERDP_ERROR_AUTHENTICATION_FAILED:
			connectErrorCode = AUTHENTICATIONERROR;
			break;

		case FREERDP_ERROR_INSUFFICIENT_PRIVILEGES:
			connectErrorCode = INSUFFICIENTPRIVILEGESERROR;
			break;

		case FREERDP_ERROR_CONNECT_CANCELLED:
			connectErrorCode = CANCELEDBYUSER;
			break;

		case FREERDP_ERROR_SECURITY_NEGO_CONNECT_FAILED:
			connectErrorCode = CONNECTERROR;
			break;

		case FREERDP_ERROR_CONNECT_TRANSPORT_FAILED:
			connectErrorCode = CONNECTERROR;
			break;
	}
}

/** Allocator function for the rdp_freerdp structure.
 *  @return an allocated structure filled with 0s. Need to be deallocated using freerdp_free()
 */
freerdp* freerdp_new()
{
	freerdp* instance;

	instance = (freerdp*) calloc(1, sizeof(freerdp));

	if (!instance)
		return NULL;

	instance->ContextSize = sizeof(rdpContext);
	instance->SendChannelData = freerdp_send_channel_data;
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

FREERDP_API ULONG freerdp_get_transport_sent(rdpContext* context, BOOL resetCount) {
	ULONG written = context->rdp->transport->written;
	if (resetCount)
		context->rdp->transport->written = 0;
	return written;
}

FREERDP_API HANDLE getChannelErrorEventHandle(rdpContext* context)
{
	return context->channelErrorEvent;
}

FREERDP_API BOOL checkChannelErrorEvent(rdpContext* context)
{
	if (WaitForSingleObject( context->channelErrorEvent, 0) == WAIT_OBJECT_0)
	{
		WLog_ERR(TAG, "%s. Error was %lu", context->errorDescription, context->channelErrorNum);
		return FALSE;
	}
	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_API UINT getChannelError(rdpContext* context)
{
	return context->channelErrorNum;
}

FREERDP_API const char* getChannelErrorDescription(rdpContext* context)
{
	return context->errorDescription;
}

FREERDP_API void clearChannelError(rdpContext* context)
{
	context->channelErrorNum = 0;
	memset(context->errorDescription, 0, 500);
	ResetEvent(context->channelErrorEvent);
}

FREERDP_API void setChannelError(rdpContext* context, UINT errorNum, char* description)
{
	context->channelErrorNum = errorNum;
	strncpy(context->errorDescription, description, 499);
	SetEvent(context->channelErrorEvent);
}

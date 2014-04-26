/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Core
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/wtsapi.h>

#include <freerdp/freerdp.h>
#include <freerdp/error.h>
#include <freerdp/event.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/channels/channels.h>
#include <freerdp/version.h>

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
	rdpSettings* settings;
	BOOL status = FALSE;
	ConnectionResultEventArgs e;

	/* We always set the return code to 0 before we start the connect sequence*/
	connectErrorCode = 0;
	freerdp_set_last_error(instance->context, FREERDP_ERROR_SUCCESS);

	rdp = instance->context->rdp;
	settings = instance->settings;

	IFCALLRET(instance->PreConnect, status, instance);

	if (settings->KeyboardLayout == KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002)
	{
		settings->KeyboardType = 7;
		settings->KeyboardSubType = 2;
		settings->KeyboardFunctionKey = 12;
	}

	if (!status)
	{
		if (!connectErrorCode)
		{
			connectErrorCode = PREECONNECTERROR;
		}

		if (!freerdp_get_last_error(rdp->context))
		{
			freerdp_set_last_error(instance->context, FREERDP_ERROR_PRE_CONNECT_FAILED);
		}

		fprintf(stderr, "freerdp_pre_connect failed\n");

		goto freerdp_connect_finally;
	}

	status = rdp_client_connect(rdp);

	/* --authonly tests the connection without a UI */
	if (instance->settings->AuthenticationOnly)
	{
		fprintf(stderr, "Authentication only, exit status %d\n", !status);
		goto freerdp_connect_finally;
	}

	if (status)
	{
		if (instance->settings->DumpRemoteFx)
		{
			instance->update->pcap_rfx = pcap_open(instance->settings->DumpRemoteFxFile, TRUE);
			if (instance->update->pcap_rfx)
				instance->update->dump_rfx = TRUE;
		}

		IFCALLRET(instance->PostConnect, status, instance);
		update_post_connect(instance->update);

		if (!status)
		{
			fprintf(stderr, "freerdp_post_connect failed\n");

			if (!connectErrorCode)
			{
				connectErrorCode = POSTCONNECTERROR;
			}

			if (!freerdp_get_last_error(rdp->context))
			{
				freerdp_set_last_error(instance->context, FREERDP_ERROR_POST_CONNECT_FAILED);
			}

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

				s = StreamPool_Take(rdp->transport->ReceivePool, record.length);
				record.data = Stream_Buffer(s);

				pcap_get_next_record_content(update->pcap_rfx, &record);
				Stream_SetLength(s,record.length);
				Stream_SetPosition(s, 0);

				update->BeginPaint(update->context);
				update_recv_surfcmds(update, Stream_Length(s) , s);
				update->EndPaint(update->context);
				Stream_Release(s);
			
				StreamPool_Return(rdp->transport->ReceivePool, s);
			}

			pcap_close(update->pcap_rfx);
			update->pcap_rfx = NULL;
			status = TRUE;
			goto freerdp_connect_finally;
		}
	}

	if (rdp->errorInfo == ERRINFO_SERVER_INSUFFICIENT_PRIVILEGES)
	{
		connectErrorCode = INSUFFICIENTPRIVILEGESERROR;
		freerdp_set_last_error(instance->context, FREERDP_ERROR_INSUFFICIENT_PRIVILEGES);
	}

	SetEvent(rdp->transport->connectedEvent);

	freerdp_connect_finally:

	EventArgsInit(&e, "freerdp");
	e.result = status ? 0 : -1;
	PubSub_OnConnectionResult(instance->context->pubSub, instance->context, &e);

	return status;
}

BOOL freerdp_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	rdpRdp* rdp;

	rdp = instance->context->rdp;
	transport_get_fds(rdp->transport, rfds, rcount);

	return TRUE;
}

BOOL freerdp_check_fds(freerdp* instance)
{
	int status;
	rdpRdp* rdp;

	assert(instance);
	assert(instance->context);
	assert(instance->context->rdp);

	rdp = instance->context->rdp;

	status = rdp_check_fds(rdp);

	if (status < 0)
	{
		TerminateEventArgs e;
		rdpContext* context = instance->context;

		EventArgsInit(&e, "freerdp");
		e.code = 0;
		PubSub_OnTerminate(context->pubSub, context, &e);

		return FALSE;
	}

	return TRUE;
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
	transport_disconnect(rdp->transport);

	IFCALL(instance->PostDisconnect, instance);

	if (instance->update->pcap_rfx)
	{
		instance->update->dump_rfx = FALSE;
		pcap_close(instance->update->pcap_rfx);
		instance->update->pcap_rfx = NULL;
	}

	return TRUE;
}

BOOL freerdp_reconnect(freerdp* instance)
{
	return rdp_client_reconnect(instance->context->rdp);
}

BOOL freerdp_shall_disconnect(freerdp* instance)
{
	return instance->context->rdp->disconnect;
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

void freerdp_get_version(int* major, int* minor, int* revision)
{
	if (major != NULL)
		*major = FREERDP_VERSION_MAJOR;

	if (minor != NULL)
		*minor = FREERDP_VERSION_MINOR;

	if (revision != NULL)
		*revision = FREERDP_VERSION_REVISION;
}

static wEventType FreeRDP_Events[] =
{
		DEFINE_EVENT_ENTRY(WindowStateChange)
		DEFINE_EVENT_ENTRY(ResizeWindow)
		DEFINE_EVENT_ENTRY(LocalResizeWindow)
		DEFINE_EVENT_ENTRY(EmbedWindow)
		DEFINE_EVENT_ENTRY(PanningChange)
		DEFINE_EVENT_ENTRY(ScalingFactorChange)
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
int freerdp_context_new(freerdp* instance)
{
	rdpRdp* rdp;
	rdpContext* context;

	instance->context = (rdpContext*) malloc(instance->ContextSize);
	ZeroMemory(instance->context, instance->ContextSize);

	context = instance->context;
	context->instance = instance;

	context->ServerMode = FALSE;
	context->settings = instance->settings;

	context->pubSub = PubSub_New(TRUE);
	PubSub_AddEventTypes(context->pubSub, FreeRDP_Events, sizeof(FreeRDP_Events) / sizeof(wEventType));

	rdp = rdp_new(context);
	instance->input = rdp->input;
	instance->update = rdp->update;
	instance->settings = rdp->settings;

	context->graphics = graphics_new(context);
	context->rdp = rdp;

	context->input = instance->input;
	context->update = instance->update;
	context->settings = instance->settings;

	instance->update->context = instance->context;
	instance->update->pointer->context = instance->context;
	instance->update->primary->context = instance->context;
	instance->update->secondary->context = instance->context;
	instance->update->altsec->context = instance->context;

	instance->input->context = context;

	update_register_client_callbacks(rdp->update);

	IFCALL(instance->ContextNew, instance, instance->context);

	return 0;
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

	free(instance->context);
	instance->context = NULL;
}

UINT32 freerdp_error_info(freerdp* instance)
{
	return instance->context->rdp->errorInfo;
}

UINT32 freerdp_get_last_error(rdpContext* context)
{
	return context->LastError;
}

void freerdp_set_last_error(rdpContext* context, UINT32 lastError)
{
	if (lastError)
		fprintf(stderr, "freerdp_set_last_error 0x%04X\n", lastError);

	context->LastError = lastError;
}

/** Allocator function for the rdp_freerdp structure.
 *  @return an allocated structure filled with 0s. Need to be deallocated using freerdp_free()
 */
freerdp* freerdp_new()
{
	freerdp* instance;

	instance = (freerdp*) malloc(sizeof(freerdp));

	if (instance)
	{
		ZeroMemory(instance, sizeof(freerdp));
		instance->ContextSize = sizeof(rdpContext);
		instance->SendChannelData = freerdp_send_channel_data;
		instance->ReceiveChannelData = freerdp_channels_data;
	}

	return instance;
}

/** Deallocator function for the rdp_freerdp structure.
 *  @param instance - pointer to the rdp_freerdp structure to deallocate.
 *                    On return, this pointer is not valid anymore.
 */
void freerdp_free(freerdp* instance)
{
	if (instance)
	{
		free(instance);
	}
}

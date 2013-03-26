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
#include "extension.h"
#include "message.h"

#include <winpr/crt.h>

#include <freerdp/freerdp.h>
#include <freerdp/error.h>
#include <freerdp/locale/keyboard.h>

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

	/* We always set the return code to 0 before we start the connect sequence*/
	connectErrorCode = 0;

	rdp = instance->context->rdp;
	settings = instance->settings;

	IFCALLRET(instance->PreConnect, status, instance);

	if (settings->KeyboardLayout == KBD_JAPANESE_INPUT_SYSTEM_MS_IME2002)
	{
		settings->KeyboardType = 7;
		settings->KeyboardSubType = 2;
		settings->KeyboardFunctionKey = 12;
	}

	extension_load_and_init_plugins(rdp->extension);
	extension_pre_connect(rdp->extension);

	if (status != TRUE)
	{
		if (!connectErrorCode)
		{
			connectErrorCode = PREECONNECTERROR;
		}
		fprintf(stderr, "%s:%d: freerdp_pre_connect failed\n", __FILE__, __LINE__);
		return FALSE;
	}

	status = rdp_client_connect(rdp);
	/* --authonly tests the connection without a UI */
	if (instance->settings->AuthenticationOnly)
	{
		fprintf(stderr, "%s:%d: Authentication only, exit status %d\n", __FILE__, __LINE__, !status);
		return status;
	}

	if (status)
	{
		if (instance->settings->DumpRemoteFx)
		{
			instance->update->pcap_rfx = pcap_open(instance->settings->DumpRemoteFxFile, TRUE);
			if (instance->update->pcap_rfx)
				instance->update->dump_rfx = TRUE;
		}

		extension_post_connect(rdp->extension);

		IFCALLRET(instance->PostConnect, status, instance);
		update_post_connect(instance->update);

		if (status != TRUE)
		{
			printf("freerdp_post_connect failed\n");
			
			if (!connectErrorCode)
			{
				connectErrorCode = POSTCONNECTERROR;
			}
			
			return FALSE;
		}

		if (instance->settings->PlayRemoteFx)
		{
			wStream* s;
			rdpUpdate* update;
			pcap_record record;

			s = stream_new(1024);
			instance->update->pcap_rfx = pcap_open(instance->settings->PlayRemoteFxFile, FALSE);

			if (instance->update->pcap_rfx)
				instance->update->play_rfx = TRUE;
			
			update = instance->update;

			while (instance->update->play_rfx && pcap_has_next_record(update->pcap_rfx))
			{
				pcap_get_next_record_header(update->pcap_rfx, &record);

				s->buffer = (BYTE*) realloc(s->buffer, record.length);
				record.data = s->buffer;
				s->capacity = record.length;

				pcap_get_next_record_content(update->pcap_rfx, &record);
				stream_set_pos(s, 0);

				update->BeginPaint(update->context);
				update_recv_surfcmds(update, s->capacity, s);
				update->EndPaint(update->context);
			}

			free(s->buffer);
			return TRUE;
		}
	}

	if (!connectErrorCode)
	{
		connectErrorCode = UNDEFINEDCONNECTERROR;
	}
	
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

	rdp = instance->context->rdp;

	status = rdp_check_fds(rdp);

	if (status < 0)
		return FALSE;

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

static int freerdp_send_channel_data(freerdp* instance, int channel_id, BYTE* data, int size)
{
	return rdp_send_channel_data(instance->context->rdp, channel_id, data, size);
}

BOOL freerdp_disconnect(freerdp* instance)
{
	rdpRdp* rdp;

	rdp = instance->context->rdp;
	transport_disconnect(rdp->transport);

	return TRUE;
}

BOOL freerdp_shall_disconnect(freerdp* instance)
{
	return instance->context->rdp->disconnect;
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

/** Allocator function for a rdp context.
 *  The function will allocate a rdpRdp structure using rdp_new(), then copy
 *  its contents to the appropriate fields in the rdp_freerdp structure given in parameters.
 *  It will also initialize the 'context' field in the rdp_freerdp structure as needed.
 *  If the caller has set the ContextNew callback in the 'instance' parameter, it will be called at the end of the function.
 *
 *  @param instance - Pointer to the rdp_freerdp structure that will be initialized with the new context.
 */
void freerdp_context_new(freerdp* instance)
{
	rdpRdp* rdp;

	rdp = rdp_new(instance);
	// FIXME - we're not checking where rdp_new returns NULL, and have no way to report an error to the caller

	instance->input = rdp->input;
	instance->update = rdp->update;
	instance->settings = rdp->settings;

	instance->context = (rdpContext*) malloc(instance->context_size);
	ZeroMemory(instance->context, instance->context_size);

	instance->context->graphics = graphics_new(instance->context);
	instance->context->instance = instance;
	instance->context->rdp = rdp;

	instance->context->input = instance->input;
	instance->context->update = instance->update;
	instance->context->settings = instance->settings;

	instance->update->context = instance->context;
	instance->update->pointer->context = instance->context;
	instance->update->primary->context = instance->context;
	instance->update->secondary->context = instance->context;
	instance->update->altsec->context = instance->context;

	instance->input->context = instance->context;

	update_register_client_callbacks(rdp->update);

	IFCALL(instance->ContextNew, instance, instance->context);
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
	if (instance->context == NULL)
		return;

	IFCALL(instance->ContextFree, instance, instance->context);

	rdp_free(instance->context->rdp);
	graphics_free(instance->context->graphics);

	free(instance->context);
	instance->context = NULL;
}

UINT32 freerdp_error_info(freerdp* instance)
{
	return instance->context->rdp->errorInfo;
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
		instance->context_size = sizeof(rdpContext);
		instance->SendChannelData = freerdp_send_channel_data;
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

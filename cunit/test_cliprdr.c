/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Clipboard Virtual Channel Unit Tests
 *
 * Copyright 2011 Vic Lee
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
#include <stdlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/channels/channels.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/memory.h>
#include <freerdp/plugins/cliprdr.h>

#include "test_cliprdr.h"

int init_cliprdr_suite(void)
{
	freerdp_channels_global_init();
	return 0;
}

int clean_cliprdr_suite(void)
{
	freerdp_channels_global_uninit();
	return 0;
}

int add_cliprdr_suite(void)
{
	add_test_suite(cliprdr);

	add_test_function(cliprdr);

	return 0;
}

static const uint8 test_clip_caps_data[] =
{
	"\x07\x00\x00\x00\x10\x00\x00\x00\x01\x00\x00\x00\x01\x00\x0C\x00"
	"\x02\x00\x00\x00\x0E\x00\x00\x00"
};

static const uint8 test_monitor_ready_data[] =
{
	"\x01\x00\x00\x00\x00\x00\x00\x00"
};

static const uint8 test_format_list_data[] =
{
	"\x02\x00\x00\x00\x48\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\xd0\x00\x00"
	"\x48\x00\x54\x00\x4D\x00\x4C\x00\x20\x00\x46\x00\x6F\x00\x72\x00"
	"\x6D\x00\x61\x00\x74\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
};

static const uint8 test_format_list_response_data[] =
{
	"\x03\x00\x01\x00\x00\x00\x00\x00"
};

static const uint8 test_data_request_data[] =
{
	"\x04\x00\x00\x00\x04\x00\x00\x00\x01\x00\x00\x00"
};

static const uint8 test_data_response_data[] =
{
	"\x05\x00\x01\x00\x18\x00\x00\x00\x68\x00\x65\x00\x6C\x00\x6C\x00"
	"\x6F\x00\x20\x00\x77\x00\x6F\x00\x72\x00\x6c\x00\x64\x00\x00\x00"
};

static int test_rdp_channel_data(freerdp* instance, int chan_id, uint8* data, int data_size)
{
	printf("chan_id %d data_size %d\n", chan_id, data_size);
	freerdp_hexdump(data, data_size);
	return 0;
}

static int event_processed;

static void event_process_callback(RDP_EVENT* event)
{
	printf("Event %d processed.\n", event->event_type);
	event_processed = 1;
}

void test_cliprdr(void)
{
	int i;
	rdpChannels* channels;
	rdpSettings settings = { 0 };
	freerdp instance = { 0 };
	RDP_EVENT* event;
	RDP_CB_FORMAT_LIST_EVENT* format_list_event;
	RDP_CB_DATA_REQUEST_EVENT* data_request_event;
	RDP_CB_DATA_RESPONSE_EVENT* data_response_event;

	settings.hostname = "testhost";
	instance.settings = &settings;
	instance.SendChannelData = test_rdp_channel_data;

	channels = freerdp_channels_new();

	freerdp_channels_load_plugin(channels, &settings, "../channels/cliprdr/cliprdr.so", NULL);
	freerdp_channels_pre_connect(channels, &instance);
	freerdp_channels_post_connect(channels, &instance);

	/* server sends cliprdr capabilities and monitor ready PDU */
	freerdp_channels_data(&instance, 0, (char*)test_clip_caps_data, sizeof(test_clip_caps_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_clip_caps_data) - 1);

	freerdp_channels_data(&instance, 0, (char*)test_monitor_ready_data, sizeof(test_monitor_ready_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_monitor_ready_data) - 1);

	/* cliprdr sends clipboard_sync event to UI */
	while ((event = freerdp_channels_pop_event(channels)) == NULL)
	{
		freerdp_channels_check_fds(channels, &instance);
	}
	printf("Got event %d\n", event->event_type);
	CU_ASSERT(event->event_type == RDP_EVENT_TYPE_CB_MONITOR_READY);
	freerdp_event_free(event);

	/* UI sends format_list event to cliprdr */
	event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_FORMAT_LIST, event_process_callback, NULL);
	format_list_event = (RDP_CB_FORMAT_LIST_EVENT*) event;
	format_list_event->num_formats = 2;
	format_list_event->formats = (uint32*) xmalloc(sizeof(uint32) * 2);
	format_list_event->formats[0] = CB_FORMAT_TEXT;
	format_list_event->formats[1] = CB_FORMAT_HTML;
	event_processed = 0;
	freerdp_channels_send_event(channels, event);

	/* cliprdr sends format list PDU to server */
	while (!event_processed)
	{
		freerdp_channels_check_fds(channels, &instance);
	}

	/* server sends format list response PDU to cliprdr */
	freerdp_channels_data(&instance, 0, (char*)test_format_list_response_data, sizeof(test_format_list_response_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_format_list_response_data) - 1);

	/* server sends format list PDU to cliprdr */
	freerdp_channels_data(&instance, 0, (char*)test_format_list_data, sizeof(test_format_list_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_format_list_data) - 1);

	/* cliprdr sends format_list event to UI */
	while ((event = freerdp_channels_pop_event(channels)) == NULL)
	{
		freerdp_channels_check_fds(channels, &instance);
	}
	printf("Got event %d\n", event->event_type);
	CU_ASSERT(event->event_type == RDP_EVENT_TYPE_CB_FORMAT_LIST);
	if (event->event_type == RDP_EVENT_TYPE_CB_FORMAT_LIST)
	{
		format_list_event = (RDP_CB_FORMAT_LIST_EVENT*)event;
		for (i = 0; i < format_list_event->num_formats; i++)
			printf("Format: 0x%X\n", format_list_event->formats[i]);
	}
	freerdp_event_free(event);

	/* server sends data request PDU to cliprdr */
	freerdp_channels_data(&instance, 0, (char*)test_data_request_data, sizeof(test_data_request_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_data_request_data) - 1);

	/* cliprdr sends data request event to UI */
	while ((event = freerdp_channels_pop_event(channels)) == NULL)
	{
		freerdp_channels_check_fds(channels, &instance);
	}
	printf("Got event %d\n", event->event_type);
	CU_ASSERT(event->event_type == RDP_EVENT_TYPE_CB_DATA_REQUEST);
	if (event->event_type == RDP_EVENT_TYPE_CB_DATA_REQUEST)
	{
		data_request_event = (RDP_CB_DATA_REQUEST_EVENT*)event;
		printf("Requested format: 0x%X\n", data_request_event->format);
	}
	freerdp_event_free(event);

	/* UI sends data response event to cliprdr */
	event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_DATA_RESPONSE, event_process_callback, NULL);
	data_response_event = (RDP_CB_DATA_RESPONSE_EVENT*)event;
	data_response_event->data = (uint8*)xmalloc(6);
	strcpy((char*)data_response_event->data, "hello");
	data_response_event->size = 6;
	event_processed = 0;
	freerdp_channels_send_event(channels, event);

	/* cliprdr sends data response PDU to server */
	while (!event_processed)
	{
		freerdp_channels_check_fds(channels, &instance);
	}

	/* UI sends data request event to cliprdr */
	event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_DATA_REQUEST, event_process_callback, NULL);
	data_request_event = (RDP_CB_DATA_REQUEST_EVENT*)event;
	data_request_event->format = CB_FORMAT_UNICODETEXT;
	event_processed = 0;
	freerdp_channels_send_event(channels, event);

	/* cliprdr sends data request PDU to server */
	while (!event_processed)
	{
		freerdp_channels_check_fds(channels, &instance);
	}

	/* server sends data response PDU to cliprdr */
	freerdp_channels_data(&instance, 0, (char*)test_data_response_data, sizeof(test_data_response_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_data_response_data) - 1);

	/* cliprdr sends data response event to UI */
	while ((event = freerdp_channels_pop_event(channels)) == NULL)
	{
		freerdp_channels_check_fds(channels, &instance);
	}
	printf("Got event %d\n", event->event_type);
	CU_ASSERT(event->event_type == RDP_EVENT_TYPE_CB_DATA_RESPONSE);
	if (event->event_type == RDP_EVENT_TYPE_CB_DATA_RESPONSE)
	{
		data_response_event = (RDP_CB_DATA_RESPONSE_EVENT*)event;
		printf("Data response size: %d\n", data_response_event->size);
		freerdp_hexdump(data_response_event->data, data_response_event->size);
	}
	freerdp_event_free(event);

	freerdp_channels_close(channels, &instance);
	freerdp_channels_free(channels);
}

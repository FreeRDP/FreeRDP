/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Channel Manager Unit Tests
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

#include "test_channels.h"

int init_chanman_suite(void)
{
	freerdp_channels_global_init();
	return 0;
}

int clean_chanman_suite(void)
{
	freerdp_channels_global_uninit();
	return 0;
}

int add_chanman_suite(void)
{
	add_test_suite(chanman);

	add_test_function(chanman);

	return 0;
}

static int test_rdp_channel_data(freerdp* instance, int chan_id, uint8* data, int data_size)
{
	printf("chan_id %d data_size %d\n", chan_id, data_size);
	return 0;
}

void test_chanman(void)
{
	rdpChannels* chan_man;
	rdpSettings settings = { 0 };
	freerdp instance = { 0 };
	RDP_EVENT* event;

	settings.hostname = "testhost";
	instance.settings = &settings;
	instance.SendChannelData = test_rdp_channel_data;

	chan_man = freerdp_channels_new();

	freerdp_channels_load_plugin(chan_man, &settings, "../channels/rdpdbg/rdpdbg.so", NULL);
	freerdp_channels_pre_connect(chan_man, &instance);
	freerdp_channels_post_connect(chan_man, &instance);

	freerdp_channels_data(&instance, 0, "testdata", 8, CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, 8);
	freerdp_channels_data(&instance, 0, "testdata1", 9, CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, 9);
	freerdp_channels_data(&instance, 0, "testdata11", 10, CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, 10);
	freerdp_channels_data(&instance, 0, "testdata111", 11, CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, 11);

	event = freerdp_event_new(RDP_EVENT_CLASS_DEBUG, 0, NULL, NULL);
	freerdp_channels_send_event(chan_man, event);

	while ((event = freerdp_channels_pop_event(chan_man)) == NULL)
	{
		freerdp_channels_check_fds(chan_man, &instance);
	}
	printf("responded event_type %d\n", event->event_type);
	freerdp_event_free(event);

	freerdp_channels_close(chan_man, &instance);
	freerdp_channels_free(chan_man);
}

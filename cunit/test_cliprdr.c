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
#include <freerdp/chanman.h>
#include <freerdp/utils/event.h>

#include "test_cliprdr.h"

int init_cliprdr_suite(void)
{
	freerdp_chanman_global_init();
	return 0;
}

int clean_cliprdr_suite(void)
{
	freerdp_chanman_global_uninit();
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

static int test_rdp_channel_data(rdpInst* inst, int chan_id, char* data, int data_size)
{
	printf("chan_id %d data_size %d\n", chan_id, data_size);
}

void test_cliprdr(void)
{
	rdpChanMan* chan_man;
	rdpSettings settings = { 0 };
	rdpInst inst = { 0 };
	FRDP_EVENT* event;

	settings.hostname = "testhost";
	inst.settings = &settings;
	inst.rdp_channel_data = test_rdp_channel_data;

	chan_man = freerdp_chanman_new();

	freerdp_chanman_load_plugin(chan_man, &settings, "../channels/cliprdr/cliprdr.so", NULL);
	freerdp_chanman_pre_connect(chan_man, &inst);
	freerdp_chanman_post_connect(chan_man, &inst);

	freerdp_chanman_data(&inst, 0, (char*)test_clip_caps_data, sizeof(test_clip_caps_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_clip_caps_data) - 1);

	freerdp_chanman_data(&inst, 0, (char*)test_monitor_ready_data, sizeof(test_monitor_ready_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_monitor_ready_data) - 1);

	while ((event = freerdp_chanman_pop_event(chan_man)) == NULL)
	{
		freerdp_chanman_check_fds(chan_man, &inst);
	}
	printf("Got event %d\n", event->event_type);
	CU_ASSERT(event->event_type == FRDP_EVENT_TYPE_CB_SYNC);
	freerdp_event_free(event);

	freerdp_chanman_close(chan_man, &inst);
	freerdp_chanman_free(chan_man);
}

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
#include <freerdp/chanman.h>
#include <freerdp/utils/event.h>

#include "test_chanman.h"

int init_chanman_suite(void)
{
	freerdp_chanman_global_init();
	return 0;
}

int clean_chanman_suite(void)
{
	freerdp_chanman_global_uninit();
	return 0;
}

int add_chanman_suite(void)
{
	add_test_suite(chanman);

	add_test_function(chanman);

	return 0;
}

static int test_rdp_channel_data(rdpInst* inst, int chan_id, char* data, int data_size)
{
	printf("chan_id %d data_size %d\n", chan_id, data_size);
}

void test_chanman(void)
{
	rdpChanMan* chan_man;
	rdpSettings settings = { 0 };
	rdpInst inst = { 0 };
	FRDP_EVENT* event;

	settings.hostname = "testhost";
	inst.settings = &settings;
	inst.rdp_channel_data = test_rdp_channel_data;

	chan_man = freerdp_chanman_new();

	freerdp_chanman_load_plugin(chan_man, &settings, "../channels/rdpdbg/rdpdbg.so", NULL);
	freerdp_chanman_pre_connect(chan_man, &inst);
	freerdp_chanman_post_connect(chan_man, &inst);

	freerdp_chanman_data(&inst, 0, "testdata", 8, CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, 8);
	freerdp_chanman_data(&inst, 0, "testdata1", 9, CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, 9);
	freerdp_chanman_data(&inst, 0, "testdata11", 10, CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, 10);
	freerdp_chanman_data(&inst, 0, "testdata111", 11, CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, 11);

	event = freerdp_event_new(FRDP_EVENT_TYPE_DEBUG, NULL, NULL);
	freerdp_chanman_send_event(chan_man, "rdpdbg", event);

	while ((event = freerdp_chanman_pop_event(chan_man)) == NULL)
	{
		freerdp_chanman_check_fds(chan_man, &inst);
	}
	printf("responded event_type %d\n", event->event_type);
	freerdp_event_free(event);

	freerdp_chanman_close(chan_man, &inst);
	freerdp_chanman_free(chan_man);
}

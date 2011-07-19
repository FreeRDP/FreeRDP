/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Dynamic Virtual Channel Unit Tests
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
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/memory.h>

#include "test_drdynvc.h"

int init_drdynvc_suite(void)
{
	freerdp_chanman_global_init();
	return 0;
}

int clean_drdynvc_suite(void)
{
	freerdp_chanman_global_uninit();
	return 0;
}

int add_drdynvc_suite(void)
{
	add_test_suite(drdynvc);

	add_test_function(drdynvc);

	return 0;
}

static const uint8 test_capability_request_data[] =
{
	"\x58\x00\x02\x00\x33\x33\x11\x11\x3D\x0A\xA7\x04"
};

static int data_received = 0;

static int test_rdp_channel_data(rdpInst* inst, int chan_id, char* data, int data_size)
{
	printf("chan_id %d data_size %d\n", chan_id, data_size);
	freerdp_hexdump(data, data_size);
	data_received = 1;
}

void test_drdynvc(void)
{
	rdpChanMan* chan_man;
	rdpSettings settings = { 0 };
	rdpInst inst = { 0 };
	int i;

	settings.hostname = "testhost";
	inst.settings = &settings;
	inst.rdp_channel_data = test_rdp_channel_data;

	chan_man = freerdp_chanman_new();

	freerdp_chanman_load_plugin(chan_man, &settings, "../channels/drdynvc/drdynvc.so", NULL);
	freerdp_chanman_pre_connect(chan_man, &inst);
	freerdp_chanman_post_connect(chan_man, &inst);

	/* server sends capability request PDU */
	freerdp_chanman_data(&inst, 0, (char*)test_capability_request_data, sizeof(test_capability_request_data) - 1,
		CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(test_capability_request_data) - 1);

	/* drdynvc sends capability response PDU to server */
	data_received = 0;
	while (!data_received)
	{
		freerdp_chanman_check_fds(chan_man, &inst);
	}

	freerdp_chanman_close(chan_man, &inst);
	freerdp_chanman_free(chan_man);
}

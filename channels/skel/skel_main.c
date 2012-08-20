/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Audio Output Virtual Channel
 *
 * Copyright 2009-2012 Jay Sorg
 * Copyright 2010-2012 Vic Lee
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

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/utils/svc_plugin.h>

#include "skel_main.h"

struct skel_plugin
{
	rdpSvcPlugin plugin;

	/* put your private data here */

};

static void skel_process_interval(rdpSvcPlugin* plugin)
{
	printf("skel_process_interval:\n");
}

static void skel_process_receive(rdpSvcPlugin* plugin, STREAM* data_in)
{
	skelPlugin* skel = (skelPlugin*)plugin;
	STREAM* data_out;
	int bytes;

	printf("skel_process_receive:\n");

	if (skel == NULL)
	{
		printf("skel_process_receive: skel is nil\n");
		return;
	}

	/* process data in(from server) here */
	/* here we just send the same data back */

	bytes = stream_get_size(data_in);
	printf("skel_process_receive: got bytes %d\n", bytes);
	if (bytes > 0)
	{
		data_out = stream_new(bytes);
		stream_copy(data_out, data_in, bytes);
		/* svc_plugin_send takes ownership of data_out, that is why
		   we do not free it */

		bytes = stream_get_length(data_in);
		printf("skel_process_receive: sending bytes %d\n", bytes);

		svc_plugin_send(plugin, data_out);
	}

	stream_free(data_in);
}

static void skel_process_connect(rdpSvcPlugin* plugin)
{
	skelPlugin* skel = (skelPlugin*)plugin;
	DEBUG_SVC("connecting");

	printf("skel_process_connect:\n");

	if (skel == NULL)
	{
		return;
	}

	/* if you want a call from channel thread once is a while do this */
	plugin->interval_ms = 1000;
	plugin->interval_callback = skel_process_interval;

}

static void skel_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	printf("skel_process_event:\n");

	/* events comming from main freerdp window to plugin */
	/* send them back with svc_plugin_send_event */

	freerdp_event_free(event);
}

static void skel_process_terminate(rdpSvcPlugin* plugin)
{
	skelPlugin* skel = (skelPlugin*)plugin;

	printf("skel_process_terminate:\n");

	if (skel == NULL)
	{
		return;
	}

	/* put your cleanup here */

	xfree(plugin);
}

DEFINE_SVC_PLUGIN(skel, "skel",
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP)

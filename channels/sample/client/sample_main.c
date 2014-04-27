/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Sample Virtual Channel
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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/utils/svc_plugin.h>

#include "sample_main.h"

struct sample_plugin
{
	rdpSvcPlugin plugin;

	/* put your private data here */
};

static void sample_process_receive(rdpSvcPlugin* plugin, wStream* data_in)
{
	int bytes;
	wStream* data_out;
	samplePlugin* sample = (samplePlugin*) plugin;

	fprintf(stderr, "sample_process_receive:\n");

	if (!sample)
	{
		fprintf(stderr, "sample_process_receive: sample is nil\n");
		return;
	}

	/* process data in (from server) here */
	/* here we just send the same data back */

	bytes = Stream_Capacity(data_in);
	fprintf(stderr, "sample_process_receive: got bytes %d\n", bytes);

	if (bytes > 0)
	{
		data_out = Stream_New(NULL, bytes);
		Stream_Copy(data_out, data_in, bytes);
		/* svc_plugin_send takes ownership of data_out, that is why
		   we do not free it */

		bytes = Stream_GetPosition(data_in);
		fprintf(stderr, "sample_process_receive: sending bytes %d\n", bytes);

		svc_plugin_send(plugin, data_out);
	}

	Stream_Free(data_in, TRUE);
}

static void sample_process_connect(rdpSvcPlugin* plugin)
{
	samplePlugin* sample = (samplePlugin*) plugin;
	DEBUG_SVC("connecting");

	fprintf(stderr, "sample_process_connect:\n");

	if (!sample)
		return;
}

static void sample_process_event(rdpSvcPlugin* plugin, wMessage* event)
{
	fprintf(stderr, "sample_process_event:\n");

	/* events coming from main freerdp window to plugin */
	/* send them back with svc_plugin_send_event */

	freerdp_event_free(event);
}

static void sample_process_terminate(rdpSvcPlugin* plugin)
{
	samplePlugin* sample = (samplePlugin*) plugin;

	fprintf(stderr, "sample_process_terminate:\n");

	if (!sample)
		return;

	/* put your cleanup here */

	free(plugin);
}

#define VirtualChannelEntry	sample_VirtualChannelEntry

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	samplePlugin* _p;

	_p = (samplePlugin*) malloc(sizeof(samplePlugin));
	ZeroMemory(_p, sizeof(samplePlugin));

	_p->plugin.channel_def.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP;

	strcpy(_p->plugin.channel_def.name, "sample");

	_p->plugin.connect_callback = sample_process_connect;
	_p->plugin.receive_callback = sample_process_receive;
	_p->plugin.event_callback = sample_process_event;
	_p->plugin.terminate_callback = sample_process_terminate;

	svc_plugin_init((rdpSvcPlugin*) _p, pEntryPoints);

	return 1;
}

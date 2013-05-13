/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
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

#ifndef __DRDYNVC_MAIN_H
#define __DRDYNVC_MAIN_H

#include <freerdp/api.h>
#include <freerdp/svc.h>
#include <freerdp/addin.h>
#include <freerdp/client/drdynvc.h>
#include <freerdp/utils/svc_plugin.h>

#define CREATE_REQUEST_PDU		0x01
#define DATA_FIRST_PDU			0x02
#define DATA_PDU			0x03
#define CLOSE_REQUEST_PDU		0x04
#define CAPABILITY_REQUEST_PDU		0x05

typedef struct drdynvc_plugin drdynvcPlugin;

struct drdynvc_plugin
{
	rdpSvcPlugin plugin;

	DrdynvcClientContext* context;

	int version;
	int PriorityCharge0;
	int PriorityCharge1;
	int PriorityCharge2;
	int PriorityCharge3;
	int channel_error;

	IWTSVirtualChannelManager* channel_mgr;
};

int drdynvc_write_data(drdynvcPlugin* plugin, UINT32 ChannelId, BYTE* data, UINT32 data_size);
int drdynvc_push_event(drdynvcPlugin* plugin, wMessage* event);

#endif

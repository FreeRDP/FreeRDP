/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Security
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH - Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
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

#ifndef __TSG_H_
#define __TSG_H_

typedef struct rdp_tsg rdpTsg;

#include "transport.h"
#include "rpch.h"

#include <time.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/utils/debug.h>

struct rdp_tsg
{
	struct rdp_settings* settings;
	struct rdp_transport* transport;
	struct rdp_rpch* rpch;

	uint8* tunnelContext;
	uint8* channelContext;
};

boolean tsg_connect(rdpTsg* tsg, const char* hostname, uint16 port);
rdpTsg* tsg_new(rdpSettings* settings);
void tsg_free(rdpTsg* tsg);
int tsg_write(rdpTsg* tsg, uint8* data, uint32 length);
int tsg_read(rdpTsg* tsg, uint* data, uint32 length);

#ifdef WITH_DEBUG_TSG
#define DEBUG_TSG(fmt, ...) DEBUG_CLASS(TSG, fmt, ## __VA_ARGS__)
#else
#define DEBUG_TSG(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __TSG_H_ */

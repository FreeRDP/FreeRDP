/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Connection Sequence
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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "tpkt.h"
#include "nego.h"
#include "mcs.h"
#include "transport.h"

#include <freerdp/settings.h>
#include <freerdp/utils/memory.h>

struct rdp_connection
{
	struct rdp_mcs* mcs;
	struct rdp_nego* nego;
	struct rdp_settings* settings;
	struct rdp_transport* transport;
};
typedef struct rdp_connection rdpConnection;

void connection_client_connect(rdpConnection* connection);

rdpConnection* connection_new();
void connection_free(rdpConnection* connection);

#endif /* __CONNECTION_H */

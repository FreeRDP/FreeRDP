/**
 * FreeRDP: A Remote Desktop Protocol Client
 * T.125 Multipoint Communication Service (MCS) Protocol
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

#ifndef __MCS_H
#define __MCS_H

#include "ber.h"
#include "transport.h"

#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

typedef struct
{
	uint32 maxChannelIds;
	uint32 maxUserIds;
	uint32 maxTokenIds;
	uint32 numPriorities;
	uint32 minThroughput;
	uint32 maxHeight;
	uint32 maxMCSPDUsize;
	uint32 protocolVersion;
} DOMAIN_PARAMETERS;

struct rdp_mcs
{
	struct rdp_transport* transport;
	DOMAIN_PARAMETERS targetParameters;
	DOMAIN_PARAMETERS minimumParameters;
	DOMAIN_PARAMETERS maximumParameters;
};
typedef struct rdp_mcs rdpMcs;

#define MCS_TYPE_CONNECT_INITIAL		0x65
#define MCS_TYPE_CONNECT_RESPONSE		0x66

void mcs_write_connect_initial(STREAM* s, rdpMcs* mcs, STREAM* user_data);

void mcs_send_connect_initial(rdpMcs* mcs);

rdpMcs* mcs_new(rdpTransport* transport);
void mcs_free(rdpMcs* mcs);

#endif /* __MCS_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multitransport PDUs
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
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

#ifndef __MULTITRANSPORT_H
#define __MULTITRANSPORT_H

typedef struct rdp_multitransport rdpMultitransport;

#include "rdp.h"

#include <freerdp/freerdp.h>

#include <winpr/stream.h>

struct rdp_multitransport
{
	UINT32 placeholder;
};

int rdp_recv_multitransport_packet(rdpRdp* rdp, wStream* s);

rdpMultitransport* multitransport_new(void);
void multitransport_free(rdpMultitransport* multitransport);

#endif /* __MULTITRANSPORT_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Heartbeat PDUs
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

#ifndef FREERDP_LIB_CORE_HEARTBEET_H
#define FREERDP_LIB_CORE_HEARTBEET_H

#include "rdp.h"

#include <freerdp/heartbeat.h>
#include <freerdp/freerdp.h>
#include <freerdp/log.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

#include "state.h"

FREERDP_LOCAL state_run_t rdp_recv_heartbeat_packet(rdpRdp* rdp, wStream* s);

FREERDP_LOCAL rdpHeartbeat* heartbeat_new(void);
FREERDP_LOCAL void heartbeat_free(rdpHeartbeat* heartbeat);

#define HEARTBEAT_TAG FREERDP_TAG("core.heartbeat")

#endif /* FREERDP_LIB_CORE_HEARTBEET_H */

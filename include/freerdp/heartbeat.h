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

#ifndef FREERDP_HEARTBEAT_H
#define FREERDP_HEARTBEAT_H

#include <freerdp/types.h>

typedef struct rdp_heartbeat rdpHeartbeat;

typedef BOOL (*pServerHeartbeat)(freerdp* instance, BYTE period, BYTE count1, BYTE count2);

struct rdp_heartbeat
{
	pServerHeartbeat ServerHeartbeat;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL freerdp_heartbeat_send_heartbeat_pdu(freerdp_peer* peer, BYTE period,
	                                                      BYTE count1, BYTE count2);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_HEARTBEAT_H */

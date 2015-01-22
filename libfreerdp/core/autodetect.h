/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Auto-Detect PDUs
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

#ifndef __AUTODETECT_H
#define __AUTODETECT_H

#include "rdp.h"

#include <freerdp/freerdp.h>
#include <freerdp/autodetect.h>
#include <freerdp/log.h>

#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#define TYPE_ID_AUTODETECT_REQUEST	0x00
#define TYPE_ID_AUTODETECT_RESPONSE	0x01

int rdp_recv_autodetect_request_packet(rdpRdp* rdp, wStream* s);
int rdp_recv_autodetect_response_packet(rdpRdp* rdp, wStream* s);

rdpAutoDetect* autodetect_new(void);
void autodetect_free(rdpAutoDetect* autodetect);

void autodetect_register_server_callbacks(rdpAutoDetect* autodetect);
BOOL autodetect_send_connecttime_rtt_measure_request(rdpContext* context, UINT16 sequenceNumber);
BOOL autodetect_send_connecttime_bandwidth_measure_start(rdpContext* context, UINT16 sequenceNumber);
BOOL autodetect_send_bandwidth_measure_payload(rdpContext* context, UINT16 payloadLength, UINT16 sequenceNumber);
BOOL autodetect_send_connecttime_bandwidth_measure_stop(rdpContext* context, UINT16 payloadLength, UINT16 sequenceNumber);

#define AUTODETECT_TAG FREERDP_TAG("core.autodetect")

#endif /* __AUTODETECT_H */

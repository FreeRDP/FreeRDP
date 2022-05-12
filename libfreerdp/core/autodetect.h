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

#ifndef FREERDP_LIB_CORE_AUTODETECT_H
#define FREERDP_LIB_CORE_AUTODETECT_H

#include "rdp.h"

#include <freerdp/freerdp.h>
#include <freerdp/autodetect.h>
#include <freerdp/log.h>
#include <freerdp/api.h>

#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#define TYPE_ID_AUTODETECT_REQUEST 0x00
#define TYPE_ID_AUTODETECT_RESPONSE 0x01

FREERDP_LOCAL int rdp_recv_autodetect_request_packet(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL int rdp_recv_autodetect_response_packet(rdpRdp* rdp, wStream* s);

FREERDP_LOCAL rdpAutoDetect* autodetect_new(rdpContext* context);
FREERDP_LOCAL void autodetect_free(rdpAutoDetect* autodetect);

FREERDP_LOCAL void autodetect_register_server_callbacks(rdpAutoDetect* autodetect);
FREERDP_LOCAL BOOL autodetect_send_connecttime_rtt_measure_request(rdpContext* context,
                                                                   UINT16 sequenceNumber);
FREERDP_LOCAL BOOL autodetect_send_connecttime_bandwidth_measure_start(rdpContext* context,
                                                                       UINT16 sequenceNumber);
FREERDP_LOCAL BOOL autodetect_send_bandwidth_measure_payload(rdpContext* context,
                                                             UINT16 payloadLength,
                                                             UINT16 sequenceNumber);
FREERDP_LOCAL BOOL autodetect_send_connecttime_bandwidth_measure_stop(rdpContext* context,
                                                                      UINT16 payloadLength,
                                                                      UINT16 sequenceNumber);

#define AUTODETECT_TAG FREERDP_TAG("core.autodetect")

#endif /* FREERDP_LIB_CORE_AUTODETECT_H */

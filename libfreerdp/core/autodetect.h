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

#include "state.h"

FREERDP_LOCAL void autodetect_free(rdpAutoDetect* autodetect);

WINPR_ATTR_MALLOC(autodetect_free, 1)
FREERDP_LOCAL rdpAutoDetect* autodetect_new(rdpContext* context);

FREERDP_LOCAL state_run_t autodetect_recv_request_packet(rdpAutoDetect* autodetect,
                                                         RDP_TRANSPORT_TYPE transport, wStream* s);
FREERDP_LOCAL state_run_t autodetect_recv_response_packet(rdpAutoDetect* autodetect,
                                                          RDP_TRANSPORT_TYPE transport, wStream* s);

FREERDP_LOCAL FREERDP_AUTODETECT_STATE autodetect_get_state(rdpAutoDetect* autodetect);

FREERDP_LOCAL void autodetect_register_server_callbacks(rdpAutoDetect* autodetect);
FREERDP_LOCAL void autodetect_on_connect_time_auto_detect_begin(rdpAutoDetect* autodetect);
FREERDP_LOCAL void autodetect_on_connect_time_auto_detect_progress(rdpAutoDetect* autodetect);

#define AUTODETECT_TAG FREERDP_TAG("core.autodetect")

#endif /* FREERDP_LIB_CORE_AUTODETECT_H */

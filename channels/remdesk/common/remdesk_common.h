/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Assistance Virtual Channel - common components
 *
 * Copyright 2024 Armin Novak <armin.novak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <winpr/wtypes.h>
#include <winpr/stream.h>

#include <freerdp/channels/remdesk.h>

FREERDP_LOCAL UINT remdesk_write_channel_header(wStream* s, const REMDESK_CHANNEL_HEADER* header);
FREERDP_LOCAL UINT remdesk_write_ctl_header(wStream* s, const REMDESK_CTL_HEADER* ctlHeader);
FREERDP_LOCAL UINT remdesk_read_channel_header(wStream* s, REMDESK_CHANNEL_HEADER* header);
FREERDP_LOCAL UINT remdesk_prepare_ctl_header(REMDESK_CTL_HEADER* ctlHeader, UINT32 msgType,
                                              size_t msgSize);

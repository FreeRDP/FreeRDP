/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Telemetry Virtual Channel Extension
 *
 * Copyright 2022 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#ifndef FREERDP_CHANNEL_TELEMETRY_H
#define FREERDP_CHANNEL_TELEMETRY_H

#include <freerdp/api.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>

#define TELEMETRY_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::Telemetry"

struct _TELEMETRY_RDP_TELEMETRY_PDU
{
	UINT32 PromptForCredentialsMillis;
	UINT32 PromptForCredentialsDoneMillis;
	UINT32 GraphicsChannelOpenedMillis;
	UINT32 FirstGraphicsReceivedMillis;
};
typedef struct _TELEMETRY_RDP_TELEMETRY_PDU TELEMETRY_RDP_TELEMETRY_PDU;

#endif /* FREERDP_CHANNEL_TELEMETRY_H */

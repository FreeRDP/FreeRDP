/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Virtual Channel
 *
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef FREERDP_CHANNEL_DRDYNVC_H
#define FREERDP_CHANNEL_DRDYNVC_H

#include <freerdp/api.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>

#define DRDYNVC_SVC_CHANNEL_NAME "drdynvc"

typedef enum
{
	CREATE_REQUEST_PDU = 0x01,
	DATA_FIRST_PDU = 0x02,
	DATA_PDU = 0x03,
	CLOSE_REQUEST_PDU = 0x04,
	CAPABILITY_REQUEST_PDU = 0x05
} DynamicChannelPDU;

#endif /* FREERDP_CHANNEL_DRDYNVC_H */

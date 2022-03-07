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

/* defined in MS-RDPEDYC 2.2.5.1 Soft-Sync Request PDU (DYNVC_SOFT_SYNC_REQUEST) */
enum
{
	SOFT_SYNC_TCP_FLUSHED = 0x01,
	SOFT_SYNC_CHANNEL_LIST_PRESENT = 0x02
};

/* define in MS-RDPEDYC 2.2.5.1.1 Soft-Sync Channel List (DYNVC_SOFT_SYNC_CHANNEL_LIST) */
enum
{
	TUNNELTYPE_UDPFECR = 0x00000001,
	TUNNELTYPE_UDPFECL = 0x00000003
};

/* @brief dynamic channel commands */
typedef enum
{
	CREATE_REQUEST_PDU = 0x01,
	DATA_FIRST_PDU = 0x02,
	DATA_PDU = 0x03,
	CLOSE_REQUEST_PDU = 0x04,
	CAPABILITY_REQUEST_PDU = 0x05,
	DATA_FIRST_COMPRESSED_PDU = 0x06,
	DATA_COMPRESSED_PDU = 0x07,
	SOFT_SYNC_REQUEST_PDU = 0x08,
	SOFT_SYNC_RESPONSE_PDU = 0x09
} DynamicChannelPDU;

#endif /* FREERDP_CHANNEL_DRDYNVC_H */

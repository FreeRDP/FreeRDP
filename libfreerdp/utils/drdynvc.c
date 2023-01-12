/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * drdynvc Utils - Helper functions converting something to string
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <freerdp/utils/drdynvc.h>
#include <freerdp/channels/drdynvc.h>

const char* drdynvc_get_packet_type(BYTE cmd)
{
	switch (cmd)
	{
		case CREATE_REQUEST_PDU:
			return "CREATE_REQUEST_PDU";
		case DATA_FIRST_PDU:
			return "DATA_FIRST_PDU";
		case DATA_PDU:
			return "DATA_PDU";
		case CLOSE_REQUEST_PDU:
			return "CLOSE_REQUEST_PDU";
		case CAPABILITY_REQUEST_PDU:
			return "CAPABILITY_REQUEST_PDU";
		case DATA_FIRST_COMPRESSED_PDU:
			return "DATA_FIRST_COMPRESSED_PDU";
		case DATA_COMPRESSED_PDU:
			return "DATA_COMPRESSED_PDU";
		case SOFT_SYNC_REQUEST_PDU:
			return "SOFT_SYNC_REQUEST_PDU";
		case SOFT_SYNC_RESPONSE_PDU:
			return "SOFT_SYNC_RESPONSE_PDU";
		default:
			return "UNKNOWN";
	}
}

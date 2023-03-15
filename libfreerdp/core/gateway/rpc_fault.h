/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC Fault Handling
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_GATEWAY_RPC_FAULT_H
#define FREERDP_LIB_CORE_GATEWAY_RPC_FAULT_H

#include <freerdp/api.h>
#include <winpr/wtypes.h>

FREERDP_LOCAL int rpc_recv_fault_pdu(UINT32 status);
FREERDP_LOCAL const char* rpc_error_to_string(UINT32 error);
FREERDP_LOCAL const char* rpc_error_to_category(UINT32 error);

#endif /* FREERDP_LIB_CORE_GATEWAY_RPC_FAULT_H */

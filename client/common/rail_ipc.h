/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Common RAIL launch IPC
 *
 * Copyright 2026 Tony Dursun <oraturk75@gmail.com>
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

#pragma once

#include <freerdp/client.h>
#include <freerdp/client/rail.h>

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rail_ipc_enable(rdpClientContext* cctx, BOOL enable);
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rail_ipc_start(rdpClientContext* cctx);
FREERDP_LOCAL void rail_ipc_stop(rdpClientContext* cctx);
FREERDP_LOCAL void rail_ipc_free(rdpClientContext* cctx);
FREERDP_LOCAL void rail_ipc_attach(rdpClientContext* cctx, RailClientContext* rail);
FREERDP_LOCAL void rail_ipc_detach(rdpClientContext* cctx);
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rail_ipc_set_ready(rdpClientContext* cctx, BOOL ready);

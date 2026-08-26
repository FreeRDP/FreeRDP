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

#ifndef FREERDP_CLIENT_RAIL_IPC_H
#define FREERDP_CLIENT_RAIL_IPC_H

#include <freerdp/api.h>
#include <freerdp/settings.h>
#include <freerdp/client/rail.h>

#include <winpr/synch.h>
#include <winpr/wlog.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct s_rail_client_ipc RailClientIpcContext;

	/**
	 * Free a common RAIL launch IPC context.
	 *
	 * Must run on the client-loop thread that created the context.
	 */
	FREERDP_API void freerdp_client_rail_ipc_free(RailClientIpcContext* ipc);

	/**
	 * Create the primary-side FIFO for the session described by settings.
	 *
	 * Returns nullptr when the transport is unavailable, setup is unsafe, or another primary
	 * already owns the session key. Must be called before connecting the RDP client.
	 */
	WINPR_ATTR_MALLOC(freerdp_client_rail_ipc_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API RailClientIpcContext* freerdp_client_rail_ipc_new(const rdpSettings* settings,
	                                                              wLog* log);

	/**
	 * Return the FIFO event while a RAIL channel is attached and ready, otherwise nullptr.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API HANDLE freerdp_client_rail_ipc_get_event(const RailClientIpcContext* ipc);

	/**
	 * Process ready FIFO input on the client-loop thread.
	 *
	 * Detached, not-ready, and permanently disabled states are successful no-ops.
	 * Returns FALSE after an operational failure permanently disables FIFO input.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_rail_ipc_check_event(RailClientIpcContext* ipc);

	/** Attach a RAIL channel on the client-loop thread. Returns FALSE for an invalid conflict. */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_rail_ipc_attach(RailClientIpcContext* ipc,
	                                                RailClientContext* rail);

	/** Detach the current RAIL channel on the client-loop thread. Returns FALSE on mismatch. */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_rail_ipc_detach(RailClientIpcContext* ipc,
	                                                RailClientContext* rail);

	/** Set whether the attached RAIL channel can accept Client Execute orders. Returns FALSE when
	 * enabling input without an attached channel. */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_rail_ipc_set_ready(RailClientIpcContext* ipc, BOOL ready);

	/** Return the FIFO path. The pointer remains valid until the context is freed. */
	WINPR_ATTR_NODISCARD
	FREERDP_API const char* freerdp_client_rail_ipc_get_path(const RailClientIpcContext* ipc);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_RAIL_IPC_H */

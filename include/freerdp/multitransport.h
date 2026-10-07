/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multitransport (UDP) tunnels
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

#ifndef FREERDP_MULTITRANSPORT_H
#define FREERDP_MULTITRANSPORT_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief State of a client side multitransport tunnel
	 *  @since version 3.33.0
	 */
	typedef enum
	{
		FREERDP_MULTITRANSPORT_TUNNEL_NONE,    /**< no tunnel, or it failed */
		FREERDP_MULTITRANSPORT_TUNNEL_PENDING, /**< the tunnel is being set up */
		FREERDP_MULTITRANSPORT_TUNNEL_READY    /**< the tunnel carries dynamic channel data */
	} FreeRDP_MultitransportTunnelState;

	/** @brief Dynamic virtual channel hooks of the client side tunnels.
	 *
	 *  Tunnel types are the TUNNELTYPE_ values of [MS-RDPEDYC]. Both callbacks run on the
	 *  thread that runs freerdp_check_event_handles().
	 *
	 *  Data that arrived on a tunnel has been acknowledged to the server already, so it cannot
	 *  be dropped without corrupting the channel stream. A callback that fails returns \b FALSE,
	 *  which ends the connection like a broken main transport.
	 *
	 *  @since version 3.33.0
	 */
	typedef struct
	{
		/** One dynamic virtual channel PDU arrived on a tunnel. */
		WINPR_ATTR_NODISCARD BOOL (*TunnelDataReceived)(void* custom, UINT32 tunnelType,
		                                                const BYTE* data, size_t length);
		/** A tunnel became ready, or went away. */
		WINPR_ATTR_NODISCARD BOOL (*TunnelStateChanged)(void* custom, UINT32 tunnelType,
		                                                FreeRDP_MultitransportTunnelState state);
	} FreeRDP_MultitransportDvcCallbacks;

	/** @brief Registers the receiver of dynamic virtual channel data arriving on tunnels.
	 *
	 *  Not thread safe: call it on the thread that runs freerdp_check_event_handles(), as the
	 *  channel init and disconnect events do. The hooks are read there without locking, so
	 *  receiving tunnel data never waits for a thread that is sending.
	 *
	 *  @param callbacks the hooks, or \b nullptr to unregister
	 *  @return \b FALSE if the context has no multitransport
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_multitransport_set_dvc_callbacks(
	    rdpContext* context, const FreeRDP_MultitransportDvcCallbacks* callbacks, void* custom);

	/** @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API FreeRDP_MultitransportTunnelState
	freerdp_multitransport_get_tunnel_state(rdpContext* context, UINT32 tunnelType);

	/** @brief Sends one dynamic virtual channel PDU over a tunnel. Thread safe.
	 *  @return \b FALSE if the tunnel is not ready, the caller then uses the main connection
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_multitransport_send_dvc(rdpContext* context, UINT32 tunnelType,
	                                                 const BYTE* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_MULTITRANSPORT_H */

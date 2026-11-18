/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server side UDP multitransport
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

#ifndef FREERDP_SERVER_UDP_H
#define FREERDP_SERVER_UDP_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/peer.h>

/** @file
 *  @brief Pacing server output on the UDP tunnel for servers built with -DWITH_RDPEUDP=ON.
 *
 *  Servers do not need this file to utilize the tunnel; each peer shares a process-wide
 *  UDP listener on the TCP address and port and is assigned a worker thread. Because the
 *  reliable tunnel does not drop data, servers producing data faster than the link capacity
 *  should check FreeRDP_UdpSendState and wait for WriteUnblocked.
 *
 *  This header is installed only if WITH_RDPEUDP_SERVER is defined in freerdp/config.h. */

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief UDP connection state for a peer
	 *  @since version 3.34.0
	 *  */
	typedef enum
	{
		FREERDP_UDP_LINK_NONE, /**< No UDP connection exists or it failed; the peer remains on TCP.
		                        */
		FREERDP_UDP_LINK_OFFERED, /**< The tunnel is offered via TCP; the flow is now awaiting a SYN
		packet. */
		FREERDP_UDP_LINK_SYN_RECEIVED, /**< A SYN provided the hash, but identity is unproven as the
		                             hash could be intercepted */
		FREERDP_UDP_LINK_VERIFIED, /**< The security cookie has been received within the tunnel's
		                            TLS session. */
		FREERDP_UDP_LINK_READY     /**< Client has also accepted on TCP; the tunnel can now carry
		                               channel data. */
	} FreeRDP_UdpLinkState;

	/** @brief Link state for pacing encoders. Set size to sizeof(FreeRDP_UdpSendState)
	 *  before calling. New fields replace reserved ones to ensure the size remains constant.
	 *  @since version 3.34.0 */
	typedef struct
	{
		ALIGN64 size_t size;       /**< Set by the caller; see the structure description above. */
		ALIGN64 BOOL writeBlocked; /**< TRUE if queued data exceeds link capacity; see
		                                WriteUnblocked. */
		ALIGN64 UINT32 rttMs;      /**< Smoothed round-trip time; 0 before the first sample. */
		UINT64 reserved[64 - 3];   /* Reserve some space for ABI compatibility */
	} FreeRDP_UdpSendState;

	/** Callback invoked when the peer's UDP connection state changes. */
	typedef void (*psPeerUdpLinkChanged)(void* custom, FreeRDP_UdpLinkState state);

	/** Called when the tunnel can accept more data after writeBlocked was set.
	 *  Note: freerdp_peer's IsWriteBlocked and DrainOutputBuffer only apply to TCP. */
	typedef void (*psPeerUdpWriteUnblocked)(void* custom);

	/** @brief Per-peer UDP callbacks, executed on the thread running the freerdp_peer check.
	 *  Set size to sizeof(FreeRDP_UdpPeerCallbacks). New fields replace reserved ones to
	 *  ensure the structure size remains constant.
	 *  @since version 3.34.0 */
	typedef struct
	{
		ALIGN64 size_t size; /**< Set by the caller; see the struct description above. */
		ALIGN64 psPeerUdpLinkChanged LinkChanged;
		ALIGN64 psPeerUdpWriteUnblocked WriteUnblocked;
		UINT64 reserved[64 - 3]; /* Reserve some space for ABI compatibility */
	} FreeRDP_UdpPeerCallbacks;

	/** Sets the UDP callbacks for a peer. This should be called after the peer context is
	 *  created and before the tunnel is offered.
	 *  @param callbacks The callback hooks, or \b nullptr for none.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_peer_set_udp_callbacks(freerdp_peer* peer,
	                                                const FreeRDP_UdpPeerCallbacks* callbacks,
	                                                void* custom);

	/** Retrieves the link state for pacing. Because the reliable tunnel cannot drop data,
	 *  exceeding its queue limit will cause the tunnel to fail. The session terminates only
	 *  if channels have already migrated to UDP; otherwise, it continues on TCP.
	 *  @return \b FALSE if no tunnel exists
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_peer_get_udp_send_state(freerdp_peer* peer,
	                                                 FreeRDP_UdpSendState* state);

#ifdef __cplusplus
}
#endif

#endif /* End of FREERDP_SERVER_UDP_H */

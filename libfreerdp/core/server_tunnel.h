/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic virtual channels of a server on the reliable UDP tunnel
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

#ifndef FREERDP_LIB_CORE_SERVER_TUNNEL_H
#define FREERDP_LIB_CORE_SERVER_TUNNEL_H

#include <freerdp/config.h>
#include <freerdp/api.h>

#include "server.h"

/** @brief All the UDP work of server.c, as calls that do nothing in a build without UDP.
 * A dynamic channel starts on TCP. Soft-Sync moves it onto the tunnel ([MS-RDPEDYC] 3.1.5.3). After
 * that, its PDUs use the same queue with a different message id. */

/** Reads one dynamic channel PDU, of an open channel. */
typedef BOOL (*wtsTunnelReadPduFn)(rdpPeerChannel* channel, wStream* s, UINT8 Cmd, UINT8 Sp,
                                   UINT8 cbChId);

#if defined(WITH_RDPEUDP)

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_new(WTSVirtualChannelManager* vcm, rdpContext* context,
                                  wtsTunnelReadPduFn readPdu);

FREERDP_LOCAL void wts_tunnel_free(WTSVirtualChannelManager* vcm);

/** A channel opened. Soft-Sync can move it. */
FREERDP_LOCAL void wts_tunnel_channel_opened(WTSVirtualChannelManager* vcm);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_is_soft_sync_response(UINT8 Cmd);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_read_soft_sync_response(WTSVirtualChannelManager* vcm, wStream* s);

/** Before the server sends the queued PDUs. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_before_drain(WTSVirtualChannelManager* vcm);

/** Sends one queued PDU if it is for the tunnel.
 *  @param status FALSE on an error
 *  @return TRUE if it was for the tunnel */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_send_message(WTSVirtualChannelManager* vcm, UINT32 id,
                                           const BYTE* buffer, UINT32 length, BOOL* status);

/** After the server sent the queued PDUs.
 *  @return status, or FALSE on an error */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_after_drain(WTSVirtualChannelManager* vcm, BOOL status);

/** After Soft-Sync, a new channel is on the tunnel. Its Create Request goes there.
 *  @param onTunnel TRUE if it went there
 *  @return FALSE on an error */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_open_channel(WTSVirtualChannelManager* vcm, rdpPeerChannel* channel,
                                           wStream* s, BOOL* onTunnel);

/** Queues a copy of one PDU of channel if the channel is on the tunnel.
 *  @param status FALSE on an error
 *  @return TRUE if it is */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_write(rdpPeerChannel* channel, const BYTE* data, size_t length,
                                    BOOL* status);

/** Queues one PDU of channel if the channel is on the tunnel. The queue takes buffer.
 *  @param status FALSE on an error
 *  @return TRUE if it is */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_post_item(rdpPeerChannel* channel, BYTE* buffer, UINT32 length,
                                        BOOL* status);

#else

WINPR_ATTR_NODISCARD
static inline BOOL wts_tunnel_new(WINPR_ATTR_UNUSED WTSVirtualChannelManager* vcm,
                                  WINPR_ATTR_UNUSED rdpContext* context,
                                  WINPR_ATTR_UNUSED wtsTunnelReadPduFn readPdu)
{
	return TRUE;
}

static inline void wts_tunnel_free(WINPR_ATTR_UNUSED WTSVirtualChannelManager* vcm)
{
}

static inline void wts_tunnel_channel_opened(WINPR_ATTR_UNUSED WTSVirtualChannelManager* vcm)
{
}

WINPR_ATTR_NODISCARD
static inline BOOL wts_tunnel_is_soft_sync_response(WINPR_ATTR_UNUSED UINT8 Cmd)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline BOOL
wts_tunnel_read_soft_sync_response(WINPR_ATTR_UNUSED WTSVirtualChannelManager* vcm,
                                   WINPR_ATTR_UNUSED wStream* s)
{
	return TRUE;
}

WINPR_ATTR_NODISCARD
static inline BOOL wts_tunnel_before_drain(WINPR_ATTR_UNUSED WTSVirtualChannelManager* vcm)
{
	return TRUE;
}

WINPR_ATTR_NODISCARD
static inline BOOL wts_tunnel_send_message(WINPR_ATTR_UNUSED WTSVirtualChannelManager* vcm,
                                           WINPR_ATTR_UNUSED UINT32 id,
                                           WINPR_ATTR_UNUSED const BYTE* buffer,
                                           WINPR_ATTR_UNUSED UINT32 length,
                                           WINPR_ATTR_UNUSED BOOL* status)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline BOOL wts_tunnel_after_drain(WINPR_ATTR_UNUSED WTSVirtualChannelManager* vcm,
                                          BOOL status)
{
	return status;
}

WINPR_ATTR_NODISCARD
static inline BOOL wts_tunnel_open_channel(WINPR_ATTR_UNUSED WTSVirtualChannelManager* vcm,
                                           WINPR_ATTR_UNUSED rdpPeerChannel* channel,
                                           WINPR_ATTR_UNUSED wStream* s, BOOL* onTunnel)
{
	*onTunnel = FALSE;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static inline BOOL wts_tunnel_write(WINPR_ATTR_UNUSED rdpPeerChannel* channel,
                                    WINPR_ATTR_UNUSED const BYTE* data,
                                    WINPR_ATTR_UNUSED size_t length, WINPR_ATTR_UNUSED BOOL* status)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline BOOL wts_tunnel_post_item(WINPR_ATTR_UNUSED rdpPeerChannel* channel,
                                        WINPR_ATTR_UNUSED BYTE* buffer,
                                        WINPR_ATTR_UNUSED UINT32 length,
                                        WINPR_ATTR_UNUSED BOOL* status)
{
	return FALSE;
}

#endif

#endif /* FREERDP_LIB_CORE_SERVER_TUNNEL_H */

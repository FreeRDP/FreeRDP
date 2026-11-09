/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server dynamic virtual channels over the reliable UDP tunnel
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

/** @brief UDP-specific server logic; functions are no-ops in builds without UDP support.
 * Dynamic channels start on TCP until Soft-Sync moves them to the tunnel ([MS-RDPEDYC] 3.1.5.3);
 * from then on, PDUs are queued under a different message ID. */

/** Read one dynamic channel PDU for an open channel. */
typedef BOOL (*wtsTunnelReadPduFn)(rdpPeerChannel* channel, wStream* s, UINT8 Cmd, UINT8 Sp,
                                   UINT8 cbChId);

#if defined(WITH_RDPEUDP)

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_new(WTSVirtualChannelManager* vcm, rdpContext* context,
                                  wtsTunnelReadPduFn readPdu);

FREERDP_LOCAL void wts_tunnel_free(WTSVirtualChannelManager* vcm);

/** Notify that a channel has opened or is no longer pending, allowing Soft-Sync to proceed. */
FREERDP_LOCAL void wts_tunnel_channel_opened(WTSVirtualChannelManager* vcm);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_is_soft_sync_response(UINT8 Cmd);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_read_soft_sync_response(WTSVirtualChannelManager* vcm, wStream* s);

/** Starts Soft-Sync if a channel answered or closed since the last call, or if Soft-Sync has
 * waited longer than SOFT_SYNC_MAX_WAIT_MS for a Create Response. Call it before draining the
 * PDU queue, so the Soft-Sync Request goes out after the PDUs the channels queued on TCP. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_start_soft_sync_if_due(WTSVirtualChannelManager* vcm);

/** Send one queued PDU if it is destined for the tunnel.
 * @param status set to FALSE on error
 * @return TRUE if the PDU was sent via the tunnel */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_send_message(WTSVirtualChannelManager* vcm, UINT32 id,
                                           const BYTE* buffer, UINT32 length, BOOL* status);

/** Called after draining the PDU queue: flushes the PDUs this drain queued for the tunnel.
 * @return status, or FALSE on error */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_after_drain(WTSVirtualChannelManager* vcm, BOOL status);

/** After Soft-Sync, new channels use the tunnel and their Create Request is sent there.
 * @param onTunnel TRUE if the request was sent via the tunnel
 * @return FALSE on error */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_open_channel(WTSVirtualChannelManager* vcm, rdpPeerChannel* channel,
                                           wStream* s, BOOL* onTunnel);

/** Queue a copy of a channel's Close Request if the channel is on the tunnel. Call with the
 * channel's writeLock held: the channel is marked as closing, so a Soft-Sync from then on leaves
 * it out.
 * @param status set to FALSE on error
 * @return TRUE if the channel is on the tunnel */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_close_request(rdpPeerChannel* channel, const BYTE* data,
                                            size_t length, BOOL* status);

/** Queue a channel PDU if the channel is on the tunnel; the queue takes ownership of the buffer.
 * @param status set to FALSE on error
 * @return TRUE if the channel is on the tunnel */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL wts_tunnel_queue_send_item(rdpPeerChannel* channel, BYTE* buffer, UINT32 length,
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
static inline BOOL
wts_tunnel_start_soft_sync_if_due(WINPR_ATTR_UNUSED WTSVirtualChannelManager* vcm)
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
static inline BOOL wts_tunnel_close_request(WINPR_ATTR_UNUSED rdpPeerChannel* channel,
                                            WINPR_ATTR_UNUSED const BYTE* data,
                                            WINPR_ATTR_UNUSED size_t length,
                                            WINPR_ATTR_UNUSED BOOL* status)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline BOOL wts_tunnel_queue_send_item(WINPR_ATTR_UNUSED rdpPeerChannel* channel,
                                              WINPR_ATTR_UNUSED BYTE* buffer,
                                              WINPR_ATTR_UNUSED UINT32 length,
                                              WINPR_ATTR_UNUSED BOOL* status)
{
	return FALSE;
}

#endif

#endif /* End of FREERDP_LIB_CORE_SERVER_TUNNEL_H */

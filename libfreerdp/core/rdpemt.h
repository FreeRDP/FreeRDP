/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multitransport tunnel ([MS-RDPEMT]) over reliable RDP-UDP
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

#ifndef FREERDP_LIB_CORE_RDPEMT_H
#define FREERDP_LIB_CORE_RDPEMT_H

#include <winpr/wtypes.h>
#include <winpr/winsock.h>
#include <winpr/wlog.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/config.h>
#include <freerdp/api.h>
#if defined(WITH_RDPEUDP)
#include <freerdp/server/udp.h>
#endif

/** @brief A client side multitransport tunnel.
 *
 * Runs its own thread: it brings up a reliable RDP-UDP connection, a TLS session inside it, and
 * the [MS-RDPEMT] tunnel inside that, and then carries tunnel data both ways.
 */
typedef struct rdp_emt rdpEmt;

typedef enum
{
	RDPEMT_EVENT_READY,      /**< the server accepted the Tunnel Create Request */
	RDPEMT_EVENT_FAILED,     /**< the tunnel could not be set up, or broke down */
	RDPEMT_EVENT_DATA,       /**< HigherLayerData of one Tunnel Data PDU */
	RDPEMT_EVENT_AUTODETECT, /**< one RDP_TUNNEL_SUBHEADER, from SubHeaderLength on: of type
	                              TYPE_ID_AUTODETECT_REQUEST on a client, and of type
	                              TYPE_ID_AUTODETECT_RESPONSE on a server */
	RDPEMT_EVENT_CLAIMED,    /**< server: a SYN with the hash of the cookie arrived */
	RDPEMT_EVENT_WRITABLE    /**< server: the tunnel has room again after it had none */
} RDPEMT_EVENT;

/** The tunnel thread calls this. The data of RDPEMT_EVENT_DATA and RDPEMT_EVENT_AUTODETECT is valid
 * only during the call. */
typedef void (*rdpEmtEventFn)(void* custom, rdpEmt* emt, RDPEMT_EVENT event, const BYTE* data,
                              size_t length);

/** Called on the tunnel thread instead of RDPEMT_EVENT_DATA, see rdpemt_set_receiver(). It owns
 * \b s, sealed and at position 0. \b FALSE fails the tunnel. */
typedef BOOL (*rdpEmtReceiverFn)(void* custom, wStream* s);

#if defined(WITH_RDPEUDP)

/** UDP multitransport is compiled in (-DWITH_RDPEUDP=ON, which requires OpenSSL). */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpemt_is_supported(void);

FREERDP_LOCAL void rdpemt_free(rdpEmt* emt);

/**
 * @param requestId the requestId of the Initiate Multitransport Request PDU
 * @param cookie the 16 byte securityCookie of that request
 * @param hostname the server name, used for SNI
 * @param publicKey the public key of the server certificate on the main connection. The TLS
 *        session inside the tunnel is only accepted when it presents the same key.
 */
WINPR_ATTR_MALLOC(rdpemt_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpEmt* rdpemt_new(wLog* log, UINT32 requestId, const BYTE* cookie,
                                 const char* hostname, const BYTE* publicKey,
                                 size_t publicKeyLength, rdpEmtEventFn callback, void* custom);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpemt_start(rdpEmt* emt, const struct sockaddr* addr, size_t addrlen);

/** @brief The server end of a reliable tunnel, for one Initiate Multitransport Request.
 * It waits on its flow, see rdpemt_get_flow(), for the client whose SYN contains the hash of
 * cookie. It runs TLS with cert and key, those of the main connection. It accepts a Tunnel Create
 * Request that names requestId and cookie. RDPEMT_EVENT_READY comes after the Tunnel Create
 * Response.
 * @param manual no thread of its own, freerdp_udp_flow_check() runs the tunnel */
WINPR_ATTR_MALLOC(rdpemt_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpEmt* rdpemt_new_server(wLog* log, UINT32 requestId, const BYTE* cookie,
                                        struct x509_st* cert, struct evp_pkey_st* key, BOOL manual,
                                        rdpEmtEventFn callback, void* custom);

/** Server: where the tunnel gets its datagrams from. It belongs to the tunnel. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpUdpFlow* rdpemt_get_flow(const rdpEmt* emt);

/** Server: what the tunnel knows about the link. Thread safe. */
FREERDP_LOCAL void rdpemt_get_send_state(rdpEmt* emt, FreeRDP_UdpSendState* state);

/** Server: tells if more data waits than the link accepts. Thread safe. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpemt_is_write_blocked(rdpEmt* emt);

/** Server: the maximum of data that waits for the link before the tunnel fails. Thread safe.
 *  @param limit in bytes, 0 for the default */
FREERDP_LOCAL void rdpemt_set_send_limit(rdpEmt* emt, size_t limit);

/** Sends one Tunnel Data PDU. Thread safe. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpemt_send_data(rdpEmt* emt, const BYTE* data, size_t length);

/** Sends one RDP_TUNNEL_SUBHEADER, from SubHeaderLength on, in a Tunnel Data PDU without
 * HigherLayerData. Thread safe. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpemt_send_subheader(rdpEmt* emt, const BYTE* subHeader, size_t length);

/** Sends the data that rdpemt_send_data collected on the server side. Thread safe. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpemt_flush(rdpEmt* emt);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT32 rdpemt_get_request_id(const rdpEmt* emt);

/** Hands tunnel data to \b receiver from now on, instead of raising RDPEMT_EVENT_DATA. Thread
 * safe.
 * @param pending the queue the events go to. Without a receiver set already, it is only set
 *        while pending is empty, so data does not overtake data still waiting there.
 * @return \b FALSE if data may still wait in pending */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpemt_set_receiver(rdpEmt* emt, rdpEmtReceiverFn receiver, void* custom,
                                       wQueue* pending);

/** Raises RDPEMT_EVENT_DATA again. Once this returns, the tunnel thread is out of the receiver
 * it had. Thread safe. */
FREERDP_LOCAL void rdpemt_clear_receiver(rdpEmt* emt);

/** The tunnel data handed to the receiver since the last call, after the tunnel headers. Thread
 * safe. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT32 rdpemt_take_direct_bytes(rdpEmt* emt);

#else

/* Built without UDP multitransport: requests are declined and no tunnel ever exists. */
WINPR_ATTR_NODISCARD
static inline BOOL rdpemt_is_supported(void)
{
	return FALSE;
}

static inline void rdpemt_free(WINPR_ATTR_UNUSED rdpEmt* emt)
{
}

WINPR_ATTR_NODISCARD
static inline rdpEmt*
rdpemt_new(WINPR_ATTR_UNUSED wLog* log, WINPR_ATTR_UNUSED UINT32 requestId,
           WINPR_ATTR_UNUSED const BYTE* cookie, WINPR_ATTR_UNUSED const char* hostname,
           WINPR_ATTR_UNUSED const BYTE* publicKey, WINPR_ATTR_UNUSED size_t publicKeyLength,
           WINPR_ATTR_UNUSED rdpEmtEventFn callback, WINPR_ATTR_UNUSED void* custom)
{
	return nullptr;
}

WINPR_ATTR_NODISCARD
static inline BOOL rdpemt_start(WINPR_ATTR_UNUSED rdpEmt* emt,
                                WINPR_ATTR_UNUSED const struct sockaddr* addr,
                                WINPR_ATTR_UNUSED size_t addrlen)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline BOOL rdpemt_send_data(WINPR_ATTR_UNUSED rdpEmt* emt,
                                    WINPR_ATTR_UNUSED const BYTE* data,
                                    WINPR_ATTR_UNUSED size_t length)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline BOOL rdpemt_send_subheader(WINPR_ATTR_UNUSED rdpEmt* emt,
                                         WINPR_ATTR_UNUSED const BYTE* subHeader,
                                         WINPR_ATTR_UNUSED size_t length)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline UINT32 rdpemt_get_request_id(WINPR_ATTR_UNUSED const rdpEmt* emt)
{
	return 0;
}

WINPR_ATTR_NODISCARD
static inline BOOL rdpemt_set_receiver(WINPR_ATTR_UNUSED rdpEmt* emt,
                                       WINPR_ATTR_UNUSED rdpEmtReceiverFn receiver,
                                       WINPR_ATTR_UNUSED void* custom,
                                       WINPR_ATTR_UNUSED wQueue* pending)
{
	return FALSE;
}

static inline void rdpemt_clear_receiver(WINPR_ATTR_UNUSED rdpEmt* emt)
{
}

#endif

#endif /* FREERDP_LIB_CORE_RDPEMT_H */

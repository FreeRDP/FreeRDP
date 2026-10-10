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

#include <freerdp/config.h>
#include <freerdp/api.h>

/** @brief A client side multitransport tunnel.
 *
 * Runs its own thread: it brings up a reliable RDP-UDP connection, a TLS session inside it, and
 * the [MS-RDPEMT] tunnel inside that, and then carries tunnel data both ways.
 */
typedef struct rdp_emt rdpEmt;

typedef enum
{
	RDPEMT_EVENT_READY,  /**< the server accepted the Tunnel Create Request */
	RDPEMT_EVENT_FAILED, /**< the tunnel could not be set up, or broke down */
	RDPEMT_EVENT_DATA    /**< HigherLayerData of one Tunnel Data PDU */
} RDPEMT_EVENT;

/** Called on the tunnel thread. For RDPEMT_EVENT_DATA the data is only valid during the call. */
typedef void (*rdpEmtEventFn)(void* custom, rdpEmt* emt, RDPEMT_EVENT event, const BYTE* data,
                              size_t length);

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

/** Sends one Tunnel Data PDU. Thread safe. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpemt_send_data(rdpEmt* emt, const BYTE* data, size_t length);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT32 rdpemt_get_request_id(const rdpEmt* emt);

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

WINPR_ATTR_MALLOC(rdpemt_free, 1)
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
static inline UINT32 rdpemt_get_request_id(WINPR_ATTR_UNUSED const rdpEmt* emt)
{
	return 0;
}

#endif

#endif /* FREERDP_LIB_CORE_RDPEMT_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Credential Guard via the native TSSSP package (tspkg.dll) over FreeRDP's TLS
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

#ifndef FREERDP_LIB_CORE_TSSSP_H
#define FREERDP_LIB_CORE_TSSSP_H

typedef struct rdp_tsssp rdpTsssp;

#include <winpr/wtypes.h>
#include <winpr/stream.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

/* The TSSSP package only exists in the native Windows SSPI. */
#if defined(_WIN32) && !defined(_UWP) && defined(WITH_NATIVE_SSPI)
#define TSSSP_SUPPORTED
#endif

#include "transport.h"

typedef enum
{
	TSSSP_STATE_INITIAL,
	TSSSP_STATE_TOKEN,
	TSSSP_STATE_EARLY_USER_AUTH,
	TSSSP_STATE_FINAL
} TSSSP_STATE;

FREERDP_LOCAL void tsssp_free(rdpTsssp* tsssp);

/** @brief Process one PDU received in CONNECTION_STATE_NLA: a TSRequest token
 * from the server, or the Early User Authorization Result after the exchange.
 *
 * @return 1 on success, -1 on failure
 */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL int tsssp_recv(rdpTsssp* tsssp, wStream* s);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL TSSSP_STATE tsssp_get_state(const rdpTsssp* tsssp);

/** @brief Get the TSSSP security context handle value (dwUpper) that [MS-RDPEAR]
 * servicing passes to the LSA.
 *
 * @return \b TRUE if the authentication completed, \b FALSE otherwise (always on
 *         platforms other than Windows)
 */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL tsssp_get_package_context(const rdpTsssp* tsssp, UINT64* pContext);

#if defined(TSSSP_SUPPORTED)
WINPR_ATTR_MALLOC(tsssp_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpTsssp* tsssp_new(rdpContext* context, rdpTransport* transport);

/** @brief Acquire the TSSSP credentials and send the first token.
 *
 * The transport must be in NLA mode: the tokens are TSRequest PDUs and the
 * remaining exchange is driven by tsssp_recv() from the connection state machine.
 *
 * @return 1 on success, -1 on failure
 */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL int tsssp_client_begin(rdpTsssp* tsssp);
#endif

#endif /* FREERDP_LIB_CORE_TSSSP_H */

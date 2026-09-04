/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Level Authentication (NLA)
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
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

#ifndef FREERDP_LIB_CORE_AAD_H
#define FREERDP_LIB_CORE_AAD_H

typedef struct rdp_aad rdpAad;

typedef enum
{
	AAD_STATE_INITIAL,
	AAD_STATE_AUTH,
	AAD_STATE_FINAL
} AAD_STATE;

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL aad_is_supported(void);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL int aad_client_begin(rdpAad* aad);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL int aad_recv(rdpAad* aad, wStream* s);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL AAD_STATE aad_get_state(rdpAad* aad);

FREERDP_LOCAL void aad_free(rdpAad* aad);

WINPR_ATTR_MALLOC(aad_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpAad* aad_new(rdpContext* context);

/** Seeds or clears the cached OpenID configuration of a context.
 *
 * The document is otherwise fetched over HTTPS the first time a freerdp_utils_aad_get_wellknown_*
 * query is made. This is a test seam: it lets the OAuth helpers of client/common be exercised
 * without network access. Core-private, so it is only linkable from tests built with
 * BUILD_TESTING_INTERNAL, which exports all symbols.
 *
 * @param context The rdpContext to install the document in
 * @param json The OpenID configuration as JSON text, or \b nullptr to drop a cached document so
 *             the next query fetches it again
 * @return \b TRUE if the document was installed or dropped, \b FALSE if it could not be parsed.
 *         The previously cached document is kept in that case.
 */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_utils_aad_set_wellknown(rdpContext* context, const char* json);

#endif /* FREERDP_LIB_CORE_AAD_H */

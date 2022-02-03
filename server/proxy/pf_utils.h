/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef FREERDP_SERVER_PROXY_PFUTILS_H
#define FREERDP_SERVER_PROXY_PFUTILS_H

#include <freerdp/server/proxy/proxy_config.h>
#include <freerdp/server/proxy/proxy_context.h>

/**
 * @brief pf_utils_channel_is_passthrough Checks of a channel identified by 'name'
 *          should be handled as passthrough.
 *
 * @param config The proxy configuration to check against. Must NOT be NULL.
 * @param name The name of the channel. Must NOT be NULL.
 * @return -1 if the channel is not handled, 0 if the channel should be ignored,
 *         1 if the channel should be passed, 2 the channel will be intercepted
 *         e.g. proxy client and server are termination points and data passed
 *         between.
 */
pf_utils_channel_mode pf_utils_get_channel_mode(const proxyConfig* config, const char* name);
const char* pf_utils_channel_mode_string(pf_utils_channel_mode mode);

BOOL pf_utils_is_passthrough(const proxyConfig* config);

#endif /* FREERDP_SERVER_PROXY_PFUTILS_H */

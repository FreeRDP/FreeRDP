/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Cong Zhang <congzhangzh@gmail.com>
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

#ifndef FREERDP_SERVER_SHADOW_AUTODETECT_H
#define FREERDP_SERVER_SHADOW_AUTODETECT_H

#include <freerdp/server/shadow.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/* [MS-RDPBCGR] 1.3.1.1 / 2.2.14: network characteristics for the client to show.
	 * At connect time one RTT measurement and one short bandwidth burst, then a Network
	 * Characteristics Result; while active an RTT measurement every few seconds, each followed
	 * by a Result. Nothing here changes what or how the encoder sends. */
	WINPR_ATTR_NODISCARD BOOL shadow_client_autodetect_init(rdpShadowClient* client);
	void shadow_client_autodetect_uninit(rdpShadowClient* client);

	/** @return the milliseconds until the next measurement is due, INFINITE if none */
	WINPR_ATTR_NODISCARD DWORD shadow_client_autodetect_timeout(rdpShadowClient* client);

	/** sends a continuous RTT Measure Request when one is due; client thread only */
	WINPR_ATTR_NODISCARD BOOL shadow_client_autodetect_poll(rdpShadowClient* client);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_AUTODETECT_H */

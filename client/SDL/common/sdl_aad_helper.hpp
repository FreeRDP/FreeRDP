/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Out-of-process AAD auth helper integration
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C"
{
#endif

	[[nodiscard]] BOOL sdl_aad_helper_get_access_token(freerdp* instance, AccessTokenType tokenType,
	                                                   char** token, size_t count, ...);

	/** @brief stop the out-of-process AAD auth helper for this connection, if one was
	 *  started. Call this on disconnect (e.g. from PostDisconnect) so the helper doesn't outlive
	 *  the connection it was serving; a following reconnect will lazily start a fresh one. */
	void sdl_aad_helper_stop(void);

#ifdef __cplusplus
}
#endif

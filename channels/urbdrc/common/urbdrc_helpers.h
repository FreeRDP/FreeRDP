/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server USB redirection channel - helper functions
 *
 * Copyright 2019 Armin Novak <armin.novak@thincast.com>
 * Copyright 2019 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_URBDRC_HELPERS_H
#define FREERDP_CHANNEL_URBDRC_HELPERS_H

#include <winpr/wtypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <winpr/wlog.h>
#include <winpr/stream.h>

	const char* urb_function_string(UINT16 urb);
	const char* mask_to_string(UINT32 mask);
	const char* interface_to_string(UINT32 id);
	const char* call_to_string(BOOL client, UINT32 interfaceNr, UINT32 functionId);

	void urbdrc_dump_message(wLog* log, BOOL client, BOOL write, wStream* s);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_URBDRC_HELPERS_H */

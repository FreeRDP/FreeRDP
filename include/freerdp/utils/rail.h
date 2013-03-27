/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Applications Integrated Locally (RAIL) Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_UTILS_RAIL_H
#define FREERDP_UTILS_RAIL_H

#include <freerdp/api.h>
#include <freerdp/rail.h>

#include <winpr/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API void rail_unicode_string_alloc(RAIL_UNICODE_STRING* unicode_string, UINT16 cbString);
FREERDP_API void rail_unicode_string_free(RAIL_UNICODE_STRING* unicode_string);
FREERDP_API BOOL rail_read_unicode_string(wStream* s, RAIL_UNICODE_STRING* unicode_string);
FREERDP_API void rail_write_unicode_string(wStream* s, RAIL_UNICODE_STRING* unicode_string);
FREERDP_API void rail_write_unicode_string_value(wStream* s, RAIL_UNICODE_STRING* unicode_string);
FREERDP_API void* rail_clone_order(UINT32 event_type, void* order);
FREERDP_API void  rail_free_cloned_order(UINT32 event_type, void* order);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_RAIL_H */

/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#ifndef __RAIL_UTILS_H
#define __RAIL_UTILS_H

#include <freerdp/api.h>
#include <freerdp/rail.h>
#include <freerdp/utils/rect.h>
#include <freerdp/utils/stream.h>

#define RAIL_ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

FREERDP_API void rail_unicode_string_alloc(UNICODE_STRING* unicode_string, uint16 cbString);
FREERDP_API void rail_unicode_string_free(UNICODE_STRING* unicode_string);
FREERDP_API void rail_read_unicode_string(STREAM* s, UNICODE_STRING* unicode_string);
FREERDP_API void rail_write_unicode_string(STREAM* s, UNICODE_STRING* unicode_string);
FREERDP_API void rail_write_unicode_string_value(STREAM* s, UNICODE_STRING* unicode_string);
FREERDP_API void* rail_clone_order(uint32 event_type, void* order);
FREERDP_API void  rail_free_cloned_order(uint32 event_type, void* order);

#endif /* __RAIL_UTILS_H */

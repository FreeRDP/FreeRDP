/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * String Utils
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

#ifndef FREERDP_UTILS_STRING_H
#define FREERDP_UTILS_STRING_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

struct rdp_string
{
	char* ascii;
	char* unicode;
	UINT32 length;
};
typedef struct rdp_string rdpString;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API BOOL freerdp_string_read_length32(STREAM* s, rdpString* string);
FREERDP_API void freerdp_string_free(rdpString* string);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_STRING_H */

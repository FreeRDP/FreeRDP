/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#ifndef __STRING_UTILS_H
#define __STRING_UTILS_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/unicode.h>

struct rdp_string
{
	char* ascii;
	char* unicode;
	uint32 length;
};
typedef struct rdp_string rdpString;

FREERDP_API void freerdp_string_read_length32(STREAM* s, rdpString* string, UNICONV* uniconv);
FREERDP_API void freerdp_string_free(rdpString* string);

#endif /* __STRING_UTILS_H */

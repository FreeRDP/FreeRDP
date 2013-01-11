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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/utils/string.h>

BOOL freerdp_string_read_length32(STREAM* s, rdpString* string)
{
	if(stream_get_left(s) < 4)
		return FALSE;
	stream_read_UINT32(s, string->length);
	if(stream_get_left(s) < string->length)
		return FALSE;
	string->unicode = (char*) malloc(string->length);
	stream_read(s, string->unicode, string->length);

	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) string->unicode, string->length / 2, &string->ascii, 0, NULL, NULL);
	return TRUE;
}

void freerdp_string_free(rdpString* string)
{
	if (string->unicode != NULL)
		free(string->unicode);

	if (string->ascii != NULL)
		free(string->ascii);
}

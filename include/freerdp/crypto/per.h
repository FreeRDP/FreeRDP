/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN.1 Packed Encoding Rules (BER)
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CRYPTO_PER_H
#define FREERDP_CRYPTO_PER_H

#include <freerdp/api.h>

#include <winpr/stream.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL per_read_length(wStream* s, UINT16* length);
	FREERDP_API void per_write_length(wStream* s, int length);
	FREERDP_API BOOL per_read_choice(wStream* s, BYTE* choice);
	FREERDP_API void per_write_choice(wStream* s, BYTE choice);
	FREERDP_API BOOL per_read_selection(wStream* s, BYTE* selection);
	FREERDP_API void per_write_selection(wStream* s, BYTE selection);
	FREERDP_API BOOL per_read_number_of_sets(wStream* s, BYTE* number);
	FREERDP_API void per_write_number_of_sets(wStream* s, BYTE number);
	FREERDP_API BOOL per_read_padding(wStream* s, int length);
	FREERDP_API void per_write_padding(wStream* s, int length);
	FREERDP_API BOOL per_read_integer(wStream* s, UINT32* integer);
	FREERDP_API BOOL per_read_integer16(wStream* s, UINT16* integer, UINT16 min);
	FREERDP_API void per_write_integer(wStream* s, UINT32 integer);
	FREERDP_API void per_write_integer16(wStream* s, UINT16 integer, UINT16 min);
	FREERDP_API BOOL per_read_enumerated(wStream* s, BYTE* enumerated, BYTE count);
	FREERDP_API void per_write_enumerated(wStream* s, BYTE enumerated, BYTE count);
	FREERDP_API void per_write_object_identifier(wStream* s, BYTE oid[6]);
	FREERDP_API BOOL per_read_object_identifier(wStream* s, BYTE oid[6]);
	FREERDP_API BOOL per_read_octet_string(wStream* s, BYTE* oct_str, int length, int min);
	FREERDP_API void per_write_octet_string(wStream* s, BYTE* oct_str, int length, int min);
	FREERDP_API BOOL per_read_numeric_string(wStream* s, int min);
	FREERDP_API void per_write_numeric_string(wStream* s, BYTE* num_str, int length, int min);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_PER_H */

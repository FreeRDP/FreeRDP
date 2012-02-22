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
#include <freerdp/utils/stream.h>

FREERDP_API boolean per_read_length(STREAM* s, uint16* length);
FREERDP_API void per_write_length(STREAM* s, int length);
FREERDP_API boolean per_read_choice(STREAM* s, uint8* choice);
FREERDP_API void per_write_choice(STREAM* s, uint8 choice);
FREERDP_API boolean per_read_selection(STREAM* s, uint8* selection);
FREERDP_API void per_write_selection(STREAM* s, uint8 selection);
FREERDP_API boolean per_read_number_of_sets(STREAM* s, uint8* number);
FREERDP_API void per_write_number_of_sets(STREAM* s, uint8 number);
FREERDP_API boolean per_read_padding(STREAM* s, int length);
FREERDP_API void per_write_padding(STREAM* s, int length);
FREERDP_API boolean per_read_integer(STREAM* s, uint32* integer);
FREERDP_API boolean per_read_integer16(STREAM* s, uint16* integer, uint16 min);
FREERDP_API void per_write_integer(STREAM* s, uint32 integer);
FREERDP_API void per_write_integer16(STREAM* s, uint16 integer, uint16 min);
FREERDP_API boolean per_read_enumerated(STREAM* s, uint8* enumerated, uint8 count);
FREERDP_API void per_write_enumerated(STREAM* s, uint8 enumerated, uint8 count);
FREERDP_API void per_write_object_identifier(STREAM* s, uint8 oid[6]);
FREERDP_API boolean per_read_object_identifier(STREAM* s, uint8 oid[6]);
FREERDP_API boolean per_read_octet_string(STREAM* s, uint8* oct_str, int length, int min);
FREERDP_API void per_write_octet_string(STREAM* s, uint8* oct_str, int length, int min);
FREERDP_API boolean per_read_numeric_string(STREAM* s, int min);
FREERDP_API void per_write_numeric_string(STREAM* s, uint8* num_str, int length, int min);

#endif /* FREERDP_CRYPTO_PER_H */

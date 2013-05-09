/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Input Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_RDPEI_CLIENT_COMMON_H
#define FREERDP_CHANNEL_RDPEI_CLIENT_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>

BOOL rdpei_read_2byte_unsigned(wStream* s, UINT32* value);
BOOL rdpei_write_2byte_unsigned(wStream* s, UINT32 value);
BOOL rdpei_read_2byte_signed(wStream* s, INT32* value);
BOOL rdpei_write_2byte_signed(wStream* s, INT32 value);
BOOL rdpei_read_4byte_unsigned(wStream* s, UINT32* value);
BOOL rdpei_write_4byte_unsigned(wStream* s, UINT32 value);
BOOL rdpei_read_4byte_signed(wStream* s, INT32* value);
BOOL rdpei_write_4byte_signed(wStream* s, INT32 value);
BOOL rdpei_read_8byte_unsigned(wStream* s, UINT64* value);
BOOL rdpei_write_8byte_unsigned(wStream* s, UINT64 value);

#endif /* FREERDP_CHANNEL_RDPEI_CLIENT_COMMON_H */


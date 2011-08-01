/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Fast Path
 *
 * Copyright 2011 Vic Lee
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

#ifndef __FASTPATH_H
#define __FASTPATH_H

#include <freerdp/utils/stream.h>

enum FASTPATH_OUTPUT_ACTION_TYPE
{
	FASTPATH_OUTPUT_ACTION_FASTPATH = 0x0,
	FASTPATH_OUTPUT_ACTION_X224 = 0x3
};

enum FASTPATH_OUTPUT_ENCRYPTION_FLAGS
{
	FASTPATH_OUTPUT_SECURE_CHECKSUM = 0x1,
	FASTPATH_OUTPUT_ENCRYPTED = 0x2
};

uint16 fastpath_read_header(STREAM* s, uint8* encryptionFlags);

#endif

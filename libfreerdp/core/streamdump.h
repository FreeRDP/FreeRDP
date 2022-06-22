/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Static Virtual Channel Interface
 *
 * Copyright 2021 Armin Novak
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef FREERDP_STREAMDUMP_INTERNAL
#define FREERDP_STREAMDUMP_INTERNAL

#include <winpr/wtypes.h>
#include <winpr/stream.h>

#if !defined(BUILD_TESTING)
static
#endif
    BOOL
    stream_dump_read_line(FILE* fp, wStream* s, UINT64* pts, size_t* pOffset, UINT32* flags);

#if !defined(BUILD_TESTING)
static
#endif
    BOOL
    stream_dump_write_line(FILE* fp, UINT32 flags, wStream* s);

#endif

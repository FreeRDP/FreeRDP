/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Interface to dump RDP session data to files
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

#ifndef FREERDP_STREAM_DUMP_H
#define FREERDP_STREAM_DUMP_H

#include <winpr/stream.h>
#include <winpr/wtypes.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

typedef struct stream_dump_context rdpStreamDumpContext;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL stream_dump_read_line(FILE* fp, wStream* s, UINT64* pts, size_t* pOffset);
	FREERDP_API BOOL stream_dump_write_line(FILE* fp, wStream* s);

	FREERDP_API SSIZE_T stream_dump_append(const rdpContext* context, const char* name, wStream* s,
	                                       size_t* offset);
	FREERDP_API SSIZE_T stream_dump_get(const rdpContext* context, const char* name, wStream* s,
	                                    size_t* offset, UINT64* pts);

	FREERDP_API BOOL stream_dump_register_handlers(rdpContext* context, CONNECTION_STATE state);

	FREERDP_API rdpStreamDumpContext* stream_dump_new(void);
	FREERDP_API void stream_dump_free(rdpStreamDumpContext* dump);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_STREAM_DUMP_H */

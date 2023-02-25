/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Clipboard Redirection
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2023 Armin Novak <armin.novak@thincst.com>
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

#ifndef FREERDP_CLIENT_X11_CLIPRDR_FILE_H
#define FREERDP_CLIENT_X11_CLIPRDR_FILE_H

#include <freerdp/client/cliprdr.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct cliprdr_file_context CliprdrFileContext;

	FREERDP_API CliprdrFileContext* cliprdr_file_context_new(void* context);
	FREERDP_API void cliprdr_file_context_free(CliprdrFileContext* file);

	FREERDP_API BOOL cliprdr_file_context_set_locally_available(CliprdrFileContext* file,
	                                                            BOOL available);
	FREERDP_API BOOL cliprdr_file_context_remote_set_flags(CliprdrFileContext* file, UINT32 flags);
	FREERDP_API UINT32 cliprdr_file_context_remote_get_flags(CliprdrFileContext* file);

	FREERDP_API UINT32 cliprdr_file_context_current_flags(CliprdrFileContext* file);

	FREERDP_API void* cliprdr_file_context_get_context(CliprdrFileContext* file);
	FREERDP_API const char* cliprdr_file_context_base_path(CliprdrFileContext* file);

	FREERDP_API BOOL cliprdr_file_context_init(CliprdrFileContext* file,
	                                           CliprdrClientContext* cliprdr);
	FREERDP_API BOOL cliprdr_file_context_uninit(CliprdrFileContext* file,
	                                             CliprdrClientContext* cliprdr);

	FREERDP_API BOOL cliprdr_file_context_clear(CliprdrFileContext* file);

	FREERDP_API BOOL cliprdr_file_context_update_client_data(CliprdrFileContext* file,
	                                                         const char* data, size_t count);

	FREERDP_API BOOL cliprdr_file_context_update_server_data(CliprdrFileContext* file,
	                                                         const void* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_X11_CLIPRDR_FILE_H */

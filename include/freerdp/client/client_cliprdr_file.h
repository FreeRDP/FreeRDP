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

#include <winpr/clipboard.h>

#include <freerdp/client/cliprdr.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct cliprdr_file_context CliprdrFileContext;

	FREERDP_API void cliprdr_file_context_free(CliprdrFileContext* file);

	WINPR_ATTR_MALLOC(cliprdr_file_context_free, 1)
	FREERDP_API CliprdrFileContext* cliprdr_file_context_new(void* context);

	/**! \brief returns if the implementation supports pasting files in a client file browser.
	 *
	 * \param file the file context to query
	 *
	 * \return \b TRUE if files can be pasted locally, \b FALSE if not (e.g. no FUSE, ...)
	 */
	FREERDP_API BOOL cliprdr_file_context_has_local_support(CliprdrFileContext* file);

	/**! \brief sets state of local file paste support
	 *
	 * \param file the file context to update
	 * \param available \b TRUE if the client supports pasting files to local file browsers, \b
	 * FALSE otherwise
	 *
	 * \return \b TRUE for success, \b FALSE otherwise
	 */
	FREERDP_API BOOL cliprdr_file_context_set_locally_available(CliprdrFileContext* file,
	                                                            BOOL available);
	FREERDP_API BOOL cliprdr_file_context_remote_set_flags(CliprdrFileContext* file, UINT32 flags);
	FREERDP_API UINT32 cliprdr_file_context_remote_get_flags(CliprdrFileContext* file);

	FREERDP_API UINT32 cliprdr_file_context_current_flags(CliprdrFileContext* file);

	FREERDP_API void* cliprdr_file_context_get_context(CliprdrFileContext* file);

	FREERDP_API BOOL cliprdr_file_context_init(CliprdrFileContext* file,
	                                           CliprdrClientContext* cliprdr);
	FREERDP_API BOOL cliprdr_file_context_uninit(CliprdrFileContext* file,
	                                             CliprdrClientContext* cliprdr);

	FREERDP_API BOOL cliprdr_file_context_clear(CliprdrFileContext* file);

	FREERDP_API UINT
	cliprdr_file_context_notify_new_server_format_list(CliprdrFileContext* file_context);

	FREERDP_API UINT
	cliprdr_file_context_notify_new_client_format_list(CliprdrFileContext* file_context);

	/** \brief updates the files the client announces to the server
	 *
	 * \param file the file context to update
	 * \param data the file list
	 * \param count the length of the file list
	 *
	 * \return \b TRUE for success, \b FALSE otherwise
	 */
	FREERDP_API BOOL cliprdr_file_context_update_client_data(CliprdrFileContext* file,
	                                                         const char* data, size_t count);
	/** \brief updates the files the server announces to the client
	 *
	 * \param file the file context to update
	 * \param clip the clipboard instance to use
	 * \param data the file list [MS-RDPECLIP] 2.2.5.2.3 Packed File List (CLIPRDR_FILELIST)
	 * \param size the length of the file list
	 *
	 * \return \b TRUE for success, \b FALSE otherwise
	 */
	FREERDP_API BOOL cliprdr_file_context_update_server_data(CliprdrFileContext* file,
	                                                         wClipboard* clip, const void* data,
	                                                         size_t size);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_X11_CLIPRDR_FILE_H */

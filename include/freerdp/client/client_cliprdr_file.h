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
	WINPR_ATTR_NODISCARD
	FREERDP_API CliprdrFileContext* cliprdr_file_context_new(void* context);

	/**! \brief returns if the implementation supports pasting files in a client file browser.
	 *
	 * \param file the file context to query
	 *
	 * \return \b TRUE if files can be pasted locally, \b FALSE if not (e.g. no FUSE, ...)
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL cliprdr_file_context_has_local_support(CliprdrFileContext* file);

	/**! \brief sets state of local file paste support
	 *
	 * \param file the file context to update
	 * \param available \b TRUE if the client supports pasting files to local file browsers, \b
	 * FALSE otherwise
	 *
	 * \return \b TRUE for success, \b FALSE otherwise
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL cliprdr_file_context_set_locally_available(CliprdrFileContext* file,
	                                                            BOOL available);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL cliprdr_file_context_remote_set_flags(CliprdrFileContext* file, UINT32 flags);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT32 cliprdr_file_context_remote_get_flags(CliprdrFileContext* file);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT32 cliprdr_file_context_current_flags(CliprdrFileContext* file);

	WINPR_ATTR_NODISCARD
	FREERDP_API void* cliprdr_file_context_get_context(CliprdrFileContext* file);

	FREERDP_API BOOL cliprdr_file_context_init(CliprdrFileContext* file,
	                                           CliprdrClientContext* cliprdr);

	FREERDP_API BOOL cliprdr_file_context_uninit(CliprdrFileContext* file,
	                                             CliprdrClientContext* cliprdr);

	FREERDP_API BOOL cliprdr_file_context_clear(CliprdrFileContext* file);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT
	cliprdr_file_context_notify_new_server_format_list(CliprdrFileContext* file_context);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT
	cliprdr_file_context_notify_new_client_format_list(CliprdrFileContext* file_context);

	/** \brief updates the files the client announces to the server
	 *
	 * \param file the file context to update
	 * \param data the file list
	 * \param size the length of the file list
	 *
	 * \return \b TRUE for success, \b FALSE otherwise
	 */
	FREERDP_API BOOL cliprdr_file_context_update_client_data(CliprdrFileContext* file,
	                                                         const char* data, size_t size);
	/** \brief updates the files the server announces to the client
	 *
	 * \param file the file context to update
	 * \param clip the clipboard instance to use
	 * \param data the file list [MS-RDPECLIP] 2.2.5.2.3 Packed File List (CLIPRDR_FILELIST)
	 * \param size the length of the file list
	 *
	 * \return \b TRUE for success, \b FALSE otherwise
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL cliprdr_file_context_update_server_data(CliprdrFileContext* file,
	                                                         wClipboard* clip, const void* data,
	                                                         size_t size);

	/** \brief returns the server clipboard data in a format, like \b SDL_ClipboardDataCallback
	 *
	 * \param userdata the \b userdata given to \b cliprdr_file_context_set_local_clipboard_data
	 * \param mime_type the MIME type or clipboard format name of the data
	 * \param size receives the length of the data
	 *
	 * \return the data, owned by the caller of \b cliprdr_file_context_set_local_clipboard_data,
	 * or \b nullptr
	 */
	typedef const void* (*cliprdr_file_context_data_fkt)(void* userdata, const char* mime_type,
	                                                     size_t* size);

	/** \brief offers the server clipboard on the local clipboard, where only the file context
	 * can offer files there
	 *
	 * On macOS the pasteboard needs one item per file, which a toolkit offering MIME types can
	 * not provide. If \b mime_types contains text/uri-list there, the file list is requested
	 * with \b fkt (format name FileGroupDescriptorW) right away, every file is offered as an
	 * item of its own and downloaded with FILECONTENTS_RANGE only when an application pastes
	 * it. The other \b mime_types are offered along with the files and requested with \b fkt on
	 * paste, so text types become the native text and a private type still identifies the copy
	 * when the local clipboard update comes back.
	 *
	 * Call this from the thread that handles local clipboard events, where the toolkit would
	 * be given the data, and without holding locks \b fkt needs.
	 *
	 * \param file the file context to use
	 * \param fkt the function returning the server data
	 * \param userdata passed to \b fkt
	 * \param mime_types the MIME types the server data is offered as
	 * \param count the number of entries in \b mime_types
	 *
	 * \return \b TRUE if the file context now holds the local clipboard, \b FALSE if the caller
	 * has to offer \b mime_types itself (no server files, no support on this platform)
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL cliprdr_file_context_set_local_clipboard_data(
	    CliprdrFileContext* file, cliprdr_file_context_data_fkt fkt, void* userdata,
	    const char** mime_types, size_t count);

	/** \brief returns the files on the local clipboard as text/uri-list, where a toolkit can not
	 * provide them
	 *
	 * On macOS the Finder offers files without a MIME type, so a toolkit working with MIME
	 * types does not see them. Elsewhere this returns \b nullptr and the files are read from
	 * the toolkit as text/uri-list.
	 *
	 * \param file the file context to use
	 * \param size receives the length of the list
	 *
	 * \return a NUL terminated text/uri-list (free with \b free) or \b nullptr
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API char* cliprdr_file_context_get_local_uri_list(CliprdrFileContext* file,
	                                                          size_t* size);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_X11_CLIPRDR_FILE_H */

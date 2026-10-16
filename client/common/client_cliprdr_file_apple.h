/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard file transfer, macOS pasteboard part
 *
 * Copyright 2026 Cong Zhang <13283869+congzhangzh@users.noreply.github.com>
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

#ifndef FREERDP_CLIENT_CLIPRDR_FILE_APPLE_H
#define FREERDP_CLIENT_CLIPRDR_FILE_APPLE_H

#include <winpr/wtypes.h>

#include <freerdp/client/client_cliprdr_file.h>

/* The general pasteboard has one item per file and no MIME type for files
 * (public.file-url has none), so toolkits that work with MIME types can not
 * offer or read files there. These helpers do it on the pasteboard directly. */

#ifdef __cplusplus
extern "C"
{
#endif

	/** Called on the main thread when an application pastes file item \b index.
	 *  \return the absolute path of the local copy, or nullptr. Freed with free(). */
	typedef char* (*cliprdr_file_apple_provide_file_fkt)(void* userdata, size_t index);

	/** \return the files on the general pasteboard as a NUL terminated text/uri-list
	 *  with path based file:// URLs, or nullptr. Free with free(). */
	WINPR_ATTR_NODISCARD
	char* cliprdr_file_apple_get_uri_list(size_t* size);

	/** Put \b count file items on the general pasteboard. The first item also carries
	 *  \b mime_types: text as the native string, other types as they are. Contents are
	 *  only requested from the callbacks when an application pastes them. */
	WINPR_ATTR_NODISCARD
	BOOL cliprdr_file_apple_offer(size_t count, cliprdr_file_apple_provide_file_fkt file_fkt,
	                              void* file_userdata, const char** mime_types, size_t mime_count,
	                              cliprdr_file_context_data_fkt data_fkt, void* data_userdata);

	/** \return a new, empty directory for the files of one offer, or nullptr.
	 *  Free with free(). */
	WINPR_ATTR_NODISCARD
	char* cliprdr_file_apple_new_download_dir(void);

	/** Stop answering for offered items and remove all downloaded files. */
	void cliprdr_file_apple_detach(void);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_CLIPRDR_FILE_APPLE_H */

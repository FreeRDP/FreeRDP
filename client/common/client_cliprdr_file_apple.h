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

	/** Called on the download queue (not the main thread) when an application starts to
	 *  read entry \b index: write its contents to its path, for a directory those of all
	 *  files below it. The reader waits until this returns.
	 *  \return FALSE if the contents are not complete: it removed the files it could not
	 *  download then, so that no empty file is read. */
	typedef BOOL (*cliprdr_file_apple_download_fkt)(void* userdata, size_t index);

	/** Called when no entry of an offer is read any more, frees \b userdata. */
	typedef void (*cliprdr_file_apple_release_fkt)(void* userdata);

	/** \return the files on the general pasteboard as a NUL terminated text/uri-list
	 *  with path based file:// URLs, or nullptr. Free with free(). */
	WINPR_ATTR_NODISCARD
	char* cliprdr_file_apple_get_uri_list(size_t* size);

	/** Put the placeholders \b paths[top_level[]] on the general pasteboard as files. An
	 *  application that pastes them (the Finder) announces its read with file coordination:
	 *  \b download_fkt then fills the placeholder while the reader waits, without blocking
	 *  the main thread. Entries with \b present set are watched for such a read.
	 *
	 *  The first item also carries \b mime_types: text as the native string, other types
	 *  as they are, requested from \b data_fkt when an application pastes them.
	 *
	 *  \b offer_userdata is taken over in any case and freed with \b release_fkt, together
	 *  with \b directory, once the offer was replaced and nobody reads from it. */
	WINPR_ATTR_NODISCARD
	BOOL cliprdr_file_apple_offer(const char* directory, const char** paths, const BOOL* present,
	                              size_t count, const size_t* top_level, size_t top_level_count,
	                              cliprdr_file_apple_download_fkt download_fkt,
	                              cliprdr_file_apple_release_fkt release_fkt, void* offer_userdata,
	                              const char** mime_types, size_t mime_count,
	                              cliprdr_file_context_data_fkt data_fkt, void* data_userdata);

	/** \return a new, empty directory for the files of one offer, or nullptr.
	 *  Free with free(). */
	WINPR_ATTR_NODISCARD
	char* cliprdr_file_apple_new_download_dir(void);

	/** Stop answering for offered items, wait for a running download to return and remove
	 *  all downloaded files. Must not be called on the download queue. */
	void cliprdr_file_apple_detach(void);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_CLIPRDR_FILE_APPLE_H */

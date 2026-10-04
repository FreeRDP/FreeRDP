/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client clipboard helper, macOS file support
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

#pragma once

#include <stdbool.h>
#include <stddef.h>

/* SDL reports the native pasteboard types on macOS, and a Finder copy has no
 * MIME type for its files (public.file-url has none), so read and write them
 * directly. */

#ifdef __cplusplus
extern "C"
{
#endif

	/** @return true if the general pasteboard holds file URLs */
	bool sdl_clip_macos_has_files(void);

	/** @return the files on the general pasteboard as a NUL terminated text/uri-list
	 *  with path based file:// URLs, or NULL. Free with free(). */
	char* sdl_clip_macos_get_uri_list(size_t* size);

	/** Called on the main thread when an application pastes item @p index.
	 *  @return the absolute path of the local copy, or NULL. Freed with free(). */
	typedef char* (*sdl_clip_macos_provide_cb)(void* userdata, size_t index);

	/** Called on the main thread when an application pastes the offered files
	 *  as text. @return UTF-8 text, or NULL. Freed with free(). */
	typedef char* (*sdl_clip_macos_provide_text_cb)(void* userdata);

	/** Put @p count files on the general pasteboard, and their text (e.g. the
	 *  paths) if @p text_cb is set. The contents are only requested from the
	 *  callbacks when an application pastes them. */
	bool sdl_clip_macos_offer_files(size_t count, sdl_clip_macos_provide_cb cb,
	                                sdl_clip_macos_provide_text_cb text_cb, void* userdata);

	/** @return true if the current pasteboard contents were written by
	 *  sdl_clip_macos_offer_files() */
	bool sdl_clip_macos_is_own_change(void);

	/** @return a new, empty directory for the files of one offer, or NULL.
	 *  Free with free(). */
	char* sdl_clip_macos_new_download_dir(void);

	/** Stop answering for offered files and remove all downloaded files. */
	void sdl_clip_macos_detach(void);

#ifdef __cplusplus
}
#endif

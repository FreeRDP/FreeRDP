/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
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

#ifndef FREERDP_CHANNEL_SERVER_CLIPRDR_MAIN_H
#define FREERDP_CHANNEL_SERVER_CLIPRDR_MAIN_H

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/server/cliprdr.h>

#define CLIPRDR_HEADER_LENGTH	8

struct _CLIPRDR_HEADER
{
	UINT16 msgType;
	UINT16 msgFlags;
	UINT32 dataLen;
};
typedef struct _CLIPRDR_HEADER CLIPRDR_HEADER;

struct _cliprdr_server_private
{
	HANDLE Thread;
	HANDLE StopEvent;
	void* ChannelHandle;

	BOOL UseLongFormatNames;
	BOOL StreamFileClipEnabled;
	BOOL FileClipNoFilePaths;
	BOOL CanLockClipData;

	UINT32 ClientFormatNameCount;
	CLIPRDR_FORMAT_NAME* ClientFormatNames;

	char* ClientTemporaryDirectory;
};

#endif /* FREERDP_CHANNEL_SERVER_CLIPRDR_MAIN_H */

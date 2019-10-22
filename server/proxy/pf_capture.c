/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server Session Capture
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include <stdio.h>
#include <string.h>

#include "pf_capture.h"

static BOOL pf_capture_create_dir_if_not_exists(const char* path)
{
	if (PathFileExistsA(path))
		return TRUE;

	return CreateDirectoryA(path, NULL);
}

/* creates a directory to store captured session frames.
 *
 * @context: current session.
 *
 * directory path will be: base_dir/username/session-start-date.
 *
 * it is important to call this function only after the connection is fully established, as it uses
 * settings->Username and settings->ServerHostname values to create the directory. After the
 * connection established, we know that those values are valid.
 */
BOOL pf_capture_create_session_directory(pClientContext* pc)
{
	proxyConfig* config = pc->pdata->config;
	rdpSettings* settings = pc->context.settings;
	SYSTEMTIME localTime;
	char tmp[MAX_PATH];
	const char* fmt = "%s/%s/%s_%02u-%02u-%"PRIu16"_%02u-%02u-%02u-%03u";

	_snprintf(tmp, sizeof(tmp), "%s/%s", config->CapturesDirectory, settings->Username);
	if (!pf_capture_create_dir_if_not_exists(tmp))
		return FALSE;

	pc->frames_dir = malloc(MAX_PATH);
	if (!pc->frames_dir)
		return FALSE;

	GetLocalTime(&localTime);
	sprintf_s(pc->frames_dir, MAX_PATH, fmt, config->CapturesDirectory, settings->Username,
	          settings->ServerHostname, localTime.wDay, localTime.wMonth, localTime.wYear,
	          localTime.wHour, localTime.wMinute, localTime.wSecond, localTime.wMilliseconds);

	return pf_capture_create_dir_if_not_exists(pc->frames_dir);
}

/* saves a captured frame in a BMP format. */
BOOL pf_capture_save_frame(pClientContext* pc, const BYTE* frame)
{
	rdpSettings* settings = pc->context.settings;
	const char* fmt = "%s/%"PRIu64".bmp";
	char file_path[MAX_PATH];

	sprintf_s(file_path, sizeof(file_path), fmt, pc->frames_dir, pc->frames_count++);
	return winpr_bitmap_write(file_path, frame, settings->DesktopWidth, settings->DesktopHeight,
	                          settings->ColorDepth);
}

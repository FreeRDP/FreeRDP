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
#include <winpr/image.h>
#include <winpr/sysinfo.h>
#include <winpr/path.h>
#include <winpr/file.h>

#include "pf_capture.h"

static BOOL pf_capture_create_dir_if_not_exists(const char* path)
{
	if (PathFileExistsA(path))
		return TRUE;

	return CreateDirectoryA(path, NULL);
}

static BOOL pf_capture_create_user_captures_dir(const char* base_dir, const char* username)
{
	int rc;
	size_t size;
	char* buf = NULL;
	BOOL ret = FALSE;

	/* create sub-directory in base captures directory for current username, if it doesn't already
	 * exists. */
	rc = _snprintf(NULL, 0, "%s/%s", base_dir, username);
	if (rc < 0)
		return FALSE;

	size = (size_t)rc;

	buf = malloc(size + 1);
	if (!buf)
		return FALSE;

	rc = sprintf(buf, "%s/%s", base_dir, username);
	if (rc < 0 || (size_t)rc != size)
		goto out;

	if (!pf_capture_create_dir_if_not_exists(buf))
		goto out;

	ret = TRUE;

out:
	free(buf);
	return ret;
}

static BOOL pf_capture_create_current_session_captures_dir(pClientContext* pc)
{
	proxyConfig* config = pc->pdata->config;
	rdpSettings* settings = pc->context.settings;
	const char* fmt = "%s/%s/%s_%02u-%02u-%" PRIu16 "_%02u-%02u-%02u-%03u";
	int rc;
	size_t size;
	SYSTEMTIME localTime;

	GetLocalTime(&localTime);

	/* create sub-directory in current user's captures directory, for the specific session. */
	rc = _snprintf(NULL, 0, fmt, config->CapturesDirectory, settings->Username,
	               settings->ServerHostname, localTime.wDay, localTime.wMonth, localTime.wYear,
	               localTime.wHour, localTime.wMinute, localTime.wSecond, localTime.wMilliseconds);

	if (rc < 0)
		return FALSE;

	size = (size_t)rc;

	/* `pc->frames_dir` will be used by proxy client for saving frames to storage. */
	pc->frames_dir = malloc(size + 1);
	if (!pc->frames_dir)
		return FALSE;

	rc = sprintf(pc->frames_dir, fmt, config->CapturesDirectory, settings->Username,
	             settings->ServerHostname, localTime.wDay, localTime.wMonth, localTime.wYear,
	             localTime.wHour, localTime.wMinute, localTime.wSecond, localTime.wMilliseconds);

	if (rc < 0 || (size_t)rc != size)
		goto error;

	if (!pf_capture_create_dir_if_not_exists(pc->frames_dir))
		goto error;

	return TRUE;

error:
	free(pc->frames_dir);
	return FALSE;
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

	if (!pf_capture_create_user_captures_dir(config->CapturesDirectory, settings->Username))
		return FALSE;

	if (!pf_capture_create_current_session_captures_dir(pc))
		return FALSE;

	return TRUE;
}

/* saves a captured frame in a BMP format. */
BOOL pf_capture_save_frame(pClientContext* pc, const BYTE* frame)
{
	rdpSettings* settings = pc->context.settings;
	int rc;
	const char* fmt = "%s/%" PRIu64 ".bmp";
	char* file_path = NULL;
	size_t size;

	if (!pc->frames_dir)
		return FALSE;

	rc = _snprintf(NULL, 0, fmt, pc->frames_dir, pc->frames_count++);
	if (rc < 0)
		return FALSE;

	size = (size_t)rc;
	file_path = malloc(size + 1);
	if (!file_path)
		return FALSE;

	rc = sprintf(file_path, fmt, pc->frames_dir, pc->frames_count++);
	if (rc < 0 || (size_t)rc != size)
		goto out;

	rc = winpr_bitmap_write(file_path, frame, settings->DesktopWidth, settings->DesktopHeight,
	                        settings->ColorDepth);

out:
	free(file_path);
	return rc;
}

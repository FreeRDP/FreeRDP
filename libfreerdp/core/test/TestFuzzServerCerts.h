/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Embedded test certificate and RSA private key used by the
 * server fuzz target and its seed generator.
 *
 * This is a throwaway, self-signed test certificate used only to
 * make the FreeRDP server core accept a connection. It is not a
 * secret and must not be used anywhere outside of tests.
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

#include <winpr/wtypes.h>
#include <winpr/path.h>
#include <winpr/tools/makecert.h>

static const char testCredentialsName[] = "TestFuzzServerCerts";

WINPR_ATTR_NODISCARD
static inline BOOL server_create_certificate(const char* filepath, const char* name)
{
	BOOL rc = FALSE;
	char* makecert_argv[6] = { "makecert", "-rdp", "-live", "-silent", "-y", "5" };

	if (!winpr_PathFileExists(filepath))
	{
		if (!winpr_PathMakePath(filepath, nullptr))
			return FALSE;
	}

	WINPR_STATIC_ASSERT(ARRAYSIZE(makecert_argv) <= INT_MAX);
	const size_t makecert_argc = ARRAYSIZE(makecert_argv);

	char* fcert = GetCombinedPathV(filepath, "%s.%s", name, "crt");
	char* fkey = GetCombinedPathV(filepath, "%s.%s", name, "key");
	const BOOL res = winpr_PathFileExists(fcert) && winpr_PathFileExists(fkey);
	free(fcert);
	free(fkey);
	if (res)
		return TRUE;

	MAKECERT_CONTEXT* makecert = makecert_context_new();

	if (!makecert)
		goto out_fail;

	if (makecert_context_process(makecert, (int)makecert_argc, makecert_argv) < 0)
		goto out_fail;

	if (makecert_context_set_output_file_name(makecert, name) != 1)
		goto out_fail;

	WINPR_ASSERT(filepath);
	if (makecert_context_output_certificate_file(makecert, filepath) != 1)
		goto out_fail;

	if (makecert_context_output_private_key_file(makecert, filepath) != 1)
		goto out_fail;

	rc = TRUE;
out_fail:
	makecert_context_free(makecert);
	return rc;
}

WINPR_ATTR_MALLOC(free, 1)
static inline char* getTestCredentialsFilePath(void)
{
	char* path = GetKnownSubPath(KNOWN_PATH_TEMP, testCredentialsName);
	if (!path)
		return nullptr;
	if (!server_create_certificate(path, testCredentialsName))
	{
		free(path);
		return nullptr;
	}
	return path;
}

WINPR_ATTR_MALLOC(free, 1)
static inline char* getTestCredentialsFileNameFor(const char* ext)
{
	if (!ext)
		return nullptr;

	char* path = getTestCredentialsFilePath();
	if (!path)
		return nullptr;

	char* name = GetCombinedPathV(path, "%s.%s", testCredentialsName, ext);
	free(path);
	return name;
}

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2025 Thincast Technologies GmbH
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdio.h>

#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/debug.h>
#include <winpr/print.h>

#include "TestFreeRDPHelpers.h"

static char* get_path(const char* codec, const char* type, const char* name)
{
	char path[500] = WINPR_C_ARRAY_INIT;
	(void)snprintf(path, sizeof(path), "%s-%s-%s.bin", codec, type, name);
	char* s1 = GetCombinedPath(CMAKE_CURRENT_SOURCE_DIR, codec);
	if (!s1)
		return nullptr;

	char* s2 = GetCombinedPath(s1, path);
	free(s1);
	return s2;
}

static FILE* open_path(const char* codec, const char* type, const char* name, const char* mode)
{
	WINPR_ASSERT(type);
	WINPR_ASSERT(name);
	WINPR_ASSERT(mode);

	char* path = get_path(codec, type, name);
	if (!path)
	{
		(void)printf("%s: get_path %s %s failed\n", __func__, type, name);
		return nullptr;
	}

	FILE* fp = winpr_fopen(path, mode);
	if (!fp)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		(void)printf("%s: %s %s: fopen(%s, %s) failed: %s\n", __func__, type, name, path, mode,
		             winpr_strerror(errno, buffer, sizeof(buffer)));
	}
	free(path);
	return fp;
}

void* test_codec_helper_read_data(const char* codec, const char* type, const char* name,
                                  size_t* plength)
{
	WINPR_ASSERT(type);
	WINPR_ASSERT(name);
	WINPR_ASSERT(plength);

	void* rc = nullptr;
	void* cmp = nullptr;

	*plength = 0;
	FILE* fp = open_path(codec, type, name, "rb");
	if (!fp)
		goto fail;

	if (_fseeki64(fp, 0, SEEK_END) != 0)
		goto fail;

	const size_t pos = _ftelli64(fp);

	if (_fseeki64(fp, 0, SEEK_SET) != 0)
		goto fail;

	cmp = calloc(pos, 1);
	if (!cmp)
		goto fail;

	if (fread(cmp, 1, pos, fp) != pos)
		goto fail;

	*plength = pos;
	rc = cmp;
	cmp = nullptr;

fail:
	(void)printf("%s: [%s] %s %s -> %p\n", __func__, codec, type, name, rc);
	free(cmp);
	if (fp)
		(void)fclose(fp);
	return rc;
}

void test_codec_helper_write_data(const char* codec, const char* type, const char* name,
                                  const void* data, size_t length)
{
	FILE* fp = open_path(codec, type, name, "wb");
	if (!fp)
		return;

	if (fwrite(data, 1, length, fp) != length)
		goto fail;

fail:
	fclose(fp);
}

bool test_codec_helper_compare(const char* codec, const char* type, const char* name,
                               const void* data, size_t length)
{
	bool rc = false;
	size_t cmplen = 0;
	void* cmp = test_codec_helper_read_data(codec, type, name, &cmplen);
	if (!cmp)
		goto fail;
	if (cmplen != length)
	{
		(void)printf("%s: [%s] %s %s: length mismatch: %" PRIuz " vs %" PRIuz "\n", __func__, codec,
		             type, name, cmplen, length);
		goto fail;
	}
	if (memcmp(data, cmp, length) != 0)
	{
		(void)printf("%s: [%s] %s %s: data mismatch\n", __func__, codec, type, name);
		winpr_HexDump(__func__, WLOG_WARN, data, length);
		winpr_HexDump(__func__, WLOG_WARN, cmp, cmplen);
		goto fail;
	}
	rc = true;
fail:
	(void)printf("%s: [%s] %s %s -> %s\n", __func__, codec, type, name, rc ? "SUCCESS" : "FAILED");
	free(cmp);
	return rc;
}

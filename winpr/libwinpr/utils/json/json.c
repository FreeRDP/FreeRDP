/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * JSON parser wrapper
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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
#include <math.h>
#include <errno.h>

#include <winpr/file.h>
#include <winpr/json.h>
#include <winpr/assert.h>

WINPR_JSON* WINPR_JSON_ParseFromFile(const char* filename)
{
	FILE* fp = winpr_fopen(filename, "r");
	if (!fp)
		return NULL;
	WINPR_JSON* json = WINPR_JSON_ParseFromFileFP(fp);
	(void)fclose(fp);
	return json;
}

WINPR_JSON* WINPR_JSON_ParseFromFileFP(FILE* fp)
{
	if (!fp)
		return NULL;

	if (fseek(fp, 0, SEEK_END) != 0)
		return NULL;

	const INT64 size = _ftelli64(fp);
	if (size < 0)
		return NULL;

	if (fseek(fp, 0, SEEK_SET) != 0)
		return NULL;

	const size_t usize = WINPR_ASSERTING_INT_CAST(size_t, size);
	char* str = calloc(usize + 1, sizeof(char));
	if (!str)
		return NULL;

	WINPR_JSON* json = NULL;
	const size_t s = fread(str, sizeof(char), usize, fp);
	if (s == usize)
		json = WINPR_JSON_ParseWithLength(str, usize);
	free(str);
	return json;
}

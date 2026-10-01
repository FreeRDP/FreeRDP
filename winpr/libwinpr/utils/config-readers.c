/**
 * WinPR: Windows Portable Runtime
 * JSON configuration read helpers
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
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
#include <float.h>
#include <winpr/config-readers.h>

#include "../log.h"
#define TAG WINPR_TAG("utils.config-readers")

BOOL winpr_config_apply_bool(const char* config, WINPR_JSON* json, const char* name, BOOL* dst)
{
	WINPR_ASSERT(dst);

	WINPR_JSON* obj = WINPR_JSON_GetObjectItemCaseSensitive(json, name);
	if (!obj)
		return FALSE;

	if (!WINPR_JSON_IsBool(obj))
	{
		WLog_WARN(TAG, "[%s] Invalid setting %s: must be of type bool", config, name);
		return FALSE;
	}

	*dst = WINPR_JSON_IsTrue(obj);
	return TRUE;
}

BOOL winpr_config_apply_string(const char* config, WINPR_JSON* json, const char* name, char** dst)
{
	WINPR_ASSERT(dst);

	WINPR_JSON* obj = WINPR_JSON_GetObjectItemCaseSensitive(json, name);
	if (!obj)
		return FALSE;

	if (!WINPR_JSON_IsString(obj))
	{
		WLog_WARN(TAG, "[%s] Invalid setting %s: must be of type string", config, name);
		return FALSE;
	}

	const char* val = WINPR_JSON_GetStringValue(obj);
	free(*dst);
	*dst = nullptr;
	if (val)
	{
		*dst = _strdup(val);
		if (!*dst)
			return FALSE;
	}
	return TRUE;
}

BOOL winpr_config_apply_uint32(const char* config, WINPR_JSON* json, const char* name, UINT32* dst)
{
	WINPR_ASSERT(dst);

	WINPR_JSON* obj = WINPR_JSON_GetObjectItemCaseSensitive(json, name);
	if (!obj)
		return FALSE;

	if (!WINPR_JSON_IsNumber(obj))
	{
		WLog_WARN(TAG, "[%s] Invalid setting %s: must be of type bool", config, name);
		return FALSE;
	}

	const double val = WINPR_JSON_GetNumberValue(obj);

	double integer = 0.0;
	const double fractional = modf(val, &integer);
	if (signbit(val) || isnan(val) || isinf(val) || (val > UINT32_MAX) ||
	    (fabs(fractional) > DBL_EPSILON))
	{
		WLog_WARN(TAG, "[%s] Invalid setting %s: value must be 0 <= %f <= %" PRIu32, config, name,
		          val, UINT32_MAX);
		return FALSE;
	}

	*dst = (UINT32)integer;
	return TRUE;
}

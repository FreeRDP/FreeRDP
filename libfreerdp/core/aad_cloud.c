/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Settings AAD cloud parser
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
#include <winpr/json.h>
#include <winpr/path.h>

#include <freerdp/utils/helpers.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/log.h>

#include "utils.h"
#include "aad_cloud.h"

#define TAG FREERDP_TAG("core.aad-cloud")

struct freerdp_aad_cloud
{
	char* name;
	char* gateway_suffix;
	char* authority;
	char* avd_scope;
	char* avd_redirect_format;
};

static const FreeRDP_AadCloud aad_clouds[] = {
	{ "commercial", ".wvd.microsoft.com", "login.microsoftonline.com",
	  "https://www.wvd.microsoft.com/.default", "https://%s/%s/oauth2/nativeclient" },
	{ "usgov", ".wvd.azure.us", "login.microsoftonline.us",
	  "https://www.wvd.azure.us/.default openid profile offline_access",
	  /* the registered redirect of the AVD public client (#11032), the .us form is rejected
	   * with AADSTS50011 */
	  "https://login.microsoftonline.com/common/oauth2/Fnativeclient" }
};

#define AAD_CLOUDS_CONFIG_FILE "aad-clouds.json"

/* '%s' conversions of the redirect format: authority and tenant id */
#define AAD_CLOUD_REDIRECT_CONVERSIONS 2

static INIT_ONCE aad_clouds_once = INIT_ONCE_STATIC_INIT;

static FreeRDP_AadCloud* aad_clouds_configured = nullptr;
static size_t aad_clouds_configured_count = 0;

static void aad_cloud_entry_free(FreeRDP_AadCloud* cloud)
{
	if (!cloud)
		return;

	free(cloud->name);
	free(cloud->gateway_suffix);
	free(cloud->authority);
	free(cloud->avd_scope);
	free(cloud->avd_redirect_format);

	const FreeRDP_AadCloud empty = { nullptr, nullptr, nullptr, nullptr, nullptr };
	*cloud = empty;
}

static void aad_cloud_table_free(FreeRDP_AadCloud* table, size_t count)
{
	for (size_t x = 0; x < count; x++)
		aad_cloud_entry_free(&table[x]);
	free(table);
}

WINPR_ATTR_MALLOC(free, 1)
static char* printescape(const char* what)
{
	if (!what)
		return nullptr;

	const size_t len = strlen(what);
	char* peseg = calloc(len + 1, 2);
	if (!peseg)
		return nullptr;

	size_t pos = 0;
	for (size_t x = 0; x < len; x++)
	{
		char cur = what[x];
		if (cur == '%')
		{
			peseg[pos++] = '%';
			peseg[pos++] = '%';
		}
		else
			peseg[pos++] = cur;
	}
	return peseg;
}

WINPR_ATTR_NODISCARD
static BOOL appendSegment(const char* seg, size_t seglen, char* buffer, size_t bufferlen)
{
	char* eseg = winpr_str_url_encode(seg, seglen);
	if (!eseg)
		return FALSE;

	char* peseg = printescape(eseg);
	free(eseg);
	if (!peseg)
		return FALSE;

	const BOOL rc = winpr_str_append(peseg, buffer, bufferlen, "");
	free(peseg);
	return rc;
}

WINPR_ATTR_MALLOC(free, 1)
static char* url_encode(const char* strWithFormat)
{
	if (!strWithFormat)
		return nullptr;
	const size_t len = strlen(strWithFormat);
	const char* token = strstr(strWithFormat, "%s");
	if (!token)
	{
		char* str = printescape(strWithFormat);
		if (!str)
			return nullptr;
		char* rc = winpr_str_url_encode(str, strlen(str));
		free(str);
		return rc;
	}

	// Overallocate. URL encoding will use at most 4 times the original length.
	char* str = calloc(len + 1, 4);
	const size_t slen = len * 4;
	if (!str)
		return nullptr;

	const char* last = strWithFormat;
	do
	{
		const size_t seglen = WINPR_ASSERTING_INT_CAST(size_t, (token - last));
		if (!appendSegment(last, seglen, str, slen))
		{
			free(str);
			return nullptr;
		}
		winpr_str_append("%s", str, slen, "");

		last = token + 2;
		token = strstr(last, "%s");
		if (!token)
		{
			const size_t llen = strnlen(last, len);
			if (llen > 0)
			{
				if (!appendSegment(last, llen, str, slen))
				{
					free(str);
					return nullptr;
				}
			}
		}
	} while (token);

	return str;
}

WINPR_ATTR_NODISCARD
static BOOL aad_cloud_entry_set(FreeRDP_AadCloud* dst, const char* name, const char* suffix,
                                const char* authority, const char* scope, const char* redirect)
{
	if (!name || !authority || !scope || !redirect)
		return FALSE;

	FreeRDP_AadCloud copy = { .name = _strdup(name),
		                      .authority = _strdup(authority),
		                      .avd_scope = url_encode(scope),
		                      .avd_redirect_format = url_encode(redirect),
		                      .gateway_suffix = suffix ? _strdup(suffix) : nullptr };

	if (!copy.name || !copy.authority || !copy.avd_scope || !copy.avd_redirect_format ||
	    (suffix && !copy.gateway_suffix))
	{
		aad_cloud_entry_free(&copy);
		return FALSE;
	}

	*dst = copy;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL aad_cloud_table_materialize(FreeRDP_AadCloud** ptable, size_t* pcount)
{
	WINPR_ASSERT(pcount);

	const size_t count = ARRAYSIZE(aad_clouds);
	FreeRDP_AadCloud* table = calloc(count, sizeof(FreeRDP_AadCloud));
	if (!table)
		return FALSE;

	for (size_t x = 0; x < count; x++)
	{
		const FreeRDP_AadCloud* src = &aad_clouds[x];
		if (!aad_cloud_entry_set(&table[x], src->name, src->gateway_suffix, src->authority,
		                         src->avd_scope, src->avd_redirect_format))
		{
			aad_cloud_table_free(table, count);
			return FALSE;
		}
	}

	*ptable = table;
	*pcount = count;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL aad_cloud_table_merge(FreeRDP_AadCloud** ptable, size_t* pcount,
                                  const FreeRDP_AadCloud* entry)
{
	WINPR_ASSERT(ptable);
	WINPR_ASSERT(pcount);
	WINPR_ASSERT(entry);

	const size_t count = *pcount;
	for (size_t x = 0; x < count; x++)
	{
		FreeRDP_AadCloud* cur = &(*ptable)[x];
		if (_stricmp(cur->name, entry->name) == 0)
		{
			WLog_DBG(TAG, "Overriding duplicate entry %s", entry->name);
			aad_cloud_entry_free(cur);
			*cur = *entry;
			return TRUE;
		}
	}

	if (count > SIZE_MAX / sizeof(FreeRDP_AadCloud) - sizeof(FreeRDP_AadCloud))
		return FALSE;

	FreeRDP_AadCloud* tmp = realloc(*ptable, (count + 1) * sizeof(FreeRDP_AadCloud));
	if (!tmp)
		return FALSE;
	tmp[count] = *entry;
	*ptable = tmp;
	*pcount = count + 1;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL aad_cloud_valid_gateway_suffix(const char* value)
{
	if (!value || (strlen(value) < 2) || (value[0] != '.'))
		return FALSE;

	return winpr_str_is_valid_hostname(&value[1]);
}

/* plain URL with at most two '%s' for the authority and the tenant id */
WINPR_ATTR_NODISCARD
static BOOL aad_cloud_valid_redirect_format(const char* value)
{
	size_t conversions = 0;
	for (const char* pos = strstr(value, "%s"); pos; pos = strstr(pos + 2, "%s"))
		conversions++;

	return conversions <= AAD_CLOUD_REDIRECT_CONVERSIONS;
}

WINPR_ATTR_NODISCARD
static const char* aad_cloud_json_string(const WINPR_JSON* obj, const char* key)
{
	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, key);
	if (!item || !WINPR_JSON_IsString(item))
		return nullptr;
	return WINPR_JSON_GetStringValue(item);
}

WINPR_ATTR_MALLOC(free, 1)
static char* aad_cloud_json_array_as_list(const WINPR_JSON* obj, const char* key)
{
	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, key);
	if (!item || !WINPR_JSON_IsArray(item))
		return nullptr;

	size_t count = 0;
	for (size_t x = 0; x < WINPR_JSON_GetArraySize(item); x++)
	{
		WINPR_JSON* cur = WINPR_JSON_GetArrayItem(item, x);
		if (!cur || !WINPR_JSON_IsString(cur))
			return nullptr;
		const char* str = WINPR_JSON_GetStringValue(cur);
		if (utils_str_is_empty(str))
			return nullptr;
		count += strlen(str) + 1;
	}

	char* str = calloc(count + 1, sizeof(char));
	if (!str)
		return nullptr;

	for (size_t x = 0; x < WINPR_JSON_GetArraySize(item); x++)
	{
		WINPR_JSON* cur = WINPR_JSON_GetArrayItem(item, x);
		const char* curstr = WINPR_JSON_GetStringValue(cur);
		if (!winpr_str_append(curstr, str, count, " "))
		{
			free(str);
			return nullptr;
		}
	}
	return str;
}

WINPR_ATTR_NODISCARD
static BOOL aad_cloud_parse_entry(const char* path, size_t index, WINPR_JSON* item,
                                  FreeRDP_AadCloud* cloud)
{
	if (!WINPR_JSON_IsObject(item))
	{
		WLog_WARN(TAG, "[%s] entry %" PRIuz " invalid: not a JSON object", path, index);
		return FALSE;
	}

	const char* name = aad_cloud_json_string(item, "name");
	const char* authority = aad_cloud_json_string(item, "active_directory");
	const char* redirect = aad_cloud_json_string(item, "avd_redirect_format");
	const char* suffix = aad_cloud_json_string(item, "gateway_suffix");

	if (!winpr_str_is_valid_hostname(name))
	{
		WLog_WARN(TAG,
		          "[%s] entry %" PRIuz
		          " invalid: 'name' is missing or not a [a-z0-9-] string, got '%s'",
		          path, index, name ? name : "");
		return FALSE;
	}
	if (_stricmp(name, aad_clouds[0].name) == 0)
	{
		WLog_WARN(TAG, "[%s] entry %" PRIuz " invalid: '%s' can not be overridden", path, index,
		          name);
		return FALSE;
	}
	if (!winpr_str_is_valid_hostname(authority))
	{
		WLog_WARN(TAG,
		          "[%s] entry %" PRIuz " ('%s') invalid: 'active_directory' is missing or not a "
		          "host name without scheme or path, got '%s'",
		          path, index, name, authority ? authority : "");
		return FALSE;
	}
	if (!aad_cloud_valid_redirect_format(redirect))
	{
		WLog_WARN(TAG,
		          "[%s] entry %" PRIuz " ('%s') invalid: 'avd_redirect_format' is missing, empty "
		          "or has more than %d '%%s'",
		          path, index, name, AAD_CLOUD_REDIRECT_CONVERSIONS);
		return FALSE;
	}
	if (WINPR_JSON_HasObjectItem(item, "gateway_suffix") && !aad_cloud_valid_gateway_suffix(suffix))
	{
		WLog_WARN(TAG,
		          "[%s] entry %" PRIuz " ('%s') invalid: 'gateway_suffix' is not a host name "
		          "starting with the '.' of a label boundary, got '%s'",
		          path, index, name, suffix ? suffix : "");
		return FALSE;
	}

	if (WINPR_JSON_HasObjectItem(item, "resource_manager"))
	{
		WLog_DBG(TAG,
		         "[%s] entry %" PRIuz " ('%s'): 'resource_manager' is accepted and ignored, no "
		         "setting holds an ARM endpoint",
		         path, index, name);
	}
	char* scope = aad_cloud_json_array_as_list(item, "avd_scope");
	if (utils_str_is_empty(scope))
	{
		WLog_WARN(TAG, "[%s] entry %" PRIuz " ('%s') invalid: 'avd_scope' is missing or empty",
		          path, index, name);
		free(scope);
		return FALSE;
	}
	const BOOL rc = aad_cloud_entry_set(cloud, name, suffix, authority, scope, redirect);
	free(scope);
	if (!rc)
	{
		WLog_ERR(TAG, "[%s] entry %" PRIuz " ('%s') invalid: out of memory", path, index, name);
		return FALSE;
	}
	return TRUE;
}

static void aad_cloud_load_file(BOOL system, FreeRDP_AadCloud** ptable, size_t* pcount)
{
	char* path = freerdp_GetConfigFilePath(system, AAD_CLOUDS_CONFIG_FILE);
	if (!path)
		return;

	WINPR_JSON* json = freerdp_GetJSONConfigFile(system, AAD_CLOUDS_CONFIG_FILE);
	if (!json)
	{
		if (winpr_PathFileExists(path))
			WLog_WARN(TAG, "[%s] ignored: the file does not parse as JSON", path);
		else
			WLog_DBG(TAG, "[%s] no AVD cloud configuration file", path);
		goto fail;
	}

	if (!WINPR_JSON_IsArray(json))
	{
		WLog_WARN(TAG, "[%s] ignored: expected a JSON array of cloud objects", path);
		goto fail;
	}

	const size_t count = WINPR_JSON_GetArraySize(json);
	for (size_t x = 0; x < count; x++)
	{
		FreeRDP_AadCloud entry = { nullptr, nullptr, nullptr, nullptr, nullptr };
		if (!aad_cloud_parse_entry(path, x, WINPR_JSON_GetArrayItem(json, x), &entry))
			goto fail;

		WLog_DBG(TAG, "[%s] configured AVD cloud '%s'", path, entry.name);
		if (!aad_cloud_table_merge(ptable, pcount, &entry))
		{
			WLog_ERR(TAG, "[%s] entry %" PRIuz " ignored: out of memory", path, x);
			aad_cloud_entry_free(&entry);
		}
	}

fail:
	WINPR_JSON_Delete(json);
	free(path);
}

WINPR_ATTR_NODISCARD
static BOOL CALLBACK aad_clouds_init(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                     WINPR_ATTR_UNUSED PVOID param,
                                     WINPR_ATTR_UNUSED PVOID* context)
{
	if (!aad_cloud_table_materialize(&aad_clouds_configured, &aad_clouds_configured_count))
		return FALSE;
	aad_cloud_load_file(TRUE, &aad_clouds_configured, &aad_clouds_configured_count);
	aad_cloud_load_file(FALSE, &aad_clouds_configured, &aad_clouds_configured_count);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static const FreeRDP_AadCloud* aad_cloud_table(size_t* pcount)
{
	WINPR_ASSERT(pcount);

	if (InitOnceExecuteOnce(&aad_clouds_once, aad_clouds_init, nullptr, nullptr) &&
	    aad_clouds_configured)
	{
		*pcount = aad_clouds_configured_count;
		return aad_clouds_configured;
	}

	*pcount = ARRAYSIZE(aad_clouds);
	return aad_clouds;
}

const FreeRDP_AadCloud* freerdp_utils_aad_cloud_by_name(const char* name)
{
	if (!name)
		return nullptr;

	size_t count = 0;
	const FreeRDP_AadCloud* clouds = aad_cloud_table(&count);
	for (size_t x = 0; x < count; x++)
	{
		if (_stricmp(name, clouds[x].name) == 0)
			return &clouds[x];
	}
	return nullptr;
}

const FreeRDP_AadCloud* freerdp_utils_aad_cloud_for_gateway(const char* hostname)
{
	if (!hostname)
		return nullptr;

	size_t count = 0;
	const FreeRDP_AadCloud* clouds = aad_cloud_table(&count);
	size_t hostlen = strlen(hostname);
	if ((hostlen > 0) && (hostname[hostlen - 1] == '.'))
		hostlen--;
	for (size_t x = 0; x < count; x++)
	{
		const char* suffix = clouds[x].gateway_suffix;
		if (!suffix)
			continue;

		const size_t suffixlen = strlen(suffix);
		if ((hostlen >= suffixlen) &&
		    (_strnicmp(&hostname[hostlen - suffixlen], suffix, suffixlen) == 0))
			return &clouds[x];
	}
	return nullptr;
}

BOOL freerdp_utils_aad_apply_cloud(rdpSettings* settings, const FreeRDP_AadCloud* cloud)
{
	if (!settings || !cloud)
		return FALSE;

	const char* authority = cloud->authority;
	const char* scope = cloud->avd_scope;
	const char* redirect = cloud->avd_redirect_format;
	if (!authority || !scope || !redirect)
		return FALSE;

	BOOL rc = freerdp_settings_set_string(settings, FreeRDP_GatewayAzureActiveDirectory, authority);
	if (!rc)
		return FALSE;

	rc = freerdp_settings_set_string(settings, FreeRDP_GatewayAvdScope, scope);
	if (!rc)
		return FALSE;

	return freerdp_settings_set_string(settings, FreeRDP_GatewayAvdAccessAadFormat, redirect);
}

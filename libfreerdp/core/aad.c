/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Level Authentication (NLA)
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <stdio.h>
#include <string.h>

#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/privatekey.h>
#include "../crypto/privatekey.h"
#include <freerdp/utils/http.h>
#include <freerdp/utils/aad.h>

#include <winpr/cast.h>
#include <winpr/crypto.h>
#include <winpr/json.h>
#include <winpr/path.h>
#include <winpr/synch.h>

#include <freerdp/utils/helpers.h>

#include "transport.h"
#include "rdp.h"
#include "settings.h"

#include "aad.h"

#include <freerdp/log.h>
#define TAG FREERDP_TAG("core.aad")

struct freerdp_aad_cloud
{
	const char* name;
	const char* gateway_suffix;
	const char* authority;
	const char* avd_scope;
	const char* avd_redirect_format;
};

static const FreeRDP_AadCloud aad_clouds[] = {
	{ "commercial", nullptr, "login.microsoftonline.com",
	  "https%3A%2F%2Fwww.wvd.microsoft.com%2F.default%20openid%20profile%20offline_access",
	  "https%%3A%%2F%%2F%s%%2F%s%%2Foauth2%%2Fnativeclient" },
	{ "usgov", ".wvd.azure.us", "login.microsoftonline.us",
	  "https%3A%2F%2Fwww.wvd.azure.us%2F.default%20openid%20profile%20offline_access",
	  /* Redirect URIs for this public client are fixed in the Azure registration (see #11032);
	   * only the scope changes per cloud. Verified against a US Government tenant: the
	   * .com/common redirect below works end to end; the sovereign login.microsoftonline.us
	   * form is rejected by Entra with AADSTS50011 (app a85cf173-4192-42f8-81fa-777a763e6e2c). */
	  "https%%3A%%2F%%2Flogin.microsoftonline.com%%2Fcommon%%2Foauth2%%2Fnativeclient" }
};

/** @brief The optional configuration file overriding or extending the compiled cloud table
 *
 * Looked up in the system configuration directory first, then in the user one, so that a
 * distribution or an administrator can add a cloud without a rebuild and a user can add a
 * private one on top of that. Entries are merged by name over the compiled table: an entry
 * naming a compiled cloud replaces it as a whole, any other one is added.
 */
#define AAD_CLOUDS_CONFIG_FILE "aad-clouds.json"

/** The number of '%s' conversions the redirect format is expanded with: the AAD authority and
 * the tenant id, see get_redirect_uri() in client/common/client.c. */
#define AAD_CLOUD_REDIRECT_CONVERSIONS 2

static INIT_ONCE aad_clouds_once = INIT_ONCE_STATIC_INIT;

/** The compiled table with the configured clouds merged over it, or \b nullptr while no
 * configuration file contributed an entry. Every entry of it owns all of its strings. */
static FreeRDP_AadCloud* aad_clouds_configured = nullptr;
static size_t aad_clouds_configured_count = 0;

/** Frees the strings of a configured entry.
 *
 * The fields are const because the lookups hand out immutable descriptions, but a configured
 * entry owns its strings and nothing else points at them. The compiled entries are never
 * passed here, they hold string literals.
 */
static void aad_cloud_entry_free(FreeRDP_AadCloud* cloud)
{
	if (!cloud)
		return;

	free(WINPR_CAST_CONST_PTR_AWAY(cloud->name, char*));
	free(WINPR_CAST_CONST_PTR_AWAY(cloud->gateway_suffix, char*));
	free(WINPR_CAST_CONST_PTR_AWAY(cloud->authority, char*));
	free(WINPR_CAST_CONST_PTR_AWAY(cloud->avd_scope, char*));
	free(WINPR_CAST_CONST_PTR_AWAY(cloud->avd_redirect_format, char*));

	const FreeRDP_AadCloud empty = { nullptr, nullptr, nullptr, nullptr, nullptr };
	*cloud = empty;
}

static void aad_cloud_table_free(FreeRDP_AadCloud* table, size_t count)
{
	for (size_t x = 0; x < count; x++)
		aad_cloud_entry_free(&table[x]);
	free(table);
}

/** Copies a cloud description into an entry owning all of its strings. */
static BOOL aad_cloud_entry_copy(FreeRDP_AadCloud* dst, const FreeRDP_AadCloud* src)
{
	FreeRDP_AadCloud copy = { nullptr, nullptr, nullptr, nullptr, nullptr };

	copy.name = _strdup(src->name);
	copy.authority = _strdup(src->authority);
	copy.avd_scope = _strdup(src->avd_scope);
	copy.avd_redirect_format = _strdup(src->avd_redirect_format);
	if (src->gateway_suffix)
		copy.gateway_suffix = _strdup(src->gateway_suffix);

	if (!copy.name || !copy.authority || !copy.avd_scope || !copy.avd_redirect_format ||
	    (src->gateway_suffix && !copy.gateway_suffix))
	{
		aad_cloud_entry_free(&copy);
		return FALSE;
	}

	*dst = copy;
	return TRUE;
}

/** Fills \b ptable with owned copies of the compiled clouds, unless it already holds them. */
static BOOL aad_cloud_table_materialize(FreeRDP_AadCloud** ptable, size_t* pcount)
{
	if (*ptable)
		return TRUE;

	FreeRDP_AadCloud* table = calloc(ARRAYSIZE(aad_clouds), sizeof(FreeRDP_AadCloud));
	if (!table)
		return FALSE;

	for (size_t x = 0; x < ARRAYSIZE(aad_clouds); x++)
	{
		if (!aad_cloud_entry_copy(&table[x], &aad_clouds[x]))
		{
			aad_cloud_table_free(table, ARRAYSIZE(aad_clouds));
			return FALSE;
		}
	}

	*ptable = table;
	*pcount = ARRAYSIZE(aad_clouds);
	return TRUE;
}

/** Merges one configured cloud into the table, taking ownership of its strings on success:
 * an entry of the same name is replaced as a whole, any other one is appended. */
static BOOL aad_cloud_table_merge(FreeRDP_AadCloud** ptable, size_t* pcount,
                                  const FreeRDP_AadCloud* entry)
{
	if (!aad_cloud_table_materialize(ptable, pcount))
		return FALSE;

	for (size_t x = 0; x < *pcount; x++)
	{
		FreeRDP_AadCloud* cur = &(*ptable)[x];
		if (_stricmp(cur->name, entry->name) == 0)
		{
			aad_cloud_entry_free(cur);
			*cur = *entry;
			return TRUE;
		}
	}

	FreeRDP_AadCloud* table = realloc(*ptable, (*pcount + 1) * sizeof(FreeRDP_AadCloud));
	if (!table)
		return FALSE;

	table[*pcount] = *entry;
	*ptable = table;
	*pcount += 1;
	return TRUE;
}

/** A cloud name is a /gateway:cloud: value, so it stays a lowercase [a-z0-9-] token. */
static BOOL aad_cloud_valid_name(const char* value)
{
	if (!value || (strlen(value) == 0))
		return FALSE;

	for (const char* pos = value; *pos != '\0'; pos++)
	{
		const char cur = *pos;
		if (((cur < 'a') || (cur > 'z')) && ((cur < '0') || (cur > '9')) && (cur != '-'))
			return FALSE;
	}
	return TRUE;
}

/** Host names are expanded into URLs, so only the characters of a DNS name are accepted: a
 * scheme, a port, a path or a query would end up in the middle of the URL built from it. */
static BOOL aad_cloud_valid_host_chars(const char* value)
{
	if (!value || (strlen(value) == 0))
		return FALSE;

	for (const char* pos = value; *pos != '\0'; pos++)
	{
		const char cur = *pos;
		if (((cur < 'a') || (cur > 'z')) && ((cur < 'A') || (cur > 'Z')) &&
		    ((cur < '0') || (cur > '9')) && (cur != '.') && (cur != '-'))
			return FALSE;
	}
	return TRUE;
}

static BOOL aad_cloud_valid_authority(const char* value)
{
	if (!aad_cloud_valid_host_chars(value))
		return FALSE;

	const size_t len = strlen(value);
	return (value[0] != '.') && (value[0] != '-') && (value[len - 1] != '.') &&
	       (value[len - 1] != '-');
}

/** The gateway suffix is matched against the end of a hostname, so it starts at a label
 * boundary: without the leading dot 'xwvd.azure.us' would match '.wvd.azure.us'. */
static BOOL aad_cloud_valid_gateway_suffix(const char* value)
{
	if (!value || (strlen(value) < 2) || (value[0] != '.'))
		return FALSE;

	return aad_cloud_valid_authority(&value[1]);
}

/** The scope and the redirect format are expanded into a URL as percent encoded values, so
 * everything outside of printable ASCII and the characters delimiting a URL is rejected. */
static BOOL aad_cloud_valid_url_value(const char* value)
{
	if (!value || (strlen(value) == 0))
		return FALSE;

	for (const char* pos = value; *pos != '\0'; pos++)
	{
		const char cur = *pos;
		if ((cur <= 0x20) || (cur >= 0x7f) || strchr("&#?\"'<>\\^`{|}", cur))
			return FALSE;
	}
	return TRUE;
}

/** The redirect format is passed to winpr_asprintf() as the format string, so accept literal
 * text, '%%' and at most \ref AAD_CLOUD_REDIRECT_CONVERSIONS '%s'. Every other conversion
 * reads past the end of the argument list, and '%n' writes through what it finds there. */
static BOOL aad_cloud_valid_redirect_format(const char* value)
{
	if (!aad_cloud_valid_url_value(value))
		return FALSE;

	size_t conversions = 0;
	for (const char* pos = strchr(value, '%'); pos; pos = strchr(pos, '%'))
	{
		switch (pos[1])
		{
			case '%':
				break;
			case 's':
				conversions++;
				break;
			default:
				return FALSE;
		}
		pos += 2;
	}

	return conversions <= AAD_CLOUD_REDIRECT_CONVERSIONS;
}

static const char* aad_cloud_json_string(const WINPR_JSON* obj, const char* key)
{
	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, key);
	if (!item)
		return nullptr;
	return WINPR_JSON_GetStringValue(item);
}

/** Parses and validates one array element of a configuration file.
 *
 * @param path The file the element was read from, for the log messages
 * @param index The index of the element in the file, for the log messages
 * @param item The element to parse
 * @param cloud Receives the parsed cloud, owning all of its strings
 * @return \b TRUE if the element describes a valid cloud, \b FALSE if it was rejected
 */
static BOOL aad_cloud_parse_entry(const char* path, size_t index, WINPR_JSON* item,
                                  FreeRDP_AadCloud* cloud)
{
	if (!WINPR_JSON_IsObject(item))
	{
		WLog_WARN(TAG, "[%s] entry %" PRIuz " ignored: not a JSON object", path, index);
		return FALSE;
	}

	const char* name = aad_cloud_json_string(item, "name");
	const char* authority = aad_cloud_json_string(item, "active_directory");
	const char* scope = aad_cloud_json_string(item, "avd_scope");
	const char* redirect = aad_cloud_json_string(item, "avd_redirect_format");
	const char* suffix = aad_cloud_json_string(item, "gateway_suffix");

	if (!aad_cloud_valid_name(name))
	{
		WLog_WARN(TAG,
		          "[%s] entry %" PRIuz
		          " ignored: 'name' is missing or not a [a-z0-9-] string, got '%s'",
		          path, index, name ? name : "");
		return FALSE;
	}
	if (!aad_cloud_valid_authority(authority))
	{
		WLog_WARN(TAG,
		          "[%s] entry %" PRIuz " ('%s') ignored: 'active_directory' is missing or not a "
		          "host name without scheme or path, got '%s'",
		          path, index, name, authority ? authority : "");
		return FALSE;
	}
	if (!aad_cloud_valid_url_value(scope))
	{
		WLog_WARN(TAG,
		          "[%s] entry %" PRIuz
		          " ('%s') ignored: 'avd_scope' is missing or not a percent encoded value",
		          path, index, name);
		return FALSE;
	}
	if (!aad_cloud_valid_redirect_format(redirect))
	{
		WLog_WARN(TAG,
		          "[%s] entry %" PRIuz " ('%s') ignored: 'avd_redirect_format' is missing or not a "
		          "format string of literal text, '%%%%' and at most %d '%%s'",
		          path, index, name, AAD_CLOUD_REDIRECT_CONVERSIONS);
		return FALSE;
	}
	if (WINPR_JSON_HasObjectItem(item, "gateway_suffix") && !aad_cloud_valid_gateway_suffix(suffix))
	{
		WLog_WARN(TAG,
		          "[%s] entry %" PRIuz " ('%s') ignored: 'gateway_suffix' is not a host name "
		          "starting with the '.' of a label boundary, got '%s'",
		          path, index, name, suffix ? suffix : "");
		return FALSE;
	}

	/* Accepted so that a file can carry the ARM endpoint of its cloud the way 'az cloud show'
	 * spells it, but nothing reads it: no setting of libfreerdp holds an ARM endpoint. */
	if (WINPR_JSON_HasObjectItem(item, "resource_manager"))
	{
		WLog_DBG(TAG,
		         "[%s] entry %" PRIuz " ('%s'): 'resource_manager' is accepted and ignored, no "
		         "setting holds an ARM endpoint",
		         path, index, name);
	}

	const FreeRDP_AadCloud parsed = { name, suffix, authority, scope, redirect };
	if (!aad_cloud_entry_copy(cloud, &parsed))
	{
		WLog_ERR(TAG, "[%s] entry %" PRIuz " ('%s') ignored: out of memory", path, index, name);
		return FALSE;
	}
	return TRUE;
}

/** Merges the clouds of one configuration file into the table.
 *
 * A missing file is the normal case and leaves the table alone, and so does a file that does
 * not parse or does not hold an array: the compiled defaults are what a broken file falls back
 * to. A single malformed entry only costs its own cloud, its siblings are still loaded.
 */
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
			continue;

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

static BOOL CALLBACK aad_clouds_init(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                     WINPR_ATTR_UNUSED PVOID param,
                                     WINPR_ATTR_UNUSED PVOID* context)
{
	FreeRDP_AadCloud* table = nullptr;
	size_t count = 0;

	/* The user file is merged last, so that it wins over the system one. */
	aad_cloud_load_file(TRUE, &table, &count);
	aad_cloud_load_file(FALSE, &table, &count);

	aad_clouds_configured = table;
	aad_clouds_configured_count = count;
	return TRUE;
}

/** @return The table the lookups run against: the compiled one, or the configured one while a
 * configuration file contributed an entry. */
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

void freerdp_utils_aad_cloud_table_reset(void)
{
	aad_cloud_table_free(aad_clouds_configured, aad_clouds_configured_count);
	aad_clouds_configured = nullptr;
	aad_clouds_configured_count = 0;

	/* InitOnceInitialize() is a stub wherever WinPR implements the run-once itself, so the
	 * value is assigned from a static initializer instead. */
	const INIT_ONCE once = INIT_ONCE_STATIC_INIT;
	aad_clouds_once = once;
}

#define AAD_CLOUD_GETTER(_name, _field)                                            \
	const char* freerdp_utils_aad_cloud_get_##_name(const FreeRDP_AadCloud* cloud) \
	{                                                                              \
		return cloud ? cloud->_field : nullptr;                                    \
	}

AAD_CLOUD_GETTER(name, name)
AAD_CLOUD_GETTER(gateway_suffix, gateway_suffix)
AAD_CLOUD_GETTER(authority, authority)
AAD_CLOUD_GETTER(avd_scope, avd_scope)
AAD_CLOUD_GETTER(avd_redirect_format, avd_redirect_format)

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
	const size_t hostlen = strlen(hostname);
	for (size_t x = 0; x < count; x++)
	{
		const char* suffix = clouds[x].gateway_suffix;
		if (!suffix)
			continue;

		const size_t suffixlen = strlen(suffix);
		if ((hostlen >= suffixlen) && (_stricmp(&hostname[hostlen - suffixlen], suffix) == 0))
			return &clouds[x];
	}
	return nullptr;
}

BOOL freerdp_utils_aad_apply_cloud(rdpSettings* settings, const FreeRDP_AadCloud* cloud)
{
	if (!settings || !cloud)
		return FALSE;

	/* Allocate all three replacements before transferring ownership of any of them, so an
	 * allocation failure can not leave a partially applied cloud behind. */
	char* authority = _strdup(cloud->authority);
	char* scope = _strdup(cloud->avd_scope);
	char* redirect = _strdup(cloud->avd_redirect_format);
	if (!authority || !scope || !redirect)
	{
		free(authority);
		free(scope);
		free(redirect);
		return FALSE;
	}

	/* These ownership-transfer setters can not fail for allocated, non-null strings. */
	BOOL rc = freerdp_settings_set_string_(settings, FreeRDP_GatewayAzureActiveDirectory, authority,
	                                       strlen(authority));
	WINPR_ASSERT(rc);
	rc = freerdp_settings_set_string_(settings, FreeRDP_GatewayAvdScope, scope, strlen(scope));
	WINPR_ASSERT(rc);
	rc = freerdp_settings_set_string_(settings, FreeRDP_GatewayAvdAccessAadFormat, redirect,
	                                  strlen(redirect));
	WINPR_ASSERT(rc);
	return TRUE;
}

struct rdp_aad
{
	AAD_STATE state;
	rdpContext* rdpcontext;
	char* access_token;
	rdpPrivateKey* key;
	char* kid;
	char* nonce;
	char* hostname;
	char* scope;
	wLog* log;
};

#ifdef WITH_AAD

static BOOL aad_fetch_wellknown(wLog* log, rdpContext* context);
static BOOL get_encoded_rsa_params(wLog* wlog, rdpPrivateKey* key, char** e, char** n);
static BOOL generate_pop_key(rdpAad* aad);

WINPR_ATTR_FORMAT_ARG(2, 3)
static SSIZE_T stream_sprintf(wStream* s, WINPR_FORMAT_ARG const char* fmt, ...)
{
	va_list ap = WINPR_C_ARRAY_INIT;
	va_start(ap, fmt);
	const int rc = vsnprintf(nullptr, 0, fmt, ap);
	va_end(ap);

	if (rc < 0)
		return rc;

	if (!Stream_EnsureRemainingCapacity(s, (size_t)rc + 1))
		return -1;

	char* ptr = Stream_PointerAs(s, char);
	va_start(ap, fmt);
	const int rc2 = vsnprintf(ptr, WINPR_ASSERTING_INT_CAST(size_t, rc) + 1, fmt, ap);
	va_end(ap);
	if (rc != rc2)
		return -23;
	if (!Stream_SafeSeek(s, (size_t)rc2))
		return -3;
	return rc2;
}

static BOOL json_get_object(wLog* wlog, WINPR_JSON* json, const char* key, WINPR_JSON** obj)
{
	WINPR_ASSERT(json);
	WINPR_ASSERT(key);

	if (!WINPR_JSON_HasObjectItem(json, key))
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] does not contain a key '%s'", key);
		return FALSE;
	}

	WINPR_JSON* prop = WINPR_JSON_GetObjectItemCaseSensitive(json, key);
	if (!prop)
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is nullptr", key);
		return FALSE;
	}
	*obj = prop;
	return TRUE;
}

static BOOL json_get_number(wLog* wlog, WINPR_JSON* json, const char* key, double* result)
{
	BOOL rc = FALSE;
	WINPR_JSON* prop = nullptr;
	if (!json_get_object(wlog, json, key, &prop))
		return FALSE;

	if (!WINPR_JSON_IsNumber(prop))
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NOT a NUMBER", key);
		goto fail;
	}

	*result = WINPR_JSON_GetNumberValue(prop);

	rc = TRUE;
fail:
	return rc;
}

static BOOL json_get_const_string(wLog* wlog, WINPR_JSON* json, const char* key,
                                  const char** result)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(result);

	*result = nullptr;

	WINPR_JSON* prop = nullptr;
	if (!json_get_object(wlog, json, key, &prop))
		return FALSE;

	if (!WINPR_JSON_IsString(prop))
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NOT a STRING", key);
		goto fail;
	}

	{
		const char* str = WINPR_JSON_GetStringValue(prop);
		if (!str)
			WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is nullptr", key);
		*result = str;
		rc = str != nullptr;
	}

fail:
	return rc;
}

static BOOL json_get_string_alloc(wLog* wlog, WINPR_JSON* json, const char* key, char** result)
{
	const char* str = nullptr;
	if (!json_get_const_string(wlog, json, key, &str))
		return FALSE;
	free(*result);
	*result = _strdup(str);
	if (!*result)
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' strdup is nullptr", key);
	return *result != nullptr;
}

static inline const char* aad_auth_result_to_string(DWORD code)
{
#define ERROR_CASE(cd, x)   \
	if ((cd) == (DWORD)(x)) \
		return #x;

	ERROR_CASE(code, S_OK)
	ERROR_CASE(code, SEC_E_INVALID_TOKEN)
	ERROR_CASE(code, E_ACCESSDENIED)
	ERROR_CASE(code, STATUS_LOGON_FAILURE)
	ERROR_CASE(code, STATUS_NO_LOGON_SERVERS)
	ERROR_CASE(code, STATUS_INVALID_LOGON_HOURS)
	ERROR_CASE(code, STATUS_INVALID_WORKSTATION)
	ERROR_CASE(code, STATUS_PASSWORD_EXPIRED)
	ERROR_CASE(code, STATUS_ACCOUNT_DISABLED)
	return "Unknown error";
}

static BOOL ensure_wellknown(rdpContext* context)
{
	if (context->rdp->wellknown)
		return TRUE;

	rdpAad* aad = context->rdp->aad;
	if (!aad)
		return FALSE;

	if (!aad_fetch_wellknown(aad->log, context))
		return FALSE;
	return context->rdp->wellknown != nullptr;
}

static BOOL aad_get_nonce(rdpAad* aad)
{
	BOOL ret = FALSE;
	BYTE* response = nullptr;
	long resp_code = 0;
	size_t response_length = 0;
	WINPR_JSON* json = nullptr;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(aad->rdpcontext);

	rdpRdp* rdp = aad->rdpcontext->rdp;
	WINPR_ASSERT(rdp);

	if (!ensure_wellknown(aad->rdpcontext))
		return FALSE;

	WINPR_JSON* obj = WINPR_JSON_GetObjectItemCaseSensitive(rdp->wellknown, "token_endpoint");
	if (!obj)
	{
		WLog_Print(aad->log, WLOG_ERROR, "wellknown does not have 'token_endpoint', aborting");
		return FALSE;
	}
	const char* url = WINPR_JSON_GetStringValue(obj);
	if (!url)
	{
		WLog_Print(aad->log, WLOG_ERROR,
		           "wellknown does have 'token_endpoint=nullptr' value, aborting");
		return FALSE;
	}

	if (!freerdp_http_request(url, "grant_type=srv_challenge", &resp_code, &response,
	                          &response_length))
	{
		WLog_Print(aad->log, WLOG_ERROR, "nonce request failed");
		goto fail;
	}

	if (resp_code != HTTP_STATUS_OK)
	{
		WLog_Print(aad->log, WLOG_ERROR,
		           "Server unwilling to provide nonce; returned status code %li", resp_code);
		if (response_length > 0)
			WLog_Print(aad->log, WLOG_ERROR, "[status message] %s", response);
		goto fail;
	}

	json = WINPR_JSON_ParseWithLength((const char*)response, response_length);
	if (!json)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Failed to parse nonce response: %s",
		           WINPR_JSON_GetErrorPtr());
		goto fail;
	}

	if (!json_get_string_alloc(aad->log, json, "Nonce", &aad->nonce))
		goto fail;

	ret = TRUE;

fail:
	free(response);
	WINPR_JSON_Delete(json);
	return ret;
}

int aad_client_begin(rdpAad* aad)
{
	size_t size = 0;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(aad->rdpcontext);

	rdpSettings* settings = aad->rdpcontext->settings;
	WINPR_ASSERT(settings);

	/* Get the host part of the hostname */
	const char* hostname = freerdp_settings_get_string(settings, FreeRDP_AadServerHostname);
	if (!hostname)
		hostname = freerdp_settings_get_server_name(settings);
	if (!hostname)
	{
		WLog_Print(aad->log, WLOG_ERROR, "hostname == nullptr");
		return -1;
	}

	aad->hostname = _strdup(hostname);
	if (!aad->hostname)
	{
		WLog_Print(aad->log, WLOG_ERROR, "_strdup(hostname) == nullptr");
		return -1;
	}

	char* p = strchr(aad->hostname, '.');
	if (p)
		*p = '\0';

	if (winpr_asprintf(&aad->scope, &size,
	                   "ms-device-service%%3A%%2F%%2Ftermsrv.wvd.microsoft.com%%2Fname%%2F%s%%"
	                   "2Fuser_impersonation",
	                   aad->hostname) <= 0)
		return -1;

	if (!generate_pop_key(aad))
		return -1;

	/* Obtain an oauth authorization code */
	pGetCommonAccessToken GetCommonAccessToken = freerdp_get_common_access_token(aad->rdpcontext);
	if (!GetCommonAccessToken)
	{
		WLog_Print(aad->log, WLOG_ERROR, "GetCommonAccessToken == nullptr");
		return -1;
	}

	if (!aad_fetch_wellknown(aad->log, aad->rdpcontext))
		return -1;

	const BOOL arc = GetCommonAccessToken(aad->rdpcontext, ACCESS_TOKEN_TYPE_AAD,
	                                      &aad->access_token, 2, aad->scope, aad->kid);
	if (!arc)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Unable to obtain access token");
		return -1;
	}

	/* Send the nonce request message */
	if (!aad_get_nonce(aad))
	{
		WLog_Print(aad->log, WLOG_ERROR, "Unable to obtain nonce");
		return -1;
	}

	return 1;
}

static char* aad_create_jws_header(rdpAad* aad)
{
	WINPR_ASSERT(aad);

	/* Construct the base64url encoded JWS header */
	char* buffer = nullptr;
	size_t bufferlen = 0;
	const int length =
	    winpr_asprintf(&buffer, &bufferlen, "{\"alg\":\"RS256\",\"kid\":\"%s\"}", aad->kid);
	if (length < 0)
		return nullptr;

	char* jws_header = crypto_base64url_encode((const BYTE*)buffer, bufferlen);
	free(buffer);
	return jws_header;
}

static char* aad_create_jws_payload(rdpAad* aad, const char* ts_nonce)
{
	const time_t ts = time(nullptr);

	WINPR_ASSERT(aad);

	char* e = nullptr;
	char* n = nullptr;
	if (!get_encoded_rsa_params(aad->log, aad->key, &e, &n))
		return nullptr;

	/* Construct the base64url encoded JWS payload */
	char* buffer = nullptr;
	size_t bufferlen = 0;
	const int length =
	    winpr_asprintf(&buffer, &bufferlen,
	                   "{"
	                   "\"ts\":\"%li\","
	                   "\"at\":\"%s\","
	                   "\"u\":\"ms-device-service://termsrv.wvd.microsoft.com/name/%s\","
	                   "\"nonce\":\"%s\","
	                   "\"cnf\":{\"jwk\":{\"kty\":\"RSA\",\"e\":\"%s\",\"n\":\"%s\"}},"
	                   "\"client_claims\":\"{\\\"aad_nonce\\\":\\\"%s\\\"}\""
	                   "}",
	                   ts, aad->access_token, aad->hostname, ts_nonce, e, n, aad->nonce);
	free(e);
	free(n);

	if (length < 0)
		return nullptr;

	char* jws_payload = crypto_base64url_encode((BYTE*)buffer, bufferlen);
	free(buffer);
	return jws_payload;
}

static BOOL aad_update_digest(rdpAad* aad, WINPR_DIGEST_CTX* ctx, const char* what)
{
	WINPR_ASSERT(aad);
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(what);

	const BOOL dsu1 = winpr_DigestSign_Update(ctx, what, strlen(what));
	if (!dsu1)
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_DigestSign_Update [%s] failed", what);
		return FALSE;
	}
	return TRUE;
}

static char* aad_final_digest(rdpAad* aad, WINPR_DIGEST_CTX* ctx)
{
	char* jws_signature = nullptr;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(ctx);

	size_t siglen = 0;
	const int dsf = winpr_DigestSign_Final(ctx, nullptr, &siglen);
	if (dsf <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_DigestSign_Final failed with %d", dsf);
		return nullptr;
	}

	char* buffer = calloc(siglen + 1, sizeof(char));
	if (!buffer)
	{
		WLog_Print(aad->log, WLOG_ERROR, "calloc %" PRIuz " bytes failed", siglen + 1);
		goto fail;
	}

	{
		size_t fsiglen = siglen;
		const int dsf2 = winpr_DigestSign_Final(ctx, (BYTE*)buffer, &fsiglen);
		if (dsf2 <= 0)
		{
			WLog_Print(aad->log, WLOG_ERROR, "winpr_DigestSign_Final failed with %d", dsf2);
			goto fail;
		}

		if (siglen != fsiglen)
		{
			WLog_Print(aad->log, WLOG_ERROR,
			           "winpr_DigestSignFinal returned different sizes, first %" PRIuz
			           " then %" PRIuz,
			           siglen, fsiglen);
			goto fail;
		}
		jws_signature = crypto_base64url_encode((const BYTE*)buffer, fsiglen);
	}

fail:
	free(buffer);
	return jws_signature;
}

static char* aad_create_jws_signature(rdpAad* aad, const char* jws_header, const char* jws_payload)
{
	char* jws_signature = nullptr;

	WINPR_ASSERT(aad);

	WINPR_DIGEST_CTX* md_ctx = freerdp_key_digest_sign(aad->key, WINPR_MD_SHA256);
	if (!md_ctx)
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_New failed");
		goto fail;
	}

	if (!aad_update_digest(aad, md_ctx, jws_header))
		goto fail;
	if (!aad_update_digest(aad, md_ctx, "."))
		goto fail;
	if (!aad_update_digest(aad, md_ctx, jws_payload))
		goto fail;

	jws_signature = aad_final_digest(aad, md_ctx);
fail:
	winpr_Digest_Free(md_ctx);
	return jws_signature;
}

static int aad_send_auth_request(rdpAad* aad, const char* ts_nonce)
{
	int ret = -1;
	char* jws_header = nullptr;
	char* jws_payload = nullptr;
	char* jws_signature = nullptr;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(ts_nonce);

	wStream* s = Stream_New(nullptr, 1024);
	if (!s)
		goto fail;

	/* Construct the base64url encoded JWS header */
	jws_header = aad_create_jws_header(aad);
	if (!jws_header)
		goto fail;

	/* Construct the base64url encoded JWS payload */
	jws_payload = aad_create_jws_payload(aad, ts_nonce);
	if (!jws_payload)
		goto fail;

	/* Sign the JWS with the pop key */
	jws_signature = aad_create_jws_signature(aad, jws_header, jws_payload);
	if (!jws_signature)
		goto fail;

	/* Construct the Authentication Request PDU with the JWS as the RDP Assertion */
	if (stream_sprintf(s, "{\"rdp_assertion\":\"%s.%s.%s\"}", jws_header, jws_payload,
	                   jws_signature) < 0)
		goto fail;

	/* Include null terminator in PDU */
	Stream_Write_UINT8(s, 0);

	Stream_SealLength(s);

	{
		rdpTransport* transport = freerdp_get_transport(aad->rdpcontext);
		if (transport_write(transport, s) < 0)
		{
			WLog_Print(aad->log, WLOG_ERROR, "transport_write [%" PRIuz " bytes] failed",
			           Stream_Length(s));
		}
		else
		{
			ret = 1;
			aad->state = AAD_STATE_AUTH;
		}
	}

fail:
	Stream_Free(s, TRUE);
	free(jws_header);
	free(jws_payload);
	free(jws_signature);

	return ret;
}

static int aad_parse_state_initial(rdpAad* aad, wStream* s)
{
	const char* jstr = Stream_PointerAs(s, char);
	const size_t jlen = Stream_GetRemainingLength(s);
	const char* ts_nonce = nullptr;
	int ret = -1;
	WINPR_JSON* json = nullptr;

	if (!Stream_SafeSeek(s, jlen))
		goto fail;

	json = WINPR_JSON_ParseWithLength(jstr, jlen);
	if (!json)
	{
		WLog_Print(aad->log, WLOG_ERROR, "WINPR_JSON_ParseWithLength failed: %s",
		           WINPR_JSON_GetErrorPtr());
		goto fail;
	}

	if (!json_get_const_string(aad->log, json, "ts_nonce", &ts_nonce))
		goto fail;

	ret = aad_send_auth_request(aad, ts_nonce);
fail:
	WINPR_JSON_Delete(json);
	return ret;
}

static int aad_parse_state_auth(rdpAad* aad, wStream* s)
{
	int rc = -1;
	double result = 0;
	DWORD error_code = 0;
	WINPR_JSON* json = nullptr;
	const char* jstr = Stream_PointerAs(s, char);
	const size_t jlength = Stream_GetRemainingLength(s);

	if (!Stream_SafeSeek(s, jlength))
		goto fail;

	json = WINPR_JSON_ParseWithLength(jstr, jlength);
	if (!json)
	{
		WLog_Print(aad->log, WLOG_ERROR, "WINPR_JSON_ParseWithLength: %s",
		           WINPR_JSON_GetErrorPtr());
		goto fail;
	}

	if (!json_get_number(aad->log, json, "authentication_result", &result))
		goto fail;
	error_code = (DWORD)result;

	if (error_code != S_OK)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Authentication result: %s (0x%08" PRIx32 ")",
		           aad_auth_result_to_string(error_code), error_code);
		goto fail;
	}
	aad->state = AAD_STATE_FINAL;
	rc = 1;
fail:
	WINPR_JSON_Delete(json);
	return rc;
}

int aad_recv(rdpAad* aad, wStream* s)
{
	WINPR_ASSERT(aad);
	WINPR_ASSERT(s);

	switch (aad->state)
	{
		case AAD_STATE_INITIAL:
			return aad_parse_state_initial(aad, s);
		case AAD_STATE_AUTH:
			return aad_parse_state_auth(aad, s);
		case AAD_STATE_FINAL:
		default:
			WLog_Print(aad->log, WLOG_ERROR, "Invalid AAD_STATE %u", aad->state);
			return -1;
	}
}

static BOOL generate_rsa_2048(rdpAad* aad)
{
	WINPR_ASSERT(aad);
	return freerdp_key_generate(aad->key, "RSA", 1, 2048);
}

static char* generate_rsa_digest_base64_str(rdpAad* aad, const char* input, size_t ilen)
{
	char* b64 = nullptr;
	WINPR_DIGEST_CTX* digest = winpr_Digest_New();
	if (!digest)
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_New failed");
		goto fail;
	}

	if (!winpr_Digest_Init(digest, WINPR_MD_SHA256))
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_Init(WINPR_MD_SHA256) failed");
		goto fail;
	}

	if (!winpr_Digest_Update(digest, (const BYTE*)input, ilen))
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_Update(%" PRIuz ") failed", ilen);
		goto fail;
	}

	{
		BYTE hash[WINPR_SHA256_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
		if (!winpr_Digest_Final(digest, hash, sizeof(hash)))
		{
			WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_Final(%" PRIuz ") failed", sizeof(hash));
			goto fail;
		}

		/* Base64url encode the hash */
		b64 = crypto_base64url_encode(hash, sizeof(hash));
	}

fail:
	winpr_Digest_Free(digest);
	return b64;
}

static BOOL generate_json_base64_str(rdpAad* aad, const char* b64_hash)
{
	WINPR_ASSERT(aad);

	char* buffer = nullptr;
	size_t blen = 0;
	const int length = winpr_asprintf(&buffer, &blen, "{\"kid\":\"%s\"}", b64_hash);
	if (length < 0)
		return FALSE;

	/* Finally, base64url encode the JSON text to form the kid */
	free(aad->kid);
	aad->kid = crypto_base64url_encode((const BYTE*)buffer, (size_t)length);
	free(buffer);

	return aad->kid != nullptr;
}

BOOL generate_pop_key(rdpAad* aad)
{
	BOOL ret = FALSE;
	char* buffer = nullptr;
	char* b64_hash = nullptr;
	char* e = nullptr;
	char* n = nullptr;

	WINPR_ASSERT(aad);

	/* Generate a 2048-bit RSA key pair */
	if (!generate_rsa_2048(aad))
		goto fail;

	/* Encode the public key as a JWK */
	if (!get_encoded_rsa_params(aad->log, aad->key, &e, &n))
		goto fail;

	{
		size_t blen = 0;
		const int alen =
		    winpr_asprintf(&buffer, &blen, "{\"e\":\"%s\",\"kty\":\"RSA\",\"n\":\"%s\"}", e, n);
		if (alen < 0)
			goto fail;

		/* Hash the encoded public key */
		b64_hash = generate_rsa_digest_base64_str(aad, buffer, blen);
		if (!b64_hash)
			goto fail;
	}

	/* Encode a JSON object with a single property "kid" whose value is the encoded hash */
	ret = generate_json_base64_str(aad, b64_hash);

fail:
	free(b64_hash);
	free(buffer);
	free(e);
	free(n);
	return ret;
}

static char* bn_to_base64_url(wLog* wlog, rdpPrivateKey* key, enum FREERDP_KEY_PARAM param)
{
	WINPR_ASSERT(wlog);
	WINPR_ASSERT(key);

	size_t len = 0;
	BYTE* bn = freerdp_key_get_param(key, param, &len);
	if (!bn)
		return nullptr;

	char* b64 = crypto_base64url_encode(bn, len);
	free(bn);

	if (!b64)
		WLog_Print(wlog, WLOG_ERROR, "failed  base64 url encode BIGNUM");

	return b64;
}

BOOL get_encoded_rsa_params(wLog* wlog, rdpPrivateKey* key, char** pe, char** pn)
{
	BOOL rc = FALSE;
	char* e = nullptr;
	char* n = nullptr;

	WINPR_ASSERT(wlog);
	WINPR_ASSERT(key);
	WINPR_ASSERT(pe);
	WINPR_ASSERT(pn);

	*pe = nullptr;
	*pn = nullptr;

	e = bn_to_base64_url(wlog, key, FREERDP_KEY_PARAM_RSA_E);
	if (!e)
	{
		WLog_Print(wlog, WLOG_ERROR, "failed  base64 url encode RSA E");
		goto fail;
	}

	n = bn_to_base64_url(wlog, key, FREERDP_KEY_PARAM_RSA_N);
	if (!n)
	{
		WLog_Print(wlog, WLOG_ERROR, "failed  base64 url encode RSA N");
		goto fail;
	}

	rc = TRUE;
fail:
	if (!rc)
	{
		free(e);
		free(n);
	}
	else
	{
		*pe = e;
		*pn = n;
	}
	return rc;
}
#else
int aad_client_begin(rdpAad* aad)
{
	WINPR_ASSERT(aad);
	WLog_Print(aad->log, WLOG_ERROR, "AAD security not compiled in, aborting!");
	return -1;
}

int aad_recv(rdpAad* aad, wStream* s)
{
	WINPR_ASSERT(aad);
	WLog_Print(aad->log, WLOG_ERROR, "AAD security not compiled in, aborting!");
	return -1;
}

static BOOL ensure_wellknown(WINPR_ATTR_UNUSED rdpContext* context)
{
	return FALSE;
}

#endif

rdpAad* aad_new(rdpContext* context)
{
	WINPR_ASSERT(context);

	rdpAad* aad = (rdpAad*)calloc(1, sizeof(rdpAad));

	if (!aad)
		return nullptr;

	aad->log = WLog_Get(FREERDP_TAG("aad"));
	aad->key = freerdp_key_new();
	if (!aad->key)
		goto fail;
	aad->rdpcontext = context;

	return aad;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	aad_free(aad);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}

void aad_free(rdpAad* aad)
{
	if (!aad)
		return;

	free(aad->hostname);
	free(aad->scope);
	free(aad->nonce);
	free(aad->access_token);
	free(aad->kid);
	freerdp_key_free(aad->key);

	free(aad);
}

AAD_STATE aad_get_state(rdpAad* aad)
{
	WINPR_ASSERT(aad);
	return aad->state;
}

BOOL aad_is_supported(void)
{
#ifdef WITH_AAD
	return TRUE;
#else
	return FALSE;
#endif
}

char* freerdp_utils_aad_get_access_token(wLog* log, const char* data, size_t length)
{
	char* token = nullptr;
	WINPR_JSON* access_token_prop = nullptr;
	const char* access_token_str = nullptr;

	WINPR_JSON* json = WINPR_JSON_ParseWithLength(data, length);
	if (!json)
	{
		WLog_Print(log, WLOG_ERROR,
		           "Failed to parse access token response [got %" PRIuz " bytes: %s", length,
		           WINPR_JSON_GetErrorPtr());
		goto cleanup;
	}

	access_token_prop = WINPR_JSON_GetObjectItemCaseSensitive(json, "access_token");
	if (!access_token_prop)
	{
		WLog_Print(log, WLOG_ERROR, "Response has no \"access_token\" property");
		goto cleanup;
	}

	access_token_str = WINPR_JSON_GetStringValue(access_token_prop);
	if (!access_token_str)
	{
		WLog_Print(log, WLOG_ERROR, "Invalid value for \"access_token\"");
		goto cleanup;
	}

	token = _strdup(access_token_str);

cleanup:
	WINPR_JSON_Delete(json);
	return token;
}

BOOL aad_fetch_wellknown(wLog* log, rdpContext* context)
{
	WINPR_ASSERT(context);

	rdpRdp* rdp = context->rdp;
	WINPR_ASSERT(rdp);

	if (rdp->wellknown)
		return TRUE;

	const char* base =
	    freerdp_settings_get_string(context->settings, FreeRDP_GatewayAzureActiveDirectory);
	const BOOL useTenant =
	    freerdp_settings_get_bool(context->settings, FreeRDP_GatewayAvdUseTenantid);
	const char* tenantid = "common";
	if (useTenant)
		tenantid = freerdp_settings_get_string(context->settings, FreeRDP_GatewayAvdAadtenantid);
	rdp->wellknown = freerdp_utils_aad_get_wellknown(log, base, tenantid);
	return rdp->wellknown != nullptr;
}

const char* freerdp_utils_aad_get_wellknown_string(rdpContext* context, AAD_WELLKNOWN_VALUES which)
{
	return freerdp_utils_aad_get_wellknown_custom_string(
	    context, freerdp_utils_aad_wellknwon_value_name(which));
}

const char* freerdp_utils_aad_get_wellknown_custom_string(rdpContext* context, const char* which)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);

	if (!ensure_wellknown(context))
		return nullptr;

	WINPR_JSON* obj = WINPR_JSON_GetObjectItemCaseSensitive(context->rdp->wellknown, which);
	if (!obj)
		return nullptr;

	return WINPR_JSON_GetStringValue(obj);
}

const char* freerdp_utils_aad_wellknwon_value_name(AAD_WELLKNOWN_VALUES which)
{
	switch (which)
	{
		case AAD_WELLKNOWN_token_endpoint:
			return "token_endpoint";
		case AAD_WELLKNOWN_token_endpoint_auth_methods_supported:
			return "token_endpoint_auth_methods_supported";
		case AAD_WELLKNOWN_jwks_uri:
			return "jwks_uri";
		case AAD_WELLKNOWN_response_modes_supported:
			return "response_modes_supported";
		case AAD_WELLKNOWN_subject_types_supported:
			return "subject_types_supported";
		case AAD_WELLKNOWN_id_token_signing_alg_values_supported:
			return "id_token_signing_alg_values_supported";
		case AAD_WELLKNOWN_response_types_supported:
			return "response_types_supported";
		case AAD_WELLKNOWN_scopes_supported:
			return "scopes_supported";
		case AAD_WELLKNOWN_issuer:
			return "issuer";
		case AAD_WELLKNOWN_request_uri_parameter_supported:
			return "request_uri_parameter_supported";
		case AAD_WELLKNOWN_userinfo_endpoint:
			return "userinfo_endpoint";
		case AAD_WELLKNOWN_authorization_endpoint:
			return "authorization_endpoint";
		case AAD_WELLKNOWN_device_authorization_endpoint:
			return "device_authorization_endpoint";
		case AAD_WELLKNOWN_http_logout_supported:
			return "http_logout_supported";
		case AAD_WELLKNOWN_frontchannel_logout_supported:
			return "frontchannel_logout_supported";
		case AAD_WELLKNOWN_end_session_endpoint:
			return "end_session_endpoint";
		case AAD_WELLKNOWN_claims_supported:
			return "claims_supported";
		case AAD_WELLKNOWN_kerberos_endpoint:
			return "kerberos_endpoint";
		case AAD_WELLKNOWN_tenant_region_scope:
			return "tenant_region_scope";
		case AAD_WELLKNOWN_cloud_instance_name:
			return "cloud_instance_name";
		case AAD_WELLKNOWN_cloud_graph_host_name:
			return "cloud_graph_host_name";
		case AAD_WELLKNOWN_msgraph_host:
			return "msgraph_host";
		case AAD_WELLKNOWN_rbac_url:
			return "rbac_url";
		default:
			return "UNKNOWN";
	}
}

WINPR_JSON* freerdp_utils_aad_get_wellknown_object(rdpContext* context, AAD_WELLKNOWN_VALUES which)
{
	return freerdp_utils_aad_get_wellknown_custom_object(
	    context, freerdp_utils_aad_wellknwon_value_name(which));
}

WINPR_JSON* freerdp_utils_aad_get_wellknown_custom_object(rdpContext* context, const char* which)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);

	if (!ensure_wellknown(context))
		return nullptr;

	return WINPR_JSON_GetObjectItemCaseSensitive(context->rdp->wellknown, which);
}

WINPR_ATTR_MALLOC(WINPR_JSON_Delete, 1)
WINPR_ATTR_NODISCARD
WINPR_JSON* freerdp_utils_aad_get_wellknown(wLog* log, const char* base, const char* tenantid)
{
	WINPR_ASSERT(base);
	WINPR_ASSERT(tenantid);

	char* str = nullptr;
	size_t len = 0;
	winpr_asprintf(&str, &len, "https://%s/%s/v2.0/.well-known/openid-configuration", base,
	               tenantid);

	if (!str)
	{
		WLog_Print(log, WLOG_ERROR, "failed to create request URL for tenantid='%s'", tenantid);
		return nullptr;
	}

	BYTE* response = nullptr;
	long resp_code = 0;
	size_t response_length = 0;
	const BOOL rc = freerdp_http_request(str, nullptr, &resp_code, &response, &response_length);
	if (!rc || (resp_code != HTTP_STATUS_OK))
	{
		WLog_Print(log, WLOG_ERROR, "request for '%s' failed with: %s", str,
		           freerdp_http_status_string(resp_code));
		free(str);
		free(response);
		return nullptr;
	}
	free(str);

	WINPR_JSON* json = WINPR_JSON_ParseWithLength((const char*)response, response_length);
	free(response);

	if (!json)
		WLog_Print(log, WLOG_ERROR, "failed to parse response as JSON: %s",
		           WINPR_JSON_GetErrorPtr());

	return json;
}

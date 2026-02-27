/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * common helper utilities
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

#include <ctype.h>

#include <freerdp/utils/helpers.h>

#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/build-config.h>
#include <freerdp/version.h>
#include <freerdp/build-config.h>

#include "../core/utils.h"

static INIT_ONCE s_freerdp_app_details_once = INIT_ONCE_STATIC_INIT;
static char s_freerdp_vendor_string[MAX_PATH] = WINPR_C_ARRAY_INIT;
static char s_freerdp_product_string[MAX_PATH] = WINPR_C_ARRAY_INIT;
static char s_freerdp_details_string[3ull * MAX_PATH] = WINPR_C_ARRAY_INIT;
static WCHAR s_freerdp_details_string_w[3ull * MAX_PATH] = WINPR_C_ARRAY_INIT;
static SSIZE_T s_freerdp_version = -1;
static BOOL s_freerdp_app_details_are_custom = FALSE;

static void updateDetailsString(void)
{
	const char* vendor = s_freerdp_vendor_string;
	const char* product = s_freerdp_product_string;
	const SSIZE_T version = s_freerdp_version;

	WINPR_ASSERT(vendor);
	WINPR_ASSERT(product);
	if (s_freerdp_app_details_are_custom)
	{
		if (version < 0)
			(void)_snprintf(s_freerdp_details_string, sizeof(s_freerdp_details_string) - 1, "%s-%s",
			                vendor, product);
		else
			(void)_snprintf(s_freerdp_details_string, sizeof(s_freerdp_details_string) - 1,
			                "%s-%s%" PRIdz, vendor, product, version);
	}
	else if (version < 0)
	{
		(void)_snprintf(s_freerdp_details_string, sizeof(s_freerdp_details_string) - 1, "%s",
		                product);
	}
	else
		(void)_snprintf(s_freerdp_details_string, sizeof(s_freerdp_details_string) - 1, "%s%" PRIdz,
		                product, version);

	(void)ConvertUtf8NToWChar(s_freerdp_details_string, sizeof(s_freerdp_details_string),
	                          s_freerdp_details_string_w, sizeof(s_freerdp_details_string_w) - 1);
}

static BOOL CALLBACK init_app_details(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                      WINPR_ATTR_UNUSED PVOID param,
                                      WINPR_ATTR_UNUSED PVOID* context)
{
	const size_t vlen = sizeof(FREERDP_VENDOR_STRING);
	const size_t plen = sizeof(FREERDP_PRODUCT_STRING);
	const char* rvlen = strncpy(s_freerdp_vendor_string, FREERDP_VENDOR_STRING, vlen);
	const char* rplen = strncpy(s_freerdp_product_string, FREERDP_PRODUCT_STRING, plen);
	if (!rvlen || !rplen)
		return FALSE;

#if defined(WITH_RESOURCE_VERSIONING)
	s_freerdp_version = FREERDP_VERSION_MAJOR;
#else
	s_freerdp_version = -1;
#endif
	updateDetailsString();
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL initializeApplicationDetails(void)
{
	return InitOnceExecuteOnce(&s_freerdp_app_details_once, init_app_details, nullptr, nullptr);
}

BOOL freerdp_setApplicationDetails(const char* vendor, const char* product, SSIZE_T version)
{
	if (!initializeApplicationDetails())
		return -1;

	if (!vendor || !product)
		return FALSE;
	const size_t vlen = strnlen(vendor, MAX_PATH);
	const size_t plen = strnlen(product, MAX_PATH);
	if ((vlen == MAX_PATH) || (plen == MAX_PATH))
		return FALSE;

	if (!strncpy(s_freerdp_vendor_string, vendor, vlen + 1))
		return FALSE;

	if (!strncpy(s_freerdp_product_string, product, plen + 1))
		return FALSE;

	s_freerdp_version = version;
	s_freerdp_app_details_are_custom = TRUE;

	const char separator = PathGetSeparatorA(PATH_STYLE_NATIVE);
	char* str = freerdp_getApplicatonDetailsCombined(separator);
	if (!str)
		return FALSE;

	const BOOL rc = winpr_setApplicationDetails(str, "WinPR", -1);
	free(str);
	updateDetailsString();
	return rc;
}

const char* freerdp_getApplicationDetailsVendor(void)
{
	if (!initializeApplicationDetails())
		return nullptr;
	return s_freerdp_vendor_string;
}

const char* freerdp_getApplicationDetailsProduct(void)
{
	if (!initializeApplicationDetails())
		return nullptr;
	return s_freerdp_product_string;
}

char* freerdp_getApplicatonDetailsRegKey(const char* fmt)
{
	char* val = freerdp_getApplicatonDetailsCombined('\\');
	if (!val)
		return nullptr;

	char* str = nullptr;
	size_t slen = 0;
	(void)winpr_asprintf(&str, &slen, fmt, val);
	free(val);
	return str;
}

char* freerdp_getApplicatonDetailsCombined(char separator)
{
	const SSIZE_T version = freerdp_getApplicationDetailsVersion();
	const char* vendor = freerdp_getApplicationDetailsVendor();
	const char* product = freerdp_getApplicationDetailsProduct();

	size_t slen = 0;
	char* str = nullptr;
	if (version < 0)
	{
		(void)winpr_asprintf(&str, &slen, "%s%c%s", vendor, separator, product);
	}
	else
	{
		(void)winpr_asprintf(&str, &slen, "%s%c%s%" PRIdz, vendor, separator, product, version);
	}

	return str;
}

SSIZE_T freerdp_getApplicationDetailsVersion(void)
{
	if (!initializeApplicationDetails())
		return -1;
	return s_freerdp_version;
}

const char* freerdp_getApplicationDetailsString(void)
{
	return s_freerdp_details_string;
}

const WCHAR* freerdp_getApplicationDetailsStringW(void)
{
	return s_freerdp_details_string_w;
}

BOOL freerdp_areApplicationDetailsCustomized(void)
{
	return s_freerdp_app_details_are_custom;
}

#if !defined(WITH_FULL_CONFIG_PATH)
WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
static char* freerdp_settings_get_legacy_config_path(const char* filename)
{
	char product[sizeof(FREERDP_PRODUCT_STRING)] = WINPR_C_ARRAY_INIT;

	for (size_t i = 0; i < sizeof(product); i++)
		product[i] = (char)tolower(FREERDP_PRODUCT_STRING[i]);

	char* path = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, product);

	if (!path)
		return nullptr;

	char* filepath = GetCombinedPath(path, filename);
	free(path);
	return filepath;
}
#endif

WINPR_ATTR_NODISCARD
WINPR_ATTR_MALLOC(free, 1) static char* getCustomConfigPath(BOOL system, const char* filename)
{
	eKnownPathTypes id = system ? KNOWN_PATH_SYSTEM_CONFIG_HOME : KNOWN_PATH_XDG_CONFIG_HOME;

	const char* vendor = freerdp_getApplicationDetailsVendor();
	const char* product = freerdp_getApplicationDetailsProduct();
	const SSIZE_T version = freerdp_getApplicationDetailsVersion();

	if (!vendor || !product)
		return nullptr;

	char* config = GetKnownSubPathV(id, "%s", vendor);
	if (!config)
		return nullptr;

	char* base = nullptr;
	if (version < 0)
		base = GetCombinedPathV(config, "%s", product);
	else
		base = GetCombinedPathV(config, "%s%" PRIdz, product, version);
	free(config);

	if (!base)
		return nullptr;

	if (!filename)
		return base;

	char* path = GetCombinedPathV(base, "%s", filename);
	free(base);
	return path;
}

char* freerdp_GetConfigFilePath(BOOL system, const char* filename)
{
#if defined(FREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR)
	const BOOL customized = TRUE;
#else
	const BOOL customized = freerdp_areApplicationDetailsCustomized();
#endif
	if (customized)
		return getCustomConfigPath(system, filename);

	eKnownPathTypes id = system ? KNOWN_PATH_SYSTEM_CONFIG_HOME : KNOWN_PATH_XDG_CONFIG_HOME;

	const char* vendor = freerdp_getApplicationDetailsVendor();
	const char* product = freerdp_getApplicationDetailsProduct();
	const SSIZE_T version = freerdp_getApplicationDetailsVersion();

	if (!vendor || !product)
		return nullptr;

#if !defined(WITH_FULL_CONFIG_PATH)
	if (!system && (_stricmp(vendor, product) == 0))
		return freerdp_settings_get_legacy_config_path(filename);
#endif

	char* config = GetKnownPath(id);
	if (!config)
		return nullptr;

	char* base = nullptr;
	if (version < 0)
		base = GetCombinedPathV(config, "%s", product);
	else
		base = GetCombinedPathV(config, "%s%" PRIdz, product, version);
	free(config);

	if (!base)
		return nullptr;

	if (!filename)
		return base;

	char* path = GetCombinedPathV(base, "%s", filename);
	free(base);
	return path;
}

WINPR_JSON* freerdp_GetJSONConfigFile(BOOL system, const char* filename)
{
	char* path = freerdp_GetConfigFilePath(system, filename);
	if (!path)
		return nullptr;

	WINPR_JSON* json = WINPR_JSON_ParseFromFile(path);
	free(path);
	return json;
}

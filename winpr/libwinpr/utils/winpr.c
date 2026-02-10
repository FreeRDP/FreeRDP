/**
 * WinPR: Windows Portable Runtime
 * Debugging Utils
 *
 * Copyright 2015 Armin Novak <armin.novak@thincast.com>
 * Copyright 2015 Thincast Technologies GmbH
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

#include <winpr/config.h>

#include <winpr/buildflags.h>

#include <stdlib.h>
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/winpr.h>
#include <winpr/version.h>
#include <winpr/wlog.h>
#include <winpr/file.h>
#include <winpr/build-config.h>

#include "../utils.h"

#if !defined(WIN32)
#include <pthread.h>
#endif

static INIT_ONCE s_winpr_app_details_once = INIT_ONCE_STATIC_INIT;
static char s_winpr_vendor_string[MAX_PATH] = { 0 };
static char s_winpr_product_string[MAX_PATH] = { 0 };
static SSIZE_T s_winpr_version = -1;
static BOOL s_winpr_app_details_are_custom = FALSE;

static BOOL CALLBACK init_app_details(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                      WINPR_ATTR_UNUSED PVOID param,
                                      WINPR_ATTR_UNUSED PVOID* context)
{
	const size_t vlen = sizeof(WINPR_VENDOR_STRING);
	const size_t plen = sizeof(WINPR_PRODUCT_STRING);
	if (!strncpy(s_winpr_vendor_string, WINPR_VENDOR_STRING, vlen))
		return FALSE;

	if (!strncpy(s_winpr_product_string, WINPR_PRODUCT_STRING, plen))
		return FALSE;

#if defined(WITH_RESOURCE_VERSIONING)
	s_winpr_version = WINPR_VERSION_MAJOR;
#else
	s_winpr_version = -1;
#endif
	return TRUE;
}

static WINPR_ATTR_NODISCARD BOOL initializeApplicationDetails(void)
{
	InitOnceExecuteOnce(&s_winpr_app_details_once, init_app_details, NULL, NULL);
	return TRUE;
}

BOOL winpr_setApplicationDetails(const char* vendor, const char* product, SSIZE_T version)
{
	if (!initializeApplicationDetails())
		return -1;

	if (!vendor || !product)
		return FALSE;
	const size_t vlen = strnlen(vendor, MAX_PATH);
	const size_t plen = strnlen(product, MAX_PATH);
	if ((vlen == MAX_PATH) || (plen == MAX_PATH))
		return FALSE;

	if (!strncpy(s_winpr_vendor_string, vendor, vlen + 1))
		return FALSE;

	if (!strncpy(s_winpr_product_string, product, plen + 1))
		return FALSE;

	s_winpr_version = version;
	s_winpr_app_details_are_custom = TRUE;
	return TRUE;
}

const char* winpr_getApplicationDetailsVendor(void)
{
	if (!initializeApplicationDetails())
		return NULL;
	return s_winpr_vendor_string;
}

const char* winpr_getApplicationDetailsProduct(void)
{
	if (!initializeApplicationDetails())
		return NULL;
	return s_winpr_product_string;
}

char* winpr_getApplicatonDetailsRegKey(const char* fmt)
{
	char* val = winpr_getApplicatonDetailsCombined('\\');
	if (!val)
		return NULL;

	char* str = NULL;
	size_t slen = 0;
	(void)winpr_asprintf(&str, &slen, fmt, val);
	free(val);
	return str;
}

char* winpr_getApplicatonDetailsCombined(char separator)
{
	const SSIZE_T version = winpr_getApplicationDetailsVersion();
	const char* vendor = winpr_getApplicationDetailsVendor();
	const char* product = winpr_getApplicationDetailsProduct();

	size_t slen = 0;
	char* str = NULL;
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

SSIZE_T winpr_getApplicationDetailsVersion(void)
{
	if (!initializeApplicationDetails())
		return -1;
	return s_winpr_version;
}

BOOL winpr_areApplicationDetailsCustomized(void)
{
	return s_winpr_app_details_are_custom;
}

void winpr_get_version(int* major, int* minor, int* revision)
{
	if (major)
		*major = WINPR_VERSION_MAJOR;
	if (minor)
		*minor = WINPR_VERSION_MINOR;
	if (revision)
		*revision = WINPR_VERSION_REVISION;
}

const char* winpr_get_version_string(void)
{
	return WINPR_VERSION_FULL;
}

const char* winpr_get_build_revision(void)
{
	return WINPR_GIT_REVISION;
}

const char* winpr_get_build_config(void)
{
	static const char build_config[] =
	    "Build configuration: " WINPR_BUILD_CONFIG "\n"
	    "Build type:          " WINPR_BUILD_TYPE "\n"
	    "CFLAGS:              " WINPR_CFLAGS "\n"
	    "Compiler:            " WINPR_COMPILER_ID ", " WINPR_COMPILER_VERSION "\n"
	    "Target architecture: " WINPR_TARGET_ARCH "\n";

	return build_config;
}

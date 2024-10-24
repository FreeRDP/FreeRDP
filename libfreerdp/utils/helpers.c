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

#include <freerdp/utils/helpers.h>

#include <winpr/path.h>
#include <freerdp/version.h>
#include <freerdp/build-config.h>

#define STR(x) #x

char* freerdp_GetConfigFilePath(BOOL system, const char* filename)
{
	eKnownPathTypes id = system ? KNOWN_PATH_SYSTEM_CONFIG_HOME : KNOWN_PATH_XDG_CONFIG_HOME;

#if defined(FREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR)
	char* vendor = GetKnownSubPath(id, FREERDP_VENDOR_STRING);
#else
	char* vendor = GetKnownPath(id);
#endif
	if (!vendor)
		return NULL;

#if defined(WITH_RESOURCE_VERSIONING)
	char* verstr = FREERDP_PRODUCT_STRING STR(FREERDP_VERSION_MAJOR);
#else
	char* verstr = FREERDP_PRODUCT_STRING;
#endif

	char* base = GetCombinedPath(vendor, verstr);
	free(vendor);

	if (!base)
		return NULL;

	if (!filename)
		return base;

	char* path = GetCombinedPath(base, filename);
	free(base);
	return path;
}

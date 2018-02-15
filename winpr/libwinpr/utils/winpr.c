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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "buildflags.h"

#include <stdlib.h>
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/winpr.h>
#include <winpr/version.h>
#include <winpr/wlog.h>

#if !defined(WIN32)
#include <pthread.h>
#endif

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

const char* winpr_get_build_date(void)
{
	static char build_date[] = __DATE__ " " __TIME__;

	return build_date;
}

const char* winpr_get_build_revision(void)
{
	return GIT_REVISION;
}

const char* winpr_get_build_config(void)
{
	static const char build_config[] =
		"Build configuration: " BUILD_CONFIG "\n"
		"Build type:          " BUILD_TYPE "\n"
		"CFLAGS:              " CFLAGS "\n"
		"Compiler:            " COMPILER_ID ", " COMPILER_VERSION "\n"
		"Target architecture: " TARGET_ARCH "\n";

	return build_config;
}

int winpr_exit(int status)
{
	WLog_Uninit();
#if defined(WIN32)
	return status;
#else
	pthread_exit(&status);
	return status;
#endif
}

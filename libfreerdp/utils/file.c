/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * File Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/path.h>

#include <freerdp/types.h>
#include <freerdp/settings.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#define getcwd			_getcwd
#endif

#include <winpr/windows.h>
#include <freerdp/utils/file.h>

#ifndef _WIN32
#define PATH_SEPARATOR_STR	"/"
#define PATH_SEPARATOR_CHR	'/'
#else
#define PATH_SEPARATOR_STR	"\\"
#define PATH_SEPARATOR_CHR	'\\'
#endif

#define FREERDP_CONFIG_DIR	".freerdp"

void freerdp_mkdir(char* path)
{
#ifndef _WIN32
		mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR);
#else
		CreateDirectoryA(path, 0);
#endif
}

char* freerdp_get_home_path(rdpSettings* settings)
{
	settings->HomePath = GetKnownPath(KNOWN_PATH_HOME);
	return settings->HomePath;
}

char* freerdp_get_config_path(rdpSettings* settings)
{
	if (settings->ConfigPath != NULL)
		return settings->ConfigPath;

	settings->ConfigPath = (char*) malloc(strlen(settings->HomePath) + sizeof(FREERDP_CONFIG_DIR) + 2);
	sprintf(settings->ConfigPath, "%s" PATH_SEPARATOR_STR "%s", settings->HomePath, FREERDP_CONFIG_DIR);

	if (!PathFileExistsA(settings->ConfigPath))
		freerdp_mkdir(settings->ConfigPath);

	return settings->ConfigPath;
}

char* freerdp_construct_path(char* base_path, char* relative_path)
{
	char* path;
	int length;
	int base_path_length;
	int relative_path_length;

	base_path_length = strlen(base_path);
	relative_path_length = strlen(relative_path);
	length = base_path_length + relative_path_length + 1;

	path = malloc(length + 1);
	sprintf(path, "%s" PATH_SEPARATOR_STR "%s", base_path, relative_path);

	return path;
}

void freerdp_detect_paths(rdpSettings* settings)
{
	freerdp_get_home_path(settings);
	freerdp_get_config_path(settings);
}

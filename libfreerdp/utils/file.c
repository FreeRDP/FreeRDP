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

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/string.h>

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
#define HOME_ENV_VARIABLE	"HOME"
#else
#define PATH_SEPARATOR_STR	"\\"
#define PATH_SEPARATOR_CHR	'\\'
#define HOME_ENV_VARIABLE	"HOME"
#endif

#ifdef _WIN32
#define SHARED_LIB_SUFFIX	".dll"
#elif __APPLE__
#define SHARED_LIB_SUFFIX	".dylib"
#else
#define SHARED_LIB_SUFFIX	".so"
#endif

#define FREERDP_CONFIG_DIR	".freerdp"

#define PARENT_PATH		".." PATH_SEPARATOR_STR

void freerdp_mkdir(char* path)
{
#ifndef _WIN32
		mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR);
#else
		CreateDirectoryA(path, 0);
#endif
}

BOOL freerdp_check_file_exists(char* file)
{
	struct stat stat_info;

	if (stat(file, &stat_info) != 0)
		return FALSE;

	return TRUE;
}

char* freerdp_get_home_path(rdpSettings* settings)
{
	if (settings->HomePath == NULL)
		settings->HomePath = getenv(HOME_ENV_VARIABLE);
	if (settings->HomePath == NULL)
		settings->HomePath = _strdup("/");

	return settings->HomePath;
}

char* freerdp_get_config_path(rdpSettings* settings)
{
	if (settings->ConfigPath != NULL)
		return settings->ConfigPath;

	settings->ConfigPath = (char*) malloc(strlen(settings->HomePath) + sizeof(FREERDP_CONFIG_DIR) + 2);
	sprintf(settings->ConfigPath, "%s" PATH_SEPARATOR_STR "%s", settings->HomePath, FREERDP_CONFIG_DIR);

	if (!freerdp_check_file_exists(settings->ConfigPath))
		freerdp_mkdir(settings->ConfigPath);

	return settings->ConfigPath;
}

char* freerdp_get_current_path(rdpSettings* settings)
{
	if (settings->CurrentPath == NULL)
		settings->CurrentPath = getcwd(NULL, 0);

	return settings->CurrentPath;
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

char* freerdp_append_shared_library_suffix(char* file_path)
{
	char* p;
	char* path = NULL;
	int file_path_length;
	int shared_lib_suffix_length;

	if (file_path == NULL)
		return NULL;

	file_path_length = strlen(file_path);
	shared_lib_suffix_length = strlen(SHARED_LIB_SUFFIX);

	if (file_path_length >= shared_lib_suffix_length)
	{
		p = &file_path[file_path_length - shared_lib_suffix_length];

		if (strcmp(p, SHARED_LIB_SUFFIX) != 0)
		{
			path = malloc(file_path_length + shared_lib_suffix_length + 1);
			sprintf(path, "%s%s", file_path, SHARED_LIB_SUFFIX);
		}
		else
		{
			path = _strdup(file_path);
		}
	}
	else
	{
		path = malloc(file_path_length + shared_lib_suffix_length + 1);
		sprintf(path, "%s%s", file_path, SHARED_LIB_SUFFIX);	
	}

	return path;
}

char* freerdp_get_parent_path(char* base_path, int depth)
{
	int i;
	char* p;
	char* path;
	int length;
	int base_length;

	if (base_path == NULL)
		return NULL;

	if (depth <= 0)
		return _strdup(base_path);

	base_length = strlen(base_path);

	p = &base_path[base_length];

	for (i = base_length - 1; ((i >= 0) && (depth > 0)); i--)
	{
		if (base_path[i] == PATH_SEPARATOR_CHR)
		{
			p = &base_path[i];
			depth--;
		}
	}

	length = (p - base_path);

	path = (char*) malloc(length + 1);
	memcpy(path, base_path, length);
	path[length] = '\0';

	return path;
}

BOOL freerdp_path_contains_separator(char* path)
{
	if (path == NULL)
		return FALSE;

	if (strchr(path, PATH_SEPARATOR_CHR) == NULL)
		return FALSE;

	return TRUE;
}

void freerdp_detect_paths(rdpSettings* settings)
{
	freerdp_get_home_path(settings);
	freerdp_get_config_path(settings);
}

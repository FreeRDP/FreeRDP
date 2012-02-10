/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

#include <freerdp/utils/file.h>

#ifndef _WIN32
#define PATH_SEPARATOR_STR	"/"
#define PATH_SEPARATOR_CHR	'/'
#define HOME_ENV_VARIABLE	"HOME"
#else
#include <windows.h>
#define PATH_SEPARATOR_STR	"\\"
#define PATH_SEPARATOR_CHR	'\\'
#define HOME_ENV_VARIABLE	"HOMEPATH"
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

boolean freerdp_check_file_exists(char* file)
{
	struct stat stat_info;

	if (stat(file, &stat_info) != 0)
		return false;

	return true;
}

char* freerdp_get_home_path(rdpSettings* settings)
{
	if (settings->home_path == NULL)
		settings->home_path = getenv(HOME_ENV_VARIABLE);

	return settings->home_path;
}

char* freerdp_get_config_path(rdpSettings* settings)
{
	char* path;

	path = (char*) xmalloc(strlen(settings->home_path) + sizeof(FREERDP_CONFIG_DIR) + 2);
	sprintf(path, "%s/%s", settings->home_path, FREERDP_CONFIG_DIR);

	if (!freerdp_check_file_exists(path))
		freerdp_mkdir(path);

	settings->config_path = path;

	return path;
}

char* freerdp_get_current_path(rdpSettings* settings)
{
	if (settings->current_path == NULL)
		settings->current_path = getcwd(NULL, 0);

	return settings->current_path;
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

	path = xmalloc(length + 1);
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
			path = xmalloc(file_path_length + shared_lib_suffix_length + 1);
			sprintf(path, "%s%s", file_path, SHARED_LIB_SUFFIX);
		}
		else
		{
			path = xstrdup(file_path);
		}
	}
	else
	{
		path = xstrdup(file_path);
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
		return xstrdup(base_path);

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

	path = (char*) xmalloc(length + 1);
	memcpy(path, base_path, length);
	path[length] = '\0';

	return path;
}

boolean freerdp_path_contains_separator(char* path)
{
	if (path == NULL)
		return false;

	if (strchr(path, PATH_SEPARATOR_CHR) == NULL)
		return false;

	return true;
}

/* detects if we are running from the source tree */

boolean freerdp_detect_development_mode(rdpSettings* settings)
{
	int depth = 0;
	char* current_path;
	char* development_path = NULL;
	boolean development_mode = false;

	if (freerdp_check_file_exists(".git"))
	{
		depth = 0;
		development_mode = true;
	}
	else if (freerdp_check_file_exists(PARENT_PATH ".git"))
	{
		depth = 1;
		development_mode = true;
	}
	else if (freerdp_check_file_exists(PARENT_PATH PARENT_PATH ".git"))
	{
		depth = 2;
		development_mode = true;
	}

	current_path = freerdp_get_current_path(settings);

	if (development_mode)
		development_path = freerdp_get_parent_path(current_path, depth);

	settings->development_mode = development_mode;
	settings->development_path = development_path;

	return settings->development_mode;
}

void freerdp_detect_paths(rdpSettings* settings)
{
	freerdp_get_home_path(settings);
	freerdp_get_config_path(settings);
	freerdp_detect_development_mode(settings);

#if 0
	printf("home path: %s\n", settings->home_path);
	printf("config path: %s\n", settings->config_path);
	printf("current path: %s\n", settings->current_path);

	if (settings->development_mode)
		printf("development path: %s\n", settings->development_path);
#endif
}

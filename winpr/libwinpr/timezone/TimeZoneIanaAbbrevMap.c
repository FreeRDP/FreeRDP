/**
 * WinPR: Windows Portable Runtime
 * Time Zone
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

#include "TimeZoneIanaAbbrevMap.h"

#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include <winpr/string.h>

typedef struct
{
	char* Iana;
	char* Abbrev;
} TimeZoneInanaAbbrevMapEntry;

const static char* zonepath = "/usr/share/zoneinfo";

static TimeZoneInanaAbbrevMapEntry* TimeZoneIanaAbbrevMap = NULL;
static size_t TimeZoneIanaAbbrevMapSize = 0;

static void append(const char* iana, const char* sname)
{
	const size_t size = TimeZoneIanaAbbrevMapSize + 1;

	TimeZoneInanaAbbrevMapEntry* tmp =
	    realloc(TimeZoneIanaAbbrevMap, size * sizeof(TimeZoneInanaAbbrevMapEntry));
	if (!tmp)
		return;
	TimeZoneIanaAbbrevMap = tmp;
	TimeZoneIanaAbbrevMapSize = size;

	TimeZoneInanaAbbrevMapEntry* cur = &TimeZoneIanaAbbrevMap[size - 1];
	cur->Abbrev = _strdup(sname);
	cur->Iana = _strdup(iana);
}

static void append_timezone(const char* dir, const char* name)
{
	char* tz = NULL;
	if (!dir && !name)
		return;
	if (!dir)
	{
		size_t len = 0;
		winpr_asprintf(&tz, &len, "%s", name);
	}
	else
	{
		size_t len = 0;
		winpr_asprintf(&tz, &len, "%s/%s", dir, name);
	}
	if (!tz)
		return;

	const char* otz = getenv("TZ");
	char* oldtz = NULL;
	if (otz)
		oldtz = _strdup(otz);
	setenv("TZ", tz, 1);
	tzset();
	const time_t t = time(NULL);
	struct tm lt = { 0 };
	localtime_r(&t, &lt);
	append(tz, lt.tm_zone);
	if (oldtz)
	{
		setenv("TZ", oldtz, 1);
		free(oldtz);
	}
	else
		unsetenv("TZ");
	free(tz);
}

static void handle_link(const char* base, const char* dir, const char* name);

static char* topath(const char* base, const char* bname, const char* name)
{
	size_t plen = 0;
	char* path = NULL;

	if (!base && !bname && !name)
		return NULL;

	if (!base && !name)
		return _strdup(bname);

	if (!bname && !name)
		return _strdup(base);

	if (!base && !bname)
		return _strdup(name);

	if (!base)
		winpr_asprintf(&path, &plen, "%s/%s", bname, name);
	else if (!bname)
		winpr_asprintf(&path, &plen, "%s/%s", base, name);
	else if (!name)
		winpr_asprintf(&path, &plen, "%s/%s", base, bname);
	else
		winpr_asprintf(&path, &plen, "%s/%s/%s", base, bname, name);
	return path;
}

static void iterate_subdir_recursive(const char* base, const char* bname, const char* name)
{
	char* path = topath(base, bname, name);
	if (!path)
		return;

	DIR* d = opendir(path);
	if (d)
	{
		struct dirent* dp = NULL;
		while ((dp = readdir(d)) != NULL)
		{
			switch (dp->d_type)
			{
				case DT_DIR:
				{
					if (strcmp(dp->d_name, ".") == 0)
						continue;
					if (strcmp(dp->d_name, "..") == 0)
						continue;
					iterate_subdir_recursive(path, dp->d_name, NULL);
				}
				break;
				case DT_LNK:
					handle_link(base, bname, dp->d_name);
					break;
				case DT_REG:
					append_timezone(bname, dp->d_name);
					break;
				default:
					break;
			}
		}
		closedir(d);
	}
	free(path);
}

static char* get_link_target(const char* base, const char* dir, const char* name)
{
	char* apath = NULL;
	char* path = topath(base, dir, name);
	if (!path)
		return NULL;

	SSIZE_T rc = -1;
	size_t size = 0;
	char* target = NULL;
	do
	{
		size += 64;
		char* tmp = realloc(target, size + 1);
		if (!tmp)
			goto fail;

		target = tmp;

		memset(target, 0, size + 1);
		rc = readlink(path, target, size);
		if (rc < 0)
			goto fail;
	} while ((size_t)rc >= size);

	apath = topath(base, dir, target);
fail:
	free(target);
	free(path);
	return apath;
}

void handle_link(const char* base, const char* dir, const char* name)
{
	int isDir = -1;

	char* target = get_link_target(base, dir, name);
	if (target)
	{
		struct stat s = { 0 };
		const int rc3 = stat(target, &s);
		if (rc3 == 0)
			isDir = S_ISDIR(s.st_mode);

		free(target);
	}

	switch (isDir)
	{
		case 1:
			iterate_subdir_recursive(base, dir, name);
			break;
		case 0:
			append_timezone(dir, name);
			break;
		default:
			break;
	}
}

static void TimeZoneIanaAbbrevCleanup(void)
{
	if (!TimeZoneIanaAbbrevMap)
		return;

	for (size_t x = 0; x < TimeZoneIanaAbbrevMapSize; x++)
	{
		TimeZoneInanaAbbrevMapEntry* entry = &TimeZoneIanaAbbrevMap[x];
		free(entry->Iana);
		free(entry->Abbrev);
	}
	free(TimeZoneIanaAbbrevMap);
	TimeZoneIanaAbbrevMap = NULL;
	TimeZoneIanaAbbrevMapSize = 0;
}

static void TimeZoneIanaAbbrevInitialize(void)
{
	static BOOL initialized = FALSE;
	if (initialized)
		return;

	iterate_subdir_recursive(zonepath, NULL, NULL);
	(void)atexit(TimeZoneIanaAbbrevCleanup);
	initialized = TRUE;
}

size_t TimeZoneIanaAbbrevGet(const char* abbrev, const char** list, size_t listsize)
{
	TimeZoneIanaAbbrevInitialize();

	size_t rc = 0;
	for (size_t x = 0; x < TimeZoneIanaAbbrevMapSize; x++)
	{
		const TimeZoneInanaAbbrevMapEntry* entry = &TimeZoneIanaAbbrevMap[x];
		if (strcmp(abbrev, entry->Abbrev) == 0)
		{
			if (listsize > rc)
				list[rc] = entry->Iana;
			rc++;
		}
	}

	return rc;
}

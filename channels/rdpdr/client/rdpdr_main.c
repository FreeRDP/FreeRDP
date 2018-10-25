/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015-2016 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpdr.h>

#ifdef _WIN32
#include <windows.h>
#include <dbt.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#ifdef __MACOSX__
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "rdpdr_capabilities.h"

#include "devman.h"
#include "irp.h"

#include "rdpdr_main.h"

typedef struct _DEVICE_DRIVE_EXT DEVICE_DRIVE_EXT;
/* IMPORTANT: Keep in sync with DRIVE_DEVICE */
struct _DEVICE_DRIVE_EXT
{
	DEVICE device;
	WCHAR* path;
	BOOL automount;
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_send_device_list_announce_request(rdpdrPlugin* rdpdr,
        BOOL userLoggedOn);

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_send_device_list_remove_request(rdpdrPlugin* rdpdr,
        UINT32 count, UINT32 ids[])
{
	UINT32 i;
	wStream* s;
	s = Stream_New(NULL, count * sizeof(UINT32) + 8);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);
	Stream_Write_UINT16(s, PAKID_CORE_DEVICELIST_REMOVE);
	Stream_Write_UINT32(s, count);

	for (i = 0; i < count; i++)
		Stream_Write_UINT32(s, ids[i]);

	Stream_SealLength(s);
	return rdpdr_send(rdpdr, s);
}

#ifdef _UWP

void first_hotplug(rdpdrPlugin* rdpdr)
{
}

static DWORD WINAPI drive_hotplug_thread_func(LPVOID arg)
{
	return CHANNEL_RC_OK;
}

static UINT drive_hotplug_thread_terminate(rdpdrPlugin* rdpdr)
{
	return CHANNEL_RC_OK;
}

#elif _WIN32

BOOL check_path(char* path)
{
	UINT type = GetDriveTypeA(path);

	if (!(type == DRIVE_FIXED || type == DRIVE_REMOVABLE || type == DRIVE_CDROM || type == DRIVE_REMOTE))
		return FALSE;

	return GetVolumeInformationA(path, NULL, 0, NULL, NULL, NULL, NULL, 0);
}

void first_hotplug(rdpdrPlugin* rdpdr)
{
	int i;
	char drive_path[5] = { 'c', ':', '\\', '\0' };
	DWORD unitmask = GetLogicalDrives();

	for (i = 0; i < 26; i++)
	{
		if (unitmask & 0x01)
		{
			RDPDR_DRIVE* drive;
			drive_path[0] = 'A' + i;
			drive_path[1] = ':';

			if (check_path(drive_path))
			{
				drive = (RDPDR_DRIVE*)malloc(sizeof(RDPDR_DRIVE));
				ZeroMemory(drive, sizeof(RDPDR_DRIVE));
				drive->Type = RDPDR_DTYP_FILESYSTEM;
				drive->Path = _strdup(drive_path);
				drive_path[1] = '\0';
				drive->Name = _strdup(drive_path);
				drive->automount = TRUE;
				devman_load_device_service(rdpdr->devman, (RDPDR_DEVICE*)drive,
				                           rdpdr->rdpcontext);
			}
		}

		unitmask = unitmask >> 1;
	}
}

LRESULT CALLBACK hotplug_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	rdpdrPlugin* rdpdr;
	PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)lParam;
	UINT error;
	rdpdr = (rdpdrPlugin*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (Msg)
	{
		case WM_DEVICECHANGE:
			switch (wParam)
			{
				case DBT_DEVICEARRIVAL:
					if (lpdb -> dbch_devicetype == DBT_DEVTYP_VOLUME)
					{
						PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lpdb;
						DWORD unitmask = lpdbv->dbcv_unitmask;
						int i;
						char drive_path[4] = { 'c', ':', '/', '\0'};

						for (i = 0; i < 26; i++)
						{
							if (unitmask & 0x01)
							{
								RDPDR_DRIVE* drive;
								drive_path[0] = 'A' + i;
								drive_path[1] = ':';

								if (check_path(drive_path))
								{
									drive = (RDPDR_DRIVE*) malloc(sizeof(RDPDR_DRIVE));
									ZeroMemory(drive, sizeof(RDPDR_DRIVE));
									drive->Type = RDPDR_DTYP_FILESYSTEM;
									drive->Path = _strdup(drive_path);
									drive_path[1] = '\0';
									drive->automount = TRUE;
									drive->Name = _strdup(drive_path);
									devman_load_device_service(rdpdr->devman, (RDPDR_DEVICE*)drive,
									                           rdpdr->rdpcontext);
									rdpdr_send_device_list_announce_request(rdpdr, TRUE);
								}
							}

							unitmask = unitmask >> 1;
						}
					}

					break;

				case DBT_DEVICEREMOVECOMPLETE:
					if (lpdb -> dbch_devicetype == DBT_DEVTYP_VOLUME)
					{
						PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lpdb;
						DWORD unitmask = lpdbv->dbcv_unitmask;
						int i, j, count;
						char drive_name_upper, drive_name_lower;
						ULONG_PTR* keys = NULL;
						DEVICE_DRIVE_EXT* device_ext;
						UINT32 ids[1];

						for (i = 0; i < 26; i++)
						{
							if (unitmask & 0x01)
							{
								drive_name_upper = 'A' + i;
								drive_name_lower = 'a' + i;
								count = ListDictionary_GetKeys(rdpdr->devman->devices, &keys);

								for (j = 0; j < count; j++)
								{
									device_ext = (DEVICE_DRIVE_EXT*)ListDictionary_GetItemValue(
									                 rdpdr->devman->devices, (void*)keys[j]);

									if (device_ext->path[0] == drive_name_upper
									    || device_ext->path[0] == drive_name_lower)
									{
										if (device_ext->automount)
										{
											devman_unregister_device(rdpdr->devman, (void*)keys[j]);
											ids[0] = keys[j];

											if ((error = rdpdr_send_device_list_remove_request(rdpdr, 1, ids)))
											{
												// dont end on error, just report ?
												WLog_ERR(TAG, "rdpdr_send_device_list_remove_request failed with error %"PRIu32"!",
												         error);
											}

											break;
										}
									}
								}

								free(keys);
							}

							unitmask = unitmask >> 1;
						}
					}

					break;

				default:
					break;
			}

			break;

		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);
	}

	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

static DWORD WINAPI drive_hotplug_thread_func(LPVOID arg)
{
	rdpdrPlugin* rdpdr;
	WNDCLASSEX wnd_cls;
	HWND hwnd;
	MSG msg;
	BOOL bRet;
	DEV_BROADCAST_HANDLE NotificationFilter;
	HDEVNOTIFY hDevNotify;
	rdpdr = (rdpdrPlugin*)arg;
	/* init windows class */
	wnd_cls.cbSize        = sizeof(WNDCLASSEX);
	wnd_cls.style         = CS_HREDRAW | CS_VREDRAW;
	wnd_cls.lpfnWndProc   = hotplug_proc;
	wnd_cls.cbClsExtra    = 0;
	wnd_cls.cbWndExtra    = 0;
	wnd_cls.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wnd_cls.hCursor       = NULL;
	wnd_cls.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wnd_cls.lpszMenuName  = NULL;
	wnd_cls.lpszClassName = L"DRIVE_HOTPLUG";
	wnd_cls.hInstance     = NULL;
	wnd_cls.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&wnd_cls);
	/* create window */
	hwnd = CreateWindowEx(0, L"DRIVE_HOTPLUG", NULL,
	                      0, 0, 0, 0, 0,
	                      NULL, NULL, NULL, NULL);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)rdpdr);
	rdpdr->hotplug_wnd = hwnd;
	/* register device interface to hwnd */
	NotificationFilter.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
	NotificationFilter.dbch_devicetype = DBT_DEVTYP_HANDLE;
	hDevNotify = RegisterDeviceNotification(hwnd, &NotificationFilter,
	                                        DEVICE_NOTIFY_WINDOW_HANDLE);

	/* message loop */
	while ((bRet = GetMessage(&msg, 0, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	UnregisterDeviceNotification(hDevNotify);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drive_hotplug_thread_terminate(rdpdrPlugin* rdpdr)
{
	UINT error = CHANNEL_RC_OK;

	if (rdpdr->hotplug_wnd && !PostMessage(rdpdr->hotplug_wnd, WM_QUIT, 0, 0))
	{
		error = GetLastError();
		WLog_ERR(TAG, "PostMessage failed with error %"PRIu32"", error);
	}

	return error;
}

#elif __MACOSX__

#define MAX_USB_DEVICES 100

typedef struct _hotplug_dev
{
	char* path;
	BOOL  to_add;
} hotplug_dev;


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT handle_hotplug(rdpdrPlugin* rdpdr)
{
	struct dirent* pDirent;
	DIR* pDir;
	char fullpath[PATH_MAX];
	char* szdir = (char*)"/Volumes";
	struct stat buf;
	hotplug_dev dev_array[MAX_USB_DEVICES];
	int count;
	DEVICE_DRIVE_EXT* device_ext;
	ULONG_PTR* keys = NULL;
	int i, j;
	int size = 0;
	UINT error;
	UINT32 ids[1];
	pDir = opendir(szdir);

	if (pDir == NULL)
	{
		printf("Cannot open directory\n");
		return ERROR_OPEN_FAILED;
	}

	while ((pDirent = readdir(pDir)) != NULL)
	{
		if (pDirent->d_name[0] != '.')
		{
			sprintf_s(fullpath, ARRAYSIZE(fullpath), "%s/%s", szdir, pDirent->d_name);
			lstat(fullpath, &buf);

			if (S_ISDIR(buf.st_mode) || S_ISLNK(buf.st_mode))
			{
				dev_array[size].path = _strdup(fullpath);

				if (!dev_array[size].path)
				{
					closedir(pDir);
					error = CHANNEL_RC_NO_MEMORY;
					goto cleanup;
				}

				dev_array[size++].to_add = TRUE;
			}
		}
	}

	closedir(pDir);
	/* delete removed devices */
	count = ListDictionary_GetKeys(rdpdr->devman->devices, &keys);

	for (j = 0; j < count; j++)
	{
		char* path = NULL;
		BOOL dev_found = FALSE;
		device_ext = (DEVICE_DRIVE_EXT*)ListDictionary_GetItemValue(
		                 rdpdr->devman->devices, (void*)keys[j]);

		if (!device_ext || !device_ext->automount)
			continue;

		if (device_ext->path == NULL)
			continue;

		if (ConvertFromUnicode(CP_UTF8, 0, device_ext->path, 0, &path, 0, NULL, FALSE) <= 0)
			continue;

		/* not plugable device */
		if (strstr(path, "/Volumes/") == NULL)
		{
			free(path);
			continue;
		}

		for (i = 0; i < size; i++)
		{
			if (strstr(path, dev_array[i].path) != NULL)
			{
				dev_found = TRUE;
				dev_array[i].to_add = FALSE;
				break;
			}
		}

		free(path);

		if (!dev_found)
		{
			devman_unregister_device(rdpdr->devman, (void*)keys[j]);
			ids[0] = keys[j];

			if ((error = rdpdr_send_device_list_remove_request(rdpdr, 1, ids)))
			{
				WLog_ERR(TAG, "rdpdr_send_device_list_remove_request failed with error %"PRIu32"!",
				         error);
				goto cleanup;
			}
		}
	}

	/* add new devices */
	for (i = 0; i < size; i++)
	{
		RDPDR_DRIVE* drive;

		if (dev_array[i].to_add)
		{
			char* name;
			drive = (RDPDR_DRIVE*) calloc(1, sizeof(RDPDR_DRIVE));

			if (!drive)
			{
				WLog_ERR(TAG, "calloc failed!");
				error = CHANNEL_RC_NO_MEMORY;
				goto cleanup;
			}

			drive->Type = RDPDR_DTYP_FILESYSTEM;
			drive->Path = dev_array[i].path;
			drive->automount = TRUE;
			dev_array[i].path = NULL;
			name = strrchr(drive->Path, '/') + 1;
			drive->Name = _strdup(name);

			if (!drive->Name)
			{
				WLog_ERR(TAG, "_strdup failed!");
				free(drive->Path);
				free(drive);
				error = CHANNEL_RC_NO_MEMORY;
				goto cleanup;
			}

			if ((error = devman_load_device_service(rdpdr->devman, (RDPDR_DEVICE*)drive,
			                                        rdpdr->rdpcontext)))
			{
				WLog_ERR(TAG, "devman_load_device_service failed!");
				free(drive->Path);
				free(drive->Name);
				free(drive);
				error = CHANNEL_RC_NO_MEMORY;
				goto cleanup;
			}
		}
	}

cleanup:
	free(keys);

	for (i = 0; i < size; i++)
		free(dev_array[i].path);

	return error;
}


static void drive_hotplug_fsevent_callback(ConstFSEventStreamRef streamRef,
        void* clientCallBackInfo,
        size_t numEvents, void* eventPaths, const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[])
{
	rdpdrPlugin* rdpdr;
	int i;
	UINT error;
	char** paths = (char**)eventPaths;
	rdpdr = (rdpdrPlugin*) clientCallBackInfo;

	for (i = 0; i < numEvents; i++)
	{
		if (strcmp(paths[i], "/Volumes/") == 0)
		{
			if ((error = handle_hotplug(rdpdr)))
			{
				WLog_ERR(TAG, "handle_hotplug failed with error %"PRIu32"!", error);
			}
			else
				rdpdr_send_device_list_announce_request(rdpdr, TRUE);

			return;
		}
	}
}

void first_hotplug(rdpdrPlugin* rdpdr)
{
	UINT error;

	if ((error = handle_hotplug(rdpdr)))
	{
		WLog_ERR(TAG, "handle_hotplug failed with error %"PRIu32"!", error);
	}
}

static DWORD WINAPI drive_hotplug_thread_func(LPVOID arg)
{
	rdpdrPlugin* rdpdr;
	FSEventStreamRef fsev;
	rdpdr = (rdpdrPlugin*) arg;
	CFStringRef path = CFSTR("/Volumes/");
	CFArrayRef pathsToWatch = CFArrayCreate(kCFAllocatorMalloc, (const void**)&path,
	                                        1, NULL);
	FSEventStreamContext ctx;
	ZeroMemory(&ctx, sizeof(ctx));
	ctx.info = arg;
	fsev = FSEventStreamCreate(kCFAllocatorMalloc, drive_hotplug_fsevent_callback,
	                           &ctx, pathsToWatch, kFSEventStreamEventIdSinceNow, 1,
	                           kFSEventStreamCreateFlagNone);
	rdpdr->runLoop = CFRunLoopGetCurrent();
	FSEventStreamScheduleWithRunLoop(fsev, rdpdr->runLoop, kCFRunLoopDefaultMode);
	FSEventStreamStart(fsev);
	CFRunLoopRun();
	FSEventStreamStop(fsev);
	FSEventStreamRelease(fsev);
	ExitThread(CHANNEL_RC_OK);
	return CHANNEL_RC_OK;
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drive_hotplug_thread_terminate(rdpdrPlugin* rdpdr)
{
	UINT error;

	if (rdpdr->hotplugThread)
	{
		CFRunLoopStop(rdpdr->runLoop);

		if (WaitForSingleObject(rdpdr->hotplugThread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			return error;
		}

		rdpdr->hotplugThread = NULL;
	}

	return CHANNEL_RC_OK;
}

#else

#define MAX_USB_DEVICES 100

typedef struct _hotplug_dev
{
	char* path;
	BOOL  to_add;
} hotplug_dev;


static const char* automountLocations[] =
{
	"/run/user/%lu/gvfs",
	"/run/media/%s",
	"/media/%s",
	"/media",
	"/mnt"
};

static BOOL isAutomountLocation(const char* path)
{
	const size_t nrLocations = sizeof(automountLocations) / sizeof(automountLocations[0]);
	size_t x;
	char buffer[MAX_PATH];
	uid_t uid = getuid();
	const char* uname = getlogin();

	if (!path)
		return FALSE;

	for (x = 0; x < nrLocations; x++)
	{
		const char* location = automountLocations[x];
		size_t length;

		if (strstr(location, "%lu"))
			snprintf(buffer, sizeof(buffer), location, (unsigned long)uid);
		else if (strstr(location, "%s"))
			snprintf(buffer, sizeof(buffer), location, uname);
		else
			snprintf(buffer, sizeof(buffer), "%s", location);

		length = strnlen(buffer, sizeof(buffer));

		if (strncmp(buffer, path, length) == 0)
		{
			const char* rest = &path[length];

			/* Only consider mount locations with max depth of 1 below the
			 * base path or the base path itself. */
			if (*rest == '\0')
				return TRUE;
			else if (*rest == '/')
			{
				const char* token = strstr(&rest[1], "/");

				if (!token || (token[1] == '\0'))
					return TRUE;
			}
		}
	}

	return FALSE;
}

static char* next_line(FILE* fd, size_t* len)
{
	size_t newsiz;
	int c;
	char* newbuf;
	char* lrbuf;
	int lrsiz;
	*len = 0;
	lrsiz = 0;
	lrbuf = NULL;
	newbuf = NULL;

	for (;;)
	{
		c = fgetc(fd);

		if (ferror(fd))
		{
			free(newbuf);
			return NULL;
		}

		if (c == EOF)
		{
			if (*len == 0)
				return NULL;
			else
			{
				lrbuf[(*len)] = '\0';
				return lrbuf;
			}
		}
		else
		{
			if (*len == lrsiz)
			{
				newsiz = lrsiz + 4096;
				newbuf = realloc(lrbuf, newsiz);

				if (newbuf == NULL)
					return NULL;

				lrbuf = newbuf;
				lrsiz = newsiz;
			}

			lrbuf[(*len)] = c;

			if (c == '\n')
			{
				lrbuf[(*len)] = '\0';
				return lrbuf;
			}

			(*len)++;
		}
	}
}

static char* get_word(char* str, unsigned int* offset)
{
	char* p;
	char* tmp;
	char* word;
	int wlen;

	if (*offset >= strlen(str))
		return NULL;

	p = str + *offset;
	tmp = p;

	while (*tmp != ' ' && *tmp  != '\0')
		tmp++;

	wlen = tmp - p;
	*offset += wlen;

	/* in case there are more than one space between words */
	while (*(str + *offset) == ' ')
		(*offset)++;

	word = malloc(wlen + 1);

	if (word != NULL)
	{
		CopyMemory(word, p, wlen);
		word[wlen] = '\0';
	}

	return word;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT handle_hotplug(rdpdrPlugin* rdpdr)
{
	FILE* f;
	size_t len;
	char* line;
	char* word;
	unsigned int wlen;
	hotplug_dev dev_array[MAX_USB_DEVICES];
	int i, j;
	int size = 0;
	int count;
	ULONG_PTR* keys = NULL;
	UINT32 ids[1];
	UINT error = 0;
	memset(dev_array, 0, sizeof(dev_array));
	f = fopen("/proc/mounts", "r");

	if (f == NULL)
	{
		WLog_ERR(TAG, "fopen failed!");
		return ERROR_OPEN_FAILED;
	}

	while ((line = next_line(f, &len)))
	{
		wlen = 0;

		while ((word = get_word(line, &wlen)))
		{
			/* copy hotpluged device mount point to the dev_array */
			if (isAutomountLocation(word))
			{
				dev_array[size].path = word;
				dev_array[size++].to_add = TRUE;
			}
			else
				free(word);
		}

		free(line);
	}

	fclose(f);
	/* delete removed devices */
	count = ListDictionary_GetKeys(rdpdr->devman->devices, &keys);

	for (j = 0; j < count; j++)
	{
		char* path = NULL;
		BOOL dev_found = FALSE;
		DEVICE_DRIVE_EXT* device_ext = (DEVICE_DRIVE_EXT*)ListDictionary_GetItemValue(
		                                   rdpdr->devman->devices, (void*)keys[j]);

		if (!device_ext || !device_ext->path || !device_ext->automount)
			continue;

		ConvertFromUnicode(CP_UTF8, 0, device_ext->path, -1, &path, 0, NULL, NULL);

		if (!path)
			continue;

		/* not plugable device */
		if (isAutomountLocation(path))
		{
			for (i = 0; i < size; i++)
			{
				if (strstr(path, dev_array[i].path) != NULL)
				{
					dev_found = TRUE;
					dev_array[i].to_add = FALSE;
					break;
				}
			}
		}

		free(path);

		if (!dev_found)
		{
			devman_unregister_device(rdpdr->devman, (void*)keys[j]);
			ids[0] = keys[j];

			if ((error = rdpdr_send_device_list_remove_request(rdpdr, 1, ids)))
			{
				WLog_ERR(TAG, "rdpdr_send_device_list_remove_request failed with error %"PRIu32"!",
				         error);
				goto cleanup;
			}
		}
	}

	/* add new devices */
	for (i = 0; i < size; i++)
	{
		RDPDR_DRIVE* drive;

		if (dev_array[i].to_add)
		{
			char* name;
			drive = (RDPDR_DRIVE*) calloc(1, sizeof(RDPDR_DRIVE));

			if (!drive)
			{
				WLog_ERR(TAG, "calloc failed!");
				error = CHANNEL_RC_NO_MEMORY;
				goto cleanup;
			}

			drive->Type = RDPDR_DTYP_FILESYSTEM;
			drive->Path = dev_array[i].path;
			drive->automount = TRUE;
			dev_array[i].path = NULL;
			name = strrchr(drive->Path, '/') + 1;
			drive->Name = _strdup(name);

			if (!drive->Name)
			{
				WLog_ERR(TAG, "_strdup failed!");
				free(drive->Path);
				free(drive);
				error = CHANNEL_RC_NO_MEMORY;
				goto cleanup;
			}

			if ((error = devman_load_device_service(rdpdr->devman, (RDPDR_DEVICE*)drive,
			                                        rdpdr->rdpcontext)))
			{
				WLog_ERR(TAG, "devman_load_device_service failed!");
				free(drive->Path);
				free(drive->Name);
				free(drive);
				goto cleanup;
			}
		}
	}

cleanup:
	free(keys);

	for (i = 0; i < size; i++)
		free(dev_array[i].path);

	return error;
}

static void first_hotplug(rdpdrPlugin* rdpdr)
{
	UINT error;

	if ((error = handle_hotplug(rdpdr)))
	{
		WLog_ERR(TAG, "handle_hotplug failed with error %"PRIu32"!", error);
	}
}

static DWORD WINAPI drive_hotplug_thread_func(LPVOID arg)
{
	rdpdrPlugin* rdpdr;
	int mfd;
	fd_set rfds;
	struct timeval tv;
	int rv;
	UINT error = 0;
	DWORD status;
	rdpdr = (rdpdrPlugin*) arg;

	if (!(rdpdr->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	mfd = open("/proc/mounts", O_RDONLY, 0);

	if (mfd < 0)
	{
		WLog_ERR(TAG, "ERROR: Unable to open /proc/mounts.");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	FD_ZERO(&rfds);
	FD_SET(mfd, &rfds);
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while ((rv = select(mfd + 1, NULL, NULL, &rfds, &tv)) >= 0)
	{
		status = WaitForSingleObject(rdpdr->stopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
			break;

		if (FD_ISSET(mfd, &rfds))
		{
			/* file /proc/mounts changed, handle this */
			if ((error = handle_hotplug(rdpdr)))
			{
				WLog_ERR(TAG, "handle_hotplug failed with error %"PRIu32"!", error);
				goto out;
			}
			else
				rdpdr_send_device_list_announce_request(rdpdr, TRUE);
		}

		FD_ZERO(&rfds);
		FD_SET(mfd, &rfds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
	}

out:

	if (error && rdpdr->rdpcontext)
		setChannelError(rdpdr->rdpcontext, error,
		                "drive_hotplug_thread_func reported an error");

	CloseHandle(rdpdr->stopEvent);
	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drive_hotplug_thread_terminate(rdpdrPlugin* rdpdr)
{
	UINT error;

	if (rdpdr->hotplugThread)
	{
		if (rdpdr->stopEvent)
			SetEvent(rdpdr->stopEvent);

		if (WaitForSingleObject(rdpdr->hotplugThread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			return error;
		}

		rdpdr->hotplugThread = NULL;
	}

	return CHANNEL_RC_OK;
}

#endif


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_process_connect(rdpdrPlugin* rdpdr)
{
	UINT32 index;
	RDPDR_DEVICE* device;
	rdpSettings* settings;
	UINT error = CHANNEL_RC_OK;
	rdpdr->devman = devman_new(rdpdr);

	if (!rdpdr->devman)
	{
		WLog_ERR(TAG, "devman_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	settings = (rdpSettings*) rdpdr->channelEntryPoints.pExtendedData;

	if (settings->ClientHostname)
		strncpy(rdpdr->computerName, settings->ClientHostname,
		        sizeof(rdpdr->computerName) - 1);
	else
		strncpy(rdpdr->computerName, settings->ComputerName,
		        sizeof(rdpdr->computerName) - 1);

	for (index = 0; index < settings->DeviceCount; index++)
	{
		device = settings->DeviceArray[index];

		if (device->Type == RDPDR_DTYP_FILESYSTEM)
		{
			RDPDR_DRIVE* drive = (RDPDR_DRIVE*)device;

			if (drive->Path && (strcmp(drive->Path, "*") == 0))
			{
				first_hotplug(rdpdr);

				if (!(rdpdr->hotplugThread = CreateThread(NULL, 0,
				                             drive_hotplug_thread_func, rdpdr, 0, NULL)))
				{
					WLog_ERR(TAG, "CreateThread failed!");
					return ERROR_INTERNAL_ERROR;
				}

				continue;
			}
		}

		if ((error = devman_load_device_service(rdpdr->devman, device,
		                                        rdpdr->rdpcontext)))
		{
			WLog_ERR(TAG, "devman_load_device_service failed with error %"PRIu32"!", error);
			return error;
		}
	}

	return error;
}

static UINT rdpdr_process_server_announce_request(rdpdrPlugin* rdpdr,
        wStream* s)
{
	if (Stream_GetRemainingLength(s) < 8)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, rdpdr->versionMajor);
	Stream_Read_UINT16(s, rdpdr->versionMinor);
	Stream_Read_UINT32(s, rdpdr->clientID);
	rdpdr->sequenceId++;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_send_client_announce_reply(rdpdrPlugin* rdpdr)
{
	wStream* s;
	s = Stream_New(NULL, 12);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE); /* Component (2 bytes) */
	Stream_Write_UINT16(s, PAKID_CORE_CLIENTID_CONFIRM); /* PacketId (2 bytes) */
	Stream_Write_UINT16(s, rdpdr->versionMajor);
	Stream_Write_UINT16(s, rdpdr->versionMinor);
	Stream_Write_UINT32(s, (UINT32) rdpdr->clientID);
	return rdpdr_send(rdpdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_send_client_name_request(rdpdrPlugin* rdpdr)
{
	wStream* s;
	WCHAR* computerNameW = NULL;
	size_t computerNameLenW;

	if (!rdpdr->computerName[0])
		gethostname(rdpdr->computerName, sizeof(rdpdr->computerName) - 1);

	computerNameLenW = ConvertToUnicode(CP_UTF8, 0, rdpdr->computerName, -1,
	                                    &computerNameW, 0) * 2;
	s = Stream_New(NULL, 16 + computerNameLenW + 2);

	if (!s)
	{
		free(computerNameW);
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE); /* Component (2 bytes) */
	Stream_Write_UINT16(s, PAKID_CORE_CLIENT_NAME); /* PacketId (2 bytes) */
	Stream_Write_UINT32(s, 1); /* unicodeFlag, 0 for ASCII and 1 for Unicode */
	Stream_Write_UINT32(s, 0); /* codePage, must be set to zero */
	Stream_Write_UINT32(s, computerNameLenW +
	                    2); /* computerNameLen, including null terminator */
	Stream_Write(s, computerNameW, computerNameLenW);
	Stream_Write_UINT16(s, 0); /* null terminator */
	free(computerNameW);
	return rdpdr_send(rdpdr, s);
}

static UINT rdpdr_process_server_clientid_confirm(rdpdrPlugin* rdpdr,
        wStream* s)
{
	UINT16 versionMajor;
	UINT16 versionMinor;
	UINT32 clientID;

	if (Stream_GetRemainingLength(s) < 8)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, versionMajor);
	Stream_Read_UINT16(s, versionMinor);
	Stream_Read_UINT32(s, clientID);

	if (versionMajor != rdpdr->versionMajor || versionMinor != rdpdr->versionMinor)
	{
		rdpdr->versionMajor = versionMajor;
		rdpdr->versionMinor = versionMinor;
	}

	if (clientID != rdpdr->clientID)
		rdpdr->clientID = clientID;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_send_device_list_announce_request(rdpdrPlugin* rdpdr,
        BOOL userLoggedOn)
{
	int i;
	BYTE c;
	size_t pos;
	int index;
	wStream* s;
	UINT32 count;
	size_t data_len;
	size_t count_pos;
	DEVICE* device;
	int keyCount;
	ULONG_PTR* pKeys = NULL;
	s = Stream_New(NULL, 256);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE); /* Component (2 bytes) */
	Stream_Write_UINT16(s, PAKID_CORE_DEVICELIST_ANNOUNCE); /* PacketId (2 bytes) */
	count_pos = Stream_GetPosition(s);
	count = 0;
	Stream_Seek_UINT32(s); /* deviceCount */
	pKeys = NULL;
	keyCount = ListDictionary_GetKeys(rdpdr->devman->devices, &pKeys);

	for (index = 0; index < keyCount; index++)
	{
		device = (DEVICE*) ListDictionary_GetItemValue(rdpdr->devman->devices,
		         (void*) pKeys[index]);

		/**
		 * 1. versionMinor 0x0005 doesn't send PAKID_CORE_USER_LOGGEDON
		 *    so all devices should be sent regardless of user_loggedon
		 * 2. smartcard devices should be always sent
		 * 3. other devices are sent only after user_loggedon
		 */

		if ((rdpdr->versionMinor == 0x0005) ||
		    (device->type == RDPDR_DTYP_SMARTCARD) || userLoggedOn)
		{
			data_len = (device->data == NULL ? 0 : Stream_GetPosition(device->data));

			if (!Stream_EnsureRemainingCapacity(s, 20 + data_len))
			{
				free(pKeys);
				Stream_Free(s, TRUE);
				WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
				return ERROR_INVALID_DATA;
			}

			Stream_Write_UINT32(s, device->type); /* deviceType */
			Stream_Write_UINT32(s, device->id); /* deviceID */
			strncpy((char*) Stream_Pointer(s), device->name, 8);

			for (i = 0; i < 8; i++)
			{
				Stream_Peek_UINT8(s, c);

				if (c > 0x7F)
					Stream_Write_UINT8(s, '_');
				else
					Stream_Seek_UINT8(s);
			}

			Stream_Write_UINT32(s, data_len);

			if (data_len > 0)
				Stream_Write(s, Stream_Buffer(device->data), data_len);

			count++;
			WLog_INFO(TAG, "registered device #%"PRIu32": %s (type=%"PRIu32" id=%"PRIu32")",
			          count, device->name, device->type, device->id);
		}
	}

	free(pKeys);
	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, count_pos);
	Stream_Write_UINT32(s, count);
	Stream_SetPosition(s, pos);
	Stream_SealLength(s);
	return rdpdr_send(rdpdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_process_irp(rdpdrPlugin* rdpdr, wStream* s)
{
	IRP* irp;
	UINT error = CHANNEL_RC_OK;
	irp = irp_new(rdpdr->devman, s, &error);

	if (!irp)
	{
		WLog_ERR(TAG, "irp_new failed with %"PRIu32"!", error);
		return error;
	}

	IFCALLRET(irp->device->IRPRequest, error, irp->device, irp);

	if (error)
		WLog_ERR(TAG, "device->IRPRequest failed with error %"PRIu32"", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_process_init(rdpdrPlugin* rdpdr)
{
	int index;
	int keyCount;
	DEVICE* device;
	ULONG_PTR* pKeys = NULL;
	UINT error = CHANNEL_RC_OK;
	pKeys = NULL;
	keyCount = ListDictionary_GetKeys(rdpdr->devman->devices, &pKeys);

	for (index = 0; index < keyCount; index++)
	{
		device = (DEVICE*) ListDictionary_GetItemValue(rdpdr->devman->devices,
		         (void*) pKeys[index]);
		IFCALLRET(device->Init, error, device);

		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "Init failed!");
			free(pKeys);
			return error;
		}
	}

	free(pKeys);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_process_receive(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 component;
	UINT16 packetId;
	UINT32 deviceId;
	UINT32 status;
	UINT error;

	if (!rdpdr || !s)
		return CHANNEL_RC_NULL_DATA;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, component); /* Component (2 bytes) */
	Stream_Read_UINT16(s, packetId); /* PacketId (2 bytes) */

	if (component == RDPDR_CTYP_CORE)
	{
		switch (packetId)
		{
			case PAKID_CORE_SERVER_ANNOUNCE:
				if ((error = rdpdr_process_server_announce_request(rdpdr, s)))
					return error;

				if ((error = rdpdr_send_client_announce_reply(rdpdr)))
				{
					WLog_ERR(TAG, "rdpdr_send_client_announce_reply failed with error %"PRIu32"", error);
					return error;
				}

				if ((error = rdpdr_send_client_name_request(rdpdr)))
				{
					WLog_ERR(TAG, "rdpdr_send_client_name_request failed with error %"PRIu32"", error);
					return error;
				}

				if ((error = rdpdr_process_init(rdpdr)))
				{
					WLog_ERR(TAG, "rdpdr_process_init failed with error %"PRIu32"", error);
					return error;
				}

				break;

			case PAKID_CORE_SERVER_CAPABILITY:
				if ((error = rdpdr_process_capability_request(rdpdr, s)))
					return error;

				if ((error = rdpdr_send_capability_response(rdpdr)))
				{
					WLog_ERR(TAG, "rdpdr_send_capability_response failed with error %"PRIu32"", error);
					return error;
				}

				break;

			case PAKID_CORE_CLIENTID_CONFIRM:
				if ((error = rdpdr_process_server_clientid_confirm(rdpdr, s)))
					return error;

				if ((error = rdpdr_send_device_list_announce_request(rdpdr, FALSE)))
				{
					WLog_ERR(TAG, "rdpdr_send_device_list_announce_request failed with error %"PRIu32"",
					         error);
					return error;
				}

				break;

			case PAKID_CORE_USER_LOGGEDON:
				if ((error = rdpdr_send_device_list_announce_request(rdpdr, TRUE)))
				{
					WLog_ERR(TAG, "rdpdr_send_device_list_announce_request failed with error %"PRIu32"",
					         error);
					return error;
				}

				break;

			case PAKID_CORE_DEVICE_REPLY:

				/* connect to a specific resource */
				if (Stream_GetRemainingLength(s) < 8)
					return ERROR_INVALID_DATA;

				Stream_Read_UINT32(s, deviceId);
				Stream_Read_UINT32(s, status);
				break;

			case PAKID_CORE_DEVICE_IOREQUEST:
				if ((error = rdpdr_process_irp(rdpdr, s)))
				{
					WLog_ERR(TAG, "rdpdr_process_irp failed with error %"PRIu32"", error);
					return error;
				}

				s = NULL;
				break;

			default:
				WLog_ERR(TAG, "RDPDR_CTYP_CORE unknown PacketId: 0x%04"PRIX16"", packetId);
				return ERROR_INVALID_DATA;
				break;
		}
	}
	else if (component == RDPDR_CTYP_PRN)
	{
		switch (packetId)
		{
			case PAKID_PRN_CACHE_DATA:
				{
					UINT32 eventID;

					if (Stream_GetRemainingLength(s) < 4)
						return ERROR_INVALID_DATA;

					Stream_Read_UINT32(s, eventID);
					WLog_ERR(TAG,
					         "Ignoring unhandled message PAKID_PRN_CACHE_DATA (EventID: 0x%08"PRIX32")", eventID);
				}
				break;

			case PAKID_PRN_USING_XPS:
				WLog_ERR(TAG, "Ignoring unhandled message PAKID_PRN_USING_XPS");
				break;

			default:
				WLog_ERR(TAG, "Unknown printing component packetID: 0x%04"PRIX16"", packetId);
				return ERROR_INVALID_DATA;
		}
	}
	else
	{
		WLog_ERR(TAG, "Unknown message: Component: 0x%04"PRIX16" PacketId: 0x%04"PRIX16"", component,
		         packetId);
		return ERROR_INVALID_DATA;
	}

	Stream_Free(s, TRUE);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpdr_send(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT status;
	rdpdrPlugin* plugin = (rdpdrPlugin*) rdpdr;

	if (!rdpdr || !s)
		return CHANNEL_RC_NULL_DATA;

	if (!plugin)
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	else
	{
		status = plugin->channelEntryPoints.pVirtualChannelWriteEx(plugin->InitHandle, plugin->OpenHandle,
		         Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG,  "pVirtualChannelWriteEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_virtual_channel_event_data_received(rdpdrPlugin* rdpdr,
        void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		/*
		 * According to MS-RDPBCGR 2.2.6.1, "All virtual channel traffic MUST be suspended.
		 * This flag is only valid in server-to-client virtual channel traffic. It MUST be
		 * ignored in client-to-server data." Thus it would be best practice to cease data
		 * transmission. However, simply returning here avoids a crash.
		 */
		return CHANNEL_RC_OK;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (rdpdr->data_in != NULL)
			Stream_Free(rdpdr->data_in, TRUE);

		rdpdr->data_in = Stream_New(NULL, totalLength);

		if (!rdpdr->data_in)
		{
			WLog_ERR(TAG,  "Stream_New failed!");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	data_in = rdpdr->data_in;

	if (!Stream_EnsureRemainingCapacity(data_in, (int) dataLength))
	{
		WLog_ERR(TAG,  "Stream_EnsureRemainingCapacity failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG, "rdpdr_virtual_channel_event_data_received: read error");
			return ERROR_INTERNAL_ERROR;
		}

		rdpdr->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (!MessageQueue_Post(rdpdr->queue, NULL, 0, (void*) data_in, NULL))
		{
			WLog_ERR(TAG, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}

	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE rdpdr_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
        UINT event,
        LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	rdpdrPlugin* rdpdr = (rdpdrPlugin*) lpUserParam;

	if (!rdpdr || !pData || (rdpdr->OpenHandle != openHandle))
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if ((error  = rdpdr_virtual_channel_event_data_received(rdpdr, pData,
			              dataLength, totalLength, dataFlags)))
				WLog_ERR(TAG,
				         "rdpdr_virtual_channel_event_data_received failed with error %"PRIu32"!", error);

			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && rdpdr->rdpcontext)
		setChannelError(rdpdr->rdpcontext, error,
		                "rdpdr_virtual_channel_open_event_ex reported an error");

	return;
}

static DWORD WINAPI rdpdr_virtual_channel_client_thread(LPVOID arg)
{
	wStream* data;
	wMessage message;
	rdpdrPlugin* rdpdr = (rdpdrPlugin*) arg;
	UINT error;

	if (!rdpdr)
	{
		ExitThread((DWORD) CHANNEL_RC_NULL_DATA);
		return CHANNEL_RC_NULL_DATA;
	}

	if ((error = rdpdr_process_connect(rdpdr)))
	{
		WLog_ERR(TAG, "rdpdr_process_connect failed with error %"PRIu32"!", error);

		if (rdpdr->rdpcontext)
			setChannelError(rdpdr->rdpcontext, error,
			                "rdpdr_virtual_channel_client_thread reported an error");

		ExitThread(error);
		return error;
	}

	while (1)
	{
		if (!MessageQueue_Wait(rdpdr->queue))
			break;

		if (MessageQueue_Peek(rdpdr->queue, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;

				if ((error = rdpdr_process_receive(rdpdr, data)))
				{
					WLog_ERR(TAG, "rdpdr_process_receive failed with error %"PRIu32"!", error);

					if (rdpdr->rdpcontext)
						setChannelError(rdpdr->rdpcontext, error,
						                "rdpdr_virtual_channel_client_thread reported an error");

					ExitThread((DWORD) error);
					return error;
				}
			}
		}
	}

	ExitThread(0);
	return 0;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_virtual_channel_event_connected(rdpdrPlugin* rdpdr,
        LPVOID pData, UINT32 dataLength)
{
	UINT32 status;
	status = rdpdr->channelEntryPoints.pVirtualChannelOpenEx(rdpdr->InitHandle,
	         &rdpdr->OpenHandle, rdpdr->channelDef.name, rdpdr_virtual_channel_open_event_ex);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "pVirtualChannelOpenEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);
		return status;
	}

	rdpdr->queue = MessageQueue_New(NULL);

	if (!rdpdr->queue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!(rdpdr->thread = CreateThread(NULL, 0,
	                                   rdpdr_virtual_channel_client_thread, (void*) rdpdr, 0,
	                                   NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_virtual_channel_event_disconnected(rdpdrPlugin* rdpdr)
{
	UINT error;

	if (rdpdr->OpenHandle == 0)
		return CHANNEL_RC_OK;

	if (MessageQueue_PostQuit(rdpdr->queue, 0)
	    && (WaitForSingleObject(rdpdr->thread, INFINITE) == WAIT_FAILED))
	{
		error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
		return error;
	}

	MessageQueue_Free(rdpdr->queue);
	CloseHandle(rdpdr->thread);
	rdpdr->queue = NULL;
	rdpdr->thread = NULL;

	if ((error = drive_hotplug_thread_terminate(rdpdr)))
	{
		WLog_ERR(TAG, "drive_hotplug_thread_terminate failed with error %"PRIu32"!", error);
		return error;
	}

	error = rdpdr->channelEntryPoints.pVirtualChannelCloseEx(rdpdr->InitHandle, rdpdr->OpenHandle);

	if (CHANNEL_RC_OK != error)
	{
		WLog_ERR(TAG, "pVirtualChannelCloseEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(error), error);
	}

	rdpdr->OpenHandle = 0;

	if (rdpdr->data_in)
	{
		Stream_Free(rdpdr->data_in, TRUE);
		rdpdr->data_in = NULL;
	}

	if (rdpdr->devman)
	{
		devman_free(rdpdr->devman);
		rdpdr->devman = NULL;
	}

	return error;
}

static void rdpdr_virtual_channel_event_terminated(rdpdrPlugin* rdpdr)
{
	rdpdr->InitHandle = 0;
	free(rdpdr);
}

static VOID VCAPITYPE rdpdr_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
        UINT event,
        LPVOID pData, UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	rdpdrPlugin* rdpdr = (rdpdrPlugin*) lpUserParam;

	if (!rdpdr || (rdpdr->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG, "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_INITIALIZED:
			break;

		case CHANNEL_EVENT_CONNECTED:
			if ((error = rdpdr_virtual_channel_event_connected(rdpdr, pData, dataLength)))
				WLog_ERR(TAG, "rdpdr_virtual_channel_event_connected failed with error %"PRIu32"!",
				         error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = rdpdr_virtual_channel_event_disconnected(rdpdr)))
				WLog_ERR(TAG, "rdpdr_virtual_channel_event_disconnected failed with error %"PRIu32"!",
				         error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			rdpdr_virtual_channel_event_terminated(rdpdr);
			break;

		case CHANNEL_EVENT_ATTACHED:
		case CHANNEL_EVENT_DETACHED:
		default:
			WLog_ERR(TAG, "unknown event %"PRIu32"!", event);
			break;
	}

	if (error && rdpdr->rdpcontext)
		setChannelError(rdpdr->rdpcontext, error,
		                "rdpdr_virtual_channel_init_event_ex reported an error");
}

/* rdpdr is always built-in */
#define VirtualChannelEntryEx	rdpdr_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	rdpdrPlugin* rdpdr;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;
	rdpdr = (rdpdrPlugin*) calloc(1, sizeof(rdpdrPlugin));

	if (!rdpdr)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	rdpdr->channelDef.options =
	    CHANNEL_OPTION_INITIALIZED |
	    CHANNEL_OPTION_ENCRYPT_RDP |
	    CHANNEL_OPTION_COMPRESS_RDP;
	sprintf_s(rdpdr->channelDef.name, ARRAYSIZE(rdpdr->channelDef.name), "rdpdr");
	rdpdr->sequenceId = 0;
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		rdpdr->rdpcontext = pEntryPointsEx->context;
	}

	CopyMemory(&(rdpdr->channelEntryPoints), pEntryPoints,
	           sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	rdpdr->InitHandle = pInitHandle;
	rc = rdpdr->channelEntryPoints.pVirtualChannelInitEx(rdpdr, NULL, pInitHandle,
	        &rdpdr->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	        rdpdr_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInitEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(rc), rc);
		free(rdpdr);
		return FALSE;
	}

	return TRUE;
}

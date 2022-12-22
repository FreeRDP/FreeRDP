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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/sysinfo.h>
#include <winpr/assert.h>
#include <winpr/stream.h>

#include <winpr/print.h>
#include <winpr/sspicli.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/utils/rdpdr_utils.h>

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

/* IMPORTANT: Keep in sync with DRIVE_DEVICE */
typedef struct
{
	DEVICE device;
	WCHAR* path;
	BOOL automount;
} DEVICE_DRIVE_EXT;

static const char* rdpdr_device_type_string(UINT32 type)
{
	switch (type)
	{
		case RDPDR_DTYP_SERIAL:
			return "serial";
		case RDPDR_DTYP_PRINT:
			return "printer";
		case RDPDR_DTYP_FILESYSTEM:
			return "drive";
		case RDPDR_DTYP_SMARTCARD:
			return "smartcard";
		case RDPDR_DTYP_PARALLEL:
			return "parallel";
		default:
			return "UNKNOWN";
	}
}

static BOOL device_foreach(rdpdrPlugin* rdpdr, BOOL abortOnFail,
                           BOOL (*fkt)(ULONG_PTR key, void* element, void* data), void* data)
{
	BOOL rc = TRUE;
	int count, x;
	ULONG_PTR* keys = NULL;

	ListDictionary_Lock(rdpdr->devman->devices);
	count = ListDictionary_GetKeys(rdpdr->devman->devices, &keys);
	for (x = 0; x < count; x++)
	{
		void* element = ListDictionary_GetItemValue(rdpdr->devman->devices, (void*)keys[x]);
		if (!fkt(keys[x], element, data))
		{
			rc = FALSE;
			if (abortOnFail)
				break;
		}
	}
	free(keys);
	ListDictionary_Unlock(rdpdr->devman->devices);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_send_device_list_announce_request(rdpdrPlugin* rdpdr, BOOL userLoggedOn);

static BOOL rdpdr_load_drive(rdpdrPlugin* rdpdr, const char* name, const char* path, BOOL automount)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	union
	{
		RDPDR_DRIVE* drive;
		RDPDR_DEVICE* device;
	} drive;
	const char* args[] = { name, path, automount ? NULL : name };

	drive.device = freerdp_device_new(RDPDR_DTYP_FILESYSTEM, ARRAYSIZE(args), args);
	if (!drive.device)
		goto fail;

	rc = devman_load_device_service(rdpdr->devman, drive.device, rdpdr->rdpcontext);
	if (rc != CHANNEL_RC_OK)
		goto fail;

fail:
	freerdp_device_free(drive.device);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_send_device_list_remove_request(rdpdrPlugin* rdpdr, UINT32 count, UINT32 ids[])
{
	UINT32 i;
	wStream* s;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(ids || (count == 0));

	if (count == 0)
		return CHANNEL_RC_OK;

	s = StreamPool_Take(rdpdr->pool, count * sizeof(UINT32) + 8);

	if (!s)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_New failed!");
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

#if defined(_UWP) || defined(__IOS__)

static void first_hotplug(rdpdrPlugin* rdpdr)
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

#elif defined(_WIN32)

static BOOL check_path(const char* path)
{
	UINT type = GetDriveTypeA(path);

	if (!(type == DRIVE_FIXED || type == DRIVE_REMOVABLE || type == DRIVE_CDROM ||
	      type == DRIVE_REMOTE))
		return FALSE;

	return GetVolumeInformationA(path, NULL, 0, NULL, NULL, NULL, NULL, 0);
}

static void first_hotplug(rdpdrPlugin* rdpdr)
{
	size_t i;
	DWORD unitmask = GetLogicalDrives();

	for (i = 0; i < 26; i++)
	{
		if (unitmask & 0x01)
		{
			char drive_path[] = { 'c', ':', '\\', '\0' };
			char drive_name[] = { 'c', '\0' };
			drive_path[0] = 'A' + (char)i;
			drive_name[0] = 'A' + (char)i;

			if (check_path(drive_path))
			{
				rdpdr_load_drive(rdpdr, drive_name, drive_path, TRUE);
			}
		}

		unitmask = unitmask >> 1;
	}
}

static LRESULT CALLBACK hotplug_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
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
					if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
					{
						PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lpdb;
						DWORD unitmask = lpdbv->dbcv_unitmask;
						int i;

						for (i = 0; i < 26; i++)
						{
							if (unitmask & 0x01)
							{
								char drive_path[] = { 'c', ':', '/', '\0' };
								char drive_name[] = { 'c', '\0' };
								drive_path[0] = 'A' + (char)i;
								drive_name[0] = 'A' + (char)i;

								if (check_path(drive_path))
								{
									rdpdr_load_drive(rdpdr, drive_name, drive_path, TRUE);
									rdpdr_send_device_list_announce_request(rdpdr, TRUE);
								}
							}

							unitmask = unitmask >> 1;
						}
					}

					break;

				case DBT_DEVICEREMOVECOMPLETE:
					if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
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

									if (device_ext->device.type != RDPDR_DTYP_FILESYSTEM)
										continue;

									if (device_ext->path[0] == drive_name_upper ||
									    device_ext->path[0] == drive_name_lower)
									{
										if (device_ext->automount)
										{
											devman_unregister_device(rdpdr->devman, (void*)keys[j]);
											ids[0] = keys[j];

											if ((error = rdpdr_send_device_list_remove_request(
											         rdpdr, 1, ids)))
											{
												// dont end on error, just report ?
												WLog_Print(
												    rdpdr->log, WLOG_ERROR,
												    "rdpdr_send_device_list_remove_request failed "
												    "with error %" PRIu32 "!",
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
	wnd_cls.cbSize = sizeof(WNDCLASSEX);
	wnd_cls.style = CS_HREDRAW | CS_VREDRAW;
	wnd_cls.lpfnWndProc = hotplug_proc;
	wnd_cls.cbClsExtra = 0;
	wnd_cls.cbWndExtra = 0;
	wnd_cls.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wnd_cls.hCursor = NULL;
	wnd_cls.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wnd_cls.lpszMenuName = NULL;
	wnd_cls.lpszClassName = L"DRIVE_HOTPLUG";
	wnd_cls.hInstance = NULL;
	wnd_cls.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&wnd_cls);
	/* create window */
	hwnd = CreateWindowEx(0, L"DRIVE_HOTPLUG", NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)rdpdr);
	rdpdr->hotplug_wnd = hwnd;
	/* register device interface to hwnd */
	NotificationFilter.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
	NotificationFilter.dbch_devicetype = DBT_DEVTYP_HANDLE;
	hDevNotify = RegisterDeviceNotification(hwnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

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
		WLog_Print(rdpdr->log, WLOG_ERROR, "PostMessage failed with error %" PRIu32 "", error);
	}

	return error;
}

#elif defined(__MACOSX__)

#define MAX_USB_DEVICES 100

typedef struct
{
	char* path;
	BOOL to_add;
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
			if (stat(fullpath, &buf) != 0)
				continue;

			if (S_ISDIR(buf.st_mode))
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
		device_ext =
		    (DEVICE_DRIVE_EXT*)ListDictionary_GetItemValue(rdpdr->devman->devices, (void*)keys[j]);

		if (!device_ext || !device_ext->automount)
			continue;

		if (device_ext->device.type != RDPDR_DTYP_FILESYSTEM)
			continue;

		if (device_ext->path == NULL)
			continue;

		path = ConvertWCharToUtf8Alloc(device_ext->path, NULL);
		if (!path)
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
				WLog_Print(rdpdr->log, WLOG_ERROR,
				           "rdpdr_send_device_list_remove_request failed with error %" PRIu32 "!",
				           error);
				goto cleanup;
			}
		}
	}

	/* add new devices */
	for (i = 0; i < size; i++)
	{
		const hotplug_dev* dev = &dev_array[i];
		if (dev->to_add)
		{
			const char* path = dev->path;
			const char* name = strrchr(path, '/') + 1;
			error = rdpdr_load_drive(rdpdr, name, path, TRUE);
			if (error)
				goto cleanup;
		}
	}

cleanup:
	free(keys);

	for (i = 0; i < size; i++)
		free(dev_array[i].path);

	return error;
}

static void drive_hotplug_fsevent_callback(ConstFSEventStreamRef streamRef,
                                           void* clientCallBackInfo, size_t numEvents,
                                           void* eventPaths,
                                           const FSEventStreamEventFlags eventFlags[],
                                           const FSEventStreamEventId eventIds[])
{
	rdpdrPlugin* rdpdr;
	size_t i;
	UINT error;
	char** paths = (char**)eventPaths;
	rdpdr = (rdpdrPlugin*)clientCallBackInfo;

	for (i = 0; i < numEvents; i++)
	{
		if (strcmp(paths[i], "/Volumes/") == 0)
		{
			if ((error = handle_hotplug(rdpdr)))
			{
				WLog_Print(rdpdr->log, WLOG_ERROR, "handle_hotplug failed with error %" PRIu32 "!",
				           error);
			}
			else
				rdpdr_send_device_list_announce_request(rdpdr, TRUE);

			return;
		}
	}
}

static void first_hotplug(rdpdrPlugin* rdpdr)
{
	UINT error;

	if ((error = handle_hotplug(rdpdr)))
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "handle_hotplug failed with error %" PRIu32 "!", error);
	}
}

static DWORD WINAPI drive_hotplug_thread_func(LPVOID arg)
{
	rdpdrPlugin* rdpdr;
	FSEventStreamRef fsev;
	rdpdr = (rdpdrPlugin*)arg;
	CFStringRef path = CFSTR("/Volumes/");
	CFArrayRef pathsToWatch = CFArrayCreate(kCFAllocatorMalloc, (const void**)&path, 1, NULL);
	FSEventStreamContext ctx = { 0 };

	ctx.info = arg;

	WINPR_ASSERT(!rdpdr->stopEvent);
	rdpdr->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!rdpdr->stopEvent)
		goto out;

	fsev =
	    FSEventStreamCreate(kCFAllocatorMalloc, drive_hotplug_fsevent_callback, &ctx, pathsToWatch,
	                        kFSEventStreamEventIdSinceNow, 1, kFSEventStreamCreateFlagNone);

	rdpdr->runLoop = CFRunLoopGetCurrent();
	FSEventStreamScheduleWithRunLoop(fsev, rdpdr->runLoop, kCFRunLoopDefaultMode);
	FSEventStreamStart(fsev);
	CFRunLoopRun();
	FSEventStreamStop(fsev);
	FSEventStreamRelease(fsev);
out:
	if (rdpdr->stopEvent)
	{
		CloseHandle(rdpdr->stopEvent);
		rdpdr->stopEvent = NULL;
	}
	ExitThread(CHANNEL_RC_OK);
	return CHANNEL_RC_OK;
}

#else

static const char* automountLocations[] = { "/run/user/%lu/gvfs", "/run/media/%s", "/media/%s",
	                                        "/media", "/mnt" };

static BOOL isAutomountLocation(const char* path)
{
	const size_t nrLocations = sizeof(automountLocations) / sizeof(automountLocations[0]);
	size_t x;
	char buffer[MAX_PATH] = { 0 };
	uid_t uid = getuid();
	char uname[MAX_PATH] = { 0 };
	ULONG size = sizeof(uname) - 1;

	if (!GetUserNameExA(NameSamCompatible, uname, &size))
		return FALSE;

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

#define MAX_USB_DEVICES 100

typedef struct
{
	char* path;
	BOOL to_add;
} hotplug_dev;

static void handle_mountpoint(hotplug_dev* dev_array, size_t* size, const char* mountpoint)
{
	if (!mountpoint)
		return;
	/* copy hotpluged device mount point to the dev_array */
	if (isAutomountLocation(mountpoint) && (*size < MAX_USB_DEVICES))
	{
		dev_array[*size].path = _strdup(mountpoint);
		dev_array[*size].to_add = TRUE;
		(*size)++;
	}
}

#ifdef __sun
#include <sys/mnttab.h>
static UINT handle_platform_mounts_sun(wLog* log, hotplug_dev* dev_array, size_t* size)
{
	FILE* f;
	struct mnttab ent;
	f = winpr_fopen("/etc/mnttab", "r");
	if (f == NULL)
	{
		WLog_Print(log, WLOG_ERROR, "fopen failed!");
		return ERROR_OPEN_FAILED;
	}
	while (getmntent(f, &ent) == 0)
	{
		handle_mountpoint(dev_array, size, ent.mnt_mountp);
	}
	fclose(f);
	return ERROR_SUCCESS;
}
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <sys/mount.h>
static UINT handle_platform_mounts_bsd(wLog* log, hotplug_dev* dev_array, size_t* size)
{
	int mntsize;
	size_t idx;
	struct statfs* mntbuf = NULL;

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	if (!mntsize)
	{
		/* TODO: handle 'errno' */
		WLog_Print(log, WLOG_ERROR, "getmntinfo failed!");
		return ERROR_OPEN_FAILED;
	}
	for (idx = 0; idx < (size_t)mntsize; idx++)
	{
		handle_mountpoint(dev_array, size, mntbuf[idx].f_mntonname);
	}
	free(mntbuf);
	return ERROR_SUCCESS;
}
#endif

#if defined(__LINUX__) || defined(__linux__)
#include <mntent.h>
static UINT handle_platform_mounts_linux(wLog* log, hotplug_dev* dev_array, size_t* size)
{
	FILE* f;
	struct mntent* ent;
	f = winpr_fopen("/proc/mounts", "r");
	if (f == NULL)
	{
		WLog_Print(log, WLOG_ERROR, "fopen failed!");
		return ERROR_OPEN_FAILED;
	}
	while ((ent = getmntent(f)) != NULL)
	{
		handle_mountpoint(dev_array, size, ent->mnt_dir);
	}
	fclose(f);
	return ERROR_SUCCESS;
}
#endif

static UINT handle_platform_mounts(wLog* log, hotplug_dev* dev_array, size_t* size)
{
#ifdef __sun
	return handle_platform_mounts_sun(log, dev_array, size);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	return handle_platform_mounts_bsd(log, dev_array, size);
#elif defined(__LINUX__) || defined(__linux__)
	return handle_platform_mounts_linux(log, dev_array, size);
#endif
	return ERROR_CALL_NOT_IMPLEMENTED;
}

static BOOL device_not_plugged(ULONG_PTR key, void* element, void* data)
{
	const WCHAR* path = (const WCHAR*)data;
	DEVICE_DRIVE_EXT* device_ext = (DEVICE_DRIVE_EXT*)element;

	WINPR_UNUSED(key);
	WINPR_ASSERT(path);

	if (!device_ext || (device_ext->device.type != RDPDR_DTYP_FILESYSTEM) || !device_ext->path)
		return TRUE;
	if (_wcscmp(device_ext->path, path) != 0)
		return TRUE;
	return FALSE;
}

static BOOL device_already_plugged(rdpdrPlugin* rdpdr, const hotplug_dev* device)
{
	BOOL rc = FALSE;
	WCHAR* path = NULL;

	if (!rdpdr || !device)
		return TRUE;
	if (!device->to_add)
		return TRUE;

	WINPR_ASSERT(rdpdr->devman);
	WINPR_ASSERT(device->path);

	path = ConvertUtf8ToWCharAlloc(device->path, NULL);
	if (!path)
		return TRUE;

	rc = device_foreach(rdpdr, TRUE, device_not_plugged, path);
	free(path);
	return !rc;
}

struct hotplug_delete_arg
{
	hotplug_dev* dev_array;
	size_t dev_array_size;
	rdpdrPlugin* rdpdr;
};

static BOOL hotplug_delete_foreach(ULONG_PTR key, void* element, void* data)
{
	char* path = NULL;
	BOOL dev_found = FALSE;
	struct hotplug_delete_arg* arg = (struct hotplug_delete_arg*)data;
	DEVICE_DRIVE_EXT* device_ext = (DEVICE_DRIVE_EXT*)element;

	WINPR_ASSERT(arg);
	WINPR_ASSERT(arg->rdpdr);
	WINPR_ASSERT(arg->dev_array || (arg->dev_array_size == 0));

	if (!device_ext || (device_ext->device.type != RDPDR_DTYP_FILESYSTEM) || !device_ext->path ||
	    !device_ext->automount)
		return TRUE;

	WINPR_ASSERT(device_ext->path);
	path = ConvertWCharToUtf8Alloc(device_ext->path, NULL);
	if (!path)
		return FALSE;

	/* not plugable device */
	if (isAutomountLocation(path))
	{
		size_t i;
		for (i = 0; i < arg->dev_array_size; i++)
		{
			hotplug_dev* cur = &arg->dev_array[i];
			if (strstr(path, cur->path) != NULL)
			{
				dev_found = TRUE;
				cur->to_add = FALSE;
				break;
			}
		}
	}

	free(path);

	if (!dev_found)
	{
		UINT error;
		UINT32 ids[1] = { key };

		WINPR_ASSERT(arg->rdpdr->devman);
		devman_unregister_device(arg->rdpdr->devman, (void*)key);
		WINPR_ASSERT(key <= UINT32_MAX);

		error = rdpdr_send_device_list_remove_request(arg->rdpdr, 1, ids);
		if (error)
		{
			WLog_Print(arg->rdpdr->log, WLOG_ERROR,
			           "rdpdr_send_device_list_remove_request failed with error %" PRIu32 "!",
			           error);
			return FALSE;
		}
	}

	return TRUE;
}

static UINT handle_hotplug(rdpdrPlugin* rdpdr)
{
	hotplug_dev dev_array[MAX_USB_DEVICES] = { 0 };
	size_t i;
	size_t size = 0;
	UINT error = ERROR_SUCCESS;
	struct hotplug_delete_arg arg = { dev_array, ARRAYSIZE(dev_array), rdpdr };

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(rdpdr->devman);

	error = handle_platform_mounts(rdpdr->log, dev_array, &size);

	/* delete removed devices */
	/* Ignore result */ device_foreach(rdpdr, FALSE, hotplug_delete_foreach, &arg);

	/* add new devices */
	for (i = 0; i < size; i++)
	{
		hotplug_dev* cur = &dev_array[i];
		if (!device_already_plugged(rdpdr, cur))
		{
			const char* path = cur->path;
			const char* name = strrchr(path, '/') + 1;

			rdpdr_load_drive(rdpdr, name, path, TRUE);
			error = ERROR_DISK_CHANGE;
		}
	}

	for (i = 0; i < size; i++)
		free(dev_array[i].path);

	return error;
}

static void first_hotplug(rdpdrPlugin* rdpdr)
{
	UINT error;

	WINPR_ASSERT(rdpdr);
	if ((error = handle_hotplug(rdpdr)))
	{
		switch (error)
		{
			case ERROR_DISK_CHANGE:
			case CHANNEL_RC_OK:
			case ERROR_OPEN_FAILED:
			case ERROR_CALL_NOT_IMPLEMENTED:
				break;
			default:
				WLog_Print(rdpdr->log, WLOG_ERROR, "handle_hotplug failed with error %" PRIu32 "!",
				           error);
				break;
		}
	}
}

static DWORD WINAPI drive_hotplug_thread_func(LPVOID arg)
{
	rdpdrPlugin* rdpdr;
	UINT error = 0;
	rdpdr = (rdpdrPlugin*)arg;

	WINPR_ASSERT(rdpdr);

	WINPR_ASSERT(!rdpdr->stopEvent);
	rdpdr->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!rdpdr->stopEvent)
		goto out;

	while (WaitForSingleObject(rdpdr->stopEvent, 1000) == WAIT_TIMEOUT)
	{
		error = handle_hotplug(rdpdr);
		switch (error)
		{
			case ERROR_DISK_CHANGE:
				rdpdr_send_device_list_announce_request(rdpdr, TRUE);
				break;
			case CHANNEL_RC_OK:
			case ERROR_OPEN_FAILED:
			case ERROR_CALL_NOT_IMPLEMENTED:
				break;
			default:
				WLog_Print(rdpdr->log, WLOG_ERROR, "handle_hotplug failed with error %" PRIu32 "!",
				           error);
				goto out;
		}
	}

out:
	error = GetLastError();
	if (error && rdpdr->rdpcontext)
		setChannelError(rdpdr->rdpcontext, error, "%s reported an error", __FUNCTION__);

	if (rdpdr->stopEvent)
	{
		CloseHandle(rdpdr->stopEvent);
		rdpdr->stopEvent = NULL;
	}

	ExitThread(error);
	return error;
}

#endif

#if !defined(_WIN32) && !defined(__IOS__)
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drive_hotplug_thread_terminate(rdpdrPlugin* rdpdr)
{
	UINT error;

	WINPR_ASSERT(rdpdr);

	if (rdpdr->hotplugThread)
	{
#if !defined(_WIN32)
		if (rdpdr->stopEvent)
			SetEvent(rdpdr->stopEvent);
#endif
#ifdef __MACOSX__
		CFRunLoopStop(rdpdr->runLoop);
#endif

		if (WaitForSingleObject(rdpdr->hotplugThread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(rdpdr->log, WLOG_ERROR, "WaitForSingleObject failed with error %" PRIu32 "!",
			           error);
			return error;
		}

		CloseHandle(rdpdr->hotplugThread);
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
	rdpSettings* settings;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(rdpdr);

	rdpdr->devman = devman_new(rdpdr);

	if (!rdpdr->devman)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "devman_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	WINPR_ASSERT(rdpdr->rdpcontext);
	settings = rdpdr->rdpcontext->settings;
	WINPR_ASSERT(settings);

	if (settings->ClientHostname)
		strncpy(rdpdr->computerName, settings->ClientHostname, sizeof(rdpdr->computerName) - 1);
	else
		strncpy(rdpdr->computerName, settings->ComputerName, sizeof(rdpdr->computerName) - 1);

	for (index = 0; index < settings->DeviceCount; index++)
	{
		const RDPDR_DEVICE* device = settings->DeviceArray[index];

		if (device->Type == RDPDR_DTYP_FILESYSTEM)
		{
			const char DynamicDrives[] = "DynamicDrives";
			const RDPDR_DRIVE* drive = (const RDPDR_DRIVE*)device;
			BOOL hotplugAll = strncmp(drive->Path, "*", 2) == 0;
			BOOL hotplugLater = strncmp(drive->Path, DynamicDrives, sizeof(DynamicDrives)) == 0;

			if (drive->Path && (hotplugAll || hotplugLater))
			{
				if (hotplugAll)
					first_hotplug(rdpdr);

				/* There might be multiple hotplug related device entries.
				 * Ensure the thread is only started once
				 */
				if (!rdpdr->hotplugThread)
				{
					rdpdr->hotplugThread =
					    CreateThread(NULL, 0, drive_hotplug_thread_func, rdpdr, 0, NULL);
					if (!rdpdr->hotplugThread)
					{
						WLog_Print(rdpdr->log, WLOG_ERROR, "CreateThread failed!");
						return ERROR_INTERNAL_ERROR;
					}
				}

				continue;
			}
		}

		if ((error = devman_load_device_service(rdpdr->devman, device, rdpdr->rdpcontext)))
		{
			WLog_Print(rdpdr->log, WLOG_ERROR,
			           "devman_load_device_service failed with error %" PRIu32 "!", error);
			return error;
		}
	}

	return error;
}

static UINT rdpdr_process_server_announce_request(rdpdrPlugin* rdpdr, wStream* s)
{
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLengthWLog(rdpdr->log, s, 8))
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

	WINPR_ASSERT(rdpdr);

	s = StreamPool_Take(rdpdr->pool, 12);

	if (!s)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);             /* Component (2 bytes) */
	Stream_Write_UINT16(s, PAKID_CORE_CLIENTID_CONFIRM); /* PacketId (2 bytes) */
	Stream_Write_UINT16(s, rdpdr->versionMajor);
	Stream_Write_UINT16(s, rdpdr->versionMinor);
	Stream_Write_UINT32(s, (UINT32)rdpdr->clientID);
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

	WINPR_ASSERT(rdpdr);

	if (!rdpdr->computerName[0])
	{
		DWORD size = sizeof(rdpdr->computerName) - 1;
		GetComputerNameA(rdpdr->computerName, &size);
	}

	WINPR_ASSERT(rdpdr->computerName);
	computerNameW = ConvertUtf8ToWCharAlloc(rdpdr->computerName, &computerNameLenW);
	computerNameLenW *= sizeof(WCHAR);
	WINPR_ASSERT(computerNameLenW >= 0);

	if (computerNameLenW > 0)
		computerNameLenW += sizeof(WCHAR); // also write '\0'

	s = StreamPool_Take(rdpdr->pool, 16U + computerNameLenW);

	if (!s)
	{
		free(computerNameW);
		WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);        /* Component (2 bytes) */
	Stream_Write_UINT16(s, PAKID_CORE_CLIENT_NAME); /* PacketId (2 bytes) */
	Stream_Write_UINT32(s, 1);                      /* unicodeFlag, 0 for ASCII and 1 for Unicode */
	Stream_Write_UINT32(s, 0);                      /* codePage, must be set to zero */
	Stream_Write_UINT32(s,
	                    (UINT32)computerNameLenW); /* computerNameLen, including null terminator */
	Stream_Write(s, computerNameW, (size_t)computerNameLenW);
	free(computerNameW);
	return rdpdr_send(rdpdr, s);
}

static UINT rdpdr_process_server_clientid_confirm(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 versionMajor;
	UINT16 versionMinor;
	UINT32 clientID;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLengthWLog(rdpdr->log, s, 8))
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

struct device_announce_arg
{
	rdpdrPlugin* rdpdr;
	wStream* s;
	BOOL userLoggedOn;
	UINT32 count;
};

static BOOL device_announce(ULONG_PTR key, void* element, void* data)
{
	struct device_announce_arg* arg = data;
	rdpdrPlugin* rdpdr;
	DEVICE* device = (DEVICE*)element;

	WINPR_UNUSED(key);

	WINPR_ASSERT(arg);
	WINPR_ASSERT(device);
	WINPR_ASSERT(arg->rdpdr);
	WINPR_ASSERT(arg->s);

	rdpdr = arg->rdpdr;

	/**
	 * 1. versionMinor 0x0005 doesn't send PAKID_CORE_USER_LOGGEDON
	 *    so all devices should be sent regardless of user_loggedon
	 * 2. smartcard devices should be always sent
	 * 3. other devices are sent only after user_loggedon
	 */

	if ((rdpdr->versionMinor == 0x0005) || (device->type == RDPDR_DTYP_SMARTCARD) ||
	    arg->userLoggedOn)
	{
		size_t i;
		size_t data_len = (device->data == NULL ? 0 : Stream_GetPosition(device->data));

		if (!Stream_EnsureRemainingCapacity(arg->s, 20 + data_len))
		{
			Stream_Release(arg->s);
			WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
			return FALSE;
		}

		Stream_Write_UINT32(arg->s, device->type); /* deviceType */
		Stream_Write_UINT32(arg->s, device->id);   /* deviceID */
		strncpy((char*)Stream_Pointer(arg->s), device->name, 8);

		for (i = 0; i < 8; i++)
		{
			BYTE c;
			Stream_Peek_UINT8(arg->s, c);

			if (c > 0x7F)
				Stream_Write_UINT8(arg->s, '_');
			else
				Stream_Seek_UINT8(arg->s);
		}

		WINPR_ASSERT(data_len <= UINT32_MAX);
		Stream_Write_UINT32(arg->s, (UINT32)data_len);

		if (data_len > 0)
			Stream_Write(arg->s, Stream_Buffer(device->data), data_len);

		arg->count++;
		WLog_Print(rdpdr->log, WLOG_INFO,
		           "registered [%09s] device #%" PRIu32 ": %s (type=%" PRIu32 " id=%" PRIu32 ")",
		           rdpdr_device_type_string(device->type), arg->count, device->name, device->type,
		           device->id);
	}
	return TRUE;
}

static UINT rdpdr_send_device_list_announce_request(rdpdrPlugin* rdpdr, BOOL userLoggedOn)
{
	size_t pos;
	wStream* s;
	size_t count_pos;
	struct device_announce_arg arg = { 0 };

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(rdpdr->devman);

	s = StreamPool_Take(rdpdr->pool, 256);

	if (!s)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);                /* Component (2 bytes) */
	Stream_Write_UINT16(s, PAKID_CORE_DEVICELIST_ANNOUNCE); /* PacketId (2 bytes) */
	count_pos = Stream_GetPosition(s);
	Stream_Seek_UINT32(s); /* deviceCount */

	arg.rdpdr = rdpdr;
	arg.userLoggedOn = userLoggedOn;
	arg.s = s;
	if (!device_foreach(rdpdr, TRUE, device_announce, &arg))
		return ERROR_INVALID_DATA;

	if (arg.count == 0)
	{
		Stream_Release(s);
		return CHANNEL_RC_OK;
	}
	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, count_pos);
	Stream_Write_UINT32(s, arg.count);
	Stream_SetPosition(s, pos);
	Stream_SealLength(s);
	return rdpdr_send(rdpdr, s);
}

static UINT dummy_irp_response(rdpdrPlugin* rdpdr, wStream* s)
{
	wStream* output;
	UINT32 DeviceId;
	UINT32 FileId;
	UINT32 CompletionId;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	output = StreamPool_Take(rdpdr->pool, 256); // RDPDR_DEVICE_IO_RESPONSE_LENGTH
	if (!output)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_SetPosition(s, 4); /* see "rdpdr_process_receive" */

	Stream_Read_UINT32(s, DeviceId);     /* DeviceId (4 bytes) */
	Stream_Read_UINT32(s, FileId);       /* FileId (4 bytes) */
	Stream_Read_UINT32(s, CompletionId); /* CompletionId (4 bytes) */

	if (!rdpdr_write_iocompletion_header(output, DeviceId, CompletionId,
	                                     (UINT32)STATUS_UNSUCCESSFUL))
		return CHANNEL_RC_NO_MEMORY;

	return rdpdr_send(rdpdr, output);
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

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	irp = irp_new(rdpdr->devman, rdpdr->pool, s, rdpdr->log, &error);

	if (!irp)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "irp_new failed with %" PRIu32 "!", error);

		if (error == CHANNEL_RC_OK)
		{
			return dummy_irp_response(rdpdr, s);
		}

		return error;
	}

	if (irp->device->IRPRequest)
		IFCALLRET(irp->device->IRPRequest, error, irp->device, irp);
	else
		irp->Discard(irp);

	if (error != CHANNEL_RC_OK)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "device->IRPRequest failed with error %" PRIu32 "",
		           error);
		irp->Discard(irp);
	}

	return error;
}

static UINT rdpdr_process_component(rdpdrPlugin* rdpdr, UINT16 component, UINT16 packetId,
                                    wStream* s)
{
	UINT32 type;
	DEVICE* device;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(s);

	switch (component)
	{
		case RDPDR_CTYP_PRN:
			type = RDPDR_DTYP_PRINT;
			break;

		default:
			return ERROR_INVALID_DATA;
	}

	device = devman_get_device_by_type(rdpdr->devman, type);

	if (!device)
		return ERROR_INVALID_PARAMETER;

	return IFCALLRESULT(ERROR_INVALID_PARAMETER, device->CustomComponentRequest, device, component,
	                    packetId, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static BOOL device_init(ULONG_PTR key, void* element, void* data)
{
	wLog* log = data;
	UINT error = CHANNEL_RC_OK;
	DEVICE* device = element;

	WINPR_UNUSED(key);
	WINPR_UNUSED(data);

	IFCALLRET(device->Init, error, device);

	if (error != CHANNEL_RC_OK)
	{
		WLog_Print(log, WLOG_ERROR, "Device init failed with %s", WTSErrorToString(error));
		return FALSE;
	}
	return TRUE;
}

static UINT rdpdr_process_init(rdpdrPlugin* rdpdr)
{
	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(rdpdr->devman);

	if (!device_foreach(rdpdr, TRUE, device_init, rdpdr->log))
		return ERROR_INTERNAL_ERROR;
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
	UINT error = ERROR_INVALID_DATA;

	if (!rdpdr || !s)
		return CHANNEL_RC_NULL_DATA;

	rdpdr_dump_received_packet(rdpdr->log, WLOG_TRACE, s, "[rdpdr-channel] receive");
	if (Stream_GetRemainingLength(s) >= 4)
	{
		Stream_Read_UINT16(s, component); /* Component (2 bytes) */
		Stream_Read_UINT16(s, packetId);  /* PacketId (2 bytes) */

		if (component == RDPDR_CTYP_CORE)
		{
			switch (packetId)
			{
				case PAKID_CORE_SERVER_ANNOUNCE:
					if ((error = rdpdr_process_server_announce_request(rdpdr, s)))
					{
					}
					else if ((error = rdpdr_send_client_announce_reply(rdpdr)))
					{
						WLog_Print(rdpdr->log, WLOG_ERROR,
						           "rdpdr_send_client_announce_reply failed with error %" PRIu32 "",
						           error);
					}
					else if ((error = rdpdr_send_client_name_request(rdpdr)))
					{
						WLog_Print(rdpdr->log, WLOG_ERROR,
						           "rdpdr_send_client_name_request failed with error %" PRIu32 "",
						           error);
					}
					else if ((error = rdpdr_process_init(rdpdr)))
					{
						WLog_Print(rdpdr->log, WLOG_ERROR,
						           "rdpdr_process_init failed with error %" PRIu32 "", error);
					}

					break;

				case PAKID_CORE_SERVER_CAPABILITY:
					if ((error = rdpdr_process_capability_request(rdpdr, s)))
					{
					}
					else if ((error = rdpdr_send_capability_response(rdpdr)))
					{
						WLog_Print(rdpdr->log, WLOG_ERROR,
						           "rdpdr_send_capability_response failed with error %" PRIu32 "",
						           error);
					}

					break;

				case PAKID_CORE_CLIENTID_CONFIRM:
					if ((error = rdpdr_process_server_clientid_confirm(rdpdr, s)))
					{
					}
					else if ((error = rdpdr_send_device_list_announce_request(rdpdr, FALSE)))
					{
						WLog_Print(
						    rdpdr->log, WLOG_ERROR,
						    "rdpdr_send_device_list_announce_request failed with error %" PRIu32 "",
						    error);
					}

					break;

				case PAKID_CORE_USER_LOGGEDON:
					if ((error = rdpdr_send_device_list_announce_request(rdpdr, TRUE)))
					{
						WLog_Print(
						    rdpdr->log, WLOG_ERROR,
						    "rdpdr_send_device_list_announce_request failed with error %" PRIu32 "",
						    error);
					}

					break;

				case PAKID_CORE_DEVICE_REPLY:

					/* connect to a specific resource */
					if (Stream_GetRemainingLength(s) >= 8)
					{
						Stream_Read_UINT32(s, deviceId);
						Stream_Read_UINT32(s, status);

						if (status != 0)
							devman_unregister_device(rdpdr->devman, (void*)((size_t)deviceId));
						error = CHANNEL_RC_OK;
					}

					break;

				case PAKID_CORE_DEVICE_IOREQUEST:
					if ((error = rdpdr_process_irp(rdpdr, s)))
					{
						WLog_Print(rdpdr->log, WLOG_ERROR,
						           "rdpdr_process_irp failed with error %" PRIu32 "", error);
						return error;
					}
					else
						s = NULL;

					break;

				default:
					WLog_Print(rdpdr->log, WLOG_ERROR,
					           "RDPDR_CTYP_CORE unknown PacketId: 0x%04" PRIX16 "", packetId);
					error = ERROR_INVALID_DATA;
					break;
			}
		}
		else
		{
			error = rdpdr_process_component(rdpdr, component, packetId, s);

			if (error != CHANNEL_RC_OK)
			{
				WLog_Print(rdpdr->log, WLOG_ERROR,
				           "Unknown message: Component: %s [0x%04" PRIX16
				           "] PacketId: %s [0x%04" PRIX16 "]",
				           rdpdr_component_string(component), component,
				           rdpdr_packetid_string(packetId), packetId);
			}
		}
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpdr_send(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT status;
	rdpdrPlugin* plugin = (rdpdrPlugin*)rdpdr;

	if (!rdpdr || !s)
	{
		Stream_Release(s);
		return CHANNEL_RC_NULL_DATA;
	}

	if (!plugin)
	{
		Stream_Release(s);
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		const size_t pos = Stream_GetPosition(s);
		rdpdr_dump_send_packet(rdpdr->log, WLOG_TRACE, s, "[rdpdr-channel] send");
		status = plugin->channelEntryPoints.pVirtualChannelWriteEx(
		    plugin->InitHandle, plugin->OpenHandle, Stream_Buffer(s), pos, s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Release(s);
		WLog_Print(rdpdr->log, WLOG_ERROR, "pVirtualChannelWriteEx failed with %s [%08" PRIX32 "]",
		           WTSErrorToString(status), status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_virtual_channel_event_data_received(rdpdrPlugin* rdpdr, void* pData,
                                                      UINT32 dataLength, UINT32 totalLength,
                                                      UINT32 dataFlags)
{
	wStream* data_in;

	WINPR_ASSERT(rdpdr);
	WINPR_ASSERT(pData || (dataLength == 0));

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
			Stream_Release(rdpdr->data_in);

		rdpdr->data_in = StreamPool_Take(rdpdr->pool, totalLength);

		if (!rdpdr->data_in)
		{
			WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_New failed!");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	data_in = rdpdr->data_in;

	if (!Stream_EnsureRemainingCapacity(data_in, dataLength))
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		const size_t pos = Stream_GetPosition(data_in);
		const size_t cap = Stream_Capacity(data_in);
		if (cap < pos)
		{
			WLog_Print(rdpdr->log, WLOG_ERROR,
			           "rdpdr_virtual_channel_event_data_received: read error");
			return ERROR_INTERNAL_ERROR;
		}

		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (!MessageQueue_Post(rdpdr->queue, NULL, 0, (void*)data_in, NULL))
		{
			WLog_Print(rdpdr->log, WLOG_ERROR, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
		rdpdr->data_in = NULL;
	}

	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE rdpdr_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
                                                          UINT event, LPVOID pData,
                                                          UINT32 dataLength, UINT32 totalLength,
                                                          UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	rdpdrPlugin* rdpdr = (rdpdrPlugin*)lpUserParam;

	WINPR_ASSERT(rdpdr);
	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if (!rdpdr || !pData || (rdpdr->OpenHandle != openHandle))
			{
				WLog_Print(rdpdr->log, WLOG_ERROR, "error no match");
				return;
			}
			if ((error = rdpdr_virtual_channel_event_data_received(rdpdr, pData, dataLength,
			                                                       totalLength, dataFlags)))
				WLog_Print(rdpdr->log, WLOG_ERROR,
				           "rdpdr_virtual_channel_event_data_received failed with error %" PRIu32
				           "!",
				           error);

			break;

		case CHANNEL_EVENT_WRITE_CANCELLED:
		case CHANNEL_EVENT_WRITE_COMPLETE:
		{
			wStream* s = (wStream*)pData;
			Stream_Release(s);
		}
		break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && rdpdr && rdpdr->rdpcontext)
		setChannelError(rdpdr->rdpcontext, error,
		                "rdpdr_virtual_channel_open_event_ex reported an error");

	return;
}

static DWORD WINAPI rdpdr_virtual_channel_client_thread(LPVOID arg)
{
	rdpdrPlugin* rdpdr = (rdpdrPlugin*)arg;
	UINT error;

	if (!rdpdr)
	{
		ExitThread((DWORD)CHANNEL_RC_NULL_DATA);
		return CHANNEL_RC_NULL_DATA;
	}

	if ((error = rdpdr_process_connect(rdpdr)))
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "rdpdr_process_connect failed with error %" PRIu32 "!",
		           error);

		if (rdpdr->rdpcontext)
			setChannelError(rdpdr->rdpcontext, error,
			                "rdpdr_virtual_channel_client_thread reported an error");

		ExitThread(error);
		return error;
	}

	while (1)
	{
		wMessage message = { 0 };
		WINPR_ASSERT(rdpdr);

		if (!MessageQueue_Wait(rdpdr->queue))
			break;

		if (MessageQueue_Peek(rdpdr->queue, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				wStream* data = (wStream*)message.wParam;

				error = rdpdr_process_receive(rdpdr, data);

				Stream_Release(data);
				if (error)
				{
					WLog_Print(rdpdr->log, WLOG_ERROR,
					           "rdpdr_process_receive failed with error %" PRIu32 "!", error);

					if (rdpdr->rdpcontext)
						setChannelError(rdpdr->rdpcontext, error,
						                "rdpdr_virtual_channel_client_thread reported an error");

					goto fail;
				}
			}
		}
	}

fail:
	if ((error = drive_hotplug_thread_terminate(rdpdr)))
		WLog_Print(rdpdr->log, WLOG_ERROR,
		           "drive_hotplug_thread_terminate failed with error %" PRIu32 "!", error);

	ExitThread(error);
	return error;
}

static void queue_free(void* obj)
{
	wStream* s;
	wMessage* msg = (wMessage*)obj;

	if (!msg || (msg->id != 0))
		return;

	s = (wStream*)msg->wParam;
	WINPR_ASSERT(s);
	Stream_Release(s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_virtual_channel_event_connected(rdpdrPlugin* rdpdr, LPVOID pData,
                                                  UINT32 dataLength)
{
	wObject* obj;

	WINPR_ASSERT(rdpdr);
	WINPR_UNUSED(pData);
	WINPR_UNUSED(dataLength);

	rdpdr->queue = MessageQueue_New(NULL);

	if (!rdpdr->queue)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "MessageQueue_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	obj = MessageQueue_Object(rdpdr->queue);
	obj->fnObjectFree = queue_free;

	if (!(rdpdr->thread =
	          CreateThread(NULL, 0, rdpdr_virtual_channel_client_thread, (void*)rdpdr, 0, NULL)))
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "CreateThread failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return rdpdr->channelEntryPoints.pVirtualChannelOpenEx(rdpdr->InitHandle, &rdpdr->OpenHandle,
	                                                       rdpdr->channelDef.name,
	                                                       rdpdr_virtual_channel_open_event_ex);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpdr_virtual_channel_event_disconnected(rdpdrPlugin* rdpdr)
{
	UINT error;

	WINPR_ASSERT(rdpdr);

	if (rdpdr->OpenHandle == 0)
		return CHANNEL_RC_OK;

	if (rdpdr->queue && rdpdr->thread)
	{
		if (MessageQueue_PostQuit(rdpdr->queue, 0) &&
		    (WaitForSingleObject(rdpdr->thread, INFINITE) == WAIT_FAILED))
		{
			error = GetLastError();
			WLog_Print(rdpdr->log, WLOG_ERROR, "WaitForSingleObject failed with error %" PRIu32 "!",
			           error);
			return error;
		}
	}

	if (rdpdr->thread)
		CloseHandle(rdpdr->thread);
	MessageQueue_Free(rdpdr->queue);
	rdpdr->queue = NULL;
	rdpdr->thread = NULL;

	WINPR_ASSERT(rdpdr->channelEntryPoints.pVirtualChannelCloseEx);
	error = rdpdr->channelEntryPoints.pVirtualChannelCloseEx(rdpdr->InitHandle, rdpdr->OpenHandle);

	if (CHANNEL_RC_OK != error)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "pVirtualChannelCloseEx failed with %s [%08" PRIX32 "]",
		           WTSErrorToString(error), error);
	}

	rdpdr->OpenHandle = 0;

	if (rdpdr->data_in)
	{
		Stream_Release(rdpdr->data_in);
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
	WINPR_ASSERT(rdpdr);
	rdpdr->InitHandle = 0;
	StreamPool_Free(rdpdr->pool);
	free(rdpdr);
}

static VOID VCAPITYPE rdpdr_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
                                                          UINT event, LPVOID pData, UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	rdpdrPlugin* rdpdr = (rdpdrPlugin*)lpUserParam;

	if (!rdpdr || (rdpdr->InitHandle != pInitHandle))
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "error no match");
		return;
	}

	WINPR_ASSERT(pData || (dataLength == 0));

	switch (event)
	{
		case CHANNEL_EVENT_INITIALIZED:
			break;

		case CHANNEL_EVENT_CONNECTED:
			if ((error = rdpdr_virtual_channel_event_connected(rdpdr, pData, dataLength)))
				WLog_Print(rdpdr->log, WLOG_ERROR,
				           "rdpdr_virtual_channel_event_connected failed with error %" PRIu32 "!",
				           error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = rdpdr_virtual_channel_event_disconnected(rdpdr)))
				WLog_Print(rdpdr->log, WLOG_ERROR,
				           "rdpdr_virtual_channel_event_disconnected failed with error %" PRIu32
				           "!",
				           error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			rdpdr_virtual_channel_event_terminated(rdpdr);
			rdpdr = NULL;
			break;

		case CHANNEL_EVENT_ATTACHED:
		case CHANNEL_EVENT_DETACHED:
		default:
			WLog_Print(rdpdr->log, WLOG_ERROR, "unknown event %" PRIu32 "!", event);
			break;
	}

	if (error && rdpdr && rdpdr->rdpcontext)
		setChannelError(rdpdr->rdpcontext, error,
		                "rdpdr_virtual_channel_init_event_ex reported an error");
}

/* rdpdr is always built-in */
#define TAG CHANNELS_TAG("rdpdr.client")
#define VirtualChannelEntryEx rdpdr_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	rdpdrPlugin* rdpdr;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;

	WINPR_ASSERT(pEntryPoints);
	WINPR_ASSERT(pInitHandle);

	rdpdr = (rdpdrPlugin*)calloc(1, sizeof(rdpdrPlugin));

	if (!rdpdr)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "calloc failed!");
		return FALSE;
	}
	rdpdr->log = WLog_Get(TAG);

	rdpdr->pool = StreamPool_New(TRUE, 1024);
	if (!rdpdr->pool)
	{
		free(rdpdr);
		return FALSE;
	}

	rdpdr->channelDef.options =
	    CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP | CHANNEL_OPTION_COMPRESS_RDP;
	sprintf_s(rdpdr->channelDef.name, ARRAYSIZE(rdpdr->channelDef.name), RDPDR_SVC_CHANNEL_NAME);
	rdpdr->sequenceId = 0;
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*)pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		rdpdr->rdpcontext = pEntryPointsEx->context;
	}

	CopyMemory(&(rdpdr->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	rdpdr->InitHandle = pInitHandle;
	rc = rdpdr->channelEntryPoints.pVirtualChannelInitEx(
	    rdpdr, NULL, pInitHandle, &rdpdr->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	    rdpdr_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_Print(rdpdr->log, WLOG_ERROR, "pVirtualChannelInitEx failed with %s [%08" PRIX32 "]",
		           WTSErrorToString(rc), rc);
		free(rdpdr);
		return FALSE;
	}

	return TRUE;
}

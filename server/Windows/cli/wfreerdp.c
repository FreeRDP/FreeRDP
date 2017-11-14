/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <errno.h>

#include <winpr/tchar.h>
#include <winpr/windows.h>

#include "wf_interface.h"

#include "wfreerdp.h"

#include <freerdp/log.h>
#define TAG SERVER_TAG("windows")

int IDcount = 0;

BOOL CALLBACK moncb(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	WLog_DBG(TAG, "%d\t(%ld, %ld), (%ld, %ld)",
	         IDcount, lprcMonitor->left, lprcMonitor->top,
	         lprcMonitor->right, lprcMonitor->bottom);
	IDcount++;
	return TRUE;
}

int main(int argc, char* argv[])
{
	BOOL screen_selected = FALSE;
	int index;
	wfServer* server;
	server = wfreerdp_server_new();
	set_screen_id(0);
	//handle args
	index = 1;
	errno = 0;

	while (index < argc)
	{
		//first the args that will cause the program to terminate
		if (strcmp("--list-screens", argv[index]) == 0)
		{
			_TCHAR name[128];
			int width;
			int height;
			int bpp;
			int i;
			WLog_INFO(TAG, "Detecting screens...");
			WLog_INFO(TAG, "ID\tResolution\t\tName (Interface)");

			for (i = 0; ; i++)
			{
				if (get_screen_info(i, name, &width, &height, &bpp) != 0)
				{
					if ((width * height * bpp) == 0)
						continue;

					WLog_INFO(TAG, "%d\t%dx%dx%d\t", i, width, height, bpp);
					WLog_INFO(TAG, "%s", name);
				}
				else
				{
					break;
				}
			}

			{
				int vscreen_w;
				int vscreen_h;
				vscreen_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
				vscreen_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
				WLog_INFO(TAG, "");
				EnumDisplayMonitors(NULL, NULL, moncb, 0);
				IDcount = 0;
				WLog_INFO(TAG, "Virtual Screen = %dx%d", vscreen_w, vscreen_h);
			}

			return 0;
		}

		if (strcmp("--screen", argv[index]) == 0)
		{
			UINT32 val;
			screen_selected = TRUE;
			index++;

			if (index == argc)
			{
				WLog_INFO(TAG, "missing screen id parameter");
				return 0;
			}

			val = strtoul(argv[index], NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return -1;

			set_screen_id(val);
			index++;
		}

		if (index == argc - 1)
		{
			UINT32 val = strtoul(argv[index], NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return -1;

			server->port = val;
			break;
		}
	}

	if (screen_selected == FALSE)
	{
		_TCHAR name[128];
		int width;
		int height;
		int bpp;
		int i;
		WLog_INFO(TAG, "screen id not provided. attempting to detect...");
		WLog_INFO(TAG, "Detecting screens...");
		WLog_INFO(TAG, "ID\tResolution\t\tName (Interface)");

		for (i = 0; ; i++)
		{
			if (get_screen_info(i, name, &width, &height, &bpp) != 0)
			{
				if ((width * height * bpp) == 0)
					continue;

				WLog_INFO(TAG, "%d\t%dx%dx%d\t", i, width, height, bpp);
				WLog_INFO(TAG, "%s", name);
				set_screen_id(i);
				break;
			}
			else
			{
				break;
			}
		}
	}

	WLog_INFO(TAG, "Starting server");
	wfreerdp_server_start(server);
	WaitForSingleObject(server->thread, INFINITE);
	WLog_INFO(TAG, "Stopping server");
	wfreerdp_server_stop(server);
	wfreerdp_server_free(server);
	return 0;
}

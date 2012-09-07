/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <winpr/tchar.h>
#include <winpr/windows.h>

#include "wf_mirage.h"

#define DEVICE_KEY_PREFIX	_T("\\Registry\\Machine\\")
/*
This function will iterate over the loaded display devices until it finds
the mirror device we want to load. If found, it will then copy the registry
key corresponding to the device to the context and returns true. Otherwise
the function returns false.
*/
BOOL wf_mirror_driver_find_display_device(wfInfo* context)
{
	BOOL result;
	BOOL devFound;
	DWORD deviceNumber;
	DISPLAY_DEVICE deviceInfo;

	devFound = FALSE;
	deviceNumber = 0;
	deviceInfo.cb = sizeof(deviceInfo);

	while (result = EnumDisplayDevices(NULL, deviceNumber, &deviceInfo, 0))
	{
		if (_tcscmp(deviceInfo.DeviceString, _T("Mirage Driver")) == 0)
		{
			int deviceKeyLength;
			int deviceKeyPrefixLength;

			deviceKeyPrefixLength = _tcslen(DEVICE_KEY_PREFIX);

			if (_tcsncmp(deviceInfo.DeviceKey, DEVICE_KEY_PREFIX, deviceKeyPrefixLength) == 0)
			{
				deviceKeyLength = _tcslen(deviceInfo.DeviceKey) - deviceKeyPrefixLength;
				context->deviceKey = (LPTSTR) malloc((deviceKeyLength + 1) * sizeof(TCHAR));

				_tcsncpy_s(context->deviceKey, deviceKeyLength + 1,
					&deviceInfo.DeviceKey[deviceKeyPrefixLength], deviceKeyLength);
			}

			_tcsncpy_s(context->deviceName, 32, deviceInfo.DeviceName, _tcslen(deviceInfo.DeviceName));
			return TRUE;
		}

		deviceNumber++;
	}

	return FALSE;
}

/**
 * This function will attempt to access the the windows registry using the device
 * key stored in the current context. It will attempt to read the value of the
 * "Attach.ToDesktop" subkey and will return true if the value is already set to
 * val. If unable to read the subkey, this function will return false. If the 
 * subkey is not set to val it will then attempt to set it to val and return true. If 
 * unsuccessful or an unexpected value is encountered, the function returns 
 * false.
 */

BOOL wf_mirror_driver_display_device_attach(wfInfo* context, DWORD mode)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, context->deviceKey,
		0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status != ERROR_SUCCESS)
		return FALSE;

	dwSize = sizeof(DWORD);
	status = RegQueryValueEx(hKey, _T("Attach.ToDesktop"),
		NULL, &dwType, (BYTE*) &dwValue, &dwSize);

	if (status != ERROR_SUCCESS)
		return FALSE;

	if (dwValue == 1)
	{
		dwValue = mode;
		dwSize = sizeof(DWORD);

		status = RegSetValueEx(HKEY_LOCAL_MACHINE, _T("Attach.ToDesktop"),
			0, REG_DWORD, (BYTE*) &dwValue, dwSize);

		if (status != ERROR_SUCCESS)
			return FALSE;
	}

	return TRUE;
}

void wf_mirror_driver_print_display_change_status(LONG status)
{
	TCHAR disp_change[64];

	switch (status)
	{
		case DISP_CHANGE_SUCCESSFUL:
			_tcscpy(disp_change, _T("DISP_CHANGE_SUCCESSFUL"));
			break;

		case DISP_CHANGE_BADDUALVIEW:
			_tcscpy(disp_change, _T("DISP_CHANGE_BADDUALVIEW"));
			break;

		case DISP_CHANGE_BADFLAGS:
			_tcscpy(disp_change, _T("DISP_CHANGE_BADFLAGS"));
			break;

		case DISP_CHANGE_BADMODE:
			_tcscpy(disp_change, _T("DISP_CHANGE_BADMODE"));
			break;

		case DISP_CHANGE_BADPARAM:
			_tcscpy(disp_change, _T("DISP_CHANGE_BADPARAM"));
			break;

		case DISP_CHANGE_FAILED:
			_tcscpy(disp_change, _T("DISP_CHANGE_FAILED"));
			break;

		case DISP_CHANGE_NOTUPDATED:
			_tcscpy(disp_change, _T("DISP_CHANGE_NOTUPDATED"));
			break;

		case DISP_CHANGE_RESTART:
			_tcscpy(disp_change, _T("DISP_CHANGE_RESTART"));
			break;

		default:
			_tcscpy(disp_change, _T("DISP_CHANGE_UNKNOWN"));
			break;
	}

	if (status != DISP_CHANGE_SUCCESSFUL)
		_tprintf(_T("ChangeDisplaySettingsEx() failed with %s (%d)\n"), disp_change, status);
	else
		_tprintf(_T("ChangeDisplaySettingsEx() succeeded with %s (%d)\n"), disp_change, status);
}

/**
 * This function will attempt to apply the currently configured display settings 
 * in the registry to the display driver. It will return true if successful 
 * otherwise it returns false.
 * If unload is nonzero then the the driver will be asked to remove itself.
 */

BOOL wf_mirror_driver_update(wfInfo* context, int unload)
{
	HDC dc;
	BOOL status;
	DWORD* extHdr;
	WORD drvExtraSaved;
	DEVMODE* deviceMode;
	LONG disp_change_status;
	DWORD dmf_devmodewext_magic_sig = 0xDF20C0DE;
	
	if (!unload)
	{
		/*
		 * Will have to come back to this for supporting non primary displays and multimonitor setups
		 */
		dc = GetDC(NULL);
		context->width = GetDeviceCaps(dc, HORZRES);
		context->height = GetDeviceCaps(dc, VERTRES);
		context->bitsPerPixel = GetDeviceCaps(dc, BITSPIXEL);
		ReleaseDC(NULL, dc);
	}
	else
	{
		context->width = 0;
		context->height = 0;
		context->bitsPerPixel = 0;
	}
	
	deviceMode = (DEVMODE*) malloc(sizeof(DEVMODE) + EXT_DEVMODE_SIZE_MAX);
	deviceMode->dmDriverExtra = 2 * sizeof(DWORD);

	extHdr = (DWORD*)((BYTE*) &deviceMode + sizeof(DEVMODE)); 
	extHdr[0] = dmf_devmodewext_magic_sig;
	extHdr[1] = 0;

	drvExtraSaved = deviceMode->dmDriverExtra;
	memset(deviceMode, 0, sizeof(DEVMODE) + EXT_DEVMODE_SIZE_MAX);
	deviceMode->dmSize = sizeof(DEVMODE);
	deviceMode->dmDriverExtra = drvExtraSaved;

	deviceMode->dmPelsWidth = context->width;
	deviceMode->dmPelsHeight = context->height;
	deviceMode->dmBitsPerPel = context->bitsPerPixel;
	deviceMode->dmPosition.x = 0;
	deviceMode->dmPosition.y = 0;

	deviceMode->dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;

	_tcsncpy_s(deviceMode->dmDeviceName, 32, context->deviceName, _tcslen(context->deviceName));

	disp_change_status = ChangeDisplaySettingsEx(context->deviceName, deviceMode, NULL, CDS_UPDATEREGISTRY, NULL);

	status = (disp_change_status == DISP_CHANGE_SUCCESSFUL) ? TRUE : FALSE;

	if (!status)
		wf_mirror_driver_print_display_change_status(disp_change_status);
		
	return status;
}

BOOL wf_mirror_driver_map_memory(wfInfo* context)
{
	int status;
	GETCHANGESBUF* b;

	context->driverDC = CreateDC(context->deviceName, NULL, NULL, NULL);

	if (context->driverDC == NULL)
	{
		_tprintf(_T("Could not create device driver context!\n"));
		return FALSE;
	}

	context->changeBuffer = malloc(sizeof(GETCHANGESBUF));
	ZeroMemory(context->changeBuffer, sizeof(GETCHANGESBUF));

	status = ExtEscape(context->driverDC, dmf_esc_usm_pipe_map, 0, 0, sizeof(GETCHANGESBUF), (LPSTR) context->changeBuffer);

	if (status <= 0)
	{
		_tprintf(_T("Failed to map shared memory from the driver! code %d\n"), status);
	}

	b = (GETCHANGESBUF*) context->changeBuffer;

	return TRUE;
}

/* Unmap the shared memory and release the DC */

BOOL wf_mirror_driver_cleanup(wfInfo* context)
{
	int status;

	status = ExtEscape(context->driverDC, dmf_esc_usm_pipe_unmap, sizeof(GETCHANGESBUF), (LPSTR) context->changeBuffer, 0, 0);
	
	if (status <= 0)
	{
		_tprintf(_T("Failed to unmap shared memory from the driver! code %d\n"), status);
	}

	if (context->driverDC != NULL)
	{
		status = DeleteDC(context->driverDC);

		if (status == 0)
		{
			_tprintf(_T("Failed to release DC!\n"));
		}
	}

	free(context->changeBuffer);

	return TRUE;
}

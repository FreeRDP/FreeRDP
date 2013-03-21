/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2012-2013 Corey Clayton <can.of.tuna@gmail.com>
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
key corresponding to the device to the wfi and returns TRUE. Otherwise
the function returns FALSE.
*/
BOOL wf_mirror_driver_find_display_device(wfInfo* wfi)
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

			if (_tcsnicmp(deviceInfo.DeviceKey, DEVICE_KEY_PREFIX, deviceKeyPrefixLength) == 0)
			{
				deviceKeyLength = _tcslen(deviceInfo.DeviceKey) - deviceKeyPrefixLength;
				wfi->deviceKey = (LPTSTR) malloc((deviceKeyLength + 1) * sizeof(TCHAR));

				_tcsncpy_s(wfi->deviceKey, deviceKeyLength + 1,
					&deviceInfo.DeviceKey[deviceKeyPrefixLength], deviceKeyLength);
			}

			_tcsncpy_s(wfi->deviceName, 32, deviceInfo.DeviceName, _tcslen(deviceInfo.DeviceName));
			return TRUE;
		}

		deviceNumber++;
	}

	return FALSE;
}

/**
 * This function will attempt to access the the windows registry using the device
 * key stored in the current wfi. It will attempt to read the value of the
 * "Attach.ToDesktop" subkey and will return TRUE if the value is already set to
 * val. If unable to read the subkey, this function will return FALSE. If the 
 * subkey is not set to val it will then attempt to set it to val and return TRUE. If 
 * unsuccessful or an unexpected value is encountered, the function returns 
 * FALSE.
 */

BOOL wf_mirror_driver_display_device_attach(wfInfo* wfi, DWORD mode)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, wfi->deviceKey,
		0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);

	if (status != ERROR_SUCCESS)
		return FALSE;

	dwSize = sizeof(DWORD);
	status = RegQueryValueEx(hKey, _T("Attach.ToDesktop"),
		NULL, &dwType, (BYTE*) &dwValue, &dwSize);

	if (status != ERROR_SUCCESS)
		return FALSE;

	if (dwValue ^ mode) //only if we want to change modes
	{
		dwValue = mode;
		dwSize = sizeof(DWORD);

		status = RegSetValueEx(hKey, _T("Attach.ToDesktop"),
			0, REG_DWORD, (BYTE*) &dwValue, dwSize);

		if (status != ERROR_SUCCESS)
		{	
			printf("Error writing registry key: %d ", status);
			if (status == ERROR_ACCESS_DENIED) 
				printf("access denied. Do you have admin privleges?");
			printf("\n");
			return FALSE;
		}
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
 * in the registry to the display driver. It will return TRUE if successful 
 * otherwise it returns FALSE.
 * If mode is MIRROR_UNLOAD then the the driver will be asked to remove itself.
 */

BOOL wf_mirror_driver_update(wfInfo* wfi, int mode)
{
	HDC dc;
	BOOL status;
	DWORD* extHdr;
	WORD drvExtraSaved;
	DEVMODE* deviceMode;
	LONG disp_change_status;
	DWORD dmf_devmodewext_magic_sig = 0xDF20C0DE;

	if ( (mode != MIRROR_LOAD) && (mode != MIRROR_UNLOAD) )
	{
		printf("Invalid mirror mode!\n");
		return FALSE;
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

	if (mode == MIRROR_LOAD)
	{
		wfi->virtscreen_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		wfi->virtscreen_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

		deviceMode->dmPelsWidth = wfi->virtscreen_width;
		deviceMode->dmPelsHeight = wfi->virtscreen_height;
		deviceMode->dmBitsPerPel = wfi->bitsPerPixel;
		deviceMode->dmPosition.x = wfi->servscreen_xoffset;
		deviceMode->dmPosition.y = wfi->servscreen_yoffset;
	}

	deviceMode->dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;

	_tcsncpy_s(deviceMode->dmDeviceName, 32, wfi->deviceName, _tcslen(wfi->deviceName));

	disp_change_status = ChangeDisplaySettingsEx(wfi->deviceName, deviceMode, NULL, CDS_UPDATEREGISTRY, NULL);

	status = (disp_change_status == DISP_CHANGE_SUCCESSFUL) ? TRUE : FALSE;

	if (!status)
		wf_mirror_driver_print_display_change_status(disp_change_status);
		
	return status;
}

BOOL wf_mirror_driver_map_memory(wfInfo* wfi)
{
	int status;

	wfi->driverDC = CreateDC(wfi->deviceName, NULL, NULL, NULL);

	if (wfi->driverDC == NULL)
	{
		_tprintf(_T("Could not create device driver context!\n"));
		return FALSE;
	}

	wfi->changeBuffer = malloc(sizeof(GETCHANGESBUF));
	ZeroMemory(wfi->changeBuffer, sizeof(GETCHANGESBUF));

	status = ExtEscape(wfi->driverDC, dmf_esc_usm_pipe_map, 0, 0, sizeof(GETCHANGESBUF), (LPSTR) wfi->changeBuffer);

	if (status <= 0)
	{
		_tprintf(_T("Failed to map shared memory from the driver! code %d\n"), status);
		return FALSE;
	}

	return TRUE;
}

/* Unmap the shared memory and release the DC */

BOOL wf_mirror_driver_cleanup(wfInfo* wfi)
{
	int status;

	status = ExtEscape(wfi->driverDC, dmf_esc_usm_pipe_unmap, sizeof(GETCHANGESBUF), (LPSTR) wfi->changeBuffer, 0, 0);
	
	if (status <= 0)
	{
		_tprintf(_T("Failed to unmap shared memory from the driver! code %d\n"), status);
	}

	if (wfi->driverDC != NULL)
	{
		status = DeleteDC(wfi->driverDC);

		if (status == 0)
		{
			_tprintf(_T("Failed to release DC!\n"));
		}
	}

	free(wfi->changeBuffer);

	return TRUE;
}

BOOL wf_mirror_driver_activate(wfInfo* wfi)
{
	if (!wfi->mirrorDriverActive)
	{
		printf("Activating Mirror Driver\n");

		if (wf_mirror_driver_find_display_device(wfi) == FALSE)
		{
			printf("Could not find dfmirage mirror driver! Is it installed?\n");
			return FALSE;
		}

		if (wf_mirror_driver_display_device_attach(wfi, 1) == FALSE)
		{
			printf("Could not attach display device!\n");
			return FALSE;
		}

		if (wf_mirror_driver_update(wfi, MIRROR_LOAD) == FALSE)
		{
			printf("could not update system with new display settings!\n");
			return FALSE;
		}

		if (wf_mirror_driver_map_memory(wfi) == FALSE)
		{
			printf("Unable to map memory for mirror driver!\n");
			return FALSE;
		}
		wfi->mirrorDriverActive = TRUE;
	}

	return TRUE;
}

void wf_mirror_driver_deactivate(wfInfo* wfi)
{
	if (wfi->mirrorDriverActive)
	{
		printf("Deactivating Mirror Driver\n");

		wf_mirror_driver_cleanup(wfi);
		wf_mirror_driver_display_device_attach(wfi, 0);
		wf_mirror_driver_update(wfi, MIRROR_UNLOAD);
		wfi->mirrorDriverActive = FALSE;
	}
}
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
BOOL wf_check_disp_devices(wfInfo* context)
{
	BOOL result, devFound;
	DWORD deviceNumber;
	DISPLAY_DEVICE deviceInfo;

	devFound = false;
	deviceNumber = 0;
	deviceInfo.cb = sizeof(deviceInfo);

	printf("Detecting display devices:\n");

	while (result = EnumDisplayDevices(NULL, deviceNumber, &deviceInfo, 0))
	{
		_tprintf(_T("\t%s\n"), deviceInfo.DeviceString);

		if (_tcscmp(deviceInfo.DeviceString, _T("Mirage Driver")) == 0)
		{
			int deviceKeyLength;
			int deviceKeyPrefixLength;

			_tprintf(_T("\n\nFound our target of interest!\n"));

			deviceKeyPrefixLength = _tcslen(DEVICE_KEY_PREFIX);

			if (_tcsncmp(deviceInfo.DeviceKey, DEVICE_KEY_PREFIX, deviceKeyPrefixLength) == 0)
			{
				deviceKeyLength = _tcslen(deviceInfo.DeviceKey) - deviceKeyPrefixLength;
				context->deviceKey = (LPTSTR) malloc((deviceKeyLength + 1) * sizeof(TCHAR));

				_tcsncpy_s(context->deviceKey, deviceKeyLength + 1,
					&deviceInfo.DeviceKey[deviceKeyPrefixLength], deviceKeyLength);

				_tprintf(_T("DeviceKey: %s\n"), context->deviceKey);
			}

			_tcsncpy_s(context->deviceName, 32, deviceInfo.DeviceName, _tcslen(deviceInfo.DeviceName));
			return true;
		}

		deviceNumber++;
	}

	return false;
}

/*
This function will attempt to access the the windows registry using the device
 key stored in the current context. It will attempt to read the value of the
 "Attach.ToDesktop" subkey and will return true if the value is already set to
 val. If unable to read the subkey, this function will return false. If the 
 subkey is not set to val it will then attempt to set it to val and return true. If 
 unsuccessful or an unexpected value is encountered, the function returns 
 false.
 */
BOOL wf_disp_device_set_attatch(wfInfo* context, DWORD val)
{
	LONG status;
	DWORD rtype, rdata, rdata_size;

	_tprintf(_T("\nOpening registry key %s\n"), context->deviceKey);

	rtype = 0;
	rdata = 0;
	rdata_size = sizeof(rdata);

	status = RegGetValue(HKEY_LOCAL_MACHINE, context->deviceKey,
		_T("Attach.ToDesktop"), RRF_RT_REG_DWORD, &rtype, &rdata, &rdata_size);

	if (status != ERROR_SUCCESS)
	{
		printf("Failed to read registry value.\n");
		printf("operation returned %d\n", status);
		return false;
	}

	_tprintf(_T("type = %04X, data = %04X\n"), rtype, rdata);

	if (rdata == (val ^ 1))
	{
		rdata = val;

		status = RegSetKeyValue(HKEY_LOCAL_MACHINE, context->deviceKey,
			_T("Attach.ToDesktop"), REG_DWORD, &rdata, rdata_size);

		if (status != ERROR_SUCCESS)
		{
			_tprintf(_T("Failed to write registry value.\n"));
			_tprintf(_T("operation returned %d\n"), status);
			return false;
		}

		_tprintf(_T("Wrote subkey \"Attach.ToDesktop\" -> %04X\n\n"), rdata);
	}
	else if (rdata == val)
	{
		_tprintf(_T("\"Attach.ToDesktop\" is already set to %04X!\n"), rdata);
	}
	else
	{
		_tprintf(_T("An wild value appeared!...\nrdata=%d\n"), rdata);
		return false;
	}

	return true;
}

/*
This function will attempt to apply the currently configured display settings 
in the registry to the display driver. It will return true if successful 
otherwise it returns false.

If unload is nonzero then the the driver will be asked to remove it self.
*/
BOOL wf_update_mirror_drv(wfInfo* context, int unload)
{
	int currentScreenPixHeight, currentScreenPixWidth, currentScreenBPP;
	HDC dc;
	LONG status;
	DWORD* extHdr;
	WORD drvExtraSaved;
	DEVMODE* deviceMode;
	DWORD dmf_devmodewext_magic_sig = 0xDF20C0DE;
	TCHAR rMsg[64];
	BOOL rturn;
	
	if(!unload)
	{
		/*
		Will have to come back to this for supporting non primary displays and 
		multimonitor setups
		*/
		dc = GetDC(NULL);
		currentScreenPixHeight = GetDeviceCaps(dc, VERTRES);
		currentScreenPixWidth = GetDeviceCaps(dc, HORZRES);
		currentScreenBPP = GetDeviceCaps(dc, BITSPIXEL);
		ReleaseDC(NULL, dc);

		context->height = currentScreenPixHeight;
		context->width = currentScreenPixWidth;
		context->bitsPerPix = currentScreenBPP;

		_tprintf(_T("Detected current screen settings: %dx%dx%d\n"), currentScreenPixHeight, currentScreenPixWidth, currentScreenBPP);
	}
	else
	{
		currentScreenPixHeight = 0;
		currentScreenPixWidth = 0;
		currentScreenBPP = 0;
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

	deviceMode->dmPelsWidth = currentScreenPixWidth;
	deviceMode->dmPelsHeight = currentScreenPixHeight;
	deviceMode->dmBitsPerPel = currentScreenBPP;
	deviceMode->dmPosition.x = 0;
	deviceMode->dmPosition.y = 0;

	deviceMode->dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;

	_tcsncpy_s(deviceMode->dmDeviceName, 32, context->deviceName, _tcslen(context->deviceName));

	status = ChangeDisplaySettingsEx(context->deviceName, deviceMode, NULL, CDS_UPDATEREGISTRY, NULL);

	rturn = false;
	switch (status)
	{
		case DISP_CHANGE_SUCCESSFUL:
			_tprintf(_T("ChangeDisplaySettingsEx() was successfull\n"));
			rturn = true;
			break;

		case DISP_CHANGE_BADDUALVIEW:
			_tcscpy(rMsg, _T("DISP_CHANGE_BADDUALVIEW"));
			break;

		case DISP_CHANGE_BADFLAGS:
			_tcscpy(rMsg, _T("DISP_CHANGE_BADFLAGS"));
			break;

		case DISP_CHANGE_BADMODE:
			_tcscpy(rMsg, _T("DISP_CHANGE_BADMODE"));
			break;

		case DISP_CHANGE_BADPARAM:
			_tcscpy(rMsg, _T("DISP_CHANGE_BADPARAM"));
			break;

		case DISP_CHANGE_FAILED:
			_tcscpy(rMsg, _T("DISP_CHANGE_FAILED"));
			break;

		case DISP_CHANGE_NOTUPDATED:
			_tcscpy(rMsg, _T("DISP_CHANGE_NOTUPDATED"));
			break;

		case DISP_CHANGE_RESTART:
			_tcscpy(rMsg, _T("DISP_CHANGE_RESTART"));
			break;
	}

	if(!rturn)
		_tprintf(_T("ChangeDisplaySettingsEx() failed with %s, code %d\n"), rMsg, status);
		
	return rturn;
}


BOOL wf_map_mirror_mem(wfInfo* context)
{
	int status;
	GETCHANGESBUF* b;
	_tprintf(_T("\n\nCreating a device context...\n"));

	context->driverDC = CreateDC(context->deviceName, NULL, NULL, NULL);

	if (context->driverDC == NULL)
	{
		_tprintf(_T("Could not create device driver context!\n"));
		return false;
	}

	context->changeBuffer = malloc(sizeof(GETCHANGESBUF));
	memset(context->changeBuffer, 0, sizeof(GETCHANGESBUF));

	_tprintf(_T("\n\nConnecting to driver...\n"));
	status = ExtEscape(context->driverDC, dmf_esc_usm_pipe_map, 0, 0, sizeof(GETCHANGESBUF), (LPSTR) context->changeBuffer);

	if (status <= 0)
	{
		_tprintf(_T("Failed to map shared memory from the driver! Code %d\n"), status);
	}

	b = (GETCHANGESBUF*)context->changeBuffer;
	_tprintf(_T("ExtEscape() returned code %d\n"), status);

	return true;
}

/*
Unmap the shared memory and release the DC
*/
BOOL wf_mirror_cleanup(wfInfo* context)
{
	int iResult;

	_tprintf(_T("\n\nCleaning up...\nDisconnecting driver...\n"));
	iResult = ExtEscape(context->driverDC, dmf_esc_usm_pipe_unmap, sizeof(context->changeBuffer), (LPSTR) context->changeBuffer, 0, 0);

	if(iResult <= 0)
	{
		_tprintf(_T("Failed to unmap shared memory from the driver! Code %d\n"), iResult);
	}

	_tprintf(_T("Releasing DC\n"));
	if(context->driverDC != NULL)
	{
		iResult = DeleteDC(context->driverDC);
		if(iResult == 0)
		{
			_tprintf(_T("Failed to release DC!\n"));
		}
	}

	free(context->changeBuffer);

	return true;
}
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

int wf_mirage_step1(wfPeerContext* context)
{
	BOOL result;
	DWORD deviceNumber;
	DISPLAY_DEVICE deviceInfo;

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
		}

		deviceNumber++;
	}

	return 0;
}

int wf_mirage_step2(wfPeerContext* context)
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
		return -1;
	}

	_tprintf(_T("type = %04X, data = %04X\n\nNow let's try attaching it...\n"), rtype, rdata);

	if (rdata == 0)
	{
		rdata = 1;

		status = RegSetKeyValue(HKEY_LOCAL_MACHINE, context->deviceKey,
			_T("Attach.ToDesktop"), REG_DWORD, &rdata, rdata_size);

		if (status != ERROR_SUCCESS)
		{
			_tprintf(_T("Failed to read registry value.\n"));
			_tprintf(_T("operation returned %d\n"), status);
			return -1;
		}

		_tprintf(_T("Attached to Desktop\n\n"));
	}
	else if (rdata == 1)
	{
		_tprintf(_T("Already attached to desktop!\n"));
	}
	else
	{
		_tprintf(_T("Something went wrong with attaching to desktop...\nrdata=%d\n"), rdata);
		return -1;
	}

	return 0;
}

int wf_mirage_step3(wfPeerContext* context)
{
	LONG status;
	DWORD* extHdr;
	WORD drvExtraSaved;
	DEVMODE* deviceMode;
	DWORD dmf_devmodewext_magic_sig = 0xDF20C0DE;

	deviceMode = (DEVMODE*) malloc(sizeof(DEVMODE) + EXT_DEVMODE_SIZE_MAX);
	deviceMode->dmDriverExtra = 2 * sizeof(DWORD);

	extHdr = (DWORD*)((BYTE*) &deviceMode + sizeof(DEVMODE)); 
	extHdr[0] = dmf_devmodewext_magic_sig;
	extHdr[1] = 0;

	drvExtraSaved = deviceMode->dmDriverExtra;
	memset(deviceMode, 0, sizeof(DEVMODE) + EXT_DEVMODE_SIZE_MAX);
	deviceMode->dmSize = sizeof(DEVMODE);
	deviceMode->dmDriverExtra = drvExtraSaved;

	deviceMode->dmPelsWidth = 640;
	deviceMode->dmPelsHeight = 480;
	deviceMode->dmBitsPerPel = 32;
	deviceMode->dmPosition.x = 0;
	deviceMode->dmPosition.y = 0;

	deviceMode->dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;

	_tcsncpy_s(deviceMode->dmDeviceName, 32, context->deviceName, _tcslen(context->deviceName));

	status = ChangeDisplaySettingsEx(context->deviceName, deviceMode, NULL, CDS_UPDATEREGISTRY, NULL);

	switch (status)
	{
		case DISP_CHANGE_SUCCESSFUL:
			printf("ChangeDisplaySettingsEx() was successfull\n");
			break;

		case DISP_CHANGE_BADDUALVIEW:
			printf("ChangeDisplaySettingsEx() failed with  DISP_CHANGE_BADDUALVIEW, code %d\n", status);
			return -1;
			break;

		case DISP_CHANGE_BADFLAGS:
			printf("ChangeDisplaySettingsEx() failed with  DISP_CHANGE_BADFLAGS, code %d\n", status);
			return -1;
			break;

		case DISP_CHANGE_BADMODE:
			printf("ChangeDisplaySettingsEx() failed with  DISP_CHANGE_BADMODE, code %d\n", status);
			return -1;
			break;

		case DISP_CHANGE_BADPARAM:
			printf("ChangeDisplaySettingsEx() failed with  DISP_CHANGE_BADPARAM, code %d\n", status);
			return -1;
			break;

		case DISP_CHANGE_FAILED:
			printf("ChangeDisplaySettingsEx() failed with  DISP_CHANGE_FAILED, code %d\n", status);
			return -1;
			break;

		case DISP_CHANGE_NOTUPDATED:
			printf("ChangeDisplaySettingsEx() failed with  DISP_CHANGE_NOTUPDATED, code %d\n", status);
			return -1;
			break;

		case DISP_CHANGE_RESTART:
			printf("ChangeDisplaySettingsEx() failed with  DISP_CHANGE_RESTART, code %d\n", status);
			return -1;
			break;
	}

	return 0;
}

int wf_mirage_step4(wfPeerContext* context)
{
	int status;

	printf("\n\nCreating a device context...\n");

	context->driverDC = CreateDC(context->deviceName, NULL, NULL, NULL);

	if (context->driverDC == NULL)
	{
		printf("Could not create device driver context!\n");
		return -1;
	}

	context->changeBuffer = malloc(sizeof(GETCHANGESBUF));

	printf("\n\nConnecting to driver...\n");
	status = ExtEscape(context->driverDC, dmf_esc_usm_pipe_map, 0, 0, sizeof(GETCHANGESBUF), (LPSTR) context->changeBuffer);

	if (status <= 0)
	{
		printf("Failed to map shared memory from the driver! Code %d\n", status);
	}

	return 0;
}

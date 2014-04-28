/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Hewlett-Packard Development Company, L.P.
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

#include <winpr/crt.h>
#include <winpr/comm.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/handle.h>

int TestCommConfig(int argc, char* argv[])
{
	DCB dcb;
	HANDLE hComm;
	BOOL success;
	LPCSTR lpFileName = "\\\\.\\COM1";

	hComm = CreateFileA(lpFileName,
			    GENERIC_READ | GENERIC_WRITE,
			    0, NULL, OPEN_EXISTING, 0, NULL);

	if (hComm && (hComm != INVALID_HANDLE_VALUE))
	{
		printf("CreateFileA failure: could create a handle on a not yet defined device: %s\n", lpFileName);
		return EXIT_FAILURE;
	}

	// TMP: FIXME: check if we can proceed with tests on the actual device, skip and warn otherwise but don't fail
	success = DefineCommDevice(lpFileName, "/dev/ttyS0");
	if(!success)
	{
		printf("DefineCommDevice failure: %s\n", lpFileName);
		return EXIT_FAILURE;
	}

	hComm = CreateFileA(lpFileName,
			    GENERIC_READ | GENERIC_WRITE,
			    FILE_SHARE_WRITE, /* invalid parmaeter */
			    NULL, 
			    CREATE_NEW, /* invalid parameter */
			    0, 
			    (HANDLE)1234); /* invalid parmaeter */
	if (hComm != INVALID_HANDLE_VALUE)
	{
		printf("CreateFileA failure: could create a handle with some invalid parameters %s\n", lpFileName);
		return EXIT_FAILURE;
	}


	hComm = CreateFileA(lpFileName,
			    GENERIC_READ | GENERIC_WRITE,
			    0, NULL, OPEN_EXISTING, 0, NULL);

	if (!hComm || (hComm == INVALID_HANDLE_VALUE))
	{
		printf("CreateFileA failure: %s GetLastError() = 0x%0.8x\n", lpFileName, GetLastError());
		return EXIT_FAILURE;
	}

	/* TODO: a second call to CreateFileA should failed and
	 * GetLastError should return ERROR_SHARING_VIOLATION */

	ZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	success = GetCommState(hComm, &dcb);
	if (!success)
	{
		printf("GetCommState failure: GetLastError() = Ox%x\n", (int) GetLastError());
		return EXIT_FAILURE;
	}

	printf("BaudRate: %d ByteSize: %d Parity: %d StopBits: %d\n",
	       (int) dcb.BaudRate, (int) dcb.ByteSize, (int) dcb.Parity, (int) dcb.StopBits);

	dcb.BaudRate = CBR_57600;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	success = SetCommState(hComm, &dcb);

	if (!success)
	{
		printf("SetCommState failure: GetLastError() = 0x%x\n", (int) GetLastError());
		return 0;
	}

	success = GetCommState(hComm, &dcb);

	if (!success)
	{
		printf("GetCommState failure: GetLastError() = 0x%x\n", (int) GetLastError());
		return 0;
	}

	printf("BaudRate: %d ByteSize: %d Parity: %d StopBits: %d\n",
	       (int) dcb.BaudRate, (int) dcb.ByteSize, (int) dcb.Parity, (int) dcb.StopBits);

	CloseHandle(hComm);

	return 0;
}


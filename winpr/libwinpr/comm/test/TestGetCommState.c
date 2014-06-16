/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
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

#include <stdio.h>

#include <winpr/comm.h>
#include <winpr/crt.h>

#include "../comm.h"

static BOOL test_generic(HANDLE hComm)
{
	DCB dcb, *pDcb;
	BOOL result;

	ZeroMemory(&dcb, sizeof(DCB));
	result = GetCommState(hComm, &dcb);
	if (result)
	{
		printf("GetCommState failure, should have returned false because dcb.DCBlength has been let uninitialized\n");
		return FALSE;
	}


	ZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB) / 2; /* improper value */
	result = GetCommState(hComm, &dcb);
	if (result)
	{
		printf("GetCommState failure, should have return false because dcb.DCBlength was not correctly initialized\n");
		return FALSE;
	}

	ZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	result = GetCommState(hComm, &dcb);
	if (!result)
	{
		printf("GetCommState failure: Ox%x, with adjusted DCBlength\n", GetLastError());
		return FALSE;
	}

	pDcb = (DCB*)calloc(1, sizeof(DCB) * 2);
	pDcb->DCBlength = sizeof(DCB) * 2;
	result = GetCommState(hComm, pDcb);
	result = result && (pDcb->DCBlength == sizeof(DCB) * 2);
	free(pDcb);
	if (!result)
	{
		printf("GetCommState failure: 0x%x, with bigger DCBlength\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

int TestGetCommState(int argc, char* argv[])
{
	BOOL result;
	HANDLE hComm;

	// TMP: FIXME: check if we can proceed with tests on the actual device, skip and warn otherwise but don't fail
	result = DefineCommDevice("COM1", "/dev/ttyS0");
	if (!result)
	{
		printf("DefineCommDevice failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	hComm = CreateFile("COM1",
			GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, 0, NULL);
	if (hComm == INVALID_HANDLE_VALUE)
	{
		printf("CreateFileA failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	if (!test_generic(hComm))
	{
		printf("test_generic failure (RemoteSerialDriverUnknown)\n");
		return EXIT_FAILURE;
	}

	_comm_setRemoteSerialDriver(hComm, RemoteSerialDriverSerialSys);
	if (!test_generic(hComm))
	{
		printf("test_generic failure (RemoteSerialDriverSerialSys)\n");
		return EXIT_FAILURE;
	}

	_comm_setRemoteSerialDriver(hComm, RemoteSerialDriverSerCxSys);
	if (!test_generic(hComm))
	{
		printf("test_generic failure (RemoteSerialDriverSerCxSys)\n");
		return EXIT_FAILURE;
	}

	_comm_setRemoteSerialDriver(hComm, RemoteSerialDriverSerCx2Sys);
	if (!test_generic(hComm))
	{
		printf("test_generic failure (RemoteSerialDriverSerCx2Sys)\n");
		return EXIT_FAILURE;
	}

	if (!CloseHandle(hComm))
	{
		fprintf(stderr, "CloseHandle failure, GetLastError()=%0.8x\n", GetLastError());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

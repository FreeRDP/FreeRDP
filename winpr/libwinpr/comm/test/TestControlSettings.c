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

#include <sys/stat.h>

#include <winpr/comm.h>
#include <winpr/crt.h>

#include "../comm.h"

int TestControlSettings(int argc, char* argv[])
{
	struct stat statbuf;
	BOOL result;
	HANDLE hComm;
	DCB dcb;

	if (stat("/dev/ttyS0", &statbuf) < 0)
	{
		fprintf(stderr, "/dev/ttyS0 not available, making the test to succeed though\n");
		return EXIT_SUCCESS;
	}

	result = DefineCommDevice("COM1", "/dev/ttyS0");
	if (!result)
	{
		fprintf(stderr, "DefineCommDevice failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	hComm = CreateFile("COM1", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hComm == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "CreateFileA failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	ZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	if (!GetCommState(hComm, &dcb))
	{
		fprintf(stderr, "GetCommState failure; GetLastError(): %08x\n", GetLastError());
		return FALSE;
	}

	/* Test 1 */

	dcb.ByteSize = 5;
	dcb.StopBits = ONESTOPBIT;
	dcb.Parity = MARKPARITY;

	if (!SetCommState(hComm, &dcb))
	{
		fprintf(stderr, "SetCommState failure; GetLastError(): %08x\n", GetLastError());
		return FALSE;
	}

	ZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	if (!GetCommState(hComm, &dcb))
	{
		fprintf(stderr, "GetCommState failure; GetLastError(): %08x\n", GetLastError());
		return FALSE;
	}

	if ((dcb.ByteSize != 5) || (dcb.StopBits != ONESTOPBIT) || (dcb.Parity != MARKPARITY))
	{
		fprintf(stderr, "test1 failed.\n");
		return FALSE;
	}

	/* Test 2 */

	dcb.ByteSize = 8;
	dcb.StopBits = ONESTOPBIT;
	dcb.Parity = NOPARITY;

	if (!SetCommState(hComm, &dcb))
	{
		fprintf(stderr, "SetCommState failure; GetLastError(): %08x\n", GetLastError());
		return FALSE;
	}

	ZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	if (!GetCommState(hComm, &dcb))
	{
		fprintf(stderr, "GetCommState failure; GetLastError(): %08x\n", GetLastError());
		return FALSE;
	}

	if ((dcb.ByteSize != 8) || (dcb.StopBits != ONESTOPBIT) || (dcb.Parity != NOPARITY))
	{
		fprintf(stderr, "test2 failed.\n");
		return FALSE;
	}

	if (!CloseHandle(hComm))
	{
		fprintf(stderr, "CloseHandle failure, GetLastError()=%08x\n", GetLastError());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

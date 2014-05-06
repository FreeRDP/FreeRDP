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
#include <termios.h>

#include <winpr/comm.h>
#include <winpr/crt.h>

#include "../comm.h"

static BOOL test_SerialSys(HANDLE hComm)
{
	// TMP: TODO:
	return TRUE;
}


int TestHandflow(int argc, char* argv[])
{
	BOOL result;
	HANDLE hComm;

	// TMP: FIXME: check if we can proceed with tests on the actual device, skip and warn otherwise but don't fail
	result = DefineCommDevice("COM1", "/dev/ttyS0");
	if (!result)
	{
		fprintf(stderr, "DefineCommDevice failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	hComm = CreateFile("COM1",
			   GENERIC_READ | GENERIC_WRITE,
			   0, NULL, OPEN_EXISTING, 0, NULL);
	if (hComm == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "CreateFileA failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	_comm_setRemoteSerialDriver(hComm, RemoteSerialDriverSerialSys);
	if (!test_SerialSys(hComm))
	{
		fprintf(stderr, "test_SerCxSys failure\n");
		return EXIT_FAILURE;
	}

	/* _comm_setRemoteSerialDriver(hComm, RemoteSerialDriverSerCxSys); */
	/* if (!test_SerCxSys(hComm)) */
	/* { */
	/* 	fprintf(stderr, "test_SerCxSys failure\n"); */
	/* 	return EXIT_FAILURE; */
	/* } */

	/* _comm_setRemoteSerialDriver(hComm, RemoteSerialDriverSerCx2Sys); */
	/* if (!test_SerCx2Sys(hComm)) */
	/* { */
	/* 	fprintf(stderr, "test_SerCxSys failure\n"); */
	/* 	return EXIT_FAILURE; */
	/* } */


	if (!CloseHandle(hComm))
	{
		fprintf(stderr, "CloseHandle failure, GetLastError()=%0.8x\n", GetLastError());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

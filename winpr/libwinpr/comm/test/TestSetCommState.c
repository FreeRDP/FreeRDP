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


static int test_fParity(HANDLE hComm)
{
	DCB dcb;
	BOOL result;

	ZeroMemory(&dcb, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	result = GetCommState(hComm, &dcb);
	if (!result)
	{
		printf("GetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	/* test 1 */
	dcb.fParity = TRUE;
	result = SetCommState(hComm, &dcb);
	if (!result)
	{
		printf("SetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	ZeroMemory(&dcb, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	result = GetCommState(hComm, &dcb);
	if (!result)
	{
		printf("GetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	if (!dcb.fParity)
	{
		printf("unexpected fParity: %d instead of TRUE\n", dcb.fParity);
		return EXIT_FAILURE;
	}

	/* test 2 */
	dcb.fParity = FALSE;
	result = SetCommState(hComm, &dcb);
	if (!result)
	{
		printf("SetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	ZeroMemory(&dcb, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	result = GetCommState(hComm, &dcb);
	if (!result)
	{
		printf("GetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	if (dcb.fParity)
	{
		printf("unexpected fParity: %d instead of FALSE\n", dcb.fParity);
		return EXIT_FAILURE;
	}

	/* test 3 (redo test 1) */
	dcb.fParity = TRUE;
	result = SetCommState(hComm, &dcb);
	if (!result)
	{
		printf("SetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	ZeroMemory(&dcb, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	result = GetCommState(hComm, &dcb);
	if (!result)
	{
		printf("GetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	if (!dcb.fParity)
	{
		printf("unexpected fParity: %d instead of TRUE\n", dcb.fParity);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int TestSetCommState(int argc, char* argv[])
{
	DCB dcb, *pDcb;
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

	ZeroMemory(&dcb, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	result = GetCommState(hComm, &dcb);
	if (!result)
	{
		printf("GetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	dcb.BaudRate = CBR_115200;
	result = SetCommState(hComm, &dcb);
	if (!result)
	{
		printf("SetCommState failure: 0x%0.8x\n", GetLastError());
		return EXIT_FAILURE;
	}

	ZeroMemory(&dcb, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	result = GetCommState(hComm, &dcb);
	if (!result)
	{
		printf("GetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}
	if (dcb.BaudRate != CBR_115200)
	{
		printf("SetCommState failure: could not set BaudRate=%d (CBR_115200)\n", CBR_115200);
		return EXIT_FAILURE;
	}


	/* Test 2 using a defferent baud rate */

	dcb.BaudRate = CBR_57600;
	result = SetCommState(hComm, &dcb);
	if (!result)
	{
		printf("SetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	ZeroMemory(&dcb, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	result = GetCommState(hComm, &dcb);
	if (!result)
	{
		printf("GetCommState failure: 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}
	if (dcb.BaudRate != CBR_57600)
	{
		printf("SetCommState failure: could not set BaudRate=%d (CBR_57600)\n", CBR_57600);
		return EXIT_FAILURE;
	}

	/* Test 3 using an unsupported baud rate  on Linux */
#ifdef __linux__
	dcb.BaudRate = CBR_128000;
	result = SetCommState(hComm, &dcb);
	if (result)
	{
		printf("SetCommState failure: unexpected support of BaudRate=%d (CBR_128000)\n", CBR_128000);
		return EXIT_FAILURE;
	}
#endif /* __linux__ */

	// TODO: a more complete and generic test using GetCommProperties()

	/* TMP: TODO: fBinary tests */

	/* fParity tests */
	if (test_fParity(hComm) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

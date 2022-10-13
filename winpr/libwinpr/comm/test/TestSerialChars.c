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

#ifndef _WIN32
#include <termios.h>
#endif

#include <winpr/comm.h>
#include <winpr/crt.h>

#include "../comm.h"

static BOOL test_SerCxSys(HANDLE hComm)
{
	DCB dcb = { 0 };
	UCHAR XonChar, XoffChar;

	struct termios currentTermios = { 0 };

	if (tcgetattr(((WINPR_COMM*)hComm)->fd, &currentTermios) < 0)
	{
		fprintf(stderr, "tcgetattr failure.\n");
		return FALSE;
	}

	dcb.DCBlength = sizeof(DCB);
	if (!GetCommState(hComm, &dcb))
	{
		fprintf(stderr, "GetCommState failure, GetLastError(): 0x%08x\n", GetLastError());
		return FALSE;
	}

	if ((dcb.XonChar == '\0') || (dcb.XoffChar == '\0'))
	{
		fprintf(stderr, "test_SerCxSys failure, expected XonChar and XoffChar to be set\n");
		return FALSE;
	}

	/* retrieve Xon/Xoff chars */
	if ((dcb.XonChar != currentTermios.c_cc[VSTART]) ||
	    (dcb.XoffChar != currentTermios.c_cc[VSTOP]))
	{
		fprintf(stderr, "test_SerCxSys failure, could not retrieve XonChar and XoffChar\n");
		return FALSE;
	}

	/* swap XonChar/XoffChar */

	XonChar = dcb.XonChar;
	XoffChar = dcb.XoffChar;
	dcb.XonChar = XoffChar;
	dcb.XoffChar = XonChar;
	if (!SetCommState(hComm, &dcb))
	{
		fprintf(stderr, "SetCommState failure, GetLastError(): 0x%08x\n", GetLastError());
		return FALSE;
	}

	ZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	if (!GetCommState(hComm, &dcb))
	{
		fprintf(stderr, "GetCommState failure, GetLastError(): 0x%08x\n", GetLastError());
		return FALSE;
	}

	if ((dcb.XonChar != XoffChar) || (dcb.XoffChar != XonChar))
	{
		fprintf(stderr, "test_SerCxSys, expected XonChar and XoffChar to be swapped\n");
		return FALSE;
	}

	/* same XonChar / XoffChar */
	dcb.XonChar = dcb.XoffChar;
	if (SetCommState(hComm, &dcb))
	{
		fprintf(stderr, "test_SerCxSys failure, SetCommState() was supposed to failed because "
		                "XonChar and XoffChar are the same\n");
		return FALSE;
	}
	if (GetLastError() != ERROR_INVALID_PARAMETER)
	{
		fprintf(stderr, "test_SerCxSys failure, SetCommState() was supposed to failed with "
		                "GetLastError()=ERROR_INVALID_PARAMETER\n");
		return FALSE;
	}

	return TRUE;
}

static BOOL test_SerCx2Sys(HANDLE hComm)
{
	DCB dcb = { 0 };

	dcb.DCBlength = sizeof(DCB);
	if (!GetCommState(hComm, &dcb))
	{
		fprintf(stderr, "GetCommState failure; GetLastError(): %08x\n", GetLastError());
		return FALSE;
	}

	if ((dcb.ErrorChar != '\0') || (dcb.EofChar != '\0') || (dcb.EvtChar != '\0') ||
	    (dcb.XonChar != '\0') || (dcb.XoffChar != '\0'))
	{
		fprintf(stderr, "test_SerCx2Sys failure, expected all characters to be: '\\0'\n");
		return FALSE;
	}

	return TRUE;
}

int TestSerialChars(int argc, char* argv[])
{
	struct stat statbuf;
	BOOL result;
	HANDLE hComm;

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

	_comm_setServerSerialDriver(hComm, SerialDriverSerCxSys);
	if (!test_SerCxSys(hComm))
	{
		fprintf(stderr, "test_SerCxSys failure\n");
		return EXIT_FAILURE;
	}

	_comm_setServerSerialDriver(hComm, SerialDriverSerCx2Sys);
	if (!test_SerCx2Sys(hComm))
	{
		fprintf(stderr, "test_SerCxSys failure\n");
		return EXIT_FAILURE;
	}

	if (!CloseHandle(hComm))
	{
		fprintf(stderr, "CloseHandle failure, GetLastError()=%08x\n", GetLastError());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

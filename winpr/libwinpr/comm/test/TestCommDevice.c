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
#include <winpr/tchar.h>

static int test_CommDevice(LPCTSTR lpDeviceName, BOOL expectedResult)
{
	TCHAR lpTargetPath[MAX_PATH] = { 0 };

	BOOL result = DefineCommDevice(lpDeviceName, _T("/dev/test"));
	if ((!expectedResult && result) || (expectedResult && !result)) /* logical XOR */
	{
		_tprintf(_T("DefineCommDevice failure: device name: %s, expected result: %s, result: %s\n"),
		         lpDeviceName, (expectedResult ? "TRUE" : "FALSE"), (result ? "TRUE" : "FALSE"));

		return FALSE;
	}

	result = IsCommDevice(lpDeviceName);
	if ((!expectedResult && result) || (expectedResult && !result)) /* logical XOR */
	{
		_tprintf(_T("IsCommDevice failure: device name: %s, expected result: %s, result: %s\n"),
		         lpDeviceName, (expectedResult ? "TRUE" : "FALSE"), (result ? "TRUE" : "FALSE"));

		return FALSE;
	}

	const size_t tclen = QueryCommDevice(lpDeviceName, lpTargetPath, MAX_PATH);
	if (expectedResult)
	{
		const size_t tlen = _tcsnlen(lpTargetPath, ARRAYSIZE(lpTargetPath) - 1);
		if (tclen <= tlen) /* at least 2 more TCHAR are expected */
		{
			_tprintf(_T("QueryCommDevice failure: didn't find the device name: %s\n"),
			         lpDeviceName);
			return FALSE;
		}

		if (_tcsncmp(_T("/dev/test"), lpTargetPath, ARRAYSIZE(lpTargetPath)) != 0)
		{
			_tprintf(
			    _T("QueryCommDevice failure: device name: %s, expected result: %s, result: %s\n"),
			    lpDeviceName, _T("/dev/test"), lpTargetPath);

			return FALSE;
		}

		if ((tlen >= (ARRAYSIZE(lpTargetPath) - 1)) || (lpTargetPath[tlen + 1] != 0))
		{
			_tprintf(_T("QueryCommDevice failure: device name: %s, the second NULL character is ")
			         _T("missing at the end of the buffer\n"),
			         lpDeviceName);
			return FALSE;
		}
	}
	else
	{
		if (tclen > 0)
		{
			_tprintf(_T("QueryCommDevice failure: device name: %s, expected result: <none>, ")
			         _T("result: %") _T(PRIuz) _T(" %s\n"),
			         lpDeviceName, tclen, lpTargetPath);

			return FALSE;
		}
	}

	return TRUE;
}

int TestCommDevice(int argc, char* argv[])
{
	if (!test_CommDevice(_T("COM0"), FALSE))
		return EXIT_FAILURE;

	if (!test_CommDevice(_T("COM1"), TRUE))
		return EXIT_FAILURE;

	if (!test_CommDevice(_T("COM1"), TRUE))
		return EXIT_FAILURE;

	if (!test_CommDevice(_T("COM10"), FALSE))
		return EXIT_FAILURE;

	if (!test_CommDevice(_T("\\\\.\\COM5"), TRUE))
		return EXIT_FAILURE;

	if (!test_CommDevice(_T("\\\\.\\COM10"), TRUE))
		return EXIT_FAILURE;

	if (!test_CommDevice(_T("\\\\.COM10"), FALSE))
		return EXIT_FAILURE;

	return 0;
}

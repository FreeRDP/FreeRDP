/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API - Dummy implementation
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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
#include <winpr/comm.h>

#include "comm_ioctl.h"
#include <../log.h>

#define TAG WINPR_TAG("comm")

BOOL CommDeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer,
                         DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize,
                         LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	WINPR_UNUSED(hDevice);
	WINPR_UNUSED(dwIoControlCode);
	WINPR_UNUSED(lpInBuffer);
	WINPR_UNUSED(nInBufferSize);
	WINPR_UNUSED(lpOutBuffer);
	WINPR_UNUSED(nOutBufferSize);
	WINPR_UNUSED(lpBytesReturned);
	WINPR_UNUSED(lpOverlapped);

	WLog_ERR(TAG, "TODO: Function not implemented for this platform");
	return FALSE;
}

int comm_ioctl_tcsetattr(int fd, int optional_actions, const struct termios* termios_p)
{
	WINPR_UNUSED(fd);
	WINPR_UNUSED(optional_actions);
	WINPR_UNUSED(termios_p);

	WLog_ERR(TAG, "TODO: Function not implemented for this platform");
	return -1;
}

BOOL _comm_set_permissive(HANDLE hDevice, BOOL permissive)
{
	WINPR_UNUSED(hDevice);
	WINPR_UNUSED(permissive);

	WLog_ERR(TAG, "TODO: Function not implemented for this platform");
	return FALSE;
}

BOOL CommReadFile(HANDLE hDevice, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                  LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	WINPR_UNUSED(hDevice);
	WINPR_UNUSED(lpBuffer);
	WINPR_UNUSED(nNumberOfBytesToRead);
	WINPR_UNUSED(lpNumberOfBytesRead);
	WINPR_UNUSED(lpOverlapped);

	WLog_ERR(TAG, "TODO: Function not implemented for this platform");
	return FALSE;
}

BOOL CommWriteFile(HANDLE hDevice, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                   LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	WINPR_UNUSED(hDevice);
	WINPR_UNUSED(lpBuffer);
	WINPR_UNUSED(nNumberOfBytesToWrite);
	WINPR_UNUSED(lpNumberOfBytesWritten);
	WINPR_UNUSED(lpOverlapped);

	WLog_ERR(TAG, "TODO: Function not implemented for this platform");
	return FALSE;
}

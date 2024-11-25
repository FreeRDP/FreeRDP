/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Named pipe transport
 *
 * Copyright 2023-2024 David Fort <contact@hardening-consulting.com>
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

#include "tcp.h"

#include <winpr/library.h>
#include <winpr/assert.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>

#include <freerdp/utils/ringbuffer.h>

#include "childsession.h"

#define TAG FREERDP_TAG("childsession")

typedef struct
{
	OVERLAPPED readOverlapped;
	HANDLE hFile;
	BOOL opInProgress;
	BOOL lastOpClosed;
	RingBuffer readBuffer;
	BOOL blocking;
	BYTE tmpReadBuffer[4096];

	HANDLE readEvent;
} WINPR_BIO_NAMED;

static int transport_bio_named_uninit(BIO* bio);

static int transport_bio_named_write(BIO* bio, const char* buf, int size)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(buf);

	WINPR_BIO_NAMED* ptr = (WINPR_BIO_NAMED*)BIO_get_data(bio);

	if (!buf)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE);
	DWORD written = 0;

	UINT64 start = GetTickCount64();
	BOOL ret = WriteFile(ptr->hFile, buf, size, &written, NULL);
	// winpr_HexDump(TAG, WLOG_DEBUG, buf, size);

	if (!ret)
	{
		WLog_VRB(TAG, "error or deferred");
		return 0;
	}

	WLog_VRB(TAG, "(%d)=%d written=%d duration=%d", size, ret, written, GetTickCount64() - start);

	if (written == 0)
	{
		WLog_VRB(TAG, "closed on write");
		return 0;
	}

	WINPR_ASSERT(written <= INT32_MAX);
	return (int)written;
}

static BOOL treatReadResult(WINPR_BIO_NAMED* ptr, DWORD readBytes)
{
	WLog_VRB(TAG, "treatReadResult(readBytes=%" PRIu32 ")", readBytes);
	ptr->opInProgress = FALSE;
	if (readBytes == 0)
	{
		WLog_VRB(TAG, "readBytes == 0");
		return TRUE;
	}

	if (!ringbuffer_write(&ptr->readBuffer, ptr->tmpReadBuffer, readBytes))
	{
		WLog_VRB(TAG, "ringbuffer_write()");
		return FALSE;
	}

	return SetEvent(ptr->readEvent);
}

static BOOL doReadOp(WINPR_BIO_NAMED* ptr)
{
	DWORD readBytes = 0;

	if (!ResetEvent(ptr->readEvent))
		return FALSE;

	ptr->opInProgress = TRUE;
	if (!ReadFile(ptr->hFile, ptr->tmpReadBuffer, sizeof(ptr->tmpReadBuffer), &readBytes,
	              &ptr->readOverlapped))
	{
		DWORD error = GetLastError();
		switch (error)
		{
			case ERROR_NO_DATA:
				WLog_VRB(TAG, "No Data, unexpected");
				return TRUE;
			case ERROR_IO_PENDING:
				WLog_VRB(TAG, "ERROR_IO_PENDING");
				return TRUE;
			case ERROR_BROKEN_PIPE:
				WLog_VRB(TAG, "broken pipe");
				ptr->lastOpClosed = TRUE;
				return TRUE;
			default:
				return FALSE;
		}
	}

	return treatReadResult(ptr, readBytes);
}

static int transport_bio_named_read(BIO* bio, char* buf, int size)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(buf);

	WINPR_BIO_NAMED* ptr = (WINPR_BIO_NAMED*)BIO_get_data(bio);
	if (!buf)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY | BIO_FLAGS_READ);

	if (ptr->blocking)
	{
		while (!ringbuffer_used(&ptr->readBuffer))
		{
			if (ptr->lastOpClosed)
				return 0;

			if (ptr->opInProgress)
			{
				DWORD status = WaitForSingleObjectEx(ptr->readEvent, 500, TRUE);
				switch (status)
				{
					case WAIT_TIMEOUT:
					case WAIT_IO_COMPLETION:
						continue;
					case WAIT_OBJECT_0:
						break;
					default:
						return -1;
				}

				DWORD readBytes = 0;
				if (!GetOverlappedResult(ptr->hFile, &ptr->readOverlapped, &readBytes, FALSE))
				{
					WLog_ERR(TAG, "GetOverlappedResult blocking(lastError=%" PRIu32 ")",
					         GetLastError());
					return -1;
				}

				if (!treatReadResult(ptr, readBytes))
				{
					WLog_ERR(TAG, "treatReadResult blocking");
					return -1;
				}
			}
		}
	}
	else
	{
		if (ptr->opInProgress)
		{
			DWORD status = WaitForSingleObject(ptr->readEvent, 0);
			switch (status)
			{
				case WAIT_OBJECT_0:
					break;
				case WAIT_TIMEOUT:
					BIO_set_flags(bio, (BIO_FLAGS_SHOULD_RETRY | BIO_FLAGS_READ));
					return -1;
				default:
					WLog_ERR(TAG, "error WaitForSingleObject(readEvent)=0x%" PRIx32 "", status);
					return -1;
			}

			DWORD readBytes = 0;
			if (!GetOverlappedResult(ptr->hFile, &ptr->readOverlapped, &readBytes, FALSE))
			{
				WLog_ERR(TAG, "GetOverlappedResult non blocking(lastError=%" PRIu32 ")",
				         GetLastError());
				return -1;
			}

			if (!treatReadResult(ptr, readBytes))
			{
				WLog_ERR(TAG, "error treatReadResult non blocking");
				return -1;
			}
		}
	}

	SSIZE_T ret = -1;
	if (size >= 0)
	{
		size_t rsize = ringbuffer_used(&ptr->readBuffer);
		if (rsize <= SSIZE_MAX)
			ret = MIN(size, (SSIZE_T)rsize);
	}
	if ((size >= 0) && ret)
	{
		DataChunk chunks[2] = { 0 };
		const int nchunks = ringbuffer_peek(&ptr->readBuffer, chunks, ret);
		for (int i = 0; i < nchunks; i++)
		{
			memcpy(buf, chunks[i].data, chunks[i].size);
			buf += chunks[i].size;
		}

		ringbuffer_commit_read_bytes(&ptr->readBuffer, ret);

		WLog_VRB(TAG, "(%d)=%" PRIdz " nchunks=%d", size, ret, nchunks);
	}

	if (!ringbuffer_used(&ptr->readBuffer))
	{
		if (!ptr->opInProgress && !doReadOp(ptr))
		{
			WLog_ERR(TAG, "error rearming read");
			return -1;
		}
	}

	if (ret <= 0)
		BIO_set_flags(bio, (BIO_FLAGS_SHOULD_RETRY | BIO_FLAGS_READ));

	WINPR_ASSERT(ret <= INT32_MAX);
	return (int)ret;
}

static int transport_bio_named_puts(BIO* bio, const char* str)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(str);

	return transport_bio_named_write(bio, str, (int)strnlen(str, INT32_MAX));
}

static int transport_bio_named_gets(BIO* bio, char* str, int size)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(str);

	return transport_bio_named_read(bio, str, size);
}

static long transport_bio_named_ctrl(BIO* bio, int cmd, long arg1, void* arg2)
{
	WINPR_ASSERT(bio);

	int status = -1;
	WINPR_BIO_NAMED* ptr = (WINPR_BIO_NAMED*)BIO_get_data(bio);

	switch (cmd)
	{
		case BIO_C_SET_SOCKET:
		case BIO_C_GET_SOCKET:
			return -1;
		case BIO_C_GET_EVENT:
			if (!BIO_get_init(bio) || !arg2)
				return 0;

			*((HANDLE*)arg2) = ptr->readEvent;
			return 1;
		case BIO_C_SET_HANDLE:
			BIO_set_init(bio, 1);
			if (!BIO_get_init(bio) || !arg2)
				return 0;

			ptr->hFile = (HANDLE)arg2;
			ptr->blocking = TRUE;
			if (!doReadOp(ptr))
				return -1;
			return 1;
		case BIO_C_SET_NONBLOCK:
		{
			WLog_DBG(TAG, "BIO_C_SET_NONBLOCK");
			ptr->blocking = FALSE;
			return 1;
		}
		case BIO_C_WAIT_READ:
		{
			WLog_DBG(TAG, "BIO_C_WAIT_READ");
			return 1;
		}

		case BIO_C_WAIT_WRITE:
		{
			WLog_DBG(TAG, "BIO_C_WAIT_WRITE");
			return 1;
		}

		default:
			break;
	}

	switch (cmd)
	{
		case BIO_CTRL_GET_CLOSE:
			status = BIO_get_shutdown(bio);
			break;

		case BIO_CTRL_SET_CLOSE:
			BIO_set_shutdown(bio, (int)arg1);
			status = 1;
			break;

		case BIO_CTRL_DUP:
			status = 1;
			break;

		case BIO_CTRL_FLUSH:
			status = 1;
			break;

		default:
			status = 0;
			break;
	}

	return status;
}

static void BIO_NAMED_free(WINPR_BIO_NAMED* ptr)
{
	if (!ptr)
		return;

	if (ptr->hFile)
	{
		(void)CloseHandle(ptr->hFile);
		ptr->hFile = NULL;
	}

	if (ptr->readEvent)
	{
		(void)CloseHandle(ptr->readEvent);
		ptr->readEvent = NULL;
	}

	ringbuffer_destroy(&ptr->readBuffer);
	free(ptr);
}

static int transport_bio_named_uninit(BIO* bio)
{
	WINPR_ASSERT(bio);
	WINPR_BIO_NAMED* ptr = (WINPR_BIO_NAMED*)BIO_get_data(bio);

	BIO_NAMED_free(ptr);

	BIO_set_init(bio, 0);
	BIO_set_flags(bio, 0);
	return 1;
}

static int transport_bio_named_new(BIO* bio)
{
	WINPR_ASSERT(bio);

	WINPR_BIO_NAMED* ptr = (WINPR_BIO_NAMED*)calloc(1, sizeof(WINPR_BIO_NAMED));
	if (!ptr)
		return 0;

	if (!ringbuffer_init(&ptr->readBuffer, 0xfffff))
		goto error;

	ptr->readEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!ptr->readEvent || ptr->readEvent == INVALID_HANDLE_VALUE)
		goto error;

	ptr->readOverlapped.hEvent = ptr->readEvent;

	BIO_set_data(bio, ptr);
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	return 1;

error:
	BIO_NAMED_free(ptr);
	return 0;
}

static int transport_bio_named_free(BIO* bio)
{
	WINPR_BIO_NAMED* ptr = NULL;

	if (!bio)
		return 0;

	transport_bio_named_uninit(bio);

	ptr = (WINPR_BIO_NAMED*)BIO_get_data(bio);
	if (ptr)
		BIO_set_data(bio, NULL);

	return 1;
}

static BIO_METHOD* BIO_s_namedpipe(void)
{
	static BIO_METHOD* bio_methods = NULL;

	if (bio_methods == NULL)
	{
		if (!(bio_methods = BIO_meth_new(BIO_TYPE_NAMEDPIPE, "NamedPipe")))
			return NULL;

		BIO_meth_set_write(bio_methods, transport_bio_named_write);
		BIO_meth_set_read(bio_methods, transport_bio_named_read);
		BIO_meth_set_puts(bio_methods, transport_bio_named_puts);
		BIO_meth_set_gets(bio_methods, transport_bio_named_gets);
		BIO_meth_set_ctrl(bio_methods, transport_bio_named_ctrl);
		BIO_meth_set_create(bio_methods, transport_bio_named_new);
		BIO_meth_set_destroy(bio_methods, transport_bio_named_free);
	}

	return bio_methods;
}

typedef NTSTATUS (*WinStationCreateChildSessionTransportFn)(WCHAR* path, DWORD len);
static BOOL createChildSessionTransport(HANDLE* pFile)
{
	WINPR_ASSERT(pFile);

	HANDLE hModule = NULL;
	BOOL ret = FALSE;
	*pFile = INVALID_HANDLE_VALUE;

	BOOL childEnabled = 0;
	if (!WTSIsChildSessionsEnabled(&childEnabled))
	{
		WLog_ERR(TAG, "error when calling WTSIsChildSessionsEnabled");
		goto out;
	}

	if (!childEnabled)
	{
		WLog_INFO(TAG, "child sessions aren't enabled");
		if (!WTSEnableChildSessions(TRUE))
		{
			WLog_ERR(TAG, "error when calling WTSEnableChildSessions");
			goto out;
		}
		WLog_INFO(TAG, "successfully enabled child sessions");
	}

	hModule = LoadLibraryA("winsta.dll");
	if (!hModule)
		return FALSE;
	WCHAR pipePath[0x80] = { 0 };
	char pipePathA[0x80] = { 0 };

	WinStationCreateChildSessionTransportFn createChildSessionFn = GetProcAddressAs(
	    hModule, "WinStationCreateChildSessionTransport", WinStationCreateChildSessionTransportFn);
	if (!createChildSessionFn)
	{
		WLog_ERR(TAG, "unable to retrieve WinStationCreateChildSessionTransport function");
		goto out;
	}

	HRESULT hStatus = createChildSessionFn(pipePath, 0x80);
	if (!SUCCEEDED(hStatus))
	{
		WLog_ERR(TAG, "error 0x%x when creating childSessionTransport", hStatus);
		goto out;
	}

	const BYTE startOfPath[] = { '\\', 0, '\\', 0, '.', 0, '\\', 0 };
	if (_wcsncmp(pipePath, (const WCHAR*)startOfPath, 4))
	{
		/* when compiled under 32 bits, the path may miss "\\.\" at the beginning of the string
		 * so add it if it's not there
		 */
		size_t len = _wcslen(pipePath);
		if (len > 0x80 - (4 + 1))
		{
			WLog_ERR(TAG, "pipePath is too long to be adjusted");
			goto out;
		}

		memmove(pipePath + 4, pipePath, (len + 1) * sizeof(WCHAR));
		memcpy(pipePath, startOfPath, 8);
	}

	(void)ConvertWCharNToUtf8(pipePath, 0x80, pipePathA, sizeof(pipePathA));
	WLog_DBG(TAG, "child session is at '%s'", pipePathA);

	HANDLE f = CreateFileW(pipePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
	                       FILE_FLAG_OVERLAPPED, NULL);
	if (f == INVALID_HANDLE_VALUE)
	{
		WLog_ERR(TAG, "error when connecting to local named pipe");
		goto out;
	}

	*pFile = f;
	ret = TRUE;

out:
	FreeLibrary(hModule);
	return ret;
}

BIO* createChildSessionBio(void)
{
	HANDLE f = INVALID_HANDLE_VALUE;
	if (!createChildSessionTransport(&f))
		return NULL;

	BIO* lowLevelBio = BIO_new(BIO_s_namedpipe());
	if (!lowLevelBio)
	{
		(void)CloseHandle(f);
		return NULL;
	}

	BIO_set_handle(lowLevelBio, &f);
	BIO* bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
	{
		BIO_free_all(lowLevelBio);
		return FALSE;
	}

	bufferedBio = BIO_push(bufferedBio, lowLevelBio);

	return bufferedBio;
}

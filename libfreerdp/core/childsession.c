/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Named pipe transport
 *
 * Copyright 2023 David Fort <contact@hardening-consulting.com>
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
#include "childsession.h"

#define TAG FREERDP_TAG("childsession")

typedef struct
{
	HANDLE hFile;
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

	BOOL ret = WriteFile(ptr->hFile, buf, size, &written, NULL);
	WLog_VRB(TAG, "transport_bio_named_write(%d)=%d written=%d", size, ret, written);

	if (!ret)
		return -1;

	if (written == 0)
		return -1;

	return written;
}

static int transport_bio_named_read(BIO* bio, char* buf, int size)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(buf);

	WINPR_BIO_NAMED* ptr = (WINPR_BIO_NAMED*)BIO_get_data(bio);

	if (!buf)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_READ);

	DWORD readBytes = 0;
	BOOL ret = ReadFile(ptr->hFile, buf, size, &readBytes, NULL);
	WLog_VRB(TAG, "transport_bio_named_read(%d)=%d read=%d", size, ret, readBytes);
	if (!ret)
	{
		if (GetLastError() == ERROR_NO_DATA)
			BIO_set_flags(bio, (BIO_FLAGS_SHOULD_RETRY | BIO_FLAGS_READ));
		return -1;
	}

	if (readBytes == 0)
	{
		BIO_set_flags(bio, (BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY));
		return 0;
	}

	return readBytes;
}

static int transport_bio_named_puts(BIO* bio, const char* str)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(str);

	return transport_bio_named_write(bio, str, strlen(str));
}

static int transport_bio_named_gets(BIO* bio, char* str, int size)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(str);

	return transport_bio_named_write(bio, str, size);
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

			*((HANDLE*)arg2) = ptr->hFile;
			return 1;
		case BIO_C_SET_HANDLE:
			BIO_set_init(bio, 1);
			if (!BIO_get_init(bio) || !arg2)
				return 0;

			ptr->hFile = (HANDLE)arg2;
			return 1;
		case BIO_C_SET_NONBLOCK:
		{
			return 1;
		}
		case BIO_C_WAIT_READ:
		{
			return 1;
		}

		case BIO_C_WAIT_WRITE:
		{
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

static int transport_bio_named_uninit(BIO* bio)
{
	WINPR_ASSERT(bio);
	WINPR_BIO_NAMED* ptr = (WINPR_BIO_NAMED*)BIO_get_data(bio);

	if (ptr && ptr->hFile)
	{
		CloseHandle(ptr->hFile);
		ptr->hFile = NULL;
	}

	BIO_set_init(bio, 0);
	BIO_set_flags(bio, 0);
	return 1;
}

static int transport_bio_named_new(BIO* bio)
{
	WINPR_ASSERT(bio);
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);

	WINPR_BIO_NAMED* ptr = (WINPR_BIO_NAMED*)calloc(1, sizeof(WINPR_BIO_NAMED));
	if (!ptr)
		return 0;

	BIO_set_data(bio, ptr);
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	return 1;
}

static int transport_bio_named_free(BIO* bio)
{
	WINPR_BIO_NAMED* ptr = NULL;

	if (!bio)
		return 0;

	transport_bio_named_uninit(bio);
	ptr = (WINPR_BIO_NAMED*)BIO_get_data(bio);

	if (ptr)
	{
		BIO_set_data(bio, NULL);
		free(ptr);
	}

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

	WinStationCreateChildSessionTransportFn createChildSessionFn =
	    (WinStationCreateChildSessionTransportFn)GetProcAddress(
	        hModule, "WinStationCreateChildSessionTransport");
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

	ConvertWCharNToUtf8(pipePath, 0x80, pipePathA, sizeof(pipePathA));
	WLog_DBG(TAG, "child session is at '%s'", pipePathA);

	HANDLE f = CreateFileW(pipePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
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
		CloseHandle(f);
		return NULL;
	}

	BIO_set_handle(lowLevelBio, f);
	BIO* bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
	{
		BIO_free_all(lowLevelBio);
		return FALSE;
	}

	bufferedBio = BIO_push(bufferedBio, lowLevelBio);

	return bufferedBio;
}

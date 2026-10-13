/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Native Windows ProxyCommand transport
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

#include <freerdp/config.h>

#if defined(_WIN32) && !defined(__CYGWIN__)

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/log.h>

#include <winpr/crypto.h>
#include <winpr/string.h>
#include <winpr/windows.h>
#include <winpr/wlog.h>

#include "proxy_command.h"

#define TAG FREERDP_TAG("core.proxycommand")
#define PROXY_PIPE_BUFFER_SIZE 65536U
#define PROXY_CLEANUP_TIMEOUT 1000U

#if _WIN32_WINNT >= 0x0601

/* Operations own their memory: even a defective driver failing to finish
 * cancelled I/O cannot make bounded cleanup release memory that the kernel
 * still uses. */
typedef struct
{
	OVERLAPPED overlapped;
	BOOL pending;
	BYTE buffer[PROXY_PIPE_BUFFER_SIZE];
} proxyWinOperation;

typedef struct
{
	proxyCommandBase base;
	HANDLE input;
	HANDLE output;
	HANDLE process;
	HANDLE job;
	proxyWinOperation* read;
	proxyWinOperation* write;
	DWORD readLength;
	DWORD readOffset;
	DWORD readError;
	DWORD writeLength;
} proxyWinContext;

static int proxy_win_error(DWORD error)
{
	switch (error)
	{
		case ERROR_BROKEN_PIPE:
		case ERROR_NO_DATA:
		case ERROR_PIPE_NOT_CONNECTED:
			errno = EPIPE;
			break;
		case ERROR_OPERATION_ABORTED:
			errno = ECANCELED;
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
			errno = ENOMEM;
			break;
		default:
			errno = EIO;
			break;
	}
	return -1;
}

static proxyWinOperation* proxy_win_operation_new(void)
{
	proxyWinOperation* op = calloc(1, sizeof(*op));
	if (!op)
		return nullptr;
	op->overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!op->overlapped.hEvent)
	{
		free(op);
		return nullptr;
	}
	return op;
}

static void proxy_win_operation_close(HANDLE pipe, proxyWinOperation* op)
{
	BOOL completed = TRUE;
	if (op && op->pending)
	{
		(void)CancelIoEx(pipe, &op->overlapped);
		completed =
		    WaitForSingleObject(op->overlapped.hEvent, PROXY_CLEANUP_TIMEOUT) == WAIT_OBJECT_0;
	}
	if (pipe && (pipe != INVALID_HANDLE_VALUE))
		(void)CloseHandle(pipe);
	if (!op)
		return;
	if (!completed)
	{
		completed =
		    WaitForSingleObject(op->overlapped.hEvent, PROXY_CLEANUP_TIMEOUT) == WAIT_OBJECT_0;
	}
	if (!completed)
	{
		/* Retain both OVERLAPPED and buffer rather than risk a kernel
		 * use-after-free. */
		WLog_ERR(TAG, "ProxyCommand pipe cancellation did not complete; retaining "
		              "I/O buffer");
		return;
	}
	(void)CloseHandle(op->overlapped.hEvent);
	free(op);
}

static BOOL proxy_win_close(void* userContext)
{
	proxyWinContext* proxy = userContext;
	if (!proxy)
		return FALSE;
	/* The job owns the complete command process tree, including descendants. */
	if (proxy->job)
	{
		(void)CloseHandle(proxy->job);
		proxy->job = nullptr;
	}
	if (proxy->process)
	{
		/* Also covers CreateProcess succeeding but AssignProcessToJobObject
		 * failing. */
		(void)TerminateProcess(proxy->process, 1);
		(void)WaitForSingleObject(proxy->process, PROXY_CLEANUP_TIMEOUT);
		(void)CloseHandle(proxy->process);
		proxy->process = nullptr;
	}
	proxy_win_operation_close(proxy->output, proxy->read);
	proxy_win_operation_close(proxy->input, proxy->write);
	proxy->input = nullptr;
	proxy->output = nullptr;
	proxy->read = nullptr;
	proxy->write = nullptr;
	return TRUE;
}

/* Keep a read posted even between BIO reads, so GetEvent is a real readiness
 * event. */
static void proxy_win_read_start(proxyWinContext* proxy)
{
	proxyWinOperation* op = proxy->read;
	if (op->pending || proxy->readLength || proxy->readError)
		return;
	DWORD count = 0;
	(void)ResetEvent(op->overlapped.hEvent);
	if (ReadFile(proxy->output, op->buffer, sizeof(op->buffer), &count, &op->overlapped))
	{
		proxy->readLength = count;
		proxy->readOffset = 0;
		if (!count)
			proxy->readError = ERROR_BROKEN_PIPE;
		(void)SetEvent(op->overlapped.hEvent);
		return;
	}
	const DWORD error = GetLastError();
	if (error == ERROR_IO_PENDING)
		op->pending = TRUE;
	else
	{
		proxy->readError = error;
		(void)SetEvent(op->overlapped.hEvent);
	}
}

static int proxy_win_read(void* userContext, void* data, int bytes)
{
	proxyWinContext* proxy = userContext;
	if (!proxy || (bytes < 0))
	{
		errno = EINVAL;
		return -1;
	}
	if (!data || !bytes)
		return 0;
	if (!proxy_command_check(&proxy->base))
		return -1;
	proxy_win_read_start(proxy);
	proxyWinOperation* op = proxy->read;
	if (op->pending)
	{
		DWORD count = 0;
		if (!GetOverlappedResult(proxy->output, &op->overlapped, &count, FALSE))
		{
			const DWORD error = GetLastError();
			if (error == ERROR_IO_INCOMPLETE)
			{
				errno = EAGAIN;
				return -1;
			}
			proxy->readError = error;
		}
		else
		{
			proxy->readLength = count;
			proxy->readOffset = 0;
			if (!count)
				proxy->readError = ERROR_BROKEN_PIPE;
		}
		op->pending = FALSE;
	}
	if (proxy->readError)
	{
		if ((proxy->readError == ERROR_BROKEN_PIPE) || (proxy->readError == ERROR_NO_DATA) ||
		    (proxy->readError == ERROR_PIPE_NOT_CONNECTED))
			return 0;
		return proxy_win_error(proxy->readError);
	}
	DWORD count = proxy->readLength - proxy->readOffset;
	if (count > (DWORD)bytes)
		count = (DWORD)bytes;
	memcpy(data, op->buffer + proxy->readOffset, count);
	proxy->readOffset += count;
	proxy_command_mark_received(&proxy->base);
	if (proxy->readOffset == proxy->readLength)
	{
		proxy->readLength = 0;
		proxy->readOffset = 0;
		proxy_win_read_start(proxy);
	}
	return (int)count;
}

static int proxy_win_write(void* userContext, const void* data, int bytes)
{
	proxyWinContext* proxy = userContext;
	if (!proxy || (bytes < 0))
	{
		errno = EINVAL;
		return -1;
	}
	if (!data || !bytes)
		return 0;
	if (!proxy_command_check(&proxy->base))
		return -1;
	proxyWinOperation* op = proxy->write;
	DWORD count = 0;
	if (op->pending)
	{
		/* A retry must describe the same bytes, even if BIO moved the buffer. */
		if (((DWORD)bytes < proxy->writeLength) ||
		    (memcmp(data, op->buffer, proxy->writeLength) != 0))
		{
			errno = EINVAL;
			return -1;
		}
		if (!GetOverlappedResult(proxy->input, &op->overlapped, &count, FALSE))
		{
			const DWORD error = GetLastError();
			if (error == ERROR_IO_INCOMPLETE)
			{
				errno = EAGAIN;
				return -1;
			}
			op->pending = FALSE;
			return proxy_win_error(error);
		}
		op->pending = FALSE;
		return (int)count;
	}
	proxy->writeLength = (DWORD)bytes;
	if (proxy->writeLength > sizeof(op->buffer))
		proxy->writeLength = sizeof(op->buffer);
	memcpy(op->buffer, data, proxy->writeLength);
	(void)ResetEvent(op->overlapped.hEvent);
	if (WriteFile(proxy->input, op->buffer, proxy->writeLength, &count, &op->overlapped))
		return (int)count;
	const DWORD error = GetLastError();
	if (error != ERROR_IO_PENDING)
		return proxy_win_error(error);
	op->pending = TRUE;
	errno = EAGAIN;
	return -1;
}

static BOOL proxy_win_wait(void* userContext, BOOL waitWrite, DWORD timeout)
{
	proxyWinContext* proxy = userContext;
	if (!proxy || !proxy_command_check(&proxy->base))
		return FALSE;
	proxyWinOperation* op = waitWrite ? proxy->write : proxy->read;
	if (waitWrite && !op->pending)
		return TRUE;
	if (!waitWrite)
		proxy_win_read_start(proxy);
	HANDLE events[2] = { op->overlapped.hEvent, freerdp_abort_event(proxy->base.context) };
	const ULONGLONG started = GetTickCount64();
	do
	{
		const ULONGLONG elapsed = GetTickCount64() - started;
		const DWORD remaining = (timeout == INFINITE) ? INFINITE
		                        : (elapsed < timeout) ? timeout - (DWORD)elapsed
		                                              : 0;
		const DWORD result = WaitForMultipleObjects(
		    ARRAYSIZE(events), events, FALSE, proxy_command_wait_timeout(&proxy->base, remaining));
		if (!proxy_command_check(&proxy->base))
			return FALSE;
		if (result == WAIT_OBJECT_0)
			return TRUE;
		if (result == WAIT_FAILED)
		{
			(void)proxy_win_error(GetLastError());
			return FALSE;
		}
		if (result != WAIT_TIMEOUT)
		{
			errno = ECANCELED;
			return FALSE;
		}
	} while ((timeout == INFINITE) || (GetTickCount64() - started < timeout));
	errno = ETIMEDOUT;
	return FALSE;
}

static HANDLE proxy_win_get_event(void* userContext)
{
	proxyWinContext* proxy = userContext;
	if (!proxy)
		return nullptr;
	proxy_win_read_start(proxy);
	return proxy->read->overlapped.hEvent;
}

/* Use unpredictable private pipe names, first-instance protection, and reject
 * remote clients. Parent endpoints are not inheritable. */
static BOOL proxy_win_pipe(BOOL parentReads, HANDLE* parent, HANDLE* child, proxyWinOperation* op)
{
	BYTE random[16] = { 0 };
	char name[128] = { 0 };
	char hex[33] = { 0 };
	SECURITY_ATTRIBUTES attributes = { sizeof(attributes), nullptr, TRUE };
	if (winpr_RAND(random, sizeof(random)) < 0)
		return FALSE;
	for (size_t x = 0; x < sizeof(random); x++)
		(void)snprintf(hex + x * 2, sizeof(hex) - x * 2, "%02x", random[x]);
	(void)snprintf(name, sizeof(name), "\\\\.\\pipe\\FreeRDP-ProxyCommand-%lu-%s",
	               (unsigned long)GetCurrentProcessId(), hex);
	const DWORD access = parentReads ? PIPE_ACCESS_INBOUND : PIPE_ACCESS_OUTBOUND;
	*parent = CreateNamedPipeA(name, access | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
	                           PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
	                               PIPE_REJECT_REMOTE_CLIENTS,
	                           1, PROXY_PIPE_BUFFER_SIZE, PROXY_PIPE_BUFFER_SIZE, 0, nullptr);
	if (*parent == INVALID_HANDLE_VALUE)
		return FALSE;
	*child = CreateFileA(name, parentReads ? GENERIC_WRITE : GENERIC_READ, 0, &attributes,
	                     OPEN_EXISTING, 0, nullptr);
	if (*child == INVALID_HANDLE_VALUE)
		return FALSE;
	/* CreateFile above has already connected. Keep the OVERLAPPED alive even if
	 * an unexpected pending connect requires cancellation on the failure path. */
	if (!ConnectNamedPipe(*parent, &op->overlapped))
	{
		const DWORD error = GetLastError();
		if (error != ERROR_PIPE_CONNECTED)
		{
			op->pending = (error == ERROR_IO_PENDING);
			return FALSE;
		}
	}
	return TRUE;
}

static HANDLE proxy_win_stderr(void)
{
	HANDLE result = nullptr;
	HANDLE source = GetStdHandle(STD_ERROR_HANDLE);
	if (source && (source != INVALID_HANDLE_VALUE) &&
	    DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &result, 0, TRUE,
	                    DUPLICATE_SAME_ACCESS))
		return result;
	SECURITY_ATTRIBUTES attributes = { sizeof(attributes), nullptr, TRUE };
	return CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes,
	                   OPEN_EXISTING, 0, nullptr);
}

rdpTransportLayer* proxy_command_platform_connect(rdpContext* context, const char* command,
                                                  DWORD timeout)
{
	rdpTransportLayer* layer =
	    transport_layer_new(freerdp_get_transport(context), sizeof(proxyWinContext));
	if (!layer)
		return nullptr;
	proxyWinContext* proxy = layer->userContext;
	layer->Read = proxy_win_read;
	layer->Write = proxy_win_write;
	layer->Wait = proxy_win_wait;
	layer->GetEvent = proxy_win_get_event;
	layer->Close = proxy_win_close;
	proxy->base.context = context;
	proxy->base.deadline = timeout ? GetTickCount64() + timeout : 0;
	STARTUPINFOEXW startup = { 0 };
	PROCESS_INFORMATION process = { 0 };
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = { 0 };
	HANDLE inherited[3] = { nullptr, nullptr, nullptr };
	WCHAR* wideCommand = nullptr;
	BOOL attributesReady = FALSE;
	SIZE_T attributesSize = 0;
	BOOL success = FALSE;
	DWORD error = ERROR_SUCCESS;
	startup.StartupInfo.cb = sizeof(startup);
	startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	proxy->read = proxy_win_operation_new();
	proxy->write = proxy_win_operation_new();
	if (!proxy->read || !proxy->write)
		goto out;
	if (!proxy_win_pipe(FALSE, &proxy->input, &inherited[0], proxy->write) ||
	    !proxy_win_pipe(TRUE, &proxy->output, &inherited[1], proxy->read))
		goto out;
	inherited[2] = proxy_win_stderr();
	if (!inherited[2] || (inherited[2] == INVALID_HANDLE_VALUE))
		goto out;
	startup.StartupInfo.hStdInput = inherited[0];
	startup.StartupInfo.hStdOutput = inherited[1];
	startup.StartupInfo.hStdError = inherited[2];
	(void)InitializeProcThreadAttributeList(nullptr, 1, 0, &attributesSize);
	startup.lpAttributeList = malloc(attributesSize);
	if (!startup.lpAttributeList ||
	    !InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributesSize))
		goto out;
	attributesReady = TRUE;
	if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
	                               inherited, sizeof(inherited), nullptr, nullptr))
		goto out;
	wideCommand = ConvertUtf8ToWCharAlloc(command, nullptr);
	if (!wideCommand)
		goto out;
	proxy->job = CreateJobObjectW(nullptr, nullptr);
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!proxy->job || !SetInformationJobObject(proxy->job, JobObjectExtendedLimitInformation,
	                                            &limits, sizeof(limits)))
		goto out;
	/* No implicit shell: cmd.exe /c or PowerShell must be requested explicitly.
	 */
	if (!CreateProcessW(nullptr, wideCommand, nullptr, nullptr, TRUE,
	                    EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, nullptr, nullptr,
	                    &startup.StartupInfo, &process))
		goto out;
	proxy->process = process.hProcess;
	if (!AssignProcessToJobObject(proxy->job, proxy->process))
		goto out;
	if (!proxy_command_check(&proxy->base))
		goto out;
	if (ResumeThread(process.hThread) == (DWORD)-1)
		goto out;
	success = TRUE;
out:
	error = GetLastError();
	if (process.hThread)
		(void)CloseHandle(process.hThread);
	for (size_t x = 0; x < ARRAYSIZE(inherited); x++)
	{
		if (inherited[x] && (inherited[x] != INVALID_HANDLE_VALUE))
			(void)CloseHandle(inherited[x]);
	}
	if (attributesReady)
		DeleteProcThreadAttributeList(startup.lpAttributeList);
	free(startup.lpAttributeList);
	free(wideCommand);
	if (!success)
	{
		WLog_ERR(TAG, "Could not start ProxyCommand process (Windows error %lu)",
		         (unsigned long)error);
		transport_layer_free(layer);
		return nullptr;
	}
	proxy_win_read_start(proxy);
	return layer;
}

#else

rdpTransportLayer* proxy_command_platform_connect(WINPR_ATTR_UNUSED rdpContext* context,
                                                  WINPR_ATTR_UNUSED const char* command,
                                                  WINPR_ATTR_UNUSED DWORD timeout)
{
	WLog_ERR(TAG, "ProxyCommand requires Windows 7 or newer");
	errno = ENOTSUP;
	return nullptr;
}

#endif /* _WIN32_WINNT >= 0x0601 */
#endif /* native Windows */

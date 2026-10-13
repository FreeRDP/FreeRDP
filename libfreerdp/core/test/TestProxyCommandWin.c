/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Native Windows ProxyCommand transport lifecycle tests
 * SPDX-License-Identifier: Apache-2.0
 */

#include <freerdp/config.h>
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/transport_io.h>
#include <winpr/windows.h>
#include <winpr/string.h>
#include <winpr/sysinfo.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define TEST_TIMEOUT 10000U
#define DUPLEX_LENGTH (2U * 1024U * 1024U)

static char* helperPath;

static BOOL proxy_test_helper_path(void)
{
	WCHAR executable[32768] = { 0 };
	const DWORD length = GetModuleFileNameW(NULL, executable, ARRAYSIZE(executable));
	if (!length || length >= ARRAYSIZE(executable))
		return FALSE;
	WCHAR* slash = wcsrchr(executable, L'\\');
	if (!slash)
		return FALSE;
	const size_t prefix = (size_t)(slash + 1 - executable);
	const WCHAR filename[] = L"TestProxyCommandHelper.exe";
	if (prefix + ARRAYSIZE(filename) > ARRAYSIZE(executable))
		return FALSE;
	memcpy(executable + prefix, filename, sizeof(filename));
	if (GetFileAttributesW(executable) == INVALID_FILE_ATTRIBUTES)
		return FALSE;
	helperPath = ConvertWCharToUtf8Alloc(executable, NULL);
	return helperPath != NULL;
}

static freerdp* proxy_test_new(void)
{
	freerdp* instance = freerdp_new();
	if (!instance)
		return NULL;
	if (!freerdp_context_new(instance))
	{
		freerdp_free(instance);
		return NULL;
	}
	return instance;
}

static void proxy_test_free(freerdp* instance)
{
	if (!instance)
		return;
	freerdp_context_free(instance);
	freerdp_free(instance);
}

static rdpTransportLayer* proxy_test_connect(freerdp* instance, const char* arguments,
                                             const char* host, int port, DWORD timeout)
{
	const size_t pathLength = strlen(helperPath);
	const size_t argumentsLength = strlen(arguments);
	/* Escape a percent in the executable path for the template token expander. */
	char* command = malloc(pathLength * 2 + argumentsLength + 5);
	if (!command)
		return NULL;
	size_t offset = 0;
	command[offset++] = '"';
	for (size_t x = 0; x < pathLength; x++)
	{
		if (helperPath[x] == '%')
			command[offset++] = '%';
		command[offset++] = helperPath[x];
	}
	command[offset++] = '"';
	command[offset++] = ' ';
	memcpy(command + offset, arguments, argumentsLength + 1);
	const BOOL configured =
	    freerdp_settings_set_string(instance->context->settings, FreeRDP_ProxyCommand, command);
	free(command);
	if (!configured)
		return NULL;
	const rdpTransportIo* io = freerdp_get_io_callbacks(instance->context);
	if (!io || !io->ConnectLayer)
		return NULL;
	return io->ConnectLayer(freerdp_get_transport(instance->context), host, port, timeout);
}

static BOOL proxy_test_read(rdpTransportLayer* layer, void* data, size_t length)
{
	BYTE* buffer = data;
	size_t offset = 0;
	const ULONGLONG deadline = GetTickCount64() + TEST_TIMEOUT;
	while (offset < length && GetTickCount64() < deadline)
	{
		errno = 0;
		const int rc = layer->Read(layer->userContext, buffer + offset, (int)(length - offset));
		if (rc > 0)
			offset += (size_t)rc;
		else if (rc == 0 || errno != EAGAIN)
			return FALSE;
		else
			(void)layer->Wait(layer->userContext, FALSE, 20);
	}
	return offset == length;
}

static BOOL proxy_test_write(rdpTransportLayer* layer, const void* data, size_t length)
{
	const BYTE* buffer = data;
	size_t offset = 0;
	const ULONGLONG deadline = GetTickCount64() + TEST_TIMEOUT;
	while (offset < length && GetTickCount64() < deadline)
	{
		errno = 0;
		const int rc = layer->Write(layer->userContext, buffer + offset, (int)(length - offset));
		if (rc > 0)
			offset += (size_t)rc;
		else if (rc == 0 || errno != EAGAIN)
			return FALSE;
		else
			(void)layer->Wait(layer->userContext, TRUE, 20);
	}
	return offset == length;
}

static BYTE proxy_test_byte(size_t offset)
{
	return (BYTE)((offset * 17U + (offset >> 8U)) & 0xffU);
}

static BOOL proxy_test_duplex(freerdp* instance)
{
	BOOL ok = FALSE;
	BOOL partialWrite = FALSE;
	BYTE* expected = malloc(DUPLEX_LENGTH);
	BYTE actual[65537];
	rdpTransportLayer* layer = NULL;
	if (!expected)
		return FALSE;
	for (size_t x = 0; x < DUPLEX_LENGTH; x++)
		expected[x] = proxy_test_byte(x);
	layer = proxy_test_connect(instance, "echo", "remote.invalid", 3389, TEST_TIMEOUT);
	if (!layer || !layer->GetEvent(layer->userContext))
		goto out;
	size_t written = 0;
	size_t received = 0;
	const ULONGLONG deadline = GetTickCount64() + TEST_TIMEOUT;
	/* Pump both directions in one thread. A blocked stdout cannot deadlock stdin. */
	while (received < DUPLEX_LENGTH && GetTickCount64() < deadline)
	{
		BOOL progress = FALSE;
		if (written < DUPLEX_LENGTH)
		{
			const size_t request = DUPLEX_LENGTH - written;
			errno = 0;
			const int rc = layer->Write(layer->userContext, expected + written, (int)request);
			if (rc > 0)
			{
				if ((size_t)rc < request)
					partialWrite = TRUE;
				written += (size_t)rc;
				progress = TRUE;
			}
			else if (rc == 0 || errno != EAGAIN)
				goto out;
		}
		errno = 0;
		const int rc = layer->Read(layer->userContext, actual, sizeof(actual));
		if (rc > 0)
		{
			if ((size_t)rc > DUPLEX_LENGTH - received ||
			    memcmp(actual, expected + received, (size_t)rc) != 0)
				goto out;
			received += (size_t)rc;
			progress = TRUE;
		}
		else if (rc == 0 || errno != EAGAIN)
			goto out;
		if (!progress)
			(void)layer->Wait(layer->userContext, FALSE, 5);
	}
	ok = received == DUPLEX_LENGTH && written == DUPLEX_LENGTH && partialWrite;
out:
	transport_layer_free(layer);
	free(expected);
	return ok;
}

static BOOL proxy_test_tokens(freerdp* instance)
{
	const char expected[] = "remote.invalid:3390:%";
	char actual[sizeof(expected) - 1] = { 0 };
	rdpTransportLayer* layer = proxy_test_connect(instance, "tokens \"%h\" \"%p\" \"%%\"",
	                                              "remote.invalid", 3390, TEST_TIMEOUT);
	if (!layer)
		return FALSE;
	const BOOL ok =
	    WaitForSingleObject(layer->GetEvent(layer->userContext), TEST_TIMEOUT) == WAIT_OBJECT_0 &&
	    proxy_test_read(layer, actual, sizeof(actual)) && !memcmp(expected, actual, sizeof(actual));
	transport_layer_free(layer);
	return ok;
}

static BOOL proxy_test_eof(freerdp* instance)
{
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "exit", "remote.invalid", 3389, TEST_TIMEOUT);
	if (!layer)
		return FALSE;
	BOOL ok = FALSE;
	BYTE byte = 0;
	const ULONGLONG deadline = GetTickCount64() + TEST_TIMEOUT;
	while (GetTickCount64() < deadline)
	{
		errno = 0;
		const int rc = layer->Read(layer->userContext, &byte, 1);
		if (!rc)
		{
			ok = TRUE;
			break;
		}
		if (rc > 0 || errno != EAGAIN)
			break;
		(void)layer->Wait(layer->userContext, FALSE, 20);
	}
	transport_layer_free(layer);
	return ok;
}

static BOOL proxy_test_timeout(freerdp* instance)
{
	const DWORD timeout = 300;
	const ULONGLONG started = GetTickCount64();
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "sleep", "remote.invalid", 3389, timeout);
	if (!layer)
		return GetTickCount64() - started >= timeout &&
		       freerdp_get_last_error(instance->context) == FREERDP_ERROR_CONNECT_TRANSPORT_FAILED;
	BYTE byte = 0;
	BOOL ok = FALSE;
	while (GetTickCount64() - started < TEST_TIMEOUT)
	{
		errno = 0;
		const int rc = layer->Read(layer->userContext, &byte, 1);
		if (rc < 0 && errno == ETIMEDOUT)
		{
			ok = GetTickCount64() - started >= timeout;
			break;
		}
		if (rc >= 0 || errno != EAGAIN)
			break;
		(void)layer->Wait(layer->userContext, FALSE, 20);
	}
	transport_layer_free(layer);
	return ok && GetTickCount64() - started < TEST_TIMEOUT;
}

static BOOL proxy_test_established_deadline(freerdp* instance)
{
	const DWORD timeout = 2000;
	const BYTE expected = 0xA5;
	BYTE actual = 0;
	BOOL ok = FALSE;
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "echo", "remote.invalid", 3389, timeout);
	if (!layer)
		return FALSE;
	if (!proxy_test_write(layer, &expected, 1) || !proxy_test_read(layer, &actual, 1) ||
	    actual != expected)
		goto out;
	Sleep(timeout + 50);
	errno = 0;
	if (layer->Read(layer->userContext, &actual, 1) >= 0 || errno != EAGAIN)
		goto out;
	if (!proxy_test_write(layer, &expected, 1) || !proxy_test_read(layer, &actual, 1) ||
	    actual != expected)
		goto out;
	ok = TRUE;
out:
	transport_layer_free(layer);
	return ok;
}

static DWORD WINAPI proxy_test_abort_thread(void* argument)
{
	Sleep(100);
	return freerdp_abort_connect_context(argument) ? 0 : 1;
}

static BOOL proxy_test_abort(freerdp* instance)
{
	BOOL ok = FALSE;
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "sleep", "remote.invalid", 3389, TEST_TIMEOUT);
	if (!layer)
		return FALSE;
	HANDLE thread = CreateThread(NULL, 0, proxy_test_abort_thread, instance->context, 0, NULL);
	if (!thread)
		goto out;
	const ULONGLONG started = GetTickCount64();
	errno = 0;
	const BOOL ready = layer->Wait(layer->userContext, FALSE, INFINITE);
	BYTE byte = 0;
	ok = !ready && errno == ECANCELED && GetTickCount64() - started < 1000;
	errno = 0;
	ok = ok && layer->Read(layer->userContext, &byte, 1) < 0 && errno == ECANCELED;
	if (WaitForSingleObject(thread, TEST_TIMEOUT) != WAIT_OBJECT_0)
		ok = FALSE;
	CloseHandle(thread);
out:
	transport_layer_free(layer);
	return ok;
}

static BOOL proxy_test_tree_cleanup(freerdp* instance)
{
	DWORD pids[2] = { 0 };
	HANDLE processes[2] = { NULL, NULL };
	BOOL ok = FALSE;
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "tree", "remote.invalid", 3389, TEST_TIMEOUT);
	if (!layer || !proxy_test_read(layer, pids, sizeof(pids)))
		goto out;
	for (size_t x = 0; x < ARRAYSIZE(pids); x++)
	{
		processes[x] = OpenProcess(SYNCHRONIZE, FALSE, pids[x]);
		if (!processes[x] || WaitForSingleObject(processes[x], 0) != WAIT_TIMEOUT)
			goto out;
	}
	const ULONGLONG started = GetTickCount64();
	transport_layer_free(layer);
	layer = NULL;
	ok = GetTickCount64() - started < 6000;
	for (size_t x = 0; x < ARRAYSIZE(processes); x++)
		ok = ok && WaitForSingleObject(processes[x], TEST_TIMEOUT) == WAIT_OBJECT_0;
out:
	transport_layer_free(layer);
	for (size_t x = 0; x < ARRAYSIZE(processes); x++)
		if (processes[x])
			CloseHandle(processes[x]);
	return ok;
}

static BOOL proxy_test_repeat(freerdp* instance)
{
	DWORD before = 0;
	DWORD after = 0;
	if (!proxy_test_tokens(instance) || !GetProcessHandleCount(GetCurrentProcess(), &before))
		return FALSE;
	for (size_t x = 0; x < 8; x++)
		if (!proxy_test_tokens(instance))
			return FALSE;
	if (!GetProcessHandleCount(GetCurrentProcess(), &after))
		return FALSE;
	if (before != after)
	{
		fprintf(stderr, "ProxyCommand handle count before=%lu after=%lu\n", (unsigned long)before,
		        (unsigned long)after);
		return FALSE;
	}
	return TRUE;
}

static BOOL proxy_test_invalid(freerdp* instance)
{
	const char* arguments[] = { "echo %x", "echo %" };
	const char* hosts[] = { "host;id",     "host$(id)",  "host`id`", "host'quote",
		                    "host\"quote", "host\nname", "host name" };
	for (size_t x = 0; x < ARRAYSIZE(arguments); x++)
	{
		rdpTransportLayer* layer =
		    proxy_test_connect(instance, arguments[x], "remote.invalid", 3389, TEST_TIMEOUT);
		if (layer)
		{
			transport_layer_free(layer);
			return FALSE;
		}
	}
	for (size_t x = 0; x < ARRAYSIZE(hosts); x++)
	{
		rdpTransportLayer* layer =
		    proxy_test_connect(instance, "echo", hosts[x], 3389, TEST_TIMEOUT);
		if (layer)
		{
			transport_layer_free(layer);
			return FALSE;
		}
	}
	return TRUE;
}

int TestProxyCommandWin(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	if (!proxy_test_helper_path())
	{
		fprintf(stderr, "Missing sibling TestProxyCommandHelper.exe\n");
		return -1;
	}
	struct
	{
		const char* name;
		BOOL (*test)(freerdp*);
	} tests[] = { { "large duplex", proxy_test_duplex },
		          { "tokens/event", proxy_test_tokens },
		          { "EOF", proxy_test_eof },
		          { "startup timeout", proxy_test_timeout },
		          { "established deadline", proxy_test_established_deadline },
		          { "abort", proxy_test_abort },
		          { "job descendants", proxy_test_tree_cleanup },
		          { "handle cleanup", proxy_test_repeat },
		          { "invalid substitutions", proxy_test_invalid } };
	int result = 0;
	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		freerdp* instance = proxy_test_new();
		if (!instance)
		{
			result = -1;
			break;
		}
		const BOOL ok = tests[x].test(instance);
		proxy_test_free(instance);
		if (!ok)
		{
			fprintf(stderr, "ProxyCommand Windows %s failed\n", tests[x].name);
			result = -1;
			break;
		}
	}
	free(helperPath);
	helperPath = NULL;
	return result;
}

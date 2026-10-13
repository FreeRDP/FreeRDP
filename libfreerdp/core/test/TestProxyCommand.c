/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ProxyCommand transport lifecycle tests
 *
 * Licensed under the Apache License, Version 2.0.
 */

#include <freerdp/config.h>
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/transport_io.h>
#include <winpr/sysinfo.h>
#include <winpr/synch.h>

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_TIMEOUT 2000

static freerdp* proxy_test_new(void)
{
	freerdp* instance = freerdp_new();
	if (!instance)
		return nullptr;
	if (!freerdp_context_new(instance))
	{
		freerdp_free(instance);
		return nullptr;
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

static rdpTransportLayer* proxy_test_connect(freerdp* instance, const char* command,
                                             const char* host, int port, DWORD timeout)
{
	if (!freerdp_settings_set_string(instance->context->settings, FreeRDP_ProxyCommand, command))
		return nullptr;
	const rdpTransportIo* io = freerdp_get_io_callbacks(instance->context);
	if (!io || !io->ConnectLayer)
		return nullptr;
	return io->ConnectLayer(freerdp_get_transport(instance->context), host, port, timeout);
}

static BOOL proxy_test_read(rdpTransportLayer* layer, void* data, size_t bytes)
{
	BYTE* buffer = data;
	size_t offset = 0;
	const UINT64 deadline = GetTickCount64() + TEST_TIMEOUT;
	while (offset < bytes && GetTickCount64() < deadline)
	{
		const int rc = layer->Read(layer->userContext, buffer + offset, (int)(bytes - offset));
		if (rc > 0)
			offset += (size_t)rc;
		else if (rc == 0 || errno != EAGAIN)
			return FALSE;
		else
			(void)layer->Wait(layer->userContext, FALSE, 20);
	}
	return offset == bytes;
}

static BOOL proxy_test_roundtrip(freerdp* instance)
{
	BOOL ok = FALSE;
	BYTE expected[1024];
	BYTE actual[sizeof(expected)] = WINPR_C_ARRAY_INIT;
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "exec cat", "remote.invalid", 3389, TEST_TIMEOUT);
	if (!layer)
		return FALSE;
	for (size_t x = 0; x < sizeof(expected); x++)
		expected[x] = (BYTE)x;
	if (!layer->GetEvent(layer->userContext))
		goto out;

	size_t offset = 0;
	const UINT64 deadline = GetTickCount64() + TEST_TIMEOUT;
	while (offset < sizeof(expected) && GetTickCount64() < deadline)
	{
		const int rc =
		    layer->Write(layer->userContext, expected + offset, (int)(sizeof(expected) - offset));
		if (rc > 0)
			offset += (size_t)rc;
		else if (errno != EAGAIN)
			goto out;
		else
			(void)layer->Wait(layer->userContext, TRUE, 20);
	}
	if (offset != sizeof(expected))
		goto out;
	if (WaitForSingleObject(layer->GetEvent(layer->userContext), TEST_TIMEOUT) != WAIT_OBJECT_0)
		goto out;
	if (!proxy_test_read(layer, actual, sizeof(actual)))
		goto out;
	ok = memcmp(expected, actual, sizeof(expected)) == 0;
out:
	transport_layer_free(layer);
	return ok;
}

static BOOL proxy_test_tokens(freerdp* instance)
{
	/* Escape printf's percent signs for the ProxyCommand token expander. */
	const char* command = "printf '%%s:%%s:%%%%' '%h' '%p'";
	const char expected[] = "remote.invalid:3390:%";
	char actual[sizeof(expected) - 1] = WINPR_C_ARRAY_INIT;
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, command, "remote.invalid", 3390, TEST_TIMEOUT);
	if (!layer)
		return FALSE;
	const BOOL ok = proxy_test_read(layer, actual, sizeof(actual)) &&
	                memcmp(expected, actual, sizeof(actual)) == 0;
	transport_layer_free(layer);
	return ok;
}

static BOOL proxy_test_eof(freerdp* instance)
{
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "exit 7", "remote.invalid", 3389, TEST_TIMEOUT);
	if (!layer)
		return FALSE;
	BYTE byte = 0;
	BOOL ok = FALSE;
	const UINT64 deadline = GetTickCount64() + TEST_TIMEOUT;
	while (GetTickCount64() < deadline)
	{
		const int rc = layer->Read(layer->userContext, &byte, 1);
		if (rc == 0)
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
	const DWORD timeout = 100;
	const UINT64 started = GetTickCount64();
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "exec sleep 30", "remote.invalid", 3389, timeout);
	if (!layer)
		return FALSE;
	BOOL ok = FALSE;
	BYTE byte = 0;
	while (GetTickCount64() - started < TEST_TIMEOUT)
	{
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
	const DWORD timeout = 500;
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "exec cat", "remote.invalid", 3389, timeout);
	if (!layer)
		return FALSE;
	BOOL ok = FALSE;
	const BYTE expected = 0xA5;
	BYTE actual = 0;
	if (layer->Write(layer->userContext, &expected, 1) != 1 ||
	    !proxy_test_read(layer, &actual, 1) || actual != expected)
		goto out;
	/* Once traffic has arrived, the startup deadline must not expire an idle session. */
	Sleep(timeout + 50);
	errno = 0;
	if (layer->Read(layer->userContext, &actual, 1) >= 0 || errno != EAGAIN)
		goto out;
	if (layer->Write(layer->userContext, &expected, 1) != 1 ||
	    !proxy_test_read(layer, &actual, 1) || actual != expected)
		goto out;
	ok = TRUE;
out:
	transport_layer_free(layer);
	return ok;
}

static BOOL proxy_test_invalid(freerdp* instance)
{
	const char* commands[] = { "exec cat %x", "exec cat %" };
	const char* hosts[] = { "host;id",     "host$(id)",  "host`id`", "host'quote",
		                    "host\"quote", "host\nname", "host name" };
	for (size_t x = 0; x < ARRAYSIZE(commands); x++)
	{
		rdpTransportLayer* layer =
		    proxy_test_connect(instance, commands[x], "remote.invalid", 3389, TEST_TIMEOUT);
		if (layer)
		{
			transport_layer_free(layer);
			return FALSE;
		}
	}
	for (size_t x = 0; x < ARRAYSIZE(hosts); x++)
	{
		rdpTransportLayer* layer =
		    proxy_test_connect(instance, "exec cat", hosts[x], 3389, TEST_TIMEOUT);
		if (layer)
		{
			transport_layer_free(layer);
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL proxy_test_stubborn_cleanup(freerdp* instance)
{
	/* The shell's ignored SIGTERM disposition survives exec; cat has no descendants. */
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "trap '' TERM; printf '%%s\\n' \"$$\"; exec cat",
	                       "remote.invalid", 3389, TEST_TIMEOUT);
	if (!layer)
		return FALSE;
	char pidText[32] = WINPR_C_ARRAY_INIT;
	BOOL havePid = FALSE;
	for (size_t x = 0; x + 1 < sizeof(pidText); x++)
	{
		if (!proxy_test_read(layer, &pidText[x], 1))
			break;
		if (pidText[x] == '\n')
		{
			havePid = TRUE;
			break;
		}
	}
	const pid_t pid = (pid_t)strtol(pidText, nullptr, 10);
	const UINT64 started = GetTickCount64();
	transport_layer_free(layer);
	if (!havePid || pid <= 1 || GetTickCount64() - started >= TEST_TIMEOUT)
		return FALSE;
	if (kill(pid, 0) != -1 || errno != ESRCH)
		return FALSE;
	int status = 0;
	return waitpid(pid, &status, WNOHANG) == -1 && errno == ECHILD;
}

static int proxy_test_fd_count(void)
{
	DIR* directory = opendir("/proc/self/fd");
	if (!directory)
		directory = opendir("/dev/fd");
	if (!directory)
		return -1;
	int count = 0;
	struct dirent* entry = nullptr;
	while ((entry = readdir(directory)))
	{
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
			count++;
	}
	closedir(directory);
	return count;
}

static BOOL proxy_test_repeat(freerdp* instance)
{
	/* Warm up event/logging allocations before comparing descriptor counts. */
	if (!proxy_test_roundtrip(instance))
		return FALSE;
	const int before = proxy_test_fd_count();
	for (size_t x = 0; x < 16; x++)
	{
		if (!proxy_test_roundtrip(instance))
			return FALSE;
	}
	const int after = proxy_test_fd_count();
	return before < 0 || after == before;
}

static BOOL proxy_test_cancel(freerdp* instance)
{
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "exec sleep 30", "remote.invalid", 3389, 30000);
	if (!layer)
		return FALSE;
	BOOL ok = FALSE;
	const UINT64 started = GetTickCount64();
	if (freerdp_abort_connect_context(instance->context))
	{
		BYTE byte = 0;
		const int rc = layer->Read(layer->userContext, &byte, 1);
		ok = rc < 0 && errno != EAGAIN;
	}
	transport_layer_free(layer);
	return ok && GetTickCount64() - started < TEST_TIMEOUT;
}

static BOOL proxy_test_connect_timeout(freerdp* instance)
{
	rdpSettings* settings = instance->context->settings;
	if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, "remote.invalid") ||
	    !freerdp_settings_set_string(settings, FreeRDP_ProxyCommand, "exec sleep 30") ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpConnectTimeout, 100))
		return FALSE;
	const ULONGLONG start = GetTickCount64();
	const BOOL connected = freerdp_connect(instance);
	const ULONGLONG elapsed = GetTickCount64() - start;
	(void)freerdp_disconnect(instance);
	return !connected && elapsed >= 100 && elapsed < TEST_TIMEOUT;
}

static BOOL proxy_test_tls_timeout(freerdp* instance)
{
	if (!freerdp_settings_set_bool(instance->context->settings, FreeRDP_NegotiateSecurityLayer,
	                               FALSE) ||
	    !freerdp_settings_set_bool(instance->context->settings, FreeRDP_NlaSecurity, FALSE) ||
	    !freerdp_settings_set_bool(instance->context->settings, FreeRDP_RdpSecurity, FALSE))
		return FALSE;
	return proxy_test_connect_timeout(instance);
}

static BOOL proxy_test_conflicts(freerdp* instance)
{
	rdpSettings* settings = instance->context->settings;
	if (!freerdp_settings_set_string(settings, FreeRDP_ProxyCommand, "exec cat"))
		return FALSE;
	const FreeRDP_Settings_Keys_Bool conflicts[] = { FreeRDP_GatewayEnabled,
		                                             FreeRDP_GatewayArmTransport,
		                                             FreeRDP_ConnectChildSession };
	const rdpTransportIo* io = freerdp_get_io_callbacks(instance->context);
	for (size_t i = 0; i < ARRAYSIZE(conflicts); i++)
	{
		if (!freerdp_settings_set_bool(settings, conflicts[i], TRUE))
			return FALSE;
		rdpTransportLayer* layer = io->ConnectLayer(freerdp_get_transport(instance->context),
		                                            "remote.invalid", 3389, TEST_TIMEOUT);
		if (layer)
		{
			transport_layer_free(layer);
			return FALSE;
		}
		if (!freerdp_settings_set_bool(settings, conflicts[i], FALSE))
			return FALSE;
	}
	rdpTransportLayer* layer =
	    proxy_test_connect(instance, "exec cat", "remote.invalid", 3389, TEST_TIMEOUT);
	if (!layer)
		return FALSE;
	transport_layer_free(layer);
	return !freerdp_settings_get_bool(settings, FreeRDP_SupportMultitransport) &&
	       freerdp_settings_get_uint32(settings, FreeRDP_MultitransportFlags) == 0;
}

int TestProxyCommand(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	struct
	{
		const char* name;
		BOOL (*test)(freerdp*);
	} tests[] = { { "roundtrip", proxy_test_roundtrip },
		          { "tokens", proxy_test_tokens },
		          { "eof", proxy_test_eof },
		          { "timeout", proxy_test_timeout },
		          { "established deadline", proxy_test_established_deadline },
		          { "invalid", proxy_test_invalid },
		          { "stubborn cleanup", proxy_test_stubborn_cleanup },
		          { "repeat", proxy_test_repeat },
		          { "connect timeout", proxy_test_connect_timeout },
		          { "TLS startup timeout", proxy_test_tls_timeout },
		          { "conflicts and stream-only", proxy_test_conflicts },
		          { "cancel", proxy_test_cancel } };
	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		freerdp* instance = proxy_test_new();
		if (!instance)
			return -1;
		const BOOL ok = tests[x].test(instance);
		proxy_test_free(instance);
		if (!ok)
		{
			fprintf(stderr, "ProxyCommand %s failed\n", tests[x].name);
			return -1;
		}
	}
	return 0;
}

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Common RAIL launch IPC tests
 *
 * Copyright 2026 Tony Dursun <oraturk75@gmail.com>
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

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <freerdp/client/rail_ipc.h>

#include <winpr/crt.h>
#include <winpr/environment.h>

typedef enum
{
	CHANGE_NONE = 0,
	CHANGE_SERVER,
	CHANGE_SERVER_CANONICAL,
	CHANGE_EXPLICIT_DEFAULT_PORTS,
	CHANGE_PORT,
	CHANGE_USER,
	CHANGE_USER_CANONICAL,
	CHANGE_DOMAIN,
	CHANGE_DOMAIN_CANONICAL,
	CHANGE_CONSOLE,
	CHANGE_REDIRECTED_SESSION,
	CHANGE_VMCONNECT,
	CHANGE_CHILD_SESSION,
	CHANGE_PRECONNECTION_ENABLED,
	CHANGE_PRECONNECTION_ID,
	CHANGE_PRECONNECTION_BLOB,
	CHANGE_LOAD_BALANCE,
	CHANGE_RECONNECT_COOKIE,
	CHANGE_GATEWAY_DISABLED,
	CHANGE_GATEWAY_DISABLED_DETAILS,
	CHANGE_GATEWAY_USAGE,
	CHANGE_GATEWAY_HOST,
	CHANGE_GATEWAY_HOST_CANONICAL,
	CHANGE_GATEWAY_PORT,
	CHANGE_GATEWAY_USER,
	CHANGE_GATEWAY_USER_CANONICAL,
	CHANGE_GATEWAY_DOMAIN,
	CHANGE_GATEWAY_DOMAIN_CANONICAL,
	CHANGE_GATEWAY_URL,
	CHANGE_GATEWAY_URL_CANONICAL,
	CHANGE_ARM_ENABLED,
	CHANGE_ARM_USE_TENANT,
	CHANGE_ARM_A,
	CHANGE_ARM_AAD,
	CHANGE_ARM_TENANT,
	CHANGE_ARM_PROGRAM,
	CHANGE_PASSWORD,
	CHANGE_APPLICATION,
	CHANGE_WORKDIR,
	CHANGE_ARGUMENTS,
	CHANGE_PRESENTATION,
	CHANGE_PROXY,
	CHANGE_GATEWAY_SECRET,
	CHANGE_GATEWAY_TRANSPORT,
	CHANGE_SERVER_REDIRECTION
} keyChange;

typedef struct
{
	UINT16 flags;
	char* program;
	char* workingDirectory;
	char* arguments;
} capturedOrder;

#define MAX_CAPTURED_ORDERS 128U
#define TEST_RAIL_IPC_MAX_BYTES_PER_PASS (64U * 1024U)

typedef struct
{
	capturedOrder orders[MAX_CAPTURED_ORDERS];
	size_t count;
	size_t rejectCall;
	UINT rejectStatus;
} executeCapture;

static void capture_reset(executeCapture* capture)
{
	if (!capture)
		return;
	for (size_t x = 0; x < capture->count; x++)
	{
		free(capture->orders[x].program);
		free(capture->orders[x].workingDirectory);
		free(capture->orders[x].arguments);
	}
	ZeroMemory(capture, sizeof(*capture));
	capture->rejectCall = SIZE_MAX;
}

static UINT capture_execute(RailClientContext* context, const RAIL_EXEC_ORDER* order)
{
	if (!context || !order || !context->custom)
		return ERROR_INVALID_PARAMETER;
	executeCapture* capture = context->custom;
	if (capture->count >= MAX_CAPTURED_ORDERS)
		return ERROR_INSUFFICIENT_BUFFER;

	capturedOrder* copy = &capture->orders[capture->count];
	copy->flags = order->flags;
	copy->program = _strdup(order->RemoteApplicationProgram);
	copy->workingDirectory = _strdup(order->RemoteApplicationWorkingDir);
	copy->arguments = _strdup(order->RemoteApplicationArguments);
	if (!copy->program || !copy->workingDirectory || !copy->arguments)
	{
		free(copy->program);
		free(copy->workingDirectory);
		free(copy->arguments);
		ZeroMemory(copy, sizeof(*copy));
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	const size_t call = capture->count++;
	return (call == capture->rejectCall) ? capture->rejectStatus : CHANNEL_RC_OK;
}

static rdpSettings* make_settings(void)
{
	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings)
		return nullptr;
	if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname,
	                                 "ipc-test.example.invalid") ||
	    !freerdp_settings_set_string(settings, FreeRDP_Username, "ipc-test-user") ||
	    !freerdp_settings_set_string(settings, FreeRDP_Domain, "IPC-TEST") ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_GatewayUsageMethod, TSC_PROXY_MODE_DIRECT) ||
	    !freerdp_settings_set_string(settings, FreeRDP_GatewayHostname,
	                                 "gateway.example.invalid") ||
	    !freerdp_settings_set_string(settings, FreeRDP_GatewayUsername, "gateway-user") ||
	    !freerdp_settings_set_string(settings, FreeRDP_GatewayDomain, "GATEWAY") ||
	    !freerdp_settings_set_string(settings, FreeRDP_GatewayUrl,
	                                 "https://gateway.example.invalid/rdp") ||
	    !freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram,
	                                 "C:\\Windows\\System32\\notepad.exe") ||
	    !freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationWorkingDir,
	                                 "C:\\Windows") ||
	    !freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationCmdLine, "arguments"))
	{
		freerdp_settings_free(settings);
		return nullptr;
	}
	return settings;
}

static BOOL set_arm(rdpSettings* settings)
{
	return freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, TRUE) &&
	       freerdp_settings_set_string(settings, FreeRDP_GatewayAzureActiveDirectory,
	                                   "login.example.invalid") &&
	       freerdp_settings_set_bool(settings, FreeRDP_GatewayAvdUseTenantid, TRUE) &&
	       freerdp_settings_set_string(settings, FreeRDP_GatewayAvdAadtenantid, "tenant-a");
}

static BOOL apply_change(rdpSettings* settings, keyChange change)
{
	switch (change)
	{
		case CHANGE_NONE:
			return TRUE;
		case CHANGE_SERVER:
			return freerdp_settings_set_string(settings, FreeRDP_ServerHostname,
			                                   "other.example.invalid");
		case CHANGE_SERVER_CANONICAL:
			return freerdp_settings_set_string(settings, FreeRDP_ServerHostname,
			                                   "IPC-TEST.EXAMPLE.INVALID.");
		case CHANGE_EXPLICIT_DEFAULT_PORTS:
			return freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, 3389) &&
			       freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, 443);
		case CHANGE_PORT:
			return freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, 3390);
		case CHANGE_USER:
			return freerdp_settings_set_string(settings, FreeRDP_Username, "other-user");
		case CHANGE_USER_CANONICAL:
			return freerdp_settings_set_string(settings, FreeRDP_Username, "IPC-TEST-USER");
		case CHANGE_DOMAIN:
			return freerdp_settings_set_string(settings, FreeRDP_Domain, "OTHER-DOMAIN");
		case CHANGE_DOMAIN_CANONICAL:
			return freerdp_settings_set_string(settings, FreeRDP_Domain, "ipc-test.");
		case CHANGE_CONSOLE:
			return freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession, TRUE);
		case CHANGE_REDIRECTED_SESSION:
			return freerdp_settings_set_uint32(settings, FreeRDP_RedirectedSessionId, 42);
		case CHANGE_VMCONNECT:
			return freerdp_settings_set_bool(settings, FreeRDP_VmConnectMode, TRUE);
		case CHANGE_CHILD_SESSION:
			return freerdp_settings_set_bool(settings, FreeRDP_ConnectChildSession, TRUE);
		case CHANGE_PRECONNECTION_ENABLED:
			return freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE);
		case CHANGE_PRECONNECTION_ID:
			return freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE) &&
			       freerdp_settings_set_uint32(settings, FreeRDP_PreconnectionId, 7);
		case CHANGE_PRECONNECTION_BLOB:
			return freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE) &&
			       freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob, "vm-a");
		case CHANGE_LOAD_BALANCE:
		{
			static const char data[] = "Cookie: msts=collection-a";
			return freerdp_settings_set_pointer_len(settings, FreeRDP_LoadBalanceInfo, data,
			                                        sizeof(data) - 1U);
		}
		case CHANGE_RECONNECT_COOKIE:
		{
			ARC_SC_PRIVATE_PACKET cookie = { .cbLen = sizeof(cookie),
				                             .version = 1,
				                             .logonId = 42,
				                             .arcRandomBits = { 1, 2, 3, 4 } };
			return freerdp_settings_set_pointer_len(settings, FreeRDP_ServerAutoReconnectCookie,
			                                        &cookie, 1);
		}
		case CHANGE_GATEWAY_DISABLED:
			return freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_NONE_DIRECT);
		case CHANGE_GATEWAY_DISABLED_DETAILS:
			return freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_NONE_DIRECT) &&
			       freerdp_settings_set_string(settings, FreeRDP_GatewayHostname,
			                                   "ignored.example.invalid") &&
			       freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, 444);
		case CHANGE_GATEWAY_USAGE:
			return freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_DETECT);
		case CHANGE_GATEWAY_HOST:
			return freerdp_settings_set_string(settings, FreeRDP_GatewayHostname,
			                                   "other-gateway.example.invalid");
		case CHANGE_GATEWAY_HOST_CANONICAL:
			return freerdp_settings_set_string(settings, FreeRDP_GatewayHostname,
			                                   "GATEWAY.EXAMPLE.INVALID.");
		case CHANGE_GATEWAY_PORT:
			return freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, 444);
		case CHANGE_GATEWAY_USER:
			return freerdp_settings_set_string(settings, FreeRDP_GatewayUsername, "other-user");
		case CHANGE_GATEWAY_USER_CANONICAL:
			return freerdp_settings_set_string(settings, FreeRDP_GatewayUsername, "GATEWAY-USER");
		case CHANGE_GATEWAY_DOMAIN:
			return freerdp_settings_set_string(settings, FreeRDP_GatewayDomain, "OTHER-GATEWAY");
		case CHANGE_GATEWAY_DOMAIN_CANONICAL:
			return freerdp_settings_set_string(settings, FreeRDP_GatewayDomain, "gateway.");
		case CHANGE_GATEWAY_URL:
			return freerdp_settings_set_string(settings, FreeRDP_GatewayUrl,
			                                   "https://gateway.example.invalid/other");
		case CHANGE_GATEWAY_URL_CANONICAL:
			return freerdp_settings_set_string(settings, FreeRDP_GatewayUrl,
			                                   "wss://GATEWAY.EXAMPLE.INVALID.:443/rdp");
		case CHANGE_ARM_ENABLED:
			return freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, TRUE);
		case CHANGE_ARM_USE_TENANT:
			return freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, TRUE) &&
			       freerdp_settings_set_bool(settings, FreeRDP_GatewayAvdUseTenantid, TRUE);
		case CHANGE_ARM_A:
			return set_arm(settings);
		case CHANGE_ARM_AAD:
			return set_arm(settings) &&
			       freerdp_settings_set_string(settings, FreeRDP_GatewayAzureActiveDirectory,
			                                   "other-login.example.invalid");
		case CHANGE_ARM_TENANT:
			return set_arm(settings) &&
			       freerdp_settings_set_string(settings, FreeRDP_GatewayAvdAadtenantid, "tenant-b");
		case CHANGE_ARM_PROGRAM:
			return set_arm(settings) &&
			       freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram,
			                                   "C:\\Windows\\System32\\calc.exe");
		case CHANGE_PASSWORD:
			return freerdp_settings_set_string(settings, FreeRDP_Password, "excluded");
		case CHANGE_APPLICATION:
			return freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram,
			                                   "C:\\Windows\\System32\\calc.exe");
		case CHANGE_WORKDIR:
			return freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationWorkingDir,
			                                   "C:\\Other");
		case CHANGE_ARGUMENTS:
			return freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationCmdLine,
			                                   "different arguments");
		case CHANGE_PRESENTATION:
			return freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1920);
		case CHANGE_PROXY:
			return freerdp_settings_set_string(settings, FreeRDP_ProxyHostname,
			                                   "proxy.example.invalid");
		case CHANGE_GATEWAY_SECRET:
			return freerdp_settings_set_string(settings, FreeRDP_GatewayPassword, "excluded") &&
			       freerdp_settings_set_string(settings, FreeRDP_GatewayAccessToken, "excluded");
		case CHANGE_GATEWAY_TRANSPORT:
			return freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, FALSE) &&
			       freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE);
		case CHANGE_SERVER_REDIRECTION:
			return freerdp_settings_set_uint32(settings, FreeRDP_RedirectionFlags,
			                                   LB_TARGET_FQDN) &&
			       freerdp_settings_set_string(settings, FreeRDP_RedirectionTargetFQDN,
			                                   "redirected.example.invalid");
		default:
			return FALSE;
	}
}

static BOOL copy_string(char* destination, size_t capacity, const char* source)
{
	if (!destination || !source)
		return FALSE;
	const size_t length = strlen(source);
	if (length >= capacity)
		return FALSE;
	CopyMemory(destination, source, length + 1U);
	return TRUE;
}

static BOOL lock_path_from_fifo(const char* fifoPath, char* lockPath, size_t capacity)
{
	static const char suffix[] = ".fifo";
	const size_t length = strlen(fifoPath);
	if ((length < sizeof(suffix) - 1U) ||
	    (strcmp(fifoPath + length - (sizeof(suffix) - 1U), suffix) != 0))
		return FALSE;
	const size_t prefixLength = length - (sizeof(suffix) - 1U);
	if (prefixLength > capacity - sizeof(".lock"))
		return FALSE;
	CopyMemory(lockPath, fifoPath, prefixLength);
	CopyMemory(lockPath + prefixLength, ".lock", sizeof(".lock"));
	return TRUE;
}

static void remove_artifacts(const char* fifoPath)
{
	char lockPath[1024] = WINPR_C_ARRAY_INIT;
	if (!fifoPath)
		return;
	(void)unlink(fifoPath);
	if (lock_path_from_fifo(fifoPath, lockPath, sizeof(lockPath)))
		(void)unlink(lockPath);
}

static BOOL prepare_directory(char* directory)
{
	return mkdtemp(directory) && (chmod(directory, S_IRWXU) == 0) &&
	       SetEnvironmentVariableA("XDG_RUNTIME_DIR", directory);
}

static BOOL discover_paths(rdpSettings* settings, wLog* log, char fifoPath[1024],
                           char lockPath[1024])
{
	RailClientIpcContext* ipc = freerdp_client_rail_ipc_new(settings, log);
	if (!ipc)
		return FALSE;
	const char* path = freerdp_client_rail_ipc_get_path(ipc);
	const BOOL rc =
	    copy_string(fifoPath, 1024, path) && lock_path_from_fifo(fifoPath, lockPath, 1024);
	freerdp_client_rail_ipc_free(ipc);
	return rc;
}

static BOOL test_key_pair(keyChange leftChange, keyChange rightChange, BOOL expectEqual,
                          const char* label, wLog* log)
{
	BOOL rc = FALSE;
	char directory[] = "/tmp/fri-key.XXXXXX";
	char leftPath[1024] = WINPR_C_ARRAY_INIT;
	char rightPath[1024] = WINPR_C_ARRAY_INIT;
	char leftLock[1024] = WINPR_C_ARRAY_INIT;
	char rightLock[1024] = WINPR_C_ARRAY_INIT;
	RailClientIpcContext* ipc = nullptr;
	rdpSettings* left = make_settings();
	rdpSettings* right = make_settings();

	if (!left || !right || !prepare_directory(directory) || !apply_change(left, leftChange) ||
	    !apply_change(right, rightChange))
		goto out;

	ipc = freerdp_client_rail_ipc_new(left, log);
	if (!ipc || !copy_string(leftPath, sizeof(leftPath), freerdp_client_rail_ipc_get_path(ipc)) ||
	    !lock_path_from_fifo(leftPath, leftLock, sizeof(leftLock)))
		goto out;
	freerdp_client_rail_ipc_free(ipc);
	ipc = nullptr;

	ipc = freerdp_client_rail_ipc_new(right, log);
	if (!ipc || !copy_string(rightPath, sizeof(rightPath), freerdp_client_rail_ipc_get_path(ipc)) ||
	    !lock_path_from_fifo(rightPath, rightLock, sizeof(rightLock)))
		goto out;
	freerdp_client_rail_ipc_free(ipc);
	ipc = nullptr;

	rc = expectEqual == (strcmp(leftPath, rightPath) == 0);
	if (!rc)
		fprintf(stderr, "session-key case failed: %s\n", label);

out:
	freerdp_client_rail_ipc_free(ipc);
	remove_artifacts(leftPath);
	remove_artifacts(rightPath);
	if (directory[0] != '\0')
		(void)rmdir(directory);
	freerdp_settings_free(left);
	freerdp_settings_free(right);
	return rc;
}

static BOOL test_session_keys(wLog* log)
{
	static const struct
	{
		keyChange left;
		keyChange right;
		BOOL equal;
		const char* label;
	} cases[] = {
		{ CHANGE_NONE, CHANGE_SERVER, FALSE, "includes endpoint" },
		{ CHANGE_NONE, CHANGE_SERVER_CANONICAL, TRUE,
		  "canonicalizes endpoint case and trailing dot" },
		{ CHANGE_NONE, CHANGE_EXPLICIT_DEFAULT_PORTS, TRUE,
		  "canonicalizes implicit and explicit default ports" },
		{ CHANGE_NONE, CHANGE_PORT, FALSE, "includes endpoint port" },
		{ CHANGE_NONE, CHANGE_USER, FALSE, "includes user" },
		{ CHANGE_NONE, CHANGE_USER_CANONICAL, FALSE,
		  "keeps user case distinct for non-Windows servers" },
		{ CHANGE_NONE, CHANGE_DOMAIN, FALSE, "includes domain" },
		{ CHANGE_NONE, CHANGE_DOMAIN_CANONICAL, FALSE,
		  "keeps domain spelling distinct for non-Windows servers" },
		{ CHANGE_NONE, CHANGE_CONSOLE, FALSE, "includes console-session selection" },
		{ CHANGE_NONE, CHANGE_REDIRECTED_SESSION, FALSE, "includes redirected session ID" },
		{ CHANGE_NONE, CHANGE_VMCONNECT, FALSE, "includes VM-connect mode" },
		{ CHANGE_NONE, CHANGE_CHILD_SESSION, FALSE, "includes child-session selection" },
		{ CHANGE_NONE, CHANGE_PRECONNECTION_ENABLED, FALSE, "includes preconnection routing" },
		{ CHANGE_PRECONNECTION_ENABLED, CHANGE_PRECONNECTION_ID, FALSE,
		  "includes preconnection ID" },
		{ CHANGE_PRECONNECTION_ENABLED, CHANGE_PRECONNECTION_BLOB, FALSE,
		  "includes preconnection blob" },
		{ CHANGE_NONE, CHANGE_LOAD_BALANCE, FALSE, "includes load-balance data" },
		{ CHANGE_NONE, CHANGE_RECONNECT_COOKIE, FALSE, "includes reconnect session cookie" },
		{ CHANGE_NONE, CHANGE_GATEWAY_DISABLED, FALSE, "includes gateway use" },
		{ CHANGE_GATEWAY_DISABLED, CHANGE_GATEWAY_DISABLED_DETAILS, TRUE,
		  "ignores inactive gateway details" },
		{ CHANGE_NONE, CHANGE_GATEWAY_USAGE, FALSE, "includes gateway bypass policy" },
		{ CHANGE_NONE, CHANGE_GATEWAY_HOST, FALSE, "includes gateway endpoint" },
		{ CHANGE_NONE, CHANGE_GATEWAY_HOST_CANONICAL, TRUE,
		  "canonicalizes gateway host case and trailing dot" },
		{ CHANGE_NONE, CHANGE_GATEWAY_PORT, FALSE, "includes gateway port" },
		{ CHANGE_NONE, CHANGE_GATEWAY_USER, FALSE, "includes gateway user" },
		{ CHANGE_NONE, CHANGE_GATEWAY_USER_CANONICAL, FALSE, "keeps gateway user case distinct" },
		{ CHANGE_NONE, CHANGE_GATEWAY_DOMAIN, FALSE, "includes gateway domain" },
		{ CHANGE_NONE, CHANGE_GATEWAY_DOMAIN_CANONICAL, FALSE,
		  "keeps gateway domain spelling distinct" },
		{ CHANGE_NONE, CHANGE_GATEWAY_URL, FALSE, "includes gateway URL route" },
		{ CHANGE_NONE, CHANGE_GATEWAY_URL_CANONICAL, TRUE,
		  "canonicalizes gateway URL scheme, host and default port" },
		{ CHANGE_NONE, CHANGE_ARM_ENABLED, FALSE, "includes ARM gateway mode" },
		{ CHANGE_ARM_ENABLED, CHANGE_ARM_USE_TENANT, FALSE, "includes ARM tenant-selection mode" },
		{ CHANGE_ARM_ENABLED, CHANGE_ARM_A, FALSE, "includes ARM authority and tenant" },
		{ CHANGE_ARM_A, CHANGE_ARM_AAD, FALSE, "includes ARM authority" },
		{ CHANGE_ARM_A, CHANGE_ARM_TENANT, FALSE, "includes ARM tenant" },
		{ CHANGE_ARM_A, CHANGE_ARM_PROGRAM, FALSE, "includes ARM application route" },
		{ CHANGE_NONE, CHANGE_PASSWORD, TRUE, "excludes credentials" },
		{ CHANGE_NONE, CHANGE_APPLICATION, TRUE, "excludes application outside ARM routing" },
		{ CHANGE_NONE, CHANGE_WORKDIR, TRUE, "excludes application working directory" },
		{ CHANGE_NONE, CHANGE_ARGUMENTS, TRUE, "excludes application arguments" },
		{ CHANGE_NONE, CHANGE_PRESENTATION, TRUE, "excludes presentation settings" },
		{ CHANGE_NONE, CHANGE_PROXY, TRUE, "excludes proxy route" },
		{ CHANGE_NONE, CHANGE_GATEWAY_SECRET, TRUE, "excludes gateway credentials" },
		{ CHANGE_NONE, CHANGE_GATEWAY_TRANSPORT, TRUE,
		  "excludes equivalent gateway transport choice" },
		{ CHANGE_NONE, CHANGE_SERVER_REDIRECTION, TRUE,
		  "excludes server-produced redirection state" }
	};

	for (size_t x = 0; x < ARRAYSIZE(cases); x++)
	{
		if (!test_key_pair(cases[x].left, cases[x].right, cases[x].equal, cases[x].label, log))
			return FALSE;
	}
	return TRUE;
}

static BOOL test_primary_lifecycle(wLog* log)
{
	BOOL rc = FALSE;
	char directory[] = "/tmp/fri-life.XXXXXX";
	char firstPath[1024] = WINPR_C_ARRAY_INIT;
	char secondPath[1024] = WINPR_C_ARRAY_INIT;
	RailClientIpcContext* first = nullptr;
	RailClientIpcContext* duplicate = nullptr;
	RailClientIpcContext* other = nullptr;
	rdpSettings* settings = make_settings();
	rdpSettings* otherSettings = make_settings();
	RailClientContext rail = WINPR_C_ARRAY_INIT;
	executeCapture capture = WINPR_C_ARRAY_INIT;

	capture_reset(&capture);
	rail.custom = &capture;
	rail.ClientExecute = capture_execute;

	if (!settings || !otherSettings || !prepare_directory(directory) ||
	    !freerdp_settings_set_uint32(otherSettings, FreeRDP_ServerPort, 3390))
		goto out;
	first = freerdp_client_rail_ipc_new(settings, log);
	if (!first ||
	    !copy_string(firstPath, sizeof(firstPath), freerdp_client_rail_ipc_get_path(first)))
		goto out;

	struct stat attributes = WINPR_C_ARRAY_INIT;
	if ((lstat(firstPath, &attributes) < 0) || !S_ISFIFO(attributes.st_mode) ||
	    ((attributes.st_mode & ALLPERMS) != (S_IRUSR | S_IWUSR)))
		goto out;
	if (!freerdp_client_rail_ipc_check_event(first))
		goto out;
	if (freerdp_client_rail_ipc_get_event(first))
		goto out;

	duplicate = freerdp_client_rail_ipc_new(settings, log);
	if (duplicate)
		goto out;
	other = freerdp_client_rail_ipc_new(otherSettings, log);
	if (!other ||
	    !copy_string(secondPath, sizeof(secondPath), freerdp_client_rail_ipc_get_path(other)) ||
	    (strcmp(firstPath, secondPath) == 0))
		goto out;

	if (!freerdp_client_rail_ipc_attach(first, &rail) ||
	    !freerdp_client_rail_ipc_check_event(first))
		goto out;
	if (freerdp_client_rail_ipc_get_event(first))
		goto out;
	if (!freerdp_client_rail_ipc_set_ready(first, TRUE))
		goto out;
	HANDLE event = freerdp_client_rail_ipc_get_event(first);
	const int readFd = GetEventFileDescriptor(event);
	if (!freerdp_client_rail_ipc_check_event(first))
		goto out;
	if (!event || (readFd < 0) || ((fcntl(readFd, F_GETFD, 0) & FD_CLOEXEC) == 0))
		goto out;

	const int writer = open(firstPath, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	static const char record[] = "program=||notepad\n\n";
	if ((writer < 0) || ((fcntl(writer, F_GETFD, 0) & FD_CLOEXEC) == 0) ||
	    (write(writer, record, sizeof(record) - 1U) != (ssize_t)(sizeof(record) - 1U)))
	{
		if (writer >= 0)
			close(writer);
		goto out;
	}
	close(writer);

	if (WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
		goto out;
	struct pollfd wait = { .fd = readFd, .events = POLLIN | POLLHUP };
	if ((poll(&wait, 1, 0) != 1) || ((wait.revents & POLLIN) == 0))
		goto out;
	char received[sizeof(record)] = WINPR_C_ARRAY_INIT;
	if ((read(readFd, received, sizeof(received)) != (ssize_t)(sizeof(record) - 1U)) ||
	    (memcmp(received, record, sizeof(record) - 1U) != 0))
		goto out;

	for (size_t x = 0; x < 32; x++)
	{
		wait.revents = 0;
		if ((poll(&wait, 1, 0) != 0) || ((wait.revents & (POLLIN | POLLHUP)) != 0))
			goto out;
	}
	if (!freerdp_client_rail_ipc_detach(first, &rail) ||
	    !freerdp_client_rail_ipc_check_event(first))
		goto out;
	if (freerdp_client_rail_ipc_get_event(first))
		goto out;

	freerdp_client_rail_ipc_free(first);
	first = nullptr;
	if ((lstat(firstPath, &attributes) == 0) || (errno != ENOENT))
		goto out;
	rc = TRUE;

out:
	capture_reset(&capture);
	freerdp_client_rail_ipc_free(duplicate);
	freerdp_client_rail_ipc_free(other);
	freerdp_client_rail_ipc_free(first);
	remove_artifacts(firstPath);
	remove_artifacts(secondPath);
	(void)rmdir(directory);
	freerdp_settings_free(settings);
	freerdp_settings_free(otherSettings);
	return rc;
}

typedef struct
{
	char directory[64];
	char fifoPath[1024];
	char lockPath[1024];
	rdpSettings* settings;
} namedFixture;

static BOOL named_fixture_init(namedFixture* fixture, wLog* log)
{
	WINPR_ASSERT(fixture);
	if (!copy_string(fixture->directory, sizeof(fixture->directory), "/tmp/fri-name.XXXXXX"))
		return FALSE;
	fixture->settings = make_settings();
	if (!fixture->settings || !prepare_directory(fixture->directory) ||
	    !discover_paths(fixture->settings, log, fixture->fifoPath, fixture->lockPath))
		return FALSE;
	remove_artifacts(fixture->fifoPath);
	return TRUE;
}

static void named_fixture_uninit(namedFixture* fixture)
{
	if (!fixture)
		return;
	remove_artifacts(fixture->fifoPath);
	(void)rmdir(fixture->lockPath);
	(void)rmdir(fixture->fifoPath);
	(void)rmdir(fixture->directory);
	freerdp_settings_free(fixture->settings);
	ZeroMemory(fixture, sizeof(*fixture));
}

typedef struct
{
	namedFixture named;
	RailClientIpcContext* ipc;
	RailClientContext rail;
	executeCapture capture;
	int writer;
	long pipeBuffer;
} dispatchFixture;

static BOOL dispatch_fixture_init(dispatchFixture* fixture, wLog* log)
{
	WINPR_ASSERT(fixture);
	ZeroMemory(fixture, sizeof(*fixture));
	fixture->writer = -1;
	fixture->pipeBuffer = -1;
	capture_reset(&fixture->capture);
	if (!copy_string(fixture->named.directory, sizeof(fixture->named.directory),
	                 "/tmp/fri-dispatch.XXXXXX"))
		return FALSE;
	fixture->named.settings = make_settings();
	if (!fixture->named.settings || !prepare_directory(fixture->named.directory))
		return FALSE;

	fixture->ipc = freerdp_client_rail_ipc_new(fixture->named.settings, log);
	if (!fixture->ipc ||
	    !copy_string(fixture->named.fifoPath, sizeof(fixture->named.fifoPath),
	                 freerdp_client_rail_ipc_get_path(fixture->ipc)) ||
	    !lock_path_from_fifo(fixture->named.fifoPath, fixture->named.lockPath,
	                         sizeof(fixture->named.lockPath)))
		return FALSE;

	fixture->writer = open(fixture->named.fifoPath, O_WRONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
	if (fixture->writer < 0)
		return FALSE;
	fixture->pipeBuffer = fpathconf(fixture->writer, _PC_PIPE_BUF);
	if (fixture->pipeBuffer <= 0)
		return FALSE;

	fixture->rail.custom = &fixture->capture;
	fixture->rail.ClientExecute = capture_execute;
	return TRUE;
}

static void dispatch_fixture_uninit(dispatchFixture* fixture)
{
	if (!fixture)
		return;
	if (fixture->writer >= 0)
		close(fixture->writer);
	freerdp_client_rail_ipc_free(fixture->ipc);
	capture_reset(&fixture->capture);
	fixture->ipc = nullptr;
	fixture->writer = -1;
	named_fixture_uninit(&fixture->named);
}

static BOOL dispatch_fixture_set_ready(dispatchFixture* fixture)
{
	WINPR_ASSERT(fixture);
	return freerdp_client_rail_ipc_attach(fixture->ipc, &fixture->rail) &&
	       freerdp_client_rail_ipc_set_ready(fixture->ipc, TRUE);
}

static BOOL write_once(int fd, const void* data, size_t length)
{
	ssize_t rc = 0;
	do
	{
		rc = write(fd, data, length);
	} while ((rc < 0) && (errno == EINTR));
	return rc == (ssize_t)length;
}

static BOOL write_all(int fd, const void* data, size_t length)
{
	const BYTE* cursor = data;
	size_t offset = 0;
	while (offset < length)
	{
		ssize_t rc = 0;
		do
		{
			rc = write(fd, &cursor[offset], length - offset);
		} while ((rc < 0) && (errno == EINTR));
		if (rc <= 0)
			return FALSE;
		offset += (size_t)rc;
	}
	return TRUE;
}

static BOOL read_exact(int fd, void* data, size_t length)
{
	BYTE* cursor = data;
	size_t offset = 0;
	while (offset < length)
	{
		ssize_t rc = 0;
		do
		{
			rc = read(fd, &cursor[offset], length - offset);
		} while ((rc < 0) && (errno == EINTR));
		if (rc <= 0)
			return FALSE;
		offset += (size_t)rc;
	}
	return TRUE;
}

static BOOL create_cloexec_pipe(int fds[2])
{
	WINPR_ASSERT(fds);
	if (pipe(fds) < 0)
		return FALSE;
	if ((fcntl(fds[0], F_SETFD, FD_CLOEXEC) == 0) && (fcntl(fds[1], F_SETFD, FD_CLOEXEC) == 0))
		return TRUE;
	close(fds[0]);
	close(fds[1]);
	fds[0] = -1;
	fds[1] = -1;
	return FALSE;
}

static BOOL captured_matches(const capturedOrder* order, const char* program,
                             const char* workingDirectory, const char* arguments, UINT16 flags)
{
	return order && (order->flags == flags) && (strcmp(order->program, program) == 0) &&
	       (strcmp(order->workingDirectory, workingDirectory) == 0) &&
	       (strcmp(order->arguments, arguments) == 0);
}

static BOOL dispatch_expect(dispatchFixture* fixture, const void* record, size_t length,
                            BOOL accepted)
{
	WINPR_ASSERT(fixture);
	const size_t before = fixture->capture.count;
	if (!write_once(fixture->writer, record, length))
		return FALSE;
	if (!freerdp_client_rail_ipc_check_event(fixture->ipc))
		return FALSE;
	return fixture->capture.count == before + (accepted ? 1U : 0U);
}

static char* make_repeated_field_record(const char* key, size_t valueLength, size_t* recordLength)
{
	WINPR_ASSERT(key);
	WINPR_ASSERT(recordLength);
	const BOOL isProgram = strcmp(key, "program") == 0;
	const char* leading = isProgram ? "" : "program=p\n";
	const size_t leadingLength = strlen(leading);
	const size_t keyLength = strlen(key);
	if ((leadingLength > SIZE_MAX - keyLength - 4U) ||
	    (valueLength > SIZE_MAX - leadingLength - keyLength - 4U))
		return nullptr;
	*recordLength = leadingLength + keyLength + 1U + valueLength + 2U;
	char* record = calloc(*recordLength + 1U, 1);
	if (!record)
		return nullptr;
	char* cursor = record;
	CopyMemory(cursor, leading, leadingLength);
	cursor += leadingLength;
	CopyMemory(cursor, key, keyLength);
	cursor += keyLength;
	*cursor++ = '=';
	memset(cursor, 'a', valueLength);
	cursor += valueLength;
	*cursor++ = '\n';
	*cursor++ = '\n';
	WINPR_ASSERT((size_t)(cursor - record) == *recordLength);
	return record;
}

static char* make_exact_record(size_t totalLength)
{
	static const char prefix[] = "program=p\narguments=";
	static const char suffix[] = "\n\n";
	if (totalLength < sizeof(prefix) - 1U + sizeof(suffix) - 1U)
		return nullptr;
	char* record = calloc(totalLength + 1U, 1);
	if (!record)
		return nullptr;
	const size_t prefixLength = sizeof(prefix) - 1U;
	const size_t suffixLength = sizeof(suffix) - 1U;
	CopyMemory(record, prefix, prefixLength);
	memset(&record[prefixLength], 'a', totalLength - prefixLength - suffixLength);
	CopyMemory(&record[totalLength - suffixLength], suffix, suffixLength);
	return record;
}

static BOOL test_stale_fifo_reuse(wLog* log)
{
	BOOL rc = FALSE;
	namedFixture fixture = WINPR_C_ARRAY_INIT;
	RailClientIpcContext* ipc = nullptr;
	if (!named_fixture_init(&fixture, log))
		goto out;

	const pid_t child = fork();
	if (child < 0)
		goto out;
	if (child == 0)
	{
		RailClientIpcContext* childIpc = freerdp_client_rail_ipc_new(fixture.settings, log);
		_exit(childIpc ? 0 : 1);
	}
	int status = 0;
	if ((waitpid(child, &status, 0) != child) || !WIFEXITED(status) || (WEXITSTATUS(status) != 0))
		goto out;

	struct stat attributes = WINPR_C_ARRAY_INIT;
	if ((lstat(fixture.fifoPath, &attributes) < 0) || !S_ISFIFO(attributes.st_mode))
		goto out;
	ipc = freerdp_client_rail_ipc_new(fixture.settings, log);
	if (!ipc || (strcmp(fixture.fifoPath, freerdp_client_rail_ipc_get_path(ipc)) != 0))
		goto out;
	rc = TRUE;

out:
	freerdp_client_rail_ipc_free(ipc);
	named_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_identity_safe_cleanup(wLog* log)
{
	BOOL rc = FALSE;
	namedFixture fixture = WINPR_C_ARRAY_INIT;
	RailClientIpcContext* ipc = nullptr;
	if (!named_fixture_init(&fixture, log))
		goto out;
	ipc = freerdp_client_rail_ipc_new(fixture.settings, log);
	if (!ipc || (unlink(fixture.fifoPath) < 0) || (mkfifo(fixture.fifoPath, S_IRUSR | S_IWUSR) < 0))
		goto out;
	freerdp_client_rail_ipc_free(ipc);
	ipc = nullptr;

	struct stat replacement = WINPR_C_ARRAY_INIT;
	rc = (lstat(fixture.fifoPath, &replacement) == 0) && S_ISFIFO(replacement.st_mode);

out:
	freerdp_client_rail_ipc_free(ipc);
	named_fixture_uninit(&fixture);
	return rc;
}

static int count_open_descriptors(void);

static BOOL setup_must_fail(rdpSettings* settings, wLog* log)
{
	const int before = count_open_descriptors();
	RailClientIpcContext* ipc = freerdp_client_rail_ipc_new(settings, log);
	if (ipc)
	{
		freerdp_client_rail_ipc_free(ipc);
		return FALSE;
	}
	const int after = count_open_descriptors();
	return (before < 0) || (after == before);
}

static BOOL test_runtime_directory_rejection(wLog* log)
{
	BOOL rc = FALSE;
	int stage = 0;
	char directory[] = "/tmp/fri-runtime.XXXXXX";
	rdpSettings* settings = make_settings();
	if (!settings || !prepare_directory(directory))
		goto out;

	stage = 1;
	if ((chmod(directory, 0755) < 0) || !setup_must_fail(settings, log) ||
	    (chmod(directory, S_IRWXU) < 0))
		goto out;
	stage = 2;
	char file[] = "/tmp/fri-runtime-file.XXXXXX";
	const int fileFd = mkstemp(file);
	if (fileFd < 0)
		goto out;
	close(fileFd);
	if (!SetEnvironmentVariableA("XDG_RUNTIME_DIR", file) || !setup_must_fail(settings, log))
	{
		(void)unlink(file);
		goto out;
	}
	(void)unlink(file);

	stage = 3;
	char root[] = "/tmp/fri-runtime-link.XXXXXX";
	if (!mkdtemp(root))
		goto out;
	char target[128] = WINPR_C_ARRAY_INIT;
	char link[128] = WINPR_C_ARRAY_INIT;
	if ((_snprintf(target, sizeof(target), "%s/target", root) <= 0) ||
	    (_snprintf(link, sizeof(link), "%s/link", root) <= 0) || (mkdir(target, S_IRWXU) < 0) ||
	    (symlink(target, link) < 0) || !SetEnvironmentVariableA("XDG_RUNTIME_DIR", link) ||
	    !setup_must_fail(settings, log))
	{
		(void)unlink(link);
		(void)rmdir(target);
		(void)rmdir(root);
		goto out;
	}
	(void)unlink(link);
	(void)rmdir(target);
	(void)rmdir(root);
	rc = TRUE;

out:
	if (!rc)
		fprintf(stderr, "runtime-directory rejection stage %d failed\n", stage);
	(void)chmod(directory, S_IRWXU);
	(void)rmdir(directory);
	freerdp_settings_free(settings);
	return rc;
}

static BOOL test_lock_rejection_case(wLog* log, int kind)
{
	BOOL rc = FALSE;
	namedFixture fixture = WINPR_C_ARRAY_INIT;
	int fd = -1;
	if (!named_fixture_init(&fixture, log))
		goto out;

	switch (kind)
	{
		case 0:
			if (mkdir(fixture.lockPath, S_IRWXU) < 0)
				goto out;
			break;
		case 1:
			if (symlink("missing-lock-target", fixture.lockPath) < 0)
				goto out;
			break;
		case 2:
			fd = open(fixture.lockPath, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0644);
			if ((fd < 0) || (fchmod(fd, 0644) < 0))
				goto out;
			close(fd);
			fd = -1;
			break;
		default:
			goto out;
	}

	rc = setup_must_fail(fixture.settings, log);

out:
	if (fd >= 0)
		close(fd);
	(void)unlink(fixture.lockPath);
	(void)rmdir(fixture.lockPath);
	named_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_fifo_rejection_case(wLog* log, int kind)
{
	BOOL rc = FALSE;
	namedFixture fixture = WINPR_C_ARRAY_INIT;
	int fd = -1;
	if (!named_fixture_init(&fixture, log))
		goto out;

	switch (kind)
	{
		case 0:
			fd = open(fixture.fifoPath, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
			if (fd < 0)
				goto out;
			close(fd);
			fd = -1;
			break;
		case 1:
			if (symlink("missing-fifo-target", fixture.fifoPath) < 0)
				goto out;
			break;
		case 2:
			if ((mkfifo(fixture.fifoPath, 0644) < 0) || (chmod(fixture.fifoPath, 0644) < 0))
				goto out;
			break;
		default:
			goto out;
	}

	rc = setup_must_fail(fixture.settings, log);

out:
	if (fd >= 0)
		close(fd);
	named_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_filesystem_rejections(wLog* log)
{
	if (!test_runtime_directory_rejection(log))
	{
		fprintf(stderr, "runtime-directory rejection case failed\n");
		return FALSE;
	}
	for (int kind = 0; kind < 3; kind++)
	{
		if (!test_lock_rejection_case(log, kind))
		{
			fprintf(stderr, "lock rejection case %d failed\n", kind);
			return FALSE;
		}
		if (!test_fifo_rejection_case(log, kind))
		{
			fprintf(stderr, "FIFO rejection case %d failed\n", kind);
			return FALSE;
		}
	}
	return TRUE;
}

static int count_open_descriptors(void)
{
	DIR* directory = opendir("/proc/self/fd");
	if (!directory)
		return -1;
	int count = 0;
	for (;;)
	{
		errno = 0;
		const struct dirent* entry = readdir(directory);
		if (!entry)
			break;
		if ((strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name, "..") != 0))
			count++;
	}
	const int error = errno;
	closedir(directory);
	return (error == 0) ? count : -1;
}

static BOOL test_construction_failures(wLog* log)
{
	BOOL rc = FALSE;
	BOOL limitChanged = FALSE;
	int probes[3] = { -1, -1, -1 };
	namedFixture fixture = WINPR_C_ARRAY_INIT;
	RailClientIpcContext* ipc = nullptr;
	struct rlimit originalLimit = WINPR_C_ARRAY_INIT;
	if (!named_fixture_init(&fixture, log))
		goto out;

	const int before = count_open_descriptors();
	for (size_t x = 0; x < ARRAYSIZE(probes); x++)
	{
		probes[x] = open("/dev/null", O_RDONLY | O_CLOEXEC);
		if (probes[x] < 0)
			goto out;
	}
	if ((before < 0) || (getrlimit(RLIMIT_NOFILE, &originalLimit) < 0))
		goto out;

	/* Releasing the three lowest free descriptors leaves exactly three slots below the limit. */
	struct rlimit reducedLimit = originalLimit;
	reducedLimit.rlim_cur = (rlim_t)probes[2] + 1;
	if (setrlimit(RLIMIT_NOFILE, &reducedLimit) < 0)
		goto out;
	limitChanged = TRUE;
	for (size_t x = 0; x < ARRAYSIZE(probes); x++)
	{
		close(probes[x]);
		probes[x] = -1;
	}

	struct stat lock = WINPR_C_ARRAY_INIT;
	const BOOL heldWriterFailed = setup_must_fail(fixture.settings, log) &&
	                              (access(fixture.fifoPath, F_OK) < 0) && (errno == ENOENT) &&
	                              (lstat(fixture.lockPath, &lock) == 0) && S_ISREG(lock.st_mode);
	if (setrlimit(RLIMIT_NOFILE, &originalLimit) < 0)
		goto out;
	limitChanged = FALSE;
	struct rlimit restoredLimit = WINPR_C_ARRAY_INIT;
	if (!heldWriterFailed || (getrlimit(RLIMIT_NOFILE, &restoredLimit) < 0) ||
	    (restoredLimit.rlim_cur != originalLimit.rlim_cur) ||
	    (restoredLimit.rlim_max != originalLimit.rlim_max) || (count_open_descriptors() != before))
		goto out;

	ipc = freerdp_client_rail_ipc_new(fixture.settings, log);
	if (!ipc)
		goto out;
	rc = TRUE;

out:
	if (limitChanged && (setrlimit(RLIMIT_NOFILE, &originalLimit) < 0))
		rc = FALSE;
	for (size_t x = 0; x < ARRAYSIZE(probes); x++)
	{
		if (probes[x] >= 0)
			close(probes[x]);
	}
	freerdp_client_rail_ipc_free(ipc);
	named_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_file_byte_budget(dispatchFixture* fixture)
{
	WINPR_ASSERT(fixture);
	BOOL rc = FALSE;
	BOOL readFdReplaced = FALSE;
	int fileFd = -1;
	int savedReadFd = -1;
	char path[] = "/tmp/fri-budget.XXXXXX";
	const int before = count_open_descriptors();
	const HANDLE event = freerdp_client_rail_ipc_get_event(fixture->ipc);
	const int readFd = GetEventFileDescriptor(event);
	if ((before < 0) || !event || (readFd < 0))
		goto out;

	fileFd = mkstemp(path);
	if ((fileFd < 0) || (unlink(path) < 0))
		goto out;
	path[0] = '\0';
	BYTE data[4096] = WINPR_C_ARRAY_INIT;
	memset(data, 'x', sizeof(data));
	size_t remaining = (2U * TEST_RAIL_IPC_MAX_BYTES_PER_PASS) + 1U;
	while (remaining > 0)
	{
		const size_t length = MIN(remaining, sizeof(data));
		if (!write_all(fileFd, data, length))
			goto out;
		remaining -= length;
	}
	if (lseek(fileFd, 0, SEEK_SET) != 0)
		goto out;

	savedReadFd = dup(readFd);
	if ((savedReadFd < 0) || (dup2(fileFd, readFd) != readFd))
		goto out;
	readFdReplaced = TRUE;

	/* A regular file is always readable, making the byte-budget arithmetic deterministic.
	 * The real FIFO tests cover FIFO-specific EAGAIN behavior. */
	if (!freerdp_client_rail_ipc_check_event(fixture->ipc) ||
	    (lseek(fileFd, 0, SEEK_CUR) != TEST_RAIL_IPC_MAX_BYTES_PER_PASS) ||
	    (WaitForSingleObject(event, 0) != WAIT_OBJECT_0) ||
	    (freerdp_client_rail_ipc_get_event(fixture->ipc) != event))
		goto out;
	if (!freerdp_client_rail_ipc_check_event(fixture->ipc) ||
	    (lseek(fileFd, 0, SEEK_CUR) != 2U * TEST_RAIL_IPC_MAX_BYTES_PER_PASS) ||
	    (WaitForSingleObject(event, 0) != WAIT_OBJECT_0) ||
	    (freerdp_client_rail_ipc_get_event(fixture->ipc) != event))
		goto out;
	if (dup2(savedReadFd, readFd) != readFd)
		goto out;
	readFdReplaced = FALSE;
	close(savedReadFd);
	savedReadFd = -1;
	close(fileFd);
	fileFd = -1;
	if ((WaitForSingleObject(event, 0) != WAIT_TIMEOUT) || (count_open_descriptors() != before))
		goto out;
	rc = TRUE;

out:
	if (readFdReplaced && (savedReadFd >= 0) && (dup2(savedReadFd, readFd) != readFd))
		rc = FALSE;
	if (savedReadFd >= 0)
		close(savedReadFd);
	if (fileFd >= 0)
		close(fileFd);
	if (path[0] != '\0')
		(void)unlink(path);
	if ((before >= 0) && (count_open_descriptors() != before))
		rc = FALSE;
	return rc;
}

static BOOL test_protocol_fields_and_order(wLog* log)
{
	BOOL rc = FALSE;
	dispatchFixture fixture = WINPR_C_ARRAY_INIT;
	if (!dispatch_fixture_init(&fixture, log))
		goto out;
	if (!dispatch_fixture_set_ready(&fixture))
		goto out;

	static const char minimal[] = "program=||notepad\n\n";
	if (!dispatch_expect(&fixture, minimal, sizeof(minimal) - 1U, TRUE) ||
	    !captured_matches(&fixture.capture.orders[0], "||notepad", "", "", 0))
		goto out;

	static const char equals[] =
	    "program=||app=one\nworking-directory=C:\\dir=one\narguments=--name=a=b\nflags=5\n\n";
	if (!dispatch_expect(&fixture, equals, sizeof(equals) - 1U, TRUE) ||
	    !captured_matches(&fixture.capture.orders[1], "||app=one", "C:\\dir=one", "--name=a=b", 5))
		goto out;

	const char* lines[] = { "program=||ordered\n", "working-directory=C:\\ordered\n",
		                    "arguments=ordered=yes\n", "flags=0x5\n" };
	for (size_t a = 0; a < 4; a++)
	{
		for (size_t b = 0; b < 4; b++)
		{
			if (b == a)
				continue;
			for (size_t c = 0; c < 4; c++)
			{
				if ((c == a) || (c == b))
					continue;
				for (size_t d = 0; d < 4; d++)
				{
					if ((d == a) || (d == b) || (d == c))
						continue;
					char record[256] = WINPR_C_ARRAY_INIT;
					const int length = _snprintf(record, sizeof(record), "%s%s%s%s\n", lines[a],
					                             lines[b], lines[c], lines[d]);
					if ((length <= 0) || ((size_t)length >= sizeof(record)) ||
					    !dispatch_expect(&fixture, record, (size_t)length, TRUE))
						goto out;
					const capturedOrder* order =
					    &fixture.capture.orders[fixture.capture.count - 1U];
					if (!captured_matches(order, "||ordered", "C:\\ordered", "ordered=yes", 5))
						goto out;
				}
			}
		}
	}

	static const char explicitEmpty[] =
	    "arguments=\nflags=\nprogram=||empty\nworking-directory=\n\n";
	if (!dispatch_expect(&fixture, explicitEmpty, sizeof(explicitEmpty) - 1U, TRUE) ||
	    !captured_matches(&fixture.capture.orders[fixture.capture.count - 1U], "||empty", "", "",
	                      0) ||
	    !captured_matches(&fixture.capture.orders[0], "||notepad", "", "", 0))
		goto out;
	rc = TRUE;

out:
	dispatch_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_flags_and_grammar_rejection(wLog* log)
{
	BOOL rc = FALSE;
	dispatchFixture fixture = WINPR_C_ARRAY_INIT;
	if (!dispatch_fixture_init(&fixture, log))
		goto out;
	if (!dispatch_fixture_set_ready(&fixture))
		goto out;

	static const struct
	{
		const char* value;
		UINT16 expected;
	} accepted[] = { { "", 0 },  { "0", 0 },   { "1", 1 },     { "4", 4 },
		             { "8", 8 }, { "08", 8 },  { "16", 16 },   { "016", 16 },
		             { "6", 6 }, { "31", 31 }, { "0x1f", 31 }, { "0X1f", 31 } };
	for (size_t x = 0; x < ARRAYSIZE(accepted); x++)
	{
		char record[128] = WINPR_C_ARRAY_INIT;
		const int length =
		    _snprintf(record, sizeof(record), "program=||flags\nflags=%s\n\n", accepted[x].value);
		if ((length <= 0) || !dispatch_expect(&fixture, record, (size_t)length, TRUE) ||
		    (fixture.capture.orders[fixture.capture.count - 1U].flags != accepted[x].expected))
			goto out;
	}

	static const char* rejected[] = {
		"program=||bad\nflags=2\n\n",          "program=||bad\nflags=010\n\n",
		"program=||bad\nflags=32\n\n",         "program=||bad\nflags=65536\n\n",
		"program=||bad\nflags=-1\n\n",         "program=||bad\nflags=0x\n\n",
		"program=||bad\nflags=0b1\n\n",        "program=||bad\nflags=junk\n\n",
		"program=||bad\nflags=0\nflags=0\n\n", "program=||bad\nprogram=||again\n\n",
		"program=||bad\nunknown=value\n\n",    "program=||bad\nmalformed\n\n",
		"working-directory=C:\\\n\n",          "program=\n\n"
	};
	for (size_t x = 0; x < ARRAYSIZE(rejected); x++)
	{
		if (!dispatch_expect(&fixture, rejected[x], strlen(rejected[x]), FALSE))
			goto out;
	}
	rc = TRUE;

out:
	dispatch_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_encoding_and_size_limits(wLog* log)
{
	BOOL rc = FALSE;
	dispatchFixture fixture = WINPR_C_ARRAY_INIT;
	char* record = nullptr;
	if (!dispatch_fixture_init(&fixture, log))
		goto out;
	if (!dispatch_fixture_set_ready(&fixture))
		goto out;

	static const BYTE invalidUtf8Continuation[] = { 'p', 'r', 'o',  'g',  'r',  'a',
		                                            'm', '=', 0xC3, 0x28, '\n', '\n' };
	static const BYTE invalidUtf8Overlong[] = { 'p', 'r', 'o',  'g',  'r',  'a',
		                                        'm', '=', 0xC0, 0x80, '\n', '\n' };
	static const BYTE invalidUtf8Surrogate[] = { 'p', 'r',  'o',  'g',  'r',  'a', 'm',
		                                         '=', 0xED, 0xA0, 0x80, '\n', '\n' };
	static const BYTE invalidUtf8TooLarge[] = { 'p', 'r',  'o',  'g',  'r',  'a',  'm',
		                                        '=', 0xF4, 0x90, 0x80, 0x80, '\n', '\n' };
	static const BYTE invalidUtf8Truncated[] = { 'p', 'r', 'o',  'g',  'r',  'a',
		                                         'm', '=', 0xE2, 0x82, '\n', '\n' };
	static const BYTE embeddedNul[] = { 'p', 'r', 'o', 'g', 'r',  'a', 'm',
		                                '=', 'a', 0,   'b', '\n', '\n' };
	static const struct
	{
		const BYTE* record;
		size_t length;
	} invalid[] = { { invalidUtf8Continuation, sizeof(invalidUtf8Continuation) },
		            { invalidUtf8Overlong, sizeof(invalidUtf8Overlong) },
		            { invalidUtf8Surrogate, sizeof(invalidUtf8Surrogate) },
		            { invalidUtf8TooLarge, sizeof(invalidUtf8TooLarge) },
		            { invalidUtf8Truncated, sizeof(invalidUtf8Truncated) },
		            { embeddedNul, sizeof(embeddedNul) } };
	for (size_t x = 0; x < ARRAYSIZE(invalid); x++)
	{
		if (!dispatch_expect(&fixture, invalid[x].record, invalid[x].length, FALSE))
			goto out;
	}

	static const char validUtf8[] = "program=||\xC3\xA9-\xE2\x82\xAC-\xF0\x9F\x98\x80\n\n";
	if (!dispatch_expect(&fixture, validUtf8, sizeof(validUtf8) - 1U, TRUE))
		goto out;

	size_t length = 0;
	record = make_repeated_field_record("program", 260, &length);
	if (!record || !dispatch_expect(&fixture, record, length, TRUE))
		goto out;
	free(record);
	record = make_repeated_field_record("program", 261, &length);
	if (!record || !dispatch_expect(&fixture, record, length, FALSE))
		goto out;
	free(record);
	record = make_repeated_field_record("working-directory", 260, &length);
	if (!record || !dispatch_expect(&fixture, record, length, TRUE))
		goto out;
	free(record);
	record = make_repeated_field_record("working-directory", 261, &length);
	if (!record || !dispatch_expect(&fixture, record, length, FALSE))
		goto out;
	free(record);
	record = nullptr;

	const size_t pipeBuffer = (size_t)fixture.pipeBuffer;
	record = make_exact_record(pipeBuffer);
	if (!record || !dispatch_expect(&fixture, record, pipeBuffer, TRUE))
		goto out;
	free(record);
	record = make_exact_record(pipeBuffer + 1U);
	if (!record || !dispatch_expect(&fixture, record, pipeBuffer + 1U, FALSE))
		goto out;
	free(record);
	record = nullptr;
	rc = TRUE;

out:
	free(record);
	dispatch_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_streaming_resynchronisation_and_rejection(wLog* log)
{
	BOOL rc = FALSE;
	dispatchFixture fixture = WINPR_C_ARRAY_INIT;
	if (!dispatch_fixture_init(&fixture, log))
		goto out;
	if (!dispatch_fixture_set_ready(&fixture))
		goto out;

	static const char firstPart[] = "program=||partial\narg";
	static const char secondPart[] = "uments=split\n\n";
	if (!dispatch_expect(&fixture, firstPart, sizeof(firstPart) - 1U, FALSE) ||
	    !dispatch_expect(&fixture, secondPart, sizeof(secondPart) - 1U, TRUE) ||
	    !captured_matches(&fixture.capture.orders[0], "||partial", "", "split", 0))
		goto out;

	static const char several[] =
	    "program=||one\n\nprogram=||two\narguments=2\n\nprogram=||three\n\n";
	const size_t beforeSeveral = fixture.capture.count;
	if (!write_once(fixture.writer, several, sizeof(several) - 1U) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (fixture.capture.count != beforeSeveral + 3U)
		goto out;

	static const char malformedThenValid[] =
	    "program=||discard\nmalformed\n\nprogram=||after-malformed\n\n";
	const size_t beforeMalformed = fixture.capture.count;
	if (!write_once(fixture.writer, malformedThenValid, sizeof(malformedThenValid) - 1U) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if ((fixture.capture.count != beforeMalformed + 1U) ||
	    (strcmp(fixture.capture.orders[beforeMalformed].program, "||after-malformed") != 0))
		goto out;

	static const char truncated[] = "program=||discard-truncated\narguments";
	static const char terminateThenValid[] = "\n\nprogram=||after-truncated\n\n";
	const size_t beforeTruncated = fixture.capture.count;
	if (!dispatch_expect(&fixture, truncated, sizeof(truncated) - 1U, FALSE) ||
	    !write_once(fixture.writer, terminateThenValid, sizeof(terminateThenValid) - 1U) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if ((fixture.capture.count != beforeTruncated + 1U) ||
	    (strcmp(fixture.capture.orders[beforeTruncated].program, "||after-truncated") != 0))
		goto out;

	static const BYTE nulThenValid[] = { 'p', 'r', 'o',  'g',  'r', 'a', 'm', '=',  'b', 'a', 'd',
		                                 0,   'x', '\n', '\n', 'p', 'r', 'o', 'g',  'r', 'a', 'm',
		                                 '=', '|', '|',  'g',  'o', 'o', 'd', '\n', '\n' };
	const size_t beforeNul = fixture.capture.count;
	if (!write_once(fixture.writer, nulThenValid, sizeof(nulThenValid)) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if ((fixture.capture.count != beforeNul + 1U) ||
	    (strcmp(fixture.capture.orders[beforeNul].program, "||good") != 0))
		goto out;

	char mutableRecord[] = "program=||lifetime\nworking-directory=C:\\live\narguments=alive\n\n";
	const size_t beforeLifetime = fixture.capture.count;
	if (!dispatch_expect(&fixture, mutableRecord, sizeof(mutableRecord) - 1U, TRUE))
		goto out;
	memset(mutableRecord, 'x', sizeof(mutableRecord) - 1U);
	if (!captured_matches(&fixture.capture.orders[beforeLifetime], "||lifetime", "C:\\live",
	                      "alive", 0))
		goto out;

	fixture.capture.rejectCall = fixture.capture.count;
	fixture.capture.rejectStatus = ERROR_BAD_COMMAND;
	static const char rejectedThenNext[] = "program=||local-reject\n\nprogram=||after-reject\n\n";
	const size_t beforeReject = fixture.capture.count;
	if (!write_once(fixture.writer, rejectedThenNext, sizeof(rejectedThenNext) - 1U) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if ((fixture.capture.count != beforeReject + 2U) ||
	    (strcmp(fixture.capture.orders[beforeReject + 1U].program, "||after-reject") != 0))
		goto out;
	rc = TRUE;

out:
	dispatch_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_readiness_and_drain_budgets(wLog* log)
{
	BOOL rc = FALSE;
	dispatchFixture fixture = WINPR_C_ARRAY_INIT;
	if (!dispatch_fixture_init(&fixture, log))
		goto out;

	static const char buffered[] = "program=||buffered\n\n";
	if (!write_once(fixture.writer, buffered, sizeof(buffered) - 1U) ||
	    freerdp_client_rail_ipc_get_event(fixture.ipc))
		goto out;
	if (!freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (fixture.capture.count != 0)
		goto out;
	if (!freerdp_client_rail_ipc_attach(fixture.ipc, &fixture.rail))
		goto out;
	if (freerdp_client_rail_ipc_get_event(fixture.ipc))
		goto out;
	if (!freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (fixture.capture.count != 0)
		goto out;
	if (!freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE))
		goto out;
	if (!freerdp_client_rail_ipc_get_event(fixture.ipc))
		goto out;
	if (!freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if ((fixture.capture.count != 1) ||
	    (strcmp(fixture.capture.orders[0].program, "||buffered") != 0))
		goto out;

	static const char suspended[] = "program=||after-suspend\n\n";
	if (!write_once(fixture.writer, suspended, sizeof(suspended) - 1U) ||
	    (WaitForSingleObject(freerdp_client_rail_ipc_get_event(fixture.ipc), 0) != WAIT_OBJECT_0))
		goto out;
	if (!freerdp_client_rail_ipc_set_ready(fixture.ipc, FALSE) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (fixture.capture.count != 1)
		goto out;
	if (!freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (fixture.capture.count != 2)
		goto out;

	static const char detached[] = "program=||after-detach\n\n";
	if (!write_once(fixture.writer, detached, sizeof(detached) - 1U) ||
	    (WaitForSingleObject(freerdp_client_rail_ipc_get_event(fixture.ipc), 0) != WAIT_OBJECT_0))
		goto out;
	if (!freerdp_client_rail_ipc_detach(fixture.ipc, &fixture.rail) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (fixture.capture.count != 2)
		goto out;
	if (!dispatch_fixture_set_ready(&fixture))
		goto out;
	if (!freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (fixture.capture.count != 3)
		goto out;

	char records[1024] = WINPR_C_ARRAY_INIT;
	size_t length = 0;
	for (size_t x = 0; x < 20; x++)
	{
		const int used = _snprintf(&records[length], sizeof(records) - length,
		                           "program=||budget-%" PRIuz "\n\n", x);
		if ((used <= 0) || ((size_t)used >= sizeof(records) - length))
			goto out;
		length += (size_t)used;
	}
	const size_t beforeBudget = fixture.capture.count;
	if (!write_once(fixture.writer, records, length) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if ((fixture.capture.count != beforeBudget + 16U) ||
	    (WaitForSingleObject(freerdp_client_rail_ipc_get_event(fixture.ipc), 0) != WAIT_OBJECT_0))
		goto out;
	if (!freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (fixture.capture.count != beforeBudget + 20U)
		goto out;

	static const char denseCompletions[] = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nx\n\nprogram=p\n\n";
	const size_t beforeDense = fixture.capture.count;
	if (!write_once(fixture.writer, denseCompletions, sizeof(denseCompletions) - 1U) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if ((fixture.capture.count != beforeDense) ||
	    (WaitForSingleObject(freerdp_client_rail_ipc_get_event(fixture.ipc), 0) != WAIT_OBJECT_0))
		goto out;
	if (!freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if ((fixture.capture.count != beforeDense + 1U) ||
	    (strcmp(fixture.capture.orders[beforeDense].program, "p") != 0))
		goto out;

	if (!test_file_byte_budget(&fixture))
		goto out;
	rc = TRUE;

out:
	dispatch_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_concurrent_atomic_writers(wLog* log)
{
	BOOL rc = FALSE;
	dispatchFixture fixture = WINPR_C_ARRAY_INIT;
	int readyPipe[2] = { -1, -1 };
	int startPipe[2] = { -1, -1 };
	pid_t children[8] = WINPR_C_ARRAY_INIT;
	size_t started = 0;
	if (!dispatch_fixture_init(&fixture, log))
		goto out;
	if (!dispatch_fixture_set_ready(&fixture))
		goto out;
	if (!create_cloexec_pipe(readyPipe) || !create_cloexec_pipe(startPipe))
		goto out;

	for (size_t x = 0; x < ARRAYSIZE(children); x++)
	{
		const pid_t child = fork();
		if (child < 0)
			goto release;
		if (child == 0)
		{
			close(readyPipe[0]);
			close(startPipe[1]);
			const BYTE ready = 1;
			BYTE start = 0;
			if (!write_once(readyPipe[1], &ready, sizeof(ready)) ||
			    !read_exact(startPipe[0], &start, sizeof(start)))
				_exit(1);
			const int writer =
			    open(fixture.named.fifoPath, O_WRONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
			char record[128] = WINPR_C_ARRAY_INIT;
			const int length =
			    _snprintf(record, sizeof(record),
			              "program=||writer-%" PRIuz "\narguments=token-%" PRIuz "\n\n", x, x);
			const BOOL written =
			    (writer >= 0) && (length > 0) && write_once(writer, record, (size_t)length);
			if (writer >= 0)
				close(writer);
			_exit(written ? 0 : 1);
		}
		children[started++] = child;
	}

release:
	close(readyPipe[1]);
	readyPipe[1] = -1;
	close(startPipe[0]);
	startPipe[0] = -1;
	BYTE ready[ARRAYSIZE(children)] = WINPR_C_ARRAY_INIT;
	if ((started != ARRAYSIZE(children)) || !read_exact(readyPipe[0], ready, started))
		goto out;
	BYTE start[ARRAYSIZE(children)];
	memset(start, 1, sizeof(start));
	if (!write_once(startPipe[1], start, started))
		goto out;
	close(startPipe[1]);
	startPipe[1] = -1;

	for (size_t x = 0; x < started; x++)
	{
		int status = 0;
		if ((waitpid(children[x], &status, 0) != children[x]) || !WIFEXITED(status) ||
		    (WEXITSTATUS(status) != 0))
			goto out;
		children[x] = 0;
	}
	if (!freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (fixture.capture.count != ARRAYSIZE(children))
		goto out;

	BOOL seen[ARRAYSIZE(children)] = WINPR_C_ARRAY_INIT;
	for (size_t x = 0; x < fixture.capture.count; x++)
	{
		for (size_t expected = 0; expected < ARRAYSIZE(children); expected++)
		{
			char program[32] = WINPR_C_ARRAY_INIT;
			char arguments[32] = WINPR_C_ARRAY_INIT;
			(void)_snprintf(program, sizeof(program), "||writer-%" PRIuz, expected);
			(void)_snprintf(arguments, sizeof(arguments), "token-%" PRIuz, expected);
			if (!seen[expected] &&
			    captured_matches(&fixture.capture.orders[x], program, "", arguments, 0))
			{
				seen[expected] = TRUE;
				break;
			}
		}
	}
	for (size_t x = 0; x < ARRAYSIZE(seen); x++)
	{
		if (!seen[x])
			goto out;
	}
	rc = TRUE;

out:
	if (readyPipe[0] >= 0)
		close(readyPipe[0]);
	if (readyPipe[1] >= 0)
		close(readyPipe[1]);
	if (startPipe[0] >= 0)
		close(startPipe[0]);
	if (startPipe[1] >= 0)
	{
		BYTE start[ARRAYSIZE(children)];
		memset(start, 1, sizeof(start));
		(void)write(startPipe[1], start, started);
		close(startPipe[1]);
	}
	for (size_t x = 0; x < started; x++)
	{
		if (children[x] > 0)
			(void)waitpid(children[x], nullptr, 0);
	}
	dispatch_fixture_uninit(&fixture);
	return rc;
}

static BOOL disabled_context_retains_ownership(dispatchFixture* fixture, wLog* log)
{
	WINPR_ASSERT(fixture);
	RailClientIpcContext* duplicate = freerdp_client_rail_ipc_new(fixture->named.settings, log);
	if (duplicate)
	{
		freerdp_client_rail_ipc_free(duplicate);
		return FALSE;
	}
	freerdp_client_rail_ipc_free(fixture->ipc);
	fixture->ipc = nullptr;
	RailClientIpcContext* successor = freerdp_client_rail_ipc_new(fixture->named.settings, log);
	if (!successor)
		return FALSE;
	freerdp_client_rail_ipc_free(successor);
	return TRUE;
}

static BOOL test_result_contracts(wLog* log)
{
	BOOL rc = FALSE;
	dispatchFixture fixture = WINPR_C_ARRAY_INIT;
	RailClientContext other = WINPR_C_ARRAY_INIT;

	if (!freerdp_client_rail_ipc_check_event(nullptr) ||
	    !freerdp_client_rail_ipc_attach(nullptr, nullptr) ||
	    !freerdp_client_rail_ipc_detach(nullptr, nullptr) ||
	    !freerdp_client_rail_ipc_set_ready(nullptr, TRUE))
		return FALSE;

	if (!dispatch_fixture_init(&fixture, log))
		goto out;
	if (freerdp_client_rail_ipc_attach(fixture.ipc, nullptr) ||
	    freerdp_client_rail_ipc_detach(fixture.ipc, nullptr) ||
	    freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE) ||
	    freerdp_client_rail_ipc_get_event(fixture.ipc))
		goto out;
	if (!freerdp_client_rail_ipc_attach(fixture.ipc, &fixture.rail) ||
	    !freerdp_client_rail_ipc_attach(fixture.ipc, &fixture.rail) ||
	    !freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE))
		goto out;
	const HANDLE event = freerdp_client_rail_ipc_get_event(fixture.ipc);
	if (!event || freerdp_client_rail_ipc_attach(fixture.ipc, &other) ||
	    freerdp_client_rail_ipc_detach(fixture.ipc, &other) ||
	    freerdp_client_rail_ipc_attach(fixture.ipc, nullptr) ||
	    freerdp_client_rail_ipc_detach(fixture.ipc, nullptr) ||
	    (freerdp_client_rail_ipc_get_event(fixture.ipc) != event))
		goto out;
	if (!freerdp_client_rail_ipc_set_ready(fixture.ipc, FALSE) ||
	    freerdp_client_rail_ipc_get_event(fixture.ipc) ||
	    !freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE) ||
	    !freerdp_client_rail_ipc_detach(fixture.ipc, &fixture.rail) ||
	    freerdp_client_rail_ipc_get_event(fixture.ipc) ||
	    freerdp_client_rail_ipc_detach(fixture.ipc, &fixture.rail))
		goto out;

	fixture.rail.ClientExecute = nullptr;
	if (!freerdp_client_rail_ipc_attach(fixture.ipc, &fixture.rail) ||
	    !freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE) ||
	    !freerdp_client_rail_ipc_get_event(fixture.ipc) ||
	    freerdp_client_rail_ipc_check_event(fixture.ipc) ||
	    freerdp_client_rail_ipc_get_event(fixture.ipc) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc) ||
	    !freerdp_client_rail_ipc_set_ready(fixture.ipc, FALSE) ||
	    !freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE) ||
	    !freerdp_client_rail_ipc_detach(fixture.ipc, &fixture.rail))
		goto out;
	if (!disabled_context_retains_ownership(&fixture, log))
		goto out;
	rc = TRUE;

out:
	dispatch_fixture_uninit(&fixture);
	return rc;
}

static BOOL test_permanent_failure_disables_input(wLog* log)
{
	BOOL rc = FALSE;
	dispatchFixture fixture = WINPR_C_ARRAY_INIT;
	if (!dispatch_fixture_init(&fixture, log))
		goto out;
	if (!dispatch_fixture_set_ready(&fixture))
		goto out;
	HANDLE event = freerdp_client_rail_ipc_get_event(fixture.ipc);
	if (!event || (SetEventFileDescriptor(event, -1, WINPR_FD_READ) < 0))
		goto out;
	if (freerdp_client_rail_ipc_check_event(fixture.ipc) ||
	    freerdp_client_rail_ipc_get_event(fixture.ipc) || (fixture.capture.count != 0) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (!freerdp_client_rail_ipc_set_ready(fixture.ipc, FALSE) ||
	    !freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE))
		goto out;
	if (freerdp_client_rail_ipc_get_event(fixture.ipc) || (fixture.capture.count != 0))
		goto out;
	if (!freerdp_client_rail_ipc_detach(fixture.ipc, &fixture.rail))
		goto out;
	if (!disabled_context_retains_ownership(&fixture, log))
		goto out;
	dispatch_fixture_uninit(&fixture);

	if (!dispatch_fixture_init(&fixture, log))
		goto out;
	if (!dispatch_fixture_set_ready(&fixture))
		goto out;
	event = freerdp_client_rail_ipc_get_event(fixture.ipc);
	if (!event)
		goto out;
	const int readFd = GetEventFileDescriptor(event);
	if (readFd < 0)
		goto out;
	const int directoryFd = open(fixture.named.directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (directoryFd < 0)
		goto out;
	const int replaced = dup2(directoryFd, readFd);
	close(directoryFd);
	if ((replaced != readFd) || freerdp_client_rail_ipc_check_event(fixture.ipc) ||
	    freerdp_client_rail_ipc_get_event(fixture.ipc) || (fixture.capture.count != 0) ||
	    !freerdp_client_rail_ipc_check_event(fixture.ipc))
		goto out;
	if (!freerdp_client_rail_ipc_set_ready(fixture.ipc, FALSE) ||
	    !freerdp_client_rail_ipc_set_ready(fixture.ipc, TRUE))
		goto out;
	if (freerdp_client_rail_ipc_get_event(fixture.ipc) || (fixture.capture.count != 0))
		goto out;
	if (!freerdp_client_rail_ipc_detach(fixture.ipc, &fixture.rail))
		goto out;
	if (!disabled_context_retains_ownership(&fixture, log))
		goto out;
	rc = TRUE;

out:
	dispatch_fixture_uninit(&fixture);
	return rc;
}

int main(void)
{
	wLog* log = WLog_Get("com.freerdp.test.client.rail-ipc");
	if (!log || !WLog_SetLogLevel(log, WLOG_OFF))
		return 1;

	if (!test_session_keys(log))
		return 1;
	printf("PASS session key preserves all 44 audited routing and exclusion cases\n");
	if (!test_primary_lifecycle(log))
		return 1;
	printf(
	    "PASS paths, primary exclusion, independent keys, writable FIFO and held-writer state\n");
	if (!test_stale_fifo_reuse(log))
		return 1;
	printf("PASS stale FIFO is safely reused after all old descriptors close\n");
	if (!test_identity_safe_cleanup(log))
		return 1;
	printf("PASS cleanup leaves a replacement FIFO whose identity does not match\n");
	if (!test_filesystem_rejections(log))
		return 1;
	printf("PASS unsafe runtime directory, lock and FIFO forms are rejected\n");
	if (!test_construction_failures(log))
		return 1;
	printf("PASS construction failures release descriptors and ownership without unsafe cleanup\n");
	if (!test_protocol_fields_and_order(log))
		return 1;
	printf("PASS labelled fields, defaults, first-equals splitting and every key order\n");
	if (!test_flags_and_grammar_rejection(log))
		return 1;
	printf("PASS flags and strict v1 grammar acceptance and rejection\n");
	if (!test_encoding_and_size_limits(log))
		return 1;
	printf("PASS UTF-8, converted RAIL lengths and exact FIFO record-size limits\n");
	if (!test_streaming_resynchronisation_and_rejection(log))
		return 1;
	printf("PASS partial input, multi-record reads, resynchronisation and local rejection\n");
	if (!test_readiness_and_drain_budgets(log))
		return 1;
	printf("PASS detached and suspended buffering plus record and byte drain budgets\n");
	if (!test_concurrent_atomic_writers(log))
		return 1;
	printf("PASS concurrent conforming writers preserve every atomic record\n");
	if (!test_result_contracts(log))
		return 1;
	printf("PASS public result contracts reject invalid state without mutation\n");
	if (!test_permanent_failure_disables_input(log))
		return 1;
	printf("PASS permanent event and read failures disable input while retaining ownership\n");
	(void)SetEnvironmentVariableA("XDG_RUNTIME_DIR", nullptr);
	return 0;
}

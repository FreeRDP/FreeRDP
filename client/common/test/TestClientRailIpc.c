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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <winpr/crt.h>
#include <winpr/environment.h>

#include "../rail_ipc.h"
#include <freerdp/channels/channels.h>
#include "../../../libfreerdp/core/connection.h"

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
	CHANGE_PRECONNECTION_INACTIVE,
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

static BOOL init_settings(rdpSettings* settings)
{
	return freerdp_settings_set_string(settings, FreeRDP_ServerHostname,
	                                   "ipc-test.example.invalid") &&
	       freerdp_settings_set_string(settings, FreeRDP_Username, "ipc-test-user") &&
	       freerdp_settings_set_string(settings, FreeRDP_Domain, "IPC-TEST") &&
	       freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, TRUE) &&
	       freerdp_settings_set_uint32(settings, FreeRDP_GatewayUsageMethod,
	                                   TSC_PROXY_MODE_DIRECT) &&
	       freerdp_settings_set_string(settings, FreeRDP_GatewayHostname,
	                                   "gateway.example.invalid") &&
	       freerdp_settings_set_string(settings, FreeRDP_GatewayUsername, "gateway-user") &&
	       freerdp_settings_set_string(settings, FreeRDP_GatewayDomain, "GATEWAY") &&
	       freerdp_settings_set_string(settings, FreeRDP_GatewayUrl,
	                                   "https://gateway.example.invalid/rdp") &&
	       freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram,
	                                   "C:\\Windows\\System32\\notepad.exe") &&
	       freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationWorkingDir,
	                                   "C:\\Windows") &&
	       freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationCmdLine, "arguments");
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
		case CHANGE_PRECONNECTION_INACTIVE:
			return freerdp_settings_set_uint32(settings, FreeRDP_PreconnectionId, 7) &&
			       freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob, "ignored");
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

static rdpClientContext* new_context(void)
{
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(entry);
	entry.ContextSize = sizeof(rdpClientContext);
	rdpClientContext* cctx = (rdpClientContext*)freerdp_client_context_new(&entry);
	if (!cctx)
		return nullptr;
	if (!init_settings(cctx->context.settings) ||
	    !freerdp_settings_set_bool(cctx->context.settings, FreeRDP_RemoteApplicationMode, TRUE))
	{
		freerdp_client_context_free(&cctx->context);
		return nullptr;
	}
	return cctx;
}

static BOOL start(rdpClientContext* cctx)
{
	return freerdp_client_rail_multi_exec_enable(cctx, TRUE) &&
	       cctx->context.instance->PreConnect(cctx->context.instance);
}

/* Inspect the private test directory, not component internals. */
static int find_files(const char* directory, const char* suffix, char path[1024])
{
	DIR* dir = opendir(directory);
	if (!dir)
		return -1;
	int count = 0;
	for (;;)
	{
		errno = 0;
		/* This DIR is local to this call; no concurrent readdir uses it. */
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		const struct dirent* item = readdir(dir);
		if (!item)
		{
			if (errno != 0)
				count = -1;
			break;
		}
		if ((strcmp(item->d_name, ".") == 0) || (strcmp(item->d_name, "..") == 0))
			continue;
		const size_t len = strlen(item->d_name);
		const size_t suffixLen = strlen(suffix);
		if ((len < suffixLen) || (strcmp(&item->d_name[len - suffixLen], suffix) != 0))
			continue;
		count++;
		if (path)
		{
			const int n = snprintf(path, 1024, "%s/%s", directory, item->d_name);
			if ((n <= 0) || (n >= 1024))
			{
				count = -1;
				break;
			}
		}
	}
	(void)closedir(dir);
	return count;
}

static BOOL test_key_pair(keyChange leftChange, keyChange rightChange, BOOL expectEqual,
                          const char* label, const char* directory)
{
	BOOL ok = FALSE;
	char leftPath[1024] = WINPR_C_ARRAY_INIT;
	char rightPath[1024] = WINPR_C_ARRAY_INIT;
	rdpClientContext* left = new_context();
	rdpClientContext* right = new_context();
	if (!left || !right || !apply_change(left->context.settings, leftChange) ||
	    !apply_change(right->context.settings, rightChange) || !start(left) ||
	    (find_files(directory, ".fifo", leftPath) != 1))
		goto out;
	rail_ipc_stop(left);
	if (!start(right) || (find_files(directory, ".fifo", rightPath) != 1))
		goto out;
	ok = expectEqual == (strcmp(leftPath, rightPath) == 0);
out:
	if (!ok)
		(void)fprintf(stderr, "session-key case failed: %s\n", label);
	freerdp_client_context_free((rdpContext*)left);
	freerdp_client_context_free((rdpContext*)right);
	return ok;
}

static BOOL test_session_keys(const char* directory)
{
	static const struct
	{
		keyChange left;
		keyChange right;
		BOOL equal;
		const char* label;
	} cases[] = {
		{ CHANGE_NONE, CHANGE_NONE, TRUE, "same settings" },
		{ CHANGE_NONE, CHANGE_PRECONNECTION_INACTIVE, TRUE,
		  "ignores inactive preconnection details" },
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
		if (!test_key_pair(cases[x].left, cases[x].right, cases[x].equal, cases[x].label,
		                   directory))
			return FALSE;
	}
	return TRUE;
}

/* F2 and F8: no transport for inactive modes; enable/free without connecting. */
static BOOL test_inactive(const char* directory)
{
	for (size_t mode = 0; mode < 4; mode++)
	{
		rdpClientContext* cctx = new_context();
		if (!cctx)
			return FALSE;
		BOOL ok = (mode == 0) || freerdp_client_rail_multi_exec_enable(cctx, TRUE);
		if (mode == 1)
			ok = ok && freerdp_settings_set_bool(cctx->context.settings,
			                                     FreeRDP_RemoteApplicationMode, FALSE);
		if (mode == 2)
			ok = ok && freerdp_settings_set_bool(cctx->context.settings, FreeRDP_AuthenticationOnly,
			                                     TRUE);
		if (mode != 3)
			ok = ok && cctx->context.instance->PreConnect(cctx->context.instance);
		ok = ok && freerdp_client_rail_multi_exec_set_ready(cctx, TRUE);
		freerdp_client_context_free(&cctx->context);
		if (!ok || (find_files(directory, "", nullptr) != 0))
			return FALSE;
	}
	return TRUE;
}

static int count_descriptors(void)
{
	const long limit = sysconf(_SC_OPEN_MAX);
	if ((limit < 0) || (limit > INT_MAX))
		return -1;
	int count = 0;
	for (int fd = 0; fd < limit; fd++)
	{
		if (fcntl(fd, F_GETFD) >= 0)
			count++;
		else if (errno != EBADF)
			return -1;
	}
	return count;
}

static BOOL expect_failed_start(rdpClientContext* cctx)
{
	const int before = count_descriptors();
	return (before >= 0) && !cctx->context.instance->PreConnect(cctx->context.instance) &&
	       (count_descriptors() == before);
}

/* F3: fail through commonPreConnect; all setup descriptors must be released. */
static BOOL test_unsafe_paths(const char* directory)
{
	BOOL ok = FALSE;
	char fifo[1024] = WINPR_C_ARRAY_INIT;
	char lock[1024] = WINPR_C_ARRAY_INIT;
	rdpClientContext* cctx = new_context();
	if (!cctx || !start(cctx) || (find_files(directory, ".fifo", fifo) != 1))
		goto out;
	memcpy(lock, fifo, strlen(fifo) + 1U);
	memcpy(&lock[strlen(lock) - 5U], ".lock", sizeof(".lock"));
	rail_ipc_stop(cctx);
	if ((chmod(directory, 0755) < 0) || !expect_failed_start(cctx) || (chmod(directory, 0700) < 0))
		goto out;
	if ((unlink(lock) < 0) || (symlink("missing", lock) < 0) || !expect_failed_start(cctx) ||
	    (unlink(lock) < 0))
		goto out;
	const int fd = open(fifo, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
	if (fd < 0)
		goto out;
	(void)close(fd);
	if (!expect_failed_start(cctx) || (unlink(fifo) < 0) || (mkfifo(fifo, 0600) < 0) ||
	    (chmod(fifo, 0644) < 0) || !expect_failed_start(cctx) || (unlink(fifo) < 0))
		goto out;
	ok = TRUE;
out:
	(void)chmod(directory, 0700);
	freerdp_client_context_free((rdpContext*)cctx);
	return ok;
}

/* F4 and F8: exclusion, final-disconnect cleanup, stop twice, stale reuse and free. */
static BOOL test_lifecycle(const char* directory)
{
	BOOL ok = FALSE;
	char fifo[1024] = WINPR_C_ARRAY_INIT;
	rdpClientContext* first = new_context();
	rdpClientContext* second = new_context();
	struct stat before = WINPR_C_ARRAY_INIT;
	struct stat after = WINPR_C_ARRAY_INIT;
	const int descriptors = count_descriptors();
	if (!first || !second || (descriptors < 0) || !start(first) ||
	    (find_files(directory, ".fifo", fifo) != 1) ||
	    !freerdp_client_rail_multi_exec_enable(second, TRUE) || !expect_failed_start(second))
		goto out;
	first->context.instance->PostFinalDisconnect(first->context.instance);
	rail_ipc_stop(first);
	if ((count_descriptors() != descriptors) || (lstat(fifo, &after) == 0) || (errno != ENOENT) ||
	    (find_files(directory, ".lock", nullptr) < 1) || (mkfifo(fifo, 0600) < 0) ||
	    (lstat(fifo, &before) < 0) ||
	    !second->context.instance->PreConnect(second->context.instance) ||
	    (lstat(fifo, &after) < 0) || (before.st_dev != after.st_dev) ||
	    (before.st_ino != after.st_ino))
		goto out;
	freerdp_client_context_free(&second->context);
	second = nullptr;
	ok = (lstat(fifo, &after) < 0) && (errno == ENOENT);
out:
	freerdp_client_context_free((rdpContext*)first);
	freerdp_client_context_free((rdpContext*)second);
	return ok;
}

typedef struct
{
	size_t count;
	BOOL rejectFirst;
} executeCapture;

static UINT count_execute(RailClientContext* rail, WINPR_ATTR_UNUSED const RAIL_EXEC_ORDER* order)
{
	executeCapture* capture = rail->custom;
	capture->count++;
	return (capture->rejectFirst && (capture->count == 1)) ? ERROR_INTERNAL_ERROR : CHANNEL_RC_OK;
}

static DWORD event_count(rdpClientContext* cctx)
{
	HANDLE events[64] = WINPR_C_ARRAY_INIT;
	return freerdp_get_event_handles(&cctx->context, events, ARRAYSIZE(events));
}

static BOOL dispatch(rdpClientContext* cctx)
{
	return freerdp_channels_check_fds(cctx->context.channels, cctx->context.instance);
}

static BOOL write_record(int writer)
{
	const char record[] = "program=app\n\n";
	return write(writer, record, sizeof(record) - 1U) == (ssize_t)(sizeof(record) - 1U);
}

/* F5-F8: real channel handlers and registry, with only ClientExecute stubbed. */
static BOOL test_dispatch(const char* directory)
{
	BOOL ok = FALSE;
	int writer = -1;
	char fifo[1024] = WINPR_C_ARRAY_INIT;
	rdpClientContext* cctx = new_context();
	rdpClientContext* other = new_context();
	executeCapture capture = WINPR_C_ARRAY_INIT;
	RailClientContext rail = { .custom = &capture, .ClientExecute = count_execute };
	const ChannelConnectedEventArgs connected = { .name = RAIL_SVC_CHANNEL_NAME,
		                                          .pInterface = &rail };
	const ChannelDisconnectedEventArgs disconnected = { .name = RAIL_SVC_CHANNEL_NAME,
		                                                .pInterface = &rail };
	if (!cctx || !other || !freerdp_client_rail_multi_exec_set_ready(cctx, TRUE) || !start(cctx) ||
	    (find_files(directory, ".fifo", fifo) != 1))
		goto out;
	const DWORD base = event_count(cctx);
	if (base == 0)
		goto out;
	freerdp_client_OnChannelConnectedEventHandler(&cctx->context, &connected);
	writer = open(fifo, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	/* Not ready: bytes stay. Ready but not ACTIVE: still unread. */
	if ((writer < 0) || (event_count(cctx) != base) ||
	    !rdp_client_transition_to_state(cctx->context.rdp, CONNECTION_STATE_ACTIVE) ||
	    !write_record(writer) || !dispatch(cctx) || (capture.count != 0) ||
	    !rdp_client_transition_to_state(cctx->context.rdp, CONNECTION_STATE_INITIAL) ||
	    !freerdp_client_rail_multi_exec_set_ready(cctx, TRUE) ||
	    !freerdp_client_rail_multi_exec_set_ready(cctx, TRUE) || (event_count(cctx) != base + 1U) ||
	    !dispatch(cctx) || (capture.count != 0) ||
	    !rdp_client_transition_to_state(cctx->context.rdp, CONNECTION_STATE_ACTIVE) ||
	    !dispatch(cctx) || (capture.count != 1))
		goto out;
	/* Empty FIFO: the zero-time wait is harmless. */
	if (!dispatch(cctx) || (capture.count != 1) ||
	    !freerdp_client_rail_multi_exec_set_ready(cctx, FALSE) ||
	    !freerdp_client_rail_multi_exec_set_ready(cctx, FALSE) || (event_count(cctx) != base))
		goto out;
	freerdp_client_OnChannelDisconnectedEventHandler(&cctx->context, &disconnected);
	freerdp_client_OnChannelConnectedEventHandler(&cctx->context, &connected);
	if (!freerdp_client_rail_multi_exec_set_ready(cctx, TRUE))
		goto out;
	freerdp_client_OnChannelDisconnectedEventHandler(&cctx->context, &disconnected);
	if ((event_count(cctx) != base) || !write_record(writer) || !dispatch(cctx) ||
	    (capture.count != 1))
		goto out;
	freerdp_client_OnChannelConnectedEventHandler(&cctx->context, &connected);
	if (!freerdp_client_rail_multi_exec_set_ready(cctx, TRUE) || !dispatch(cctx) ||
	    (capture.count != 2))
		goto out;
	/* F6: the seventeenth record stays in the FIFO for the next pass. */
	capture.count = 0;
	for (size_t x = 0; x < 17; x++)
	{
		if (!write_record(writer))
			goto out;
	}
	if (!dispatch(cctx) || (capture.count != 16) || !dispatch(cctx) || (capture.count != 17))
		goto out;
	/* Rejections consume the same record budget, including one-byte empty records. */
	const char rejected[] = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
	capture.count = 0;
	if ((write(writer, rejected, sizeof(rejected) - 1U) != (ssize_t)(sizeof(rejected) - 1U)) ||
	    !write_record(writer) || !dispatch(cctx) || (capture.count != 0) || !dispatch(cctx) ||
	    (capture.count != 1))
		goto out;
	/* F7: one local rejection does not prevent the next Execute. */
	capture.count = 0;
	capture.rejectFirst = TRUE;
	if (!write_record(writer) || !write_record(writer) || !dispatch(cctx) || (capture.count != 2))
		goto out;
	rail.ClientExecute = nullptr;
	if (!write_record(writer) || !dispatch(cctx) || (event_count(cctx) != base) ||
	    (find_files(directory, ".fifo", nullptr) != 0) ||
	    !freerdp_client_rail_multi_exec_set_ready(cctx, TRUE) || (event_count(cctx) != base) ||
	    !freerdp_client_rail_multi_exec_enable(other, TRUE) || !expect_failed_start(other))
		goto out;
	ok = TRUE;
out:
	if (writer >= 0)
		(void)close(writer);
	freerdp_client_context_free((rdpContext*)cctx);
	freerdp_client_context_free((rdpContext*)other);
	return ok;
}

int TestClientRailIpc(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	char directory[] = "/tmp/fri-r3.XXXXXX";
	char path[1024] = WINPR_C_ARRAY_INIT;
	if (!mkdtemp(directory))
		return -1;
	BOOL ok = SetEnvironmentVariableA("XDG_RUNTIME_DIR", directory);
	if (ok)
		ok = test_inactive(directory) && test_session_keys(directory) &&
		     test_unsafe_paths(directory) && test_lifecycle(directory) && test_dispatch(directory);
	if (!ok)
		(void)fprintf(stderr, "RAIL IPC lifecycle tests failed\n");
	while (find_files(directory, "", path) > 0)
	{
		if (unlink(path) < 0)
		{
			ok = FALSE;
			break;
		}
	}
	if (rmdir(directory) < 0)
		ok = FALSE;
	if (!SetEnvironmentVariableA("XDG_RUNTIME_DIR", nullptr))
		ok = FALSE;
	return ok ? 0 : -1;
}

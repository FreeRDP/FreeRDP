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
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <winpr/crt.h>
#include <winpr/environment.h>
#include <winpr/synch.h>

#include "../rail_decoder.h"

#include "../rail_ipc.h"
#include <freerdp/channels/channels.h>
#include "../../../libfreerdp/core/connection.h"
#include "../../../libfreerdp/core/client.h"

#define CHECK(cond)                                                                             \
	do                                                                                          \
	{                                                                                           \
		if (!(cond))                                                                            \
		{                                                                                       \
			(void)fprintf(stderr, "%s:%d %s: check failed: %s\n", __FILE__, __LINE__, __func__, \
			              #cond);                                                               \
			goto fail;                                                                          \
		}                                                                                       \
	} while (0)

typedef struct
{
	rdpClientContext* cctx;
	rdpClientContext* other;
	RailClientContext rail;
	size_t count;
	BOOL rejectFirst;
	int writer;
	char fifo[1024];
	DWORD base;
} testContext;

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
	return freerdp_settings_set_bool(cctx->context.settings, FreeRDP_RemoteApplicationMultiExec,
	                                 TRUE) &&
	       cctx->context.instance->PreConnect(cctx->context.instance);
}

/* Inspect only the private test directory. */
static int find_files(const char* directory, const char* suffix, char path[1024])
{
	char pattern[1024] = WINPR_C_ARRAY_INIT;
	glob_t matches = WINPR_C_ARRAY_INIT;
	int count = -1;
	const int length = snprintf(pattern, sizeof(pattern), "%s/*%s", directory, suffix);
	CHECK((length > 0) && ((size_t)length < sizeof(pattern)));
	// NOLINTNEXTLINE(concurrency-mt-unsafe): scans are sequential, with no locale changes.
	const int status = glob(pattern, GLOB_ERR, nullptr, &matches);
	if (status == GLOB_NOMATCH)
		count = 0;
	else
	{
		CHECK((status == 0) && (matches.gl_pathc <= INT_MAX));
		count = (int)matches.gl_pathc;
		if (path && (count > 0))
		{
			const int n = snprintf(path, 1024, "%s", matches.gl_pathv[0]);
			if ((n < 0) || (n >= 1024))
				count = -1;
		}
	}
fail:
	globfree(&matches);
	return count;
}

static BOOL cleanup_directory(const char* directory)
{
	char path[1024] = WINPR_C_ARRAY_INIT;
	int count = 0;
	while ((count = find_files(directory, "", path)) > 0)
		CHECK(unlink(path) == 0);
	CHECK(count == 0);
	return TRUE;
fail:
	return FALSE;
}

static BOOL fifo_absent(const char* path)
{
	struct stat attributes = WINPR_C_ARRAY_INIT;
	return (lstat(path, &attributes) < 0) && (errno == ENOENT);
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

static BOOL subscriptions(rdpClientContext* cctx, size_t expected)
{
	return (PubSub_FindEventType(cctx->context.pubSub, "ChannelConnected")->EventHandlerCount ==
	        expected) &&
	       (PubSub_FindEventType(cctx->context.pubSub, "ChannelDisconnected")->EventHandlerCount ==
	        expected);
}

typedef BOOL (*settingsSetup)(rdpSettings*);

static BOOL enable_preconnection(rdpSettings* settings)
{
	return freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE);
}

static BOOL enable_arm(rdpSettings* settings)
{
	return freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, TRUE);
}

/* Each comparison gets fresh settings; only its settings preparation differs. */
static BOOL test_key_pair(const char* name, const char* value, settingsSetup setup, BOOL equal,
                          BOOL (*change)(rdpSettings*))
{
	BOOL ok = FALSE;
	BYTE leftHash[WINPR_SHA256_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE rightHash[WINPR_SHA256_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
	rdpSettings* left = freerdp_settings_new(0);
	rdpSettings* right = freerdp_settings_new(0);
	CHECK(left && right);
	CHECK(init_settings(left));
	CHECK(init_settings(right));
	if (setup)
	{
		CHECK(setup(left));
		CHECK(setup(right));
	}
	if (value)
		CHECK(freerdp_settings_set_value_for_name(right, name, value));
	if (change)
		CHECK(change(right));
	CHECK(rail_ipc_build_hash(left, leftHash));
	CHECK(rail_ipc_build_hash(right, rightHash));
	CHECK(equal == (memcmp(leftHash, rightHash, sizeof(leftHash)) == 0));
	ok = TRUE;
fail:
	freerdp_settings_free(left);
	freerdp_settings_free(right);
	if (!ok)
		(void)fprintf(stderr, "session-key case: %s (%s)\n", name,
		              setup ? "routing setup" : "ordinary");
	return ok;
}

static BOOL gateway_disabled(rdpSettings* settings)
{
	return freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_NONE_DIRECT);
}

static BOOL gateway_detect(rdpSettings* settings)
{
	return freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_DETECT);
}

static BOOL gateway_secrets(rdpSettings* settings)
{
	return freerdp_settings_set_string(settings, FreeRDP_GatewayPassword, "excluded") &&
	       freerdp_settings_set_string(settings, FreeRDP_GatewayAccessToken, "excluded");
}

static BOOL gateway_transport(rdpSettings* settings)
{
	return freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, FALSE) &&
	       freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE);
}

static BOOL server_redirection(rdpSettings* settings)
{
	return freerdp_settings_set_uint32(settings, FreeRDP_RedirectionFlags, LB_TARGET_FQDN) &&
	       freerdp_settings_set_string(settings, FreeRDP_RedirectionTargetFQDN,
	                                   "redirected.example.invalid");
}

static BOOL load_balance(rdpSettings* settings)
{
	static const char data[] = "Cookie: msts=collection-a";
	return freerdp_settings_set_pointer_len(settings, FreeRDP_LoadBalanceInfo, data,
	                                        sizeof(data) - 1U);
}

static BOOL reconnect_cookie(rdpSettings* settings)
{
	const ARC_SC_PRIVATE_PACKET cookie = {
		.cbLen = sizeof(cookie), .version = 1, .logonId = 42, .arcRandomBits = { 1, 2, 3, 4 }
	};
	return freerdp_settings_set_pointer_len(settings, FreeRDP_ServerAutoReconnectCookie, &cookie,
	                                        1);
}

static BOOL test_session_keys(void)
{
	const struct
	{
		const char* name;
		const char* value;
		BOOL equal;
		settingsSetup setup;
	} cases[] = {
		{ "same settings", nullptr, TRUE, nullptr },
		{ "FreeRDP_ServerHostname", "other.example.invalid", FALSE, nullptr },
		{ "FreeRDP_UserSpecifiedServerName", "route-a.example.invalid", FALSE, nullptr },
		{ "FreeRDP_ServerPort", "3390", FALSE, nullptr },
		{ "FreeRDP_Username", "other-user", FALSE, nullptr },
		{ "FreeRDP_Domain", "OTHER-DOMAIN", FALSE, nullptr },
		{ "FreeRDP_ConsoleSession", "TRUE", FALSE, nullptr },
		{ "FreeRDP_RedirectedSessionId", "42", FALSE, nullptr },
		{ "FreeRDP_VmConnectMode", "TRUE", FALSE, nullptr },
		{ "FreeRDP_ConnectChildSession", "TRUE", FALSE, nullptr },
		{ "FreeRDP_SendPreconnectionPdu", "TRUE", FALSE, nullptr },
		{ "FreeRDP_PreconnectionId", "7", FALSE, enable_preconnection },
		{ "FreeRDP_PreconnectionBlob", "vm-a", FALSE, enable_preconnection },
		{ "FreeRDP_GatewayHostname", "other-gateway.example.invalid", FALSE, nullptr },
		{ "FreeRDP_GatewayPort", "444", FALSE, nullptr },
		{ "FreeRDP_GatewayUsername", "other-user", FALSE, nullptr },
		{ "FreeRDP_GatewayDomain", "OTHER-GATEWAY", FALSE, nullptr },
		{ "FreeRDP_GatewayUrl", "https://gateway.example.invalid/other", FALSE, nullptr },
		{ "FreeRDP_GatewayArmTransport", "TRUE", FALSE, nullptr },
		{ "FreeRDP_GatewayAvdUseTenantid", "TRUE", FALSE, enable_arm },
		{ "FreeRDP_GatewayAzureActiveDirectory", "other-login.example.invalid", FALSE, set_arm },
		{ "FreeRDP_GatewayAvdAadtenantid", "tenant-b", FALSE, set_arm },
		{ "FreeRDP_RemoteApplicationProgram", "C:\\Windows\\System32\\calc.exe", FALSE, set_arm },
		{ "FreeRDP_Password", "excluded", TRUE, nullptr },
		{ "FreeRDP_RemoteApplicationProgram", "C:\\Windows\\System32\\calc.exe", TRUE, nullptr },
		{ "FreeRDP_RemoteApplicationWorkingDir", "C:\\Other", TRUE, nullptr },
		{ "FreeRDP_RemoteApplicationCmdLine", "different arguments", TRUE, nullptr },
		{ "FreeRDP_DesktopWidth", "1920", TRUE, nullptr },
		{ "FreeRDP_ProxyHostname", "proxy.example.invalid", TRUE, nullptr }
	};
	for (size_t x = 0; x < ARRAYSIZE(cases); x++)
		CHECK(
		    test_key_pair(cases[x].name, cases[x].value, cases[x].setup, cases[x].equal, nullptr));
	const struct
	{
		const char* name;
		BOOL (*change)(rdpSettings*);
		settingsSetup setup;
		BOOL equal;
	} groups[] = { { "gateway disabled", gateway_disabled, nullptr, FALSE },
		           { "gateway auto-detection", gateway_detect, nullptr, FALSE },
		           { "ARM authority and tenant", set_arm, enable_arm, FALSE },
		           { "gateway secrets", gateway_secrets, nullptr, TRUE },
		           { "gateway transport", gateway_transport, nullptr, TRUE },
		           { "server redirection", server_redirection, nullptr, TRUE },
		           { "load-balance bytes", load_balance, nullptr, FALSE },
		           { "reconnect cookie", reconnect_cookie, nullptr, FALSE } };
	for (size_t x = 0; x < ARRAYSIZE(groups); x++)
		CHECK(test_key_pair(groups[x].name, nullptr, groups[x].setup, groups[x].equal,
		                    groups[x].change));
	return TRUE;
fail:
	return FALSE;
}

static BOOL test_inactive_modes(const char* directory)
{
	const struct
	{
		const char* name;
		BOOL enabled;
		BOOL remoteApp;
		BOOL authOnly;
		BOOL preConnect;
	} cases[] = { { "disabled", FALSE, TRUE, FALSE, TRUE },
		          { "desktop", TRUE, FALSE, FALSE, TRUE },
		          { "authentication only", TRUE, TRUE, TRUE, TRUE },
		          { "never started", TRUE, TRUE, FALSE, FALSE } };
	for (size_t x = 0; x < ARRAYSIZE(cases); x++)
	{
		BOOL ok = FALSE;
		rdpClientContext* cctx = new_context();
		CHECK(cctx);
		CHECK(freerdp_settings_set_bool(cctx->context.settings, FreeRDP_RemoteApplicationMultiExec,
		                                cases[x].enabled));
		CHECK(freerdp_settings_set_bool(cctx->context.settings, FreeRDP_RemoteApplicationMode,
		                                cases[x].remoteApp));
		CHECK(freerdp_settings_set_bool(cctx->context.settings, FreeRDP_AuthenticationOnly,
		                                cases[x].authOnly));
		if (cases[x].preConnect)
			CHECK(cctx->context.instance->PreConnect(cctx->context.instance));
		CHECK(freerdp_client_rail_multi_exec_set_ready(cctx, TRUE));
		CHECK(subscriptions(cctx, 0));
		ok = TRUE;
	fail:
		freerdp_client_context_free((rdpContext*)cctx);
		if (!ok || (find_files(directory, "", nullptr) != 0))
		{
			(void)fprintf(stderr, "inactive case: %s\n", cases[x].name);
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL test_runtime_directory_and_lock(testContext* ctx, const char* directory)
{
	BOOL ok = FALSE;
	char path[1024] = WINPR_C_ARRAY_INIT;
	rdpClientContext* cctx = ctx->cctx;
	CHECK(start(cctx));
	CHECK(find_files(directory, ".fifo", path) == 1);
	rail_ipc_stop(cctx);
	CHECK(chmod(directory, 0755) == 0);
	CHECK(expect_failed_start(cctx));
	CHECK(chmod(directory, 0700) == 0);
	memcpy(&path[strlen(path) - 5U], ".lock", sizeof(".lock"));
	CHECK(unlink(path) == 0);
	CHECK(symlink("missing", path) == 0);
	CHECK(find_files(directory, ".lock", nullptr) == 1);
	CHECK(expect_failed_start(cctx));
	CHECK(unlink(path) == 0);
	ok = TRUE;
fail:
	if (chmod(directory, 0700) < 0)
		ok = FALSE;
	return ok;
}

static BOOL test_ownership_and_shutdown(testContext* ctx, const char* directory)
{
	char fifo[1024] = WINPR_C_ARRAY_INIT;
	rdpClientContext* first = ctx->cctx;
	ctx->other = new_context();
	const int descriptors = count_descriptors();
	CHECK(ctx->other && (descriptors >= 0));
	CHECK(start(first));
	CHECK(start(first));
	CHECK(find_files(directory, ".fifo", fifo) == 1);
	CHECK(freerdp_settings_set_bool(ctx->other->context.settings,
	                                FreeRDP_RemoteApplicationMultiExec, TRUE));
	CHECK(expect_failed_start(ctx->other));
	CHECK(find_files(directory, ".fifo", nullptr) == 1);
	CHECK(freerdp_settings_set_string(ctx->other->context.settings, FreeRDP_ServerHostname,
	                                  "other.example.invalid"));
	CHECK(start(ctx->other));
	CHECK(find_files(directory, ".fifo", nullptr) == 2);
	rail_ipc_stop(ctx->other);
	CHECK(find_files(directory, ".fifo", nullptr) == 1);
	CHECK(access(fifo, F_OK) == 0);
	CHECK(subscriptions(first, 1));
	first->context.instance->PostFinalDisconnect(first->context.instance);
	CHECK(subscriptions(first, 0));
	CHECK(count_descriptors() == descriptors);
	CHECK(fifo_absent(fifo));
	rail_ipc_stop(first);
	CHECK(subscriptions(first, 0));
	CHECK(count_descriptors() == descriptors);
	CHECK(fifo_absent(fifo));
	CHECK(find_files(directory, ".lock", nullptr) >= 1);
	return TRUE;
fail:
	return FALSE;
}

static BOOL test_stale_fifo_replaced(testContext* ctx, const char* directory)
{
	char fifo[1024] = WINPR_C_ARRAY_INIT;
	struct stat after = WINPR_C_ARRAY_INIT;
	rdpClientContext* cctx = ctx->cctx;
	CHECK(start(cctx));
	CHECK(find_files(directory, ".fifo", fifo) == 1);
	rail_ipc_stop(cctx);
	CHECK(mkfifo(fifo, 0600) == 0);
	CHECK(chmod(fifo, 0644) == 0);
	CHECK(start(cctx));
	CHECK(lstat(fifo, &after) == 0);
	CHECK(S_ISFIFO(after.st_mode) && ((after.st_mode & 0777) == 0600));
	freerdp_client_context_free(&cctx->context);
	ctx->cctx = nullptr;
	CHECK(fifo_absent(fifo));
	return TRUE;
fail:
	return FALSE;
}

static BOOL fail_pre_connect(WINPR_ATTR_UNUSED freerdp* instance)
{
	return FALSE;
}

static BOOL test_retry_and_restart(testContext* ctx, const char* directory)
{
	rdpClientContext* cctx = ctx->cctx;
	cctx->PreConnect = fail_pre_connect;
	CHECK(!start(cctx));
	CHECK(!start(cctx));
	CHECK(subscriptions(cctx, 1));
	CHECK(find_files(directory, ".fifo", nullptr) == 1);
	rail_ipc_stop(cctx);
	CHECK(subscriptions(cctx, 0));
	CHECK(find_files(directory, ".fifo", nullptr) == 0);
	CHECK(!start(cctx));
	CHECK(!start(cctx));
	CHECK(subscriptions(cctx, 1));
	CHECK(find_files(directory, ".fifo", nullptr) == 1);
	freerdp_client_context_free(&cctx->context);
	ctx->cctx = nullptr;
	CHECK(find_files(directory, ".fifo", nullptr) == 0);
	return TRUE;
fail:
	return FALSE;
}

static UINT count_execute(RailClientContext* rail, WINPR_ATTR_UNUSED const RAIL_EXEC_ORDER* order)
{
	testContext* capture = rail->custom;
	capture->count++;
	return (capture->rejectFirst && (capture->count == 1)) ? ERROR_INTERNAL_ERROR : CHANNEL_RC_OK;
}

static DWORD event_count(rdpClientContext* cctx)
{
	HANDLE events[64] = WINPR_C_ARRAY_INIT;
	return freerdp_get_event_handles(&cctx->context, events, ARRAYSIZE(events));
}

static BOOL dispatch_expect(testContext* ctx, size_t expected)
{
	return freerdp_channels_check_fds(ctx->cctx->context.channels, ctx->cctx->context.instance) &&
	       (ctx->count == expected);
}

static BOOL write_text(int writer, const char* text)
{
	const size_t length = strlen(text);
	return write(writer, text, length) == (ssize_t)length;
}

static BOOL write_record(int writer)
{
	return write_text(writer, "program=app\n\n");
}

static int connect_rail(testContext* ctx, const char* name)
{
	const ChannelConnectedEventArgs args = { .name = name, .pInterface = &ctx->rail };
	return PubSub_OnChannelConnected(ctx->cctx->context.pubSub, &ctx->cctx->context, &args);
}

static int disconnect_rail(testContext* ctx, const char* name)
{
	const ChannelDisconnectedEventArgs args = { .name = name, .pInterface = &ctx->rail };
	return PubSub_OnChannelDisconnected(ctx->cctx->context.pubSub, &ctx->cctx->context, &args);
}

static void test_close(testContext* ctx)
{
	freerdp_client_context_free((rdpContext*)ctx->other);
	if (ctx->writer >= 0)
		(void)close(ctx->writer);
	freerdp_client_context_free((rdpContext*)ctx->cctx);
}

static BOOL dispatch_open(testContext* ctx, const char* directory)
{
	ctx->rail.custom = ctx;
	ctx->rail.ClientExecute = count_execute;
	CHECK(start(ctx->cctx));
	CHECK(find_files(directory, ".fifo", ctx->fifo) == 1);
	ctx->base = event_count(ctx->cctx);
	CHECK(ctx->base > 0);
	CHECK(connect_rail(ctx, RAIL_SVC_CHANNEL_NAME) == 1);
	CHECK(freerdp_client_channel_get_registered_event_handles(ctx->cctx->context.channels, nullptr,
	                                                          0) == 0);
	ctx->writer = open(ctx->fifo, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	CHECK(ctx->writer >= 0);
	CHECK(rdp_client_transition_to_state(ctx->cctx->context.rdp, CONNECTION_STATE_ACTIVE));
	return TRUE;
fail:
	return FALSE;
}

static BOOL test_readiness_and_channel_filter(testContext* ctx, const char* directory)
{
	CHECK(dispatch_open(ctx, directory));
	CHECK(event_count(ctx->cctx) == ctx->base);
	CHECK(write_record(ctx->writer));
	CHECK(dispatch_expect(ctx, 0));
	CHECK(rdp_client_transition_to_state(ctx->cctx->context.rdp, CONNECTION_STATE_INITIAL));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(event_count(ctx->cctx) == ctx->base + 1U);
	CHECK(dispatch_expect(ctx, 0));
	CHECK(rdp_client_transition_to_state(ctx->cctx->context.rdp, CONNECTION_STATE_ACTIVE));
	CHECK(dispatch_expect(ctx, 1));
	CHECK(connect_rail(ctx, "unrelated") == 1);
	CHECK(disconnect_rail(ctx, "unrelated") == 1);
	CHECK(event_count(ctx->cctx) == ctx->base + 1U);
	return TRUE;
fail:
	return FALSE;
}

static BOOL test_revoke_disconnect_and_recovery(testContext* ctx, const char* directory)
{
	CHECK(dispatch_open(ctx, directory));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(dispatch_expect(ctx, 0));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, FALSE));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, FALSE));
	CHECK(event_count(ctx->cctx) == ctx->base);
	CHECK(write_record(ctx->writer));
	CHECK(dispatch_expect(ctx, 0));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	ctx->cctx->context.instance->PostDisconnect(ctx->cctx->context.instance);
	CHECK(event_count(ctx->cctx) == ctx->base);
	CHECK(disconnect_rail(ctx, RAIL_SVC_CHANNEL_NAME) == 1);
	CHECK(event_count(ctx->cctx) == ctx->base);
	CHECK(write_record(ctx->writer));
	CHECK(dispatch_expect(ctx, 0));
	CHECK(connect_rail(ctx, RAIL_SVC_CHANNEL_NAME) == 1);
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(dispatch_expect(ctx, 2));
	return TRUE;
fail:
	return FALSE;
}

static BOOL test_channel_container_replaced(testContext* ctx, const char* directory)
{
	struct stat before = WINPR_C_ARRAY_INIT;
	struct stat after = WINPR_C_ARRAY_INIT;
	CHECK(dispatch_open(ctx, directory));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(lstat(ctx->fifo, &before) == 0);
	CHECK(disconnect_rail(ctx, RAIL_SVC_CHANNEL_NAME) == 1);
	CHECK(event_count(ctx->cctx) == ctx->base);
	freerdp_channels_close(ctx->cctx->context.channels, ctx->cctx->context.instance);
	freerdp_channels_free(ctx->cctx->context.channels);
	ctx->cctx->context.channels = freerdp_channels_new(ctx->cctx->context.instance);
	CHECK(ctx->cctx->context.channels);
	CHECK(connect_rail(ctx, RAIL_SVC_CHANNEL_NAME) == 1);
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(write_record(ctx->writer));
	CHECK(dispatch_expect(ctx, 1));
	CHECK(lstat(ctx->fifo, &after) == 0);
	CHECK((before.st_dev == after.st_dev) && (before.st_ino == after.st_ino));
	CHECK(subscriptions(ctx->cctx, 1));
	return TRUE;
fail:
	return FALSE;
}

static BOOL test_partial_record_survives_reconnect(testContext* ctx, const char* directory)
{
	CHECK(dispatch_open(ctx, directory));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(write_text(ctx->writer, "program="));
	CHECK(dispatch_expect(ctx, 0));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, FALSE));
	CHECK(disconnect_rail(ctx, RAIL_SVC_CHANNEL_NAME) == 1);
	CHECK(write_text(ctx->writer, "app\n\n"));
	CHECK(dispatch_expect(ctx, 0));
	CHECK(connect_rail(ctx, RAIL_SVC_CHANNEL_NAME) == 1);
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(dispatch_expect(ctx, 1));
	return TRUE;
fail:
	return FALSE;
}

static BOOL test_delivery_and_local_failure(testContext* ctx, const char* directory)
{
	char full[RAIL_DECODER_CAPACITY] = WINPR_C_ARRAY_INIT;
	const char prefix[] = "program=app\narguments=";
	const size_t first = sizeof(full) - 16U;
	CHECK(dispatch_open(ctx, directory));
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	HANDLE event = nullptr;
	CHECK(freerdp_client_channel_get_registered_event_handles(ctx->cctx->context.channels, &event,
	                                                          1) == 1);
	CHECK(event);
	memset(full, 'x', sizeof(full));
	memcpy(full, prefix, sizeof(prefix) - 1U);
	full[sizeof(full) - 2U] = '\n';
	full[sizeof(full) - 1U] = '\n';
	CHECK(write(ctx->writer, full, first) == (ssize_t)first);
	CHECK(dispatch_expect(ctx, 0));
	CHECK(write(ctx->writer, full + first, 16) == 16);
	CHECK(write_record(ctx->writer));
	CHECK(dispatch_expect(ctx, 1));
	CHECK(WaitForSingleObject(event, 0) == WAIT_OBJECT_0);
	CHECK(dispatch_expect(ctx, 2));
	CHECK(WaitForSingleObject(event, 0) == WAIT_TIMEOUT);
	ctx->count = 0;
	ctx->rejectFirst = TRUE;
	CHECK(write_record(ctx->writer));
	CHECK(write_record(ctx->writer));
	CHECK(dispatch_expect(ctx, 2));
	return TRUE;
fail:
	return FALSE;
}

static BOOL test_missing_channel_disables_keeps_lock(testContext* ctx, const char* directory)
{
	ctx->other = new_context();
	CHECK(dispatch_open(ctx, directory));
	CHECK(ctx->other);
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(disconnect_rail(ctx, RAIL_SVC_CHANNEL_NAME) == 1);
	CHECK(!freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(event_count(ctx->cctx) == ctx->base);
	CHECK(find_files(directory, ".fifo", nullptr) == 0);
	CHECK(ctx->count == 0);
	CHECK(freerdp_client_rail_multi_exec_set_ready(ctx->cctx, TRUE));
	CHECK(event_count(ctx->cctx) == ctx->base);
	CHECK(freerdp_settings_set_bool(ctx->other->context.settings,
	                                FreeRDP_RemoteApplicationMultiExec, TRUE));
	CHECK(expect_failed_start(ctx->other));
	CHECK(subscriptions(ctx->cctx, 1));
	return TRUE;
fail:
	return FALSE;
}

int TestClientRailIpc(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	struct rlimit limits = WINPR_C_ARRAY_INIT;
	if (getrlimit(RLIMIT_NOFILE, &limits) != 0)
	{
		perror("getrlimit");
		return -1;
	}
	if (limits.rlim_cur > 256)
	{
		limits.rlim_cur = 256;
		if (setrlimit(RLIMIT_NOFILE, &limits) != 0)
		{
			perror("setrlimit");
			return -1;
		}
	}
	char directory[] = "/tmp/freerdp-rail-test.XXXXXX";
	BOOL (*const tests[])(testContext*, const char*) = {
		test_runtime_directory_and_lock,   test_ownership_and_shutdown,
		test_stale_fifo_replaced,          test_retry_and_restart,
		test_readiness_and_channel_filter, test_revoke_disconnect_and_recovery,
		test_channel_container_replaced,   test_partial_record_survives_reconnect,
		test_delivery_and_local_failure,   test_missing_channel_disables_keeps_lock
	};
	if (!mkdtemp(directory))
		return -1;
	BOOL ok = SetEnvironmentVariableA("XDG_RUNTIME_DIR", directory);
	if (ok)
		ok = test_session_keys();
	for (size_t x = 0; ok && (x < ARRAYSIZE(tests)); x++)
	{
		testContext ctx = { .writer = -1 };
		ok = cleanup_directory(directory);
		if (ok)
		{
			ctx.cctx = new_context();
			ok = ctx.cctx && tests[x](&ctx, directory);
		}
		test_close(&ctx);
	}
	if (ok)
		ok = cleanup_directory(directory) && test_inactive_modes(directory);
	if (!cleanup_directory(directory))
		ok = FALSE;
	if (rmdir(directory) < 0)
		ok = FALSE;
	if (!SetEnvironmentVariableA("XDG_RUNTIME_DIR", nullptr))
		ok = FALSE;
	return ok ? 0 : -1;
}

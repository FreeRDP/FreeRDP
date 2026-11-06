/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Common RAIL launch IPC
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

#include <freerdp/config.h>
#include "rail_ipc.h"

#include <winpr/assert.h>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && defined(CHANNEL_RAIL_CLIENT)

#include <freerdp/channels/channels.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/custom-crypto.h>
#include <winpr/endian.h>
#include <winpr/environment.h>
#include <winpr/path.h>
#include <winpr/print.h>
#include <winpr/string.h>
#include <winpr/synch.h>

#include "rail_decoder.h"

#define RAIL_IPC_HASH_VERSION 2U
#define RAIL_IPC_NAME_PREFIX "freerdp-rail-v1-"
struct rdp_client_rail_ipc
{
	RailClientContext* rail;
	BOOL subscribed;
	rdpChannels* registeredOn;
	wLog* log;
	char* path;
	char* lockPath;
	int lockFd;
	int readFd;
	int heldWriteFd;
	HANDLE readEvent;
	BOOL fifoCreated;
	RailDecoder decoder;
};

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_field(WINPR_DIGEST_CTX* digest, const void* data, size_t length)
{
	BYTE encodedLength[4] = WINPR_C_ARRAY_INIT;

	if (length > UINT32_MAX)
		return FALSE;

	winpr_Data_Write_UINT32_BE(encodedLength, WINPR_ASSERTING_INT_CAST(UINT32, length));
	if (!winpr_Digest_Update(digest, encodedLength, sizeof(encodedLength)))
		return FALSE;

	return (length == 0) || winpr_Digest_Update(digest, data, length);
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_string(WINPR_DIGEST_CTX* digest, const char* value)
{
	if (!value)
		return rail_ipc_digest_field(digest, nullptr, 0);
	return rail_ipc_digest_field(digest, value, strlen(value));
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_uint32(WINPR_DIGEST_CTX* digest, UINT32 value)
{
	BYTE encoded[4] = WINPR_C_ARRAY_INIT;
	winpr_Data_Write_UINT32_BE(encoded, value);
	return rail_ipc_digest_field(digest, encoded, sizeof(encoded));
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_bool(WINPR_DIGEST_CTX* digest, BOOL value)
{
	const BYTE encoded = value ? 1 : 0;
	return rail_ipc_digest_field(digest, &encoded, sizeof(encoded));
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_reconnect_cookie(WINPR_DIGEST_CTX* digest,
                                             const ARC_SC_PRIVATE_PACKET* cookie)
{
	if (!rail_ipc_digest_bool(digest, cookie != nullptr))
		return FALSE;
	if (!cookie)
		return TRUE;
	return rail_ipc_digest_uint32(digest, cookie->cbLen) &&
	       rail_ipc_digest_uint32(digest, cookie->version) &&
	       rail_ipc_digest_uint32(digest, cookie->logonId) &&
	       rail_ipc_digest_field(digest, cookie->arcRandomBits, sizeof(cookie->arcRandomBits));
}

WINPR_ATTR_NODISCARD
BOOL rail_ipc_build_hash(const rdpSettings* settings, BYTE hash[WINPR_SHA256_DIGEST_LENGTH])
{
	BOOL rc = FALSE;
	const BYTE version = RAIL_IPC_HASH_VERSION;
	WINPR_DIGEST_CTX* digest = nullptr;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(hash);

	digest = winpr_Digest_New();
	if (!digest)
		return FALSE;

	static const FreeRDP_Settings_Keys_String strings[] = { FreeRDP_ServerHostname,
		                                                    FreeRDP_UserSpecifiedServerName,
		                                                    FreeRDP_Username,
		                                                    FreeRDP_Domain,
		                                                    FreeRDP_PreconnectionBlob,
		                                                    FreeRDP_GatewayHostname,
		                                                    FreeRDP_GatewayUsername,
		                                                    FreeRDP_GatewayDomain,
		                                                    FreeRDP_GatewayUrl,
		                                                    FreeRDP_GatewayAzureActiveDirectory,
		                                                    FreeRDP_GatewayAvdAadtenantid };
	static const FreeRDP_Settings_Keys_UInt32 integers[] = {
		FreeRDP_ServerPort, FreeRDP_RedirectedSessionId, FreeRDP_PreconnectionId,
		FreeRDP_GatewayPort, FreeRDP_GatewayUsageMethod
	};
	static const FreeRDP_Settings_Keys_Bool booleans[] = {
		FreeRDP_ConsoleSession,       FreeRDP_VmConnectMode,        FreeRDP_ConnectChildSession,
		FreeRDP_SendPreconnectionPdu, FreeRDP_GatewayEnabled,       FreeRDP_GatewayBypassLocal,
		FreeRDP_GatewayArmTransport,  FreeRDP_GatewayAvdUseTenantid
	};
	if (!winpr_Digest_Init(digest, WINPR_MD_SHA256) ||
	    !winpr_Digest_Update(digest, &version, sizeof(version)))
		goto out;
	for (size_t x = 0; x < ARRAYSIZE(strings); x++)
	{
		if (!rail_ipc_digest_string(digest, freerdp_settings_get_string(settings, strings[x])))
			goto out;
	}
	for (size_t x = 0; x < ARRAYSIZE(integers); x++)
	{
		if (!rail_ipc_digest_uint32(digest, freerdp_settings_get_uint32(settings, integers[x])))
			goto out;
	}
	for (size_t x = 0; x < ARRAYSIZE(booleans); x++)
	{
		if (!rail_ipc_digest_bool(digest, freerdp_settings_get_bool(settings, booleans[x])))
			goto out;
	}
	const UINT32 loadBalanceLength =
	    freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
	const void* loadBalance = freerdp_settings_get_pointer(settings, FreeRDP_LoadBalanceInfo);
	if (((loadBalanceLength > 0) && !loadBalance) ||
	    !rail_ipc_digest_field(digest, loadBalance, loadBalanceLength) ||
	    !rail_ipc_digest_reconnect_cookie(
	        digest, freerdp_settings_get_pointer(settings, FreeRDP_ServerAutoReconnectCookie)))
		goto out;
	/* The program selects an ARM route; ordinary programs share the same connection. */
	if (freerdp_settings_get_bool(settings, FreeRDP_GatewayEnabled) &&
	    freerdp_settings_get_bool(settings, FreeRDP_GatewayArmTransport) &&
	    !rail_ipc_digest_string(
	        digest, freerdp_settings_get_string(settings, FreeRDP_RemoteApplicationProgram)))
		goto out;

	rc = winpr_Digest_Final(digest, hash, WINPR_SHA256_DIGEST_LENGTH);

out:
	winpr_Digest_Free(digest);
	return rc;
}

static void rail_ipc_log_errno(const rdpClientRailIpc* ipc, DWORD level, const char* operation,
                               int error)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(operation);
	char ebuffer[256] = WINPR_C_ARRAY_INIT;
	WLog_Print(ipc->log, level, "RAIL IPC %s failed: %s [%d]", operation,
	           winpr_strerror(error, ebuffer, sizeof(ebuffer)), error);
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_build_names(rdpClientRailIpc* ipc, const rdpSettings* settings,
                                 const char* runtimeDirectory)
{
	BYTE hash[WINPR_SHA256_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
	char hex[WINPR_SHA256_DIGEST_LENGTH * 2U + 1U] = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(ipc);
	WINPR_ASSERT(settings);
	WINPR_ASSERT(runtimeDirectory);

	if (!rail_ipc_build_hash(settings, hash) ||
	    (winpr_BinToHexStringBuffer(hash, sizeof(hash), hex, sizeof(hex), FALSE) !=
	     sizeof(hash) * 2U))
		return FALSE;

	ipc->path = GetCombinedPathV(runtimeDirectory, RAIL_IPC_NAME_PREFIX "%s.fifo", hex);
	ipc->lockPath = GetCombinedPathV(runtimeDirectory, RAIL_IPC_NAME_PREFIX "%s.lock", hex);
	return ipc->path && ipc->lockPath;
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_check_runtime_directory(rdpClientRailIpc* ipc, const char* runtimeDirectory)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(runtimeDirectory);

	struct stat attributes = WINPR_C_ARRAY_INIT;
	if (stat(runtimeDirectory, &attributes) < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "checking XDG_RUNTIME_DIR", errno);
		return FALSE;
	}
	if (!S_ISDIR(attributes.st_mode) || (attributes.st_uid != getuid()) ||
	    ((attributes.st_mode & (S_IRWXG | S_IRWXO)) != 0))
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC rejected XDG_RUNTIME_DIR with unsafe type, owner, or permissions");
		return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_open_lock(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);

	ipc->lockFd = open(ipc->lockPath, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
	if (ipc->lockFd < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "opening the ownership lock", errno);
		return FALSE;
	}

	for (;;)
	{
		if (flock(ipc->lockFd, LOCK_EX | LOCK_NB) == 0)
			return TRUE;
		const int error = errno;
		if (error == EINTR)
			continue;
		if ((error == EWOULDBLOCK) || (error == EAGAIN))
			WLog_Print(ipc->log, WLOG_ERROR,
			           "RAIL IPC setup refused because another primary owns this session");
		else
			rail_ipc_log_errno(ipc, WLOG_ERROR, "locking primary ownership", error);
		return FALSE;
	}
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_open_fifo(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);
	/* Ownership is locked before replacing a stale FIFO. */
	if ((unlink(ipc->path) < 0) && (errno != ENOENT))
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "removing the stale launch FIFO", errno);
		return FALSE;
	}
	if (mkfifo(ipc->path, 0600) < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "creating the launch FIFO", errno);
		return FALSE;
	}
	ipc->fifoCreated = TRUE;
	ipc->readFd = open(ipc->path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
	if (ipc->readFd < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "opening the launch FIFO for reading", errno);
		return FALSE;
	}
	ipc->heldWriteFd = open(ipc->path, O_WRONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
	if (ipc->heldWriteFd < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "opening the held launch FIFO writer", errno);
		return FALSE;
	}
	ipc->readEvent = CreateFileDescriptorEvent(nullptr, FALSE, FALSE, ipc->readFd, WINPR_FD_READ);
	if (!ipc->readEvent)
	{
		WLog_Print(ipc->log, WLOG_ERROR, "RAIL IPC could not create the launch FIFO event");
		return FALSE;
	}
	return TRUE;
}

static void rail_ipc_unregister(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);
	if (!ipc->registeredOn)
		return;
	(void)freerdp_client_channel_unregister(ipc->registeredOn, ipc->readEvent);
	ipc->registeredOn = nullptr;
}

static void rail_ipc_disable(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);
	rail_ipc_unregister(ipc);
	if (ipc->readEvent)
	{
		(void)CloseHandle(ipc->readEvent);
		ipc->readEvent = nullptr;
	}
	if (ipc->heldWriteFd >= 0)
	{
		(void)close(ipc->heldWriteFd);
		ipc->heldWriteFd = -1;
	}
	if (ipc->readFd >= 0)
	{
		(void)close(ipc->readFd);
		ipc->readFd = -1;
	}
	if (ipc->fifoCreated)
	{
		if ((unlink(ipc->path) < 0) && (errno != ENOENT))
			rail_ipc_log_errno(ipc, WLOG_WARN, "removing the launch FIFO", errno);
		ipc->fifoCreated = FALSE;
	}
}

static BOOL rail_ipc_on_event(rdpContext* context, void* userdata)
{
	rdpClientRailIpc* ipc = userdata;
	WINPR_ASSERT(context);
	WINPR_ASSERT(ipc);
	if (freerdp_get_state(context) != CONNECTION_STATE_ACTIVE)
		return TRUE;
	WINPR_ASSERT(ipc->rail);
	WINPR_ASSERT(ipc->rail->ClientExecute);
	RailDecoder* dec = &ipc->decoder;
	const ssize_t count =
	    read(ipc->readFd, dec->buffer + dec->used, RAIL_DECODER_CAPACITY - dec->used);
	if (count <= 0)
	{
		if ((count < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)))
			return TRUE;
		if (count == 0)
			WLog_Print(ipc->log, WLOG_ERROR, "RAIL IPC disabled after unexpected FIFO EOF");
		else
			rail_ipc_log_errno(ipc, WLOG_ERROR, "reading the launch FIFO", errno);
		rail_ipc_disable(ipc);
		return TRUE;
	}
	dec->used += (size_t)count;
	for (;;)
	{
		size_t consumed = 0;
		RAIL_EXEC_ORDER order = WINPR_C_ARRAY_INIT;
		const char* reason = nullptr;
		const RailDecoderResult result = rail_decoder_next(dec, &consumed, &order, &reason);
		if (result == RAIL_DECODER_INCOMPLETE)
			break;
		if (reason)
			WLog_Print(ipc->log, WLOG_WARN, "RAIL IPC rejected launch record: %s", reason);
		if (result == RAIL_DECODER_RECORD)
		{
			const UINT status = ipc->rail->ClientExecute(ipc->rail, &order);
			if (status != CHANNEL_RC_OK)
				WLog_Print(ipc->log, WLOG_WARN,
				           "RAIL IPC ClientExecute rejected a launch locally with error %" PRIu32,
				           status);
		}
		rail_decoder_consume(dec, consumed);
	}
	return TRUE;
}

void rail_ipc_attach(rdpClientContext* cctx, RailClientContext* rail)
{
	WINPR_ASSERT(cctx);
	rdpClientRailIpc* ipc = cctx->railIpc;
	if (!ipc || (ipc->lockFd < 0))
		return;
	rail_ipc_unregister(ipc);
	ipc->rail = rail;
}

void rail_ipc_detach(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);
	rdpClientRailIpc* ipc = cctx->railIpc;
	if (!ipc || (ipc->lockFd < 0))
		return;
	rail_ipc_unregister(ipc);
	ipc->rail = nullptr;
}

BOOL rail_ipc_set_ready(rdpClientContext* cctx, BOOL ready)
{
	WINPR_ASSERT(cctx);
	rdpClientRailIpc* ipc = cctx->railIpc;
	if (!ipc || (ipc->lockFd < 0))
		return TRUE;
	if (!ipc->readEvent)
		return TRUE;
	if (!ready)
	{
		rail_ipc_unregister(ipc);
		return TRUE;
	}
	if (!ipc->rail)
	{
		WLog_Print(ipc->log, WLOG_ERROR, "RAIL IPC cannot become ready without a RAIL channel");
		rail_ipc_disable(ipc);
		return FALSE;
	}
	if (ipc->registeredOn)
		return TRUE;
	if (!freerdp_client_channel_register(cctx->context.channels, ipc->readEvent, rail_ipc_on_event,
	                                     ipc))
	{
		WLog_Print(ipc->log, WLOG_ERROR, "RAIL IPC could not register its launch FIFO event");
		rail_ipc_disable(ipc);
		return FALSE;
	}
	ipc->registeredOn = cctx->context.channels;
	return TRUE;
}

static void rail_ipc_on_channel_connected(void* context, const wEventArgs* args)
{
	const ChannelConnectedEventArgs* e = (const ChannelConnectedEventArgs*)args;
	WINPR_ASSERT(context);
	WINPR_ASSERT(e);
	if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
		rail_ipc_attach((rdpClientContext*)context, (RailClientContext*)e->pInterface);
}

static void rail_ipc_on_channel_disconnected(void* context, const wEventArgs* args)
{
	const ChannelDisconnectedEventArgs* e = (const ChannelDisconnectedEventArgs*)args;
	WINPR_ASSERT(context);
	WINPR_ASSERT(e);
	if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
		rail_ipc_detach((rdpClientContext*)context);
}

BOOL rail_ipc_start(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);
	const rdpSettings* settings = cctx->context.settings;
	if (!freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationMultiExec) ||
	    !freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationMode) ||
	    freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
		return TRUE;
	rdpClientRailIpc* ipc = cctx->railIpc;
	if (!ipc)
	{
		ipc = calloc(1, sizeof(*ipc));
		if (!ipc)
			return FALSE;
		ipc->lockFd = -1;
		ipc->readFd = -1;
		ipc->heldWriteFd = -1;
		cctx->railIpc = ipc;
	}
	if (ipc->lockFd >= 0)
		return TRUE;
	ipc->log = cctx->context.log;

	char* runtimeDirectory = GetEnvAlloc("XDG_RUNTIME_DIR");
	if (!runtimeDirectory)
	{
		WLog_Print(ipc->log, WLOG_ERROR, "RAIL IPC requires XDG_RUNTIME_DIR");
		goto fail;
	}
	if (!rail_ipc_build_names(ipc, settings, runtimeDirectory) ||
	    !rail_ipc_check_runtime_directory(ipc, runtimeDirectory) || !rail_ipc_open_lock(ipc) ||
	    !rail_ipc_open_fifo(ipc))
	{
		free(runtimeDirectory);
		goto fail;
	}
	free(runtimeDirectory);
	if (PubSub_Subscribe(cctx->context.pubSub, "ChannelConnected", rail_ipc_on_channel_connected) <
	    0)
		goto fail;
	if (PubSub_Subscribe(cctx->context.pubSub, "ChannelDisconnected",
	                     rail_ipc_on_channel_disconnected) < 0)
	{
		PubSub_Unsubscribe(cctx->context.pubSub, "ChannelConnected", rail_ipc_on_channel_connected);
		goto fail;
	}
	ipc->subscribed = TRUE;
	WLog_Print(ipc->log, WLOG_INFO, "RAIL launch FIFO available at %s", ipc->path);
	return TRUE;
fail:
	rail_ipc_stop(cctx);
	return FALSE;
}

void rail_ipc_stop(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);
	rdpClientRailIpc* ipc = cctx->railIpc;
	if (!ipc)
		return;
	if (ipc->subscribed)
	{
		PubSub_Unsubscribe(cctx->context.pubSub, "ChannelConnected", rail_ipc_on_channel_connected);
		PubSub_Unsubscribe(cctx->context.pubSub, "ChannelDisconnected",
		                   rail_ipc_on_channel_disconnected);
	}
	rail_ipc_disable(ipc);
	/* Closing the lock descriptor releases ownership; the lock file stays. */
	if (ipc->lockFd >= 0)
		(void)close(ipc->lockFd);
	free(ipc->path);
	free(ipc->lockPath);
	*ipc = (rdpClientRailIpc){ .lockFd = -1, .readFd = -1, .heldWriteFd = -1 };
}

void rail_ipc_free(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);
	rail_ipc_stop(cctx);
	free(cctx->railIpc);
	cctx->railIpc = nullptr;
}

#else

BOOL rail_ipc_start(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);
	if (!freerdp_settings_get_bool(cctx->context.settings, FreeRDP_RemoteApplicationMultiExec))
		return TRUE;
	WLog_Print(cctx->context.log, WLOG_ERROR, "RAIL multi-exec is unsupported by this build");
	return FALSE;
}

void rail_ipc_stop(WINPR_ATTR_UNUSED rdpClientContext* cctx)
{
}

void rail_ipc_free(WINPR_ATTR_UNUSED rdpClientContext* cctx)
{
}

void rail_ipc_attach(WINPR_ATTR_UNUSED rdpClientContext* cctx,
                     WINPR_ATTR_UNUSED RailClientContext* rail)
{
}

void rail_ipc_detach(WINPR_ATTR_UNUSED rdpClientContext* cctx)
{
}

BOOL rail_ipc_set_ready(WINPR_ATTR_UNUSED rdpClientContext* cctx, WINPR_ATTR_UNUSED BOOL ready)
{
	return TRUE;
}

#endif

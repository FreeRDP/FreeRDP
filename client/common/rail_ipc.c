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

#endif

#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/custom-crypto.h>
#include <winpr/endian.h>
#include <winpr/environment.h>
#include <winpr/path.h>
#include <winpr/print.h>
#include <winpr/string.h>
#include <winpr/synch.h>

#define RAIL_IPC_NAME_PREFIX "freerdp-rail-v1-"

struct rdp_client_rail_callbacks
{
	BOOL saved;
	pcRailServerExecuteResult ServerExecuteResult;
	pMonitoredDesktop MonitoredDesktop;
	pNonMonitoredDesktop NonMonitoredDesktop;
	void* context;
};

struct rdp_client_rail_ipc
{
	RailClientContext* rail;
	wLog* log;
	char* path;
	int readFd;
	int writeFd;
	HANDLE readEvent;
	BOOL fifoCreated;

	const rdpClientContext* cctx;
	wStream* buffer;
	struct rdp_client_rail_callbacks client;
	CRITICAL_SECTION lock;
};

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && defined(CHANNEL_RAIL_CLIENT)

static void rail_ipc_stop(rdpClientRailIpc* ipc);

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_start(rdpClientRailIpc* ipc);

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_build_hash(const rdpSettings* settings, BYTE* hash, size_t len)
{
	BOOL rc = FALSE;
	WINPR_DIGEST_CTX* digest = nullptr;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(hash);

	digest = winpr_Digest_New();
	if (!digest)
		return FALSE;

	static const FreeRDP_Settings_Keys_String strings[] = { FreeRDP_ServerHostname,
		                                                    FreeRDP_UserSpecifiedServerName,
		                                                    FreeRDP_Username, FreeRDP_Domain,
		                                                    FreeRDP_PreconnectionBlob };
	static const FreeRDP_Settings_Keys_UInt32 integers[] = { FreeRDP_ServerPort,
		                                                     FreeRDP_RedirectedSessionId,
		                                                     FreeRDP_PreconnectionId };

	if (!winpr_Digest_Init(digest, WINPR_MD_SHA256))
		goto out;
	for (size_t x = 0; x < ARRAYSIZE(strings); x++)
	{
		const char* str = freerdp_settings_get_string(settings, strings[x]);
		if (str)
		{
			if (!winpr_Digest_Update(digest, str, strlen(str)))
				goto out;
		}
	}
	for (size_t x = 0; x < ARRAYSIZE(integers); x++)
	{
		const UINT32 val = freerdp_settings_get_uint32(settings, integers[x]);
		if (!winpr_Digest_Update(digest, &val, sizeof(val)))
			goto out;
	}

	const UINT32 loadBalanceLength =
	    freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
	const void* loadBalance = freerdp_settings_get_pointer(settings, FreeRDP_LoadBalanceInfo);
	if (loadBalanceLength > 0)
	{
		if (!winpr_Digest_Update(digest, loadBalance, loadBalanceLength))
			goto out;
	}

	rc = winpr_Digest_Final(digest, hash, len);

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
static BOOL rail_ipc_build_names(rdpClientRailIpc* ipc, const rdpSettings* settings)
{
	BYTE hash[WINPR_SHA256_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
	char hex[WINPR_SHA256_DIGEST_LENGTH * 2U + 1U] = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(ipc);
	WINPR_ASSERT(settings);

	if (!rail_ipc_build_hash(settings, hash, sizeof(hash)) ||
	    (winpr_BinToHexStringBuffer(hash, sizeof(hash), hex, sizeof(hex), FALSE) !=
	     sizeof(hash) * 2U))
		return FALSE;

	ipc->path = GetKnownSubPathV(KNOWN_PATH_XDG_RUNTIME_DIR, RAIL_IPC_NAME_PREFIX "%s.fifo", hex);
	return ipc->path != nullptr;
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
	ipc->writeFd = open(ipc->path, O_WRONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
	if (ipc->writeFd < 0)
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

static void rail_ipc_disable(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);
	if (ipc->readEvent)
	{
		(void)CloseHandle(ipc->readEvent);
		ipc->readEvent = nullptr;
	}
	if (ipc->writeFd >= 0)
	{
		(void)close(ipc->writeFd);
		ipc->writeFd = -1;
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

static BOOL rail_ipc_on_event_int(rdpContext* context, void* userdata)
{
	rdpClientRailIpc* ipc = userdata;
	WINPR_ASSERT(context);
	WINPR_ASSERT(ipc);

	if (freerdp_get_state(context) != CONNECTION_STATE_ACTIVE)
		return TRUE;
	WINPR_ASSERT(ipc->rail);
	WINPR_ASSERT(ipc->rail->ClientExecute);

	const size_t RAIL_DECODER_BLOCK = 1024;
	const size_t RAIL_DECODER_MAX = RAIL_DECODER_BLOCK * 32;

	if (!Stream_EnsureRemainingCapacity(ipc->buffer, RAIL_DECODER_BLOCK))
		return FALSE;
	const ssize_t count =
	    read(ipc->readFd, Stream_Pointer(ipc->buffer), Stream_GetRemainingCapacity(ipc->buffer));
	if (count <= 0)
	{
		if (errno == EAGAIN)
			return TRUE;
		if ((count < 0) && ((errno == EWOULDBLOCK) || (errno == EINTR)))
			return TRUE;
		if (count == 0)
			WLog_Print(ipc->log, WLOG_ERROR, "RAIL IPC disabled after unexpected FIFO EOF");
		else
			rail_ipc_log_errno(ipc, WLOG_ERROR, "reading the launch FIFO", errno);
		return TRUE;
	}

	if (!Stream_SafeSeek(ipc->buffer, WINPR_ASSERTING_INT_CAST(size_t, count)))
		return FALSE;

	const size_t pos = Stream_GetPosition(ipc->buffer);
	if (pos > RAIL_DECODER_MAX)
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC read %" PRIuz " bytes, but no valid data found. Discarding", pos);

		Stream_ResetPosition(ipc->buffer);
		return TRUE;
	}

	WINPR_JSON* input = WINPR_JSON_ParseWithLength(Stream_BufferAs(ipc->buffer, char), pos);
	if (input)
	{
		RAIL_EXEC_ORDER exec = WINPR_C_ARRAY_INIT;
		const INT64 val =
		    WINPR_JSON_GetIntegerValue(WINPR_JSON_GetObjectItemCaseSensitive(input, "flags"));
		if ((val >= 0) && (val <= UINT16_MAX))
			exec.flags = (uint16_t)val;
		else
			WLog_Print(ipc->log, WLOG_WARN,
			           "JSON: flags: %" PRId64 " out of range, expected in range[0,65535]", val);
		exec.RemoteApplicationProgram =
		    WINPR_JSON_GetStringValue(WINPR_JSON_GetObjectItemCaseSensitive(input, "program"));
		exec.RemoteApplicationWorkingDir =
		    WINPR_JSON_GetStringValue(WINPR_JSON_GetObjectItemCaseSensitive(input, "workingdir"));
		exec.RemoteApplicationArguments =
		    WINPR_JSON_GetStringValue(WINPR_JSON_GetObjectItemCaseSensitive(input, "arguments"));
		if (ipc->rail->ClientExecute)
		{
			WLog_Print(ipc->log, WLOG_DEBUG, "execute %s", exec.RemoteApplicationProgram);
			if (ipc && ipc->rail)
			{
				const UINT rc = ipc->rail->ClientExecute(ipc->rail, &exec);
				(void)WINPR_JSON_AddUIntegerToObject(input, "ClientExecuteReturn", rc);
				char* str = WINPR_JSON_PrintUnformatted(input);
				if (rc != 0)
					WLog_Print(ipc->log, WLOG_WARN, "Failed to execute %s", str);

				if (str)
				{
					// TODO: write(ipc->writeFd, str, strlen(str));
					free(str);
				}
			}
		}
		WINPR_JSON_Delete(input);
	}
	Stream_ResetPosition(ipc->buffer);

	return TRUE;
}

static BOOL rail_ipc_on_event(rdpContext* context, void* userdata)
{
	rdpClientRailIpc* ipc = userdata;
	WINPR_ASSERT(context);
	WINPR_ASSERT(ipc);

	EnterCriticalSection(&ipc->lock);
	const BOOL rc = rail_ipc_on_event_int(context, userdata);
	LeaveCriticalSection(&ipc->lock);
	return rc;
}

WINPR_ATTR_MALLOC(free, 1)
static char* exec_result_to_str(const RAIL_EXEC_RESULT_ORDER* execResult, size_t* plen)
{
	if (plen)
		*plen = 0;

	WINPR_JSON* json = WINPR_JSON_CreateObject();
	if (!json)
		return nullptr;

	if (!WINPR_JSON_AddIntegerToObject(json, "flags", execResult->flags))
		goto fail;
	if (!WINPR_JSON_AddIntegerToObject(json, "execResult", execResult->execResult))
		goto fail;
	if (!WINPR_JSON_AddIntegerToObject(json, "rawResult", execResult->rawResult))
		goto fail;

	char* name = ConvertWCharNToUtf8Alloc(execResult->exeOrFile.string,
	                                      execResult->exeOrFile.length, nullptr);
	if (name)
	{
		const void* rc = WINPR_JSON_AddStringToObject(json, "exeOrFile", name);
		free(name);
		if (!rc)
			goto fail;
	}

fail:
	char* str = WINPR_JSON_PrintUnformatted(json);
	if (plen)
		*plen = strlen(str);
	WINPR_JSON_Delete(json);
	return str;
}

static UINT rail_ipc_server_execute_result(RailClientContext* context,
                                           const RAIL_EXEC_RESULT_ORDER* execResult)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(execResult);

	rdpClientContext* cctx = context->custom;
	WINPR_ASSERT(cctx);

	size_t len = 0;
	char* str = exec_result_to_str(execResult, &len);
	// TODO: write(cctx->railIpc->writeFd, str, len);
	winpr_znfree(str, len);
	// Call client code
	cctx->railIpc->rail->custom = cctx->railIpc->client.context;
	const UINT rc = IFCALLRESULT(CHANNEL_RC_OK, cctx->railIpc->client.ServerExecuteResult,
	                             cctx->railIpc->rail, execResult);
	cctx->railIpc->rail->custom = cctx;
	return rc;
}

static BOOL rail_ipc_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                       const MONITORED_DESKTOP_ORDER* monitored_desktop)
{
	WINPR_ASSERT(context);

	rdpClientContext* cctx = (rdpClientContext*)context;

	rdpClientRailIpc* ipc = cctx->railIpc;
	WINPR_ASSERT(ipc);
	EnterCriticalSection(&ipc->lock);
	const BOOL rc = rail_ipc_start(ipc);
	LeaveCriticalSection(&ipc->lock);
	if (!rc)
		return rc;
	return IFCALLRESULT(TRUE, cctx->railIpc->client.MonitoredDesktop, context, orderInfo,
	                    monitored_desktop);
}

static BOOL rail_ipc_non_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	WINPR_ASSERT(context);

	rdpClientContext* cctx = (rdpClientContext*)context;
	return IFCALLRESULT(TRUE, cctx->railIpc->client.NonMonitoredDesktop, context, orderInfo);
}

static void rail_ipc_attach_int(rdpClientRailIpc* ipc, RailClientContext* rail,
                                rdpClientContext* cctx)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(rail);
	WINPR_ASSERT(cctx);

	const rdpSettings* settings = cctx->context.settings;
	if (!freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationExecFromPipe))
		return;

	// Back up client context and callbacks
	WINPR_ASSERT(!ipc->client.saved);
	ipc->client.saved = TRUE;
	ipc->client.context = rail->custom;
	ipc->client.ServerExecuteResult = rail->ServerExecuteResult;
	ipc->client.MonitoredDesktop = cctx->context.update->window->MonitoredDesktop;
	ipc->client.NonMonitoredDesktop = cctx->context.update->window->NonMonitoredDesktop;
	ipc->rail = rail;

	// Register our own wrappers
	rail->custom = cctx;
	rail->ServerExecuteResult = rail_ipc_server_execute_result;
	ipc->cctx = cctx;
	ipc->log = cctx->context.log;
	cctx->context.update->window->MonitoredDesktop = rail_ipc_monitored_desktop;
	cctx->context.update->window->NonMonitoredDesktop = rail_ipc_non_monitored_desktop;
}

void rail_ipc_attach(rdpClientRailIpc* ipc, RailClientContext* rail, rdpClientContext* cctx)
{
	WINPR_ASSERT(ipc);
	EnterCriticalSection(&ipc->lock);
	rail_ipc_attach_int(ipc, rail, cctx);
	LeaveCriticalSection(&ipc->lock);
}

static void rail_ipc_detach_int(rdpClientRailIpc* ipc, RailClientContext* rail,
                                rdpClientContext* cctx)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(rail);
	WINPR_ASSERT(cctx);

	const rdpSettings* settings = cctx->context.settings;
	if (!freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationExecFromPipe))
		return;

	rail_ipc_stop(ipc);

	// restore client context and callbacks
	if (ipc->client.saved)
	{
		rail->custom = ipc->client.context;
		rail->ServerExecuteResult = ipc->client.ServerExecuteResult;
		cctx->context.update->window->MonitoredDesktop = ipc->client.MonitoredDesktop;
		cctx->context.update->window->NonMonitoredDesktop = ipc->client.NonMonitoredDesktop;
	}

	const struct rdp_client_rail_callbacks empty = WINPR_C_ARRAY_INIT;
	ipc->client = empty;
	ipc->cctx = nullptr;
	ipc->log = nullptr;
}

void rail_ipc_detach(rdpClientRailIpc* ipc, RailClientContext* rail, rdpClientContext* cctx)
{
	WINPR_ASSERT(ipc);
	EnterCriticalSection(&ipc->lock);
	rail_ipc_detach_int(ipc, rail, cctx);
	LeaveCriticalSection(&ipc->lock);
}

BOOL rail_ipc_start(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);
	if (ipc->fifoCreated)
		return TRUE;

	if (!rail_ipc_build_names(ipc, ipc->cctx->context.settings) || !rail_ipc_open_fifo(ipc))
		goto fail;

	if (!freerdp_client_channel_register(ipc->cctx->context.channels, ipc->readEvent,
	                                     rail_ipc_on_event, ipc))
		goto fail;

	WLog_Print(ipc->log, WLOG_INFO, "RAIL launch FIFO available at %s", ipc->path);
	return TRUE;
fail:
	rail_ipc_stop(ipc);
	return FALSE;
}

void rail_ipc_stop(rdpClientRailIpc* ipc)
{
	if (!ipc)
		return;

	freerdp_client_channel_unregister(ipc->cctx->context.channels, ipc->readEvent);

	rail_ipc_disable(ipc);

	free(ipc->path);
	ipc->path = nullptr;
}

#else

void rail_ipc_attach(WINPR_ATTR_UNUSED rdpClientRailIpc* ipc,
                     WINPR_ATTR_UNUSED RailClientContext* rail,
                     WINPR_ATTR_UNUSED rdpClientContext* cctx)
{
}

void rail_ipc_detach(WINPR_ATTR_UNUSED rdpClientRailIpc* ipc,
                     WINPR_ATTR_UNUSED RailClientContext* rail,
                     WINPR_ATTR_UNUSED rdpClientContext* cctx)
{
}

#endif

rdpClientRailIpc* rail_ipc_new(void)
{
	rdpClientRailIpc* ipc = calloc(1, sizeof(rdpClientRailIpc));
	if (!ipc)
		return nullptr;
	ipc->buffer = Stream_New(nullptr, 1024);
	if (!ipc->buffer)
		goto fail;

	InitializeCriticalSection(&ipc->lock);
	ipc->readFd = -1;
	ipc->writeFd = -1;
	return ipc;

fail:
	rail_ipc_free(ipc);
	return nullptr;
}

void rail_ipc_free(rdpClientRailIpc* ipc)
{
	if (!ipc)
		return;
	DeleteCriticalSection(&ipc->lock);
	Stream_Free(ipc->buffer, TRUE);
	free(ipc);
}

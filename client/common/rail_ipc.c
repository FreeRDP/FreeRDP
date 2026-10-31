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
#include <freerdp/channels/channels.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/custom-crypto.h>
#include <winpr/debug.h>
#include <winpr/environment.h>
#include <winpr/print.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include "rail_ipc.h"
#include "rail_decoder.h"

#define RAIL_IPC_HASH_VERSION 2U
#define RAIL_IPC_NAME_PREFIX "freerdp-rail-v1-"
#define RAIL_IPC_NAME_CAPACITY 96U
#define RAIL_IPC_MAX_RECORDS_PER_PASS 16U
#define RAIL_IPC_READ_BUFFER_SIZE RAIL_IPC_MAX_RECORDS_PER_PASS
static const size_t RAIL_IPC_MAX_BYTES_PER_PASS = WINPR_CXX_COMPAT_CAST(size_t, 64U) * 1024U;

struct rdp_client_rail_ipc
{
	BOOL intent;
	RailClientContext* rail;
	BOOL registered;
	rdpChannels* registeredOn;
	wLog* log;
	DWORD ownerThreadId;
	char* path;
	int runtimeDirectoryFd;
	int lockFd;
	int readFd;
	int heldWriteFd;
	HANDLE readEvent;
	char fifoName[RAIL_IPC_NAME_CAPACITY];
	char lockName[RAIL_IPC_NAME_CAPACITY];
	dev_t fifoDevice;
	ino_t fifoInode;
	uid_t fifoOwner;
	BOOL fifoIdentityValid;
	RailDecoder decoder;
};

static void rail_ipc_write_uint32_be(BYTE data[4], UINT32 value)
{
	data[0] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 24U) & UINT32_C(0xFF));
	data[1] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 16U) & UINT32_C(0xFF));
	data[2] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 8U) & UINT32_C(0xFF));
	data[3] = WINPR_ASSERTING_INT_CAST(BYTE, value & UINT32_C(0xFF));
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_field(WINPR_DIGEST_CTX* digest, const void* data, size_t length)
{
	BYTE encodedLength[4] = WINPR_C_ARRAY_INIT;

	if (length > UINT32_MAX)
		return FALSE;

	rail_ipc_write_uint32_be(encodedLength, WINPR_ASSERTING_INT_CAST(UINT32, length));
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
static BOOL rail_ipc_digest_ascii_ci_n(WINPR_DIGEST_CTX* digest, const char* value, size_t length,
                                       BOOL stripTrailingDot)
{
	char* canonical = nullptr;
	BOOL rc = FALSE;

	if (!value)
		return rail_ipc_digest_field(digest, nullptr, 0);
	while (stripTrailingDot && (length > 0) && (value[length - 1U] == '.'))
		length--;
	canonical = strndup(value, length);
	if (!canonical)
		return FALSE;
	for (size_t x = 0; x < length; x++)
	{
		if ((canonical[x] >= 'A') && (canonical[x] <= 'Z'))
			canonical[x] = (char)(canonical[x] + ('a' - 'A'));
	}
	rc = rail_ipc_digest_field(digest, canonical, length);
	free(canonical);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_ascii_ci(WINPR_DIGEST_CTX* digest, const char* value,
                                     BOOL stripTrailingDot)
{
	return rail_ipc_digest_ascii_ci_n(digest, value, value ? strlen(value) : 0, stripTrailingDot);
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_uint32(WINPR_DIGEST_CTX* digest, UINT32 value)
{
	BYTE encoded[4] = WINPR_C_ARRAY_INIT;
	rail_ipc_write_uint32_be(encoded, value);
	return rail_ipc_digest_field(digest, encoded, sizeof(encoded));
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_bool(WINPR_DIGEST_CTX* digest, BOOL value)
{
	const BYTE encoded = value ? 1 : 0;
	return rail_ipc_digest_field(digest, &encoded, sizeof(encoded));
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_digest_gateway_url(WINPR_DIGEST_CTX* digest, const char* url)
{
	const char* host = nullptr;
	const char* cursor = nullptr;
	const char* path = nullptr;
	UINT32 port = 443;

	if (!url)
		return rail_ipc_digest_bool(digest, FALSE) && rail_ipc_digest_string(digest, nullptr);
	if (strncmp(url, "wss://", 6) == 0)
		host = url + 6;
	else if (strncmp(url, "https://", 8) == 0)
		host = url + 8;
	else
		goto invalid;

	cursor = host;
	while ((*cursor != '\0') && (*cursor != ':') && (*cursor != '/'))
		cursor++;
	if (cursor == host)
		goto invalid;
	const size_t hostLength = WINPR_ASSERTING_INT_CAST(size_t, cursor - host);
	if (*cursor == ':')
	{
		char encodedPort[6] = WINPR_C_ARRAY_INIT;
		char* end = nullptr;
		const char* portStart = ++cursor;
		while ((*cursor != '\0') && (*cursor != '/'))
			cursor++;
		const size_t portLength = WINPR_ASSERTING_INT_CAST(size_t, cursor - portStart);
		if ((portLength == 0) || (portLength >= sizeof(encodedPort)))
			goto invalid;
		CopyMemory(encodedPort, portStart, portLength);
		const unsigned long parsed = strtoul(encodedPort, &end, 10);
		if (!end || (*end != '\0') || (parsed == 0) || (parsed > UINT16_MAX))
			goto invalid;
		port = WINPR_ASSERTING_INT_CAST(UINT32, parsed);
	}
	path = cursor;
	return rail_ipc_digest_bool(digest, TRUE) &&
	       rail_ipc_digest_ascii_ci_n(digest, host, hostLength, TRUE) &&
	       rail_ipc_digest_uint32(digest, port) && rail_ipc_digest_string(digest, path);

invalid:
	return rail_ipc_digest_bool(digest, FALSE) && rail_ipc_digest_string(digest, url);
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
static BOOL rail_ipc_build_hash(const rdpSettings* settings, BYTE hash[WINPR_SHA256_DIGEST_LENGTH])
{
	BOOL rc = FALSE;
	const BYTE version = RAIL_IPC_HASH_VERSION;
	WINPR_DIGEST_CTX* digest = nullptr;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(hash);

	digest = winpr_Digest_New();
	if (!digest)
		return FALSE;

	const BOOL gatewayEnabled = freerdp_settings_get_bool(settings, FreeRDP_GatewayEnabled);
	const BOOL armGateway =
	    gatewayEnabled && freerdp_settings_get_bool(settings, FreeRDP_GatewayArmTransport);
	const BOOL sendPreconnection =
	    freerdp_settings_get_bool(settings, FreeRDP_SendPreconnectionPdu);
	const UINT32 loadBalanceLength =
	    freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
	const void* loadBalance = freerdp_settings_get_pointer(settings, FreeRDP_LoadBalanceInfo);
	const ARC_SC_PRIVATE_PACKET* reconnectCookie =
	    freerdp_settings_get_pointer(settings, FreeRDP_ServerAutoReconnectCookie);

	if (!winpr_Digest_Init(digest, WINPR_MD_SHA256) ||
	    !winpr_Digest_Update(digest, &version, sizeof(version)) ||
	    !rail_ipc_digest_ascii_ci(
	        digest, freerdp_settings_get_string(settings, FreeRDP_ServerHostname), TRUE) ||
	    !rail_ipc_digest_uint32(digest,
	                            freerdp_settings_get_uint32(settings, FreeRDP_ServerPort)) ||
	    !rail_ipc_digest_string(digest, freerdp_settings_get_string(settings, FreeRDP_Username)) ||
	    !rail_ipc_digest_string(digest, freerdp_settings_get_string(settings, FreeRDP_Domain)) ||
	    !rail_ipc_digest_bool(digest,
	                          freerdp_settings_get_bool(settings, FreeRDP_ConsoleSession)) ||
	    !rail_ipc_digest_uint32(
	        digest, freerdp_settings_get_uint32(settings, FreeRDP_RedirectedSessionId)) ||
	    !rail_ipc_digest_bool(digest, freerdp_settings_get_bool(settings, FreeRDP_VmConnectMode)) ||
	    !rail_ipc_digest_bool(digest,
	                          freerdp_settings_get_bool(settings, FreeRDP_ConnectChildSession)) ||
	    !rail_ipc_digest_bool(digest, sendPreconnection) ||
	    (sendPreconnection &&
	     (!rail_ipc_digest_uint32(digest,
	                              freerdp_settings_get_uint32(settings, FreeRDP_PreconnectionId)) ||
	      !rail_ipc_digest_string(
	          digest, freerdp_settings_get_string(settings, FreeRDP_PreconnectionBlob)))) ||
	    ((loadBalanceLength > 0) && !loadBalance) ||
	    !rail_ipc_digest_field(digest, loadBalance, loadBalanceLength) ||
	    !rail_ipc_digest_reconnect_cookie(digest, reconnectCookie) ||
	    !rail_ipc_digest_bool(digest, gatewayEnabled) ||
	    (gatewayEnabled &&
	     (!rail_ipc_digest_uint32(digest, freerdp_get_gateway_usage_method(settings)) ||
	      !rail_ipc_digest_ascii_ci(
	          digest, freerdp_settings_get_string(settings, FreeRDP_GatewayHostname), TRUE) ||
	      !rail_ipc_digest_uint32(digest,
	                              freerdp_settings_get_uint32(settings, FreeRDP_GatewayPort)) ||
	      !rail_ipc_digest_string(digest,
	                              freerdp_settings_get_string(settings, FreeRDP_GatewayUsername)) ||
	      !rail_ipc_digest_string(digest,
	                              freerdp_settings_get_string(settings, FreeRDP_GatewayDomain)) ||
	      !rail_ipc_digest_gateway_url(digest,
	                                   freerdp_settings_get_string(settings, FreeRDP_GatewayUrl)) ||
	      !rail_ipc_digest_bool(digest, armGateway) ||
	      (armGateway &&
	       (!rail_ipc_digest_string(digest, freerdp_settings_get_string(
	                                            settings, FreeRDP_GatewayAzureActiveDirectory)) ||
	        !rail_ipc_digest_bool(
	            digest, freerdp_settings_get_bool(settings, FreeRDP_GatewayAvdUseTenantid)) ||
	        (freerdp_settings_get_bool(settings, FreeRDP_GatewayAvdUseTenantid) &&
	         !rail_ipc_digest_string(
	             digest, freerdp_settings_get_string(settings, FreeRDP_GatewayAvdAadtenantid))) ||
	        !rail_ipc_digest_string(digest, freerdp_settings_get_string(
	                                            settings, FreeRDP_RemoteApplicationProgram)))))))
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
static BOOL rail_ipc_same_identity(const struct stat* left, const struct stat* right)
{
	WINPR_ASSERT(left);
	WINPR_ASSERT(right);
	return (left->st_dev == right->st_dev) && (left->st_ino == right->st_ino) &&
	       ((left->st_mode & S_IFMT) == (right->st_mode & S_IFMT)) &&
	       (left->st_uid == right->st_uid);
}

WINPR_ATTR_NODISCARD
static char* rail_ipc_get_runtime_directory(void)
{
	const DWORD required = GetEnvironmentVariableA("XDG_RUNTIME_DIR", nullptr, 0);
	if (required == 0)
		return nullptr;

	char* directory = calloc(required, 1);
	if (!directory)
		return nullptr;
	if (GetEnvironmentVariableA("XDG_RUNTIME_DIR", directory, required) != required - 1U)
	{
		free(directory);
		return nullptr;
	}

	size_t length = strlen(directory);
	while ((length > 1U) && (directory[length - 1U] == '/'))
		directory[--length] = '\0';
	return directory;
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

	const int fifoLength =
	    _snprintf(ipc->fifoName, sizeof(ipc->fifoName), RAIL_IPC_NAME_PREFIX "%s.fifo", hex);
	const int lockLength =
	    _snprintf(ipc->lockName, sizeof(ipc->lockName), RAIL_IPC_NAME_PREFIX "%s.lock", hex);
	if ((fifoLength <= 0) || ((size_t)fifoLength >= sizeof(ipc->fifoName)) || (lockLength <= 0) ||
	    ((size_t)lockLength >= sizeof(ipc->lockName)))
		return FALSE;

	const size_t directoryLength = strlen(runtimeDirectory);
	const size_t nameLength = strlen(ipc->fifoName);
	if (directoryLength > SIZE_MAX - nameLength - 2U)
		return FALSE;
	ipc->path = calloc(directoryLength + nameLength + 2U, 1);
	if (!ipc->path)
		return FALSE;

	const int pathLength = _snprintf(ipc->path, directoryLength + nameLength + 2U, "%s/%s",
	                                 runtimeDirectory, ipc->fifoName);
	return (pathLength > 0) && ((size_t)pathLength == directoryLength + nameLength + 1U);
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_open_runtime_directory(rdpClientRailIpc* ipc, const char* runtimeDirectory)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(runtimeDirectory);

	ipc->runtimeDirectoryFd =
	    open(runtimeDirectory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (ipc->runtimeDirectoryFd < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "opening XDG_RUNTIME_DIR", errno);
		return FALSE;
	}

	struct stat attributes = WINPR_C_ARRAY_INIT;
	if (fstat(ipc->runtimeDirectoryFd, &attributes) < 0)
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

	BOOL created = FALSE;
	ipc->lockFd = openat(ipc->runtimeDirectoryFd, ipc->lockName,
	                     O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
	if (ipc->lockFd >= 0)
		created = TRUE;
	else if (errno == EEXIST)
		ipc->lockFd =
		    openat(ipc->runtimeDirectoryFd, ipc->lockName, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
	if (ipc->lockFd < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "opening the ownership lock", errno);
		return FALSE;
	}

	if (created && (fchmod(ipc->lockFd, S_IRUSR | S_IWUSR) < 0))
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "setting ownership lock permissions", errno);
		return FALSE;
	}

	struct stat descriptor = WINPR_C_ARRAY_INIT;
	struct stat path = WINPR_C_ARRAY_INIT;
	if ((fstat(ipc->lockFd, &descriptor) < 0) ||
	    (fstatat(ipc->runtimeDirectoryFd, ipc->lockName, &path, AT_SYMLINK_NOFOLLOW) < 0))
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "checking the ownership lock", errno);
		return FALSE;
	}
	if (!S_ISREG(descriptor.st_mode) || (descriptor.st_uid != getuid()) ||
	    ((descriptor.st_mode & (S_IRWXG | S_IRWXO)) != 0) ||
	    ((descriptor.st_mode & (S_IRWXU)) != (S_IRUSR | S_IWUSR)) ||
	    !rail_ipc_same_identity(&descriptor, &path))
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC rejected the ownership lock with unsafe type, owner, permissions, or "
		           "identity");
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
static BOOL rail_ipc_validate_fifo(const rdpClientRailIpc* ipc, const struct stat* attributes)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(attributes);
	WINPR_UNUSED(ipc);
	return S_ISFIFO(attributes->st_mode) && (attributes->st_uid == getuid()) &&
	       ((attributes->st_mode & (S_IRWXG | S_IRWXO)) == 0) &&
	       ((attributes->st_mode & S_IRWXU) == (S_IRUSR | S_IWUSR));
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_query_pipe_buffer(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(ipc->readFd >= 0);

	size_t pipeBuffer = 0;
	errno = 0;
	const long value = fpathconf(ipc->readFd, _PC_PIPE_BUF);
	if (value > 0)
	{
		if ((UINT64)value > SIZE_MAX - 1U)
			return FALSE;
		pipeBuffer = (size_t)value;
	}
	else if ((value < 0) && (errno == 0))
	{
#if defined(PIPE_BUF)
		pipeBuffer = PIPE_BUF;
#elif defined(_POSIX_PIPE_BUF)
		pipeBuffer = _POSIX_PIPE_BUF;
#else
		pipeBuffer = 512U;
#endif
	}
	else
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "querying the launch FIFO atomic-write limit", errno);
		return FALSE;
	}

	return rail_decoder_init(&ipc->decoder, pipeBuffer);
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_open_fifo(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);

	BOOL created = FALSE;
	if (mkfifoat(ipc->runtimeDirectoryFd, ipc->fifoName, S_IRUSR | S_IWUSR) == 0)
		created = TRUE;
	else if (errno != EEXIST)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "creating the launch FIFO", errno);
		return FALSE;
	}

	if (created && (fchmodat(ipc->runtimeDirectoryFd, ipc->fifoName, S_IRUSR | S_IWUSR, 0) < 0))
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "setting launch FIFO permissions", errno);
		return FALSE;
	}

	struct stat before = WINPR_C_ARRAY_INIT;
	if (fstatat(ipc->runtimeDirectoryFd, ipc->fifoName, &before, AT_SYMLINK_NOFOLLOW) < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "checking the launch FIFO", errno);
		return FALSE;
	}
	if (!rail_ipc_validate_fifo(ipc, &before))
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC rejected the launch FIFO with unsafe type, owner, or permissions");
		return FALSE;
	}

	ipc->fifoDevice = before.st_dev;
	ipc->fifoInode = before.st_ino;
	ipc->fifoOwner = before.st_uid;
	ipc->fifoIdentityValid = TRUE;

	ipc->readFd = openat(ipc->runtimeDirectoryFd, ipc->fifoName,
	                     O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
	if (ipc->readFd < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "opening the launch FIFO for reading", errno);
		return FALSE;
	}

	struct stat readAttributes = WINPR_C_ARRAY_INIT;
	if ((fstat(ipc->readFd, &readAttributes) < 0) ||
	    !rail_ipc_validate_fifo(ipc, &readAttributes) ||
	    !rail_ipc_same_identity(&before, &readAttributes))
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC launch FIFO changed while it was being opened for reading");
		return FALSE;
	}

	ipc->heldWriteFd = openat(ipc->runtimeDirectoryFd, ipc->fifoName,
	                          O_WRONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
	if (ipc->heldWriteFd < 0)
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "opening the held launch FIFO writer", errno);
		return FALSE;
	}

	struct stat writeAttributes = WINPR_C_ARRAY_INIT;
	struct stat after = WINPR_C_ARRAY_INIT;
	if ((fstat(ipc->heldWriteFd, &writeAttributes) < 0) ||
	    (fstatat(ipc->runtimeDirectoryFd, ipc->fifoName, &after, AT_SYMLINK_NOFOLLOW) < 0) ||
	    !rail_ipc_validate_fifo(ipc, &writeAttributes) || !rail_ipc_validate_fifo(ipc, &after) ||
	    !rail_ipc_same_identity(&before, &writeAttributes) ||
	    !rail_ipc_same_identity(&before, &after))
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC launch FIFO changed while its descriptors were being opened");
		return FALSE;
	}

	ipc->readEvent = CreateFileDescriptorEvent(nullptr, FALSE, FALSE, ipc->readFd, WINPR_FD_READ);
	if (!ipc->readEvent)
	{
		WLog_Print(ipc->log, WLOG_ERROR, "RAIL IPC could not create the launch FIFO event");
		return FALSE;
	}
	if (!rail_ipc_query_pipe_buffer(ipc))
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC could not establish the launch FIFO atomic-write limit");
		return FALSE;
	}

	return TRUE;
}

static void rail_ipc_remove_owned_fifo(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);
	if (!ipc->fifoIdentityValid || (ipc->runtimeDirectoryFd < 0))
		return;

	struct stat current = WINPR_C_ARRAY_INIT;
	if (fstatat(ipc->runtimeDirectoryFd, ipc->fifoName, &current, AT_SYMLINK_NOFOLLOW) < 0)
	{
		if (errno != ENOENT)
			rail_ipc_log_errno(ipc, WLOG_WARN, "checking the launch FIFO during cleanup", errno);
		return;
	}
	if (!S_ISFIFO(current.st_mode) || (current.st_uid != ipc->fifoOwner) ||
	    (current.st_dev != ipc->fifoDevice) || (current.st_ino != ipc->fifoInode))
	{
		WLog_Print(ipc->log, WLOG_WARN,
		           "RAIL IPC left the launch FIFO in place because its identity changed");
		return;
	}
	if (unlinkat(ipc->runtimeDirectoryFd, ipc->fifoName, 0) < 0)
		rail_ipc_log_errno(ipc, WLOG_WARN, "removing the launch FIFO", errno);
}

static void rail_ipc_unregister(rdpClientRailIpc* ipc)
{
	WINPR_ASSERT(ipc);
	if (!ipc->registered)
		return;
	(void)freerdp_client_channel_unregister(ipc->registeredOn, ipc->readEvent);
	ipc->registered = FALSE;
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
	rail_ipc_remove_owned_fifo(ipc);
	ipc->fifoIdentityValid = FALSE;
}

static BOOL rail_ipc_on_event(rdpContext* context, void* userdata)
{
	rdpClientRailIpc* ipc = userdata;
	WINPR_ASSERT(context);
	WINPR_ASSERT(ipc);
	if (ipc->lockFd < 0)
		return TRUE;
	WINPR_ASSERT(ipc->ownerThreadId == GetCurrentThreadId());
	if (!ipc->readEvent)
		return TRUE;
	const DWORD waitStatus = WaitForSingleObject(ipc->readEvent, 0);
	if (waitStatus == WAIT_TIMEOUT)
		return TRUE;
	if (waitStatus != WAIT_OBJECT_0)
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC disabled after its launch FIFO event failed with status 0x%08" PRIx32,
		           waitStatus);
		rail_ipc_disable(ipc);
		return TRUE;
	}
	if (freerdp_get_state(context) != CONNECTION_STATE_ACTIVE)
		return TRUE;
	if (!ipc->rail || !ipc->rail->ClientExecute)
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC disabled because the attached channel cannot execute launches");
		rail_ipc_disable(ipc);
		return TRUE;
	}

	size_t bytes = 0;
	size_t completedRecords = 0;
	while ((bytes < RAIL_IPC_MAX_BYTES_PER_PASS) &&
	       (completedRecords < RAIL_IPC_MAX_RECORDS_PER_PASS))
	{
		/* Lifecycle transitions share this thread. Recheck before every read so a transition
		 * observed between the wake and this call wins without draining the FIFO. */
		if (!ipc->readEvent || !ipc->rail || !ipc->registered)
			break;

		BYTE buffer[RAIL_IPC_READ_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
		const size_t completionBudget = RAIL_IPC_MAX_RECORDS_PER_PASS - completedRecords;
		const size_t byteBudget = RAIL_IPC_MAX_BYTES_PER_PASS - bytes;
		/* Every byte can terminate at most one record. A FIFO cannot return unread bytes, so
		 * limit read-ahead to the remaining completion budget. This enforces the record cap
		 * without retaining complete records in a user-space queue. */
		const size_t requested = MIN(ARRAYSIZE(buffer), MIN(completionBudget, byteBudget));
		ssize_t readBytes = 0;
		do
		{
			readBytes = read(ipc->readFd, buffer, requested);
		} while ((readBytes < 0) && (errno == EINTR));
		if (readBytes > 0)
		{
			bytes += (size_t)readBytes;
			size_t offset = 0;
			while (offset < (size_t)readBytes)
			{
				size_t consumed = 0;
				RAIL_EXEC_ORDER order = WINPR_C_ARRAY_INIT;
				const char* reason = nullptr;
				const RailDecoderResult result =
				    rail_decoder_feed(&ipc->decoder, &buffer[offset], (size_t)readBytes - offset,
				                      &consumed, &order, &reason);
				offset += consumed;
				if (result == RAIL_DECODER_INCOMPLETE)
					continue;
				completedRecords++;
				if (result == RAIL_DECODER_REJECTED)
					WLog_Print(ipc->log, WLOG_WARN, "RAIL IPC rejected launch record: %s", reason);
				else
				{
					const UINT status = ipc->rail->ClientExecute(ipc->rail, &order);
					if (status != CHANNEL_RC_OK)
						WLog_Print(
						    ipc->log, WLOG_WARN,
						    "RAIL IPC ClientExecute rejected a launch locally with error %" PRIu32,
						    status);
				}
			}
			continue;
		}
		if (readBytes == 0)
		{
			WLog_Print(ipc->log, WLOG_ERROR,
			           "RAIL IPC disabled after an unexpected end of launch FIFO input");
			rail_ipc_disable(ipc);
			return TRUE;
		}
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			break;
		rail_ipc_log_errno(ipc, WLOG_ERROR, "reading the launch FIFO", errno);
		rail_ipc_disable(ipc);
		return TRUE;
	}
	return TRUE;
}

void rail_ipc_attach(rdpClientContext* cctx, RailClientContext* rail)
{
	WINPR_ASSERT(cctx);
	rdpClientRailIpc* ipc = cctx->railIpc;
	if (!ipc || (ipc->lockFd < 0))
		return;
	WINPR_ASSERT(ipc->ownerThreadId == GetCurrentThreadId());
	rail_ipc_unregister(ipc);
	ipc->rail = rail;
}

void rail_ipc_detach(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);
	rdpClientRailIpc* ipc = cctx->railIpc;
	if (!ipc || (ipc->lockFd < 0))
		return;
	WINPR_ASSERT(ipc->ownerThreadId == GetCurrentThreadId());
	rail_ipc_unregister(ipc);
	ipc->rail = nullptr;
}

BOOL rail_ipc_set_ready(rdpClientContext* cctx, BOOL ready)
{
	WINPR_ASSERT(cctx);
	rdpClientRailIpc* ipc = cctx->railIpc;
	if (!ipc || (ipc->lockFd < 0))
		return TRUE;
	WINPR_ASSERT(ipc->ownerThreadId == GetCurrentThreadId());
	if (!ipc->intent || !ipc->readEvent)
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
	if (ipc->registered)
		return TRUE;
	if (!freerdp_client_channel_register(cctx->context.channels, ipc->readEvent, rail_ipc_on_event,
	                                     ipc))
	{
		WLog_Print(ipc->log, WLOG_ERROR, "RAIL IPC could not register its launch FIFO event");
		rail_ipc_disable(ipc);
		return FALSE;
	}
	ipc->registered = TRUE;
	ipc->registeredOn = cctx->context.channels;
	return TRUE;
}

BOOL rail_ipc_enable(rdpClientContext* cctx, BOOL enable)
{
	WINPR_ASSERT(cctx);
	if (enable && !cctx->railIpc)
	{
		rdpClientRailIpc* ipc = calloc(1, sizeof(*ipc));
		if (!ipc)
			return FALSE;
		ipc->runtimeDirectoryFd = -1;
		ipc->lockFd = -1;
		ipc->readFd = -1;
		ipc->heldWriteFd = -1;
		cctx->railIpc = ipc;
	}
	if (cctx->railIpc)
		cctx->railIpc->intent = enable;
	return TRUE;
}

BOOL rail_ipc_start(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);
	rdpClientRailIpc* ipc = cctx->railIpc;
	const rdpSettings* settings = cctx->context.settings;
	if (!ipc || !ipc->intent ||
	    !freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationMode) ||
	    freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
		return TRUE;
	if (ipc->lockFd >= 0)
		return TRUE;
	ipc->log = cctx->context.log;
	ipc->ownerThreadId = GetCurrentThreadId();

	char* runtimeDirectory = rail_ipc_get_runtime_directory();
	if (!runtimeDirectory)
	{
		WLog_Print(ipc->log, WLOG_ERROR, "RAIL IPC requires XDG_RUNTIME_DIR");
		goto fail;
	}
	if (!rail_ipc_build_names(ipc, settings, runtimeDirectory) ||
	    !rail_ipc_open_runtime_directory(ipc, runtimeDirectory) || !rail_ipc_open_lock(ipc) ||
	    !rail_ipc_open_fifo(ipc))
	{
		free(runtimeDirectory);
		goto fail;
	}
	free(runtimeDirectory);
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
	rail_ipc_unregister(ipc);
	if (ipc->readEvent)
		(void)CloseHandle(ipc->readEvent);
	if (ipc->heldWriteFd >= 0)
		(void)close(ipc->heldWriteFd);
	if (ipc->readFd >= 0)
		(void)close(ipc->readFd);
	rail_ipc_remove_owned_fifo(ipc);
	/* Closing the lock descriptor releases ownership; the lock file stays. */
	if (ipc->lockFd >= 0)
		(void)close(ipc->lockFd);
	if (ipc->runtimeDirectoryFd >= 0)
		(void)close(ipc->runtimeDirectoryFd);
	rail_decoder_uninit(&ipc->decoder);
	free(ipc->path);
	*ipc = (rdpClientRailIpc){ .intent = ipc->intent,
		                       .runtimeDirectoryFd = -1,
		                       .lockFd = -1,
		                       .readFd = -1,
		                       .heldWriteFd = -1 };
}

void rail_ipc_free(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);
	rail_ipc_stop(cctx);
	free(cctx->railIpc);
	cctx->railIpc = nullptr;
}

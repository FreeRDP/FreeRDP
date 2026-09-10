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
#include <freerdp/client/rail_ipc.h>

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/custom-crypto.h>
#include <winpr/environment.h>
#include <winpr/print.h>
#include <winpr/string.h>
#include <winpr/thread.h>

#define RAIL_IPC_HASH_VERSION 2U
#define RAIL_IPC_NAME_PREFIX "freerdp-rail-v1-"
#define RAIL_IPC_NAME_CAPACITY 96U
#define RAIL_IPC_MAX_RECORDS_PER_PASS 16U
#define RAIL_IPC_READ_BUFFER_SIZE RAIL_IPC_MAX_RECORDS_PER_PASS
#define RAIL_IPC_MAX_BYTES_PER_PASS (64U * 1024U)
#define RAIL_IPC_KNOWN_EXEC_FLAGS                                                    \
	(TS_RAIL_EXEC_FLAG_EXPAND_WORKINGDIRECTORY | TS_RAIL_EXEC_FLAG_TRANSLATE_FILES | \
	 TS_RAIL_EXEC_FLAG_FILE | TS_RAIL_EXEC_FLAG_EXPAND_ARGUMENTS |                   \
	 TS_RAIL_EXEC_FLAG_APP_USER_MODEL_ID)

struct s_rail_client_ipc
{
	wLog* log;
	DWORD ownerThreadId;
	char* path;
	RailClientContext* rail;
	BOOL ready;
	BOOL enabled;
#if !defined(_WIN32)
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
	size_t pipeBuffer;
	char* record;
	size_t recordBytes;
	BOOL atLineStart;
	BOOL recordInvalid;
	const char* recordError;
#endif
};

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_is_owner_thread(const RailClientIpcContext* ipc)
{
	WINPR_ASSERT(ipc);
	return ipc->ownerThreadId == GetCurrentThreadId();
}

/* The component has no internal locking; every lifecycle entry point runs on its owner thread. */
static void rail_ipc_assert_owner_thread(const RailClientIpcContext* ipc)
{
	WINPR_ASSERT(rail_ipc_is_owner_thread(ipc));
}

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

#if !defined(_WIN32)

static void rail_ipc_log_errno(const RailClientIpcContext* ipc, DWORD level, const char* operation,
                               int error)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(operation);
	WLog_Print(ipc->log, level, "RAIL IPC %s failed: %s [%d]", operation, strerror(error), error);
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
static BOOL rail_ipc_build_names(RailClientIpcContext* ipc, const rdpSettings* settings,
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
static BOOL rail_ipc_open_runtime_directory(RailClientIpcContext* ipc, const char* runtimeDirectory)
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
static BOOL rail_ipc_open_lock(RailClientIpcContext* ipc)
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
static BOOL rail_ipc_validate_fifo(const RailClientIpcContext* ipc, const struct stat* attributes)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(attributes);
	return S_ISFIFO(attributes->st_mode) && (attributes->st_uid == getuid()) &&
	       ((attributes->st_mode & (S_IRWXG | S_IRWXO)) == 0) &&
	       ((attributes->st_mode & S_IRWXU) == (S_IRUSR | S_IWUSR));
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_query_pipe_buffer(RailClientIpcContext* ipc)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(ipc->readFd >= 0);

	errno = 0;
	const long value = fpathconf(ipc->readFd, _PC_PIPE_BUF);
	if (value > 0)
	{
		if ((UINT64)value > SIZE_MAX - 1U)
			return FALSE;
		ipc->pipeBuffer = (size_t)value;
	}
	else if ((value < 0) && (errno == 0))
	{
#if defined(PIPE_BUF)
		ipc->pipeBuffer = PIPE_BUF;
#elif defined(_POSIX_PIPE_BUF)
		ipc->pipeBuffer = _POSIX_PIPE_BUF;
#else
		ipc->pipeBuffer = 512U;
#endif
	}
	else
	{
		rail_ipc_log_errno(ipc, WLOG_ERROR, "querying the launch FIFO atomic-write limit", errno);
		return FALSE;
	}

	if ((ipc->pipeBuffer == 0) || (ipc->pipeBuffer == SIZE_MAX))
		return FALSE;
	ipc->record = calloc(ipc->pipeBuffer + 1U, 1);
	if (!ipc->record)
		return FALSE;
	ipc->atLineStart = TRUE;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_open_fifo(RailClientIpcContext* ipc)
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

	ipc->enabled = TRUE;
	return TRUE;
}

static void rail_ipc_remove_owned_fifo(RailClientIpcContext* ipc)
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

static void rail_ipc_disable(RailClientIpcContext* ipc)
{
	WINPR_ASSERT(ipc);
	rail_ipc_assert_owner_thread(ipc);

	ipc->ready = FALSE;
	ipc->enabled = FALSE;
	if (ipc->readEvent)
	{
		CloseHandle(ipc->readEvent);
		ipc->readEvent = nullptr;
	}
	rail_ipc_remove_owned_fifo(ipc);
	if (ipc->heldWriteFd >= 0)
	{
		close(ipc->heldWriteFd);
		ipc->heldWriteFd = -1;
	}
	if (ipc->readFd >= 0)
	{
		close(ipc->readFd);
		ipc->readFd = -1;
	}
	ipc->fifoIdentityValid = FALSE;
	free(ipc->record);
	ipc->record = nullptr;
	ipc->recordBytes = 0;
	ipc->atLineStart = TRUE;
	ipc->recordInvalid = FALSE;
	ipc->recordError = nullptr;
}

static void rail_ipc_reject_record(const RailClientIpcContext* ipc, const char* reason)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(reason);
	WLog_Print(ipc->log, WLOG_WARN, "RAIL IPC rejected launch record: %s", reason);
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_parse_flags(const char* value, UINT16* flags)
{
	WINPR_ASSERT(value);
	WINPR_ASSERT(flags);

	if (value[0] == '\0')
	{
		*flags = 0;
		return TRUE;
	}
	if ((value[0] < '0') || (value[0] > '9'))
		return FALSE;

	const BOOL hexadecimal = (value[0] == '0') && ((value[1] == 'x') || (value[1] == 'X'));
	const char* start = hexadecimal ? &value[2] : value;
	const int base = hexadecimal ? 16 : 10;
	if (hexadecimal &&
	    !(((start[0] >= '0') && (start[0] <= '9')) || ((start[0] >= 'a') && (start[0] <= 'f')) ||
	      ((start[0] >= 'A') && (start[0] <= 'F'))))
		return FALSE;

	errno = 0;
	char* end = nullptr;
	const unsigned long parsed = strtoul(start, &end, base);
	if ((errno != 0) || !end || (end == start) || (*end != '\0') || (parsed > UINT16_MAX))
		return FALSE;
	const UINT16 converted = (UINT16)parsed;
	if ((converted & ~RAIL_IPC_KNOWN_EXEC_FLAGS) != 0)
		return FALSE;
	if (((converted & TS_RAIL_EXEC_FLAG_TRANSLATE_FILES) != 0) &&
	    ((converted & TS_RAIL_EXEC_FLAG_FILE) == 0))
		return FALSE;
	*flags = converted;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_is_valid_utf8(const char* value)
{
	WINPR_ASSERT(value);
	const BYTE* bytes = (const BYTE*)value;
	const size_t length = strlen(value);

	for (size_t x = 0; x < length;)
	{
		const BYTE first = bytes[x];
		if (first <= 0x7F)
		{
			x++;
			continue;
		}

		size_t count = 0;
		BYTE secondMinimum = 0x80;
		BYTE secondMaximum = 0xBF;
		if ((first >= 0xC2) && (first <= 0xDF))
			count = 2;
		else if ((first >= 0xE0) && (first <= 0xEF))
		{
			count = 3;
			if (first == 0xE0)
				secondMinimum = 0xA0;
			else if (first == 0xED)
				secondMaximum = 0x9F;
		}
		else if ((first >= 0xF0) && (first <= 0xF4))
		{
			count = 4;
			if (first == 0xF0)
				secondMinimum = 0x90;
			else if (first == 0xF4)
				secondMaximum = 0x8F;
		}
		else
			return FALSE;

		if ((count > length - x) || (bytes[x + 1U] < secondMinimum) ||
		    (bytes[x + 1U] > secondMaximum))
			return FALSE;
		for (size_t y = 2; y < count; y++)
		{
			if ((bytes[x + y] < 0x80) || (bytes[x + y] > 0xBF))
				return FALSE;
		}
		x += count;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static const char* rail_ipc_validate_order(const RAIL_EXEC_ORDER* order)
{
	WINPR_ASSERT(order);
	if (!order->RemoteApplicationProgram || (order->RemoteApplicationProgram[0] == '\0'))
		return "program is missing or empty";
	if (!rail_ipc_is_valid_utf8(order->RemoteApplicationProgram))
		return "program is not valid UTF-8";
	if (!rail_ipc_is_valid_utf8(order->RemoteApplicationWorkingDir))
		return "working directory is not valid UTF-8";
	if (!rail_ipc_is_valid_utf8(order->RemoteApplicationArguments))
		return "arguments are not valid UTF-8";

	RAIL_UNICODE_STRING program = WINPR_C_ARRAY_INIT;
	RAIL_UNICODE_STRING workingDirectory = WINPR_C_ARRAY_INIT;
	RAIL_UNICODE_STRING arguments = WINPR_C_ARRAY_INIT;
	const BOOL programConverted =
	    utf8_string_to_rail_string(order->RemoteApplicationProgram, &program);
	const BOOL workingDirectoryConverted =
	    utf8_string_to_rail_string(order->RemoteApplicationWorkingDir, &workingDirectory);
	const BOOL argumentsConverted =
	    utf8_string_to_rail_string(order->RemoteApplicationArguments, &arguments);
	const char* error = nullptr;
	if (!programConverted)
		error = "program could not be converted to RAIL encoding";
	else if (!workingDirectoryConverted)
		error = "working directory could not be converted to RAIL encoding";
	else if (!argumentsConverted)
		error = "arguments could not be converted to RAIL encoding";
	else if ((program.length == 0) || (program.length > 520))
		error = "converted program length is outside the RAIL limit";
	else if (workingDirectory.length > 520)
		error = "converted working-directory length exceeds the RAIL limit";
	else if (arguments.length > 16000)
		error = "converted argument length exceeds the RAIL limit";
	rail_unicode_string_free(&program);
	rail_unicode_string_free(&workingDirectory);
	rail_unicode_string_free(&arguments);
	return error;
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_dispatch_record(RailClientIpcContext* ipc)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(ipc->record);
	WINPR_ASSERT(ipc->recordBytes > 0);

	char* program = nullptr;
	char* workingDirectory = nullptr;
	char* arguments = nullptr;
	UINT16 flags = 0;
	BOOL haveProgram = FALSE;
	BOOL haveWorkingDirectory = FALSE;
	BOOL haveArguments = FALSE;
	BOOL haveFlags = FALSE;

	const size_t contentLength = ipc->recordBytes - 1U;
	size_t offset = 0;
	while (offset < contentLength)
	{
		char* line = &ipc->record[offset];
		char* newline = memchr(line, '\n', contentLength - offset);
		if (!newline)
		{
			rail_ipc_reject_record(ipc, "line is not terminated");
			return TRUE;
		}
		*newline = '\0';
		offset = (size_t)(newline - ipc->record) + 1U;

		char* separator = strchr(line, '=');
		if (!separator || (separator == line))
		{
			rail_ipc_reject_record(ipc, "line has no valid key separator");
			return TRUE;
		}
		*separator = '\0';
		char* value = separator + 1U;
		if (strcmp(line, "program") == 0)
		{
			if (haveProgram)
			{
				rail_ipc_reject_record(ipc, "program key is duplicated");
				return TRUE;
			}
			haveProgram = TRUE;
			program = value;
		}
		else if (strcmp(line, "working-directory") == 0)
		{
			if (haveWorkingDirectory)
			{
				rail_ipc_reject_record(ipc, "working-directory key is duplicated");
				return TRUE;
			}
			haveWorkingDirectory = TRUE;
			workingDirectory = value;
		}
		else if (strcmp(line, "arguments") == 0)
		{
			if (haveArguments)
			{
				rail_ipc_reject_record(ipc, "arguments key is duplicated");
				return TRUE;
			}
			haveArguments = TRUE;
			arguments = value;
		}
		else if (strcmp(line, "flags") == 0)
		{
			if (haveFlags)
			{
				rail_ipc_reject_record(ipc, "flags key is duplicated");
				return TRUE;
			}
			haveFlags = TRUE;
			if (!rail_ipc_parse_flags(value, &flags))
			{
				rail_ipc_reject_record(ipc, "flags value is invalid");
				return TRUE;
			}
		}
		else
		{
			rail_ipc_reject_record(ipc, "record contains an unknown key");
			return TRUE;
		}
	}

	const RAIL_EXEC_ORDER order = { .flags = flags,
		                            .RemoteApplicationProgram = haveProgram ? program : "",
		                            .RemoteApplicationWorkingDir =
		                                haveWorkingDirectory ? workingDirectory : "",
		                            .RemoteApplicationArguments = haveArguments ? arguments : "" };
	const char* error = rail_ipc_validate_order(&order);
	if (error)
	{
		rail_ipc_reject_record(ipc, error);
		return TRUE;
	}

	WINPR_ASSERT(ipc->rail);
	WINPR_ASSERT(ipc->rail->ClientExecute);
	const UINT status = ipc->rail->ClientExecute(ipc->rail, &order);
	if (status != CHANNEL_RC_OK)
		WLog_Print(ipc->log, WLOG_WARN,
		           "RAIL IPC ClientExecute rejected a launch locally with error %" PRIu32, status);
	return TRUE;
}

static void rail_ipc_reset_record(RailClientIpcContext* ipc)
{
	WINPR_ASSERT(ipc);
	ipc->recordBytes = 0;
	ipc->atLineStart = TRUE;
	ipc->recordInvalid = FALSE;
	ipc->recordError = nullptr;
}

WINPR_ATTR_NODISCARD
static BOOL rail_ipc_feed(RailClientIpcContext* ipc, const BYTE* data, size_t length,
                          size_t* completedRecords)
{
	WINPR_ASSERT(ipc);
	WINPR_ASSERT(data || (length == 0));
	WINPR_ASSERT(completedRecords);

	for (size_t x = 0; x < length; x++)
	{
		const BYTE value = data[x];
		const BOOL terminator = (value == '\n') && ipc->atLineStart;
		if (ipc->recordBytes < ipc->pipeBuffer)
			ipc->record[ipc->recordBytes] = (char)value;
		else if (!ipc->recordInvalid)
		{
			ipc->recordInvalid = TRUE;
			ipc->recordError = "record exceeds the FIFO atomic-write limit";
		}
		if (ipc->recordBytes <= ipc->pipeBuffer)
			ipc->recordBytes++;
		if ((value == '\0') && !ipc->recordInvalid)
		{
			ipc->recordInvalid = TRUE;
			ipc->recordError = "record contains NUL";
		}
		ipc->atLineStart = (value == '\n');

		if (!terminator)
			continue;
		(*completedRecords)++;
		if (ipc->recordInvalid)
			rail_ipc_reject_record(ipc, ipc->recordError ? ipc->recordError : "record is invalid");
		else
		{
			ipc->record[ipc->recordBytes] = '\0';
			if (!rail_ipc_dispatch_record(ipc))
				return FALSE;
		}
		rail_ipc_reset_record(ipc);
	}
	return TRUE;
}

#endif

RailClientIpcContext* freerdp_client_rail_ipc_new(const rdpSettings* settings, wLog* log)
{
	if (!settings || !log)
		return nullptr;

#if defined(_WIN32)
	WLog_Print(log, WLOG_ERROR, "RAIL IPC FIFO transport is unsupported on this platform");
	return nullptr;
#else
	RailClientIpcContext* ipc = calloc(1, sizeof(*ipc));
	if (!ipc)
		return nullptr;
	ipc->log = log;
	ipc->ownerThreadId = GetCurrentThreadId();
	ipc->runtimeDirectoryFd = -1;
	ipc->lockFd = -1;
	ipc->readFd = -1;
	ipc->heldWriteFd = -1;

	char* runtimeDirectory = rail_ipc_get_runtime_directory();
	if (!runtimeDirectory)
	{
		WLog_Print(log, WLOG_ERROR, "RAIL IPC requires XDG_RUNTIME_DIR");
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

	WLog_Print(log, WLOG_INFO, "RAIL launch FIFO available at %s", ipc->path);
	return ipc;

fail:
	freerdp_client_rail_ipc_free(ipc);
	return nullptr;
#endif
}

void freerdp_client_rail_ipc_free(RailClientIpcContext* ipc)
{
	if (!ipc)
		return;
	rail_ipc_assert_owner_thread(ipc);

#if !defined(_WIN32)
	rail_ipc_disable(ipc);
	if (ipc->lockFd >= 0)
	{
		close(ipc->lockFd);
		ipc->lockFd = -1;
	}
	if (ipc->runtimeDirectoryFd >= 0)
	{
		close(ipc->runtimeDirectoryFd);
		ipc->runtimeDirectoryFd = -1;
	}
#endif
	free(ipc->path);
	free(ipc);
}

HANDLE freerdp_client_rail_ipc_get_event(const RailClientIpcContext* ipc)
{
	if (!ipc)
		return nullptr;
	rail_ipc_assert_owner_thread(ipc);
#if defined(_WIN32)
	return nullptr;
#else
	return (ipc->enabled && ipc->rail && ipc->ready) ? ipc->readEvent : nullptr;
#endif
}

BOOL freerdp_client_rail_ipc_check_event(RailClientIpcContext* ipc)
{
	if (!ipc)
		return TRUE;
	rail_ipc_assert_owner_thread(ipc);
	if (!ipc->enabled || !ipc->rail || !ipc->ready)
		return TRUE;
#if !defined(_WIN32)
	if (!ipc->rail->ClientExecute)
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC disabled because the attached channel cannot execute launches");
		rail_ipc_disable(ipc);
		return FALSE;
	}

	const DWORD waitStatus = WaitForSingleObject(ipc->readEvent, 0);
	if (waitStatus == WAIT_TIMEOUT)
		return TRUE;
	if (waitStatus != WAIT_OBJECT_0)
	{
		WLog_Print(ipc->log, WLOG_ERROR,
		           "RAIL IPC disabled after its launch FIFO event failed with status 0x%08" PRIx32,
		           waitStatus);
		rail_ipc_disable(ipc);
		return FALSE;
	}

	size_t bytes = 0;
	size_t completedRecords = 0;
	while ((bytes < RAIL_IPC_MAX_BYTES_PER_PASS) &&
	       (completedRecords < RAIL_IPC_MAX_RECORDS_PER_PASS))
	{
		/* Lifecycle transitions share this thread. Recheck before every read so a transition
		 * observed between the wake and this call wins without draining the FIFO. */
		if (!ipc->enabled || !ipc->rail || !ipc->ready)
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
			if (!rail_ipc_feed(ipc, buffer, (size_t)readBytes, &completedRecords))
			{
				rail_ipc_disable(ipc);
				return FALSE;
			}
			continue;
		}
		if (readBytes == 0)
		{
			WLog_Print(ipc->log, WLOG_ERROR,
			           "RAIL IPC disabled after an unexpected end of launch FIFO input");
			rail_ipc_disable(ipc);
			return FALSE;
		}
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			break;
		rail_ipc_log_errno(ipc, WLOG_ERROR, "reading the launch FIFO", errno);
		rail_ipc_disable(ipc);
		return FALSE;
	}
#endif
	return TRUE;
}

BOOL freerdp_client_rail_ipc_attach(RailClientIpcContext* ipc, RailClientContext* rail)
{
	if (!ipc)
		return TRUE;
	rail_ipc_assert_owner_thread(ipc);
	if (!rail || (ipc->rail && (ipc->rail != rail)))
		return FALSE;
	ipc->rail = rail;
	ipc->ready = FALSE;
	return TRUE;
}

BOOL freerdp_client_rail_ipc_detach(RailClientIpcContext* ipc, RailClientContext* rail)
{
	if (!ipc)
		return TRUE;
	rail_ipc_assert_owner_thread(ipc);
	if (!rail || (ipc->rail != rail))
		return FALSE;
	ipc->ready = FALSE;
	ipc->rail = nullptr;
	return TRUE;
}

BOOL freerdp_client_rail_ipc_set_ready(RailClientIpcContext* ipc, BOOL ready)
{
	if (!ipc)
		return TRUE;
	rail_ipc_assert_owner_thread(ipc);
	if (!ipc->enabled || !ready)
	{
		ipc->ready = FALSE;
		return TRUE;
	}
	if (!ipc->rail)
		return FALSE;
	ipc->ready = TRUE;
	return TRUE;
}

const char* freerdp_client_rail_ipc_get_path(const RailClientIpcContext* ipc)
{
	if (!ipc)
		return nullptr;
	return ipc->path;
}

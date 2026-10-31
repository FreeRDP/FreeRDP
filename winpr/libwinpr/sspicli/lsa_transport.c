/**
 * WinPR: Windows Portable Runtime
 * lsa4posix client transport (AF_UNIX connection to the daemon)
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

#include <winpr/config.h>

#ifndef _WIN32

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/wlog.h>
#include <winpr/nt.h>
#include <winpr/environment.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "lsa_transport.h"

#define TAG "com.winpr.sspicli.lsa"

/* Return a malloc'd copy of an environment variable's value, or NULL when unset
 * or empty. Uses the WinPR environment API, which is thread safe unlike getenv. */
static char* dup_env(const char* name)
{
	const DWORD n = GetEnvironmentVariableX(name, nullptr, 0);
	if (n <= 1) /* unset or empty: the reported length-plus-null is 1 */
		return nullptr;

	char* value = malloc(n);
	if (!value)
		return nullptr;

	if (GetEnvironmentVariableX(name, value, n) != (n - 1))
	{
		free(value);
		return nullptr;
	}
	return value;
}

static char* lsa_socket_path(void)
{
	char* env = dup_env("LSA4POSIX_SOCKET");
	if (env)
		return env;

	char* path = nullptr;
	size_t len = 0;

	char* xdg = dup_env("XDG_RUNTIME_DIR");
	if (xdg)
	{
		const int rc = winpr_asprintf(&path, &len, "%s/lsa4posix/lsa.sock", xdg);
		free(xdg);
		if (rc < 0)
			return nullptr;
		return path;
	}

#ifdef __APPLE__
	char* tmp = dup_env("TMPDIR");
	if (tmp)
	{
		const int rc =
		    winpr_asprintf(&path, &len, "%slsa4posix-%u/lsa.sock", tmp, (unsigned)geteuid());
		free(tmp);
		if (rc < 0)
			return nullptr;
		return path;
	}
#endif

	if (winpr_asprintf(&path, &len, "/tmp/lsa4posix-%u/lsa.sock", (unsigned)geteuid()) < 0)
		return nullptr;
	return path;
}

/* The socket directory must be owned by this user and not accessible to group or others. */
static BOOL lsa_dir_is_safe(const char* sockPath)
{
	char* copy = _strdup(sockPath);
	if (!copy)
		return FALSE;

	char* slash = strrchr(copy, '/');
	if (!slash || (slash == copy))
	{
		free(copy);
		return FALSE;
	}
	*slash = '\0';

	struct stat st = { 0 };
	const BOOL rc = (stat(copy, &st) == 0) && S_ISDIR(st.st_mode) && (st.st_uid == geteuid()) &&
	                ((st.st_mode & (S_IRWXG | S_IRWXO)) == 0);
	if (!rc)
		WLog_WARN(TAG, "lsa4posix socket directory is missing or not private");
	free(copy);
	return rc;
}

int lsa4posix_client_connect(void)
{
	char* path = lsa_socket_path();
	if (!path)
		return -1;

	if (!lsa_dir_is_safe(path))
	{
		free(path);
		return -1;
	}

	struct sockaddr_un addr = { 0 };
	addr.sun_family = AF_UNIX;
	if (strlen(path) >= sizeof(addr.sun_path))
	{
		WLog_ERR(TAG, "lsa4posix socket path too long");
		free(path);
		return -1;
	}
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
	free(path);

	const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
	{
		close(fd);
		return -1;
	}
	return fd;
}

void lsa4posix_client_close(int fd)
{
	if (fd >= 0)
		close(fd);
}

static BOOL lsa_write_all(int fd, const BYTE* data, size_t len)
{
	size_t off = 0;
	while (off < len)
	{
		const ssize_t n = write(fd, &data[off], len - off);
		if (n > 0)
			off += (size_t)n;
		else if ((n < 0) && (errno == EINTR))
			continue;
		else
			return FALSE;
	}
	return TRUE;
}

static BOOL lsa_read_all(int fd, BYTE* data, size_t len)
{
	size_t off = 0;
	while (off < len)
	{
		const ssize_t n = read(fd, &data[off], len - off);
		if (n > 0)
			off += (size_t)n;
		else if ((n < 0) && (errno == EINTR))
			continue;
		else
			return FALSE;
	}
	return TRUE;
}

static BOOL lsa_send(int fd, wStream* payload)
{
	const size_t len = Stream_Length(payload);
	if ((len == 0) || (len > LSA_PROTO_MAX_FRAME))
		return FALSE;

	BYTE prefix[4] = { 0 };
	prefix[0] = (BYTE)((len >> 24) & 0xFF);
	prefix[1] = (BYTE)((len >> 16) & 0xFF);
	prefix[2] = (BYTE)((len >> 8) & 0xFF);
	prefix[3] = (BYTE)(len & 0xFF);

	return lsa_write_all(fd, prefix, sizeof(prefix)) &&
	       lsa_write_all(fd, Stream_Buffer(payload), len);
}

static wStream* lsa_recv(int fd)
{
	BYTE prefix[4] = { 0 };
	if (!lsa_read_all(fd, prefix, sizeof(prefix)))
		return nullptr;

	const UINT32 len = ((UINT32)prefix[0] << 24) | ((UINT32)prefix[1] << 16) |
	                   ((UINT32)prefix[2] << 8) | (UINT32)prefix[3];
	if ((len == 0) || (len > LSA_PROTO_MAX_FRAME))
		return nullptr;

	wStream* s = Stream_New(nullptr, len);
	if (!s)
		return nullptr;

	if (!lsa_read_all(fd, Stream_Buffer(s), len))
	{
		Stream_Free(s, TRUE);
		return nullptr;
	}
	if (!Stream_SetLength(s, len) || !Stream_SetPosition(s, 0))
	{
		Stream_Free(s, TRUE);
		return nullptr;
	}
	return s;
}

NTSTATUS lsa4posix_transact(int fd, UINT16 expectType, wStream* request, UINT32 requestId,
                            LsaProtoReader* reader, LSA_MSG_HDR* respHdr, wStream** response)
{
	WINPR_ASSERT(response);
	*response = nullptr;

	const BOOL sent = lsa_send(fd, request);
	Stream_Free(request, TRUE);
	if (!sent)
		return STATUS_UNSUCCESSFUL;

	wStream* resp = lsa_recv(fd);
	if (!resp)
		return STATUS_UNSUCCESSFUL;

	if (!lsa_proto_reader_init(reader, resp, respHdr))
	{
		Stream_Free(resp, TRUE);
		return STATUS_UNSUCCESSFUL;
	}

	if ((respHdr->type != expectType) || !(respHdr->flags & LSA_MSG_FLAG_RESPONSE) ||
	    (respHdr->requestId != requestId))
	{
		lsa_proto_reader_done(reader);
		Stream_Free(resp, TRUE);
		return STATUS_UNSUCCESSFUL;
	}

	*response = resp;
	return STATUS_SUCCESS;
}

#endif /* _WIN32 */

#include <freerdp/client/client_cliprdr_file.h>
#include <freerdp/utils/cliprdr_utils.h>
#include <winpr/collections.h>
#include <winpr/thread.h>
#include <winpr/synch.h>
#include <winpr/string.h>
#include <winpr/path.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/vfs.h>

#define FILE_SIZE 32768

typedef struct
{
	CliprdrClientContext context;
	wMessageQueue* requests;
	HANDLE request_seen;
	HANDLE allow_response;
	CRITICAL_SECTION lock;
	BOOL locked[256];
	UINT32 last_id;
	UINT32 unlocks;
} TestPeer;

static UINT lock_data(CliprdrClientContext* context, const CLIPRDR_LOCK_CLIPBOARD_DATA* data)
{
	TestPeer* peer = context->handle;
	if (data->clipDataId >= ARRAYSIZE(peer->locked))
		return ERROR_INVALID_DATA;
	EnterCriticalSection(&peer->lock);
	peer->locked[data->clipDataId] = TRUE;
	peer->last_id = data->clipDataId;
	LeaveCriticalSection(&peer->lock);
	return CHANNEL_RC_OK;
}

static UINT unlock_data(CliprdrClientContext* context, const CLIPRDR_UNLOCK_CLIPBOARD_DATA* data)
{
	TestPeer* peer = context->handle;
	if (data->clipDataId >= ARRAYSIZE(peer->locked))
		return ERROR_INVALID_DATA;
	EnterCriticalSection(&peer->lock);
	peer->locked[data->clipDataId] = FALSE;
	peer->unlocks++;
	LeaveCriticalSection(&peer->lock);
	return CHANNEL_RC_OK;
}

static UINT request_data(CliprdrClientContext* context,
                         const CLIPRDR_FILE_CONTENTS_REQUEST* request)
{
	TestPeer* peer = context->handle;
	CLIPRDR_FILE_CONTENTS_REQUEST* copy = malloc(sizeof(*copy));
	if (!copy)
		return ERROR_OUTOFMEMORY;
	*copy = *request;
	if (!MessageQueue_Post(peer->requests, nullptr, 1, copy, nullptr))
	{
		free(copy);
		return ERROR_INTERNAL_ERROR;
	}
	return CHANNEL_RC_OK;
}

static DWORD WINAPI serve_requests(void* arg)
{
	TestPeer* peer = arg;
	wMessage msg = WINPR_C_ARRAY_INIT;
	while (MessageQueue_Wait(peer->requests))
	{
		if (!MessageQueue_Peek(peer->requests, &msg, TRUE))
			continue;
		if (msg.id == WMQ_QUIT)
			break;
		CLIPRDR_FILE_CONTENTS_REQUEST* request = msg.wParam;
		(void)SetEvent(peer->request_seen);
		if (WaitForSingleObject(peer->allow_response, 5000) != WAIT_OBJECT_0)
		{
			free(request);
			return ERROR_TIMEOUT;
		}
		CLIPRDR_FILE_CONTENTS_RESPONSE response = WINPR_C_ARRAY_INIT;
		BYTE data[FILE_SIZE];
		response.streamId = request->streamId;
		response.common.msgFlags = CB_RESPONSE_FAIL;
		EnterCriticalSection(&peer->lock);
		const BOOL locked = request->haveClipDataId &&
		                    request->clipDataId < ARRAYSIZE(peer->locked) &&
		                    peer->locked[request->clipDataId];
		LeaveCriticalSection(&peer->lock);
		if (locked)
		{
			memset(data, (BYTE)request->clipDataId, sizeof(data));
			response.common.msgFlags = CB_RESPONSE_OK;
			response.cbRequested = MIN(request->cbRequested, sizeof(data));
			response.requestedData = data;
			if (request->dwFlags == FILECONTENTS_SIZE)
			{
				const UINT64 file_size = FILE_SIZE;
				memcpy(data, &file_size, sizeof(file_size));
				response.cbRequested = sizeof(file_size);
			}
		}
		const UINT status = peer->context.ServerFileContentsResponse(&peer->context, &response);
		free(request);
		if (status)
			return status;
	}
	return 0;
}

static BOOL read_data(int fd, BYTE expected)
{
	BYTE data[1024] = WINPR_C_ARRAY_INIT;
	if (read(fd, data, sizeof(data)) != sizeof(data))
	{
		perror("read clipboard file");
		return FALSE;
	}
	for (size_t i = 0; i < sizeof(data); i++)
	{
		if (data[i] != expected)
			return FALSE;
	}
	return TRUE;
}

typedef struct
{
	int fd;
	BYTE expected;
} PendingRead;

static DWORD WINAPI read_pending(void* arg)
{
	PendingRead* read = arg;
	return read_data(read->fd, read->expected) ? 0 : 1;
}

int TestClientClipboardTransfer(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	TestPeer peer = WINPR_C_ARRAY_INIT;
	CliprdrFileContext* file = nullptr;
	wClipboard* clipboard = nullptr;
	HANDLE thread = nullptr;
	HANDLE reader = nullptr;
	PendingRead pending = WINPR_C_ARRAY_INIT;
	BYTE* list = nullptr;
	UINT32 list_size = 0;
	char* path = nullptr;
	int fd = -1;
	int second_fd = -1;
	int directory_fd = -1;
	int child_fd = -1;
	int rc = -1;
	FILEDESCRIPTORW descriptors[3] = WINPR_C_ARRAY_INIT;
	InitializeCriticalSection(&peer.lock);
	peer.requests = MessageQueue_New(nullptr);
	peer.request_seen = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	peer.allow_response = CreateEvent(nullptr, TRUE, TRUE, nullptr);
	peer.context.handle = &peer;
	peer.context.ClientLockClipboardData = lock_data;
	peer.context.ClientUnlockClipboardData = unlock_data;
	peer.context.ClientFileContentsRequest = request_data;
	if (!peer.requests || !peer.request_seen || !peer.allow_response)
		goto out;
	file = cliprdr_file_context_new(&peer);
	clipboard = ClipboardCreate();
	if (!file || !clipboard || !cliprdr_file_context_init(file, &peer.context))
		goto out;
	if (!cliprdr_file_context_remote_set_flags(file,
	                                           CB_STREAM_FILECLIP_ENABLED | CB_CAN_LOCK_CLIPDATA))
		goto out;
	if (cliprdr_file_context_notify_new_server_format_list(file))
		goto out;
	descriptors[0].dwFlags = FD_FILESIZE | FD_ATTRIBUTES;
	descriptors[0].dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	descriptors[0].nFileSizeLow = FILE_SIZE;
	if (!ConvertUtf8ToWChar("test.bin", descriptors[0].cFileName,
	                        ARRAYSIZE(descriptors[0].cFileName)))
		goto out;
	descriptors[1].dwFlags = FD_ATTRIBUTES;
	descriptors[1].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	descriptors[2].dwFlags = FD_ATTRIBUTES;
	descriptors[2].dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	if (!ConvertUtf8ToWChar("folder", descriptors[1].cFileName,
	                        ARRAYSIZE(descriptors[1].cFileName)) ||
	    !ConvertUtf8ToWChar("folder\\child.bin", descriptors[2].cFileName,
	                        ARRAYSIZE(descriptors[2].cFileName)))
		goto out;
	if (cliprdr_serialize_file_list(descriptors, ARRAYSIZE(descriptors), &list, &list_size))
		goto out;
	if (!cliprdr_file_context_update_server_data(file, clipboard, list, list_size))
		goto out;
	const wClipboardDelegate* delegate = ClipboardGetDelegate(clipboard);
	path = GetCombinedPath(delegate->basePath, "test.bin");
	if (!path)
		goto out;
	struct statfs fs = WINPR_C_ARRAY_INIT;
	BOOL mounted = FALSE;
	for (size_t i = 0; i < 100; i++)
	{
		if (statfs(delegate->basePath, &fs) == 0 && fs.f_type == 0x65735546)
		{
			mounted = TRUE;
			break;
		}
		Sleep(10);
	}
	if (!mounted)
	{
		fprintf(stderr, "FUSE mount unavailable\n");
		rc = 77;
		goto out;
	}
	thread = CreateThread(nullptr, 0, serve_requests, &peer, 0, nullptr);
	if (!thread)
		goto out;
	fd = open(path, O_RDONLY);
	const BYTE original_id = (BYTE)peer.last_id;
	if (fd < 0 || !read_data(fd, original_id))
		goto out;
	char* directory_path = GetCombinedPath(delegate->basePath, "folder");
	if (!directory_path)
		goto out;
	directory_fd = open(directory_path, O_RDONLY | O_DIRECTORY);
	free(directory_path);
	if (directory_fd < 0)
		goto out;
	(void)ResetEvent(peer.request_seen);
	(void)ResetEvent(peer.allow_response);
	pending.fd = fd;
	pending.expected = original_id;
	reader = CreateThread(nullptr, 0, read_pending, &pending, 0, nullptr);
	if (!reader || WaitForSingleObject(peer.request_seen, 5000) != WAIT_OBJECT_0)
		goto out;
	if (cliprdr_file_context_notify_new_client_format_list(file))
		goto out;
	(void)SetEvent(peer.allow_response);
	DWORD read_status = 1;
	if (WaitForSingleObject(reader, 5000) != WAIT_OBJECT_0 ||
	    !GetExitCodeThread(reader, &read_status) || read_status != 0)
	{
		fprintf(stderr, "Pending read aborted after local clipboard change\n");
		goto out;
	}
	child_fd = openat(directory_fd, "child.bin", O_RDONLY);
	if (child_fd < 0 || !read_data(child_fd, original_id))
	{
		fprintf(stderr, "Cannot continue directory copy after clipboard change\n");
		goto out;
	}
	if (cliprdr_file_context_notify_new_server_format_list(file) || !read_data(fd, original_id))
		goto out;
	if (!cliprdr_file_context_update_server_data(file, clipboard, list, list_size))
		goto out;
	free(path);
	path = GetCombinedPath(delegate->basePath, "test.bin");
	if (!path)
		goto out;
	second_fd = open(path, O_RDONLY);
	const BYTE second_id = (BYTE)peer.last_id;
	if (second_fd < 0 || !read_data(second_fd, second_id))
		goto out;
	if (!cliprdr_file_context_update_server_data(file, clipboard, list, list_size) ||
	    !read_data(second_fd, second_id))
		goto out;
	for (size_t i = 0; i < 100; i++)
	{
		if (cliprdr_file_context_notify_new_server_format_list(file))
			goto out;
	}
	EnterCriticalSection(&peer.lock);
	const BOOL bounded = peer.unlocks + 3 == peer.last_id;
	LeaveCriticalSection(&peer.lock);
	if (!bounded || !read_data(fd, original_id) || !read_data(second_fd, second_id))
	{
		fprintf(stderr, "Old transfers lost or unused locks retained\n");
		goto out;
	}
	rc = 0;
out:
	if (peer.allow_response)
		(void)SetEvent(peer.allow_response);
	if (reader)
	{
		(void)WaitForSingleObject(reader, INFINITE);
		(void)CloseHandle(reader);
	}
	if (child_fd >= 0)
		close(child_fd);
	if (directory_fd >= 0)
		close(directory_fd);
	if (second_fd >= 0)
		close(second_fd);
	if (fd >= 0)
		close(fd);
	if (thread)
	{
		(void)MessageQueue_PostQuit(peer.requests, 0);
		(void)WaitForSingleObject(thread, INFINITE);
		(void)CloseHandle(thread);
	}
	if (file)
		(void)cliprdr_file_context_uninit(file, &peer.context);
	cliprdr_file_context_free(file);
	if (rc == 0 && peer.unlocks != peer.last_id)
	{
		fprintf(stderr, "Clipboard locks remain after disconnect\n");
		rc = -1;
	}
	if (peer.request_seen)
		(void)CloseHandle(peer.request_seen);
	if (peer.allow_response)
		(void)CloseHandle(peer.allow_response);
	ClipboardDestroy(clipboard);
	MessageQueue_Free(peer.requests);
	DeleteCriticalSection(&peer.lock);
	free(path);
	free(list);
	return rc;
}

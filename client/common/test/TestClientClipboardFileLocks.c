#include <freerdp/client/client_cliprdr_file.h>
#include <freerdp/channels/channels.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static UINT16 response_flags;
static BYTE response_data[8];

#define CHECK(x)                                                       \
	do                                                                 \
	{                                                                  \
		if (!(x))                                                      \
		{                                                              \
			(void)fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
			goto fail;                                                 \
		}                                                              \
	} while (0)

static UINT response(CliprdrClientContext* c, const CLIPRDR_FILE_CONTENTS_RESPONSE* r)
{
	(void)c;
	response_flags = r->common.msgFlags;
	memset(response_data, 0, sizeof(response_data));
	if (r->requestedData && r->cbRequested <= sizeof(response_data))
		memcpy(response_data, r->requestedData, r->cbRequested);
	return 0;
}

static BOOL read_data(CliprdrClientContext* c, BOOL locked, UINT32 id, const char* expected)
{
	CLIPRDR_FILE_CONTENTS_REQUEST r = { 0 };
	r.haveClipDataId = locked;
	r.clipDataId = id;
	r.streamId = 1;
	r.dwFlags = FILECONTENTS_RANGE;
	r.nPositionLow = 20U * 1024U * 1024U;
	r.cbRequested = 4;
	response_flags = 0;
	CHECK(c->ServerFileContentsRequest(c, &r) == 0);
	CHECK(response_flags == (expected ? CB_RESPONSE_OK : CB_RESPONSE_FAIL));
	if (expected)
		CHECK(memcmp(response_data, expected, 4) == 0);
	return TRUE;
fail:
	return FALSE;
}

static BOOL lock_data(CliprdrClientContext* c, UINT32 id)
{
	CLIPRDR_LOCK_CLIPBOARD_DATA r = { 0 };
	r.clipDataId = id;
	return c->ServerLockClipboardData(c, &r) == 0;
}
static BOOL unlock_data(CliprdrClientContext* c, UINT32 id)
{
	CLIPRDR_UNLOCK_CLIPBOARD_DATA r = { 0 };
	r.clipDataId = id;
	return c->ServerUnlockClipboardData(c, &r) == 0;
}

int main(void)
{
	char a[] = "/tmp/cliprdr-lock-a-XXXXXX";
	char b[] = "/tmp/cliprdr-lock-b-XXXXXX";
	char ua[128];
	char ub[128];
	int fd = -1;
	int rc = 1;
	CliprdrFileContext* f = nullptr;
	CliprdrClientContext c = { 0 };
	const char* paths[] = { a, b };
	const char* values[] = { "AAAA", "BBBB" };
	for (size_t i = 0; i < 2; i++)
	{
		fd = mkstemp((i == 0) ? a : b);
		CHECK(fd >= 0);
		CHECK(ftruncate(fd, 32LL * 1024LL * 1024LL) == 0);
		CHECK(pwrite(fd, values[i], 4, 20LL * 1024LL * 1024LL) == 4);
		CHECK(close(fd) == 0);
		fd = -1;
	}
	CHECK(snprintf(ua, sizeof(ua), "file://%s\n", paths[0]) > 0);
	CHECK(snprintf(ub, sizeof(ub), "file://%s\n", paths[1]) > 0);
	c.ClientFileContentsResponse = response;
	f = cliprdr_file_context_new(nullptr);
	CHECK(f);
	CHECK(cliprdr_file_context_init(f, &c));
	CHECK(cliprdr_file_context_remote_set_flags(f,
	                                            CB_STREAM_FILECLIP_ENABLED | CB_CAN_LOCK_CLIPDATA));
	CHECK(cliprdr_file_context_set_locally_available(f, TRUE));
	CHECK(cliprdr_file_context_current_flags(f) & CB_CAN_LOCK_CLIPDATA);
	CHECK(cliprdr_file_context_notify_new_client_format_list(f) == 0);
	CHECK(lock_data(&c, 0));
	CHECK(cliprdr_file_context_update_client_data(f, ua, strlen(ua)));
	CHECK(read_data(&c, TRUE, 0, "AAAA"));
	CHECK(read_data(&c, FALSE, 0, "AAAA"));
	CHECK(lock_data(&c, 7));
	CHECK(cliprdr_file_context_clear(f));
	CHECK(read_data(&c, TRUE, 0, "AAAA"));
	CHECK(read_data(&c, TRUE, 7, "AAAA"));
	CHECK(read_data(&c, FALSE, 0, nullptr));
	CHECK(cliprdr_file_context_notify_new_client_format_list(f) == 0);
	CHECK(lock_data(&c, 8));
	CHECK(cliprdr_file_context_update_client_data(f, ub, strlen(ub)));
	CHECK(read_data(&c, FALSE, 0, "BBBB"));
	CHECK(read_data(&c, TRUE, 8, "BBBB"));
	CHECK(read_data(&c, TRUE, 0, "AAAA"));
	CHECK(read_data(&c, TRUE, 7, "AAAA"));
	CHECK(cliprdr_file_context_clear(f));
	CHECK(read_data(&c, TRUE, 8, "BBBB"));
	CHECK(unlock_data(&c, 7));
	CHECK(read_data(&c, TRUE, 7, nullptr));
	CHECK(read_data(&c, TRUE, 0, "AAAA"));
	CHECK(unlock_data(&c, 0));
	CHECK(unlock_data(&c, 8));
	CHECK(read_data(&c, TRUE, 0, nullptr));
	CHECK(read_data(&c, TRUE, 8, nullptr));
	rc = 0;
fail:
	if (f)
		cliprdr_file_context_free(f);
	if (fd >= 0)
		(void)close(fd);
	(void)unlink(a);
	(void)unlink(b);
	return rc;
}

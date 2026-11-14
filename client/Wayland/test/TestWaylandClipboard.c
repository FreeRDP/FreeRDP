/**
 * FreeRDP: Wayland clipboard dispatch regression
 * Licensed under the Apache License, Version 2.0.
 *
 * Exercise the actual callback/handoff/read implementation with real WinPR
 * synchronization and pipes. UWAC is a test double recording caller threads;
 * no compositor or RDP server is needed.
 */
#include <freerdp/config.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <winpr/synch.h>

/* Observe the real wait returning on abort so the active-call test uses a
 * handshake rather than a timing assumption. */
static HANDLE abortObserved;
static DWORD observed_wait(DWORD count, const HANDLE* handles, BOOL all, DWORD timeout)
{
	const DWORD rc = WaitForMultipleObjects(count, handles, all, timeout);
	if (abortObserved && (rc == WAIT_OBJECT_0 + 1))
		SetEvent(abortObserved);
	return rc;
}
#define WaitForMultipleObjects observed_wait
#include "../wlf_cliprdr.c"
#undef WaitForMultipleObjects

#define CHECK(x)                                                    \
	do                                                              \
	{                                                               \
		if (!(x))                                                   \
		{                                                           \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
			exit(1);                                                \
		}                                                           \
	} while (0)

static DWORD owner;
static atomic_int wrong_thread;
static atomic_int announcements;
static atomic_int opens;
static atomic_int responses;
static atomic_int data_responses;
static int write_fd = -1;
static UwacReturnCode announce_result = UWAC_SUCCESS;
static BOOL fail_open;
static HANDLE announce_entered;
static HANDLE announce_release;

static void check_owner(void)
{
	if (GetCurrentThreadId() != owner)
		atomic_fetch_add(&wrong_thread, 1);
}

UwacReturnCode UwacClipboardOfferDestroy(UwacSeat* seat)
{
	WINPR_UNUSED(seat);
	check_owner();
	return UWAC_SUCCESS;
}
UwacReturnCode UwacClipboardOfferCreate(UwacSeat* seat, const char* mime)
{
	WINPR_UNUSED(seat);
	WINPR_UNUSED(mime);
	check_owner();
	return UWAC_SUCCESS;
}
UwacReturnCode UwacClipboardOfferAnnounce(UwacSeat* seat, void* context,
                                          UwacDataTransferHandler transfer,
                                          UwacCancelDataTransferHandler cancel)
{
	WINPR_UNUSED(seat);
	WINPR_UNUSED(context);
	WINPR_UNUSED(transfer);
	WINPR_UNUSED(cancel);
	check_owner();
	atomic_fetch_add(&announcements, 1);
	if (announce_entered)
	{
		SetEvent(announce_entered);
		WaitForSingleObject(announce_release, INFINITE);
	}
	return announce_result;
}
int UwacClipboardDataGetFd(UwacSeat* seat, const char* mime)
{
	WINPR_UNUSED(seat);
	WINPR_UNUSED(mime);
	check_owner();
	atomic_fetch_add(&opens, 1);
	if (fail_open)
		return -1;
	int fds[2];
	if (pipe(fds) < 0)
		return -1;
	fcntl(fds[0], F_SETFL, O_NONBLOCK);
	write_fd = fds[1];
	return fds[0];
}

static UINT format_response(CliprdrClientContext* context,
                            const CLIPRDR_FORMAT_LIST_RESPONSE* response)
{
	WINPR_UNUSED(context);
	if (!(response->common.msgFlags & CB_RESPONSE_OK))
		return ERROR_INTERNAL_ERROR;
	atomic_fetch_add(&responses, 1);
	return CHANNEL_RC_OK;
}

static UINT data_response(CliprdrClientContext* context,
                          const CLIPRDR_FORMAT_DATA_RESPONSE* response)
{
	WINPR_UNUSED(context);
	const BYTE expected[] = { 'h', 0, 'e', 0, 'l', 0, 'l', 0, 'o', 0, 0, 0 };
	CHECK(response->common.msgFlags == CB_RESPONSE_OK);
	CHECK(response->common.dataLen == sizeof(expected));
	CHECK(response->requestedFormatData);
	CHECK(memcmp(response->requestedFormatData, expected, sizeof(expected)) == 0);
	atomic_fetch_add(&data_responses, 1);
	return CHANNEL_RC_OK;
}

typedef struct
{
	wfClipboard* clipboard;
	CliprdrClientContext* channel;
	BOOL read;
	BOOL request_data;
	char* data;
	size_t size;
	UINT result;
	HANDLE done;
} Worker;

static DWORD WINAPI worker(void* arg)
{
	Worker* w = arg;
	if (w->request_data)
	{
		const CLIPRDR_FORMAT_DATA_REQUEST request = { .requestedFormatId = CF_UNICODETEXT };
		w->result = w->channel->ServerFormatDataRequest(w->channel, &request);
	}
	else if (w->read)
		w->data = wlf_cliprdr_read_data(w->clipboard, "text/plain", &w->size);
	else
	{
		/* Borrowed, stack-resident arguments must survive the entire handoff. */
		char name[] = "HTML Format";
		CLIPRDR_FORMAT format = { .formatId = 0xc001, .formatName = name };
		const CLIPRDR_FORMAT_LIST list = { .numFormats = 1, .formats = &format };
		w->result = w->channel->ServerFormatList(w->channel, &list);
	}
	SetEvent(w->done);
	return 0;
}

static BOOL start(Worker* w, HANDLE* thread)
{
	w->data = nullptr;
	w->size = 0;
	w->result = ERROR_INTERNAL_ERROR;
	w->done = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	CHECK(w->done);
	*thread = CreateThread(nullptr, 0, worker, w, 0, nullptr);
	CHECK(*thread);
	/* If the old callback ran immediately, done wins and the assertion fails. */
	const HANDLE handles[] = { w->done, wlf_cliprdr_get_event_handle(w->clipboard) };
	CHECK(WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, 5000) == WAIT_OBJECT_0 + 1);
	CHECK(WaitForSingleObject(w->done, 0) == WAIT_TIMEOUT);
	return TRUE;
}
static BOOL join(Worker* w, HANDLE thread)
{
	CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
	CHECK(CloseHandle(thread));
	CHECK(CloseHandle(w->done));
	CHECK(atomic_load(&wrong_thread) == 0);
	return TRUE;
}

typedef struct
{
	HANDLE abort;
	Worker* worker;
	BOOL stayed_alive;
} AbortActive;
static DWORD WINAPI abort_active(void* arg)
{
	AbortActive* a = arg;
	WaitForSingleObject(announce_entered, INFINITE);
	SetEvent(a->abort);
	WaitForSingleObject(abortObserved, INFINITE);
	/* The call still borrows the worker's stack while executing. */
	a->stayed_alive = (WaitForSingleObject(a->worker->done, 0) == WAIT_TIMEOUT);
	SetEvent(announce_release);
	return 0;
}

static BOOL run_tests(wfClipboard* clipboard, CliprdrClientContext* channel, HANDLE abort)
{
	Worker w = { .clipboard = clipboard, .channel = channel };
	HANDLE thread = nullptr;
	for (size_t i = 0; i < 100; i++)
	{
		CHECK(start(&w, &thread));
		wlf_cliprdr_dispatch(clipboard);
		CHECK(join(&w, thread));
		CHECK(w.result == CHANNEL_RC_OK);
		CHECK(clipboard->numServerFormats == 1);
		CHECK(strcmp(clipboard->serverFormats[0].formatName, "HTML Format") == 0);
	}
	CHECK(atomic_load(&announcements) == 100);
	CHECK(atomic_load(&responses) == 100);
	CHECK(WaitForSingleObject(wlf_cliprdr_get_event_handle(clipboard), 0) == WAIT_TIMEOUT);

	/* Forward UWAC errors without a positive format-list response. */
	announce_result = UWAC_ERROR_INTERNAL;
	CHECK(start(&w, &thread));
	wlf_cliprdr_dispatch(clipboard);
	CHECK(join(&w, thread));
	CHECK(w.result == ERROR_INTERNAL_ERROR);
	CHECK(atomic_load(&responses) == 100);
	announce_result = UWAC_SUCCESS;

	/* Disconnect before the main loop services the queued call. */
	CHECK(start(&w, &thread));
	SetEvent(abort);
	CHECK(join(&w, thread));
	CHECK(w.result == ERROR_OPERATION_ABORTED);
	wlf_cliprdr_dispatch(clipboard);
	CHECK(atomic_load(&announcements) == 101);
	ResetEvent(abort);

	/* Abort during a call: stack arguments cannot be released prematurely. */
	announce_entered = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	announce_release = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	abortObserved = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	CHECK(announce_entered && announce_release && abortObserved);
	CHECK(start(&w, &thread));
	AbortActive a = { .abort = abort, .worker = &w };
	HANDLE canceller = CreateThread(nullptr, 0, abort_active, &a, 0, nullptr);
	CHECK(canceller);
	wlf_cliprdr_dispatch(clipboard);
	CHECK(join(&w, thread));
	CHECK(WaitForSingleObject(canceller, 5000) == WAIT_OBJECT_0);
	CHECK(a.stayed_alive);
	CloseHandle(canceller);
	CloseHandle(announce_entered);
	CloseHandle(announce_release);
	CloseHandle(abortObserved);
	abortObserved = nullptr;
	announce_entered = announce_release = nullptr;
	ResetEvent(abort);

	/* Main-thread invocation (channel threading disabled) must not self-wait. */
	const CLIPRDR_FORMAT fmt = { .formatId = CF_UNICODETEXT };
	const CLIPRDR_FORMAT_LIST list = { .numFormats = 1, .formats = (CLIPRDR_FORMAT*)&fmt };
	CHECK(channel->ServerFormatList(channel, &list) == CHANNEL_RC_OK);

	w.read = TRUE;
	CHECK(start(&w, &thread));
	wlf_cliprdr_dispatch(clipboard);
	CHECK(write_fd >= 0);
	/* Data delivery is deliberately split and larger than one read allocation. */
	char payload[32768];
	memset(payload, 'x', sizeof(payload));
	for (size_t i = 0; i < sizeof(payload); i += 512)
		CHECK(write(write_fd, payload + i, 512) == 512);
	close(write_fd);
	write_fd = -1;
	CHECK(join(&w, thread));
	CHECK(w.data && w.size == sizeof(payload) + 1);
	CHECK(memcmp(w.data, payload, sizeof(payload)) == 0);
	CHECK(w.data[sizeof(payload)] == 0);
	free(w.data);

	/* Empty transfer still has the legacy NUL terminator. */
	CHECK(start(&w, &thread));
	wlf_cliprdr_dispatch(clipboard);
	close(write_fd);
	write_fd = -1;
	CHECK(join(&w, thread));
	CHECK(w.data && w.size == 1 && w.data[0] == 0);
	free(w.data);

	/* No response/EOF from the local clipboard owner: disconnect releases read. */
	CHECK(start(&w, &thread));
	wlf_cliprdr_dispatch(clipboard);
	CHECK(WaitForSingleObject(w.done, 0) == WAIT_TIMEOUT);
	SetEvent(abort);
	CHECK(join(&w, thread));
	CHECK(!w.data && w.size == 0);
	close(write_fd);
	write_fd = -1;
	ResetEvent(abort);

	fail_open = TRUE;
	CHECK(start(&w, &thread));
	wlf_cliprdr_dispatch(clipboard);
	CHECK(join(&w, thread));
	CHECK(!w.data && w.size == 0);
	fail_open = FALSE;

	/* Exercise the real incoming callback as well: it must release the WinPR
	 * clipboard lock before waiting for display-thread work. This checks dispatch
	 * and the complete UTF-16 response payload. */
	w.read = FALSE;
	w.request_data = TRUE;
	CHECK(start(&w, &thread));
	ClipboardLock(clipboard->system);
	wlf_cliprdr_dispatch(clipboard);
	ClipboardUnlock(clipboard->system);
	CHECK(write(write_fd, "hello", 5) == 5);
	close(write_fd);
	write_fd = -1;
	CHECK(join(&w, thread));
	CHECK(w.result == CHANNEL_RC_OK && atomic_load(&data_responses) == 1);
	return TRUE;
}

int main(void)
{
	owner = GetCurrentThreadId();
	freerdp* instance = freerdp_new();
	if (!instance)
		return 1;
	instance->ContextSize = sizeof(wlfContext);
	if (!freerdp_context_new(instance))
		return 1;
	wlfContext* context = (wlfContext*)instance->context;
	wfClipboard* clipboard = wlf_clipboard_new(context);
	if (!clipboard)
		return 1;
	/* No real seat is dereferenced by the UWAC doubles. */
	clipboard->seat = (UwacSeat*)clipboard;
	CliprdrClientContext channel = { .ClientFormatListResponse = format_response,
		                             .ClientFormatDataResponse = data_response,
		                             .rdpcontext = instance->context };
	if (!wlf_cliprdr_init(clipboard, &channel))
		return 1;
	const BOOL ok = run_tests(clipboard, &channel, freerdp_abort_event(instance->context));
	wlf_cliprdr_uninit(clipboard, &channel);
	wlf_clipboard_free(clipboard);
	freerdp_context_free(instance);
	freerdp_free(instance);
	if (!ok)
		return 1;
	puts("PASS: display-thread ownership, repeated offers, error propagation, pending/active "
	     "cancellation, inline calls, pipe transfer, empty transfer, stalled-owner cancellation");
	return 0;
}

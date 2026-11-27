/*
 * UWAC clipboard synchronization regression.
 * Licensed under the Apache License, Version 2.0.
 * Wayland calls are deterministic test doubles; execute the real UWAC code.
 */
#include "../uwac-priv.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* Private UWAC helpers not used by the tested paths. Unexpected calls fail. */
UwacErrorHandler uwacErrorHandler = NULL;
UwacEvent* UwacDisplayNewEvent(UwacDisplay* display, int type)
{
	(void)display;
	(void)type;
	abort();
}
void* xrealloc(void* data, size_t size)
{
	return realloc(data, size);
}

static const struct wl_callback_listener* listener;
static void* listener_data;
static int dispatched;
static int destroyed;
static int roundtrips;
static int received;
static int write_fd;
static int flush_result;
static bool dispatch_fail;
static bool sync_fail;
static bool listener_fail;
static bool roundtrip_fail;
static uint32_t callback_serial;
static uint32_t selection_serial;
static struct wl_callback* callback_token = (struct wl_callback*)&listener;

static struct wl_callback* test_sync(struct wl_display* display)
{
	(void)display;
	return sync_fail ? NULL : callback_token;
}
static int test_add_listener(struct wl_callback* callback,
                             const struct wl_callback_listener* handlers, void* data)
{
	(void)callback;
	listener = handlers;
	listener_data = data;
	return listener_fail ? -1 : 0;
}
static void test_destroy(struct wl_callback* callback)
{
	(void)callback;
	destroyed++;
	listener = NULL;
	listener_data = NULL;
}
static int test_dispatch(struct wl_display* display)
{
	(void)display;
	dispatched++;
	if (dispatch_fail)
		return -1;
	listener->done(listener_data, callback_token, callback_serial);
	return 1;
}
static int test_roundtrip(struct wl_display* display)
{
	(void)display;
	roundtrips++;
	return roundtrip_fail ? -1 : 1;
}
static void test_selection(struct wl_data_device* device, struct wl_data_source* source,
                           uint32_t serial)
{
	(void)device;
	(void)source;
	selection_serial = serial;
}
static int test_flush(struct wl_display* display)
{
	(void)display;
	return flush_result;
}
static void test_receive(struct wl_data_offer* offer, const char* mime, int fd)
{
	(void)offer;
	(void)mime;
	received++;
	write_fd = fd;
	const char bytes[] = "hello";
	if (write(fd, bytes, sizeof(bytes)) != sizeof(bytes))
		abort();
}

#define wl_display_sync test_sync
#define wl_callback_add_listener test_add_listener
#define wl_callback_destroy test_destroy
#define wl_display_dispatch test_dispatch
#define wl_display_roundtrip test_roundtrip
#define wl_data_device_set_selection test_selection
#define wl_display_flush test_flush
#define wl_data_offer_receive test_receive
#include "../uwac-clipboard.c"

#define CHECK(x)                                                    \
	do                                                              \
	{                                                               \
		if (!(x))                                                   \
		{                                                           \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
			return 1;                                               \
		}                                                           \
	} while (0)

int main(void)
{
	UwacDisplay display = { 0 };
	UwacSeat seat = { .display = &display };
	seat.offer = (struct wl_data_offer*)&seat;
	/* A sync serial may be zero. Completion is separate from its value. */
	CHECK(UwacClipboardOfferAnnounce(&seat, NULL, NULL, NULL) == UWAC_SUCCESS);
	CHECK(dispatched == 1 && destroyed == 1 && roundtrips == 1);
	CHECK(selection_serial == 0 && !seat.ignore_announcement);
	callback_serial = 123;
	CHECK(UwacClipboardOfferAnnounce(&seat, NULL, NULL, NULL) == UWAC_SUCCESS);
	CHECK(selection_serial == 123 && destroyed == 2);

	dispatch_fail = true;
	CHECK(UwacClipboardOfferAnnounce(&seat, NULL, NULL, NULL) == UWAC_ERROR_INTERNAL);
	CHECK(destroyed == 3 && !seat.ignore_announcement && roundtrips == 2);
	dispatch_fail = false;

	sync_fail = true;
	CHECK(UwacClipboardOfferAnnounce(&seat, NULL, NULL, NULL) == UWAC_ERROR_INTERNAL);
	CHECK(destroyed == 3 && !seat.ignore_announcement);
	sync_fail = false;
	listener_fail = true;
	CHECK(UwacClipboardOfferAnnounce(&seat, NULL, NULL, NULL) == UWAC_ERROR_INTERNAL);
	CHECK(destroyed == 4 && !seat.ignore_announcement);
	listener_fail = false;

	roundtrip_fail = true;
	CHECK(UwacClipboardOfferAnnounce(&seat, NULL, NULL, NULL) == UWAC_ERROR_INTERNAL);
	CHECK(destroyed == 5 && !seat.ignore_announcement);
	roundtrip_fail = false;

	const int old_dispatch = dispatched;
	const int old_roundtrips = roundtrips;
	const int fd = UwacClipboardDataGetFd(&seat, "text/plain");
	CHECK(fd >= 0 && received == 1);
	CHECK(dispatched == old_dispatch && roundtrips == old_roundtrips);
	CHECK(fcntl(fd, F_GETFL) & O_NONBLOCK);
	CHECK(fcntl(fd, F_GETFD) & FD_CLOEXEC);
	char buf[16] = { 0 };
	CHECK(read(fd, buf, sizeof(buf)) == 6);
	CHECK(strcmp(buf, "hello") == 0);
	CHECK(read(fd, buf, sizeof(buf)) == 0);
	close(fd);
	errno = 0;
	CHECK(fcntl(write_fd, F_GETFD) == -1 && errno == EBADF);

	flush_result = -1;
	errno = EPIPE;
	CHECK(UwacClipboardDataGetFd(&seat, "text/plain") == -1);
	CHECK(fcntl(write_fd, F_GETFD) == -1);
	/* Backpressure leaves the request queued for the display loop to flush. */
	errno = EAGAIN;
	const int pending = UwacClipboardDataGetFd(&seat, "text/plain");
	CHECK(pending >= 0);
	close(pending);
	CHECK(UwacClipboardDataGetFd(NULL, "text/plain") == -1);
	CHECK(UwacClipboardDataGetFd(&seat, NULL) == -1);
	seat.offer = NULL;
	CHECK(UwacClipboardDataGetFd(&seat, "text/plain") == -1);
	puts("PASS: zero/nonzero sync serial, callback cleanup, disconnect/listener/sync/roundtrip "
	     "errors, receive without dispatch, fd flags/EOF/error cleanup");
	return 0;
}

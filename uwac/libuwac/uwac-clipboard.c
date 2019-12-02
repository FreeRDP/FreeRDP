/*
 * Copyright © 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright © 2018 Thincast Technologies GmbH
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
#include "uwac-priv.h"
#include "uwac-utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

/* paste */
static void data_offer_offer(void* data, struct wl_data_offer* data_offer,
                             const char* offered_mime_type)
{
	UwacSeat* seat = (UwacSeat*)data;

	assert(seat);
	if (!seat->ignore_announcement)
	{
		UwacClipboardEvent* event =
		    (UwacClipboardEvent*)UwacDisplayNewEvent(seat->display, UWAC_EVENT_CLIPBOARD_OFFER);

		if (!event)
		{
			assert(uwacErrorHandler(seat->display, UWAC_ERROR_INTERNAL,
			                        "failed to allocate a clipboard event\n"));
		}
		else
		{
			event->seat = seat;
			sprintf_s(event->mime, sizeof(event->mime), "%s", offered_mime_type);
		}
	}
}

static const struct wl_data_offer_listener data_offer_listener = { .offer = data_offer_offer };

static void data_device_data_offer(void* data, struct wl_data_device* data_device,
                                   struct wl_data_offer* data_offer)
{
	UwacSeat* seat = (UwacSeat*)data;

	assert(seat);
	if (!seat->ignore_announcement)
	{
		UwacClipboardEvent* event =
		    (UwacClipboardEvent*)UwacDisplayNewEvent(seat->display, UWAC_EVENT_CLIPBOARD_SELECT);

		if (!event)
		{
			assert(uwacErrorHandler(seat->display, UWAC_ERROR_INTERNAL,
			                        "failed to allocate a close event\n"));
		}
		else
			event->seat = seat;

		wl_data_offer_add_listener(data_offer, &data_offer_listener, data);
		seat->offer = data_offer;
	}
	else
		seat->offer = NULL;
}

static void data_device_selection(void* data, struct wl_data_device* data_device,
                                  struct wl_data_offer* data_offer)
{
}

static const struct wl_data_device_listener data_device_listener = {
	.data_offer = data_device_data_offer, .selection = data_device_selection
};

/* copy */
static void data_source_target_handler(void* data, struct wl_data_source* data_source,
                                       const char* mime_type)
{
}

static void data_source_send_handler(void* data, struct wl_data_source* data_source,
                                     const char* mime_type, int fd)
{
	UwacSeat* seat = (UwacSeat*)data;
	seat->transfer_data(seat, seat->data_context, mime_type, fd);
}

static void data_source_cancelled_handler(void* data, struct wl_data_source* data_source)
{
	UwacSeat* seat = (UwacSeat*)data;
	seat->cancel_data(seat, seat->data_context);
}

static const struct wl_data_source_listener data_source_listener = {
	.target = data_source_target_handler,
	.send = data_source_send_handler,
	.cancelled = data_source_cancelled_handler
};

static void UwacRegisterDeviceListener(UwacSeat* s)
{
	wl_data_device_add_listener(s->data_device, &data_device_listener, s);
}

static UwacReturnCode UwacCreateDataSource(UwacSeat* s)
{
	if (!s)
		return UWAC_ERROR_INTERNAL;

	s->data_source = wl_data_device_manager_create_data_source(s->display->data_device_manager);
	wl_data_source_add_listener(s->data_source, &data_source_listener, s);
	return UWAC_SUCCESS;
}

UwacReturnCode UwacSeatRegisterClipboard(UwacSeat* s)
{
	UwacReturnCode rc;
	UwacClipboardEvent* event;

	if (!s)
		return UWAC_ERROR_INTERNAL;

	if (!s->display->data_device_manager || !s->data_device)
		return UWAC_NOT_ENOUGH_RESOURCES;

	UwacRegisterDeviceListener(s);

	rc = UwacCreateDataSource(s);

	if (rc != UWAC_SUCCESS)
		return rc;

	event = (UwacClipboardEvent*)UwacDisplayNewEvent(s->display, UWAC_EVENT_CLIPBOARD_AVAILABLE);

	if (!event)
	{
		assert(uwacErrorHandler(s->display, UWAC_ERROR_INTERNAL,
		                        "failed to allocate a clipboard event\n"));
		return UWAC_ERROR_INTERNAL;
	}

	event->seat = s;
	return UWAC_SUCCESS;
}

UwacReturnCode UwacClipboardOfferDestroy(UwacSeat* seat)
{
	if (!seat)
		return UWAC_ERROR_INTERNAL;

	if (seat->data_source)
		wl_data_source_destroy(seat->data_source);

	return UwacCreateDataSource(seat);
}

UwacReturnCode UwacClipboardOfferCreate(UwacSeat* seat, const char* mime)
{
	if (!seat || !mime)
		return UWAC_ERROR_INTERNAL;

	wl_data_source_offer(seat->data_source, mime);
	return UWAC_SUCCESS;
}

static void callback_done(void* data, struct wl_callback* callback, uint32_t serial)
{
	*(uint32_t*)data = serial;
}

static const struct wl_callback_listener callback_listener = { .done = callback_done };

uint32_t get_serial(UwacSeat* s)
{
	struct wl_callback* callback;
	uint32_t serial = 0;
	callback = wl_display_sync(s->display->display);
	wl_callback_add_listener(callback, &callback_listener, &serial);

	while (serial == 0)
	{
		wl_display_dispatch(s->display->display);
	}

	return serial;
}

UwacReturnCode UwacClipboardOfferAnnounce(UwacSeat* seat, void* context,
                                          UwacDataTransferHandler transfer,
                                          UwacCancelDataTransferHandler cancel)
{
	if (!seat)
		return UWAC_ERROR_INTERNAL;

	seat->data_context = context;
	seat->transfer_data = transfer;
	seat->cancel_data = cancel;
	seat->ignore_announcement = true;
	wl_data_device_set_selection(seat->data_device, seat->data_source, get_serial(seat));
	wl_display_roundtrip(seat->display->display);
	seat->ignore_announcement = false;
	return UWAC_SUCCESS;
}

void* UwacClipboardDataGet(UwacSeat* seat, const char* mime, size_t* size)
{
	ssize_t r = 0;
	size_t alloc = 0;
	size_t pos = 0;
	char* data = NULL;
	int pipefd[2];

	if (!seat || !mime || !size || !seat->offer)
		return NULL;

	if (pipe(pipefd) != 0)
		return NULL;

	wl_data_offer_receive(seat->offer, mime, pipefd[1]);
	close(pipefd[1]);
	wl_display_roundtrip(seat->display->display);
	wl_display_flush(seat->display->display);

	do
	{
		void* tmp;
		alloc += 1024;
		tmp = xrealloc(data, alloc);
		if (!tmp)
		{
			free(data);
			close(pipefd[0]);
			return NULL;
		}

		data = tmp;
		r = read(pipefd[0], &data[pos], alloc - pos);
		if (r > 0)
			pos += r;
		if (r < 0)
		{
			free(data);
			close(pipefd[0]);
			return NULL;
		}
	} while (r > 0);

	close(pipefd[0]);
	close(pipefd[1]);

	*size = pos + 1;
	return data;
}

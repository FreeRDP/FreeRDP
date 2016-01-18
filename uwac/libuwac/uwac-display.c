/*
 * Copyright © 2014 David FORT <contact@hardening-consulting.com>
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
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>

#include "uwac-os.h"

#define TARGET_COMPOSITOR_INTERFACE 3
#define TARGET_SHM_INTERFACE 1
#define TARGET_SHELL_INTERFACE 1
#define TARGET_DDM_INTERFACE 1
#define TARGET_SEAT_INTERFACE 5
#define TARGET_XDG_VERSION 5 /* The version of xdg-shell that we implement */

static const char *event_names[] = {
	"new seat",
	"removed seat",
	"new output",
	"configure",
	"pointer enter",
	"pointer leave",
	"pointer motion",
	"pointer buttons",
	"pointer axis",
	"keyboard enter",
	"key",
	"touch frame begin",
	"touch up",
	"touch down",
	"touch motion",
	"touch cancel",
	"touch frame end",
	"frame done",
	"close",
	NULL
};

bool uwac_default_error_handler(UwacDisplay *display, UwacReturnCode code, const char *msg, ...) {
	va_list args;
	va_start(args, msg);

	vfprintf(stderr, "%s", args);
	return false;
}

UwacErrorHandler uwacErrorHandler = uwac_default_error_handler;

void UwacInstallErrorHandler(UwacErrorHandler handler) {
	if (handler)
		uwacErrorHandler = handler;
	else
		uwacErrorHandler = uwac_default_error_handler;
}


static void cb_shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	UwacDisplay *d = data;

	if (format == WL_SHM_FORMAT_RGB565)
		d->has_rgb565 = true;

	d->shm_formats_nb++;
	d->shm_formats = xrealloc((void *)d->shm_formats, sizeof(enum wl_shm_format) * d->shm_formats_nb);
	d->shm_formats[d->shm_formats_nb - 1] = format;
}


struct wl_shm_listener shm_listener = {
	cb_shm_format
};

static void xdg_shell_ping(void *data, struct xdg_shell *shell, uint32_t serial)
{
	xdg_shell_pong(shell, serial);
}

static const struct xdg_shell_listener xdg_shell_listener = {
	xdg_shell_ping,
};

#ifdef BUILD_FULLSCREEN_SHELL
static void fullscreen_capability(void *data, struct _wl_fullscreen_shell *_wl_fullscreen_shell,
			   uint32_t capabilty)
{
}

static const struct _wl_fullscreen_shell_listener fullscreen_shell_listener = {
		fullscreen_capability,
};
#endif


static UwacSeat *display_destroy_seat(UwacDisplay *d, uint32_t name)
{
	UwacSeat *seat;

	wl_list_for_each(seat, &d->seats, link) {
		if (seat->seat_id == name) {
			UwacSeatDestroy(seat);
			return seat;
		}
	}

	return NULL;
}

static void registry_handle_global(void *data, struct wl_registry *registry, uint32_t id,
		       const char *interface, uint32_t version)
{
	UwacDisplay *d = data;
	UwacGlobal *global;

	global = xmalloc(sizeof *global);
	global->name = id;
	global->interface = xstrdup(interface);
	global->version = version;
	wl_list_insert(d->globals.prev, &global->link);

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, min(TARGET_COMPOSITOR_INTERFACE, version));
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry, id, &wl_shm_interface, min(TARGET_SHM_INTERFACE, version));
		wl_shm_add_listener(d->shm, &shm_listener, d);
	} else if (strcmp(interface, "wl_output") == 0) {
		UwacOutput *output;
		UwacOutputNewEvent *ev;

		output = UwacCreateOutput(d, id, version);
		if (!output) {
			assert(uwacErrorHandler(d, UWAC_ERROR_NOMEMORY, "unable to create output\n"));
			return;
		}

		ev = (UwacOutputNewEvent *)UwacDisplayNewEvent(d, UWAC_EVENT_NEW_OUTPUT);
		if (ev)
			ev->output = output;

	} else if (strcmp(interface, "wl_seat") == 0) {
		UwacSeatNewEvent *ev;
		UwacSeat *seat;

		seat = UwacSeatNew(d, id, min(version, TARGET_SEAT_INTERFACE));
		if (!seat) {
			assert(uwacErrorHandler(d, UWAC_ERROR_NOMEMORY, "unable to create new seat\n"));
			return;
		}

		ev = (UwacSeatNewEvent *)UwacDisplayNewEvent(d, UWAC_EVENT_NEW_SEAT);
		if (!ev) {
			assert(uwacErrorHandler(d, UWAC_ERROR_NOMEMORY, "unable to create new seat event\n"));
			return;
		}

		ev->seat = seat;
	} else if (strcmp(interface, "wl_data_device_manager") == 0) {
		d->data_device_manager = wl_registry_bind(registry, id, &wl_data_device_manager_interface, min(TARGET_DDM_INTERFACE, version));
	} else if (strcmp(interface, "wl_shell") == 0) {
		d->shell = wl_registry_bind(registry, id, &wl_shell_interface, min(TARGET_SHELL_INTERFACE, version));
	} else if (strcmp(interface, "xdg_shell") == 0) {
		d->xdg_shell = wl_registry_bind(registry, id, &xdg_shell_interface, 1);
		xdg_shell_use_unstable_version(d->xdg_shell, TARGET_XDG_VERSION);
		xdg_shell_add_listener(d->xdg_shell, &xdg_shell_listener, d);
#if BUILD_IVI
	} else if (strcmp(interface, "ivi_application") == 0) {
		d->ivi_application = wl_registry_bind(registry, id, &ivi_application_interface, 1);
#endif
#if BUILD_FULLSCREEN_SHELL
	} else if (strcmp(interface, "_wl_fullscreen_shell") == 0) {
		d->fullscreen_shell = wl_registry_bind(registry, id, &_wl_fullscreen_shell_interface, 1);
		_wl_fullscreen_shell_add_listener(d->fullscreen_shell, &fullscreen_shell_listener, d);
#endif
#if 0
	} else if (strcmp(interface, "text_cursor_position") == 0) {
		d->text_cursor_position = wl_registry_bind(registry, id, &text_cursor_position_interface, 1);
	} else if (strcmp(interface, "workspace_manager") == 0) {
		//init_workspace_manager(d, id);
	} else if (strcmp(interface, "wl_subcompositor") == 0) {
		d->subcompositor = wl_registry_bind(registry, id, &wl_subcompositor_interface, 1);
#endif
	}
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	UwacDisplay *d = data;
	UwacGlobal *global;
	UwacGlobal *tmp;

	wl_list_for_each_safe(global, tmp, &d->globals, link) {
		if (global->name != name)
			continue;

#if 0
		if (strcmp(global->interface, "wl_output") == 0)
			display_destroy_output(d, name);
#endif

		if (strcmp(global->interface, "wl_seat") == 0) {
			UwacSeatRemovedEvent *ev;
			UwacSeat *seat;

			seat = display_destroy_seat(d, name);
			ev = (UwacSeatRemovedEvent *)UwacDisplayNewEvent(d, UWAC_EVENT_REMOVED_SEAT);
			if (ev)
				ev->seat = seat;
		}


		wl_list_remove(&global->link);
		free(global->interface);
		free(global);
	}
}

void UwacDestroyGlobal(UwacGlobal *global) {
	free(global->interface);
	wl_list_remove(&global->link);
	free(global);
}

void *display_bind(UwacDisplay *display, uint32_t name, const struct wl_interface *interface, uint32_t version) {
	return wl_registry_bind(display->registry, name, interface, version);
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};


int UwacDisplayWatchFd(UwacDisplay *display, int fd, uint32_t events, UwacTask *task) {
	struct epoll_event ep;

	ep.events = events;
	ep.data.ptr = task;
	return epoll_ctl(display->epoll_fd, EPOLL_CTL_ADD, fd, &ep);
}

void UwacDisplayUnwatchFd(UwacDisplay *display, int fd) {
	epoll_ctl(display->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

static void display_exit(UwacDisplay *display) {
	display->running = false;
}

static void display_dispatch_events(UwacTask *task, uint32_t events)
{
	UwacDisplay *display = container_of(task, UwacDisplay, dispatch_fd_task);
	struct epoll_event ep;
	int ret;

	display->display_fd_events = events;

	if ((events & EPOLLERR) || (events & EPOLLHUP)) {
		display_exit(display);
		return;
	}

	if (events & EPOLLIN) {
		ret = wl_display_dispatch(display->display);
		if (ret == -1) {
			display_exit(display);
			return;
		}
	}

	if (events & EPOLLOUT) {
		ret = wl_display_flush(display->display);
		if (ret == 0) {
			ep.events = EPOLLIN | EPOLLERR | EPOLLHUP;
			ep.data.ptr = &display->dispatch_fd_task;
			epoll_ctl(display->epoll_fd, EPOLL_CTL_MOD, display->display_fd, &ep);
		} else if (ret == -1 && errno != EAGAIN) {
			display_exit(display);
			return;
		}
	}
}


UwacDisplay *UwacOpenDisplay(const char *name, UwacReturnCode *err) {
	UwacDisplay *ret;

	ret = (UwacDisplay *)calloc(1, sizeof(*ret));
	if (!ret) {
		*err = UWAC_ERROR_NOMEMORY;
		return NULL;
	}

	wl_list_init(&ret->globals);
	wl_list_init(&ret->seats);
	wl_list_init(&ret->outputs);
	wl_list_init(&ret->windows);

	ret->display = wl_display_connect(name);
	if (ret->display == NULL) {
		fprintf(stderr, "failed to connect to Wayland display %s: %m\n", name);
		*err = UWAC_ERROR_UNABLE_TO_CONNECT;
		goto out_free;
	}

	ret->epoll_fd = uwac_os_epoll_create_cloexec();
	if (ret->epoll_fd < 0) {
		*err = UWAC_NOT_ENOUGH_RESOURCES;
		goto out_disconnect;
	}

	ret->display_fd = wl_display_get_fd(ret->display);

	ret->registry = wl_display_get_registry(ret->display);
	if (!ret->registry) {
		*err = UWAC_ERROR_NOMEMORY;
		goto out_close_epoll;
	}

	wl_registry_add_listener(ret->registry, &registry_listener, ret);

	if ((wl_display_roundtrip(ret->display) < 0) || (wl_display_roundtrip(ret->display) < 0)) {
		uwacErrorHandler(ret, UWAC_ERROR_UNABLE_TO_CONNECT, "Failed to process Wayland connection: %m\n");
		*err = UWAC_ERROR_UNABLE_TO_CONNECT;
		goto out_free_registry;
	}

	ret->dispatch_fd_task.run = display_dispatch_events;
	if (UwacDisplayWatchFd(ret, ret->display_fd, EPOLLIN | EPOLLERR | EPOLLHUP, &ret->dispatch_fd_task) < 0) {
		uwacErrorHandler(ret, UWAC_ERROR_INTERNAL, "unable to watch display fd: %m\n");
		*err = UWAC_ERROR_INTERNAL;
		goto out_free_registry;
	}

	ret->running = true;
	ret->last_error = *err = UWAC_SUCCESS;
	return ret;

out_free_registry:
	wl_registry_destroy(ret->registry);
out_close_epoll:
	close(ret->epoll_fd);
out_disconnect:
	wl_display_disconnect(ret->display);
out_free:
	free(ret);
	return NULL;
}

int UwacDisplayDispatch(UwacDisplay *display, int timeout) {
	int ret, count, i;
	UwacTask *task;
	struct epoll_event ep[16];

	wl_display_dispatch_pending(display->display);

	if (!display->running)
		return 0;

	ret = wl_display_flush(display->display);
	if (ret < 0 && errno == EAGAIN) {
		ep[0].events = (EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP);
		ep[0].data.ptr = &display->dispatch_fd_task;

		epoll_ctl(display->epoll_fd, EPOLL_CTL_MOD, display->display_fd, &ep[0]);
	} else if (ret < 0) {
		return -1;
	}

	count = epoll_wait(display->epoll_fd, ep, ARRAY_LENGTH(ep), timeout);
	for (i = 0; i < count; i++) {
		task = ep[i].data.ptr;
		task->run(task, ep[i].events);
	}

	return 1;
}



UwacReturnCode UwacDisplayGetLastError(const UwacDisplay *display) {
	return display->last_error;
}

UwacReturnCode UwacCloseDisplay(UwacDisplay **pdisplay) {
	UwacDisplay *display;
	UwacSeat *seat, *tmpSeat;
	UwacWindow *window, *tmpWindow;
	UwacOutput *output, *tmpOutput;
	UwacGlobal *global, *tmpGlobal;

	assert(pdisplay);
	display = *pdisplay;
	if (!display)
		return UWAC_ERROR_INVALID_DISPLAY;

	/* destroy windows */
	wl_list_for_each_safe(window, tmpWindow, &display->windows, link) {
		UwacDestroyWindow(&window);
	}

	/* destroy seats */
	wl_list_for_each_safe(seat, tmpSeat, &display->seats, link) {
		UwacSeatDestroy(seat);
	}

	/* destroy output */
	wl_list_for_each_safe(output, tmpOutput, &display->outputs, link) {
		UwacDestroyOutput(output);
	}

	/* destroy globals */
	wl_list_for_each_safe(global, tmpGlobal, &display->globals, link) {
		UwacDestroyGlobal(global);
	}

	if (display->compositor)
		wl_compositor_destroy(display->compositor);
#ifdef BUILD_FULLSCREEN_SHELL
	if (display->fullscreen_shell)
		_wl_fullscreen_shell_destroy(display->fullscreen_shell);
#endif
#ifdef BUILD_IVI
	if (display->ivi_application)
		ivi_application_destroy(display->ivi_application);
#endif
	if (display->xdg_shell)
		xdg_shell_destroy(display->xdg_shell);
	if (display->shell)
		wl_shell_destroy(display->shell);
	if (display->shm)
		wl_shm_destroy(display->shm);
	if (display->subcompositor)
		wl_subcompositor_destroy(display->subcompositor);
	if (display->data_device_manager)
		wl_data_device_manager_destroy(display->data_device_manager);

	free(display->shm_formats);
	wl_registry_destroy(display->registry);

	close(display->epoll_fd);

	wl_display_disconnect(display->display);

	/* cleanup the event queue */
	while (display->push_queue) {
		UwacEventListItem *item = display->push_queue;

		display->push_queue = item->tail;
		free(item);
	}

	free(display);
	*pdisplay = NULL;
	return UWAC_SUCCESS;
}

int UwacDisplayGetFd(UwacDisplay *display) {
	return display->epoll_fd;
}

static const char *errorStrings[] = {
	"success",
	"out of memory error",
	"unable to connect to wayland display",
	"invalid UWAC display",
	"not enough resources",
	"timed out",
	"not found",
	"closed connection",

	"internal error",
};

const char *UwacErrorString(UwacReturnCode error) {
	if (error < 0 || error >= UWAC_ERROR_LAST)
		return "invalid error code";

	return errorStrings[error];
}

UwacReturnCode UwacDisplayQueryInterfaceVersion(const UwacDisplay *display, const char *name, uint32_t *version) {
	const UwacGlobal *global;

	if (!display)
		return UWAC_ERROR_INVALID_DISPLAY;

	wl_list_for_each(global, &display->globals, link) {
		if (strcmp(global->interface, name) == 0) {
			if (version)
				*version = global->version;
			return UWAC_SUCCESS;
		}
	}

	return UWAC_NOT_FOUND;
}

uint32_t UwacDisplayQueryGetNbShmFormats(UwacDisplay *display) {
	if (!display) {
		display->last_error = UWAC_ERROR_INVALID_DISPLAY;
		return 0;
	}

	if (!display->shm) {
		display->last_error = UWAC_NOT_FOUND;
		return 0;
	}

	display->last_error = UWAC_SUCCESS;
	return display->shm_formats_nb;
}


UwacReturnCode UwacDisplayQueryShmFormats(const UwacDisplay *display, enum wl_shm_format *formats, int formats_size, int *filled) {
	if (!display)
		return UWAC_ERROR_INVALID_DISPLAY;

	*filled = min(display->shm_formats_nb, formats_size);
	memcpy(formats, (const void *)display->shm_formats, *filled * sizeof(enum wl_shm_format));

	return UWAC_SUCCESS;
}

uint32_t UwacDisplayGetNbOutputs(UwacDisplay *display) {
	return wl_list_length(&display->outputs);
}

UwacOutput *UwacDisplayGetOutput(UwacDisplay *display, int index) {
	struct wl_list *l;
	int i;

	for (i = 0, l = &display->outputs; l && i < index; i++, l = l->next)
		;

	if (!l) {
		display->last_error = UWAC_NOT_FOUND;
		return NULL;
	}

	display->last_error = UWAC_SUCCESS;
	return container_of(l, UwacOutput, link);
}

UwacReturnCode UwacOutputGetResolution(UwacOutput *output, UwacSize *resolution) {
	*resolution = output->resolution;
	return UWAC_SUCCESS;
}


UwacEvent *UwacDisplayNewEvent(UwacDisplay *display, int type) {
	UwacEventListItem *ret;

	if (!display) {
		display->last_error = UWAC_ERROR_INVALID_DISPLAY;
		return 0;
	}

	ret = zalloc(sizeof(UwacEventListItem));
	if (!ret) {
		assert(uwacErrorHandler(display, UWAC_ERROR_NOMEMORY, "unable to allocate a '%s' event", event_names[type]));
		display->last_error = UWAC_ERROR_NOMEMORY;
		return 0;
	}

	ret->event.type = type;
	ret->tail = display->push_queue;
	if (ret->tail)
		ret->tail->head = ret;
	else
		display->pop_queue = ret;
	display->push_queue = ret;
	return &ret->event;
}

bool UwacHasEvent(UwacDisplay *display) {
	return display->pop_queue != NULL;
}


UwacReturnCode UwacNextEvent(UwacDisplay *display, UwacEvent *event) {
	UwacEventListItem *prevItem;
	int ret;

	if (!display)
		return UWAC_ERROR_INVALID_DISPLAY;

	while (!display->pop_queue) {
		ret = UwacDisplayDispatch(display, 1 * 1000);
		if (ret < 0)
			return UWAC_ERROR_INTERNAL;
		else if (ret == 0)
			return UWAC_ERROR_CLOSED;
	}

	prevItem = display->pop_queue->head;
	*event = display->pop_queue->event;
	free(display->pop_queue);
	display->pop_queue = prevItem;
	if (prevItem)
		prevItem->tail = NULL;
	else
		display->push_queue = NULL;
	return UWAC_SUCCESS;
}

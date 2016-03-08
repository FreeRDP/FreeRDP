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

#ifndef __UWAC_PRIV_H_
#define __UWAC_PRIV_H_

#include "config.h"

#include <stdbool.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#ifdef BUILD_IVI
#include "ivi-application-client-protocol.h"
#endif
#ifdef BUILD_FULLSCREEN_SHELL
#include "fullscreen-shell-client-protocol.h"
#endif

#ifdef HAVE_PIXMAN_REGION
#include <pixman-1/pixman.h>
#else
#include <freerdp/codec/region.h>
#endif

#include <xkbcommon/xkbcommon.h>

#include <uwac/uwac.h>


extern UwacErrorHandler uwacErrorHandler;

typedef struct uwac_task UwacTask;

/** @brief */
struct uwac_task {
	void (*run)(UwacTask *task, uint32_t events);
	struct wl_list link;
};

/** @brief a global registry object */
struct uwac_global {
	uint32_t name;
	char *interface;
	uint32_t version;
	struct wl_list link;
};
typedef struct uwac_global UwacGlobal;

struct uwac_event_list_item;
typedef struct uwac_event_list_item UwacEventListItem;

/** @brief */
struct uwac_event_list_item {
	UwacEvent event;
	UwacEventListItem *tail, *head;
};


/** @brief main connection object to a wayland display */
struct uwac_display {
	struct wl_list globals;

	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_subcompositor *subcompositor;
	struct wl_shell *shell;
	struct xdg_shell *xdg_shell;
#ifdef BUILD_IVI
	struct ivi_application *ivi_application;
#endif
#ifdef BUILD_FULLSCREEN_SHELL
	struct _wl_fullscreen_shell *fullscreen_shell;
#endif

	struct wl_shm *shm;
	enum wl_shm_format *shm_formats;
	uint32_t shm_formats_nb;
	bool has_rgb565;

	struct wl_data_device_manager *data_device_manager;
	struct text_cursor_position *text_cursor_position;
	struct workspace_manager *workspace_manager;

	struct wl_list seats;

	int display_fd;
	UwacReturnCode last_error;
	uint32_t display_fd_events;
	int epoll_fd;
	bool running;
	UwacTask dispatch_fd_task;
	uint32_t serial;

	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor **cursors;

	struct wl_list windows;

	struct wl_list outputs;

	UwacEventListItem *push_queue, *pop_queue;
};

/** @brief an output on a wayland display */
struct uwac_output {
	UwacDisplay *display;

	bool doneNeeded;
	bool doneReceived;

	UwacSize resolution;
	int transform;
	int scale;
	char *make;
	char *model;
	uint32_t server_output_id;
	struct wl_output *output;

	struct wl_list link;
};

/** @brief a seat attached to a wayland display */
struct uwac_seat {
	UwacDisplay *display;
	char *name;
	struct wl_seat *seat;
	uint32_t seat_id;
	uint32_t seat_version;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct wl_touch *touch;
	struct xkb_context *xkb_context;

	struct {
		struct xkb_keymap *keymap;
		struct xkb_state *state;
		xkb_mod_mask_t control_mask;
		xkb_mod_mask_t alt_mask;
		xkb_mod_mask_t shift_mask;
	} xkb;
	uint32_t modifiers;
	int32_t repeat_rate_sec, repeat_rate_nsec;
	int32_t repeat_delay_sec, repeat_delay_nsec;
	uint32_t repeat_sym, repeat_key, repeat_time;

	struct wl_array pressed_keys;

	UwacWindow *pointer_focus;

	UwacWindow *keyboard_focus;

	UwacWindow *touch_focus;
	bool touch_frame_started;

	int repeat_timer_fd;
	UwacTask repeat_task;
	float sx, sy;
	struct wl_list link;
};


/** @brief a buffer used for drawing a surface frame */
struct uwac_buffer {
	bool used;
#ifdef HAVE_PIXMAN_REGION
	pixman_region32_t damage;
#else
	REGION16 damage;
#endif
	struct wl_buffer *wayland_buffer;
	void *data;
};
typedef struct uwac_buffer UwacBuffer;


/** @brief a window */
struct uwac_window {
	UwacDisplay *display;
	int width, height, stride;
	int surfaceStates;
	enum wl_shm_format format;

	int nbuffers;
	UwacBuffer *buffers;

	struct wl_region *opaque_region;
	struct wl_region *input_region;
	struct wl_callback *frame_callback;
	UwacBuffer *drawingBuffer, *pendingBuffer;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct xdg_surface *xdg_surface;
#ifdef BUILD_IVI
	struct ivi_surface *ivi_surface;
#endif
	struct wl_list link;

	uint32_t pointer_enter_serial;
	uint32_t pointer_cursor_serial;
	int pointer_current_cursor;
};


/* in uwa-display.c */
UwacEvent *UwacDisplayNewEvent(UwacDisplay *d, int type);
int UwacDisplayWatchFd(UwacDisplay *display, int fd, uint32_t events, UwacTask *task);


/* in uwac-input.c */
UwacSeat *UwacSeatNew(UwacDisplay *d, uint32_t id, uint32_t version);
void UwacSeatDestroy(UwacSeat *s);

/* in uwac-output.c */
UwacOutput *UwacCreateOutput(UwacDisplay *d, uint32_t id, uint32_t version);
int UwacDestroyOutput(UwacOutput *output);

#endif /* __UWAC_PRIV_H_ */

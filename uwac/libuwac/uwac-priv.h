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

#ifndef UWAC_PRIV_H_
#define UWAC_PRIV_H_

#include <uwac/config.h>

#include <stdbool.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "server-decoration-client-protocol.h"

#ifdef BUILD_IVI
#include "ivi-application-client-protocol.h"
#endif
#ifdef BUILD_FULLSCREEN_SHELL
#include "fullscreen-shell-unstable-v1-client-protocol.h"
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
struct uwac_task
{
	void (*run)(UwacTask* task, uint32_t events);
	struct wl_list link;
};

/** @brief a global registry object */
struct uwac_global
{
	uint32_t name;
	char* interface;
	uint32_t version;
	struct wl_list link;
};
typedef struct uwac_global UwacGlobal;

struct uwac_event_list_item;
typedef struct uwac_event_list_item UwacEventListItem;

/** @brief */
struct uwac_event_list_item
{
	UwacEvent event;
	UwacEventListItem *tail, *head;
};

/** @brief main connection object to a wayland display */
struct uwac_display
{
	struct wl_list globals;

	struct wl_display* display;
	struct wl_registry* registry;
	struct wl_compositor* compositor;
	struct wl_subcompositor* subcompositor;
	struct wl_shell* shell;
	struct xdg_toplevel* xdg_toplevel;
	struct xdg_wm_base* xdg_base;
	struct wl_data_device_manager* devicemanager;
	struct zwp_keyboard_shortcuts_inhibit_manager_v1* keyboard_inhibit_manager;
	struct zxdg_decoration_manager_v1* deco_manager;
	struct org_kde_kwin_server_decoration_manager* kde_deco_manager;
#ifdef BUILD_IVI
	struct ivi_application* ivi_application;
#endif
#ifdef BUILD_FULLSCREEN_SHELL
	struct zwp_fullscreen_shell_v1* fullscreen_shell;
#endif

	struct wl_shm* shm;
	enum wl_shm_format* shm_formats;
	uint32_t shm_formats_nb;
	bool has_rgb565;

	struct wl_data_device_manager* data_device_manager;
	struct text_cursor_position* text_cursor_position;
	struct workspace_manager* workspace_manager;

	struct wl_list seats;

	int display_fd;
	UwacReturnCode last_error;
	uint32_t display_fd_events;
	int epoll_fd;
	bool running;
	UwacTask dispatch_fd_task;
	uint32_t serial;

	struct wl_list windows;

	struct wl_list outputs;

	UwacEventListItem *push_queue, *pop_queue;
};

/** @brief an output on a wayland display */
struct uwac_output
{
	UwacDisplay* display;

	bool doneNeeded;
	bool doneReceived;

	UwacPosition position;
	UwacSize resolution;
	int transform;
	int scale;
	char* make;
	char* model;
	uint32_t server_output_id;
	struct wl_output* output;

	struct wl_list link;
};

/** @brief a seat attached to a wayland display */
struct uwac_seat
{
	UwacDisplay* display;
	char* name;
	struct wl_seat* seat;
	uint32_t seat_id;
	uint32_t seat_version;
	struct wl_data_device* data_device;
	struct wl_data_source* data_source;
	struct wl_pointer* pointer;
	struct wl_surface* pointer_surface;
	struct wl_cursor_image* pointer_image;
	struct wl_cursor_theme* cursor_theme;
	struct wl_cursor* default_cursor;
	void* pointer_data;
	size_t pointer_size;
	int pointer_type;
	struct wl_keyboard* keyboard;
	struct wl_touch* touch;
	struct wl_data_offer* offer;
	struct xkb_context* xkb_context;
	struct zwp_keyboard_shortcuts_inhibitor_v1* keyboard_inhibitor;

	struct
	{
		struct xkb_keymap* keymap;
		struct xkb_state* state;
		xkb_mod_mask_t control_mask;
		xkb_mod_mask_t alt_mask;
		xkb_mod_mask_t shift_mask;
		xkb_mod_mask_t caps_mask;
		xkb_mod_mask_t num_mask;
	} xkb;
	uint32_t modifiers;
	int32_t repeat_rate_sec, repeat_rate_nsec;
	int32_t repeat_delay_sec, repeat_delay_nsec;
	uint32_t repeat_sym, repeat_key, repeat_time;

	struct wl_array pressed_keys;

	UwacWindow* pointer_focus;

	UwacWindow* keyboard_focus;

	UwacWindow* touch_focus;
	bool touch_frame_started;

	int repeat_timer_fd;
	UwacTask repeat_task;
	float sx, sy;
	struct wl_list link;

	void* data_context;
	UwacDataTransferHandler transfer_data;
	UwacCancelDataTransferHandler cancel_data;
	bool ignore_announcement;
};

/** @brief a buffer used for drawing a surface frame */
struct uwac_buffer
{
	bool used;
	bool dirty;
#ifdef HAVE_PIXMAN_REGION
	pixman_region32_t damage;
#else
	REGION16 damage;
#endif
	struct wl_buffer* wayland_buffer;
	void* data;
	size_t size;
};
typedef struct uwac_buffer UwacBuffer;

/** @brief a window */
struct uwac_window
{
	UwacDisplay* display;
	int width, height, stride;
	int surfaceStates;
	enum wl_shm_format format;

	int nbuffers;
	UwacBuffer* buffers;

	struct wl_region* opaque_region;
	struct wl_region* input_region;
	ssize_t drawingBufferIdx;
	ssize_t pendingBufferIdx;
	struct wl_surface* surface;
	struct wl_shell_surface* shell_surface;
	struct xdg_surface* xdg_surface;
	struct xdg_toplevel* xdg_toplevel;
	struct zxdg_toplevel_decoration_v1* deco;
	struct org_kde_kwin_server_decoration* kde_deco;
#ifdef BUILD_IVI
	struct ivi_surface* ivi_surface;
#endif
	struct wl_list link;

	uint32_t pointer_enter_serial;
	uint32_t pointer_cursor_serial;
	int pointer_current_cursor;
};

/**@brief data to pass to wl_buffer release listener */
struct uwac_buffer_release_data
{
	UwacWindow* window;
	int bufferIdx;
};
typedef struct uwac_buffer_release_data UwacBufferReleaseData;

/* in uwa-display.c */
UwacEvent* UwacDisplayNewEvent(UwacDisplay* d, int type);
int UwacDisplayWatchFd(UwacDisplay* display, int fd, uint32_t events, UwacTask* task);

/* in uwac-input.c */
UwacSeat* UwacSeatNew(UwacDisplay* d, uint32_t id, uint32_t version);
void UwacSeatDestroy(UwacSeat* s);

/* in uwac-output.c */
UwacOutput* UwacCreateOutput(UwacDisplay* d, uint32_t id, uint32_t version);
int UwacDestroyOutput(UwacOutput* output);

UwacReturnCode UwacSeatRegisterClipboard(UwacSeat* s);

#endif /* UWAC_PRIV_H_ */

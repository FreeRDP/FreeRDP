/*
 * Copyright © 2014-2015 David FORT <contact@hardening-consulting.com>
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

#ifndef __UWAC_H_
#define __UWAC_H_

#include <wayland-client.h>
#include <stdbool.h>

#if __GNUC__ >= 4
	#define UWAC_API   __attribute__ ((visibility("default")))
#else
	#define UWAC_API
#endif

typedef struct uwac_size UwacSize;
typedef struct uwac_display UwacDisplay;
typedef struct uwac_output UwacOutput;
typedef struct uwac_window UwacWindow;
typedef struct uwac_seat UwacSeat;


/** @brief error codes */
typedef enum {
	UWAC_SUCCESS = 0,
	UWAC_ERROR_NOMEMORY,
	UWAC_ERROR_UNABLE_TO_CONNECT,
	UWAC_ERROR_INVALID_DISPLAY,
	UWAC_NOT_ENOUGH_RESOURCES,
	UWAC_TIMEDOUT,
	UWAC_NOT_FOUND,
	UWAC_ERROR_CLOSED,
	UWAC_ERROR_INTERNAL,

	UWAC_ERROR_LAST,
} UwacReturnCode;

/** @brief input modifiers */
enum {
	UWAC_MOD_SHIFT_MASK	= 0x01,
	UWAC_MOD_ALT_MASK = 0x02,
	UWAC_MOD_CONTROL_MASK = 0x04,
};

/** @brief a rectangle size measure */
struct uwac_size {
	int width;
	int height;
};


/** @brief event types */
enum {
	UWAC_EVENT_NEW_SEAT = 0,
	UWAC_EVENT_REMOVED_SEAT,
	UWAC_EVENT_NEW_OUTPUT,
	UWAC_EVENT_CONFIGURE,
	UWAC_EVENT_POINTER_ENTER,
	UWAC_EVENT_POINTER_LEAVE,
	UWAC_EVENT_POINTER_MOTION,
	UWAC_EVENT_POINTER_BUTTONS,
	UWAC_EVENT_POINTER_AXIS,
	UWAC_EVENT_KEYBOARD_ENTER,
	UWAC_EVENT_KEY,
	UWAC_EVENT_TOUCH_FRAME_BEGIN,
	UWAC_EVENT_TOUCH_UP,
	UWAC_EVENT_TOUCH_DOWN,
	UWAC_EVENT_TOUCH_MOTION,
	UWAC_EVENT_TOUCH_CANCEL,
	UWAC_EVENT_TOUCH_FRAME_END,
	UWAC_EVENT_FRAME_DONE,
	UWAC_EVENT_CLOSE,
};

/** @brief window states */
enum {
	UWAC_WINDOW_MAXIMIZED 	= 0x1,
	UWAC_WINDOW_RESIZING 	= 0x2,
	UWAC_WINDOW_FULLSCREEN 	= 0x4,
	UWAC_WINDOW_ACTIVATED 	= 0x8,
};

struct uwac_new_output_event {
	int type;
	UwacOutput *output;
};
typedef struct uwac_new_output_event UwacOutputNewEvent;

struct uwac_new_seat_event {
	int type;
	UwacSeat *seat;
};
typedef struct uwac_new_seat_event UwacSeatNewEvent;

typedef struct uwac_new_seat_event UwacSeatRemovedEvent;

struct uwac_keyboard_enter_event {
	int type;
	UwacWindow *window;
	UwacSeat *seat;
};
typedef struct uwac_keyboard_enter_event UwacKeyboardEnterLeaveEvent;

struct uwac_pointer_enter_event {
	int type;
	UwacWindow *window;
	UwacSeat *seat;
	uint32_t x, y;
};
typedef struct uwac_pointer_enter_event UwacPointerEnterLeaveEvent;

struct uwac_pointer_motion_event {
	int type;
	UwacWindow *window;
	UwacSeat *seat;
	uint32_t x, y;
};
typedef struct uwac_pointer_motion_event UwacPointerMotionEvent;

struct uwac_pointer_button_event {
	int type;
	UwacWindow *window;
	UwacSeat *seat;
	uint32_t x, y;
	uint32_t button;
	enum wl_pointer_button_state state;
};
typedef struct uwac_pointer_button_event UwacPointerButtonEvent;

struct uwac_pointer_axis_event {
	int type;
	UwacWindow *window;
	UwacSeat *seat;
	uint32_t x, y;
	uint32_t axis;
	wl_fixed_t value;
};
typedef struct uwac_pointer_axis_event UwacPointerAxisEvent;

struct uwac_touch_frame_event {
	int type;
	UwacWindow *window;
	UwacSeat *seat;
};
typedef struct uwac_touch_frame_event UwacTouchFrameBegin;
typedef struct uwac_touch_frame_event UwacTouchFrameEnd;
typedef struct uwac_touch_frame_event UwacTouchCancel;

struct uwac_touch_data {
	int type;
	UwacWindow *window;
	UwacSeat *seat;
	int32_t id;
	wl_fixed_t x;
	wl_fixed_t y;
};
typedef struct uwac_touch_data UwacTouchUp;
typedef struct uwac_touch_data UwacTouchDown;
typedef struct uwac_touch_data UwacTouchMotion;

struct uwac_frame_done_event {
	int type;
	UwacWindow *window;
};
typedef struct uwac_frame_done_event UwacFrameDoneEvent;

struct uwac_configure_event {
	int type;
	UwacWindow *window;
	int32_t width;
	int32_t height;
	int states;
};
typedef struct uwac_configure_event UwacConfigureEvent;

struct uwac_key_event {
	int type;
	UwacWindow *window;
	uint32_t raw_key;
	uint32_t sym;
	bool pressed;
};
typedef struct uwac_key_event UwacKeyEvent;

struct uwac_close_event {
	int type;
	UwacWindow *window;
};
typedef struct uwac_close_event UwacCloseEvent;


/** @brief */
union uwac_event {
	int type;
	UwacOutputNewEvent output_new;
	UwacSeatNewEvent seat_new;
	UwacPointerEnterLeaveEvent mouse_enter_leave;
	UwacPointerMotionEvent mouse_motion;
	UwacPointerButtonEvent mouse_button;
	UwacPointerAxisEvent mouse_axis;
	UwacKeyboardEnterLeaveEvent keyboard_enter_leave;
	UwacKeyEvent key;
	UwacTouchFrameBegin touchFrameBegin;
	UwacTouchUp touchUp;
	UwacTouchDown touchDown;
	UwacTouchMotion touchMotion;
	UwacTouchFrameEnd touchFrameEnd;
	UwacTouchCancel touchCancel;
	UwacFrameDoneEvent frame_done;
	UwacConfigureEvent configure;
	UwacCloseEvent close;
};
typedef union uwac_event UwacEvent;

typedef bool (*UwacErrorHandler)(UwacDisplay *d, UwacReturnCode code, const char *msg, ...);

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	install a handler that will be called when UWAC encounter internal errors. The
 *	handler is supposed to answer if the execution can continue. I can also be used
 *	to log things.
 *
 * @param handler
 */
UWAC_API void UwacInstallErrorHandler(UwacErrorHandler handler);


/**
 *	Opens the corresponding wayland display, using NULL you will open the default
 *	display.
 *
 * @param name the name of the display to open
 * @return the created UwacDisplay object
 */
UWAC_API UwacDisplay *UwacOpenDisplay(const char *name, UwacReturnCode *err);

/**
 *	closes the corresponding UwacDisplay
 *
 * @param pdisplay a pointer on the display to close
 * @return UWAC_SUCCESS if the operation was successful, the corresponding error otherwise
 */
UWAC_API UwacReturnCode UwacCloseDisplay(UwacDisplay **pdisplay);

/**
 * Returns the file descriptor associated with the UwacDisplay, this is useful when
 * you want to poll that file descriptor for activity.
 *
 * @param display an opened UwacDisplay
 * @return the corresponding descriptor
 */
UWAC_API int UwacDisplayGetFd(UwacDisplay *display);

/**
 *	Returns a human readable form of a Uwac error code
 *
 * @param error the error number
 * @return the associated string
 */
UWAC_API const char *UwacErrorString(UwacReturnCode error);

/**
 * returns the last error that occurred on a display
 *
 * @param display the display
 * @return the last error that have been set for this display
 */
UWAC_API UwacReturnCode UwacDisplayGetLastError(const UwacDisplay *display);

/**
 * retrieves the version of a given interface
 *
 * @param display the display connection
 * @param name the name of the interface
 * @param version the output variable for the version
 * @return UWAC_SUCCESS if the interface was found, UWAC_NOT_FOUND otherwise
 */
UWAC_API UwacReturnCode UwacDisplayQueryInterfaceVersion(const UwacDisplay *display, const char *name, uint32_t *version);

/**
 *	returns the number SHM formats that have been reported by the compositor
 *
 * @param display a connected UwacDisplay
 * @return the number of SHM formats supported
 */
UWAC_API uint32_t UwacDisplayQueryGetNbShmFormats(UwacDisplay *display);

/**
 *	returns the supported ShmFormats
 *
 * @param display a connected UwacDisplay
 * @param formats a pointer on an array of wl_shm_format with enough place for formats_size items
 * @param formats_size the size of the formats array
 * @param filled the number of filled entries in the formats array
 * @return UWAC_SUCCESS on success, an error otherwise
 */
UWAC_API UwacReturnCode UwacDisplayQueryShmFormats(const UwacDisplay *display, enum wl_shm_format *formats, int formats_size, int *filled);

/**
 *	returns the number of registered outputs
 *
 * @param display the display to query
 * @return the number of outputs
 */
UWAC_API uint32_t UwacDisplayGetNbOutputs(UwacDisplay *display);

/**
 *	retrieve a particular UwacOutput object
 *
 * @param display the display to query
 * @param index index of the output
 * @return the given UwacOutput, NULL if something failed (so you should query UwacDisplayGetLastError() to have the reason)
 */
UWAC_API UwacOutput *UwacDisplayGetOutput(UwacDisplay *display, int index);

/**
 * retrieve the resolution of a given UwacOutput
 *
 * @param output the UwacOutput
 * @param resolution a pointer on the
 * @return UWAC_SUCCESS on success
 */
UWAC_API UwacReturnCode UwacOutputGetResolution(UwacOutput *output, UwacSize *resolution);


/**
 *	creates a window using a SHM surface
 *
 * @param display the display to attach the window to
 * @param width the width of the window
 * @param height the heigh of the window
 * @param format format to use for the SHM surface
 * @return the created UwacWindow, NULL if something failed (use UwacDisplayGetLastError() to know more about this)
 */
UWAC_API UwacWindow *UwacCreateWindowShm(UwacDisplay *display, uint32_t width, uint32_t height, enum wl_shm_format format);

/**
 *	destroys the corresponding UwacWindow
 *
 * @param window a pointer on the UwacWindow to destroy
 * @return if the operation completed successfully
 */
UWAC_API UwacReturnCode UwacDestroyWindow(UwacWindow **window);

/**
 *	Sets the region that should be considered opaque to the compositor.
 *
 * @param window the UwacWindow
 * @param x
 * @param y
 * @param width
 * @param height
 * @return UWAC_SUCCESS on success, an error otherwise
 */
UWAC_API UwacReturnCode UwacWindowSetOpaqueRegion(UwacWindow *window, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

/**
 *	Sets the region of the window that can trigger input events
 *
 * @param window the UwacWindow
 * @param x
 * @param y
 * @param width
 * @param height
 * @return
 */
UWAC_API UwacReturnCode UwacWindowSetInputRegion(UwacWindow *window, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

/**
 *	retrieves a pointer on the current window content to draw a frame
 * @param window the UwacWindow
 * @return a pointer on the current window content
 */
UWAC_API void *UwacWindowGetDrawingBuffer(UwacWindow *window);

/**
 *	sets a rectangle as dirty for the next frame of a window
 *
 * @param window the UwacWindow
 * @param x left coordinate
 * @param y top coordinate
 * @param width the width of the dirty rectangle
 * @param height the height of the dirty rectangle
 * @return UWAC_SUCCESS on success, an Uwac error otherwise
 */
UWAC_API UwacReturnCode UwacWindowAddDamage(UwacWindow *window, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

/**
 *	Sends a frame to the compositor with the content of the drawing buffer
 *
 * @param window the UwacWindow to refresh
 * @param copyContentForNextFrame if true the content to display is copied in the next drawing buffer
 * @return UWAC_SUCCESS if the operation was successful
 */
UWAC_API UwacReturnCode UwacWindowSubmitBuffer(UwacWindow *window, bool copyContentForNextFrame);

/**
 *	returns the geometry of the given UwacWindows
 *
 * @param window the UwacWindow
 * @param geometry the geometry to fill
 * @return UWAC_SUCCESS on success, an Uwac error otherwise
 */
UWAC_API UwacReturnCode UwacWindowGetGeometry(UwacWindow *window, UwacSize *geometry);

/**
 *	Sets or unset the fact that the window is set fullscreen. After this call the
 *	application should get prepared to receive a configure event. The output is used
 *	only when going fullscreen, it is optional and not used when exiting fullscreen.
 *
 * @param window the UwacWindow
 * @param output an optional UwacOutput to put the window fullscreen on
 * @param isFullscreen set or unset fullscreen
 * @return UWAC_SUCCESS if the operation was a success
 */
UWAC_API UwacReturnCode UwacWindowSetFullscreenState(UwacWindow *window, UwacOutput *output, bool isFullscreen);

/**
 *	When possible (depending on the shell) sets the title of the UwacWindow
 *
 * @param window the UwacWindow
 * @param name title
 */
UWAC_API void UwacWindowSetTitle(UwacWindow *window, const char *name);

/**
 *
 * @param display
 * @param timeout
 * @return
 */
UWAC_API int UwacDisplayDispatch(UwacDisplay *display, int timeout);

/**
 *	Returns if you have some pending events, and you can UwacNextEvent() without blocking
 *
 * @param display the UwacDisplay
 * @return if there's some pending events
 */
UWAC_API bool UwacHasEvent(UwacDisplay *display);

/** Waits until an event occurs, and when it's there copy the event from the queue to
 * event.
 *
 * @param display the Uwac display
 * @param event the event to fill
 * @return if the operation completed successfully
 */
UWAC_API UwacReturnCode UwacNextEvent(UwacDisplay *display, UwacEvent *event);


/**
 * returns the name of the given UwacSeat
 *
 * @param seat the UwacSeat
 * @return the name of the seat
 */
UWAC_API const char *UwacSeatGetName(const UwacSeat *seat);

#ifdef __cplusplus
}
#endif

#endif /* __UWAC_H_ */

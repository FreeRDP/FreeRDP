/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 helper utilities
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
 * Copyringht 2023 Thincast Technologies GmbH
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
#pragma once

#include <winpr/wlog.h>
#include <winpr/wtypes.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "xfreerdp.h"

const char* x11_error_to_string(xfContext* xfc, int error, char* buffer, size_t size);

#define X_GET_ATOM_VAR_NAME(x) #x
#define Safe_XGetAtomName(log, display, atom) \
	Safe_XGetAtomNameEx((log), (display), (atom), X_GET_ATOM_VAR_NAME(atom))
char* Safe_XGetAtomNameEx(wLog* log, Display* display, Atom atom, const char* varname);
Atom Logging_XInternAtom(wLog* log, Display* display, _Xconst char* atom_name, Bool only_if_exists);

typedef BOOL (*fn_action_script_run)(xfContext* xfc, const char* buffer, size_t size, void* user,
                                     const char* what, const char* arg);
BOOL run_action_script(xfContext* xfc, const char* what, const char* arg, fn_action_script_run fkt,
                       void* user);

#define LogDynAndXCreatePixmap(log, display, d, width, height, depth)                       \
	LogDynAndXCreatePixmap_ex((log), __FILE__, __func__, __LINE__, (display), (d), (width), \
	                          (height), (depth))

Pixmap LogDynAndXCreatePixmap_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                 Display* display, Drawable d, unsigned int width,
                                 unsigned int height, unsigned int depth);

#define LogDynAndXFreePixmap(log, display, pixmap) \
	LogDynAndXFreePixmap_ex(log, __FILE__, __func__, __LINE__, (display), (pixmap))

int LogDynAndXFreePixmap_ex(wLog* log, const char* file, const char* fkt, size_t line,
                            Display* display, Pixmap pixmap);

#define LogDynAndXCreateWindow(log, display, parent, x, y, width, height, border_width, depth,    \
                               class, visual, valuemask, attributes)                              \
	LogDynAndXCreateWindow_ex((log), __FILE__, __func__, __LINE__, (display), (parent), (x), (y), \
	                          (width), (height), (border_width), (depth), (class), (visual),      \
	                          (valuemask), (attributes))

Window LogDynAndXCreateWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                 Display* display, Window parent, int x, int y, unsigned int width,
                                 unsigned int height, unsigned int border_width, int depth,
                                 unsigned int c_class, Visual* visual, unsigned long valuemask,
                                 XSetWindowAttributes* attributes);

#define LogDynAndXRaiseWindow(log, display, w) \
	LogDynAndXRaiseWindow_ex((log), __FILE__, __func__, __LINE__, (display), (w))

int LogDynAndXRaiseWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                             Display* display, Window w);

#define LogDynAndXMapWindow(log, display, w) \
	LogDynAndXMapWindow_ex((log), __FILE__, __func__, __LINE__, (display), (w))

int LogDynAndXMapWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                           Display* display, Window w);

#define LogDynAndXUnmapWindow(log, display, w) \
	LogDynAndXUnmapWindow_ex((log), __FILE__, __func__, __LINE__, (display), (w))

int LogDynAndXUnmapWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                             Display* display, Window w);

#define LogDynAndXMoveResizeWindow(log, display, w, x, y, width, height)                         \
	LogDynAndXMoveResizeWindow_ex((log), __FILE__, __func__, __LINE__, (display), (w), (x), (y), \
	                              (width), (height))

int LogDynAndXMoveResizeWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                  Display* display, Window w, int x, int y, unsigned int width,
                                  unsigned int height);

#define LogDynAndXWithdrawWindow(log, display, w, screen_number)                     \
	LogDynAndXWithdrawWindow_ex((log), __FILE__, __func__, __LINE__, (display), (w), \
	                            (screen_number))

Status LogDynAndXWithdrawWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                   Display* display, Window w, int screen_number);

#define LogDynAndXMoveWindow(log, display, w, x, y) \
	LogDynAndXMoveWindow_ex((log), __FILE__, __func__, __LINE__, (display), (w), (x), (y))

int LogDynAndXMoveWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                            Display* display, Window w, int x, int y);

#define LogDynAndXResizeWindow(log, display, w, width, height)                              \
	LogDynAndXResizeWindow_ex((log), __FILE__, __func__, __LINE__, (display), (w), (width), \
	                          (height))

int LogDynAndXResizeWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                              Display* display, Window w, unsigned int width, unsigned int height);

#define LogDynAndXClearWindow(log, display, w) \
	LogDynAndXClearWindow_ex((log), __FILE__, __func__, __LINE__, (display), (w))

int LogDynAndXClearWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                             Display* display, Window w);

#define LogDynAndXGetWindowProperty(log, display, w, property, long_offset, long_length, delete,   \
                                    req_type, actual_type_return, actual_format_return,            \
                                    nitems_return, bytes_after_return, prop_return)                \
	LogDynAndXGetWindowProperty_ex((log), __FILE__, __func__, __LINE__, (display), (w),            \
	                               (property), (long_offset), (long_length), (delete), (req_type), \
	                               (actual_type_return), (actual_format_return), (nitems_return),  \
	                               (bytes_after_return), (prop_return))

int LogDynAndXGetWindowProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                   Display* display, Window w, Atom property, long long_offset,
                                   long long_length, Bool c_delete, Atom req_type,
                                   Atom* actual_type_return, int* actual_format_return,
                                   unsigned long* nitems_return, unsigned long* bytes_after_return,
                                   unsigned char** prop_return);

#define LogDynAndXReparentWindow(log, display, w, parent, x, y)                                \
	LogDynAndXReparentWindow_ex((log), __FILE__, __func__, __LINE__, (display), (w), (parent), \
	                            (x), (y))

int LogDynAndXReparentWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Window parent, int x, int y);

#define LogDynAndXChangeProperty(log, display, w, property, type, format, mode, data, nelements) \
	LogDynAndXChangeProperty_ex((log), __FILE__, __func__, __LINE__, (display), (w), (property), \
	                            (type), (format), (mode), (data), (nelements))
int LogDynAndXChangeProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property, Atom type, int format,
                                int mode, _Xconst unsigned char* data, int nelements);

#define LogDynAndXDeleteProperty(log, display, w, property) \
	LogDynAndXDeleteProperty_ex((log), __FILE__, __func__, __LINE__, (display), (w), (property))
int LogDynAndXDeleteProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property);

#define LogDynAndXConvertSelection(log, display, selection, target, property, requestor, time) \
	LogDynAndXConvertSelection_ex((log), __FILE__, __func__, __LINE__, (display), (selection), \
	                              (target), (property), (requestor), (time))
int LogDynAndXConvertSelection_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                  Display* display, Atom selection, Atom target, Atom property,
                                  Window requestor, Time time);

#define LogDynAndXCreateGC(log, display, d, valuemask, values) \
	LogDynAndXCreateGC_ex(log, __FILE__, __func__, __LINE__, (display), (d), (valuemask), (values))

GC LogDynAndXCreateGC_ex(wLog* log, const char* file, const char* fkt, size_t line,
                         Display* display, Drawable d, unsigned long valuemask, XGCValues* values);

#define LogDynAndXFreeGC(log, display, gc) \
	LogDynAndXFreeGC_ex(log, __FILE__, __func__, __LINE__, (display), (gc))

int LogDynAndXFreeGC_ex(wLog* log, const char* file, const char* fkt, size_t line, Display* display,
                        GC gc);

#define LogDynAndXCreateImage(log, display, visual, depth, format, offset, data, width, height, \
                              bitmap_pad, bytes_per_line)                                       \
	LogDynAndXCreateImage_ex(log, __FILE__, __func__, __LINE__, (display), (visual), (depth),   \
	                         (format), (offset), (data), (width), (height), (bitmap_pad),       \
	                         (bytes_per_line))
XImage* LogDynAndXCreateImage_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                 Display* display, Visual* visual, unsigned int depth, int format,
                                 int offset, char* data, unsigned int width, unsigned int height,
                                 int bitmap_pad, int bytes_per_line);

#define LogDynAndXPutImage(log, display, d, gc, image, src_x, src_y, dest_x, dest_y, width, \
                           height)                                                          \
	LogDynAndXPutImage_ex(log, __FILE__, __func__, __LINE__, (display), (d), (gc), (image), \
	                      (src_x), (src_y), (dest_x), (dest_y), (width), (height))
int LogDynAndXPutImage_ex(wLog* log, const char* file, const char* fkt, size_t line,
                          Display* display, Drawable d, GC gc, XImage* image, int src_x, int src_y,
                          int dest_x, int dest_y, unsigned int width, unsigned int height);

#define LogDynAndXCopyArea(log, display, src, dest, gc, src_x, src_y, width, height, dest_x, \
                           dest_y)                                                           \
	LogDynAndXCopyArea_ex(log, __FILE__, __func__, __LINE__, (display), (src), (dest), (gc), \
	                      (src_x), (src_y), (width), (height), (dest_x), (dest_y))
extern int LogDynAndXCopyArea_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                 Display* display, Pixmap src, Window dest, GC gc, int src_x,
                                 int src_y, unsigned int width, unsigned int height, int dest_x,
                                 int dest_y);

#define LogDynAndXSendEvent(log, display, w, propagate, event_mask, event_send)            \
	LogDynAndXSendEvent_ex(log, __FILE__, __func__, __LINE__, (display), (w), (propagate), \
	                       (event_mask), (event_send))
extern Status LogDynAndXSendEvent_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                     Display* display, Window w, Bool propagate, long event_mask,
                                     XEvent* event_send);

#define LogDynAndXFlush(log, display) \
	LogDynAndXFlush_ex(log, __FILE__, __func__, __LINE__, (display))
extern Status LogDynAndXFlush_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                 Display* display);

#define LogDynAndXSync(log, display, discard) \
	LogDynAndXSync_ex(log, __FILE__, __func__, __LINE__, (display), (discard))
extern int LogDynAndXSync_ex(wLog* log, const char* file, const char* fkt, size_t line,
                             Display* display, Bool discard);

#define LogDynAndXGetSelectionOwner(log, display, selection) \
	LogDynAndXGetSelectionOwner_ex(log, __FILE__, __func__, __LINE__, (display), (selection))
extern Window LogDynAndXGetSelectionOwner_ex(wLog* log, const char* file, const char* fkt,
                                             size_t line, Display* display, Atom selection);

#define LogDynAndXSetSelectionOwner(log, display, selection, owner, time)                     \
	LogDynAndXSetSelectionOwner_ex(log, __FILE__, __func__, __LINE__, (display), (selection), \
	                               (owner), (time))
extern int LogDynAndXSetSelectionOwner_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                          Display* display, Atom selection, Window owner,
                                          Time time);

#define LogDynAndXDestroyWindow(log, display, window) \
	LogDynAndXDestroyWindow_ex(log, __FILE__, __func__, __LINE__, (display), (window))
extern int LogDynAndXDestroyWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                      Display* display, Window window);

#define LogDynAndXChangeWindowAttributes(log, display, window, valuemask, attributes)           \
	LogDynAndXChangeWindowAttributes_ex(log, __FILE__, __func__, __LINE__, (display), (window), \
	                                    (valuemask), (attributes))
extern int LogDynAndXChangeWindowAttributes_ex(wLog* log, const char* file, const char* fkt,
                                               size_t line, Display* display, Window window,
                                               unsigned long valuemask,
                                               XSetWindowAttributes* attributes);

#define LogDynAndXSetTransientForHint(log, display, window, prop_window)                     \
	LogDynAndXSetTransientForHint_ex(log, __FILE__, __func__, __LINE__, (display), (window), \
	                                 (prop_window))
extern int LogDynAndXSetTransientForHint_ex(wLog* log, const char* file, const char* fkt,
                                            size_t line, Display* display, Window window,
                                            Window prop_window);

#define LogDynAndXCloseDisplay(log, display) \
	LogDynAndXCloseDisplay_ex(log, __FILE__, __func__, __LINE__, (display))
extern int LogDynAndXCloseDisplay_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                     Display* display);

#define LogDynAndXSetClipMask(log, display, gc, pixmap) \
	LogDynAndXSetClipMask_ex(log, __FILE__, __func__, __LINE__, (display), (gc), (pixmap))
extern int LogDynAndXSetClipMask_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                    Display* display, GC gc, Pixmap pixmap);

#define LogDynAndXSetRegion(log, display, gc, r) \
	LogDynAndXSetRegion_ex(log, __FILE__, __func__, __LINE__, (display), (gc), (r))
extern int LogDynAndXSetRegion_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                  Display* display, GC gc, Region r);

#define LogDynAndXSetBackground(log, display, gc, background) \
	LogDynAndXSetBackground_ex(log, __FILE__, __func__, __LINE__, (display), (gc), (background))
extern int LogDynAndXSetBackground_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                      Display* display, GC gc, unsigned long background);

#define LogDynAndXSetForeground(log, display, gc, foreground) \
	LogDynAndXSetForeground_ex(log, __FILE__, __func__, __LINE__, (display), (gc), (foreground))
extern int LogDynAndXSetForeground_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                      Display* display, GC gc, unsigned long foreground);

#define LogDynAndXSetFillStyle(log, display, gc, fill_style) \
	LogDynAndXSetFillStyle_ex(log, __FILE__, __func__, __LINE__, (display), (gc), (fill_style))
extern int LogDynAndXSetFillStyle_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                     Display* display, GC gc, int fill_style);

#define LogDynAndXFillRectangle(log, display, w, gc, x, y, width, height)                         \
	LogDynAndXFillRectangle_ex(log, __FILE__, __func__, __LINE__, (display), (w), (gc), (x), (y), \
	                           (width), (height))
extern int LogDynAndXFillRectangle_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                      Display* display, Window w, GC gc, int x, int y,
                                      unsigned int width, unsigned int height);

#define LogDynAndXSetFunction(log, display, gc, function) \
	LogDynAndXSetFunction_ex(log, __FILE__, __func__, __LINE__, (display), (gc), (function))
extern int LogDynAndXSetFunction_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                    Display* display, GC gc, int function);

#define LogDynAndXRestackWindows(log, display, windows, count) \
	LogDynAndXRestackWindows_ex(log, __FILE__, __func__, __LINE__, (display), (windows), (count))
extern int LogDynAndXRestackWindows_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                       Display* display, Window* windows, int nwindows);

BOOL IsGnome(void);

char* getConfigOption(BOOL system, const char* option);

const char* request_code_2_str(int code);

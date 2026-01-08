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

#include <string.h>
#include <winpr/assert.h>
#include <winpr/wtypes.h>
#include <winpr/path.h>

#include "xf_utils.h"
#include "xfreerdp.h"

#include <freerdp/utils/helpers.h>
#include <freerdp/log.h>

#define TAG CLIENT_TAG("xfreerdp.utils")

static const DWORD log_level = WLOG_TRACE;

static const char* error_to_string(wLog* log, Display* display, int error, char* buffer,
                                   size_t size)
{
	WINPR_ASSERT(size <= INT32_MAX);
	const int rc = XGetErrorText(display, error, buffer, (int)size);
	if (rc != Success)
		WLog_Print(log, WLOG_WARN, "XGetErrorText returned %d", rc);
	return buffer;
}

WINPR_ATTR_FORMAT_ARG(6, 7)
static void write_log(wLog* log, DWORD level, const char* fname, const char* fkt, size_t line,
                      WINPR_FORMAT_ARG const char* fmt, ...)
{
	va_list ap = { 0 };
	va_start(ap, fmt);
	WLog_PrintTextMessageVA(log, level, line, fname, fkt, fmt, ap);
	va_end(ap);
}

static BOOL ignore_code(int rc, size_t count, va_list ap)
{
	for (size_t x = 0; x < count; x++)
	{
		const int val = va_arg(ap, int);
		if (rc == val)
			return TRUE;
	}
	return FALSE;
}

/* libx11 return codes are not really well documented, so checked against
 * https://gitlab.freedesktop.org/xorg/lib/libx11.git */
static int write_result_log_va(wLog* log, DWORD level, const char* fname, const char* fkt,
                               size_t line, Display* display, char* name, int rc, size_t count,
                               va_list ap)
{
	const BOOL ignore = ignore_code(rc, count, ap);
	if (!ignore)
	{
		char buffer[128] = { 0 };

		if (WLog_IsLevelActive(log, level))
		{
			WLog_PrintTextMessage(log, level, line, fname, fkt, "%s returned %s", name,
			                      error_to_string(log, display, rc, buffer, sizeof(buffer)));
		}
	}
	return rc;
}

static int write_result_log_expect_success(wLog* log, DWORD level, const char* fname,
                                           const char* fkt, size_t line, Display* display,
                                           char* name, int rc)
{
	if (rc != Success)
	{
		va_list ap;
		(void)write_result_log_va(log, level, fname, fkt, line, display, name, rc, 0, ap);
		va_end(ap);
	}
	return rc;
}

static int write_result_log_expect_one(wLog* log, DWORD level, const char* fname, const char* fkt,
                                       size_t line, Display* display, char* name, int rc)
{
	if (rc != 1)
	{
		va_list ap;
		(void)write_result_log_va(log, level, fname, fkt, line, display, name, rc, 0, ap);
		va_end(ap);
	}
	return rc;
}

char* Safe_XGetAtomNameEx(wLog* log, Display* display, Atom atom, const char* varname)
{
	WLog_Print(log, log_level, "XGetAtomName(%s, 0x%08lx)", varname, atom);
	if (atom == None)
		return strdup("Atom_None");
	return XGetAtomName(display, atom);
}

Atom Logging_XInternAtom(wLog* log, Display* display, _Xconst char* atom_name, Bool only_if_exists)
{
	Atom atom = XInternAtom(display, atom_name, only_if_exists);
	if (WLog_IsLevelActive(log, log_level))
	{
		WLog_Print(log, log_level, "XInternAtom(%p, %s, %s) -> 0x%08" PRIx32, (void*)display,
		           atom_name, only_if_exists ? "True" : "False",
		           WINPR_CXX_COMPAT_CAST(UINT32, atom));
	}
	return atom;
}

const char* x11_error_to_string(xfContext* xfc, int error, char* buffer, size_t size)
{
	WINPR_ASSERT(xfc);
	return error_to_string(xfc->log, xfc->display, error, buffer, size);
}

int LogDynAndXChangeProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property, Atom type, int format,
                                int mode, const unsigned char* data, int nelements)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* propstr = Safe_XGetAtomName(log, display, property);
		char* typestr = Safe_XGetAtomName(log, display, type);
		write_log(log, log_level, file, fkt, line,
		          "XChangeProperty(%p, %lu, %s [%lu], %s [%lu], %d, %d, %p, %d)", (void*)display, w,
		          propstr, property, typestr, type, format, mode, (const void*)data, nelements);
		XFree(propstr);
		XFree(typestr);
	}
	const int rc = XChangeProperty(display, w, property, type, format, mode, data, nelements);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XChangeProperty",
	                                   rc);
}

int LogDynAndXDeleteProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* propstr = Safe_XGetAtomName(log, display, property);
		write_log(log, log_level, file, fkt, line, "XDeleteProperty(%p, %lu, %s [%lu])",
		          (void*)display, w, propstr, property);
		XFree(propstr);
	}
	const int rc = XDeleteProperty(display, w, property);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XDeleteProperty",
	                                   rc);
}

int LogDynAndXConvertSelection_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                  Display* display, Atom selection, Atom target, Atom property,
                                  Window requestor, Time time)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* selectstr = Safe_XGetAtomName(log, display, selection);
		char* targetstr = Safe_XGetAtomName(log, display, target);
		char* propstr = Safe_XGetAtomName(log, display, property);
		write_log(log, log_level, file, fkt, line,
		          "XConvertSelection(%p, %s [%lu], %s [%lu], %s [%lu], %lu, %lu)", (void*)display,
		          selectstr, selection, targetstr, target, propstr, property, requestor, time);
		XFree(propstr);
		XFree(targetstr);
		XFree(selectstr);
	}
	const int rc = XConvertSelection(display, selection, target, property, requestor, time);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display,
	                                   "XConvertSelection", rc);
}

int LogDynAndXGetWindowProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                   Display* display, Window w, Atom property, long long_offset,
                                   long long_length, int c_delete, Atom req_type,
                                   Atom* actual_type_return, int* actual_format_return,
                                   unsigned long* nitems_return, unsigned long* bytes_after_return,
                                   unsigned char** prop_return)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* propstr = Safe_XGetAtomName(log, display, property);
		char* req_type_str = Safe_XGetAtomName(log, display, req_type);
		write_log(
		    log, log_level, file, fkt, line,
		    "XGetWindowProperty(%p, %lu, %s [%lu], %ld, %ld, %d, %s [%lu], %p, %p, %p, %p, %p)",
		    (void*)display, w, propstr, property, long_offset, long_length, c_delete, req_type_str,
		    req_type, (void*)actual_type_return, (void*)actual_format_return, (void*)nitems_return,
		    (void*)bytes_after_return, (void*)prop_return);
		XFree(propstr);
		XFree(req_type_str);
	}
	const int rc = XGetWindowProperty(display, w, property, long_offset, long_length, c_delete,
	                                  req_type, actual_type_return, actual_format_return,
	                                  nitems_return, bytes_after_return, prop_return);
	return write_result_log_expect_success(log, WLOG_WARN, file, fkt, line, display,
	                                       "XGetWindowProperty", rc);
}

BOOL IsGnome(void)
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	char* env = getenv("DESKTOP_SESSION");
	return (env != NULL && strcmp(env, "gnome") == 0);
}

BOOL run_action_script(xfContext* xfc, const char* what, const char* arg, fn_action_script_run fkt,
                       void* user)
{
	BOOL rc = FALSE;
	FILE* keyScript = NULL;
	WINPR_ASSERT(xfc);

	rdpSettings* settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	const char* ActionScript = freerdp_settings_get_string(settings, FreeRDP_ActionScript);

	xfc->actionScriptExists = winpr_PathFileExists(ActionScript);

	if (!xfc->actionScriptExists)
	{
		WLog_DBG(TAG, "[ActionScript] no such script '%s'", ActionScript);
		goto fail;
	}

	{
		char command[2048] = { 0 };
		(void)sprintf_s(command, sizeof(command), "%s %s", ActionScript, what);
		keyScript = popen(command, "r");

		if (!keyScript)
		{
			WLog_ERR(TAG, "[ActionScript] Failed to execute '%s'", command);
			goto fail;
		}

		{
			BOOL read_data = FALSE;
			char buffer[2048] = { 0 };
			while (fgets(buffer, sizeof(buffer), keyScript) != NULL)
			{
				char* context = NULL;
				(void)strtok_s(buffer, "\n", &context);

				if (fkt)
				{
					if (!fkt(xfc, buffer, strnlen(buffer, sizeof(buffer)), user, what, arg))
						goto fail;
				}
				read_data = TRUE;
			}

			rc = read_data;
		}
		if (!rc)
			WLog_ERR(TAG, "[ActionScript] No data returned from command '%s'", command);
	}
fail:
	if (keyScript)
		pclose(keyScript);
	const BOOL res = rc || !xfc->actionScriptExists;
	if (!rc)
		xfc->actionScriptExists = FALSE;
	return res;
}

int LogDynAndXCopyArea_ex(wLog* log, const char* file, const char* fkt, size_t line,
                          Display* display, Pixmap src, Window dest, GC gc, int src_x, int src_y,
                          unsigned int width, unsigned int height, int dest_x, int dest_y)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		XWindowAttributes attr = { 0 };
		const Status rc = XGetWindowAttributes(display, dest, &attr);

		write_log(log, log_level, file, fkt, line,
		          "XCopyArea(%p, src: {%lu}, dest: [%d]{%lu, %lu, %d}, gc: {%p}, src_x: {%d}, "
		          "src_y: {%d}, "
		          "width: {%u}, "
		          "height: {%u}, dest_x: {%d}, dest_y: {%d})",
		          (void*)display, src, rc, dest, attr.root, attr.depth, (void*)gc, src_x, src_y,
		          width, height, dest_x, dest_y);
	}

	if ((width == 0) || (height == 0))
	{
		const DWORD lvl = WLOG_WARN;
		if (WLog_IsLevelActive(log, lvl))
			write_log(log, lvl, file, fkt, line, "XCopyArea(width=%u, height=%u) !", width, height);
		return Success;
	}

	const int rc = XCopyArea(display, src, dest, gc, src_x, src_y, width, height, dest_x, dest_y);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XCopyArea", rc);
}

int LogDynAndXPutImage_ex(wLog* log, const char* file, const char* fkt, size_t line,
                          Display* display, Drawable d, GC gc, XImage* image, int src_x, int src_y,
                          int dest_x, int dest_y, unsigned int width, unsigned int height)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line,
		          "XPutImage(%p, d: {%lu}, gc: {%p}, image: [%p]{%d}, src_x: {%d}, src_y: {%d}, "
		          "dest_x: {%d}, "
		          "dest_y: {%d}, width: {%u}, "
		          "height: {%u})",
		          (void*)display, d, (void*)gc, (void*)image, image ? image->depth : -1, src_x,
		          src_y, dest_x, dest_y, width, height);
	}

	if ((width == 0) || (height == 0))
	{
		const DWORD lvl = WLOG_WARN;
		if (WLog_IsLevelActive(log, lvl))
			write_log(log, lvl, file, fkt, line, "XPutImage(width=%u, height=%u) !", width, height);
		return Success;
	}

	const int rc = XPutImage(display, d, gc, image, src_x, src_y, dest_x, dest_y, width, height);
	return write_result_log_expect_success(log, WLOG_WARN, file, fkt, line, display, "XPutImage",
	                                       rc);
}

/* be careful here.
 * XSendEvent returns Status, but implementation always returns 1
 */
Status LogDynAndXSendEvent_ex(wLog* log, const char* file, const char* fkt, size_t line,
                              Display* display, Window w, int propagate, long event_mask,
                              XEvent* event_send)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line,
		          "XSendEvent(d: {%p}, w: {%lu}, propagate: {%d}, event_mask: {%ld}, "
		          "event_send: [%p]{TODO})",
		          (void*)display, w, propagate, event_mask, (void*)event_send);
	}

	const int rc = XSendEvent(display, w, propagate, event_mask, event_send);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XSendEvent", rc);
}

int LogDynAndXFlush_ex(wLog* log, const char* file, const char* fkt, size_t line, Display* display)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XFlush(%p)", (void*)display);
	}

	const int rc = XFlush(display);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XFlush", rc);
}

Window LogDynAndXGetSelectionOwner_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                      Display* display, Atom selection)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* selectionstr = Safe_XGetAtomName(log, display, selection);
		write_log(log, log_level, file, fkt, line, "XGetSelectionOwner(%p, %s)", (void*)display,
		          selectionstr);
		XFree(selectionstr);
	}
	return XGetSelectionOwner(display, selection);
}

int LogDynAndXDestroyWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                               Display* display, Window window)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XDestroyWindow(%p, %lu)", (void*)display,
		          window);
	}
	const int rc = XDestroyWindow(display, window);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XDestroyWindow",
	                                   rc);
}

int LogDynAndXSync_ex(wLog* log, const char* file, const char* fkt, size_t line, Display* display,
                      Bool discard)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XSync(%p, %d)", (void*)display, discard);
	}
	const int rc = XSync(display, discard);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XSync", rc);
}

int LogDynAndXChangeWindowAttributes_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                        Display* display, Window window, unsigned long valuemask,
                                        XSetWindowAttributes* attributes)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XChangeWindowAttributes(%p, %lu, 0x%08lu, %p)",
		          (void*)display, window, valuemask, (void*)attributes);
	}
	const int rc = XChangeWindowAttributes(display, window, valuemask, attributes);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display,
	                                   "XChangeWindowAttributes", rc);
}

int LogDynAndXSetTransientForHint_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                     Display* display, Window window, Window prop_window)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XSetTransientForHint(%p, %lu, %lu)",
		          (void*)display, window, prop_window);
	}
	const int rc = XSetTransientForHint(display, window, prop_window);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display,
	                                   "XSetTransientForHint", rc);
}

int LogDynAndXCloseDisplay_ex(wLog* log, const char* file, const char* fkt, size_t line,
                              Display* display)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XCloseDisplay(%p)", (void*)display);
	}
	const int rc = XCloseDisplay(display);
	return write_result_log_expect_success(log, WLOG_WARN, file, fkt, line, display,
	                                       "XCloseDisplay", rc);
}

XImage* LogDynAndXCreateImage_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                 Display* display, Visual* visual, unsigned int depth, int format,
                                 int offset, char* data, unsigned int width, unsigned int height,
                                 int bitmap_pad, int bytes_per_line)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XCreateImage(%p)", (void*)display);
	}
	return XCreateImage(display, visual, depth, format, offset, data, width, height, bitmap_pad,
	                    bytes_per_line);
}

Window LogDynAndXCreateWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                 Display* display, Window parent, int x, int y, unsigned int width,
                                 unsigned int height, unsigned int border_width, int depth,
                                 unsigned int c_class, Visual* visual, unsigned long valuemask,
                                 XSetWindowAttributes* attributes)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XCreateWindow(%p)", (void*)display);
	}
	return XCreateWindow(display, parent, x, y, width, height, border_width, depth, c_class, visual,
	                     valuemask, attributes);
}

GC LogDynAndXCreateGC_ex(wLog* log, const char* file, const char* fkt, size_t line,
                         Display* display, Drawable d, unsigned long valuemask, XGCValues* values)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XCreateGC(%p)", (void*)display);
	}
	return XCreateGC(display, d, valuemask, values);
}

int LogDynAndXFreeGC_ex(wLog* log, const char* file, const char* fkt, size_t line, Display* display,
                        GC gc)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XFreeGC(%p)", (void*)display);
	}
	const int rc = XFreeGC(display, gc);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XFreeGC", rc);
}

Pixmap LogDynAndXCreatePixmap_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                 Display* display, Drawable d, unsigned int width,
                                 unsigned int height, unsigned int depth)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XCreatePixmap(%p, 0x%08lu, %u, %u, %u)",
		          (void*)display, d, width, height, depth);
	}
	return XCreatePixmap(display, d, width, height, depth);
}

int LogDynAndXFreePixmap_ex(wLog* log, const char* file, const char* fkt, size_t line,
                            Display* display, Pixmap pixmap)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XFreePixmap(%p)", (void*)display);
	}
	const int rc = XFreePixmap(display, pixmap);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XFreePixmap", rc);
}

int LogDynAndXSetSelectionOwner_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                   Display* display, Atom selection, Window owner, Time time)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* selectionstr = Safe_XGetAtomName(log, display, selection);
		write_log(log, log_level, file, fkt, line, "XSetSelectionOwner(%p, %s, 0x%08lu, %lu)",
		          (void*)display, selectionstr, owner, time);
		XFree(selectionstr);
	}
	const int rc = XSetSelectionOwner(display, selection, owner, time);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display,
	                                   "XSetSelectionOwner", rc);
}

int LogDynAndXSetForeground_ex(wLog* log, const char* file, const char* fkt, size_t line,
                               Display* display, GC gc, unsigned long foreground)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XSetForeground(%p, %p, 0x%08lu)",
		          (void*)display, (void*)gc, foreground);
	}
	const int rc = XSetForeground(display, gc, foreground);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XSetForeground",
	                                   rc);
}

int LogDynAndXMoveWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                            Display* display, Window w, int x, int y)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XMoveWindow(%p, 0x%08lu, %d, %d)",
		          (void*)display, w, x, y);
	}
	const int rc = XMoveWindow(display, w, x, y);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XMoveWindow", rc);
}

int LogDynAndXSetFillStyle_ex(wLog* log, const char* file, const char* fkt, size_t line,
                              Display* display, GC gc, int fill_style)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XSetFillStyle(%p, %p, %d)", (void*)display,
		          (void*)gc, fill_style);
	}
	const int rc = XSetFillStyle(display, gc, fill_style);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XSetFillStyle",
	                                   rc);
}

int LogDynAndXSetFunction_ex(wLog* log, const char* file, const char* fkt, size_t line,
                             Display* display, GC gc, int function)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XSetFunction(%p, %p, %d)", (void*)display,
		          (void*)gc, function);
	}
	const int rc = XSetFunction(display, gc, function);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XSetFunction",
	                                   rc);
}

int LogDynAndXRaiseWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                             Display* display, Window w)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XRaiseWindow(%p, %lu)", (void*)display, w);
	}
	const int rc = XRaiseWindow(display, w);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XRaiseWindow",
	                                   rc);
}

int LogDynAndXMapWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                           Display* display, Window w)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XMapWindow(%p, %lu)", (void*)display, w);
	}
	const int rc = XMapWindow(display, w);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XMapWindow", rc);
}

int LogDynAndXUnmapWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                             Display* display, Window w)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XUnmapWindow(%p, %lu)", (void*)display, w);
	}
	const int rc = XUnmapWindow(display, w);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XUnmapWindow",
	                                   rc);
}

int LogDynAndXMoveResizeWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                  Display* display, Window w, int x, int y, unsigned int width,
                                  unsigned int height)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XMoveResizeWindow(%p, %lu, %d, %d, %u, %u)",
		          (void*)display, w, x, y, width, height);
	}
	const int rc = XMoveResizeWindow(display, w, x, y, width, height);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display,
	                                   "XMoveResizeWindow", rc);
}

Status LogDynAndXWithdrawWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                   Display* display, Window w, int screen_number)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XWithdrawWindow(%p, %lu, %d)", (void*)display,
		          w, screen_number);
	}
	const Status rc = XWithdrawWindow(display, w, screen_number);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XWithdrawWindow",
	                                   rc);
}

int LogDynAndXResizeWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                              Display* display, Window w, unsigned int width, unsigned int height)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XResizeWindow(%p, %lu, %u, %u)", (void*)display,
		          w, width, height);
	}
	const int rc = XResizeWindow(display, w, width, height);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XResizeWindow",
	                                   rc);
}

int LogDynAndXClearWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                             Display* display, Window w)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XClearWindow(%p, %lu)", (void*)display, w);
	}
	const int rc = XClearWindow(display, w);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XClearWindow",
	                                   rc);
}

int LogDynAndXSetBackground_ex(wLog* log, const char* file, const char* fkt, size_t line,
                               Display* display, GC gc, unsigned long background)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XSetBackground(%p, %p, %lu)", (void*)display,
		          (void*)gc, background);
	}
	const int rc = XSetBackground(display, gc, background);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XSetBackground",
	                                   rc);
}

int LogDynAndXSetClipMask_ex(wLog* log, const char* file, const char* fkt, size_t line,
                             Display* display, GC gc, Pixmap pixmap)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XSetClipMask(%p, %p, %lu)", (void*)display,
		          (void*)gc, pixmap);
	}
	const int rc = XSetClipMask(display, gc, pixmap);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XSetClipMask",
	                                   rc);
}

int LogDynAndXFillRectangle_ex(wLog* log, const char* file, const char* fkt, size_t line,
                               Display* display, Window w, GC gc, int x, int y, unsigned int width,
                               unsigned int height)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XFillRectangle(%p, %lu, %p, %d, %d, %u, %u)",
		          (void*)display, w, (void*)gc, x, y, width, height);
	}
	const int rc = XFillRectangle(display, w, gc, x, y, width, height);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XFillRectangle",
	                                   rc);
}

int LogDynAndXSetRegion_ex(wLog* log, const char* file, const char* fkt, size_t line,
                           Display* display, GC gc, Region r)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XSetRegion(%p, %p, %p)", (void*)display,
		          (void*)gc, (void*)r);
	}
	const int rc = XSetRegion(display, gc, r);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XSetRegion", rc);
}

int LogDynAndXReparentWindow_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Window parent, int x, int y)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XReparentWindow(%p, %lu, %lu, %d, %d)",
		          (void*)display, w, parent, x, y);
	}
	const int rc = XReparentWindow(display, w, parent, x, y);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XReparentWindow",
	                                   rc);
}

char* getConfigOption(BOOL system, const char* option)
{
	char* res = NULL;
	WINPR_JSON* file = freerdp_GetJSONConfigFile(system, "xfreerdp.json");
	if (!file)
		return NULL;

	WINPR_JSON* obj = WINPR_JSON_GetObjectItemCaseSensitive(file, option);
	if (obj)
	{
		const char* val = WINPR_JSON_GetStringValue(obj);
		if (val)
			res = _strdup(val);
	}
	WINPR_JSON_Delete(file);

	return res;
}

int LogDynAndXRestackWindows_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window* windows, int nwindows)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		write_log(log, log_level, file, fkt, line, "XRestackWindows(%p, %p, %d)", (void*)display,
		          (const void*)windows, nwindows);
	}
	const int rc = XRestackWindows(display, windows, nwindows);
	return write_result_log_expect_one(log, WLOG_WARN, file, fkt, line, display, "XRestackWindows",
	                                   rc);
}

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

#define LogTagAndXGetWindowProperty(tag, display, w, property, long_offset, long_length, delete,   \
                                    req_type, actual_type_return, actual_format_return,            \
                                    nitems_return, bytes_after_return, prop_return)                \
	LogTagAndXGetWindowProperty_ex((tag), __FILE__, __func__, __LINE__, (display), (w),            \
	                               (property), (long_offset), (long_length), (delete), (req_type), \
	                               (actual_type_return), (actual_format_return), (nitems_return),  \
	                               (bytes_after_return), (prop_return))
int LogTagAndXGetWindowProperty_ex(const char* tag, const char* file, const char* fkt, size_t line,
                                   Display* display, Window w, Atom property, long long_offset,
                                   long long_length, Bool delete, Atom req_type,
                                   Atom* actual_type_return, int* actual_format_return,
                                   unsigned long* nitems_return, unsigned long* bytes_after_return,
                                   unsigned char** prop_return);

#define LogDynAndXGetWindowProperty(log, display, w, property, long_offset, long_length, delete,   \
                                    req_type, actual_type_return, actual_format_return,            \
                                    nitems_return, bytes_after_return, prop_return)                \
	LogDynAndXGetWindowProperty_ex((log), __FILE__, __func__, __LINE__, (display), (w),            \
	                               (property), (long_offset), (long_length), (delete), (req_type), \
	                               (actual_type_return), (actual_format_return), (nitems_return),  \
	                               (bytes_after_return), (prop_return))
int LogDynAndXGetWindowProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                   Display* display, Window w, Atom property, long long_offset,
                                   long long_length, Bool delete, Atom req_type,
                                   Atom* actual_type_return, int* actual_format_return,
                                   unsigned long* nitems_return, unsigned long* bytes_after_return,
                                   unsigned char** prop_return);

#define LogTagAndXChangeProperty(tag, display, w, property, type, format, mode, data, nelements) \
	LogTagAndXChangeProperty_ex((tag), __FILE__, __func__, __LINE__, (display), (w), (property), \
	                            (type), (format), (mode), (data), (nelements))
int LogTagAndXChangeProperty_ex(const char* tag, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property, Atom type, int format,
                                int mode, _Xconst unsigned char* data, int nelements);

#define LogDynAndXChangeProperty(log, display, w, property, type, format, mode, data, nelements) \
	LogDynAndXChangeProperty_ex((log), __FILE__, __func__, __LINE__, (display), (w), (property), \
	                            (type), (format), (mode), (data), (nelements))
int LogDynAndXChangeProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property, Atom type, int format,
                                int mode, _Xconst unsigned char* data, int nelements);

#define LogTagAndXDeleteProperty(tag, display, w, property) \
	LogTagAndXDeleteProperty_ex((tag), __FILE__, __func__, __LINE__, (display), (w), (property))
int LogTagAndXDeleteProperty_ex(const char* tag, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property);

#define LogDynAndXDeleteProperty(log, display, w, property) \
	LogDynAndXDeleteProperty_ex((log), __FILE__, __func__, __LINE__, (display), (w), (property))
int LogDynAndXDeleteProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property);

#define LogTagAndXConvertSelection(tag, display, selection, target, property, requestor, time) \
	LogTagAndXConvertSelection_ex((tag), __FILE__, __func__, __LINE__, (display), (selection), \
	                              (target), (property), (requestor), (time))
int LogTagAndXConvertSelection_ex(const char* tag, const char* file, const char* fkt, size_t line,
                                  Display* display, Atom selection, Atom target, Atom property,
                                  Window requestor, Time time);

#define LogDynAndXConvertSelection(log, display, selection, target, property, requestor, time) \
	LogDynAndXConvertSelection_ex((log), __FILE__, __func__, __LINE__, (display), (w), (property))
int LogDynAndXConvertSelection_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                  Display* display, Atom selection, Atom target, Atom property,
                                  Window requestor, Time time);

BOOL IsGnome(void);

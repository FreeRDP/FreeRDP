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

#include <X11/Xlib.h>

int LogTagAndXGetWindowProperty(const char* tag, Display* display, Window w, Atom property,
                                long long_offset, long long_length, Bool delete, Atom req_type,
                                Atom* actual_type_return, int* actual_format_return,
                                unsigned long* nitems_return, unsigned long* bytes_after_return,
                                unsigned char** prop_return);
int LogDynAndXGetWindowProperty(wLog* log, Display* display, Window w, Atom property,
                                long long_offset, long long_length, Bool delete, Atom req_type,
                                Atom* actual_type_return, int* actual_format_return,
                                unsigned long* nitems_return, unsigned long* bytes_after_return,
                                unsigned char** prop_return);

int LogTagAndXChangeProperty(const char* tag, Display* display, Window w, Atom property, Atom type,
                             int format, int mode, _Xconst unsigned char* data, int nelements);
int LogDynAndXChangeProperty(wLog* log, Display* display, Window w, Atom property, Atom type,
                             int format, int mode, _Xconst unsigned char* data, int nelements);

int LogTagAndXDeleteProperty(const char* tag, Display* display, Window w, Atom property);
int LogDynAndXDeleteProperty(wLog* log, Display* display, Window w, Atom property);

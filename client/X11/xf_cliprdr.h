/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Clipboard Redirection
 *
 * Copyright 2010-2011 Vic Lee
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

#ifndef __XF_CLIPRDR_H
#define __XF_CLIPRDR_H

#include "xfreerdp.h"

void xf_cliprdr_init(xfInfo* xfi, rdpChannels* chanman);
void xf_cliprdr_uninit(xfInfo* xfi);
void xf_process_cliprdr_event(xfInfo* xfi, RDP_EVENT* event);
boolean xf_cliprdr_process_selection_notify(xfInfo* xfi, XEvent* xevent);
boolean xf_cliprdr_process_selection_request(xfInfo* xfi, XEvent* xevent);
boolean xf_cliprdr_process_selection_clear(xfInfo* xfi, XEvent* xevent);
boolean xf_cliprdr_process_property_notify(xfInfo* xfi, XEvent* xevent);
void xf_cliprdr_check_owner(xfInfo* xfi);

#ifdef WITH_DEBUG_X11_CLIPRDR
#define DEBUG_X11_CLIPRDR(fmt, ...) DEBUG_CLASS(X11_CLIPRDR, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11_CLIPRDR(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __XF_CLIPRDR_H */

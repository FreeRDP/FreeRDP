/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_CLIENT_X11_CLIPRDR_H
#define FREERDP_CLIENT_X11_CLIPRDR_H

#include "xf_client.h"
#include "xfreerdp.h"

#include <freerdp/client/cliprdr.h>

void xf_clipboard_free(xfClipboard* clipboard);

WINPR_ATTR_MALLOC(xf_clipboard_free, 1)
xfClipboard* xf_clipboard_new(xfContext* xfc, BOOL relieveFilenameRestriction);

void xf_cliprdr_init(xfContext* xfc, CliprdrClientContext* cliprdr);
void xf_cliprdr_uninit(xfContext* xfc, CliprdrClientContext* cliprdr);

void xf_cliprdr_handle_xevent(xfContext* xfc, const XEvent* event);

#endif /* FREERDP_CLIENT_X11_CLIPRDR_H */

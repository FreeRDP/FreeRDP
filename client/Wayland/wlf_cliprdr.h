/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Clipboard Redirection
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#ifndef FREERDP_CLIENT_WAYLAND_CLIPRDR_H
#define FREERDP_CLIENT_WAYLAND_CLIPRDR_H

#include "wlfreerdp.h"

#include <freerdp/client/cliprdr.h>

wfClipboard* wlf_clipboard_new(wlfContext* wlc);
void wlf_clipboard_free(wfClipboard* clipboard);

BOOL wlf_cliprdr_init(wfClipboard* clipboard, CliprdrClientContext* cliprdr);
BOOL wlf_cliprdr_uninit(wfClipboard* clipboard, CliprdrClientContext* cliprdr);

BOOL wlf_cliprdr_handle_event(wfClipboard* clipboard, const UwacClipboardEvent* event);

#endif /* FREERDP_CLIENT_WAYLAND_CLIPRDR_H */

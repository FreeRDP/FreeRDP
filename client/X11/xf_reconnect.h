/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Windows
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

#ifndef FREERDP_CLIENT_X11_RECONNECT_H
#define FREERDP_CLIENT_X11_RECONNECT_H
#include "xfreerdp.h"
SSIZE_T xf_retry_dialog(freerdp* instance, const char* what, size_t current, void* userarg);
BOOL xf_reconnect_event(xfContext* xfc, const XEvent* event);
void xf_reconnect_close(xfContext* xfc);
#endif

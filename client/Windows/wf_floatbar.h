/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Float Bar
 *
 * Copyright 2013 Zhang Zhaolong <zhangzl2013@126.com>
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

#ifndef FREERDP_CLIENT_WIN_FLOATBAR_H
#define FREERDP_CLIENT_WIN_FLOATBAR_H

#include <winpr/crt.h>

typedef struct _FloatBar wfFloatBar;
typedef struct wf_context wfContext;

wfFloatBar* wf_floatbar_new(wfContext* wfc, HINSTANCE window, DWORD flags);
void wf_floatbar_free(wfFloatBar* floatbar);

BOOL wf_floatbar_toggle_fullscreen(wfFloatBar* floatbar, BOOL fullscreen);

#endif /* FREERDP_CLIENT_WIN_FLOATBAR_H */

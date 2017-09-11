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

typedef struct _FloatBar FloatBar;
typedef struct wf_context wfContext;

void floatbar_window_create(wfContext* wfc);
int floatbar_show(FloatBar* floatbar);
int floatbar_hide(FloatBar* floatbar);

#endif /* FREERDP_CLIENT_WIN_FLOATBAR_H */

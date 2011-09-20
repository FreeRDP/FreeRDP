/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Video Redirection
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

#ifndef __XF_TSMF_H
#define __XF_TSMF_H

#include "xfreerdp.h"

void xf_tsmf_init(xfInfo* xfi, long xv_port);
void xf_tsmf_uninit(xfInfo* xfi);
void xf_process_tsmf_event(xfInfo* xfi, RDP_EVENT* event);

#endif /* __XF_TSMF_H */

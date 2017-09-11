/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_CLIENT_X11_TSMF_H
#define FREERDP_CLIENT_X11_TSMF_H

#include "xf_client.h"
#include "xfreerdp.h"

int xf_tsmf_init(xfContext* xfc, TsmfClientContext* tsmf);
int xf_tsmf_uninit(xfContext* xfc, TsmfClientContext* tsmf);

#endif /* FREERDP_CLIENT_X11_TSMF_H */

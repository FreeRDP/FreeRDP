/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Display Control channel
 *
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
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
#ifndef FREERDP_CLIENT_X11_DISP_H
#define FREERDP_CLIENT_X11_DISP_H

#include <freerdp/types.h>
#include <freerdp/client/disp.h>

#include "xf_client.h"
#include "xfreerdp.h"

typedef struct _xfDispContext xfDispContext;

FREERDP_API BOOL xf_disp_init(xfContext* xfc, DispClientContext *disp);

xfDispContext *xf_disp_new(xfContext* xfc);
void xf_disp_free(xfDispContext *disp);
BOOL xf_disp_handle_xevent(xfContext *xfc, XEvent *event);
BOOL xf_disp_handle_configureNotify(xfContext *xfc, int width, int height);
void xf_disp_resized(xfDispContext *disp);

#endif /* FREERDP_CLIENT_X11_DISP_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Input
 *
 * Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

#ifndef FREERDP_CLIENT_X11_INPUT_H
#define FREERDP_CLIENT_X11_INPUT_H

#include "xf_client.h"
#include "xfreerdp.h"

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#endif

int xf_input_init(xfContext* xfc, Window window);
int xf_input_handle_event(xfContext* xfc, const XEvent* event);

#endif /* FREERDP_CLIENT_X11_INPUT_H */

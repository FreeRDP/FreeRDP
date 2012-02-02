/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 RemoteFX Encoder
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __XF_ENCODE_H
#define __XF_ENCODE_H

#include <pthread.h>
#include "xfreerdp.h"

#include "xf_peer.h"

XImage* xf_snapshot(xfPeerContext* xfp, int x, int y, int width, int height);
void xf_xdamage_subtract_region(xfPeerContext* xfp, int x, int y, int width, int height);
void* xf_monitor_updates(void* param);

#endif /* __XF_ENCODE_H */

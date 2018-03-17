/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Optimized Remoting Virtual Channel Extension for X11
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
#ifndef CLIENT_X11_XF_VIDEO_H_
#define CLIENT_X11_XF_VIDEO_H_

#include "xfreerdp.h"

#include <freerdp/channels/geometry.h>
#include <freerdp/channels/video.h>

void xf_video_control_init(xfContext* xfc, VideoClientContext* video);
void xf_video_control_uninit(xfContext* xfc, VideoClientContext* video);

xfVideoContext* xf_video_new(xfContext* xfc);
void xf_video_free(xfVideoContext* context);


#endif /* CLIENT_X11_XF_VIDEO_H_ */

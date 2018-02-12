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
#ifndef FREERDP_GDI_VIDEO_H_
#define FREERDP_GDI_VIDEO_H_

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/client/geometry.h>
#include <freerdp/client/video.h>

struct _gdiVideoContext;
typedef struct _gdiVideoContext gdiVideoContext;

FREERDP_API void gdi_video_geometry_init(rdpGdi* gdi, GeometryClientContext* geom);
FREERDP_API void gdi_video_geometry_uninit(rdpGdi* gdi, GeometryClientContext* geom);

FREERDP_API void gdi_video_control_init(rdpGdi* gdi, VideoClientContext* video);
FREERDP_API void gdi_video_control_uninit(rdpGdi* gdi, VideoClientContext* video);

FREERDP_API void gdi_video_data_init(rdpGdi* gdi, VideoClientContext* video);
FREERDP_API void gdi_video_data_uninit(rdpGdi* gdi, VideoClientContext* context);

FREERDP_API gdiVideoContext* gdi_video_new(rdpGdi* gdi);
FREERDP_API void gdi_video_free(gdiVideoContext* context);


#endif /* FREERDP_GDI_VIDEO_H_ */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, V4L Interface
 *
 * Copyright 2025 Oleg Turovski <oleg2104@hotmail.com>
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

#ifndef CAMERA_V4L_H
#define CAMERA_V4L_H

#include <winpr/synch.h>
#include <winpr/wtypes.h>

#include "../camera.h"

typedef struct
{
	void* start;
	size_t length;

} CamV4lBuffer;

typedef struct
{
	CRITICAL_SECTION lock;

	/* members used to call the callback */
	CameraDevice* dev;
	int streamIndex;
	ICamHalSampleCapturedCallback sampleCallback;

	BOOL streaming;
	int fd;
	uint8_t h264UnitId; /* UVC H264 UnitId, if 0 then UVC H264 is not supported */

	size_t nBuffers;
	CamV4lBuffer* buffers;
	HANDLE captureThread;

} CamV4lStream;

#endif /* CAMERA_V4L_H */

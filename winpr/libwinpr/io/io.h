/**
 * WinPR: Windows Portable Runtime
 * Asynchronous I/O Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_IO_PRIVATE_H
#define WINPR_IO_PRIVATE_H

#ifndef _WIN32

#include <winpr/io.h>

#include "../handle/handle.h"

typedef struct _DEVICE_OBJECT_EX DEVICE_OBJECT_EX;

struct _DEVICE_OBJECT_EX
{
	char* DeviceName;
	char* DeviceFileName;
};

#endif

#endif /* WINPR_IO_PRIVATE_H */

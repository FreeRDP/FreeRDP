/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012-2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __RDPSND_MAIN_H
#define __RDPSND_MAIN_H

#include <freerdp/api.h>
#include <freerdp/svc.h>
#include <freerdp/addin.h>
#include <freerdp/client/rdpsnd.h>

#if defined(WITH_DEBUG_SND)
#define DEBUG_SND(fmt, ...) DEBUG_CLASS("rdpsnd", fmt, ## __VA_ARGS__)
#else
#define DEBUG_SND(fmt, ...) do { } while (0)
#endif

int rdpsnd_virtual_channel_write(rdpsndPlugin* rdpsnd, wStream* s);

#endif /* __RDPSND_MAIN_H */

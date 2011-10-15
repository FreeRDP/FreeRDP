/*
   FreeRDP: A Remote Desktop Protocol client.
   Redirected Smart Card Device Service

   Copyright 2011 O.S. Systems Software Ltda.
   Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef __SCARD_MAIN_H
#define __SCARD_MAIN_H

#include <inttypes.h>

#include "devman.h"
#include "rdpdr_types.h"
#include <freerdp/utils/debug.h>

struct _SCARD_DEVICE
{
	DEVICE device;

	char * name;
	char * path;

	LIST* irp_list;

	freerdp_thread* thread;
};
typedef struct _SCARD_DEVICE SCARD_DEVICE;

#ifdef WITH_DEBUG_SCARD
#define DEBUG_SCARD(fmt, ...) DEBUG_CLASS(SCARD, fmt, ## __VA_ARGS__)
#else
#define DEBUG_SCARD(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

boolean scard_async_op(IRP*);
void scard_device_control(SCARD_DEVICE*, IRP*);

#endif

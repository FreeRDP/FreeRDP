/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
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

#ifndef FREERDP_CHANNEL_SMARTCARD_CLIENT_MAIN_H
#define FREERDP_CHANNEL_SMARTCARD_CLIENT_MAIN_H

#include <freerdp/utils/list.h>
#include <freerdp/utils/debug.h>
#include <freerdp/channels/rdpdr.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/smartcard.h>
#include <winpr/collections.h>

struct _COMPLETIONIDINFO
{
        UINT32 ID;
        BOOL duplicate;
};
typedef struct _COMPLETIONIDINFO COMPLETIONIDINFO;

struct _SMARTCARD_DEVICE
{
	DEVICE device;

	char* name;
	char* path;

	HANDLE thread;
	wMessageQueue* IrpQueue;

	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
};
typedef struct _SMARTCARD_DEVICE SMARTCARD_DEVICE;

#ifdef WITH_DEBUG_SCARD
#define DEBUG_SCARD(fmt, ...) DEBUG_CLASS(SCARD, fmt, ## __VA_ARGS__)
#else
#define DEBUG_SCARD(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

BOOL smartcard_async_op(IRP* irp);
void smartcard_device_control(SMARTCARD_DEVICE* smartcard, IRP* irp);

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_MAIN_H */

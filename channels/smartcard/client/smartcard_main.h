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

#include <freerdp/utils/debug.h>
#include <freerdp/channels/rdpdr.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/synch.h>
#include <winpr/smartcard.h>
#include <winpr/collections.h>

/* [MS-RDPESC] 3.1.4 */
#define SCARD_IOCTL_ESTABLISH_CONTEXT		0x00090014	/* EstablishContext */
#define SCARD_IOCTL_RELEASE_CONTEXT		0x00090018	/* ReleaseContext */
#define SCARD_IOCTL_IS_VALID_CONTEXT		0x0009001C	/* IsValidContext */
#define SCARD_IOCTL_LIST_READER_GROUPS		0x00090020	/* ListReaderGroups */
#define SCARD_IOCTL_LIST_READERS		0x00090028	/* ListReadersA */
#define SCARD_IOCTL_INTRODUCE_READER_GROUP	0x00090050	/* IntroduceReaderGroup */
#define SCARD_IOCTL_FORGET_READER_GROUP		0x00090058	/* ForgetReader */
#define SCARD_IOCTL_INTRODUCE_READER		0x00090060	/* IntroduceReader */
#define SCARD_IOCTL_FORGET_READER		0x00090068	/* IntroduceReader */
#define SCARD_IOCTL_ADD_READER_TO_GROUP		0x00090070	/* AddReaderToGroup */
#define SCARD_IOCTL_REMOVE_READER_FROM_GROUP	0x00090078	/* RemoveReaderFromGroup */
#define SCARD_IOCTL_GET_STATUS_CHANGE		0x000900A0	/* GetStatusChangeA */
#define SCARD_IOCTL_CANCEL			0x000900A8	/* Cancel */
#define SCARD_IOCTL_CONNECT			0x000900AC	/* ConnectA */
#define SCARD_IOCTL_RECONNECT			0x000900B4	/* Reconnect */
#define SCARD_IOCTL_DISCONNECT			0x000900B8	/* Disconnect */
#define SCARD_IOCTL_BEGIN_TRANSACTION		0x000900BC	/* BeginTransaction */
#define SCARD_IOCTL_END_TRANSACTION		0x000900C0	/* EndTransaction */
#define SCARD_IOCTL_STATE			0x000900C4	/* State */
#define SCARD_IOCTL_STATUS			0x000900C8	/* StatusA */
#define SCARD_IOCTL_TRANSMIT			0x000900D0	/* Transmit */
#define SCARD_IOCTL_CONTROL			0x000900D4	/* Control */
#define SCARD_IOCTL_GETATTRIB			0x000900D8	/* GetAttrib */
#define SCARD_IOCTL_SETATTRIB			0x000900DC	/* SetAttrib */
#define SCARD_IOCTL_ACCESS_STARTED_EVENT	0x000900E0	/* SCardAccessStartedEvent */
#define SCARD_IOCTL_LOCATE_CARDS_BY_ATR		0x000900E8	/* LocateCardsByATR */

struct _SMARTCARD_DEVICE
{
	DEVICE device;

	wLog* log;

	char* name;
	char* path;

	HANDLE thread;
	wMessageQueue* IrpQueue;

	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
};
typedef struct _SMARTCARD_DEVICE SMARTCARD_DEVICE;

BOOL smartcard_async_op(IRP* irp);
void smartcard_device_control(SMARTCARD_DEVICE* smartcard, IRP* irp);

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_MAIN_H */

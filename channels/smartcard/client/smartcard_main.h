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

#define RDP_SCARD_CTL_CODE(code)	CTL_CODE(FILE_DEVICE_FILE_SYSTEM, (code), METHOD_BUFFERED, FILE_ANY_ACCESS)

#define SCARD_IOCTL_ESTABLISHCONTEXT		RDP_SCARD_CTL_CODE(5)	/* SCardEstablishContext */
#define SCARD_IOCTL_RELEASECONTEXT		RDP_SCARD_CTL_CODE(6)	/* SCardReleaseContext */
#define SCARD_IOCTL_ISVALIDCONTEXT		RDP_SCARD_CTL_CODE(7)	/* SCardIsValidContext */
#define SCARD_IOCTL_LISTREADERGROUPSA		RDP_SCARD_CTL_CODE(8)	/* SCardListReaderGroupsA */
#define SCARD_IOCTL_LISTREADERGROUPSW		RDP_SCARD_CTL_CODE(9)	/* SCardListReaderGroupsW */
#define SCARD_IOCTL_LISTREADERSA		RDP_SCARD_CTL_CODE(10)	/* SCardListReadersA */
#define SCARD_IOCTL_LISTREADERSW		RDP_SCARD_CTL_CODE(11)	/* SCardListReadersW */
#define SCARD_IOCTL_INTRODUCEREADERGROUPA	RDP_SCARD_CTL_CODE(20)	/* SCardIntroduceReaderGroupA */
#define SCARD_IOCTL_INTRODUCEREADERGROUPW	RDP_SCARD_CTL_CODE(21)	/* SCardIntroduceReaderGroupW */
#define SCARD_IOCTL_FORGETREADERGROUPA		RDP_SCARD_CTL_CODE(22)	/* SCardForgetReaderGroupA */
#define SCARD_IOCTL_FORGETREADERGROUPW		RDP_SCARD_CTL_CODE(23)	/* SCardForgetReaderGroupW */
#define SCARD_IOCTL_INTRODUCEREADERA		RDP_SCARD_CTL_CODE(24)	/* SCardIntroduceReaderA */
#define SCARD_IOCTL_INTRODUCEREADERW		RDP_SCARD_CTL_CODE(25)	/* SCardIntroduceReaderW */
#define SCARD_IOCTL_FORGETREADERA		RDP_SCARD_CTL_CODE(26)	/* SCardForgetReaderA */
#define SCARD_IOCTL_FORGETREADERW		RDP_SCARD_CTL_CODE(27)	/* SCardForgetReaderW */
#define SCARD_IOCTL_ADDREADERTOGROUPA		RDP_SCARD_CTL_CODE(28)	/* SCardAddReaderToGroupA */
#define SCARD_IOCTL_ADDREADERTOGROUPW		RDP_SCARD_CTL_CODE(29)	/* SCardAddReaderToGroupW */
#define SCARD_IOCTL_REMOVEREADERFROMGROUPA	RDP_SCARD_CTL_CODE(30)	/* SCardRemoveReaderFromGroupA */
#define SCARD_IOCTL_REMOVEREADERFROMGROUPW	RDP_SCARD_CTL_CODE(31)	/* SCardRemoveReaderFromGroupW */
#define SCARD_IOCTL_LOCATECARDSA		RDP_SCARD_CTL_CODE(38)	/* SCardLocateCardsA */
#define SCARD_IOCTL_LOCATECARDSW		RDP_SCARD_CTL_CODE(39)	/* SCardLocateCardsW */
#define SCARD_IOCTL_GETSTATUSCHANGEA		RDP_SCARD_CTL_CODE(40)	/* SCardGetStatusChangeA */
#define SCARD_IOCTL_GETSTATUSCHANGEW		RDP_SCARD_CTL_CODE(41)	/* SCardGetStatusChangeW */
#define SCARD_IOCTL_CANCEL			RDP_SCARD_CTL_CODE(42)	/* SCardCancel */
#define SCARD_IOCTL_CONNECTA			RDP_SCARD_CTL_CODE(43)	/* SCardConnectA */
#define SCARD_IOCTL_CONNECTW			RDP_SCARD_CTL_CODE(44)	/* SCardConnectW */
#define SCARD_IOCTL_RECONNECT			RDP_SCARD_CTL_CODE(45)	/* SCardReconnect */
#define SCARD_IOCTL_DISCONNECT			RDP_SCARD_CTL_CODE(46)	/* SCardDisconnect */
#define SCARD_IOCTL_BEGINTRANSACTION		RDP_SCARD_CTL_CODE(47)	/* SCardBeginTransaction */
#define SCARD_IOCTL_ENDTRANSACTION		RDP_SCARD_CTL_CODE(48)	/* SCardEndTransaction */
#define SCARD_IOCTL_STATE			RDP_SCARD_CTL_CODE(49)	/* SCardState */
#define SCARD_IOCTL_STATUSA			RDP_SCARD_CTL_CODE(50)	/* SCardStatusA */
#define SCARD_IOCTL_STATUSW			RDP_SCARD_CTL_CODE(51)	/* SCardStatusW */
#define SCARD_IOCTL_TRANSMIT			RDP_SCARD_CTL_CODE(52)	/* SCardTransmit */
#define SCARD_IOCTL_CONTROL			RDP_SCARD_CTL_CODE(53)	/* SCardControl */
#define SCARD_IOCTL_GETATTRIB			RDP_SCARD_CTL_CODE(54)	/* SCardGetAttrib */
#define SCARD_IOCTL_SETATTRIB			RDP_SCARD_CTL_CODE(55)	/* SCardSetAttrib */
#define SCARD_IOCTL_ACCESSSTARTEDEVENT		RDP_SCARD_CTL_CODE(56)	/* SCardAccessStartedEvent */
#define SCARD_IOCTL_LOCATECARDSBYATRA		RDP_SCARD_CTL_CODE(58)	/* SCardLocateCardsByATRA */
#define SCARD_IOCTL_LOCATECARDSBYATRW		RDP_SCARD_CTL_CODE(59)	/* SCardLocateCardsByATRW */
#define SCARD_IOCTL_READCACHEA			RDP_SCARD_CTL_CODE(60)	/* SCardReadCacheA */
#define SCARD_IOCTL_READCACHEW			RDP_SCARD_CTL_CODE(61)	/* SCardReadCacheW */
#define SCARD_IOCTL_WRITECACHEA			RDP_SCARD_CTL_CODE(62)	/* SCardWriteCacheA */
#define SCARD_IOCTL_WRITECACHEW			RDP_SCARD_CTL_CODE(63)	/* SCardWriteCacheW */
#define SCARD_IOCTL_GETTRANSMITCOUNT		RDP_SCARD_CTL_CODE(64)	/* SCardGetTransmitCount */
#define SCARD_IOCTL_RELEASESTARTEDEVENT		RDP_SCARD_CTL_CODE(66)	/* SCardReleaseStartedEvent */
#define SCARD_IOCTL_GETREADERICON		RDP_SCARD_CTL_CODE(67)	/* SCardGetReaderIconA */
#define SCARD_IOCTL_GETDEVICETYPEID		RDP_SCARD_CTL_CODE(68)	/* SCardGetDeviceTypeIdA */

struct _SMARTCARD_DEVICE
{
	DEVICE device;

	wLog* log;

	char* name;
	char* path;

	HANDLE thread;
	wMessageQueue* IrpQueue;
	wQueue* CompletedIrpQueue;
	wListDictionary* rgSCardContextList;
	wListDictionary* rgOutstandingMessages;
};
typedef struct _SMARTCARD_DEVICE SMARTCARD_DEVICE;

void smartcard_complete_irp(SMARTCARD_DEVICE* smartcard, IRP* irp);
void smartcard_process_irp(SMARTCARD_DEVICE* smartcard, IRP* irp);

void smartcard_irp_device_control(SMARTCARD_DEVICE* smartcard, IRP* irp);
void smartcard_irp_device_control_peek_io_control_code(SMARTCARD_DEVICE* smartcard, IRP* irp, UINT32* ioControlCode);

#include "smartcard_pack.h"

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_MAIN_H */

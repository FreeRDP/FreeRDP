/**
 * WinPR: Windows Portable Runtime
 * Smart Card API
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * libpcsc-winpr.a:
 *
 * Add the WinPR_PCSC_* definitions
 * to pcsc-lite/src/PCSC/winscard.h
 *
 * Configure project as static in a local prefix:
 * ./configure --enable-static --prefix=/usr/local
 *
 * Install, and then rename
 * libpcsclite.a to libpcsc-winpr.a
 * 
 * This static library will contain a non-conflicting ABI
 * so we can safely link to it with and import its functions.
 */

#if 0
#define WinPR_PCSC
#define SCardEstablishContext WinPR_PCSC_SCardEstablishContext
#define SCardReleaseContext WinPR_PCSC_SCardReleaseContext
#define SCardIsValidContext WinPR_PCSC_SCardIsValidContext
#define SCardConnect WinPR_PCSC_SCardConnect
#define SCardReconnect WinPR_PCSC_SCardReconnect
#define SCardDisconnect WinPR_PCSC_SCardDisconnect
#define SCardBeginTransaction WinPR_PCSC_SCardBeginTransaction
#define SCardEndTransaction WinPR_PCSC_SCardEndTransaction
#define SCardStatus WinPR_PCSC_SCardStatus
#define SCardGetStatusChange WinPR_PCSC_SCardGetStatusChange
#define SCardControl WinPR_PCSC_SCardControl
#define SCardTransmit WinPR_PCSC_SCardTransmit
#define SCardListReaderGroups WinPR_PCSC_SCardListReaderGroups
#define SCardListReaders WinPR_PCSC_SCardListReaders
#define SCardFreeMemory WinPR_PCSC_SCardFreeMemory
#define SCardCancel WinPR_PCSC_SCardCancel
#define SCardGetAttrib WinPR_PCSC_SCardGetAttrib
#define SCardSetAttrib WinPR_PCSC_SCardSetAttrib
#define list_size WinPR_PCSC_list_size /* src/simclist.h */
#endif

#ifdef WITH_WINPR_PCSC
extern void* WinPR_PCSC_SCardEstablishContext();
extern void* WinPR_PCSC_SCardReleaseContext();
extern void* WinPR_PCSC_SCardIsValidContext();
extern void* WinPR_PCSC_SCardConnect();
extern void* WinPR_PCSC_SCardReconnect();
extern void* WinPR_PCSC_SCardDisconnect();
extern void* WinPR_PCSC_SCardBeginTransaction();
extern void* WinPR_PCSC_SCardEndTransaction();
extern void* WinPR_PCSC_SCardStatus();
extern void* WinPR_PCSC_SCardGetStatusChange();
extern void* WinPR_PCSC_SCardControl();
extern void* WinPR_PCSC_SCardTransmit();
extern void* WinPR_PCSC_SCardListReaderGroups();
extern void* WinPR_PCSC_SCardListReaders();
extern void* WinPR_PCSC_SCardFreeMemory();
extern void* WinPR_PCSC_SCardCancel();
extern void* WinPR_PCSC_SCardGetAttrib();
extern void* WinPR_PCSC_SCardSetAttrib();
#endif

struct _PCSCFunctionTable
{
	void* pfnSCardEstablishContext;
	void* pfnSCardReleaseContext;
	void* pfnSCardIsValidContext;
	void* pfnSCardConnect;
	void* pfnSCardReconnect;
	void* pfnSCardDisconnect;
	void* pfnSCardBeginTransaction;
	void* pfnSCardEndTransaction;
	void* pfnSCardStatus;
	void* pfnSCardGetStatusChange;
	void* pfnSCardControl;
	void* pfnSCardTransmit;
	void* pfnSCardListReaderGroups;
	void* pfnSCardListReaders;
	void* pfnSCardFreeMemory;
	void* pfnSCardCancel;
	void* pfnSCardGetAttrib;
	void* pfnSCardSetAttrib;
};
typedef struct _PCSCFunctionTable PCSCFunctionTable;

PCSCFunctionTable g_PCSC_Link = { 0 };

int PCSC_InitializeSCardApi_Link(void)
{
	int status = -1;
	
#ifdef WITH_WINPR_PCSC
	g_PCSC_Link.pfnSCardEstablishContext = (void*) WinPR_PCSC_SCardEstablishContext;
	g_PCSC_Link.pfnSCardReleaseContext = (void*) WinPR_PCSC_SCardReleaseContext;
	g_PCSC_Link.pfnSCardIsValidContext = (void*) WinPR_PCSC_SCardIsValidContext;
	g_PCSC_Link.pfnSCardConnect = (void*) WinPR_PCSC_SCardConnect;
	g_PCSC_Link.pfnSCardReconnect = (void*) WinPR_PCSC_SCardReconnect;
	g_PCSC_Link.pfnSCardDisconnect = (void*) WinPR_PCSC_SCardDisconnect;
	g_PCSC_Link.pfnSCardBeginTransaction = (void*) WinPR_PCSC_SCardBeginTransaction;
	g_PCSC_Link.pfnSCardEndTransaction = (void*) WinPR_PCSC_SCardEndTransaction;
	g_PCSC_Link.pfnSCardStatus = (void*) WinPR_PCSC_SCardStatus;
	g_PCSC_Link.pfnSCardGetStatusChange = (void*) WinPR_PCSC_SCardGetStatusChange;
	g_PCSC_Link.pfnSCardControl = (void*) WinPR_PCSC_SCardControl;
	g_PCSC_Link.pfnSCardTransmit = (void*) WinPR_PCSC_SCardTransmit;
	g_PCSC_Link.pfnSCardListReaderGroups = (void*) WinPR_PCSC_SCardListReaderGroups;
	g_PCSC_Link.pfnSCardListReaders = (void*) WinPR_PCSC_SCardListReaders;
	//g_PCSC_Link.pfnSCardFreeMemory = (void*) WinPR_PCSC_SCardFreeMemory;
	g_PCSC_Link.pfnSCardFreeMemory = (void*) NULL;
	g_PCSC_Link.pfnSCardCancel = (void*) WinPR_PCSC_SCardCancel;
	g_PCSC_Link.pfnSCardGetAttrib = (void*) WinPR_PCSC_SCardGetAttrib;
	g_PCSC_Link.pfnSCardSetAttrib = (void*) WinPR_PCSC_SCardSetAttrib;
	
	status = 1;
#endif
	
	return status;
}

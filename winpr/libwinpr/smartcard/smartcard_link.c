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

#ifndef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISABLE_PCSC_WINPR

/**
 * PCSC-WinPR (optional for Mac OS X):
 *
 * PCSC-WinPR is a static build of libpcsclite with
 * minor modifications meant to avoid an ABI conflict.
 *
 * At this point, it is only a work in progress, as I was not
 * yet able to make it work properly. However, if we could make it
 * work, we might be able to build against a version of pcsc-lite
 * that doesn't have issues present in Apple's SmartCard Services.
 *
 * Patch pcsc-lite/src/PCSC/wintypes.h:
 * Change "unsigned long" to "unsigned int" for
 *
 * typedef unsigned int ULONG;
 * typedef unsigned int DWORD;
 *
 * This is important as ULONG and DWORD are supposed to be 32-bit,
 * but unsigned long on 64-bit OS X is 64-bit, not 32-bit.
 * More importantly, Apple's SmartCard Services uses unsigned int,
 * while vanilla pcsc-lite still uses unsigned long.
 *
 * Patch pcsc-lite/src/PCSC/pcsclite.h.in:
 *
 * Add the WinPR_PCSC_* definitions that follow "#define WinPR_PCSC"
 * in this source file at the beginning of the pcsclite.h.in.
 *
 * Change "unsigned long" to "unsigned int" in the definition
 * of the SCARD_IO_REQUEST structure:
 *
 * unsigned int dwProtocol;
 * unsigned int cbPciLength;
 *
 * Configure pcsc-lite with the following options (Mac OS X):
 * 
 * ./configure --enable-static --prefix=/usr --program-suffix=winpr
 *	--enable-usbdropdir=/usr/libexec/SmartCardServices/drivers
 *	--enable-confdir=/etc --enable-ipcdir=/var/run
 *
 * Build pcsc-lite, and then copy libpcsclite.a to libpcsc-winpr.a:
 * 
 * make
 * cp ./src/.libs/libpcsclite.a libpcsc-winpr.a
 *
 * Validate that libpcsc-winpr.a has a modified ABI:
 *
 * nm libpcsc-winpr.a | grep " T " | grep WinPR
 *
 * If the ABI has been successfully modified, you should
 * see pcsc-lite functions prefixed with "WinPR_PCSC_".
 *
 * You can keep this custom pcsc-lite build for later.
 * To install the library, copy it to /usr/lib or /usr/local/lib.
 *
 * The FreeRDP build system will then automatically pick it up
 * as long as it is present on the system. To ensure PCSC-WinPR
 * is properly detected at cmake generation time, look for the
 * following debug output:
 *
 * -- Found PCSC-WinPR: /usr/lib/libpcsc-winpr.a
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

#define g_rgSCardT0Pci WinPR_PCSC_g_rgSCardT0Pci
#define g_rgSCardT1Pci WinPR_PCSC_g_rgSCardT1Pci
#define g_rgSCardRawPci WinPR_PCSC_g_rgSCardRawPci
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
	
#ifdef DISABLE_PCSC_WINPR
	return -1;
#endif
	
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

#endif

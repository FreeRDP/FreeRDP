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

#include <inttypes.h>

#include <freerdp/utils/list.h>
#include <freerdp/utils/debug.h>
#include <freerdp/channels/rdpdr.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

/* 
 * When using Windows Server 2008 R2 as the Terminal Services (TS)
 * server, and with a smart card reader connected to the TS client machine 
 * and used to authenticate to an existing login session, the TS server 
 * will initiate the protocol initialization of MS-RDPEFS, Section 1.3.1, 
 * twice as it re-establishes a connection.  The TS server starts both 
 * initializations with a "Server Announce Request" message.
 * When the TS client receives this message, as per Section 3.2.5.1.2,
 * 
 * 	The client SHOULD treat this packet as the beginning 
 *	of a new sequence. The client SHOULD also cancel all 
 * 	outstanding requests and release previous references to 
 * 	all devices.
 *
 * As of this writing, the code does not cancel all outstanding requests.
 * This leads to a problem where, after the first MS-RDPEFS initialization,
 * the TS server sends an SCARD_IOCTL_GETSTATUSCHANGEx control in a message
 * that uses an available "CompletionID".  The 
 * TS client doesn't respond immediately because it is blocking while 
 * waiting for a change in the smart card's status in the reader.
 * Then the TS server initiates a second MS-RDPEFS initialization sequence.
 * As noted above, this should cancel the outstanding 
 * SCARD_IOCTL_GETSTATUSCHANGEx request, but it does not.
 * At this point, the TS server is free to reuse the previously used
 * "CompletionID", and it does reuse it for other SCARD_IOCTLs.
 * Therefore, when the user removes (for example) the card from the reader, 
 * the TS client sends an "IOCompetion" message in response to the 
 * GETSTATUSCHANGEx using the original "CompletionID".  The TS server does not 
 * expect this "CompletionID" and so, as per Section 3.1.5.2 of MS-RDPEFS, 
 * it treats that "IOCompletion" message as an error and terminates the
 * virtual channel.
 *
 * The following structure is part of a work-around for this missing
 * capability of canceling outstanding requests.  This work-around 
 * allows the TS client to send an "IOCompletion" back to the
 * TS server for the second (and subsequent) SCARD_IOCTLs that use 
 * the same "CompletionID" as the still outstanding 
 * SCARD_IOCTL_GETSTATUSCHANGEx.  The work-around in the TS client 
 * prevents the client from sending the "IOCompletion" back (when
 * the user removes the card) for the SCARD_IOCTL_GETSTATUSCHANGEx.
 * 
 * This TS client expects the responses from the PCSC daemon for the second  
 * and subsequent SCARD_IOCTLs that use the same "CompletionID"
 * to arrive at the TS client before the daemon's response to the 
 * SCARD_IOCTL_GETSTATUSCHANGEx.  This is a race condition.
 *
 * The "CompletionIDs" are a global pool of IDs across all "DeviceIDs".  
 * However, this problem of duplicate "CompletionIDs" only affects smart cards.
 *
 * This structure tracks outstanding Terminal Services server "CompletionIDs"
 * used by the redirected smart card device.
 */

struct _COMPLETIONIDINFO
{
        UINT32 ID;              /* CompletionID */
        BOOL duplicate;      /* Indicates whether or not this 
				 * CompletionID is a duplicate of an 
                                 * earlier, outstanding, CompletionID.
                                 */
};
typedef struct _COMPLETIONIDINFO COMPLETIONIDINFO;

struct _SMARTCARD_DEVICE
{
	DEVICE device;

	char* name;
	char* path;

	PSLIST_HEADER pIrpList;

	HANDLE thread;
	HANDLE irpEvent;
	HANDLE stopEvent;

        LIST* CompletionIds;
        HANDLE CompletionIdsMutex;
};
typedef struct _SMARTCARD_DEVICE SMARTCARD_DEVICE;

#ifdef WITH_DEBUG_SCARD
#define DEBUG_SCARD(fmt, ...) DEBUG_CLASS(SCARD, fmt, ## __VA_ARGS__)
#else
#define DEBUG_SCARD(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

BOOL smartcard_async_op(IRP*);
void smartcard_device_control(SMARTCARD_DEVICE*, IRP*);

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_MAIN_H */

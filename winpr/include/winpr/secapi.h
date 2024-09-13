/**
 * WinPR: Windows Portable Runtime
 * Schannel Security Package
 *
 * Copyright 2023 David Fort <contact@hardening-consulting.com>
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

#ifndef WINPR_SECAPI_H_
#define WINPR_SECAPI_H_

#ifdef _WIN32
#define _NTDEF_
#include <ntsecapi.h>
#else

#include <winpr/wtypes.h>

typedef enum
{
	KerbInvalidValue = 0, /** @since version 3.9.0 */
	KerbInteractiveLogon = 2,
	KerbSmartCardLogon = 6,
	KerbWorkstationUnlockLogon = 7,
	KerbSmartCardUnlockLogon = 8,
	KerbProxyLogon = 9,
	KerbTicketLogon = 10,
	KerbTicketUnlockLogon = 11,
	KerbS4ULogon = 12,
	KerbCertificateLogon = 13,
	KerbCertificateS4ULogon = 14,
	KerbCertificateUnlockLogon = 15,
	KerbNoElevationLogon = 83,
	KerbLuidLogon = 84
} KERB_LOGON_SUBMIT_TYPE,
    *PKERB_LOGON_SUBMIT_TYPE;

typedef struct
{
	KERB_LOGON_SUBMIT_TYPE MessageType;
	ULONG Flags;
	ULONG ServiceTicketLength;
	ULONG TicketGrantingTicketLength;
	PUCHAR ServiceTicket;
	PUCHAR TicketGrantingTicket;
} KERB_TICKET_LOGON, *PKERB_TICKET_LOGON;

#define KERB_LOGON_FLAG_ALLOW_EXPIRED_TICKET 0x1

#define MSV1_0_OWF_PASSWORD_LENGTH 16

typedef struct
{
	ULONG Version;
	ULONG Flags;
	UCHAR LmPassword[MSV1_0_OWF_PASSWORD_LENGTH];
	UCHAR NtPassword[MSV1_0_OWF_PASSWORD_LENGTH];
} MSV1_0_SUPPLEMENTAL_CREDENTIAL, *PMSV1_0_SUPPLEMENTAL_CREDENTIAL;

#define MSV1_0_CRED_VERSION_REMOTE 0xffff0002

#endif /* _WIN32 */

#ifndef KERB_LOGON_FLAG_REDIRECTED
#define KERB_LOGON_FLAG_REDIRECTED 0x2
#endif

#endif /* WINPR_SECAPI_H_ */

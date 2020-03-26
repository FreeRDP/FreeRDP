
/**
 * WinPR: Windows Portable Runtime
 * Windows Native System Services
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_NT_H
#define WINPR_NT_H
#ifndef _WIN32

/* Defined in winnt.h, do not redefine */



#define STATUS_LOGON_FAILURE						((NTSTATUS)0xC000006DL)
#define STATUS_WRONG_PASSWORD						((NTSTATUS)0xC000006AL)
#define STATUS_PASSWORD_EXPIRED						((NTSTATUS)0xC0000071L)
#define STATUS_PASSWORD_MUST_CHANGE					((NTSTATUS)0xC0000224L)
#define STATUS_ACCESS_DENIED						((NTSTATUS)0xC0000022L)
#define STATUS_DOWNGRADE_DETECTED					((NTSTATUS)0xC0000388L)
#define STATUS_AUTHENTICATION_FIREWALL_FAILED				((NTSTATUS)0xC0000413L)
#define STATUS_ACCOUNT_DISABLED						((NTSTATUS)0xC0000072L)
#define STATUS_ACCOUNT_RESTRICTION					((NTSTATUS)0xC000006EL)
#define STATUS_ACCOUNT_LOCKED_OUT					((NTSTATUS)0xC0000234L)
#define STATUS_ACCOUNT_EXPIRED						((NTSTATUS)0xC0000193L)
#define STATUS_LOGON_TYPE_NOT_GRANTED					((NTSTATUS)0xC000015BL)

#else
#include <wincred.h>
#endif

#endif /* WINPR_NT_H */

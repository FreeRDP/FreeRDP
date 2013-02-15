/**
 * WinPR: Windows Portable Runtime
 * Schannel Security Package
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_SSPI_SCHANNEL_PRIVATE_H
#define WINPR_SSPI_SCHANNEL_PRIVATE_H

#include <winpr/sspi.h>
#include <winpr/schannel.h>

#include "../sspi.h"

#include "schannel_openssl.h"

struct _SCHANNEL_CREDENTIALS
{
	SCHANNEL_CRED cred;
	ULONG fCredentialUse;
};
typedef struct _SCHANNEL_CREDENTIALS SCHANNEL_CREDENTIALS;

struct _SCHANNEL_CONTEXT
{
	BOOL server;
	SCHANNEL_CRED cred;
	SCHANNEL_OPENSSL* openssl;
};
typedef struct _SCHANNEL_CONTEXT SCHANNEL_CONTEXT;

SCHANNEL_CONTEXT* schannel_ContextNew(void);
void schannel_ContextFree(SCHANNEL_CONTEXT* context);

#endif /* WINPR_SSPI_SCHANNEL_PRIVATE_H */


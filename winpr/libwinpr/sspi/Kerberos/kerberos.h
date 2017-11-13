/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Kerberos Auth Protocol
 *
 * Copyright 2015 ANSSI, Author Thomas Calderon
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_SSPI_KERBEROS_PRIVATE_H
#define FREERDP_SSPI_KERBEROS_PRIVATE_H

#include <winpr/sspi.h>
#include <winpr/windows.h>

#include "../sspi.h"
#include "../../log.h"

#ifdef WITH_GSSAPI
#include <krb5.h>
#include <gssapi.h>
#endif

struct _KRB_CONTEXT
{
	CtxtHandle context;
	SSPI_CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY identity;

	/* GSSAPI */
	UINT32 major_status;
	UINT32 minor_status;
	UINT32 actual_time;
	sspi_gss_cred_id_t cred;
	sspi_gss_ctx_id_t gss_ctx;
	sspi_gss_name_t target_name;
};
typedef struct _KRB_CONTEXT KRB_CONTEXT;

const SecPkgInfoA KERBEROS_SecPkgInfoA =
{
	0x000F3BBF,		/* fCapabilities */
	1,			/* wVersion */
	0x0010,			/* wRPCID */
	0x0000BB80,		/* cbMaxToken : 48k bytes maximum for Windows Server 2012 */
	"Kerberos",		/* Name */
	"Kerberos Security Package" /* Comment */
};

WCHAR KERBEROS_SecPkgInfoW_Name[] = { 'K', 'e', 'r', 'b', 'e', 'r', 'o', 's', '\0' };

WCHAR KERBEROS_SecPkgInfoW_Comment[] =
{
	'K', 'e', 'r', 'b', 'e', 'r', 'o', 's', ' ',
	'S', 'e', 'c', 'u', 'r', 'i', 't', 'y', ' ',
	'P', 'a', 'c', 'k', 'a', 'g', 'e', '\0'
};

const SecPkgInfoW KERBEROS_SecPkgInfoW =
{
	0x000F3BBF,		/* fCapabilities */
	1,			/* wVersion */
	0x0010,			/* wRPCID */
	0x0000BB80,		/* cbMaxToken : 48k bytes maximum for Windows Server 2012 */
	KERBEROS_SecPkgInfoW_Name,	/* Name */
	KERBEROS_SecPkgInfoW_Comment	/* Comment */
};


void krb_ContextFree(KRB_CONTEXT* context);

#endif /* FREERDP_SSPI_KERBEROS_PRIVATE_H */

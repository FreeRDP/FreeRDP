/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: Copyright 2025 Siemens
 */

#ifndef FREERDP_CLIENT_COMMON_SSO_MIB_TOKENS_H
#define FREERDP_CLIENT_COMMON_SSO_MIB_TOKENS_H

#include <freerdp/freerdp.h>
#include <sso-mib/sso-mib.h>

enum sso_mib_state
{
	SSO_MIB_STATE_INIT = 0,
	SSO_MIB_STATE_FAILED = 1,
	SSO_MIB_STATE_SUCCESS = 2,
};

struct MIBClientWrapper
{
	MIBClientApp* app;
	enum sso_mib_state state;
};

BOOL sso_mib_get_avd_access_token(freerdp* instance, char** token);

BOOL sso_mib_get_rdsaad_access_token(freerdp* instance, const char* scope, const char* req_cnf,
                                     char** token);

#endif /* FREERDP_CLIENT_COMMON_SSO_MIB_TOKENS_H */

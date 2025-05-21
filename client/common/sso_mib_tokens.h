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
	pGetCommonAccessToken GetCommonAccessToken;
};

BOOL sso_mib_get_access_token(rdpContext* context, AccessTokenType tokenType, char** token,
                              size_t count, ...);

#endif /* FREERDP_CLIENT_COMMON_SSO_MIB_TOKENS_H */

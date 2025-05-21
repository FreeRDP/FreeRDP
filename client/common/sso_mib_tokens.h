/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: Copyright 2025 Siemens
 */

#ifndef FREERDP_CLIENT_COMMON_SSO_MIB_TOKENS_H
#define FREERDP_CLIENT_COMMON_SSO_MIB_TOKENS_H

#include <freerdp/freerdp.h>

void sso_mib_free(MIBClientWrapper* sso);

WINPR_ATTR_MALLOC(sso_mib_free, 1)
MIBClientWrapper* sso_mib_new(rdpContext* context);

#endif /* FREERDP_CLIENT_COMMON_SSO_MIB_TOKENS_H */

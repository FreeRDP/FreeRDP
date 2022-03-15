/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package (Message)
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_SSPI_NTLM_MESSAGE_H
#define WINPR_SSPI_NTLM_MESSAGE_H

#include "ntlm.h"

SECURITY_STATUS ntlm_read_NegotiateMessage(NTLM_CONTEXT* context, PSecBuffer buffer);
SECURITY_STATUS ntlm_write_NegotiateMessage(NTLM_CONTEXT* context, const PSecBuffer buffer);
SECURITY_STATUS ntlm_read_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer);
SECURITY_STATUS ntlm_write_ChallengeMessage(NTLM_CONTEXT* context, const PSecBuffer buffer);
SECURITY_STATUS ntlm_read_AuthenticateMessage(NTLM_CONTEXT* context, PSecBuffer buffer);
SECURITY_STATUS ntlm_write_AuthenticateMessage(NTLM_CONTEXT* context, const PSecBuffer buffer);

SECURITY_STATUS ntlm_server_AuthenticateComplete(NTLM_CONTEXT* context);

const char* ntlm_get_negotiate_string(UINT32 flag);

#endif /* WINPR_SSPI_NTLM_MESSAGE_H */

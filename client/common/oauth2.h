/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * OAuth2 state handling
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
 * Copyright 2026 Thincast Technologies GmbH
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
#pragma once

#include <winpr/wtypes.h>
#include <winpr/memory.h>

typedef struct rdp_client_oauth2 rdpClientOAuth2;

void freerdp_oauth2_free(rdpClientOAuth2* oauth2);

WINPR_ATTR_MALLOC(freerdp_oauth2_free, 1)
rdpClientOAuth2* freerdp_oauth2_new(void);

WINPR_ATTR_NODISCARD
BOOL freerdp_oauth2_reset(rdpClientOAuth2* oauth2);

WINPR_ATTR_MALLOC(winpr_zfree, 1)
char* freerdp_oauth2_append_state(rdpClientOAuth2* oauth2, const char* url, size_t len,
                                  size_t* plen);

WINPR_ATTR_NODISCARD
BOOL freerdp_oauth2_check_return_valid(rdpClientOAuth2* oauth2, const char* response, size_t len);

WINPR_ATTR_MALLOC(winpr_zfree, 1)
char* freerdp_oauth2_extract_code(rdpClientOAuth2* oauth2, const char* response, size_t len);

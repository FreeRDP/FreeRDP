/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Websocket Transport
 *
 * Copyright 2023 Michael Saxl <mike@mwsys.mine.bz>
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

#ifndef FREERDP_LIB_CORE_GATEWAY_WEBSOCKET_TRANSPORT_H
#define FREERDP_LIB_CORE_GATEWAY_WEBSOCKET_TRANSPORT_H

#include <winpr/wtypes.h>
#include <winpr/stream.h>
#include <winpr/winpr.h>

/* needed for BIO */
#include <openssl/ssl.h>

typedef struct rdp_wst rdpWst;

FREERDP_LOCAL void wst_free(rdpWst* wst);

WINPR_ATTR_MALLOC(wst_free, 1)
FREERDP_LOCAL rdpWst* wst_new(rdpContext* context);

FREERDP_LOCAL BIO* wst_get_front_bio_and_take_ownership(rdpWst* wst);

FREERDP_LOCAL BOOL wst_connect(rdpWst* wst, DWORD timeout);
FREERDP_LOCAL DWORD wst_get_event_handles(rdpWst* wst, HANDLE* events, DWORD count);

#endif /* FREERDP_LIB_CORE_GATEWAY_WEBSOCKET_TRANSPORT_H */

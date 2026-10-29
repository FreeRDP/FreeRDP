/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#ifndef FREERDP_SERVER_PROXY_PFCLIENT_H
#define FREERDP_SERVER_PROXY_PFCLIENT_H

#include <freerdp/freerdp.h>
#include <freerdp/server/proxy/proxy_context.h>
#include <winpr/wtypes.h>

#include <freerdp/private/proxy_client.h>

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);
WINPR_ATTR_NODISCARD DWORD WINAPI pf_client_start(LPVOID arg);

/** @brief Queue an input event or update request received by the proxy server thread.
 *
 *  The connection to the server is owned by the proxy client thread, which resets it on
 *  redirection, reconnect and disconnect. No other thread must send on it, so the proxy server
 *  thread queues here and the proxy client thread sends.
 *
 *  @param pc The proxy client context
 *  @param id The message id, of \b Input_Class or \b Update_Class
 *  @param wParam message specific argument, flags or count
 *  @param lParam message specific argument. For \b Update_Class an allocated buffer or
 *  \b nullptr, ownership is taken even on failure.
 *
 *  @return \b TRUE for success, \b FALSE otherwise
 */
WINPR_ATTR_NODISCARD BOOL pf_client_post_message(pClientContext* pc, UINT32 id, size_t wParam,
                                                 void* lParam);

#endif /* FREERDP_SERVER_PROXY_PFCLIENT_H */

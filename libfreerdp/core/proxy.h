/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * HTTP proxy support
 *
 * Copyright 2014 Christian Plattner <ccpp@gmx.at>
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

#ifndef FREERDP_LIB_CORE_HTTP_PROXY_H
#define FREERDP_LIB_CORE_HTTP_PROXY_H

#include "freerdp/settings.h"
#include <openssl/bio.h>
#include "transport.h"

BOOL proxy_prepare(const rdpSettings* settings, DWORD* pProxyType, char** lpPeerHostname,
                   UINT16* lpPeerPort, char** lpProxyUsername, char** lpProxyPassword);
BOOL proxy_connect(DWORD ProxyType, BIO* bio, const char* proxyUsername, const char* proxyPassword,
                   const char* hostname, UINT16 port);
BIO* proxy_multi_connect(rdpContext* context, DWORD ProxyType, const char* ProxyHost,
                         UINT16 ProxyPort, const char* ProxyUsername, const char* ProxyPassword,
                         const char** hostnames, const UINT16* ports, size_t count, int timeout);
BOOL proxy_resolve(DWORD ProxyType, const char* ProxyHostname, UINT16 ProxyPort,
                   const char* ProxyUsername, const char* ProxyPassword, const char* hostname);

#endif /* FREERDP_LIB_CORE_HTTP_PROXY_H */

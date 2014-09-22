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

#ifndef __HTTP_PROXY_H
#define __HTTP_PROXY_H

void http_proxy_read_environment(rdpSettings *settings, char *envname);
BOOL http_proxy_connect(rdpTcp* tcp, const char* hostname, UINT16 port);

#endif

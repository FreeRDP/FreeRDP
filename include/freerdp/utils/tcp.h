/**
 * FreeRDP: A Remote Desktop Protocol Client
 * TCP Utils
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_TCP_UTILS_H
#define FREERDP_TCP_UTILS_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/windows.h>

FREERDP_API int freerdp_tcp_connect(const char* hostname, int port);
FREERDP_API int freerdp_tcp_read(int sockfd, uint8* data, int length);
FREERDP_API int freerdp_tcp_write(int sockfd, uint8* data, int length);
FREERDP_API int freerdp_tcp_disconnect(int sockfd);

#endif /* FREERDP_TCP_UTILS_H */

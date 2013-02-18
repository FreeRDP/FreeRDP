/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int freerdp_tcp_connect(const char* hostname, int port);
FREERDP_API int freerdp_tcp_read(int sockfd, BYTE* data, int length);
FREERDP_API int freerdp_tcp_write(int sockfd, BYTE* data, int length);
FREERDP_API int freerdp_tcp_wait_read(int sockfd);
FREERDP_API int freerdp_tcp_wait_write(int sockfd);
FREERDP_API int freerdp_tcp_disconnect(int sockfd);

FREERDP_API int freerdp_tcp_set_no_delay(int sockfd, BOOL no_delay);

FREERDP_API int freerdp_wsa_startup(void);
FREERDP_API int freerdp_wsa_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_TCP_UTILS_H */

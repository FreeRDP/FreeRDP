/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __XF_INTERFACE_H
#define __XF_INTERFACE_H

#include <freerdp/freerdp.h>

void xf_context_new(freerdp* instance, rdpContext* context);
void xf_context_free(freerdp* instance, rdpContext* context);

BOOL xf_pre_connect(freerdp* instance);
BOOL xf_post_connect(freerdp* instance);

BOOL xf_authenticate(freerdp* instance, char** username, char** password, char** domain);
BOOL xf_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint);
int xf_logon_error_info(freerdp* instance, UINT32 data, UINT32 type);

int xf_receive_channel_data(freerdp* instance, int channelId, BYTE* data, int size, int flags, int total_size);

DWORD xf_exit_code_from_disconnect_reason(DWORD reason);

void* xf_thread(void* param);

#endif /* __XF_INTERFACE_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_SMARTCARD_CALL_H
#define FREERDP_CHANNEL_SMARTCARD_CALL_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/api.h>
#include <freerdp/settings.h>
#include <freerdp/channels/scard.h>
#include <freerdp/utils/smartcard_operations.h>

typedef struct s_scard_call_context scard_call_context;

FREERDP_API scard_call_context* smartcard_call_context_new(const rdpSettings* settings);
FREERDP_API void smartcard_call_context_free(scard_call_context* ctx);
FREERDP_API BOOL smartcard_call_context_signal_stop(scard_call_context* ctx, BOOL reset);
FREERDP_API BOOL smartcard_call_context_add(scard_call_context* ctx, const char* name);
FREERDP_API BOOL smartcard_call_cancel_context(scard_call_context* ctx, SCARDCONTEXT context);
FREERDP_API BOOL smartcard_call_cancel_all_context(scard_call_context* ctx);
FREERDP_API BOOL smartcard_call_release_context(scard_call_context* ctx, SCARDCONTEXT context);
FREERDP_API BOOL smartcard_call_is_configured(scard_call_context* ctx);

FREERDP_API BOOL smarcard_call_set_callbacks(scard_call_context* ctx, void* userdata,
                                             void* (*fn_new)(void*, SCARDCONTEXT),
                                             void (*fn_free)(void*));
FREERDP_API void* smartcard_call_get_context(scard_call_context* ctx, SCARDCONTEXT hContext);

FREERDP_API LONG smartcard_irp_device_control_call(scard_call_context* context, wStream* out,
                                                   UINT32* pIoStatus,
                                                   SMARTCARD_OPERATION* operation);

#endif /* FREERDP_CHANNEL_SMARTCARD_CALL_H */

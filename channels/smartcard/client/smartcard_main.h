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

#ifndef FREERDP_CHANNEL_SMARTCARD_CLIENT_MAIN_H
#define FREERDP_CHANNEL_SMARTCARD_CLIENT_MAIN_H

#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpdr.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/synch.h>
#include <winpr/smartcard.h>
#include <winpr/collections.h>

#include <freerdp/utils/smartcard_operations.h>
#include <freerdp/utils/smartcard_call.h>

#if defined(WITH_SMARTCARD_EMULATE)
#include <freerdp/emulate/scard/smartcard_emulate.h>
#endif

#define TAG CHANNELS_TAG("smartcard.client")

typedef struct
{
	DEVICE device;

	HANDLE thread;
	scard_call_context* callctx;
	wMessageQueue* IrpQueue;
	wListDictionary* rgOutstandingMessages;
	rdpContext* rdpcontext;
} SMARTCARD_DEVICE;

typedef struct
{
	HANDLE thread;
	SCARDCONTEXT hContext;
	wMessageQueue* IrpQueue;
	SMARTCARD_DEVICE* smartcard;
} SMARTCARD_CONTEXT;

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_MAIN_H */

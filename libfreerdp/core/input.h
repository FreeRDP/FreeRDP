/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Input PDUs
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_INPUT_H
#define FREERDP_LIB_CORE_INPUT_H

#include "rdp.h"
#include "fastpath.h"
#include "message.h"

#include <freerdp/input.h>
#include <freerdp/freerdp.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

typedef struct
{
	rdpInput common;
	/* Internal */

	rdpInputProxy* proxy;
	wMessageQueue* queue;
} rdp_input_internal;

static INLINE rdp_input_internal* input_cast(rdpInput* input)
{
	union
	{
		rdpInput* pub;
		rdp_input_internal* internal;
	} cnv;

	WINPR_ASSERT(input);
	cnv.pub = input;
	return cnv.internal;
}
FREERDP_LOCAL BOOL input_recv(rdpInput* input, wStream* s);

FREERDP_LOCAL int input_process_events(rdpInput* input);
FREERDP_LOCAL BOOL input_register_client_callbacks(rdpInput* input);

FREERDP_LOCAL rdpInput* input_new(rdpRdp* rdp);
FREERDP_LOCAL void input_free(rdpInput* input);

#endif /* FREERDP_LIB_CORE_INPUT_H */

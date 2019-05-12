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

#include "pf_input.h"
#include "pf_context.h"

static BOOL pf_server_synchronize_event(rdpInput* input, UINT32 flags)
{
	pServerContext* ps = (pServerContext*)input->context;
	pClientContext* pc = ps->pdata->pc;
	rdpContext* context = (rdpContext*) pc;
	return freerdp_input_send_synchronize_event(context->input, flags);
}

static BOOL pf_server_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	pServerContext* ps = (pServerContext*)input->context;
	pClientContext* pc = ps->pdata->pc;
	rdpContext* context = (rdpContext*) pc;
	proxyConfig* config = ps->pdata->config;

	if (!config->Keyboard)
		return TRUE;

	return freerdp_input_send_keyboard_event(context->input, flags, code);
}

static BOOL pf_server_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	pServerContext* ps = (pServerContext*)input->context;
	pClientContext* pc = ps->pdata->pc;
	rdpContext* context = (rdpContext*) pc;
	proxyConfig* config = ps->pdata->config;

	if (!config->Keyboard)
		return TRUE;

	return freerdp_input_send_unicode_keyboard_event(context->input, flags, code);
}

static BOOL pf_server_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	pServerContext* ps = (pServerContext*)input->context;
	pClientContext* pc = ps->pdata->pc;
	rdpContext* context = (rdpContext*) pc;
	proxyConfig* config = ps->pdata->config;

	if (!config->Mouse)
		return TRUE;

	return freerdp_input_send_mouse_event(context->input, flags, x, y);
}

static BOOL pf_server_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x,
        UINT16 y)
{
	pServerContext* ps = (pServerContext*)input->context;
	pClientContext* pc = ps->pdata->pc;
	rdpContext* context = (rdpContext*) pc;
	proxyConfig* config = ps->pdata->config;

	if (!config->Mouse)
		return TRUE;

	return freerdp_input_send_extended_mouse_event(context->input, flags, x, y);
}

void pf_server_register_input_callbacks(rdpInput* input)
{
	input->SynchronizeEvent = pf_server_synchronize_event;
	input->KeyboardEvent = pf_server_keyboard_event;
	input->UnicodeKeyboardEvent = pf_server_unicode_keyboard_event;
	input->MouseEvent = pf_server_mouse_event;
	input->ExtendedMouseEvent = pf_server_extended_mouse_event;
}

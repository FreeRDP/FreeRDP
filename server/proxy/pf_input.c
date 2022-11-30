/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <winpr/assert.h>

#include "pf_input.h"
#include <freerdp/server/proxy/proxy_config.h>
#include <freerdp/server/proxy/proxy_context.h>

#include "proxy_modules.h"

static BOOL pf_server_check_and_sync_input_state(pClientContext* pc)
{
	WINPR_ASSERT(pc);

	if (!freerdp_is_active_state(&pc->context))
		return FALSE;
	if (pc->input_state_sync_pending)
	{
		BOOL rc = freerdp_input_send_synchronize_event(pc->context.input, pc->input_state);
		if (rc)
			pc->input_state_sync_pending = FALSE;
	}
	return TRUE;
}

static BOOL pf_server_synchronize_event(rdpInput* input, UINT32 flags)
{
	pServerContext* ps;
	pClientContext* pc;

	WINPR_ASSERT(input);
	ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pc = ps->pdata->pc;
	WINPR_ASSERT(pc);

	pc->input_state = flags;
	pc->input_state_sync_pending = TRUE;

	pf_server_check_and_sync_input_state(pc);
	return TRUE;
}

static BOOL pf_server_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	const proxyConfig* config;
	proxyKeyboardEventInfo event = { 0 };
	pServerContext* ps;
	pClientContext* pc;

	WINPR_ASSERT(input);
	ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pc = ps->pdata->pc;
	WINPR_ASSERT(pc);

	config = ps->pdata->config;
	WINPR_ASSERT(config);

	if (!pf_server_check_and_sync_input_state(pc))
		return TRUE;

	if (!config->Keyboard)
		return TRUE;

	event.flags = flags;
	event.rdp_scan_code = code;

	if (pf_modules_run_filter(pc->pdata->module, FILTER_TYPE_KEYBOARD, pc->pdata, &event))
		return freerdp_input_send_keyboard_event(pc->context.input, flags, code);

	return TRUE;
}

static BOOL pf_server_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	const proxyConfig* config;
	proxyUnicodeEventInfo event = { 0 };
	pServerContext* ps;
	pClientContext* pc;

	WINPR_ASSERT(input);
	ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pc = ps->pdata->pc;
	WINPR_ASSERT(pc);

	config = ps->pdata->config;
	WINPR_ASSERT(config);

	if (!pf_server_check_and_sync_input_state(pc))
		return TRUE;

	if (!config->Keyboard)
		return TRUE;

	event.flags = flags;
	event.code = code;
	if (pf_modules_run_filter(pc->pdata->module, FILTER_TYPE_UNICODE, pc->pdata, &event))
		return freerdp_input_send_unicode_keyboard_event(pc->context.input, flags, code);
	return TRUE;
}

static BOOL pf_server_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	proxyMouseEventInfo event = { 0 };
	const proxyConfig* config;
	pServerContext* ps;
	pClientContext* pc;

	WINPR_ASSERT(input);
	ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pc = ps->pdata->pc;
	WINPR_ASSERT(pc);

	config = ps->pdata->config;
	WINPR_ASSERT(config);

	if (!pf_server_check_and_sync_input_state(pc))
		return TRUE;

	if (!config->Mouse)
		return TRUE;

	event.flags = flags;
	event.x = x;
	event.y = y;

	if (pf_modules_run_filter(pc->pdata->module, FILTER_TYPE_MOUSE, pc->pdata, &event))
		return freerdp_input_send_mouse_event(pc->context.input, flags, x, y);

	return TRUE;
}

static BOOL pf_server_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	const proxyConfig* config;
	proxyMouseExEventInfo event = { 0 };
	pServerContext* ps;
	pClientContext* pc;

	WINPR_ASSERT(input);
	ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pc = ps->pdata->pc;
	WINPR_ASSERT(pc);

	config = ps->pdata->config;
	WINPR_ASSERT(config);

	if (!pf_server_check_and_sync_input_state(pc))
		return TRUE;

	if (!config->Mouse)
		return TRUE;

	event.flags = flags;
	event.x = x;
	event.y = y;
	if (pf_modules_run_filter(pc->pdata->module, FILTER_TYPE_MOUSE, pc->pdata, &event))
		return freerdp_input_send_extended_mouse_event(pc->context.input, flags, x, y);
	return TRUE;
}

void pf_server_register_input_callbacks(rdpInput* input)
{
	WINPR_ASSERT(input);

	input->SynchronizeEvent = pf_server_synchronize_event;
	input->KeyboardEvent = pf_server_keyboard_event;
	input->UnicodeKeyboardEvent = pf_server_unicode_keyboard_event;
	input->MouseEvent = pf_server_mouse_event;
	input->ExtendedMouseEvent = pf_server_extended_mouse_event;
}

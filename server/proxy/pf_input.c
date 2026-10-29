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

#include <freerdp/message.h>

#include "pf_input.h"
#include "pf_client.h"
#include "pf_server.h"
#include <freerdp/server/proxy/proxy_config.h>

#include "proxy_modules.h"

WINPR_ATTR_NODISCARD
static BOOL pf_server_check_and_sync_input_state(pClientContext* pc)
{
	WINPR_ASSERT(pc);

	if (!freerdp_is_active_state(&pc->cctx.context))
		return FALSE;
	if (pc->input_state_sync_pending)
	{
		BOOL rc = freerdp_input_send_synchronize_event(pc->cctx.context.input, pc->input_state);
		if (rc)
			pc->input_state_sync_pending = FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL pf_server_synchronize_event(rdpInput* input, UINT32 flags)
{
	WINPR_ASSERT(input);
	pServerContext* ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pClientContext* pc = proxy_data_get_client_context(ps->pdata);
	if (!pc)
		return TRUE;

	return pf_client_post_message(pc, MakeMessageId(Input, SynchronizeEvent), flags, nullptr);
}

WINPR_ATTR_NODISCARD
static BOOL pf_server_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	proxyKeyboardEventInfo event = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(input);
	pServerContext* ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pClientContext* pc = proxy_data_get_client_context(ps->pdata);
	if (!pc)
		return TRUE;

	const proxyConfig* config = ps->pdata->config;
	WINPR_ASSERT(config);

	if (!config->Keyboard)
		return TRUE;

	event.flags = flags;
	event.rdp_scan_code = code;

	if (pf_modules_run_filter(pc->pdata->module, FILTER_TYPE_KEYBOARD, pc->pdata, &event))
		return pf_client_post_message(pc, MakeMessageId(Input, KeyboardEvent), flags,
		                              (void*)(size_t)code);

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL pf_server_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	proxyUnicodeEventInfo event = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(input);
	pServerContext* ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pClientContext* pc = proxy_data_get_client_context(ps->pdata);
	if (!pc)
		return TRUE;

	const proxyConfig* config = ps->pdata->config;
	WINPR_ASSERT(config);

	if (!config->Keyboard)
		return TRUE;

	event.flags = flags;
	event.code = code;
	if (pf_modules_run_filter(pc->pdata->module, FILTER_TYPE_UNICODE, pc->pdata, &event))
		return pf_client_post_message(pc, MakeMessageId(Input, UnicodeKeyboardEvent), flags,
		                              (void*)(size_t)code);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL pf_server_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	proxyMouseEventInfo event = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(input);
	pServerContext* ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pClientContext* pc = proxy_data_get_client_context(ps->pdata);
	if (!pc)
		return TRUE;

	const proxyConfig* config = ps->pdata->config;
	WINPR_ASSERT(config);

	if (!config->Mouse)
		return TRUE;

	event.flags = flags;
	event.x = x;
	event.y = y;

	if (pf_modules_run_filter(pc->pdata->module, FILTER_TYPE_MOUSE, pc->pdata, &event))
		return pf_client_post_message(pc, MakeMessageId(Input, MouseEvent), flags,
		                              (void*)(size_t)(((UINT32)x << 16) | y));

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL pf_server_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	proxyMouseExEventInfo event = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(input);

	pServerContext* ps = (pServerContext*)input->context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pClientContext* pc = proxy_data_get_client_context(ps->pdata);
	if (!pc)
		return TRUE;

	const proxyConfig* config = ps->pdata->config;
	WINPR_ASSERT(config);

	if (!config->Mouse)
		return TRUE;

	event.flags = flags;
	event.x = x;
	event.y = y;
	if (pf_modules_run_filter(pc->pdata->module, FILTER_TYPE_MOUSE_EX, pc->pdata, &event))
		return pf_client_post_message(pc, MakeMessageId(Input, ExtendedMouseEvent), flags,
		                              (void*)(size_t)(((UINT32)x << 16) | y));
	return TRUE;
}

BOOL pf_client_process_input_message(pClientContext* pc, const wMessage* msg)
{
	WINPR_ASSERT(pc);
	WINPR_ASSERT(msg);

	const UINT32 type = GetMessageType(msg->id);
	if (type == Input_SynchronizeEvent)
	{
		pc->input_state = (UINT32)(size_t)msg->wParam;
		pc->input_state_sync_pending = TRUE;
	}

	if (!pf_server_check_and_sync_input_state(pc))
		return TRUE;

	rdpInput* input = pc->cctx.context.input;
	const UINT16 flags = (UINT16)(size_t)msg->wParam;
	const UINT32 arg = (UINT32)(size_t)msg->lParam;
	const UINT16 x = (UINT16)(arg >> 16);
	const UINT16 y = (UINT16)(arg & 0xFFFF);

	switch (type)
	{
		case Input_SynchronizeEvent:
			return TRUE;
		case Input_KeyboardEvent:
			return freerdp_input_send_keyboard_event(input, flags, (UINT8)arg);
		case Input_UnicodeKeyboardEvent:
			return freerdp_input_send_unicode_keyboard_event(input, flags, (UINT16)arg);
		case Input_MouseEvent:
			return freerdp_input_send_mouse_event(input, flags, x, y);
		case Input_ExtendedMouseEvent:
			return freerdp_input_send_extended_mouse_event(input, flags, x, y);
		default:
			return FALSE;
	}
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

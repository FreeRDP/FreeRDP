/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Touch input emulated as mouse buttons when there is no RDPEI channel.
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

#include <stdio.h>
#include <winpr/crt.h>
#include <freerdp/client.h>
#include <freerdp/input.h>

#include "../../../libfreerdp/core/connection.h"

static UINT16 g_events[16];
static size_t g_count = 0;

static BOOL record_mouse(WINPR_ATTR_UNUSED rdpInput* input, UINT16 flags,
                         WINPR_ATTR_UNUSED UINT16 x, WINPR_ATTR_UNUSED UINT16 y)
{
	if (g_count < ARRAYSIZE(g_events))
		g_events[g_count] = flags;
	g_count++;
	return TRUE;
}

/* the button flags of every recorded event, the pointer move bit aside */
static BOOL expect(const char* what, const UINT16* want, size_t count)
{
	const UINT16 mask = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1 | PTR_FLAGS_BUTTON2 | PTR_FLAGS_BUTTON3;
	BOOL ok = (g_count == count);
	for (size_t x = 0; ok && (x < count); x++)
		ok = ((g_events[x] & mask) == want[x]);
	if (!ok)
	{
		(void)fprintf(stderr, "%s: got %" PRIuz " events:", what, g_count);
		for (size_t x = 0; (x < g_count) && (x < ARRAYSIZE(g_events)); x++)
			(void)fprintf(stderr, " 0x%04" PRIx16, (UINT16)(g_events[x] & mask));
		(void)fprintf(stderr, ", wanted %" PRIuz ":", count);
		for (size_t x = 0; x < count; x++)
			(void)fprintf(stderr, " 0x%04" PRIx16, want[x]);
		(void)fprintf(stderr, "\n");
	}
	g_count = 0;
	return ok;
}

int TestClientTouch(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(entry);
	entry.ContextSize = sizeof(rdpClientContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		goto fail;
	rdpClientContext* cctx = (rdpClientContext*)context;

	/* a connected session without the RDPEI channel: touch is sent as mouse buttons */
	if (!rdp_client_transition_to_state(context->rdp, CONNECTION_STATE_ACTIVE))
		goto fail;
	context->input->MouseEvent = record_mouse;

	{
		/* one finger: a tap is a left click */
		const UINT16 tap[] = { PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1, PTR_FLAGS_BUTTON1 };
		if (!freerdp_client_handle_touch(cctx, FREERDP_TOUCH_DOWN, 1, 0, 100, 200) ||
		    !freerdp_client_handle_touch(cctx, FREERDP_TOUCH_UP, 1, 0, 100, 200) ||
		    !expect("one finger", tap, ARRAYSIZE(tap)))
			goto fail;
	}

	{
		/* a second finger down while the first is held is the second button */
		const UINT16 two[] = { PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1,
			                   PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2, PTR_FLAGS_BUTTON2,
			                   PTR_FLAGS_BUTTON1 };
		if (!freerdp_client_handle_touch(cctx, FREERDP_TOUCH_DOWN, 1, 0, 100, 200) ||
		    !freerdp_client_handle_touch(cctx, FREERDP_TOUCH_DOWN, 2, 0, 150, 200) ||
		    !freerdp_client_handle_touch(cctx, FREERDP_TOUCH_UP, 2, 0, 150, 200) ||
		    !freerdp_client_handle_touch(cctx, FREERDP_TOUCH_UP, 1, 0, 100, 200) ||
		    !expect("two fingers", two, ARRAYSIZE(two)))
			goto fail;
	}

	rc = 0;
fail:
	freerdp_client_context_free(context);
	return rc;
}

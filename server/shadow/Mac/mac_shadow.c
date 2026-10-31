/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2026 Cong Zhang
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

#include <math.h>
#include <pwd.h>
#include <unistd.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/input.h>
#include <winpr/sysinfo.h>

#include <freerdp/server/server-common.h>
#include <freerdp/codec/audio.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/codec/region.h>
#include <freerdp/log.h>

#include <ApplicationServices/ApplicationServices.h>

#ifdef WITH_PAM
#include <security/pam_appl.h>
#endif

#include "mac_shadow.h"

#define TAG SERVER_TAG("shadow.mac")

/* Frames per second asked from ScreenCaptureKit at most */
#define MAC_SHADOW_FPS 30
/* how often to look for the display again after the stream ended */
#define MAC_SHADOW_RESTART_MS 1000
/* how often to look at the cursor, and how long after a client moved the
 * pointer not to tell it where the pointer is: its own cursor is ahead of
 * what the server sees by a round trip, and would jump back */
#define MAC_SHADOW_CURSOR_MS 30
#define MAC_SHADOW_CURSOR_QUIET_MS 500
/* the largest cursor sent, in points */
#define MAC_SHADOW_CURSOR_MAX 96
/* how often the sound statistics are logged (debug level) */
#define MAC_SHADOW_AUDIO_LOG_MS 5000

/* Two clicks closer than this, in time and in points, make a double click */
#define MAC_SHADOW_CLICK_MS 500
#define MAC_SHADOW_CLICK_DISTANCE 4.0

/* What the clients hear: 48 kHz, 2 channels, 16 bits, the format
 * ScreenCaptureKit is asked for. The build may have no resampler, so every
 * format offered has that rate and channel count, and 16 bits a sample:
 * rdpsnd sizes its buffers from the bits and channels of the first one. Opus
 * first, so that a client that has it takes it (96 kbit/s instead of
 * 1.5 Mbit/s for PCM). Its block alignment is 1: rdpsnd pads every wave to
 * a multiple of it with zeros, and an Opus packet with zeros after it no
 * longer decodes. */
#define MAC_AUDIO_RATE 48000
#define MAC_AUDIO_CHANNELS 2
#define MAC_AUDIO_FRAME_BYTES (MAC_AUDIO_CHANNELS * 2)
/* samples are sent in messages of 20 ms */
#define MAC_AUDIO_MESSAGE_FRAMES (MAC_AUDIO_RATE / 50)

static const AUDIO_FORMAT mac_audio_offered_formats[] = {
	{ WAVE_FORMAT_OPUS, MAC_AUDIO_CHANNELS, MAC_AUDIO_RATE, 12000, 1, 16, 0, nullptr },
	{ WAVE_FORMAT_PCM, MAC_AUDIO_CHANNELS, MAC_AUDIO_RATE, MAC_AUDIO_RATE* MAC_AUDIO_FRAME_BYTES,
	  MAC_AUDIO_FRAME_BYTES, 16, 0, nullptr },
};

/* The format of the samples in each message: it outlives every client, since
 * rdpsnd keeps the pointer as its source format */
static AUDIO_FORMAT mac_audio_source_format = { WAVE_FORMAT_PCM,
	                                            MAC_AUDIO_CHANNELS,
	                                            MAC_AUDIO_RATE,
	                                            MAC_AUDIO_RATE* MAC_AUDIO_FRAME_BYTES,
	                                            MAC_AUDIO_FRAME_BYTES,
	                                            16,
	                                            0,
	                                            nullptr };

/* Apple virtual key codes of the modifier keys (HIToolbox/Events.h) */
enum
{
	MAC_VK_COMMAND = 0x37,
	MAC_VK_SHIFT = 0x38,
	MAC_VK_CAPSLOCK = 0x39,
	MAC_VK_OPTION = 0x3A,
	MAC_VK_CONTROL = 0x3B,
	MAC_VK_RIGHT_COMMAND = 0x36,
	MAC_VK_RIGHT_SHIFT = 0x3C,
	MAC_VK_RIGHT_OPTION = 0x3D,
	MAC_VK_RIGHT_CONTROL = 0x3E,
	MAC_VK_FUNCTION = 0x3F
};

static CGEventFlags mac_shadow_modifier_flag(CGKeyCode keycode)
{
	switch (keycode)
	{
		case MAC_VK_SHIFT:
		case MAC_VK_RIGHT_SHIFT:
			return kCGEventFlagMaskShift;
		case MAC_VK_CONTROL:
		case MAC_VK_RIGHT_CONTROL:
			return kCGEventFlagMaskControl;
		case MAC_VK_OPTION:
		case MAC_VK_RIGHT_OPTION:
			return kCGEventFlagMaskAlternate;
		case MAC_VK_COMMAND:
		case MAC_VK_RIGHT_COMMAND:
			return kCGEventFlagMaskCommand;
		case MAC_VK_FUNCTION:
			return kCGEventFlagMaskSecondaryFn;
		default:
			return 0;
	}
}

/* The active displays, with their monitor rectangles moved so that the
 * leftmost and topmost one start at 0 (the shadow surface cannot start
 * below 0); bounds are left in global display coordinates. */
static UINT32 mac_shadow_list_displays(MONITOR_DEF* monitors, CGDirectDisplayID* ids,
                                       CGRect* bounds, UINT32 maxMonitors)
{
	CGDirectDisplayID active[MAC_SHADOW_MAX_DISPLAYS] = WINPR_C_ARRAY_INIT;
	uint32_t count = 0;

	if (CGGetActiveDisplayList(ARRAYSIZE(active), active, &count) != kCGErrorSuccess)
		return 0;

	count = MIN(count, maxMonitors);
	CGFloat minX = 0;
	CGFloat minY = 0;

	for (uint32_t x = 0; x < count; x++)
	{
		const CGRect rect = CGDisplayBounds(active[x]);
		minX = MIN(minX, rect.origin.x);
		minY = MIN(minY, rect.origin.y);
	}

	const CGDirectDisplayID mainDisplay = CGMainDisplayID();
	for (uint32_t x = 0; x < count; x++)
	{
		const CGRect rect = CGDisplayBounds(active[x]);
		MONITOR_DEF* monitor = &monitors[x];
		monitor->left = (INT32)(rect.origin.x - minX);
		monitor->top = (INT32)(rect.origin.y - minY);
		monitor->right = monitor->left + (INT32)rect.size.width - 1;
		monitor->bottom = monitor->top + (INT32)rect.size.height - 1;
		monitor->flags = (active[x] == mainDisplay) ? 1 : 0;

		if (ids)
			ids[x] = active[x];
		if (bounds)
			bounds[x] = rect;
	}

	return count;
}

static UINT32 mac_shadow_enum_monitors(MONITOR_DEF* monitors, UINT32 maxMonitors)
{
	return mac_shadow_list_displays(monitors, nullptr, nullptr, maxMonitors);
}

static BOOL mac_shadow_post(CGEventRef event)
{
	if (!event)
		return FALSE;

	CGEventPost(kCGHIDEventTap, event);
	CFRelease(event);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL mac_shadow_input_synchronize_event(rdpShadowSubsystem* subsystem,
                                               rdpShadowClient* client,
                                               WINPR_ATTR_UNUSED UINT32 flags)
{
	if (!subsystem || !client)
		return FALSE;

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL mac_shadow_input_keyboard_event(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                            UINT16 flags, UINT8 code)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;

	if (!subsystem || !client)
		return FALSE;

	DWORD scancode = code;
	if (flags & KBD_FLAGS_EXTENDED)
		scancode |= KBDEXT;

	const DWORD vkcode =
	    GetVirtualKeyCodeFromVirtualScanCode(scancode, WINPR_KBD_TYPE_IBM_ENHANCED);
	const DWORD keycode = GetKeycodeFromVirtualKeyCode(vkcode, WINPR_KEYCODE_TYPE_APPLE);

	/* Apple's key code 0 is A: any other key that maps to 0 has no Mac key */
	if ((keycode == 0) && (vkcode != VK_KEY_A))
	{
		WLog_DBG(TAG, "no key for scancode 0x%04" PRIx32, scancode);
		return TRUE;
	}

	/* A key is pressed unless the release flag is set; KBD_FLAGS_DOWN only
	 * says it was down already (a repeat). */
	const BOOL down = (flags & KBD_FLAGS_RELEASE) ? FALSE : TRUE;

	if ((keycode == MAC_VK_CAPSLOCK) && !down)
		return TRUE;

	CGEventRef event = CGEventCreateKeyboardEvent(nullptr, (CGKeyCode)keycode, down);
	if (!event)
		return FALSE;

	/* Events posted from here do not move the system's modifier state, so the
	 * modifiers held are tracked and set on every event, and a modifier key
	 * itself becomes a flags-changed event, as from a real keyboard. */
	const CGEventFlags modifier = mac_shadow_modifier_flag((CGKeyCode)keycode);
	if (modifier)
	{
		if (down)
			mac->modifiers |= modifier;
		else
			mac->modifiers &= ~modifier;
		CGEventSetType(event, kCGEventFlagsChanged);
	}
	else if (keycode == MAC_VK_CAPSLOCK)
	{
		mac->modifiers ^= kCGEventFlagMaskAlphaShift;
		CGEventSetType(event, kCGEventFlagsChanged);
	}

	CGEventSetFlags(event, mac->modifiers);
	return mac_shadow_post(event);
}

WINPR_ATTR_NODISCARD
static BOOL mac_shadow_input_unicode_keyboard_event(rdpShadowSubsystem* subsystem,
                                                    rdpShadowClient* client, UINT16 flags,
                                                    UINT16 code)
{
	if (!subsystem || !client)
		return FALSE;

	/* a key event with no key: the text it types is the character */
	const BOOL down = (flags & KBD_FLAGS_RELEASE) ? FALSE : TRUE;
	CGEventRef event = CGEventCreateKeyboardEvent(nullptr, 0, down);
	if (!event)
		return FALSE;

	const UniChar character = code;
	CGEventKeyboardSetUnicodeString(event, 1, &character);
	return mac_shadow_post(event);
}

static CGPoint mac_shadow_point(const macShadowSubsystem* mac, UINT16 x, UINT16 y)
{
	const CGFloat scaleX = mac->bounds.size.width / (CGFloat)mac->width;
	const CGFloat scaleY = mac->bounds.size.height / (CGFloat)mac->height;

	return CGPointMake(mac->bounds.origin.x + (CGFloat)x * scaleX,
	                   mac->bounds.origin.y + (CGFloat)y * scaleY);
}

static BOOL mac_shadow_mouse_event(macShadowSubsystem* mac, CGEventType type, CGPoint point,
                                   CGMouseButton button, int64_t clicks)
{
	CGEventRef event = CGEventCreateMouseEvent(nullptr, type, point, button);
	if (!event)
		return FALSE;

	if (clicks > 0)
		CGEventSetIntegerValueField(event, kCGMouseEventClickState, clicks);
	CGEventSetFlags(event, mac->modifiers);
	return mac_shadow_post(event);
}

static BOOL mac_shadow_mouse_move(macShadowSubsystem* mac, CGPoint point)
{
	EnterCriticalSection(&mac->clientLock);
	mac->pointerInput = GetTickCount64();
	LeaveCriticalSection(&mac->clientLock);

	/* with a button held the move is a drag, as macOS expects it */
	if (mac->buttonsDown & (1u << kCGMouseButtonLeft))
		return mac_shadow_mouse_event(mac, kCGEventLeftMouseDragged, point, kCGMouseButtonLeft, 0);
	if (mac->buttonsDown & (1u << kCGMouseButtonRight))
		return mac_shadow_mouse_event(mac, kCGEventRightMouseDragged, point, kCGMouseButtonRight,
		                              0);
	for (UINT32 button = kCGMouseButtonCenter; button < 32; button++)
	{
		if (mac->buttonsDown & (1u << button))
			return mac_shadow_mouse_event(mac, kCGEventOtherMouseDragged, point,
			                              (CGMouseButton)button, 0);
	}
	return mac_shadow_mouse_event(mac, kCGEventMouseMoved, point, kCGMouseButtonLeft, 0);
}

static BOOL mac_shadow_mouse_button(macShadowSubsystem* mac, CGMouseButton button, BOOL down,
                                    CGPoint point)
{
	CGEventType type = kCGEventNull;

	switch (button)
	{
		case kCGMouseButtonLeft:
			type = down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
			break;
		case kCGMouseButtonRight:
			type = down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
			break;
		default:
			type = down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
			break;
	}

	/* macOS counts clicks from the click state of the event itself: without
	 * it a double click is two single clicks. */
	if (down)
	{
		const UINT64 now = GetTickCount64();
		if ((button == mac->lastClickButton) && (now - mac->lastClickTime <= MAC_SHADOW_CLICK_MS) &&
		    (fabs(point.x - mac->lastClickPoint.x) <= MAC_SHADOW_CLICK_DISTANCE) &&
		    (fabs(point.y - mac->lastClickPoint.y) <= MAC_SHADOW_CLICK_DISTANCE))
			mac->clickCount++;
		else
			mac->clickCount = 1;

		mac->lastClickTime = now;
		mac->lastClickButton = button;
		mac->lastClickPoint = point;
		mac->buttonsDown |= (1u << button);
	}
	else
		mac->buttonsDown &= ~(1u << button);

	const int64_t clicks = (button == mac->lastClickButton) ? mac->clickCount : 1;
	return mac_shadow_mouse_event(mac, type, point, button, clicks);
}

static BOOL mac_shadow_mouse_wheel(macShadowSubsystem* mac, UINT16 flags, BOOL horizontal)
{
	/* 9-bit two's complement, 120 for one notch */
	INT32 delta = flags & WheelRotationMask;
	if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
		delta -= 0x200;

	INT32* remainder = &mac->wheelRemainder[horizontal ? 1 : 0];
	*remainder += delta;
	const INT32 lines = *remainder / 120;
	if (lines == 0)
		return TRUE;
	*remainder -= lines * 120;

	/* RDP: positive is up, and right; Quartz: positive is up, and left */
	const int32_t vertical = horizontal ? 0 : lines;
	const int32_t sideways = horizontal ? -lines : 0;
	CGEventRef event =
	    CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 2, vertical, sideways);
	if (!event)
		return FALSE;

	CGEventSetFlags(event, mac->modifiers);
	return mac_shadow_post(event);
}

WINPR_ATTR_NODISCARD
static BOOL mac_shadow_input_mouse_event(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                         UINT16 flags, UINT16 x, UINT16 y)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;

	if (!subsystem || !client)
		return FALSE;

	if (flags & PTR_FLAGS_WHEEL)
		return mac_shadow_mouse_wheel(mac, flags, FALSE);
	if (flags & PTR_FLAGS_HWHEEL)
		return mac_shadow_mouse_wheel(mac, flags, TRUE);

	const CGPoint point = mac_shadow_point(mac, x, y);

	if (flags & PTR_FLAGS_MOVE)
	{
		if (!mac_shadow_mouse_move(mac, point))
			return FALSE;
	}

	const BOOL down = (flags & PTR_FLAGS_DOWN) ? TRUE : FALSE;
	if (flags & PTR_FLAGS_BUTTON1)
		return mac_shadow_mouse_button(mac, kCGMouseButtonLeft, down, point);
	if (flags & PTR_FLAGS_BUTTON2)
		return mac_shadow_mouse_button(mac, kCGMouseButtonRight, down, point);
	if (flags & PTR_FLAGS_BUTTON3)
		return mac_shadow_mouse_button(mac, kCGMouseButtonCenter, down, point);

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL mac_shadow_input_extended_mouse_event(rdpShadowSubsystem* subsystem,
                                                  rdpShadowClient* client, UINT16 flags, UINT16 x,
                                                  UINT16 y)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;

	if (!subsystem || !client)
		return FALSE;

	const CGPoint point = mac_shadow_point(mac, x, y);

	if (flags & PTR_FLAGS_MOVE)
	{
		if (!mac_shadow_mouse_move(mac, point))
			return FALSE;
	}

	/* X1 and X2 are the fourth and fifth buttons, 3 and 4 counted from 0 */
	const BOOL down = (flags & PTR_XFLAGS_DOWN) ? TRUE : FALSE;
	if (flags & PTR_XFLAGS_BUTTON1)
		return mac_shadow_mouse_button(mac, (CGMouseButton)3, down, point);
	if (flags & PTR_XFLAGS_BUTTON2)
		return mac_shadow_mouse_button(mac, (CGMouseButton)4, down, point);

	return TRUE;
}

#ifdef WITH_PAM
typedef struct
{
	const char* user;
	const char* password;
} MAC_SHADOW_PAM_DATA;

static int mac_shadow_pam_conv(int num_msg, const struct pam_message** msg,
                               struct pam_response** resp, void* appdata_ptr)
{
	const MAC_SHADOW_PAM_DATA* data = (const MAC_SHADOW_PAM_DATA*)appdata_ptr;

	if ((num_msg <= 0) || !msg || !resp || !data)
		return PAM_CONV_ERR;

	struct pam_response* response = calloc((size_t)num_msg, sizeof(struct pam_response));
	if (!response)
		return PAM_BUF_ERR;

	for (int index = 0; index < num_msg; index++)
	{
		const char* answer = nullptr;
		switch (msg[index]->msg_style)
		{
			case PAM_PROMPT_ECHO_ON:
				answer = data->user;
				break;
			case PAM_PROMPT_ECHO_OFF:
				answer = data->password;
				break;
			default:
				continue;
		}

		response[index].resp = _strdup(answer);
		if (!response[index].resp)
		{
			for (int x = 0; x < index; x++)
				free(response[x].resp);
			free(response);
			return PAM_BUF_ERR;
		}
	}

	*resp = response;
	return PAM_SUCCESS;
}

/* The screen shared is the one of the user running the server: only that
 * user may log in, with the password of their account (checked by Open
 * Directory, through PAM's "checkpw" service). */
WINPR_ATTR_NODISCARD
static int mac_shadow_pam_authenticate(WINPR_ATTR_UNUSED rdpShadowSubsystem* subsystem,
                                       WINPR_ATTR_UNUSED rdpShadowClient* client, const char* user,
                                       WINPR_ATTR_UNUSED const char* domain, const char* password)
{
	if (!user || !password)
		return -1;

	const struct passwd* owner = getpwuid(getuid());
	if (!owner || !owner->pw_name || (strcmp(owner->pw_name, user) != 0))
	{
		WLog_ERR(TAG, "login as '%s' refused: this screen belongs to '%s'", user,
		         (owner && owner->pw_name) ? owner->pw_name : "?");
		return -1;
	}

	MAC_SHADOW_PAM_DATA data = { .user = user, .password = password };
	const struct pam_conv conv = { .conv = mac_shadow_pam_conv, .appdata_ptr = &data };
	pam_handle_t* handle = nullptr;

	int status = pam_start("checkpw", user, &conv, &handle);
	if (status != PAM_SUCCESS)
	{
		WLog_ERR(TAG, "pam_start failure: %s", pam_strerror(handle, status));
		return -1;
	}

	/* checkpw takes the password as given (use_first_pass), it does not ask */
	status = pam_set_item(handle, PAM_AUTHTOK, password);
	if (status == PAM_SUCCESS)
		status = pam_authenticate(handle, 0);
	if (status == PAM_SUCCESS)
		status = pam_acct_mgmt(handle, 0);

	if (status != PAM_SUCCESS)
		WLog_ERR(TAG, "login as '%s' failed: %s", user, pam_strerror(handle, status));

	pam_end(handle, status);
	return (status == PAM_SUCCESS) ? 1 : -1;
}
#endif

static void mac_shadow_on_frame(void* context, CVPixelBufferRef frame, const CGRect* dirty,
                                size_t count)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)context;
	WINPR_ASSERT(mac);

	const CGRect frameRect = CGRectMake(0, 0, (CGFloat)CVPixelBufferGetWidth(frame),
	                                    (CGFloat)CVPixelBufferGetHeight(frame));

	EnterCriticalSection(&mac->frameLock);

	/* keep the newest frame only: what changed in the ones skipped is still
	 * in the pending region, and the newest frame has it */
	CVPixelBufferRetain(frame);
	if (mac->frame)
		CVPixelBufferRelease(mac->frame);
	mac->frame = frame;

	for (size_t x = 0; x < count; x++)
	{
		const CGRect rect = CGRectIntegral(CGRectIntersection(dirty[x], frameRect));
		if (CGRectIsEmpty(rect))
			continue;

		const RECTANGLE_16 invalid = { .left = (UINT16)rect.origin.x,
			                           .top = (UINT16)rect.origin.y,
			                           .right = (UINT16)(rect.origin.x + rect.size.width),
			                           .bottom = (UINT16)(rect.origin.y + rect.size.height) };
		(void)region16_union_rect(&mac->pending, &mac->pending, &invalid);
	}

	LeaveCriticalSection(&mac->frameLock);
	(void)SetEvent(mac->frameEvent);
}

static void mac_shadow_on_stopped(void* context)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)context;
	WINPR_ASSERT(mac);

	/* the subsystem thread makes a new stream once the display is back */
	(void)SetEvent(mac->lostEvent);
}

static void mac_shadow_audio_msg_free(WINPR_ATTR_UNUSED UINT32 id, SHADOW_MSG_OUT* msg)
{
	SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* samples = (SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES*)msg;
	if (samples)
		free(samples->buf);
	free(samples);
}

/* Sends what s holds to every client, as one message */
static void mac_shadow_audio_post(macShadowSubsystem* mac, wStream* s)
{
	const size_t size = Stream_GetPosition(s);
	Stream_ResetPosition(s);
	if (size == 0)
		return;

	rdpShadowServer* server = mac->common.server;
	if (!server || !server->clients || (ArrayList_Count(server->clients) == 0))
		return;

	SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* msg = calloc(1, sizeof(SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES));
	if (!msg)
		return;

	msg->buf = malloc(size);
	if (!msg->buf)
	{
		free(msg);
		return;
	}

	memcpy(msg->buf, Stream_Buffer(s), size);
	mac->audioMessages++;
	msg->common.Free = mac_shadow_audio_msg_free;
	msg->audio_format = &mac_audio_source_format;
	msg->nFrames = size / MAC_AUDIO_FRAME_BYTES;
	msg->wTimestamp = (UINT16)(GetTickCount64() & 0xFFFF);
	/* the message is freed with its last reference, also when no client took it */
	(void)shadow_client_boardcast_msg(server, nullptr, SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES_ID,
	                                  &msg->common, nullptr);
}

static INT16 mac_shadow_audio_sample(float value)
{
	if (value >= 1.0f)
		return INT16_MAX;
	if (value <= -1.0f)
		return -INT16_MAX;
	return (INT16)lrintf(value * (float)INT16_MAX);
}

static void mac_shadow_on_audio(void* context, const float* samples, size_t frames, UINT32 channels,
                                UINT32 rate)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)context;
	WINPR_ASSERT(mac);

	if ((channels != MAC_AUDIO_CHANNELS) || (rate != MAC_AUDIO_RATE))
	{
		WLog_DBG(TAG, "sound in %" PRIu32 " Hz, %" PRIu32 " channels, not what was asked: dropped",
		         rate, channels);
		return;
	}

	const UINT64 now = GetTickCount64();
	if (mac->audioLogged == 0)
	{
		WLog_INFO(TAG, "sound: capturing %" PRIu32 " Hz, %" PRIu32 " channels", rate, channels);
		mac->audioLogged = now;
	}
	else if (now - mac->audioLogged >= MAC_SHADOW_AUDIO_LOG_MS)
	{
		WLog_DBG(TAG,
		         "sound: %" PRIu64 " frames captured, %" PRIu64
		         " messages sent, peak %.3f in the last %" PRIu64 " ms",
		         mac->audioFrames, mac->audioMessages, (double)mac->audioPeak,
		         now - mac->audioLogged);
		mac->audioFrames = 0;
		mac->audioMessages = 0;
		mac->audioPeak = 0.0f;
		mac->audioLogged = now;
	}
	mac->audioFrames += frames;

	wStream* s = mac->audioStream;
	for (size_t f = 0; f < frames; f++)
	{
		for (UINT32 c = 0; c < channels; c++)
		{
			const float value = samples[f * channels + c];
			mac->audioPeak = MAX(mac->audioPeak, fabsf(value));
			Stream_Write_INT16(s, mac_shadow_audio_sample(value));
		}

		if (Stream_GetPosition(s) >= MAC_AUDIO_MESSAGE_FRAMES * MAC_AUDIO_FRAME_BYTES)
			mac_shadow_audio_post(mac, s);
	}
}

static BOOL mac_shadow_audio_init(macShadowSubsystem* mac)
{
	AUDIO_FORMAT* formats = audio_formats_new(ARRAYSIZE(mac_audio_offered_formats));
	if (!formats)
		return FALSE;

	size_t count = 0;
	for (size_t x = 0; x < ARRAYSIZE(mac_audio_offered_formats); x++)
	{
		const AUDIO_FORMAT* format = &mac_audio_offered_formats[x];
		if (freerdp_dsp_supports_format(format, TRUE))
			formats[count++] = *format;
	}

	mac->audioStream = Stream_New(nullptr, MAC_AUDIO_MESSAGE_FRAMES * MAC_AUDIO_FRAME_BYTES);
	if (!mac->audioStream || (count == 0))
	{
		Stream_Free(mac->audioStream, TRUE);
		mac->audioStream = nullptr;
		audio_formats_free(formats, ARRAYSIZE(mac_audio_offered_formats));
		return FALSE;
	}

	mac->common.rdpsndFormats = formats;
	mac->common.nRdpsndFormats = count;
	return TRUE;
}

static void mac_shadow_audio_uninit(macShadowSubsystem* mac)
{
	audio_formats_free(mac->common.rdpsndFormats, mac->common.nRdpsndFormats);
	mac->common.rdpsndFormats = nullptr;
	mac->common.nRdpsndFormats = 0;
	Stream_Free(mac->audioStream, TRUE);
	mac->audioStream = nullptr;
}

/* Copies what changed from the newest frame to the surface and lets the
 * clients send it */
static void mac_shadow_send_frame(macShadowSubsystem* mac)
{
	rdpShadowServer* server = mac->common.server;
	rdpShadowSurface* surface = server->surface;
	REGION16 region;
	region16_init(&region);

	EnterCriticalSection(&mac->frameLock);
	CVPixelBufferRef frame = mac->frame;
	if (frame)
		CVPixelBufferRetain(frame);
	(void)region16_copy(&region, &mac->pending);
	region16_clear(&mac->pending);
	LeaveCriticalSection(&mac->frameLock);

	if (!frame)
		goto out;

	if ((CVPixelBufferGetWidth(frame) != surface->width) ||
	    (CVPixelBufferGetHeight(frame) != surface->height))
	{
		/* the display mode changed under us: the surface keeps its size */
		if (!mac->sizeWarned)
			WLog_WARN(TAG,
			          "frames of %zux%zu for a surface of %" PRIu32 "x%" PRIu32
			          ": restart the server after changing the display mode",
			          CVPixelBufferGetWidth(frame), CVPixelBufferGetHeight(frame),
			          (UINT32)surface->width, (UINT32)surface->height);
		mac->sizeWarned = TRUE;
		goto out;
	}

	{
		const RECTANGLE_16 surfaceRect = { 0, 0, (UINT16)surface->width, (UINT16)surface->height };
		(void)region16_intersect_rect(&region, &region, &surfaceRect);
	}

	if (region16_is_empty(&region))
		goto out;

	if (CVPixelBufferLockBaseAddress(frame, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
		goto out;

	{
		const BYTE* data = CVPixelBufferGetBaseAddress(frame);
		const size_t stride = CVPixelBufferGetBytesPerRow(frame);
		UINT32 count = 0;
		const RECTANGLE_16* rects = region16_rects(&region, &count);

		EnterCriticalSection(&surface->lock);
		for (UINT32 x = 0; x < count; x++)
		{
			const RECTANGLE_16* rect = &rects[x];
			(void)freerdp_image_copy_no_overlap(
			    surface->data, surface->format, surface->scanline, rect->left, rect->top,
			    rect->right - rect->left, rect->bottom - rect->top, data, PIXEL_FORMAT_BGRX32,
			    (UINT32)stride, rect->left, rect->top, nullptr, FREERDP_FLIP_NONE);
			(void)region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, rect);
		}
		LeaveCriticalSection(&surface->lock);
	}

	(void)CVPixelBufferUnlockBaseAddress(frame, kCVPixelBufferLock_ReadOnly);

	ArrayList_Lock(server->clients);
	if (ArrayList_Count(server->clients) > 0)
		shadow_subsystem_frame_update(&mac->common);
	ArrayList_Unlock(server->clients);

	EnterCriticalSection(&surface->lock);
	region16_clear(&surface->invalidRegion);
	LeaveCriticalSection(&surface->lock);

out:
	if (frame)
		CVPixelBufferRelease(frame);
	region16_uninit(&region);
}

static void mac_shadow_refresh(macShadowSubsystem* mac)
{
	const RECTANGLE_16 all = { 0, 0, (UINT16)mac->width, (UINT16)mac->height };

	EnterCriticalSection(&mac->frameLock);
	(void)region16_union_rect(&mac->pending, &mac->pending, &all);
	LeaveCriticalSection(&mac->frameLock);

	EnterCriticalSection(&mac->clientLock);
	mac->cursorResend = TRUE;
	LeaveCriticalSection(&mac->clientLock);

	(void)SetEvent(mac->frameEvent);
}

static void mac_shadow_cursor_msg_free(UINT32 id, SHADOW_MSG_OUT* msg)
{
	if (msg && (id == SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID))
	{
		SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* shape = (SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE*)msg;
		free(shape->xorMaskData);
		free(shape->andMaskData);
	}
	free(msg);
}

static UINT32 mac_shadow_cursor_hash(const macCursorImage* image)
{
	/* FNV-1a over the size, the hot spot and the pixels */
	UINT32 hash = 2166136261u;
	const UINT32 header[] = { image->width, image->height, image->xHot, image->yHot };
	const BYTE* parts[] = { (const BYTE*)header, image->pixels };
	const size_t sizes[] = { sizeof(header), 4ull * image->width * image->height };
	for (size_t p = 0; p < ARRAYSIZE(parts); p++)
	{
		for (size_t x = 0; x < sizes[p]; x++)
		{
			hash ^= parts[p][x];
			hash *= 16777619u;
		}
	}
	return hash;
}

static BOOL mac_shadow_cursor_send_shape(macShadowSubsystem* mac, const macCursorImage* image)
{
	SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* msg =
	    calloc(1, sizeof(SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE));
	if (!msg)
		return FALSE;

	msg->xHot = image->xHot;
	msg->yHot = image->yHot;
	msg->width = image->width;
	msg->height = image->height;
	if (shadow_subsystem_pointer_convert_alpha_pointer_data_to_format(
	        image->pixels, PIXEL_FORMAT_BGRA32, TRUE, image->width, image->height, msg) < 0)
	{
		free(msg);
		return FALSE;
	}

	msg->common.Free = mac_shadow_cursor_msg_free;
	return shadow_client_boardcast_msg(mac->common.server, nullptr,
	                                   SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID, &msg->common,
	                                   nullptr) >= 0;
}

static void mac_shadow_cursor_send_position(macShadowSubsystem* mac, UINT64 now)
{
	EnterCriticalSection(&mac->clientLock);
	const UINT64 input = mac->pointerInput;
	LeaveCriticalSection(&mac->clientLock);
	if (now - input < MAC_SHADOW_CURSOR_QUIET_MS)
		return;

	CGEventRef event = CGEventCreate(nullptr);
	if (!event)
		return;
	const CGPoint where = CGEventGetLocation(event);
	CFRelease(event);

	/* from global points to the surface, as mac_shadow_point the other way */
	const CGFloat x =
	    (where.x - mac->bounds.origin.x) * (CGFloat)mac->width / mac->bounds.size.width;
	const CGFloat y =
	    (where.y - mac->bounds.origin.y) * (CGFloat)mac->height / mac->bounds.size.height;
	const UINT32 xPos = (UINT32)MIN(MAX(x, 0.0), (CGFloat)mac->width - 1.0);
	const UINT32 yPos = (UINT32)MIN(MAX(y, 0.0), (CGFloat)mac->height - 1.0);
	if ((xPos == mac->common.pointerX) && (yPos == mac->common.pointerY))
		return;

	mac->common.pointerX = xPos;
	mac->common.pointerY = yPos;

	SHADOW_MSG_OUT_POINTER_POSITION_UPDATE* msg =
	    calloc(1, sizeof(SHADOW_MSG_OUT_POINTER_POSITION_UPDATE));
	if (!msg)
		return;
	msg->xPos = xPos;
	msg->yPos = yPos;
	msg->common.Free = mac_shadow_cursor_msg_free;
	(void)shadow_client_boardcast_msg(mac->common.server, nullptr,
	                                  SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID, &msg->common,
	                                  nullptr);
}

/* The clients draw the cursor themselves (the frames have none): its shape
 * when it changed, and where it is when something else than a client moved it */
static void mac_shadow_cursor_update(macShadowSubsystem* mac, UINT64 now)
{
	if (now < mac->cursorNext)
		return;
	mac->cursorNext = now + MAC_SHADOW_CURSOR_MS;

	macCursorImage image = WINPR_C_ARRAY_INIT;
	if (mac_cursor_get(&image, MAC_SHADOW_CURSOR_MAX))
	{
		EnterCriticalSection(&mac->clientLock);
		const BOOL resend = mac->cursorResend;
		mac->cursorResend = FALSE;
		LeaveCriticalSection(&mac->clientLock);

		const UINT32 hash = mac_shadow_cursor_hash(&image);
		if ((resend || !mac->cursorKnown || (hash != mac->cursorHash)) &&
		    mac_shadow_cursor_send_shape(mac, &image))
		{
			mac->cursorHash = hash;
			mac->cursorKnown = TRUE;
		}
		mac_cursor_image_free(&image);
	}

	mac_shadow_cursor_send_position(mac, now);
}

static BOOL mac_shadow_has_clients(macShadowSubsystem* mac)
{
	EnterCriticalSection(&mac->clientLock);
	const BOOL clients = mac->clients > 0;
	LeaveCriticalSection(&mac->clientLock);
	return clients;
}

static void mac_shadow_subsystem_process_message(macShadowSubsystem* mac, wMessage* message)
{
	switch (message->id)
	{
		case SHADOW_MSG_IN_REFRESH_REQUEST_ID:
			mac_shadow_refresh(mac);
			break;

		default:
			WLog_ERR(TAG, "Unknown message id: %" PRIu32 "", message->id);
			break;
	}

	if (message->Free)
		message->Free(message);
}

/* Replaces the stream that ended with a new one. It fails while there is no
 * display to capture (the display is off, the lid is closed). */
static BOOL mac_shadow_capture_restart(macShadowSubsystem* mac)
{
	/* ask Quartz first: ScreenCaptureKit would log an error on every try */
	if (!CGDisplayIsActive(mac->display) || CGDisplayIsAsleep(mac->display))
		return FALSE;

	mac_capture_free(mac->capture);
	mac->capture = mac_capture_new(&mac->config);
	if (!mac->capture)
		return FALSE;

	if (!mac_capture_start(mac->capture))
	{
		mac_capture_free(mac->capture);
		mac->capture = nullptr;
		return FALSE;
	}

	/* the clients get the whole screen again with the first frame */
	mac_shadow_refresh(mac);
	return TRUE;
}

static DWORD WINAPI mac_shadow_subsystem_thread(LPVOID arg)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)arg;
	wMessagePipe* MsgPipe = mac->common.MsgPipe;
	HANDLE events[] = { mac->stopEvent, MessageQueue_Event(MsgPipe->In), mac->frameEvent,
		                mac->lostEvent };
	BOOL lost = FALSE;
	UINT64 restart = 0;

	while (1)
	{
		/* with clients, look at the cursor; without a stream, try to make one
		 * again every second */
		const BOOL clients = mac_shadow_has_clients(mac);
		DWORD timeout = clients ? MAC_SHADOW_CURSOR_MS : INFINITE;
		if (lost)
			timeout = MIN(timeout, MAC_SHADOW_RESTART_MS);

		const DWORD status = WaitForMultipleObjects(ARRAYSIZE(events), events, FALSE, timeout);
		if ((status == WAIT_FAILED) || (WaitForSingleObject(mac->stopEvent, 0) == WAIT_OBJECT_0))
			break;

		const UINT64 now = GetTickCount64();
		if (WaitForSingleObject(mac->lostEvent, 0) == WAIT_OBJECT_0)
		{
			(void)ResetEvent(mac->lostEvent);
			if (!lost)
				WLog_WARN(TAG, "the screen is no longer captured (the display is off?): the "
				               "clients see the last picture until it is back");
			lost = TRUE;
			restart = now + MAC_SHADOW_RESTART_MS;
		}
		else if (lost && (now >= restart))
		{
			restart = now + MAC_SHADOW_RESTART_MS;
			if (mac_shadow_capture_restart(mac))
			{
				WLog_INFO(TAG, "the display is back: capturing the screen again");
				lost = FALSE;
			}
		}

		if (clients)
			mac_shadow_cursor_update(mac, now);

		wMessage message = WINPR_C_ARRAY_INIT;
		while (MessageQueue_Peek(MsgPipe->In, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				goto out;
			mac_shadow_subsystem_process_message(mac, &message);
		}

		if (WaitForSingleObject(mac->frameEvent, 0) == WAIT_OBJECT_0)
		{
			(void)ResetEvent(mac->frameEvent);
			mac_shadow_send_frame(mac);
		}
	}

out:
	ExitThread(0);
	return 0;
}

static BOOL mac_shadow_check_input_access(void)
{
	/* posting input needs the Accessibility permission; without it the
	 * clients only watch */
	const void* keys[] = { kAXTrustedCheckOptionPrompt };
	const void* values[] = { kCFBooleanTrue };
	CFDictionaryRef options =
	    CFDictionaryCreate(kCFAllocatorDefault, keys, values, ARRAYSIZE(keys),
	                       &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	const Boolean trusted = AXIsProcessTrustedWithOptions(options);
	if (options)
		CFRelease(options);

	if (!trusted)
		WLog_WARN(TAG, "no permission to control the computer: the clients can only watch. "
		               "Allow this program in System Settings > Privacy & Security > "
		               "Accessibility, then start it again");
	return trusted ? TRUE : FALSE;
}

WINPR_ATTR_NODISCARD
static int mac_shadow_subsystem_init(rdpShadowSubsystem* subsystem)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;
	CGDirectDisplayID ids[MAC_SHADOW_MAX_DISPLAYS] = WINPR_C_ARRAY_INIT;
	CGRect bounds[MAC_SHADOW_MAX_DISPLAYS] = WINPR_C_ARRAY_INIT;

	const UINT32 count = mac_shadow_list_displays(subsystem->monitors, ids, bounds, ARRAYSIZE(ids));
	if (count == 0)
	{
		WLog_ERR(TAG, "no display found");
		return -1;
	}

	subsystem->numMonitors = count;
	if (subsystem->selectedMonitor >= count)
		subsystem->selectedMonitor = 0;

	const MONITOR_DEF* monitor = &subsystem->monitors[subsystem->selectedMonitor];
	mac->display = ids[subsystem->selectedMonitor];
	mac->bounds = bounds[subsystem->selectedMonitor];
	/* the surface has the size of the display in points: on a Retina display
	 * ScreenCaptureKit scales the picture down to it */
	mac->width = (UINT32)(monitor->right - monitor->left + 1);
	mac->height = (UINT32)(monitor->bottom - monitor->top + 1);

	MONITOR_DEF* virtualScreen = &subsystem->virtualScreen;
	virtualScreen->left = 0;
	virtualScreen->top = 0;
	virtualScreen->right = 0;
	virtualScreen->bottom = 0;
	for (UINT32 x = 0; x < count; x++)
	{
		virtualScreen->right = MAX(virtualScreen->right, subsystem->monitors[x].right);
		virtualScreen->bottom = MAX(virtualScreen->bottom, subsystem->monitors[x].bottom);
	}
	virtualScreen->flags = 1;

	WLog_INFO(TAG,
	          "sharing display %" PRIu32 " (%" PRIu32 " of %" PRIu32 "): %" PRIu32 "x%" PRIu32
	          " at %.0f,%.0f",
	          mac->display, subsystem->selectedMonitor, count, mac->width, mac->height,
	          mac->bounds.origin.x, mac->bounds.origin.y);

	if (!mac_capture_check_access())
		return -1;

	(void)mac_shadow_check_input_access();

	const BOOL audio = mac_shadow_audio_init(mac);
	if (!audio)
		WLog_WARN(TAG, "sound: no format to offer: no sound");

	const macCaptureConfig config = { .display = mac->display,
		                              .width = mac->width,
		                              .height = mac->height,
		                              .fps = MAC_SHADOW_FPS,
		                              /* the clients draw the cursor */
		                              .showCursor = FALSE,
		                              .audio = audio,
		                              .audioRate = MAC_AUDIO_RATE,
		                              .audioChannels = MAC_AUDIO_CHANNELS,
		                              .onFrame = mac_shadow_on_frame,
		                              .onAudio = audio ? mac_shadow_on_audio : nullptr,
		                              .onStopped = mac_shadow_on_stopped,
		                              .context = mac };
	mac->config = config;

	mac->capture = mac_capture_new(&mac->config);
	if (!mac->capture)
		return -1;

	return 1;
}

static int mac_shadow_subsystem_uninit(rdpShadowSubsystem* subsystem)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;
	if (!mac)
		return -1;

	mac_capture_free(mac->capture);
	mac->capture = nullptr;

	EnterCriticalSection(&mac->frameLock);
	if (mac->frame)
		CVPixelBufferRelease(mac->frame);
	mac->frame = nullptr;
	region16_clear(&mac->pending);
	LeaveCriticalSection(&mac->frameLock);

	mac_shadow_audio_uninit(mac);
	return 1;
}

static int mac_shadow_subsystem_stop(rdpShadowSubsystem* subsystem)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;

	if (!mac)
		return -1;

	/* the thread first: it may be replacing the stream */
	if (mac->thread)
	{
		(void)SetEvent(mac->stopEvent);
		(void)WaitForSingleObject(mac->thread, INFINITE);
		(void)CloseHandle(mac->thread);
		mac->thread = nullptr;
	}

	mac_capture_stop(mac->capture);
	return 1;
}

/* A connected client keeps the display awake: when it turns off there is
 * nothing left to capture. The first client also wakes it, if it slept. */
WINPR_ATTR_NODISCARD
static BOOL mac_shadow_client_connect(rdpShadowSubsystem* subsystem,
                                      WINPR_ATTR_UNUSED rdpShadowClient* client)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;
	WINPR_ASSERT(mac);

	EnterCriticalSection(&mac->clientLock);
	if (mac->clients++ == 0)
	{
		IOPMAssertionID activity = kIOPMNullAssertionID;
		if (IOPMAssertionDeclareUserActivity(CFSTR("FreeRDP shadow server: a client connected"),
		                                     kIOPMUserActiveRemote, &activity) == kIOReturnSuccess)
			(void)IOPMAssertionRelease(activity);

		if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
		                                kIOPMAssertionLevelOn,
		                                CFSTR("FreeRDP shadow server: clients are connected"),
		                                &mac->awake) != kIOReturnSuccess)
		{
			WLog_WARN(TAG, "could not keep the display awake: when it turns off, the clients "
			               "see the last picture");
			mac->awake = kIOPMNullAssertionID;
		}
	}
	LeaveCriticalSection(&mac->clientLock);
	return TRUE;
}

static void mac_shadow_client_disconnect(rdpShadowSubsystem* subsystem,
                                         WINPR_ATTR_UNUSED rdpShadowClient* client)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;
	WINPR_ASSERT(mac);

	EnterCriticalSection(&mac->clientLock);
	if ((mac->clients > 0) && (--mac->clients == 0) && (mac->awake != kIOPMNullAssertionID))
	{
		(void)IOPMAssertionRelease(mac->awake);
		mac->awake = kIOPMNullAssertionID;
	}
	LeaveCriticalSection(&mac->clientLock);
}

WINPR_ATTR_NODISCARD
static int mac_shadow_subsystem_start(rdpShadowSubsystem* subsystem)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;

	if (!mac)
		return -1;

	(void)ResetEvent(mac->stopEvent);
	mac->thread = CreateThread(nullptr, 0, mac_shadow_subsystem_thread, mac, 0, nullptr);
	if (!mac->thread)
	{
		WLog_ERR(TAG, "Failed to create thread");
		return -1;
	}

	if (!mac_capture_start(mac->capture))
	{
		(void)mac_shadow_subsystem_stop(subsystem);
		return -1;
	}

	return 1;
}

static void mac_shadow_subsystem_free(rdpShadowSubsystem* subsystem)
{
	macShadowSubsystem* mac = (macShadowSubsystem*)subsystem;

	if (!mac)
		return;

	(void)mac_shadow_subsystem_uninit(subsystem);
	if (mac->awake != kIOPMNullAssertionID)
		(void)IOPMAssertionRelease(mac->awake);
	region16_uninit(&mac->pending);
	DeleteCriticalSection(&mac->clientLock);
	DeleteCriticalSection(&mac->frameLock);
	if (mac->frameEvent)
		(void)CloseHandle(mac->frameEvent);
	if (mac->stopEvent)
		(void)CloseHandle(mac->stopEvent);
	if (mac->lostEvent)
		(void)CloseHandle(mac->lostEvent);
	free(mac);
}

WINPR_ATTR_NODISCARD
static rdpShadowSubsystem* mac_shadow_subsystem_new(void)
{
	macShadowSubsystem* mac = calloc(1, sizeof(macShadowSubsystem));

	if (!mac)
		return nullptr;

	region16_init(&mac->pending);
	if (!InitializeCriticalSectionAndSpinCount(&mac->frameLock, 4000))
	{
		free(mac);
		return nullptr;
	}

	if (!InitializeCriticalSectionAndSpinCount(&mac->clientLock, 4000))
	{
		DeleteCriticalSection(&mac->frameLock);
		free(mac);
		return nullptr;
	}

	mac->frameEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	mac->stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	mac->lostEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!mac->frameEvent || !mac->stopEvent || !mac->lostEvent)
	{
		mac_shadow_subsystem_free(&mac->common);
		return nullptr;
	}

	mac->common.SynchronizeEvent = mac_shadow_input_synchronize_event;
	mac->common.KeyboardEvent = mac_shadow_input_keyboard_event;
	mac->common.UnicodeKeyboardEvent = mac_shadow_input_unicode_keyboard_event;
	mac->common.MouseEvent = mac_shadow_input_mouse_event;
	mac->common.ExtendedMouseEvent = mac_shadow_input_extended_mouse_event;
	mac->common.ClientConnect = mac_shadow_client_connect;
	mac->common.ClientDisconnect = mac_shadow_client_disconnect;
#ifdef WITH_PAM
	mac->common.Authenticate = mac_shadow_pam_authenticate;
#endif
	return &mac->common;
}

FREERDP_API const char* ShadowSubsystemName(void)
{
	return "Mac";
}

FREERDP_API int ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->New = mac_shadow_subsystem_new;
	pEntryPoints->Free = mac_shadow_subsystem_free;
	pEntryPoints->Init = mac_shadow_subsystem_init;
	pEntryPoints->Uninit = mac_shadow_subsystem_uninit;
	pEntryPoints->Start = mac_shadow_subsystem_start;
	pEntryPoints->Stop = mac_shadow_subsystem_stop;
	pEntryPoints->EnumMonitors = mac_shadow_enum_monitors;
	return 1;
}

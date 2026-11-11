/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
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

/* The shadow server for a GNOME session, through Mutter's own D-Bus APIs on
 * the session bus (no portal, so no permission dialog: only the session's own
 * user reaches its bus):
 *
 *   1. org.gnome.Mutter.DisplayConfig.GetCurrentState   the monitors
 *   2. RemoteDesktop.CreateSession                      a remote desktop session
 *   3. ScreenCast.CreateSession, tied to it             a screen cast session
 *   4. ScreenCast.Session.RecordMonitor(connector)      a stream of one monitor,
 *      the cursor as metadata (the clients draw it)
 *   5. RemoteDesktop.Session.Start                      PipeWireStreamAdded(node)
 *   6. RemoteDesktop.Session.ConnectToEIS               input through libei
 *
 * The frames arrive as a PipeWire stream in shared memory, with the damaged
 * regions and the cursor as buffer metadata. One thread waits on the session
 * bus, the PipeWire loop and libei; the clients' threads post input to libei
 * under a lock.
 */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <linux/input-event-codes.h>

#include <spa/param/audio/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/buffers.h>
#include <spa/buffer/meta.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/collections.h>
#include <winpr/input.h>
#include <winpr/interlocked.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/server/server-common.h>
#include <freerdp/codec/audio.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/codec/region.h>
#include <freerdp/log.h>

#ifdef WITH_PAM
#include <security/pam_appl.h>
#endif

#include "mutter_shadow.h"

#define TAG SERVER_TAG("shadow.mutter")

#define DC_NAME "org.gnome.Mutter.DisplayConfig"
#define DC_PATH "/org/gnome/Mutter/DisplayConfig"
#define RD_NAME "org.gnome.Mutter.RemoteDesktop"
#define RD_PATH "/org/gnome/Mutter/RemoteDesktop"
#define RD_SESSION_IFACE "org.gnome.Mutter.RemoteDesktop.Session"
#define SC_NAME "org.gnome.Mutter.ScreenCast"
#define SC_PATH "/org/gnome/Mutter/ScreenCast"
#define SC_SESSION_IFACE "org.gnome.Mutter.ScreenCast.Session"
#define SC_STREAM_IFACE "org.gnome.Mutter.ScreenCast.Stream"

/* ScreenCast cursor mode: the cursor comes as buffer metadata, not painted in */
#define MUTTER_CURSOR_MODE_EMBEDDED 1u
#define MUTTER_CURSOR_MODE_METADATA 2u
#define MUTTER_VIRTUAL_ENV "FREERDP_SHADOW_MUTTER_VIRTUAL"
/* "0" turns off the check for changes Mutter left out of its damage */
#define MUTTER_DAMAGE_CHECK_ENV "FREERDP_SHADOW_MUTTER_DAMAGE_CHECK"
#define MUTTER_CHECK_TILE 64

/* FREERDP_SHADOW_MUTTER_VIRTUAL=<w>x<h>: TRUE and the size when set and valid */
WINPR_ATTR_NODISCARD
static BOOL mutter_virtual_size(UINT32* width, UINT32* height)
{
	WINPR_ASSERT(width);
	WINPR_ASSERT(height);

	*width = 0;
	*height = 0;

	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	const char* env = getenv(MUTTER_VIRTUAL_ENV);
	if (!env)
		return FALSE;

	errno = 0;

	char* next = nullptr;
	unsigned long w = strtoul(env, &next, 0);
	if ((errno != 0) || (w < 64) || (w > 8192))
		return FALSE;
	if (!next || (*next != 'x'))
		return FALSE;

	unsigned long h = strtoul(next, nullptr, 0);
	if ((errno != 0) || (h < 64) || (h > 8192))
		return FALSE;

	*width = WINPR_ASSERTING_INT_CAST(UINT32, w);
	*height = WINPR_ASSERTING_INT_CAST(UINT32, h);
	return TRUE;
}

/* cursor metadata: position, hot spot and, when the shape changes, a bitmap;
 * Mutter's bitmaps are up to 384x384 */
#define MUTTER_CURSOR_META_SIZE(width, height) \
	(sizeof(struct spa_meta_cursor) + sizeof(struct spa_meta_bitmap) + (width) * (height) * 4)
#define MUTTER_CURSOR_META_MAX 384u

/* the largest pointer sent to the clients */
#define MUTTER_SHADOW_CURSOR_MAX 96u

/* how long after a client's own pointer motion the server leaves the pointer
 * position to that client */
#define MUTTER_SHADOW_CURSOR_QUIET_MS 500u

/* damage rectangles taken from one buuffer */
#define MUTTER_MAX_DAMAGE 16u

/* how long Mutter may take to offer its API, announce the stream, and give
 * the stream a format */
#define MUTTER_NAME_TIMEOUT_MS 10000u
#define MUTTER_STREAM_TIMEOUT_MS 5000u
#define MUTTER_FORMAT_TIMEOUT_MS 5000u
/* after Mutter's node goes away, how long a Closed may take to follow it */
#define MUTTER_STREAM_GONE_GRACE_MS 250u

/* What the clients hear: 48 kHz, 2 channels, 16 bits; PipeWire converts what
 * the sink plays to it. The build may have no resampler, so every format
 * offered has that rate and channel count, and 16 bits a sample: rdpsnd sizes
 * its buffers from the bits and channels of the first one. Opus first, so that
 * a client that has it takes it (96 kbit/s instead of 1.5 Mbit/s for PCM). Its
 * block alignment is 1: rdpsnd pads every wave to a multiple of it with zeros,
 * and an Opus packet with zeros after it no longer decodes. */
#define MUTTER_AUDIO_RATE 48000u
#define MUTTER_AUDIO_CHANNELS 2u
#define MUTTER_AUDIO_FRAME_BYTES (MUTTER_AUDIO_CHANNELS * 2ull)
/* samples are sent in messages of 20 ms */
#define MUTTER_AUDIO_MESSAGE_FRAMES (MUTTER_AUDIO_RATE / 50u)
/* how often the sound statistics are logged (debug level) */
#define MUTTER_AUDIO_LOG_MS 5000u

/* xkbcommon's core modifiers: Lock is Caps Lock, Mod2 is Num Lock in every
 * stock keymap */
#define MUTTER_MOD_LOCK (1u << 1)
#define MUTTER_MOD_NUM (1u << 4)

/* DisplayConfig.GetCurrentState: a physical monitor's connector and the size of
 * its current mode */
typedef struct
{
	char connector[MUTTER_SHADOW_CONNECTOR_SIZE];
	INT32 width;
	INT32 height;
} MUTTER_MODE;

/* DisplayConfig.GetCurrentState: a logical monitor, where it is in the layout */
typedef struct
{
	INT32 x;
	INT32 y;
	double scale;
	uint32_t transform;
	int primary;
	char connector[MUTTER_SHADOW_CONNECTOR_SIZE];
} MUTTER_LOGICAL;

static const AUDIO_FORMAT mutter_audio_offered_formats[] = {
	{ WAVE_FORMAT_OPUS, MUTTER_AUDIO_CHANNELS, MUTTER_AUDIO_RATE, 12000, 1, 16, 0, nullptr },
	{ WAVE_FORMAT_PCM, MUTTER_AUDIO_CHANNELS, MUTTER_AUDIO_RATE,
	  (MUTTER_AUDIO_RATE * MUTTER_AUDIO_FRAME_BYTES), MUTTER_AUDIO_FRAME_BYTES, 16, 0, nullptr },
};

/* ------------------------------------------------------------------------ */
/* D-Bus                                                                     */
/* ------------------------------------------------------------------------ */

WINPR_ATTR_NODISCARD
static BOOL mutter_wait_for_name(sd_bus* bus, const char* name, UINT64 timeoutMs)
{
	WINPR_ASSERT(bus);
	WINPR_ASSERT(name);

	const UINT64 start = GetTickCount64();
	BOOL logged = FALSE;

	for (;;)
	{
		sd_bus_error err = SD_BUS_ERROR_NULL;
		sd_bus_message* reply = nullptr;
		int hasOwner = 0;

		const int called =
		    sd_bus_call_method(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
		                       "org.freedesktop.DBus", "NameHasOwner", &err, &reply, "s", name);
		const BOOL owned =
		    (called >= 0) && (sd_bus_message_read(reply, "b", &hasOwner) >= 0) && hasOwner;
		sd_bus_message_unref(reply);
		sd_bus_error_free(&err);
		if (owned)
			return TRUE;

		if (GetTickCount64() - start > timeoutMs)
			return FALSE;
		if (!logged)
		{
			WLog_INFO(TAG, "waiting for %s on the session bus", name);
			logged = TRUE;
		}
		Sleep(200);
	}
}

/* Calls a method that returns one object path; *out gets a copy */
WINPR_ATTR_NODISCARD
static BOOL mutter_call_for_path(sd_bus* bus, sd_bus_message* call, const char* what, char** out)
{
	WINPR_ASSERT(bus);
	WINPR_ASSERT(call);
	WINPR_ASSERT(what);
	WINPR_ASSERT(out);

	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = nullptr;
	const char* path = nullptr;

	const int called = sd_bus_call(bus, call, 0, &err, &reply);
	if (called < 0)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "%s failed: %s", what,
		         err.message ? err.message : winpr_strerror(-called, buffer, sizeof(buffer)));
		sd_bus_error_free(&err);
		return FALSE;
	}

	if ((sd_bus_message_read(reply, "o", &path) >= 0) && path)
		*out = _strdup(path);
	sd_bus_message_unref(reply);
	return *out != nullptr;
}

/* Reads a dictionary, a{sv}: the value of key (of type 'b', 'u' or 's') goes to value
 * when the key is there, the other entries are skipped */
WINPR_ATTR_NODISCARD
static BOOL mutter_read_dict(sd_bus_message* msg, const char* key, char type, void* value)
{
	WINPR_ASSERT(msg);
	WINPR_ASSERT(key);
	WINPR_ASSERT(value);

	const char contents[2] = { type, '\0' };

	if (sd_bus_message_enter_container(msg, 'a', "{sv}") < 0)
		return FALSE;

	for (;;)
	{
		const int entered = sd_bus_message_enter_container(msg, 'e', "sv");
		if (entered < 0)
			return FALSE;
		if (entered == 0)
			break;

		const char* name = nullptr;
		if (sd_bus_message_read(msg, "s", &name) < 0)
			return FALSE;
		if (name && (strcmp(name, key) == 0))
		{
			if (sd_bus_message_read(msg, "v", contents, value) < 0)
				return FALSE;
		}
		else if (sd_bus_message_skip(msg, "v") < 0)
			return FALSE;

		if (sd_bus_message_exit_container(msg) < 0)
			return FALSE;
	}

	return sd_bus_message_exit_container(msg) >= 0;
}

/* Reads one physical monitor, ((ssss)a(siiddada{sv})a{sv}): its connector and the
 * size of its current mode. *more is FALSE after the last one. */
WINPR_ATTR_NODISCARD
static BOOL mutter_read_monitor(sd_bus_message* reply, MUTTER_MODE* mode, BOOL* more)
{
	WINPR_ASSERT(reply);
	WINPR_ASSERT(mode);
	WINPR_ASSERT(more);

	const int entered = sd_bus_message_enter_container(reply, 'r', "(ssss)a(siiddada{sv})a{sv}");
	if (entered < 0)
		return FALSE;
	*more = (entered > 0);
	if (!*more)
		return TRUE;

	const char* connector = nullptr;
	if (sd_bus_message_read(reply, "(ssss)", &connector, nullptr, nullptr, nullptr) < 0)
		return FALSE;
	(void)_snprintf(mode->connector, sizeof(mode->connector), "%s", connector ? connector : "");

	if (sd_bus_message_enter_container(reply, 'a', "(siiddada{sv})") < 0)
		return FALSE;
	for (;;)
	{
		const int modeEntered = sd_bus_message_enter_container(reply, 'r', "siiddada{sv}");
		if (modeEntered < 0)
			return FALSE;
		if (modeEntered == 0)
			break;

		int32_t width = 0;
		int32_t height = 0;
		int current = 0;
		if ((sd_bus_message_read(reply, "siidd", nullptr, &width, &height, nullptr, nullptr) < 0) ||
		    (sd_bus_message_skip(reply, "ad") < 0) ||
		    !mutter_read_dict(reply, "is-current", 'b', &current) ||
		    (sd_bus_message_exit_container(reply) < 0))
			return FALSE;

		if (current)
		{
			mode->width = width;
			mode->height = height;
		}
	}

	return (sd_bus_message_exit_container(reply) >= 0) && /* the modes */
	       (sd_bus_message_skip(reply, "a{sv}") >= 0) &&
	       (sd_bus_message_exit_container(reply) >= 0);
}

/* Reads one logical monitor, (iiduba(ssss)a{sv}): where it is and its (first)
 * connector. *more is FALSE after the last one. */
WINPR_ATTR_NODISCARD
static BOOL mutter_read_logical(sd_bus_message* reply, MUTTER_LOGICAL* logical, BOOL* more)
{
	WINPR_ASSERT(reply);
	WINPR_ASSERT(logical);
	WINPR_ASSERT(more);

	const int entered = sd_bus_message_enter_container(reply, 'r', "iiduba(ssss)a{sv}");
	if (entered < 0)
		return FALSE;
	*more = (entered > 0);
	if (!*more)
		return TRUE;

	int32_t x = 0;
	int32_t y = 0;
	double scale = 1.0;
	uint32_t transform = 0;
	int primary = 0;
	if ((sd_bus_message_read(reply, "iidub", &x, &y, &scale, &transform, &primary) < 0) ||
	    (sd_bus_message_enter_container(reply, 'a', "(ssss)") < 0))
		return FALSE;

	const char* connector = nullptr;
	for (;;)
	{
		const char* name = nullptr;
		const int read = sd_bus_message_read(reply, "(ssss)", &name, nullptr, nullptr, nullptr);
		if (read < 0)
			return FALSE;
		if (read == 0)
			break;
		if (!connector)
			connector = name;
	}

	logical->x = x;
	logical->y = y;
	logical->scale = (scale > 0.0) ? scale : 1.0;
	logical->transform = transform;
	logical->primary = primary;
	(void)_snprintf(logical->connector, sizeof(logical->connector), "%s",
	                connector ? connector : "");

	return (sd_bus_message_exit_container(reply) >= 0) && /* the connectors */
	       (sd_bus_message_skip(reply, "a{sv}") >= 0) &&
	       (sd_bus_message_exit_container(reply) >= 0);
}

/* Reads GetCurrentState's reply: the physical monitors' modes, the logical
 * monitors, and the layout mode */
WINPR_ATTR_NODISCARD
static BOOL mutter_read_state(sd_bus_message* reply, MUTTER_MODE* modes, size_t* nmodes,
                              MUTTER_LOGICAL* logical, size_t* nlogical, uint32_t* layoutMode)
{
	WINPR_ASSERT(reply);
	WINPR_ASSERT(modes);
	WINPR_ASSERT(nmodes);
	WINPR_ASSERT(logical);
	WINPR_ASSERT(nlogical);
	WINPR_ASSERT(layoutMode);

	if ((sd_bus_message_read(reply, "u", nullptr) < 0) ||
	    (sd_bus_message_enter_container(reply, 'a', "((ssss)a(siiddada{sv})a{sv})") < 0))
		return FALSE;
	for (;;)
	{
		MUTTER_MODE mode = WINPR_C_ARRAY_INIT;
		BOOL more = FALSE;
		if (!mutter_read_monitor(reply, &mode, &more))
			return FALSE;
		if (!more)
			break;
		if (*nmodes < MUTTER_SHADOW_MAX_MONITORS)
			modes[(*nmodes)++] = mode;
	}
	if ((sd_bus_message_exit_container(reply) < 0) ||
	    (sd_bus_message_enter_container(reply, 'a', "(iiduba(ssss)a{sv})") < 0))
		return FALSE;

	for (;;)
	{
		MUTTER_LOGICAL entry = WINPR_C_ARRAY_INIT;
		BOOL more = FALSE;
		if (!mutter_read_logical(reply, &entry, &more))
			return FALSE;
		if (!more)
			break;
		if (*nlogical < MUTTER_SHADOW_MAX_MONITORS)
			logical[(*nlogical)++] = entry;
	}
	if (sd_bus_message_exit_container(reply) < 0)
		return FALSE;

	return mutter_read_dict(reply, "layout-mode", 'u', layoutMode);
}

/* The logical monitors, in Mutter's layout, primary first. connectors gets
 * each one's (first) connector. Returns the count. */
WINPR_ATTR_NODISCARD
static UINT32 mutter_list_monitors(sd_bus* bus, MONITOR_DEF* monitors,
                                   char (*connectors)[MUTTER_SHADOW_CONNECTOR_SIZE], UINT32 max)
{
	WINPR_ASSERT(bus);
	WINPR_ASSERT(monitors);

	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = nullptr;
	MUTTER_MODE modes[MUTTER_SHADOW_MAX_MONITORS] = WINPR_C_ARRAY_INIT;
	MUTTER_LOGICAL logical[MUTTER_SHADOW_MAX_MONITORS] = WINPR_C_ARRAY_INIT;
	size_t nmodes = 0;
	size_t nlogical = 0;
	uint32_t layoutMode = 1; /* logical: sizes are divided by the scale */
	UINT32 count = 0;

	const int called =
	    sd_bus_call_method(bus, DC_NAME, DC_PATH, DC_NAME, "GetCurrentState", &err, &reply, "");
	if (called < 0)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_WARN(TAG, "GetCurrentState failed: %s",
		          err.message ? err.message : winpr_strerror(-called, buffer, sizeof(buffer)));
		sd_bus_error_free(&err);
		return 0;
	}

	const BOOL parsed = mutter_read_state(reply, modes, &nmodes, logical, &nlogical, &layoutMode);
	sd_bus_message_unref(reply);
	if (!parsed)
		WLog_WARN(TAG, "GetCurrentState: unexpected reply");

	/* primary first, then Mutter's order */
	for (int pass = 0; pass < 2; pass++)
	{
		for (size_t x = 0; (x < nlogical) && (count < max); x++)
		{
			if ((pass == 0) != (logical[x].primary != 0))
				continue;

			INT32 width = 0;
			INT32 height = 0;
			for (size_t m = 0; m < nmodes; m++)
			{
				if (strcmp(modes[m].connector, logical[x].connector) == 0)
				{
					width = modes[m].width;
					height = modes[m].height;
				}
			}
			if ((width <= 0) || (height <= 0))
				continue;

			/* 90 and 270 degrees, flipped or not */
			if (logical[x].transform & 1)
			{
				const INT32 tmp = width;
				width = height;
				height = tmp;
			}
			if (layoutMode == 1)
			{
				width = (INT32)(width / logical[x].scale + 0.5);
				height = (INT32)(height / logical[x].scale + 0.5);
			}

			MONITOR_DEF* monitor = &monitors[count];
			monitor->left = logical[x].x;
			monitor->top = logical[x].y;
			monitor->right = logical[x].x + width - 1;
			monitor->bottom = logical[x].y + height - 1;
			monitor->flags = logical[x].primary ? 1 : 0;
			if (connectors)
				(void)_snprintf(connectors[count], MUTTER_SHADOW_CONNECTOR_SIZE, "%s",
				                logical[x].connector);
			count++;
		}
	}

	return count;
}

WINPR_ATTR_NODISCARD
static UINT32 mutter_shadow_enum_monitors(MONITOR_DEF* monitors, UINT32 maxMonitors)
{
	WINPR_ASSERT(monitors);

	UINT32 width = 0;
	UINT32 height = 0;
	if ((maxMonitors > 0) && mutter_virtual_size(&width, &height))
	{
		/* the monitor this server will add: a GNOME session may have none yet */
		monitors[0] = (MONITOR_DEF){ 0, 0, (INT32)width - 1, (INT32)height - 1, 1 };
		return 1;
	}

	sd_bus* bus = nullptr;
	const int opened = sd_bus_open_user(&bus);
	if (opened < 0)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_WARN(TAG, "cannot connect to the session bus: %s",
		          winpr_strerror(-opened, buffer, sizeof(buffer)));
		return 0;
	}

	const UINT32 count = mutter_list_monitors(bus, monitors, nullptr, maxMonitors);
	WLog_DBG(TAG, "%" PRIu32 " monitors", count);
	sd_bus_flush_close_unref(bus);
	return count;
}

WINPR_ATTR_NODISCARD
static int mutter_on_stream_added(sd_bus_message* msg, void* data,
                                  WINPR_ATTR_UNUSED sd_bus_error* err)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
	WINPR_ASSERT(msg);
	WINPR_ASSERT(mutter);

	uint32_t node = 0;
	if (sd_bus_message_read(msg, "u", &node) >= 0)
	{
		mutter->nodeId = node;
		mutter->haveNode = TRUE;
		WLog_DBG(TAG, "PipeWire node %" PRIu32, mutter->nodeId);
		if (mutter->awaitNode)
		{
			mutter->awaitNode = FALSE;
			mutter->reconnect = TRUE;
		}
	}
	return 0;
}

WINPR_ATTR_NODISCARD
static int mutter_on_closed(WINPR_ATTR_UNUSED sd_bus_message* msg, void* data,
                            WINPR_ATTR_UNUSED sd_bus_error* err)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
	WINPR_ASSERT(mutter);

	mutter->closed = TRUE;
	return 0;
}

/* GNOME Shell left the session bus (it ended, or crashed): the sessions went with it */
WINPR_ATTR_NODISCARD
static int mutter_on_name_owner_changed(sd_bus_message* msg, void* data,
                                        WINPR_ATTR_UNUSED sd_bus_error* err)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
	WINPR_ASSERT(msg);
	WINPR_ASSERT(mutter);

	const char* name = nullptr;
	const char* oldOwner = nullptr;
	const char* newOwner = nullptr;
	if ((sd_bus_message_read(msg, "sss", &name, &oldOwner, &newOwner) >= 0) && newOwner &&
	    (newOwner[0] == '\0'))
	{
		mutter->shellGone = TRUE;
		mutter->closed = TRUE;
	}
	return 0;
}

/* The stream's "mapping-id" parameter names the libei region of its monitor */
static void mutter_read_stream_parameters(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = nullptr;
	const char* mappingId = nullptr;

	const int got = sd_bus_get_property(mutter->bus, SC_NAME, mutter->streamPath, SC_STREAM_IFACE,
	                                    "Parameters", &err, &reply, "a{sv}");
	if (got < 0)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_DBG(TAG, "no stream parameters: %s",
		         err.message ? err.message : winpr_strerror(-got, buffer, sizeof(buffer)));
		sd_bus_error_free(&err);
		return;
	}

	if (mutter_read_dict(reply, "mapping-id", 's', (void*)(&mappingId)) && mappingId)
	{
		/* the client threads look it up when they move the pointer */
		char* copy = _strdup(mappingId);
		EnterCriticalSection(&mutter->eiLock);
		free(mutter->mappingId);
		mutter->mappingId = copy;
		LeaveCriticalSection(&mutter->eiLock);
	}
	sd_bus_message_unref(reply);
}

/* A stream of the monitor, in the screen cast session, with the cursor drawn
 * into the picture.
 *
 * With the cursor as metadata instead (the clients would draw it, with no lag),
 * Mutter 50 records a cursor frame at each stage update and skips the picture
 * that follows it as "too early": on a stream that has just started, the
 * stage may keep updating and no picture comes at all, neither the first one
 * nor later changes (MUTTER_DEBUG=screen-cast: no full frame and about a
 * thousand skipped in 10 s). Drawn in, there are no cursor frames to skip. */
WINPR_ATTR_NODISCARD
static BOOL mutter_record_monitor(mutterShadowSubsystem* mutter, const char* connector)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(connector);

	sd_bus_message* call = nullptr;
	BOOL rc = FALSE;

	if (mutter->virtualMonitor)
	{
		/* a monitor of its own: Mutter records it directly, with no "too early"
		 * skips, so the clients draw the cursor (metadata, no lag) */
		if ((sd_bus_message_new_method_call(mutter->bus, &call, SC_NAME, mutter->scSessionPath,
		                                    SC_SESSION_IFACE, "RecordVirtual") < 0) ||
		    (sd_bus_message_append(call, "a{sv}", 1, "cursor-mode", "u",
		                           (uint32_t)MUTTER_CURSOR_MODE_METADATA) < 0))
			goto out;
		WLog_DBG(TAG, "ScreenCast.Session.RecordVirtual(%" PRIu32 "x%" PRIu32 ")",
		         mutter->virtualWidth, mutter->virtualHeight);
		if (!mutter_call_for_path(mutter->bus, call, "ScreenCast.Session.RecordVirtual",
		                          &mutter->streamPath))
			goto out;
	}
	else
	{
		if ((sd_bus_message_new_method_call(mutter->bus, &call, SC_NAME, mutter->scSessionPath,
		                                    SC_SESSION_IFACE, "RecordMonitor") < 0) ||
		    (sd_bus_message_append(call, "sa{sv}", connector, 1, "cursor-mode", "u",
		                           (uint32_t)MUTTER_CURSOR_MODE_EMBEDDED) < 0))
			goto out;
		WLog_DBG(TAG, "ScreenCast.Session.RecordMonitor('%s')", connector);
		if (!mutter_call_for_path(mutter->bus, call, "ScreenCast.Session.RecordMonitor",
		                          &mutter->streamPath))
			goto out;
	}

	if (sd_bus_match_signal(mutter->bus, &mutter->streamAddedSlot, SC_NAME, mutter->streamPath,
	                        SC_STREAM_IFACE, "PipeWireStreamAdded", mutter_on_stream_added,
	                        mutter) < 0)
	{
		WLog_ERR(TAG, "cannot watch the stream's signals");
		goto out;
	}
	rc = TRUE;
out:
	sd_bus_message_unref(call);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL mutter_setup_sessions(mutterShadowSubsystem* mutter, const char* connector)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(connector);

	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message* call = nullptr;
	sd_bus_message* reply = nullptr;
	char* sessionId = nullptr;
	BOOL rc = FALSE;

	/* 1. the remote desktop session */
	if (sd_bus_message_new_method_call(mutter->bus, &call, RD_NAME, RD_PATH, RD_NAME,
	                                   "CreateSession") < 0)
		goto out;
	WLog_DBG(TAG, "RemoteDesktop.CreateSession");
	if (!mutter_call_for_path(mutter->bus, call, "RemoteDesktop.CreateSession",
	                          &mutter->rdSessionPath))
		goto out;
	call = sd_bus_message_unref(call);

	const int gotId = sd_bus_get_property_string(mutter->bus, RD_NAME, mutter->rdSessionPath,
	                                             RD_SESSION_IFACE, "SessionId", &err, &sessionId);
	if (gotId < 0)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "no SessionId: %s",
		         err.message ? err.message : winpr_strerror(-gotId, buffer, sizeof(buffer)));
		goto out;
	}

	/* 2. the screen cast session, tied to it */
	if ((sd_bus_message_new_method_call(mutter->bus, &call, SC_NAME, SC_PATH, SC_NAME,
	                                    "CreateSession") < 0) ||
	    (sd_bus_message_append(call, "a{sv}", 1, "remote-desktop-session-id", "s", sessionId) < 0))
		goto out;
	WLog_DBG(TAG, "ScreenCast.CreateSession");
	if (!mutter_call_for_path(mutter->bus, call, "ScreenCast.CreateSession",
	                          &mutter->scSessionPath))
		goto out;
	call = sd_bus_message_unref(call);

	/* 3. the monitor */
	if (!mutter_record_monitor(mutter, connector))
		goto out;

	if ((sd_bus_match_signal(mutter->bus, &mutter->closedSlot, RD_NAME, mutter->rdSessionPath,
	                         RD_SESSION_IFACE, "Closed", mutter_on_closed, mutter) < 0) ||
	    (sd_bus_add_match(mutter->bus, &mutter->ownerSlot,
	                      "type='signal',sender='org.freedesktop.DBus',"
	                      "path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',"
	                      "member='NameOwnerChanged',arg0='" RD_NAME "'",
	                      mutter_on_name_owner_changed, mutter) < 0))
	{
		WLog_ERR(TAG, "cannot watch the stream's signals");
		goto out;
	}

	/* 4. start, then wait for the PipeWire node */
	WLog_DBG(TAG, "RemoteDesktop.Session.Start");
	const int started = sd_bus_call_method(mutter->bus, RD_NAME, mutter->rdSessionPath,
	                                       RD_SESSION_IFACE, "Start", &err, nullptr, "");
	if (started < 0)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "RemoteDesktop.Session.Start failed: %s",
		         err.message ? err.message : winpr_strerror(-started, buffer, sizeof(buffer)));
		goto out;
	}
	mutter->started = TRUE;

	const UINT64 start = GetTickCount64();
	while (!mutter->haveNode)
	{
		const int processed = sd_bus_process(mutter->bus, nullptr);
		if (processed < 0)
			goto out;
		if (processed > 0)
			continue;
		if (GetTickCount64() - start > MUTTER_STREAM_TIMEOUT_MS)
		{
			WLog_ERR(TAG, "Mutter announced no PipeWire stream");
			goto out;
		}
		(void)sd_bus_wait(mutter->bus, 100ull * 1000ull);
	}
	mutter_read_stream_parameters(mutter);

	/* 5. input */
	int fd = -1;
	const int connected =
	    sd_bus_call_method(mutter->bus, RD_NAME, mutter->rdSessionPath, RD_SESSION_IFACE,
	                       "ConnectToEIS", &err, &reply, "a{sv}", 0);
	if ((connected < 0) || (sd_bus_message_read(reply, "h", &fd) < 0))
	{
		WLog_WARN(TAG, "ConnectToEIS failed: %s: the clients can only watch",
		          err.message ? err.message : "bad reply");
	}
	else
	{
		/* the reply owns the fd */
		fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
		mutter->ei = ei_new_sender(mutter);
		if (mutter->ei)
		{
			ei_configure_name(mutter->ei, "FreeRDP shadow server");
			if (ei_setup_backend_fd(mutter->ei, fd) != 0)
			{
				WLog_WARN(TAG, "libei setup failed: the clients can only watch");
				mutter->ei = ei_unref(mutter->ei);
			}
		}
		else
			close(fd);
	}

	rc = TRUE;
out:
	sd_bus_error_free(&err);
	sd_bus_message_unref(reply);
	sd_bus_message_unref(call);
	free(sessionId);
	return rc;
}

/* ------------------------------------------------------------------------ */
/* the cursor                                                                */
/* ------------------------------------------------------------------------ */

static void mutter_cursor_msg_free(UINT32 id, SHADOW_MSG_OUT* msg)
{
	if (msg && (id == SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID))
	{
		SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* shape = (SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE*)msg;
		free(shape->xorMaskData);
		free(shape->andMaskData);
	}
	free(msg);
}

WINPR_ATTR_NODISCARD
static UINT32 mutter_cursor_hash(const BYTE* pixels, UINT32 width, UINT32 height, UINT32 xHot,
                                 UINT32 yHot)
{
	WINPR_ASSERT(pixels);

	/* FNV-1a over the size, the hot spot and the pixels */
	UINT32 hash = 2166136261u;
	const UINT32 header[] = { width, height, xHot, yHot };
	const BYTE* parts[] = { (const BYTE*)header, pixels };
	const size_t sizes[] = { sizeof(header), 4ull * width * height };
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

static void mutter_cursor_send_shape(mutterShadowSubsystem* mutter, const BYTE* pixels,
                                     UINT32 width, UINT32 height, UINT32 xHot, UINT32 yHot)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(pixels);

	const UINT32 hash = mutter_cursor_hash(pixels, width, height, xHot, yHot);
	WLog_DBG(TAG, "pointer shape %" PRIu32 "x%" PRIu32 ", resend %d", width, height,
	         mutter->cursorResend);
	if (mutter->cursorKnown && !mutter->cursorResend && (hash == mutter->cursorHash))
		return;

	SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* msg =
	    calloc(1, sizeof(SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE));
	if (!msg)
		return;

	msg->xHot = xHot;
	msg->yHot = yHot;
	msg->width = width;
	msg->height = height;
	/* Mutter's bitmaps are premultiplied RGBA */
	if (shadow_subsystem_pointer_convert_alpha_pointer_data_to_format(pixels, PIXEL_FORMAT_RGBA32,
	                                                                  TRUE, width, height, msg) < 0)
	{
		free(msg);
		return;
	}

	msg->common.Free = mutter_cursor_msg_free;
	if (shadow_client_boardcast_msg(mutter->common.server, nullptr,
	                                SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID, &msg->common,
	                                nullptr) >= 0)
	{
		mutter->cursorHash = hash;
		mutter->cursorKnown = TRUE;
		mutter->cursorResend = FALSE;
	}
}

static void mutter_cursor_send_position(mutterShadowSubsystem* mutter, INT32 x, INT32 y)
{
	WINPR_ASSERT(mutter);

	if ((x == mutter->cursorX) && (y == mutter->cursorY))
		return;
	mutter->cursorX = x;
	mutter->cursorY = y;

	/* while a client moves the pointer, it already knows where it is */
	EnterCriticalSection(&mutter->eiLock);
	const UINT64 input = mutter->pointerInput;
	LeaveCriticalSection(&mutter->eiLock);
	if (GetTickCount64() - input < MUTTER_SHADOW_CURSOR_QUIET_MS)
		return;

	if ((x < 0) || (y < 0) || ((UINT32)x >= mutter->width) || ((UINT32)y >= mutter->height))
		return;

	mutter->common.pointerX = (UINT32)x;
	mutter->common.pointerY = (UINT32)y;

	SHADOW_MSG_OUT_POINTER_POSITION_UPDATE* msg =
	    calloc(1, sizeof(SHADOW_MSG_OUT_POINTER_POSITION_UPDATE));
	if (!msg)
		return;
	msg->xPos = (UINT32)x;
	msg->yPos = (UINT32)y;
	msg->common.Free = mutter_cursor_msg_free;
	/* the message is freed with its last reference, also when no client took it */
	if (shadow_client_boardcast_msg(mutter->common.server, nullptr,
	                                SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID, &msg->common,
	                                nullptr) < 0)
		WLog_DBG(TAG, "the pointer position was not sent");
}

/* Mutter sends a shape only when it changes, so a client that connects later would
 * have no pointer until something else changed it */
static void mutter_cursor_remember(mutterShadowSubsystem* mutter, const BYTE* pixels, UINT32 width,
                                   UINT32 height, UINT32 xHot, UINT32 yHot)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(pixels);

	const size_t size = 4ull * width * height;
	BYTE* copy = malloc(size);
	if (!copy)
		return;
	memcpy(copy, pixels, size);

	free(mutter->cursorPixels);
	mutter->cursorPixels = copy;
	mutter->cursorWidth = width;
	mutter->cursorHeight = height;
	mutter->cursorXHot = xHot;
	mutter->cursorYHot = yHot;
}

/* Until Mutter has sent a shape, which it may not do for long: an arrow, so a client is
 * never left with no pointer */
static void mutter_cursor_send_default(mutterShadowSubsystem* mutter)
{
	static const char* const arrow[] = { "X          ", "XX         ", "X.X        ", "X..X       ",
		                                 "X...X      ", "X....X     ", "X.....X    ", "X......X   ",
		                                 "X.......X  ", "X........X ", "X.....XXXXX", "X..X..X    ",
		                                 "X.X X..X   ", "XX  X..X   ", "X    X..X  ", "     X..X  ",
		                                 "      XX   " };
	enum
	{
		ARROW_WIDTH = 11,
		ARROW_HEIGHT = ARRAYSIZE(arrow)
	};
	BYTE pixels[4ull * ARROW_WIDTH * ARROW_HEIGHT] = WINPR_C_ARRAY_INIT;
	for (size_t y = 0; y < ARROW_HEIGHT; y++)
	{
		for (size_t x = 0; x < ARROW_WIDTH; x++)
		{
			BYTE* pixel = &pixels[4ull * (y * ARROW_WIDTH + x)];
			if (arrow[y][x] == ' ')
				continue;
			const BYTE value = (arrow[y][x] == '.') ? 0xff : 0x00;
			pixel[0] = pixel[1] = pixel[2] = value;
			pixel[3] = 0xff;
		}
	}
	mutter_cursor_send_shape(mutter, pixels, ARROW_WIDTH, ARROW_HEIGHT, 0, 0);
}

/* The cursor in a buffer's metadata: its position, and its shape when it changed */
static void mutter_cursor_update(mutterShadowSubsystem* mutter, struct spa_buffer* buf)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(buf);

	struct spa_meta* meta = spa_buffer_find_meta(buf, SPA_META_Cursor);
	if (!meta || (meta->size < sizeof(struct spa_meta_cursor)))
		return;

	const struct spa_meta_cursor* mc = (const struct spa_meta_cursor*)meta->data;
	if (mc->id == 0)
		return; /* no cursor in this buffer */

	if (mc->bitmap_offset != 0)
	{
		if ((size_t)mc->bitmap_offset + sizeof(struct spa_meta_bitmap) > meta->size)
			return;

		const struct spa_meta_bitmap* mb =
		    SPA_PTROFF(mc, mc->bitmap_offset, const struct spa_meta_bitmap);
		if ((mb->format == 0) || (mb->offset == 0) || (mb->size.width == 0) ||
		    (mb->size.height == 0))
		{
			/* Mutter sends an empty bitmap for a hidden pointer */
			const BYTE transparent[4] = WINPR_C_ARRAY_INIT;
			mutter_cursor_remember(mutter, transparent, 1, 1, 0, 0);
			mutter_cursor_send_shape(mutter, transparent, 1, 1, 0, 0);
		}
		else
		{
			const UINT32 width = mb->size.width;
			const UINT32 height = mb->size.height;
			const UINT32 stride = (mb->stride > 0) ? (UINT32)mb->stride : width * 4;
			if ((mb->format != SPA_VIDEO_FORMAT_RGBA) || (width > MUTTER_CURSOR_META_MAX) ||
			    (height > MUTTER_CURSOR_META_MAX) || (stride < width * 4) ||
			    ((size_t)mc->bitmap_offset + mb->offset + (size_t)stride * height > meta->size))
			{
				WLog_DBG(TAG,
				         "unusable pointer bitmap (format %" PRIu32 ", %" PRIu32 "x%" PRIu32 ")",
				         mb->format, width, height);
			}
			else
			{
				const BYTE* src = SPA_PTROFF(mb, mb->offset, const BYTE);

				/* a pointer larger than the clients take is shrunk, nearest pixel */
				const UINT32 side = MAX(width, height);
				const UINT32 scaled = MIN(side, MUTTER_SHADOW_CURSOR_MAX);
				const UINT32 outW = MAX(1, width * scaled / side);
				const UINT32 outH = MAX(1, height * scaled / side);
				BYTE* pixels = calloc(4ull * outW, outH);
				if (pixels)
				{
					for (UINT32 y = 0; y < outH; y++)
					{
						const BYTE* row = src + (size_t)(y * side / scaled) * stride;
						for (UINT32 x = 0; x < outW; x++)
							memcpy(&pixels[4ull * (y * outW + x)], &row[4ull * (x * side / scaled)],
							       4);
					}
					const UINT32 xHot =
					    MIN((UINT32)MAX(mc->hotspot.x, 0) * scaled / side, outW - 1);
					const UINT32 yHot =
					    MIN((UINT32)MAX(mc->hotspot.y, 0) * scaled / side, outH - 1);
					mutter_cursor_remember(mutter, pixels, outW, outH, xHot, yHot);
					mutter_cursor_send_shape(mutter, pixels, outW, outH, xHot, yHot);
					free(pixels);
				}
			}
		}
	}

	mutter_cursor_send_position(mutter, mc->position.x, mc->position.y);
}

/* ------------------------------------------------------------------------ */
/* PipeWire                                                                  */
/* ------------------------------------------------------------------------ */

static void mutter_set_monitor_size(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	rdpShadowSubsystem* subsystem = &mutter->common;
	MONITOR_DEF* monitor = &subsystem->monitors[subsystem->selectedMonitor];

	/* the surface has the size of the frames */
	monitor->right = monitor->left + (INT32)mutter->width - 1;
	monitor->bottom = monitor->top + (INT32)mutter->height - 1;

	MONITOR_DEF* virtualScreen = &subsystem->virtualScreen;
	*virtualScreen = subsystem->monitors[0];
	for (UINT32 x = 1; x < subsystem->numMonitors; x++)
	{
		const MONITOR_DEF* m = &subsystem->monitors[x];
		virtualScreen->left = MIN(virtualScreen->left, m->left);
		virtualScreen->top = MIN(virtualScreen->top, m->top);
		virtualScreen->right = MAX(virtualScreen->right, m->right);
		virtualScreen->bottom = MAX(virtualScreen->bottom, m->bottom);
	}
	virtualScreen->flags = 1;
}

static void mutter_on_param_changed(void* data, uint32_t id, const struct spa_pod* param)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
	WINPR_ASSERT(mutter);

	struct spa_video_info_raw info = WINPR_C_ARRAY_INIT;
	uint8_t podBuffer[1024] = WINPR_C_ARRAY_INIT;
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
	const struct spa_pod* params[4] = WINPR_C_ARRAY_INIT;

	if (!param || (id != SPA_PARAM_Format))
		return;
	if (spa_format_video_raw_parse(param, &info) < 0)
	{
		WLog_ERR(TAG, "cannot parse the stream format");
		return;
	}

	const BOOL sized = mutter->haveFormat;
	const BOOL changed = (info.size.width != mutter->width) || (info.size.height != mutter->height);
	mutter->width = info.size.width;
	mutter->height = info.size.height;
	mutter->format =
	    (info.format == SPA_VIDEO_FORMAT_BGRA) ? PIXEL_FORMAT_BGRA32 : PIXEL_FORMAT_BGRX32;
	mutter->haveFormat = TRUE;
	mutter->fullPaint = TRUE;
	WLog_INFO(TAG, "stream: %" PRIu32 "x%" PRIu32 " %s", mutter->width, mutter->height,
	          (info.format == SPA_VIDEO_FORMAT_BGRA) ? "BGRA" : "BGRx");

	if (sized && changed)
	{
		/* the monitor changed its mode: the clients get the new size */
		mutter_set_monitor_size(mutter);
		if (!shadow_screen_resize(mutter->common.server->screen))
			WLog_ERR(TAG, "cannot resize the screen to %" PRIu32 "x%" PRIu32, mutter->width,
			         mutter->height);
	}

	/* shared memory only (no dma-buf), with the damage and the cursor */
	const int32_t stride = (int32_t)mutter->width * 4;
	params[0] = spa_pod_builder_add_object(
	    &b, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers, SPA_PARAM_BUFFERS_buffers,
	    SPA_POD_CHOICE_RANGE_Int(4, 2, 16), SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
	    SPA_PARAM_BUFFERS_size, SPA_POD_Int(stride * (int32_t)mutter->height),
	    SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride), SPA_PARAM_BUFFERS_dataType,
	    SPA_POD_CHOICE_FLAGS_Int((1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr)));
	params[1] = spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
	                                       SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
	                                       SPA_PARAM_META_size,
	                                       SPA_POD_Int(sizeof(struct spa_meta_header)));
	params[2] = spa_pod_builder_add_object(
	    &b, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
	    SPA_POD_Id(SPA_META_VideoDamage), SPA_PARAM_META_size,
	    SPA_POD_CHOICE_RANGE_Int(sizeof(struct spa_meta_region) * MUTTER_MAX_DAMAGE,
	                             sizeof(struct spa_meta_region),
	                             sizeof(struct spa_meta_region) * MUTTER_MAX_DAMAGE));
	params[3] = spa_pod_builder_add_object(
	    &b, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
	    SPA_POD_Id(SPA_META_Cursor), SPA_PARAM_META_size,
	    SPA_POD_CHOICE_RANGE_Int(
	        MUTTER_CURSOR_META_SIZE(64, 64), MUTTER_CURSOR_META_SIZE(1, 1),
	        MUTTER_CURSOR_META_SIZE(MUTTER_CURSOR_META_MAX, MUTTER_CURSOR_META_MAX)));
	(void)pw_stream_update_params(mutter->stream, params, ARRAYSIZE(params));
}

/* Copies one rectangle of a frame to the surface and marks it changed */
static void mutter_copy_rect(mutterShadowSubsystem* mutter, rdpShadowSurface* surface,
                             const BYTE* src, UINT32 stride, INT32 x, INT32 y, INT32 width,
                             INT32 height)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(surface);
	WINPR_ASSERT(src);

	const INT32 maxW = (INT32)MIN(mutter->width, surface->width);
	const INT32 maxH = (INT32)MIN(mutter->height, surface->height);
	const INT32 left = MAX(x, 0);
	const INT32 top = MAX(y, 0);
	const INT32 right = MIN(x + width, maxW);
	const INT32 bottom = MIN(y + height, maxH);
	if ((right <= left) || (bottom <= top))
		return;

	if (!freerdp_image_copy_no_overlap(surface->data, surface->format, surface->scanline,
	                                   (UINT32)left, (UINT32)top, (UINT32)(right - left),
	                                   (UINT32)(bottom - top), src, mutter->format, stride,
	                                   (UINT32)left, (UINT32)top, nullptr, FREERDP_FLIP_NONE))
	{
		WLog_ERR(TAG, "cannot copy the frame to the surface");
		return;
	}
	const RECTANGLE_16 rect = { (UINT16)left, (UINT16)top, (UINT16)right, (UINT16)bottom };
	if (!region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &rect))
		WLog_ERR(TAG, "cannot mark the changed region");
}

/* Mutter's damage metadata now and then leaves out a change: a window that opens or closes, the
 * clock in the top bar. Nothing repaints it later, so the client kept the old picture there.
 * Compare the buffer with the surface in tiles after the damage was copied, and copy the tiles that
 * still differ. When the damage was right, every tile is equal and nothing is copied. Returns the
 * tiles copied. */
static UINT32 mutter_repair_missed_damage(mutterShadowSubsystem* mutter, rdpShadowSurface* surface,
                                          const BYTE* src, UINT32 stride)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(surface);
	WINPR_ASSERT(src);

	const UINT32 width = MIN(mutter->width, surface->width);
	const UINT32 height = MIN(mutter->height, surface->height);
	/* the same format is copied byte for byte; otherwise compare the colour (alpha is undefined
	 * in BGRx) */
	const BOOL sameFormat = (mutter->format == surface->format);
	UINT32 tiles = 0;

	for (UINT32 ty = 0; ty < height; ty += MUTTER_CHECK_TILE)
	{
		const UINT32 th = MIN(MUTTER_CHECK_TILE, height - ty);
		for (UINT32 tx = 0; tx < width; tx += MUTTER_CHECK_TILE)
		{
			const UINT32 tw = MIN(MUTTER_CHECK_TILE, width - tx);
			BOOL dirty = FALSE;
			for (UINT32 y = ty; (y < ty + th) && !dirty; y++)
			{
				const BYTE* a = &src[1ull * y * stride + 4ull * tx];
				const BYTE* b = &surface->data[1ull * y * surface->scanline + 4ull * tx];
				if (sameFormat)
					dirty = (memcmp(a, b, 4ull * tw) != 0);
				else
				{
					for (UINT32 x = 0; (x < tw) && !dirty; x++)
						dirty = (memcmp(&a[4ull * x], &b[4ull * x], 3) != 0);
				}
			}
			if (dirty)
			{
				mutter_copy_rect(mutter, surface, src, stride, (INT32)tx, (INT32)ty, (INT32)tw,
				                 (INT32)th);
				tiles++;
			}
		}
	}
	return tiles;
}

/* counts and logs what mutter_repair_missed_damage found */
static void mutter_damage_stats(mutterShadowSubsystem* mutter, UINT32 tiles)
{
	WINPR_ASSERT(mutter);

	mutter->damageFrames++;
	mutter->damageTiles += tiles;
	if ((tiles > 0) && !mutter->damageMissSeen)
	{
		mutter->damageMissSeen = TRUE;
		WLog_INFO(TAG,
		          "Mutter left changes out of its damage: %" PRIu32
		          " tile(s) copied, the check stays on (" MUTTER_DAMAGE_CHECK_ENV
		          "=0 turns it off)",
		          tiles);
	}
	const UINT64 now = GetTickCount64();
	if (now - mutter->damageLogged >= 5000)
	{
		WLog_DBG(TAG, "damage check: %" PRIu64 " tile(s) Mutter left out, in %" PRIu64 " frame(s)",
		         mutter->damageTiles, mutter->damageFrames);
		mutter->damageTiles = 0;
		mutter->damageFrames = 0;
		mutter->damageLogged = now;
	}
}

static void mutter_on_process(void* data)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
	WINPR_ASSERT(mutter);

	rdpShadowServer* server = mutter->common.server;
	if (!server || !server->surface)
		return;
	rdpShadowSurface* surface = server->surface;
	BOOL painted = FALSE;
	struct pw_buffer* pb = nullptr;

	/* every queued buffer: no damage is lost when frames come faster */
	while ((pb = pw_stream_dequeue_buffer(mutter->stream)) != nullptr)
	{
		struct spa_buffer* buf = pb->buffer;
		struct spa_data* d = &buf->datas[0]; // codespell:ignore datas

		mutter_cursor_update(mutter, buf);

		const struct spa_meta_header* h =
		    spa_buffer_find_meta_data(buf, SPA_META_Header, sizeof(struct spa_meta_header));
		if ((h && (h->flags & SPA_META_HEADER_FLAG_CORRUPTED)) || !d->data || !d->chunk ||
		    (d->chunk->size == 0))
		{
			/* no picture in this buffer (only the cursor moved) */
			(void)pw_stream_queue_buffer(mutter->stream, pb);
			continue;
		}

		const BYTE* src = (const BYTE*)d->data + d->chunk->offset;
		const UINT32 stride = (d->chunk->stride > 0) ? (UINT32)d->chunk->stride : mutter->width * 4;

		EnterCriticalSection(&surface->lock);
		struct spa_meta* dm = spa_buffer_find_meta(buf, SPA_META_VideoDamage);
		if (!dm || mutter->fullPaint)
		{
			mutter_copy_rect(mutter, surface, src, stride, 0, 0, (INT32)mutter->width,
			                 (INT32)mutter->height);
			mutter->fullPaint = FALSE;
		}
		else
		{
			struct spa_meta_region* r = nullptr;
			spa_meta_for_each(r, dm)
			{
				if (!spa_meta_region_is_valid(r))
					break;
				mutter_copy_rect(mutter, surface, src, stride, r->region.position.x,
				                 r->region.position.y, (INT32)r->region.size.width,
				                 (INT32)r->region.size.height);
			}
			if (mutter->damageCheck)
				mutter_damage_stats(mutter,
				                    mutter_repair_missed_damage(mutter, surface, src, stride));
		}
		LeaveCriticalSection(&surface->lock);
		painted = TRUE;
		(void)pw_stream_queue_buffer(mutter->stream, pb);
	}

	if (!painted)
		return;

	/* Not under the clients list lock: the frame update waits until every client has encoded
	 * the frame (an AV2 frame takes 40 to 120 ms, a key frame up to 800 ms), and the sound
	 * thread needs the lock for each chunk it hands to the clients; it stalled, and PipeWire
	 * dropped the sound. The update has a subscriber list of its own (see win_shadow.c). */
	if (ArrayList_Count(server->clients) > 0)
		shadow_subsystem_frame_update(&mutter->common);

	EnterCriticalSection(&surface->lock);
	region16_clear(&surface->invalidRegion);
	LeaveCriticalSection(&surface->lock);
}

static void mutter_on_state_changed(WINPR_ATTR_UNUSED void* data,
                                    WINPR_ATTR_UNUSED enum pw_stream_state old,
                                    enum pw_stream_state state, const char* error)
{
	WLog_DBG(TAG, "stream %s%s%s", pw_stream_state_as_string(state), error ? ": " : "",
	         error ? error : "");
}

static const struct pw_stream_events mutter_stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = mutter_on_state_changed,
	.param_changed = mutter_on_param_changed,
	.process = mutter_on_process,
};

static void mutter_on_core_error(WINPR_ATTR_UNUSED void* data, uint32_t id,
                                 WINPR_ATTR_UNUSED int seq, WINPR_ATTR_UNUSED int res,
                                 const char* message)
{
	WLog_ERR(TAG, "PipeWire error on %" PRIu32 ": %s", id, message);
}

static const struct pw_core_events mutter_core_events = {
	PW_VERSION_CORE_EVENTS,
	.error = mutter_on_core_error,
};

/* Mutter ends a monitor's stream when the monitor's layout changes (its scale,
 * its rotation, its position): it removes the node, and the session goes on
 * without a picture. No D-Bus signal says so; PipeWire's registry does. */
static void mutter_on_global_remove(void* data, uint32_t id)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
	WINPR_ASSERT(mutter);

	if (mutter->haveNode && (id == mutter->nodeId))
	{
		mutter->haveNode = FALSE;
		mutter->streamGone = TRUE;
		mutter->streamGoneAt = GetTickCount64();
	}
}

static const struct pw_registry_events mutter_registry_events = {
	PW_VERSION_REGISTRY_EVENTS,
	.global_remove = mutter_on_global_remove,
};

WINPR_ATTR_NODISCARD
static BOOL mutter_stream_connect_node(mutterShadowSubsystem* mutter);

WINPR_ATTR_NODISCARD
static BOOL mutter_connect_stream(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	pw_init(nullptr, nullptr);
	mutter->loop = pw_loop_new(nullptr);
	if (!mutter->loop)
		return FALSE;
	mutter->context = pw_context_new(mutter->loop, nullptr, 0);
	if (!mutter->context)
		return FALSE;
	/* the session's PipeWire daemon, through XDG_RUNTIME_DIR */
	mutter->core = pw_context_connect(mutter->context, nullptr, 0);
	if (!mutter->core)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "cannot connect to PipeWire: %s",
		         winpr_strerror(errno, buffer, sizeof(buffer)));
		return FALSE;
	}
	pw_core_add_listener(mutter->core, &mutter->coreListener, &mutter_core_events, mutter);

	mutter->registry = pw_core_get_registry(mutter->core, PW_VERSION_REGISTRY, 0);
	if (!mutter->registry)
		return FALSE;
	pw_registry_add_listener(mutter->registry, &mutter->registryListener, &mutter_registry_events,
	                         mutter);

	if (!mutter_stream_connect_node(mutter))
		return FALSE;

	/* the surface needs the stream's size before the server starts */
	const UINT64 start = GetTickCount64();
	while (!mutter->haveFormat)
	{
		if (GetTickCount64() - start > MUTTER_FORMAT_TIMEOUT_MS)
		{
			WLog_ERR(TAG, "the stream got no format");
			return FALSE;
		}
		pw_loop_enter(mutter->loop);
		(void)pw_loop_iterate(mutter->loop, 100);
		pw_loop_leave(mutter->loop);
	}
	return TRUE;
}

/* A new stream, connected to Mutter's node mutter->nodeId. Each node gets a
 * stream of its own: a stream connected again keeps its first target. */
WINPR_ATTR_NODISCARD
static BOOL mutter_stream_connect_node(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(mutter->core);

	if (mutter->stream)
		pw_stream_destroy(mutter->stream);
	mutter->stream =
	    pw_stream_new(mutter->core, "FreeRDP shadow server",
	                  pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
	                                    "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr));
	if (!mutter->stream)
		return FALSE;
	pw_stream_add_listener(mutter->stream, &mutter->streamListener, &mutter_stream_events, mutter);

	uint8_t podBuffer[1024] = WINPR_C_ARRAY_INIT;
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
	/* a virtual monitor takes the size the stream asks for */
	const struct spa_rectangle size =
	    mutter->virtualMonitor ? SPA_RECTANGLE(mutter->virtualWidth, mutter->virtualHeight)
	                           : SPA_RECTANGLE(1920, 1080);
	const struct spa_rectangle minSize = SPA_RECTANGLE(1, 1);
	const struct spa_rectangle maxSize = SPA_RECTANGLE(8192, 8192);
	const struct spa_fraction rate = SPA_FRACTION(0, 1);
	const struct spa_fraction maxRate = SPA_FRACTION(60, 1);
	const struct spa_fraction minRate = SPA_FRACTION(1, 1);

	/* no modifiers offered: Mutter sends frames in shared memory */
	const struct spa_pod* params[] = { spa_pod_builder_add_object(
		&b, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat, SPA_FORMAT_mediaType,
		SPA_POD_Id(SPA_MEDIA_TYPE_video), SPA_FORMAT_mediaSubtype,
		SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), SPA_FORMAT_VIDEO_format,
		SPA_POD_CHOICE_ENUM_Id(3, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx,
		                       SPA_VIDEO_FORMAT_BGRA),
		SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&size, &minSize, &maxSize),
		SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&rate), SPA_FORMAT_VIDEO_maxFramerate,
		SPA_POD_CHOICE_RANGE_Fraction(&maxRate, &minRate, &maxRate)) };

	if (pw_stream_connect(mutter->stream, PW_DIRECTION_INPUT, mutter->nodeId,
	                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS, params,
	                      ARRAYSIZE(params)) < 0)
	{
		WLog_ERR(TAG, "pw_stream_connect failed");
		return FALSE;
	}
	return TRUE;
}

/* ------------------------------------------------------------------------ */
/* sound                                                                     */
/* ------------------------------------------------------------------------ */

static void mutter_audio_msg_free(WINPR_ATTR_UNUSED UINT32 id, SHADOW_MSG_OUT* msg)
{
	SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* samples = (SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES*)msg;
	if (samples)
		free(samples->buf);
	free(samples);
}

/* Sends what s holds to every client, as one message */
static void mutter_audio_post(mutterShadowSubsystem* mutter, wStream* s)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(s);

	const size_t size = Stream_GetPosition(s);
	Stream_ResetPosition(s);
	if (size == 0)
		return;

	rdpShadowServer* server = mutter->common.server;
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
	mutter->audioMessages++;
	msg->common.Free = mutter_audio_msg_free;
	msg->audio_format = &mutter->audioSourceFormat;
	msg->nFrames = size / MUTTER_AUDIO_FRAME_BYTES;
	msg->wTimestamp = (UINT16)(GetTickCount64() & 0xFFFF);
	/* the message is freed with its last reference, also when no client took it */
	if (shadow_client_boardcast_msg(server, nullptr, SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES_ID,
	                                &msg->common, nullptr) < 0)
		WLog_DBG(TAG, "sound: a message was not sent");
}

static void mutter_audio_stats(mutterShadowSubsystem* mutter, size_t frames)
{
	WINPR_ASSERT(mutter);

	const UINT64 now = GetTickCount64();
	if (mutter->audioLogged == 0)
	{
		WLog_INFO(TAG, "sound: capturing what the default sink plays");
		mutter->audioLogged = now;
	}
	else if (now - mutter->audioLogged >= MUTTER_AUDIO_LOG_MS)
	{
		WLog_DBG(TAG,
		         "sound: %" PRIu64 " frames captured, %" PRIu64
		         " messages sent, peak %.3f in the last %" PRIu64 " ms",
		         mutter->audioFrames, mutter->audioMessages, (double)mutter->audioPeak,
		         now - mutter->audioLogged);
		mutter->audioFrames = 0;
		mutter->audioMessages = 0;
		mutter->audioPeak = 0.0f;
		mutter->audioLogged = now;
	}
	mutter->audioFrames += frames;
}

static void mutter_on_audio_process(void* data)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
	WINPR_ASSERT(mutter);

	struct pw_buffer* pb = nullptr;

	while ((pb = pw_stream_dequeue_buffer(mutter->audio)) != nullptr)
	{
		const struct spa_data* d = &pb->buffer->datas[0]; // codespell:ignore datas
		if (d->data && d->chunk && (d->chunk->size > 0) &&
		    ((size_t)d->chunk->offset + d->chunk->size <= d->maxsize))
		{
			const BYTE* bdata = d->data;
			const BYTE* offset = &bdata[d->chunk->offset];
			const INT16* samples = WINPR_PACKED_ALIGN_CAST(const INT16*, offset);
			const size_t frames = d->chunk->size / MUTTER_AUDIO_FRAME_BYTES;
			mutter_audio_stats(mutter, frames);

			wStream* s = mutter->audioStream;
			for (size_t f = 0; f < frames; f++)
			{
				for (size_t c = 0; c < MUTTER_AUDIO_CHANNELS; c++)
				{
					const INT16 value = samples[f * MUTTER_AUDIO_CHANNELS + c];
					mutter->audioPeak =
					    MAX(mutter->audioPeak, fabsf((float)value / (float)INT16_MAX));
					Stream_Write_INT16(s, value);
				}

				if (Stream_GetPosition(s) >= MUTTER_AUDIO_MESSAGE_FRAMES * MUTTER_AUDIO_FRAME_BYTES)
					mutter_audio_post(mutter, s);
			}
		}
		(void)pw_stream_queue_buffer(mutter->audio, pb);
	}
}

static const struct pw_stream_events mutter_audio_events = {
	PW_VERSION_STREAM_EVENTS,
	.process = mutter_on_audio_process,
};

/* The formats offered to the clients, and a capture of the default sink's
 * monitor, inactive until a client connects. Without it there is no sound,
 * and the rest works. */
WINPR_ATTR_NODISCARD
static BOOL mutter_audio_init(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	uint8_t podBuffer[1024] = WINPR_C_ARRAY_INIT;
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));

	AUDIO_FORMAT* formats = audio_formats_new(ARRAYSIZE(mutter_audio_offered_formats));
	if (!formats)
		return FALSE;

	size_t count = 0;
	for (size_t x = 0; x < ARRAYSIZE(mutter_audio_offered_formats); x++)
	{
		const AUDIO_FORMAT* format = &mutter_audio_offered_formats[x];
		if (freerdp_dsp_supports_format(format, TRUE))
			formats[count++] = *format;
	}

	mutter->audioStream =
	    Stream_New(nullptr, MUTTER_AUDIO_MESSAGE_FRAMES * MUTTER_AUDIO_FRAME_BYTES);
	if (!mutter->audioStream || (count == 0))
		goto fail;

	mutter->audio =
	    pw_stream_new(mutter->core, "FreeRDP shadow server sound",
	                  pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY,
	                                    "Capture", PW_KEY_MEDIA_ROLE, "Screen",
	                                    PW_KEY_STREAM_CAPTURE_SINK, "true", nullptr));
	if (!mutter->audio)
		goto fail;
	pw_stream_add_listener(mutter->audio, &mutter->audioListener, &mutter_audio_events, mutter);

	struct spa_audio_info_raw info = WINPR_C_ARRAY_INIT;
	info.format = SPA_AUDIO_FORMAT_S16_LE;
	info.rate = MUTTER_AUDIO_RATE;
	info.channels = MUTTER_AUDIO_CHANNELS;
	info.position[0] = SPA_AUDIO_CHANNEL_FL;
	info.position[1] = SPA_AUDIO_CHANNEL_FR;
	const struct spa_pod* params[] = { spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
		                                                          &info) };

	/* PipeWire links it to the default sink's monitor and converts to our format */
	if (pw_stream_connect(mutter->audio, PW_DIRECTION_INPUT, PW_ID_ANY,
	                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
	                          PW_STREAM_FLAG_INACTIVE,
	                      params, ARRAYSIZE(params)) < 0)
		goto fail;

	mutter->common.rdpsndFormats = formats;
	mutter->common.nRdpsndFormats = count;
	return TRUE;

fail:
	if (mutter->audio)
		pw_stream_destroy(mutter->audio);
	mutter->audio = nullptr;
	Stream_Free(mutter->audioStream, TRUE);
	mutter->audioStream = nullptr;
	audio_formats_free(formats, ARRAYSIZE(mutter_audio_offered_formats));
	return FALSE;
}

static void mutter_audio_uninit(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	if (mutter->audio)
		pw_stream_destroy(mutter->audio);
	mutter->audio = nullptr;
	mutter->audioActive = FALSE;
	audio_formats_free(mutter->common.rdpsndFormats, mutter->common.nRdpsndFormats);
	mutter->common.rdpsndFormats = nullptr;
	mutter->common.nRdpsndFormats = 0;
	Stream_Free(mutter->audioStream, TRUE);
	mutter->audioStream = nullptr;
}

/* in the subsystem thread: capture only while clients are connected */
static void mutter_audio_follow_clients(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	if (!mutter->audio)
		return;

	const BOOL active = InterlockedCompareExchange(&mutter->clients, 0, 0) > 0;
	if (active == mutter->audioActive)
		return;

	if (pw_stream_set_active(mutter->audio, active ? true : false) < 0)
	{
		WLog_WARN(TAG, "sound: cannot %s the capture", active ? "start" : "stop");
		return;
	}
	mutter->audioActive = active;
	if (!active)
		Stream_ResetPosition(mutter->audioStream);
	WLog_DBG(TAG, "sound: capture %s", active ? "started" : "stopped");
}

/* ------------------------------------------------------------------------ */
/* libei                                                                     */
/* ------------------------------------------------------------------------ */

/* under eiLock */
static void mutter_process_ei_events(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	struct ei_event* e = nullptr;

	ei_dispatch(mutter->ei);
	while ((e = ei_get_event(mutter->ei)) != nullptr)
	{
		struct ei_device* dev = ei_event_get_device(e);

		switch (ei_event_get_type(e))
		{
			case EI_EVENT_SEAT_ADDED:
				ei_seat_bind_capabilities(ei_event_get_seat(e), EI_DEVICE_CAP_POINTER_ABSOLUTE,
				                          EI_DEVICE_CAP_KEYBOARD, EI_DEVICE_CAP_BUTTON,
				                          EI_DEVICE_CAP_SCROLL, nullptr);
				break;
			case EI_EVENT_DEVICE_ADDED:
				if (ei_device_has_capability(dev, EI_DEVICE_CAP_KEYBOARD) && !mutter->keyboard)
				{
					mutter->keyboard = ei_device_ref(dev);
					WLog_DBG(TAG, "libei keyboard '%s'", ei_device_get_name(dev));
				}
				if (ei_device_has_capability(dev, EI_DEVICE_CAP_POINTER_ABSOLUTE) &&
				    !mutter->pointer)
				{
					mutter->pointer = ei_device_ref(dev);
					WLog_DBG(TAG, "libei pointer '%s'", ei_device_get_name(dev));
				}
				break;
			case EI_EVENT_DEVICE_RESUMED:
				ei_device_start_emulating(dev, ++mutter->sequence);
				if (dev == mutter->keyboard)
					mutter->keyboardReady = TRUE;
				if (dev == mutter->pointer)
					mutter->pointerReady = TRUE;
				break;
			case EI_EVENT_DEVICE_PAUSED:
				if (dev == mutter->keyboard)
					mutter->keyboardReady = FALSE;
				if (dev == mutter->pointer)
					mutter->pointerReady = FALSE;
				break;
			case EI_EVENT_DEVICE_REMOVED:
				if (dev == mutter->keyboard)
				{
					mutter->keyboard = ei_device_unref(mutter->keyboard);
					mutter->keyboardReady = FALSE;
				}
				if (dev == mutter->pointer)
				{
					mutter->pointer = ei_device_unref(mutter->pointer);
					mutter->pointerReady = FALSE;
				}
				break;
			case EI_EVENT_KEYBOARD_MODIFIERS:
				mutter->lockedMods = ei_event_keyboard_get_xkb_mods_locked(e);
				mutter->lockedModsKnown = TRUE;
				WLog_DBG(TAG, "keyboard: locked modifiers 0x%08" PRIx32, mutter->lockedMods);
				break;
			case EI_EVENT_DISCONNECT:
				WLog_WARN(TAG, "libei disconnected: the clients can only watch");
				break;
			default:
				break;
		}
		ei_event_unref(e);
	}
}

/* under eiLock */
static void mutter_key(mutterShadowSubsystem* mutter, uint32_t code, BOOL down)
{
	WINPR_ASSERT(mutter);

	if (!mutter->keyboard || !mutter->keyboardReady)
		return;
	/* Mutter keeps the modifier state itself, as for a real keyboard */
	ei_device_keyboard_key(mutter->keyboard, code, down ? true : false);
	ei_device_frame(mutter->keyboard, ei_now(mutter->ei));

	/* Mutter reports the new locks later, through libei: until then, follow the
	 * lock keys pressed here, or a Synchronize that comes first (clients send one
	 * on each focus) would tap them again and undo them */
	if (down && (code == KEY_CAPSLOCK))
		mutter->lockedMods ^= MUTTER_MOD_LOCK;
	if (down && (code == KEY_NUMLOCK))
		mutter->lockedMods ^= MUTTER_MOD_NUM;
}

WINPR_ATTR_NODISCARD
static BOOL mutter_shadow_input_synchronize_event(rdpShadowSubsystem* subsystem,
                                                  rdpShadowClient* client, UINT32 flags)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter || !client)
		return FALSE;

	EnterCriticalSection(&mutter->eiLock);
	WLog_DBG(TAG, "synchronize: client caps %d num %d, session %s 0x%08" PRIx32,
	         (flags & KBD_SYNC_CAPS_LOCK) ? 1 : 0, (flags & KBD_SYNC_NUM_LOCK) ? 1 : 0,
	         mutter->lockedModsKnown ? "locked" : "unknown", mutter->lockedMods);
	if (mutter->ei && mutter->lockedModsKnown)
	{
		/* the client's lock keys win: tap each one that differs */
		const BOOL caps = (flags & KBD_SYNC_CAPS_LOCK) != 0;
		const BOOL num = (flags & KBD_SYNC_NUM_LOCK) != 0;
		if (caps != ((mutter->lockedMods & MUTTER_MOD_LOCK) != 0))
		{
			mutter_key(mutter, KEY_CAPSLOCK, TRUE);
			mutter_key(mutter, KEY_CAPSLOCK, FALSE);
		}
		if (num != ((mutter->lockedMods & MUTTER_MOD_NUM) != 0))
		{
			mutter_key(mutter, KEY_NUMLOCK, TRUE);
			mutter_key(mutter, KEY_NUMLOCK, FALSE);
		}
	}
	LeaveCriticalSection(&mutter->eiLock);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL mutter_shadow_input_keyboard_event(rdpShadowSubsystem* subsystem,
                                               rdpShadowClient* client, UINT16 flags, UINT8 code)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter || !client)
		return FALSE;

	DWORD scancode = code;
	if (flags & KBD_FLAGS_EXTENDED)
		scancode |= KBDEXT;

	DWORD vkcode = GetVirtualKeyCodeFromVirtualScanCode(scancode, WINPR_KBD_TYPE_IBM_ENHANCED);
	if (flags & KBD_FLAGS_EXTENDED)
		vkcode |= KBDEXT;
	/* WinPR's evdev table is indexed by the Linux input codes libei takes */
	const DWORD keycode = GetKeycodeFromVirtualKeyCode(vkcode, WINPR_KEYCODE_TYPE_EVDEV);
	if (keycode == 0)
	{
		WLog_DBG(TAG, "no key for scancode 0x%04" PRIx32, scancode);
		return TRUE;
	}

	/* a key is pressed unless the release flag is set (KBD_FLAGS_DOWN marks a repeat) */
	const BOOL down = (flags & KBD_FLAGS_RELEASE) == 0;

	EnterCriticalSection(&mutter->eiLock);
	if (mutter->ei)
		mutter_key(mutter, keycode, down);
	LeaveCriticalSection(&mutter->eiLock);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL mutter_shadow_input_unicode_keyboard_event(rdpShadowSubsystem* subsystem,
                                                       rdpShadowClient* client, UINT16 flags,
                                                       UINT16 code)
{
	if (!subsystem || !client)
		return FALSE;

	/* libei sends key codes only; typing a character needs a key in the keymap */
	WLog_DBG(TAG, "unicode key U+%04" PRIX16 " (flags 0x%04" PRIx16 ") not supported", code, flags);
	return TRUE;
}

/* under eiLock: the client's surface position into Mutter's layout, in the
 * libei region of the shared monitor */
static void mutter_pointer_move(mutterShadowSubsystem* mutter, UINT16 x, UINT16 y)
{
	WINPR_ASSERT(mutter);

	struct ei_region* region = nullptr;
	struct ei_region* first = nullptr;

	for (size_t index = 0;; index++)
	{
		struct ei_region* r = ei_device_get_region(mutter->pointer, index);
		if (!r)
			break;
		if (!first)
			first = r;
		const char* id = ei_region_get_mapping_id(r);
		if (mutter->mappingId && id && (strcmp(id, mutter->mappingId) == 0))
		{
			region = r;
			break;
		}
	}
	if (!region)
		region = first;

	mutter->pointerInput = GetTickCount64();
	double px = x;
	double py = y;
	if (region && (mutter->width > 0) && (mutter->height > 0))
	{
		px = ei_region_get_x(region) + (double)x * ei_region_get_width(region) / mutter->width;
		py = ei_region_get_y(region) + (double)y * ei_region_get_height(region) / mutter->height;
	}
	WLog_VRB(TAG,
	         "pointer %" PRIu16 ",%" PRIu16 " -> %.1f,%.1f (region %s %" PRIu32 ",%" PRIu32
	         " %" PRIu32 "x%" PRIu32 ")",
	         x, y, px, py,
	         region ? (ei_region_get_mapping_id(region) ? ei_region_get_mapping_id(region) : "-")
	                : "none",
	         region ? ei_region_get_x(region) : 0, region ? ei_region_get_y(region) : 0,
	         region ? ei_region_get_width(region) : 0, region ? ei_region_get_height(region) : 0);
	ei_device_pointer_motion_absolute(mutter->pointer, px, py);
	ei_device_frame(mutter->pointer, ei_now(mutter->ei));
}

/* under eiLock */
static void mutter_pointer_button(mutterShadowSubsystem* mutter, uint32_t button, BOOL down)
{
	WINPR_ASSERT(mutter);

	if (!ei_device_has_capability(mutter->pointer, EI_DEVICE_CAP_BUTTON))
		return;
	ei_device_button_button(mutter->pointer, button, down ? true : false);
	ei_device_frame(mutter->pointer, ei_now(mutter->ei));
}

/* under eiLock; 120 is one notch, as in RDP */
static void mutter_pointer_wheel(mutterShadowSubsystem* mutter, UINT16 flags, BOOL horizontal)
{
	WINPR_ASSERT(mutter);

	if (!ei_device_has_capability(mutter->pointer, EI_DEVICE_CAP_SCROLL))
		return;

	INT32 value = flags & 0xFF;
	if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
		value -= 0x100;

	/* RDP: positive turns away from the user (up) or to the right; libei:
	 * positive scrolls down or to the right */
	if (horizontal)
		ei_device_scroll_discrete(mutter->pointer, value, 0);
	else
		ei_device_scroll_discrete(mutter->pointer, 0, -value);
	ei_device_frame(mutter->pointer, ei_now(mutter->ei));
}

WINPR_ATTR_NODISCARD
static BOOL mutter_shadow_input_mouse_event(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                            UINT16 flags, UINT16 x, UINT16 y)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter || !client)
		return FALSE;

	EnterCriticalSection(&mutter->eiLock);
	if (mutter->ei && mutter->pointer && mutter->pointerReady)
	{
		if (flags & PTR_FLAGS_WHEEL)
			mutter_pointer_wheel(mutter, flags, FALSE);
		else if (flags & PTR_FLAGS_HWHEEL)
			mutter_pointer_wheel(mutter, flags, TRUE);
		else
		{
			/* button events carry the position too */
			mutter_pointer_move(mutter, x, y);

			const BOOL down = (flags & PTR_FLAGS_DOWN) != 0;
			if (flags & PTR_FLAGS_BUTTON1)
				mutter_pointer_button(mutter, BTN_LEFT, down);
			else if (flags & PTR_FLAGS_BUTTON2)
				mutter_pointer_button(mutter, BTN_RIGHT, down);
			else if (flags & PTR_FLAGS_BUTTON3)
				mutter_pointer_button(mutter, BTN_MIDDLE, down);
		}
	}
	LeaveCriticalSection(&mutter->eiLock);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL mutter_shadow_input_extended_mouse_event(rdpShadowSubsystem* subsystem,
                                                     rdpShadowClient* client, UINT16 flags,
                                                     UINT16 x, UINT16 y)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter || !client)
		return FALSE;

	EnterCriticalSection(&mutter->eiLock);
	if (mutter->ei && mutter->pointer && mutter->pointerReady)
	{
		mutter_pointer_move(mutter, x, y);

		const BOOL down = (flags & PTR_XFLAGS_DOWN) != 0;
		if ((flags & PTR_XFLAGS_BUTTON1) != 0)
			mutter_pointer_button(mutter, BTN_SIDE, down);
		else if ((flags & PTR_XFLAGS_BUTTON2) != 0)
			mutter_pointer_button(mutter, BTN_EXTRA, down);
	}
	LeaveCriticalSection(&mutter->eiLock);
	return TRUE;
}

/* ------------------------------------------------------------------------ */
/* login                                                                     */
/* ------------------------------------------------------------------------ */

#ifdef WITH_PAM
typedef struct
{
	const char* user;
	const char* password;
} MUTTER_SHADOW_PAM_DATA;

WINPR_ATTR_NODISCARD
static int mutter_shadow_pam_conv(int num_msg, const struct pam_message** msg,
                                  struct pam_response** resp, void* appdata_ptr)
{
	const MUTTER_SHADOW_PAM_DATA* data = (const MUTTER_SHADOW_PAM_DATA*)appdata_ptr;

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

/* The session shared belongs to the user running the server: only that user
 * may log in, with the password of their account. */
WINPR_ATTR_NODISCARD
static int mutter_shadow_pam_authenticate(WINPR_ATTR_UNUSED rdpShadowSubsystem* subsystem,
                                          WINPR_ATTR_UNUSED rdpShadowClient* client,
                                          const char* user, WINPR_ATTR_UNUSED const char* domain,
                                          const char* password)
{
	if (!user || !password)
		return -1;

	struct passwd pbuffer = WINPR_C_ARRAY_INIT;
	struct passwd* owner = nullptr;

	/* -1 means no fixed limit: the usual size */
	long buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (buflen <= 0)
		buflen = 16384;
	char* buf = calloc((size_t)buflen, sizeof(char));
	if (!buf)
	{
		WLog_ERR(TAG, "login as '%s' refused: no memory for the session's account", user);
		return -1;
	}
	const int rc = getpwuid_r(getuid(), &pbuffer, buf, (size_t)buflen, &owner);
	if (rc != 0)
	{
		char msg[128] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "login as '%s' refused: the session's account: %s", user,
		         winpr_strerror(rc, msg, sizeof(msg)));
		free(buf);
		return -1;
	}
	if (!owner || !owner->pw_name || (strcmp(owner->pw_name, user) != 0))
	{
		WLog_ERR(TAG, "login as '%s' refused: this session belongs to '%s'", user,
		         (owner && owner->pw_name) ? owner->pw_name : "?");
		free(buf);
		return -1;
	}

	MUTTER_SHADOW_PAM_DATA data = { .user = user, .password = password };
	const struct pam_conv conv = { .conv = mutter_shadow_pam_conv, .appdata_ptr = &data };
	pam_handle_t* handle = nullptr;

	/* a service of our own if the system has one, else the console login's */
	const char* service =
	    winpr_PathFileExists("/etc/pam.d/freerdp-shadow") ? "freerdp-shadow" : "login";
	int status = pam_start(service, user, &conv, &handle);
	if (status != PAM_SUCCESS)
	{
		WLog_ERR(TAG, "pam_start(%s) failure: %s", service, pam_strerror(handle, status));
		free(buf);
		return -1;
	}

	status = pam_authenticate(handle, 0);
	if (status == PAM_SUCCESS)
		status = pam_acct_mgmt(handle, 0);
	if (status != PAM_SUCCESS)
		WLog_ERR(TAG, "login as '%s' failed: %s", user, pam_strerror(handle, status));

	pam_end(handle, status);
	free(buf);
	return (status == PAM_SUCCESS) ? 1 : -1;
}
#endif

/* ------------------------------------------------------------------------ */
/* the subsystem                                                             */
/* ------------------------------------------------------------------------ */

static void mutter_shadow_refresh(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	/* the next frame goes out whole; Mutter sends one when asked for nothing
	 * else only once something changes, so ask the clients' surface again */
	mutter->fullPaint = TRUE;
	mutter->cursorResend = TRUE;

	/* the picture carries the cursor: the clients hide their own pointer */
	if (!mutter->virtualMonitor)
	{
		static const BYTE clear[4] = { 0 };
		mutter_cursor_send_shape(mutter, clear, 1, 1, 0, 0);
	}
	else if (mutter->cursorPixels)
		mutter_cursor_send_shape(mutter, mutter->cursorPixels, mutter->cursorWidth,
		                         mutter->cursorHeight, mutter->cursorXHot, mutter->cursorYHot);
	else
		mutter_cursor_send_default(mutter);

	rdpShadowServer* server = mutter->common.server;
	rdpShadowSurface* surface = server ? server->surface : nullptr;
	if (!surface)
		return;

	EnterCriticalSection(&surface->lock);
	const RECTANGLE_16 all = { 0, 0, (UINT16)surface->width, (UINT16)surface->height };
	if (!region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &all))
		WLog_ERR(TAG, "cannot mark the screen changed");
	LeaveCriticalSection(&surface->lock);

	/* not under the clients list lock, as in mutter_on_process */
	if (ArrayList_Count(server->clients) > 0)
		shadow_subsystem_frame_update(&mutter->common);

	EnterCriticalSection(&surface->lock);
	region16_clear(&surface->invalidRegion);
	LeaveCriticalSection(&surface->lock);
}

static void mutter_shadow_process_message(mutterShadowSubsystem* mutter, wMessage* message)
{
	WINPR_ASSERT(mutter);
	WINPR_ASSERT(message);

	switch (message->id)
	{
		case SHADOW_MSG_IN_REFRESH_REQUEST_ID:
			mutter_shadow_refresh(mutter);
			break;

		default:
			WLog_ERR(TAG, "Unknown message id: %" PRIu32 "", message->id);
			break;
	}

	if (message->Free)
		message->Free(message);
}

/* Mutter ended the stream: record the same monitor again in the running
 * session. Its node comes with PipeWireStreamAdded (see mutter_reconnect). */
static void mutter_record_again(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	mutter->streamGone = FALSE;
	const char* connector = mutter->connectors[mutter->common.selectedMonitor];
	WLog_INFO(TAG, "Mutter ended the stream of %s (its layout changed): recording it again",
	          connector[0] ? connector : "the primary monitor");

	mutter->streamAddedSlot = sd_bus_slot_unref(mutter->streamAddedSlot);
	free(mutter->streamPath);
	mutter->streamPath = nullptr;

	sd_bus_error err = SD_BUS_ERROR_NULL;
	int started = -1;
	if (mutter_record_monitor(mutter, connector))
	{
		mutter->awaitNode = TRUE;
		started = sd_bus_call_method(mutter->bus, SC_NAME, mutter->streamPath, SC_STREAM_IFACE,
		                             "Start", &err, nullptr, "");
		if (started < 0)
		{
			char buffer[128] = WINPR_C_ARRAY_INIT;
			WLog_ERR(TAG, "ScreenCast.Stream.Start failed: %s",
			         err.message ? err.message : winpr_strerror(-started, buffer, sizeof(buffer)));
		}
	}
	sd_bus_error_free(&err);

	if (started < 0)
	{
		/* the session closed meanwhile, or the monitor is gone (unplugged, or
		 * disabled): nothing left to share either way */
		mutter->awaitNode = FALSE;
		while (sd_bus_process(mutter->bus, nullptr) > 0)
		{
		}
		if (!mutter->closed)
			mutter->monitorGone = TRUE;
		mutter->closed = TRUE;
	}
}

/* The new node is there: the same PipeWire stream connects to it */
static void mutter_reconnect(mutterShadowSubsystem* mutter)
{
	WINPR_ASSERT(mutter);

	mutter->reconnect = FALSE;
	if (!mutter_stream_connect_node(mutter))
	{
		mutter->monitorGone = TRUE;
		mutter->closed = TRUE;
		return;
	}
	mutter_read_stream_parameters(mutter);
	mutter->fullPaint = TRUE;
	WLog_INFO(TAG, "sharing again from PipeWire node %" PRIu32, mutter->nodeId);
}

WINPR_ATTR_NODISCARD
static DWORD WINAPI mutter_shadow_subsystem_thread(LPVOID arg)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)arg;
	WINPR_ASSERT(mutter);

	wMessagePipe* MsgPipe = mutter->common.MsgPipe;
	HANDLE events[6] = WINPR_C_ARRAY_INIT;
	DWORD count = 0;

	HANDLE busEvent = CreateFileDescriptorEventA(nullptr, FALSE, FALSE, sd_bus_get_fd(mutter->bus),
	                                             WINPR_FD_READ);
	HANDLE pwEvent = CreateFileDescriptorEventA(nullptr, FALSE, FALSE, pw_loop_get_fd(mutter->loop),
	                                            WINPR_FD_READ);
	HANDLE eiEvent = mutter->ei ? CreateFileDescriptorEventA(nullptr, FALSE, FALSE,
	                                                         ei_get_fd(mutter->ei), WINPR_FD_READ)
	                            : nullptr;
	if (!busEvent || !pwEvent || (mutter->ei && !eiEvent))
	{
		WLog_ERR(TAG, "cannot wait on the session's connections");
		goto out;
	}

	events[count++] = mutter->stopEvent;
	events[count++] = MessageQueue_Event(MsgPipe->In);
	events[count++] = mutter->clientsEvent;
	events[count++] = busEvent;
	events[count++] = pwEvent;
	if (eiEvent)
		events[count++] = eiEvent;

	while (1)
	{
		/* sd-bus may want to run before its fd is readable (a queued message, a
		 * call's deadline): its timeout is an absolute CLOCK_MONOTONIC time */
		uint64_t usec = UINT64_MAX;
		DWORD timeout = 1000;
		if ((sd_bus_get_timeout(mutter->bus, &usec) >= 0) && (usec != UINT64_MAX))
		{
			struct timespec ts = WINPR_C_ARRAY_INIT;
			(void)clock_gettime(CLOCK_MONOTONIC, &ts);
			const uint64_t now = (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
			const uint64_t waitMs = (usec > now) ? (usec - now + 999) / 1000 : 0;
			timeout = (DWORD)MIN(waitMs, 1000);
		}
		if (mutter->streamGone)
			timeout = MIN(timeout, 50);

		const DWORD status = WaitForMultipleObjects(count, events, FALSE, timeout);
		if ((status == WAIT_FAILED) || (WaitForSingleObject(mutter->stopEvent, 0) == WAIT_OBJECT_0))
			break;

		while (sd_bus_process(mutter->bus, nullptr) > 0)
		{
		}
		(void)sd_bus_flush(mutter->bus);
		if (mutter->closed && !mutter->closedLogged)
		{
			/* The user stopped the sharing (GNOME's screen sharing indicator, or anything
			 * that inhibits remote access), or GNOME Shell is gone. The server ends with
			 * success, so a service restarted on failure does not share the screen again
			 * against the user's choice. */
			if (mutter->shellGone)
				WLog_WARN(TAG, "GNOME Shell left the session: the server ends");
			else if (mutter->monitorGone)
				WLog_WARN(TAG, "the shared monitor is gone: the server ends");
			else
				WLog_WARN(TAG, "the GNOME session stopped the sharing (its user did, or GNOME "
				               "Shell is ending): the server ends");
			mutter->closedLogged = TRUE;
			if (mutter->common.server)
				(void)SetEvent(mutter->common.server->StopEvent);
		}

		if (WaitForSingleObject(mutter->clientsEvent, 0) == WAIT_OBJECT_0)
		{
			/* reset before reading the count: a change after this sets it again */
			(void)ResetEvent(mutter->clientsEvent);
			mutter_audio_follow_clients(mutter);
		}

		pw_loop_enter(mutter->loop);
		(void)pw_loop_iterate(mutter->loop, 0);
		pw_loop_leave(mutter->loop);

		/* a Closed that follows the node's end (the user stopped the sharing)
		 * wins over recording the monitor again */
		if (mutter->streamGone && !mutter->closed &&
		    (GetTickCount64() - mutter->streamGoneAt >= MUTTER_STREAM_GONE_GRACE_MS))
			mutter_record_again(mutter);
		if (mutter->reconnect && !mutter->closed)
			mutter_reconnect(mutter);

		if (mutter->ei)
		{
			EnterCriticalSection(&mutter->eiLock);
			mutter_process_ei_events(mutter);
			LeaveCriticalSection(&mutter->eiLock);
		}

		wMessage message = WINPR_C_ARRAY_INIT;
		while (MessageQueue_Peek(MsgPipe->In, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				goto out;
			mutter_shadow_process_message(mutter, &message);
		}
	}

out:
	if (busEvent)
		(void)CloseHandle(busEvent);
	if (pwEvent)
		(void)CloseHandle(pwEvent);
	if (eiEvent)
		(void)CloseHandle(eiEvent);
	ExitThread(0);
	return 0;
}

WINPR_ATTR_NODISCARD
static int mutter_shadow_subsystem_init(rdpShadowSubsystem* subsystem)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	WINPR_ASSERT(mutter);

	const int opened = sd_bus_open_user(&mutter->bus);
	if (opened < 0)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG,
		         "cannot connect to the session bus: %s (start the server inside the GNOME "
		         "session, as its user)",
		         winpr_strerror(-opened, buffer, sizeof(buffer)));
		return -1;
	}

	if (!mutter_wait_for_name(mutter->bus, RD_NAME, MUTTER_NAME_TIMEOUT_MS))
	{
		WLog_ERR(TAG, "%s is not on the session bus: is this a GNOME session?", RD_NAME);
		return -1;
	}

	UINT32 count = 0;
	mutter->virtualMonitor = mutter_virtual_size(&mutter->virtualWidth, &mutter->virtualHeight);
	{
		/* on unless "0" */
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		const char* check = getenv(MUTTER_DAMAGE_CHECK_ENV);
		mutter->damageCheck = !check || (strcmp(check, "0") != 0);
		if (!mutter->damageCheck)
			WLog_INFO(TAG, "the check for changes Mutter left out of its damage is off");
	}
	if (mutter->virtualMonitor)
	{
		/* one monitor, this server's own */
		subsystem->monitors[0] = (MONITOR_DEF){ 0, 0, (INT32)mutter->virtualWidth - 1,
			                                    (INT32)mutter->virtualHeight - 1, 1 };
		mutter->connectors[0][0] = '\0';
		count = 1;
	}
	else
	{
		WLog_DBG(TAG, "DisplayConfig.GetCurrentState");
		count = mutter_list_monitors(mutter->bus, subsystem->monitors, mutter->connectors,
		                             MUTTER_SHADOW_MAX_MONITORS);
	}
	if (count == 0)
	{
		/* no layout: the primary monitor, sized by its stream */
		WLog_WARN(TAG, "no monitor layout from Mutter: sharing the primary monitor");
		subsystem->monitors[0] = (MONITOR_DEF){ 0, 0, 0, 0, 1 };
		mutter->connectors[0][0] = '\0';
		count = 1;
	}
	subsystem->numMonitors = count;
	if (subsystem->selectedMonitor >= count)
		subsystem->selectedMonitor = 0;

	const char* connector = mutter->connectors[subsystem->selectedMonitor];
	if (!mutter_setup_sessions(mutter, connector))
		return -1;
	if (!mutter_connect_stream(mutter))
		return -1;

	mutter_set_monitor_size(mutter);

	if (!mutter_audio_init(mutter))
		WLog_WARN(TAG, "sound: no capture of the default sink: no sound");

	/* mirrored monitors carry the cursor in their frames and the clients get a
	 * transparent shape; a virtual monitor sends the cursor's shapes. Either way
	 * shapes go out, which needs ShowMouseCursor (otherwise only set by
	 * /server-side-cursor) */
	if (subsystem->server)
		subsystem->server->ShowMouseCursor = TRUE;

	if (mutter->virtualMonitor)
		WLog_INFO(TAG, "sharing a virtual monitor of its own: %" PRIu32 "x%" PRIu32 ", input %s",
		          mutter->width, mutter->height, mutter->ei ? "through libei" : "off");
	else
		WLog_INFO(TAG,
		          "sharing monitor %s (%" PRIu32 " of %" PRIu32 "): %" PRIu32 "x%" PRIu32
		          ", input %s",
		          connector[0] ? connector : "(primary)", subsystem->selectedMonitor, count,
		          mutter->width, mutter->height, mutter->ei ? "through libei" : "off");
	return 1;
}

WINPR_ATTR_NODISCARD
static int mutter_shadow_subsystem_uninit(rdpShadowSubsystem* subsystem)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter)
		return -1;

	mutter_audio_uninit(mutter);
	if (mutter->stream)
		pw_stream_destroy(mutter->stream);
	mutter->stream = nullptr;
	if (mutter->registry)
	{
		spa_hook_remove(&mutter->registryListener);
		pw_proxy_destroy((struct pw_proxy*)mutter->registry);
	}
	mutter->registry = nullptr;
	if (mutter->core)
		pw_core_disconnect(mutter->core);
	mutter->core = nullptr;
	if (mutter->context)
		pw_context_destroy(mutter->context);
	mutter->context = nullptr;
	if (mutter->loop)
		pw_loop_destroy(mutter->loop);
	mutter->loop = nullptr;

	EnterCriticalSection(&mutter->eiLock);
	mutter->keyboard = ei_device_unref(mutter->keyboard);
	mutter->pointer = ei_device_unref(mutter->pointer);
	mutter->ei = ei_unref(mutter->ei);
	mutter->keyboardReady = FALSE;
	mutter->pointerReady = FALSE;
	LeaveCriticalSection(&mutter->eiLock);

	if (mutter->bus)
	{
		if (mutter->started && !mutter->closed && mutter->rdSessionPath)
			(void)sd_bus_call_method(mutter->bus, RD_NAME, mutter->rdSessionPath, RD_SESSION_IFACE,
			                         "Stop", nullptr, nullptr, "");
		mutter->streamAddedSlot = sd_bus_slot_unref(mutter->streamAddedSlot);
		mutter->closedSlot = sd_bus_slot_unref(mutter->closedSlot);
		mutter->ownerSlot = sd_bus_slot_unref(mutter->ownerSlot);
		mutter->bus = sd_bus_flush_close_unref(mutter->bus);
	}
	mutter->started = FALSE;

	free(mutter->rdSessionPath);
	mutter->rdSessionPath = nullptr;
	free(mutter->scSessionPath);
	mutter->scSessionPath = nullptr;
	free(mutter->streamPath);
	mutter->streamPath = nullptr;
	free(mutter->mappingId);
	mutter->mappingId = nullptr;
	return 1;
}

WINPR_ATTR_NODISCARD
static int mutter_shadow_subsystem_start(rdpShadowSubsystem* subsystem)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter || !mutter->bus || !mutter->loop)
		return -1;

	(void)ResetEvent(mutter->stopEvent);
	mutter->thread = CreateThread(nullptr, 0, mutter_shadow_subsystem_thread, mutter, 0, nullptr);
	if (!mutter->thread)
	{
		WLog_ERR(TAG, "Failed to create thread");
		return -1;
	}
	return 1;
}

WINPR_ATTR_NODISCARD
static int mutter_shadow_subsystem_stop(rdpShadowSubsystem* subsystem)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter)
		return -1;

	if (mutter->thread)
	{
		(void)SetEvent(mutter->stopEvent);
		(void)WaitForSingleObject(mutter->thread, INFINITE);
		(void)CloseHandle(mutter->thread);
		mutter->thread = nullptr;
	}
	return 1;
}

WINPR_ATTR_NODISCARD
static BOOL mutter_shadow_client_connect(rdpShadowSubsystem* subsystem,
                                         WINPR_ATTR_UNUSED rdpShadowClient* client)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	WINPR_ASSERT(mutter);

	/* a new client gets the cursor shape with the next one seen */
	mutter->cursorResend = TRUE;
	(void)InterlockedIncrement(&mutter->clients);
	(void)SetEvent(mutter->clientsEvent);
	return TRUE;
}

static void mutter_shadow_client_disconnect(rdpShadowSubsystem* subsystem,
                                            WINPR_ATTR_UNUSED rdpShadowClient* client)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	WINPR_ASSERT(mutter);

	(void)InterlockedDecrement(&mutter->clients);
	(void)SetEvent(mutter->clientsEvent);
}

static void mutter_shadow_subsystem_free(rdpShadowSubsystem* subsystem)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter)
		return;

	if (mutter_shadow_subsystem_stop(subsystem) < 0)
		WLog_WARN(TAG, "the subsystem thread did not stop");
	if (mutter_shadow_subsystem_uninit(subsystem) < 0)
		WLog_WARN(TAG, "the subsystem was not uninitialized");
	DeleteCriticalSection(&mutter->eiLock);
	if (mutter->stopEvent)
		(void)CloseHandle(mutter->stopEvent);
	if (mutter->clientsEvent)
		(void)CloseHandle(mutter->clientsEvent);
	free(mutter->cursorPixels);
	free(mutter);
}

WINPR_ATTR_NODISCARD
static rdpShadowSubsystem* mutter_shadow_subsystem_new(void)
{
	mutterShadowSubsystem* mutter = calloc(1, sizeof(mutterShadowSubsystem));
	if (!mutter)
		return nullptr;

	if (!InitializeCriticalSectionAndSpinCount(&mutter->eiLock, 4000))
	{
		free(mutter);
		return nullptr;
	}

	mutter->stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	/* manual reset: WinPR has no auto-reset events (it creates a manual-reset one and
	 * logs an error), so the thread resets it */
	mutter->clientsEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!mutter->stopEvent || !mutter->clientsEvent)
	{
		mutter_shadow_subsystem_free(&mutter->common);
		return nullptr;
	}

	mutter->cursorX = -1;
	mutter->cursorY = -1;
	mutter->audioSourceFormat.wFormatTag = WAVE_FORMAT_PCM;
	mutter->audioSourceFormat.nChannels = MUTTER_AUDIO_CHANNELS;
	mutter->audioSourceFormat.nSamplesPerSec = MUTTER_AUDIO_RATE;
	mutter->audioSourceFormat.nAvgBytesPerSec = MUTTER_AUDIO_RATE * MUTTER_AUDIO_FRAME_BYTES;
	mutter->audioSourceFormat.nBlockAlign = MUTTER_AUDIO_FRAME_BYTES;
	mutter->audioSourceFormat.wBitsPerSample = 16;
	mutter->common.SynchronizeEvent = mutter_shadow_input_synchronize_event;
	mutter->common.KeyboardEvent = mutter_shadow_input_keyboard_event;
	mutter->common.UnicodeKeyboardEvent = mutter_shadow_input_unicode_keyboard_event;
	mutter->common.MouseEvent = mutter_shadow_input_mouse_event;
	mutter->common.ExtendedMouseEvent = mutter_shadow_input_extended_mouse_event;
	mutter->common.ClientConnect = mutter_shadow_client_connect;
	mutter->common.ClientDisconnect = mutter_shadow_client_disconnect;
#ifdef WITH_PAM
	mutter->common.Authenticate = mutter_shadow_pam_authenticate;
#endif
	return &mutter->common;
}

FREERDP_ENTRY_POINT(WINPR_ATTR_NODISCARD const char* MutterShadowSubsystemName(void))
{
	return "Mutter";
}

FREERDP_ENTRY_POINT(
    WINPR_ATTR_NODISCARD int MutterShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints))
{
	WINPR_ASSERT(pEntryPoints);

	pEntryPoints->New = mutter_shadow_subsystem_new;
	pEntryPoints->Free = mutter_shadow_subsystem_free;
	pEntryPoints->Init = mutter_shadow_subsystem_init;
	pEntryPoints->Uninit = mutter_shadow_subsystem_uninit;
	pEntryPoints->Start = mutter_shadow_subsystem_start;
	pEntryPoints->Stop = mutter_shadow_subsystem_stop;
	pEntryPoints->EnumMonitors = mutter_shadow_enum_monitors;
	return 1;
}

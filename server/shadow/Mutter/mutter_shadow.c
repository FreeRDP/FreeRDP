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
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <linux/input-event-codes.h>

#include <spa/param/video/format-utils.h>
#include <spa/param/buffers.h>
#include <spa/buffer/meta.h>

#include <winpr/crt.h>
#include <winpr/collections.h>
#include <winpr/input.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/server/server-common.h>
#include <freerdp/codec/color.h>
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
#define MUTTER_CURSOR_MODE_METADATA 2

/* cursor metadata: position, hot spot and, when the shape changes, a bitmap;
 * Mutter's bitmaps are up to 384x384 */
#define MUTTER_CURSOR_META_SIZE(width, height) \
	(sizeof(struct spa_meta_cursor) + sizeof(struct spa_meta_bitmap) + (width) * (height) * 4)
#define MUTTER_CURSOR_META_MAX 384

/* the largest pointer sent to the clients */
#define MUTTER_SHADOW_CURSOR_MAX 96

/* how long after a client's own pointer motion the server leaves the pointer
 * position to that client */
#define MUTTER_SHADOW_CURSOR_QUIET_MS 500

/* damage rectangles taken from one buffer */
#define MUTTER_MAX_DAMAGE 16

/* how long Mutter may take to offer its API, announce the stream, and give
 * the stream a format */
#define MUTTER_NAME_TIMEOUT_MS 10000
#define MUTTER_STREAM_TIMEOUT_MS 5000
#define MUTTER_FORMAT_TIMEOUT_MS 5000

/* xkbcommon's core modifiers: Lock is Caps Lock, Mod2 is Num Lock in every
 * stock keymap */
#define MUTTER_MOD_LOCK (1u << 1)
#define MUTTER_MOD_NUM (1u << 4)

/* ------------------------------------------------------------------------ */
/* D-Bus                                                                     */
/* ------------------------------------------------------------------------ */

static BOOL mutter_wait_for_name(sd_bus* bus, const char* name, UINT64 timeoutMs)
{
	const UINT64 start = GetTickCount64();
	BOOL logged = FALSE;

	for (;;)
	{
		sd_bus_error err = SD_BUS_ERROR_NULL;
		sd_bus_message* reply = nullptr;
		int hasOwner = 0;

		const int r =
		    sd_bus_call_method(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
		                       "org.freedesktop.DBus", "NameHasOwner", &err, &reply, "s", name);
		if ((r >= 0) && (sd_bus_message_read(reply, "b", &hasOwner) >= 0) && hasOwner)
		{
			sd_bus_message_unref(reply);
			return TRUE;
		}
		sd_bus_message_unref(reply);
		sd_bus_error_free(&err);

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
static BOOL mutter_call_for_path(sd_bus* bus, sd_bus_message* call, const char* what, char** out)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = nullptr;
	const char* path = nullptr;

	int r = sd_bus_call(bus, call, 0, &err, &reply);
	if (r < 0)
	{
		WLog_ERR(TAG, "%s failed: %s", what, err.message ? err.message : strerror(-r));
		sd_bus_error_free(&err);
		return FALSE;
	}

	r = sd_bus_message_read(reply, "o", &path);
	if ((r >= 0) && path)
		*out = _strdup(path);
	sd_bus_message_unref(reply);
	return (r >= 0) && *out;
}

typedef struct
{
	char connector[MUTTER_SHADOW_CONNECTOR_SIZE];
	INT32 width;
	INT32 height;
} MUTTER_MODE;

/* Reads one physical monitor, ((ssss)a(siiddada{sv})a{sv}): its connector
 * and the size of its current mode */
static int mutter_read_monitor(sd_bus_message* reply, MUTTER_MODE* mode)
{
	const char* connector = nullptr;
	int r = sd_bus_message_enter_container(reply, 'r', "(ssss)a(siiddada{sv})a{sv}");
	if (r <= 0)
		return r;

	r = sd_bus_message_read(reply, "(ssss)", &connector, nullptr, nullptr, nullptr);
	if (r < 0)
		return r;
	(void)_snprintf(mode->connector, sizeof(mode->connector), "%s", connector ? connector : "");

	r = sd_bus_message_enter_container(reply, 'a', "(siiddada{sv})");
	if (r < 0)
		return r;
	while ((r = sd_bus_message_enter_container(reply, 'r', "siiddada{sv}")) > 0)
	{
		int32_t width = 0;
		int32_t height = 0;
		r = sd_bus_message_read(reply, "siidd", nullptr, &width, &height, nullptr, nullptr);
		if (r < 0)
			return r;
		r = sd_bus_message_skip(reply, "ad");
		if (r < 0)
			return r;

		int current = 0;
		r = sd_bus_message_enter_container(reply, 'a', "{sv}");
		if (r < 0)
			return r;
		while ((r = sd_bus_message_enter_container(reply, 'e', "sv")) > 0)
		{
			const char* key = nullptr;
			r = sd_bus_message_read(reply, "s", &key);
			if (r < 0)
				return r;
			if (key && (strcmp(key, "is-current") == 0))
				r = sd_bus_message_read(reply, "v", "b", &current);
			else
				r = sd_bus_message_skip(reply, "v");
			if (r < 0)
				return r;
			r = sd_bus_message_exit_container(reply);
			if (r < 0)
				return r;
		}
		if (r < 0)
			return r;
		r = sd_bus_message_exit_container(reply); /* a{sv} */
		if (r < 0)
			return r;
		r = sd_bus_message_exit_container(reply); /* the mode */
		if (r < 0)
			return r;

		if (current)
		{
			mode->width = width;
			mode->height = height;
		}
	}
	if (r < 0)
		return r;
	r = sd_bus_message_exit_container(reply); /* the modes */
	if (r < 0)
		return r;
	r = sd_bus_message_skip(reply, "a{sv}");
	if (r < 0)
		return r;
	r = sd_bus_message_exit_container(reply);
	return (r < 0) ? r : 1;
}

/* The logical monitors, in Mutter's layout, primary first. connectors gets
 * each one's (first) connector. Returns the count. */
static UINT32 mutter_list_monitors(sd_bus* bus, MONITOR_DEF* monitors,
                                   char (*connectors)[MUTTER_SHADOW_CONNECTOR_SIZE], UINT32 max)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = nullptr;
	MUTTER_MODE modes[MUTTER_SHADOW_MAX_MONITORS] = WINPR_C_ARRAY_INIT;
	size_t nmodes = 0;
	UINT32 count = 0;
	uint32_t layoutMode = 1; /* logical: sizes are divided by the scale */

	int r = sd_bus_call_method(bus, DC_NAME, DC_PATH, DC_NAME, "GetCurrentState", &err, &reply, "");
	if (r < 0)
	{
		WLog_WARN(TAG, "GetCurrentState failed: %s", err.message ? err.message : strerror(-r));
		sd_bus_error_free(&err);
		return 0;
	}

	struct
	{
		INT32 x;
		INT32 y;
		double scale;
		uint32_t transform;
		int primary;
		char connector[MUTTER_SHADOW_CONNECTOR_SIZE];
	} logical[MUTTER_SHADOW_MAX_MONITORS] = WINPR_C_ARRAY_INIT;
	size_t nlogical = 0;

	r = sd_bus_message_read(reply, "u", nullptr);
	if (r >= 0)
		r = sd_bus_message_enter_container(reply, 'a', "((ssss)a(siiddada{sv})a{sv})");
	while (r >= 0)
	{
		MUTTER_MODE mode = WINPR_C_ARRAY_INIT;
		r = mutter_read_monitor(reply, &mode);
		if (r <= 0)
			break;
		if (nmodes < ARRAYSIZE(modes))
			modes[nmodes++] = mode;
	}
	if (r >= 0)
		r = sd_bus_message_exit_container(reply);

	if (r >= 0)
		r = sd_bus_message_enter_container(reply, 'a', "(iiduba(ssss)a{sv})");
	while (r >= 0)
	{
		r = sd_bus_message_enter_container(reply, 'r', "iiduba(ssss)a{sv}");
		if (r <= 0)
			break;

		int32_t x = 0;
		int32_t y = 0;
		double scale = 1.0;
		uint32_t transform = 0;
		int primary = 0;
		r = sd_bus_message_read(reply, "iidub", &x, &y, &scale, &transform, &primary);
		if (r < 0)
			break;

		const char* connector = nullptr;
		r = sd_bus_message_enter_container(reply, 'a', "(ssss)");
		if (r < 0)
			break;
		while ((r = sd_bus_message_read(reply, "(ssss)", connector ? nullptr : &connector, nullptr,
		                                nullptr, nullptr)) > 0)
		{
		}
		if (r < 0)
			break;
		r = sd_bus_message_exit_container(reply);
		if (r >= 0)
			r = sd_bus_message_skip(reply, "a{sv}");
		if (r >= 0)
			r = sd_bus_message_exit_container(reply);
		if (r < 0)
			break;

		if (nlogical < ARRAYSIZE(logical))
		{
			logical[nlogical].x = x;
			logical[nlogical].y = y;
			logical[nlogical].scale = (scale > 0.0) ? scale : 1.0;
			logical[nlogical].transform = transform;
			logical[nlogical].primary = primary;
			(void)_snprintf(logical[nlogical].connector, sizeof(logical[nlogical].connector), "%s",
			                connector ? connector : "");
			nlogical++;
		}
	}
	if (r >= 0)
		r = sd_bus_message_exit_container(reply);

	if (r >= 0)
		r = sd_bus_message_enter_container(reply, 'a', "{sv}");
	while (r > 0)
	{
		r = sd_bus_message_enter_container(reply, 'e', "sv");
		if (r <= 0)
			break;
		const char* key = nullptr;
		r = sd_bus_message_read(reply, "s", &key);
		if ((r >= 0) && key && (strcmp(key, "layout-mode") == 0))
			r = sd_bus_message_read(reply, "v", "u", &layoutMode);
		else if (r >= 0)
			r = sd_bus_message_skip(reply, "v");
		if (r >= 0)
			r = sd_bus_message_exit_container(reply);
	}
	sd_bus_message_unref(reply);

	if (r < 0)
		WLog_WARN(TAG, "GetCurrentState: unexpected reply: %s", strerror(-r));

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

static UINT32 mutter_shadow_enum_monitors(MONITOR_DEF* monitors, UINT32 maxMonitors)
{
	sd_bus* bus = nullptr;
	const int r = sd_bus_open_user(&bus);
	if (r < 0)
	{
		WLog_WARN(TAG, "cannot connect to the session bus: %s", strerror(-r));
		return 0;
	}

	const UINT32 count = mutter_list_monitors(bus, monitors, nullptr, maxMonitors);
	WLog_DBG(TAG, "%" PRIu32 " monitors", count);
	sd_bus_flush_close_unref(bus);
	return count;
}

static int mutter_on_stream_added(sd_bus_message* msg, void* data,
                                  WINPR_ATTR_UNUSED sd_bus_error* err)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
	uint32_t node = 0;

	if (sd_bus_message_read(msg, "u", &node) >= 0)
	{
		mutter->nodeId = node;
		mutter->haveNode = TRUE;
		WLog_DBG(TAG, "PipeWire node %" PRIu32, mutter->nodeId);
	}
	return 0;
}

static int mutter_on_closed(WINPR_ATTR_UNUSED sd_bus_message* msg, void* data,
                            WINPR_ATTR_UNUSED sd_bus_error* err)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
	mutter->closed = TRUE;
	return 0;
}

/* The stream's "mapping-id" parameter names the libei region of its monitor */
static void mutter_read_stream_parameters(mutterShadowSubsystem* mutter)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = nullptr;

	int r = sd_bus_get_property(mutter->bus, SC_NAME, mutter->streamPath, SC_STREAM_IFACE,
	                            "Parameters", &err, &reply, "a{sv}");
	if (r < 0)
	{
		WLog_DBG(TAG, "no stream parameters: %s", err.message ? err.message : strerror(-r));
		sd_bus_error_free(&err);
		return;
	}

	r = sd_bus_message_enter_container(reply, 'a', "{sv}");
	while (r > 0)
	{
		r = sd_bus_message_enter_container(reply, 'e', "sv");
		if (r <= 0)
			break;
		const char* key = nullptr;
		const char* value = nullptr;
		r = sd_bus_message_read(reply, "s", &key);
		if ((r >= 0) && key && (strcmp(key, "mapping-id") == 0))
		{
			r = sd_bus_message_read(reply, "v", "s", &value);
			if ((r >= 0) && value)
			{
				free(mutter->mappingId);
				mutter->mappingId = _strdup(value);
			}
		}
		else if (r >= 0)
			r = sd_bus_message_skip(reply, "v");
		if (r >= 0)
			r = sd_bus_message_exit_container(reply);
	}
	sd_bus_message_unref(reply);
}

static BOOL mutter_setup_sessions(mutterShadowSubsystem* mutter, const char* connector)
{
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

	int r = sd_bus_get_property_string(mutter->bus, RD_NAME, mutter->rdSessionPath,
	                                   RD_SESSION_IFACE, "SessionId", &err, &sessionId);
	if (r < 0)
	{
		WLog_ERR(TAG, "no SessionId: %s", err.message ? err.message : strerror(-r));
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

	/* 3. the monitor; the clients draw the cursor from its metadata */
	if ((sd_bus_message_new_method_call(mutter->bus, &call, SC_NAME, mutter->scSessionPath,
	                                    SC_SESSION_IFACE, "RecordMonitor") < 0) ||
	    (sd_bus_message_append(call, "sa{sv}", connector, 1, "cursor-mode", "u",
	                           (uint32_t)MUTTER_CURSOR_MODE_METADATA) < 0))
		goto out;
	WLog_DBG(TAG, "ScreenCast.Session.RecordMonitor('%s')", connector);
	if (!mutter_call_for_path(mutter->bus, call, "ScreenCast.Session.RecordMonitor",
	                          &mutter->streamPath))
		goto out;
	call = sd_bus_message_unref(call);

	if ((sd_bus_match_signal(mutter->bus, &mutter->streamAddedSlot, SC_NAME, mutter->streamPath,
	                         SC_STREAM_IFACE, "PipeWireStreamAdded", mutter_on_stream_added,
	                         mutter) < 0) ||
	    (sd_bus_match_signal(mutter->bus, &mutter->closedSlot, RD_NAME, mutter->rdSessionPath,
	                         RD_SESSION_IFACE, "Closed", mutter_on_closed, mutter) < 0))
	{
		WLog_ERR(TAG, "cannot watch the stream's signals");
		goto out;
	}

	/* 4. start, then wait for the PipeWire node */
	WLog_DBG(TAG, "RemoteDesktop.Session.Start");
	r = sd_bus_call_method(mutter->bus, RD_NAME, mutter->rdSessionPath, RD_SESSION_IFACE, "Start",
	                       &err, nullptr, "");
	if (r < 0)
	{
		WLog_ERR(TAG, "RemoteDesktop.Session.Start failed: %s",
		         err.message ? err.message : strerror(-r));
		goto out;
	}
	mutter->started = TRUE;

	const UINT64 start = GetTickCount64();
	while (!mutter->haveNode)
	{
		r = sd_bus_process(mutter->bus, nullptr);
		if (r < 0)
			goto out;
		if (r > 0)
			continue;
		if (GetTickCount64() - start > MUTTER_STREAM_TIMEOUT_MS)
		{
			WLog_ERR(TAG, "Mutter announced no PipeWire stream");
			goto out;
		}
		(void)sd_bus_wait(mutter->bus, 100 * 1000);
	}
	mutter_read_stream_parameters(mutter);

	/* 5. input */
	int fd = -1;
	r = sd_bus_call_method(mutter->bus, RD_NAME, mutter->rdSessionPath, RD_SESSION_IFACE,
	                       "ConnectToEIS", &err, &reply, "a{sv}", 0);
	if ((r < 0) || (sd_bus_message_read(reply, "h", &fd) < 0))
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

static UINT32 mutter_cursor_hash(const BYTE* pixels, UINT32 width, UINT32 height, UINT32 xHot,
                                 UINT32 yHot)
{
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
	const UINT32 hash = mutter_cursor_hash(pixels, width, height, xHot, yHot);
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

/* The cursor in a buffer's metadata: its position, and its shape when it changed */
static void mutter_cursor_update(mutterShadowSubsystem* mutter, struct spa_buffer* buf)
{
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

static void mutter_on_process(void* data)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)data;
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
		struct spa_data* d = &buf->datas[0];

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
		}
		LeaveCriticalSection(&surface->lock);
		painted = TRUE;
		(void)pw_stream_queue_buffer(mutter->stream, pb);
	}

	if (!painted)
		return;

	ArrayList_Lock(server->clients);
	if (ArrayList_Count(server->clients) > 0)
		shadow_subsystem_frame_update(&mutter->common);
	ArrayList_Unlock(server->clients);

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

static BOOL mutter_connect_stream(mutterShadowSubsystem* mutter)
{
	uint8_t podBuffer[1024] = WINPR_C_ARRAY_INIT;
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
	const struct spa_rectangle size = SPA_RECTANGLE(1920, 1080);
	const struct spa_rectangle minSize = SPA_RECTANGLE(1, 1);
	const struct spa_rectangle maxSize = SPA_RECTANGLE(8192, 8192);
	const struct spa_fraction rate = SPA_FRACTION(0, 1);
	const struct spa_fraction maxRate = SPA_FRACTION(60, 1);
	const struct spa_fraction minRate = SPA_FRACTION(1, 1);

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
		WLog_ERR(TAG, "cannot connect to PipeWire: %s", strerror(errno));
		return FALSE;
	}
	pw_core_add_listener(mutter->core, &mutter->coreListener, &mutter_core_events, mutter);

	mutter->stream =
	    pw_stream_new(mutter->core, "FreeRDP shadow server",
	                  pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
	                                    "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr));
	if (!mutter->stream)
		return FALSE;
	pw_stream_add_listener(mutter->stream, &mutter->streamListener, &mutter_stream_events, mutter);

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

/* ------------------------------------------------------------------------ */
/* libei                                                                     */
/* ------------------------------------------------------------------------ */

/* under eiLock */
static void mutter_process_ei_events(mutterShadowSubsystem* mutter)
{
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
	if (!mutter->keyboard || !mutter->keyboardReady)
		return;
	/* Mutter keeps the modifier state itself, as for a real keyboard */
	ei_device_keyboard_key(mutter->keyboard, code, down ? true : false);
	ei_device_frame(mutter->keyboard, ei_now(mutter->ei));
}

WINPR_ATTR_NODISCARD
static BOOL mutter_shadow_input_synchronize_event(rdpShadowSubsystem* subsystem,
                                                  rdpShadowClient* client, UINT32 flags)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter || !client)
		return FALSE;

	EnterCriticalSection(&mutter->eiLock);
	if (mutter->ei && mutter->lockedModsKnown)
	{
		/* the client's lock keys win: tap each one that differs */
		const BOOL caps = (flags & KBD_SYNC_CAPS_LOCK) ? TRUE : FALSE;
		const BOOL num = (flags & KBD_SYNC_NUM_LOCK) ? TRUE : FALSE;
		if (caps != ((mutter->lockedMods & MUTTER_MOD_LOCK) ? TRUE : FALSE))
		{
			mutter_key(mutter, KEY_CAPSLOCK, TRUE);
			mutter_key(mutter, KEY_CAPSLOCK, FALSE);
		}
		if (num != ((mutter->lockedMods & MUTTER_MOD_NUM) ? TRUE : FALSE))
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
	const BOOL down = (flags & KBD_FLAGS_RELEASE) ? FALSE : TRUE;

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
	if (!ei_device_has_capability(mutter->pointer, EI_DEVICE_CAP_BUTTON))
		return;
	ei_device_button_button(mutter->pointer, button, down ? true : false);
	ei_device_frame(mutter->pointer, ei_now(mutter->ei));
}

/* under eiLock; 120 is one notch, as in RDP */
static void mutter_pointer_wheel(mutterShadowSubsystem* mutter, UINT16 flags, BOOL horizontal)
{
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

			const BOOL down = (flags & PTR_FLAGS_DOWN) ? TRUE : FALSE;
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

		const BOOL down = (flags & PTR_XFLAGS_DOWN) ? TRUE : FALSE;
		if (flags & PTR_XFLAGS_BUTTON1)
			mutter_pointer_button(mutter, BTN_SIDE, down);
		else if (flags & PTR_XFLAGS_BUTTON2)
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

	const struct passwd* owner = getpwuid(getuid());
	if (!owner || !owner->pw_name || (strcmp(owner->pw_name, user) != 0))
	{
		WLog_ERR(TAG, "login as '%s' refused: this session belongs to '%s'", user,
		         (owner && owner->pw_name) ? owner->pw_name : "?");
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
		return -1;
	}

	status = pam_authenticate(handle, 0);
	if (status == PAM_SUCCESS)
		status = pam_acct_mgmt(handle, 0);
	if (status != PAM_SUCCESS)
		WLog_ERR(TAG, "login as '%s' failed: %s", user, pam_strerror(handle, status));

	pam_end(handle, status);
	return (status == PAM_SUCCESS) ? 1 : -1;
}
#endif

/* ------------------------------------------------------------------------ */
/* the subsystem                                                             */
/* ------------------------------------------------------------------------ */

static void mutter_shadow_refresh(mutterShadowSubsystem* mutter)
{
	/* the next frame goes out whole; Mutter sends one when asked for nothing
	 * else only once something changes, so ask the clients' surface again */
	mutter->fullPaint = TRUE;
	mutter->cursorResend = TRUE;

	rdpShadowServer* server = mutter->common.server;
	rdpShadowSurface* surface = server ? server->surface : nullptr;
	if (!surface)
		return;

	EnterCriticalSection(&surface->lock);
	const RECTANGLE_16 all = { 0, 0, (UINT16)surface->width, (UINT16)surface->height };
	if (!region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &all))
		WLog_ERR(TAG, "cannot mark the screen changed");
	LeaveCriticalSection(&surface->lock);

	ArrayList_Lock(server->clients);
	if (ArrayList_Count(server->clients) > 0)
		shadow_subsystem_frame_update(&mutter->common);
	ArrayList_Unlock(server->clients);

	EnterCriticalSection(&surface->lock);
	region16_clear(&surface->invalidRegion);
	LeaveCriticalSection(&surface->lock);
}

static void mutter_shadow_process_message(mutterShadowSubsystem* mutter, wMessage* message)
{
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

static DWORD WINAPI mutter_shadow_subsystem_thread(LPVOID arg)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)arg;
	wMessagePipe* MsgPipe = mutter->common.MsgPipe;
	HANDLE events[5] = WINPR_C_ARRAY_INIT;
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

		const DWORD status = WaitForMultipleObjects(count, events, FALSE, timeout);
		if ((status == WAIT_FAILED) || (WaitForSingleObject(mutter->stopEvent, 0) == WAIT_OBJECT_0))
			break;

		while (sd_bus_process(mutter->bus, nullptr) > 0)
		{
		}
		(void)sd_bus_flush(mutter->bus);
		if (mutter->closed && !mutter->closedLogged)
		{
			WLog_WARN(TAG, "the GNOME session stopped the sharing (from its screen sharing "
			               "indicator, or GNOME Shell ended): the clients see the last picture");
			mutter->closedLogged = TRUE;
		}

		pw_loop_enter(mutter->loop);
		(void)pw_loop_iterate(mutter->loop, 0);
		pw_loop_leave(mutter->loop);

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

	int r = sd_bus_open_user(&mutter->bus);
	if (r < 0)
	{
		WLog_ERR(TAG,
		         "cannot connect to the session bus: %s (start the server inside the GNOME "
		         "session, as its user)",
		         strerror(-r));
		return -1;
	}

	if (!mutter_wait_for_name(mutter->bus, RD_NAME, MUTTER_NAME_TIMEOUT_MS))
	{
		WLog_ERR(TAG, "%s is not on the session bus: is this a GNOME session?", RD_NAME);
		return -1;
	}

	WLog_DBG(TAG, "DisplayConfig.GetCurrentState");
	UINT32 count = mutter_list_monitors(mutter->bus, subsystem->monitors, mutter->connectors,
	                                    MUTTER_SHADOW_MAX_MONITORS);
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

	/* the frames never carry the cursor: the clients draw it from the shapes
	 * sent (ShowMouseCursor is otherwise only set by /server-side-cursor) */
	if (subsystem->server)
		subsystem->server->ShowMouseCursor = TRUE;

	WLog_INFO(TAG,
	          "sharing monitor %s (%" PRIu32 " of %" PRIu32 "): %" PRIu32 "x%" PRIu32 ", input %s",
	          connector[0] ? connector : "(primary)", subsystem->selectedMonitor, count,
	          mutter->width, mutter->height, mutter->ei ? "through libei" : "off");
	return 1;
}

static int mutter_shadow_subsystem_uninit(rdpShadowSubsystem* subsystem)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter)
		return -1;

	if (mutter->stream)
		pw_stream_destroy(mutter->stream);
	mutter->stream = nullptr;
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
	return TRUE;
}

static void mutter_shadow_subsystem_free(rdpShadowSubsystem* subsystem)
{
	mutterShadowSubsystem* mutter = (mutterShadowSubsystem*)subsystem;
	if (!mutter)
		return;

	(void)mutter_shadow_subsystem_stop(subsystem);
	(void)mutter_shadow_subsystem_uninit(subsystem);
	DeleteCriticalSection(&mutter->eiLock);
	if (mutter->stopEvent)
		(void)CloseHandle(mutter->stopEvent);
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
	if (!mutter->stopEvent)
	{
		mutter_shadow_subsystem_free(&mutter->common);
		return nullptr;
	}

	mutter->cursorX = -1;
	mutter->cursorY = -1;
	mutter->common.SynchronizeEvent = mutter_shadow_input_synchronize_event;
	mutter->common.KeyboardEvent = mutter_shadow_input_keyboard_event;
	mutter->common.UnicodeKeyboardEvent = mutter_shadow_input_unicode_keyboard_event;
	mutter->common.MouseEvent = mutter_shadow_input_mouse_event;
	mutter->common.ExtendedMouseEvent = mutter_shadow_input_extended_mouse_event;
	mutter->common.ClientConnect = mutter_shadow_client_connect;
#ifdef WITH_PAM
	mutter->common.Authenticate = mutter_shadow_pam_authenticate;
#endif
	return &mutter->common;
}

FREERDP_API const char* ShadowSubsystemName(void)
{
	return "Mutter";
}

FREERDP_API int ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->New = mutter_shadow_subsystem_new;
	pEntryPoints->Free = mutter_shadow_subsystem_free;
	pEntryPoints->Init = mutter_shadow_subsystem_init;
	pEntryPoints->Uninit = mutter_shadow_subsystem_uninit;
	pEntryPoints->Start = mutter_shadow_subsystem_start;
	pEntryPoints->Stop = mutter_shadow_subsystem_stop;
	pEntryPoints->EnumMonitors = mutter_shadow_enum_monitors;
	return 1;
}

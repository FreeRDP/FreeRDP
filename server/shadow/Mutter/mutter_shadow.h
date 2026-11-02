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

#ifndef FREERDP_SERVER_SHADOW_MUTTER_SHADOW_H
#define FREERDP_SERVER_SHADOW_MUTTER_SHADOW_H

#include <freerdp/server/shadow.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include <systemd/sd-bus.h>
#include <pipewire/pipewire.h>
#include <libei.h>

#define MUTTER_SHADOW_MAX_MONITORS 16
#define MUTTER_SHADOW_CONNECTOR_SIZE 64

typedef struct mutter_shadow_subsystem mutterShadowSubsystem;

struct mutter_shadow_subsystem
{
	rdpShadowSubsystem common;

	/* the session bus, and the remote desktop and screen cast sessions on it;
	 * used by the init and then only by the subsystem thread */
	sd_bus* bus;
	sd_bus_slot* streamAddedSlot;
	sd_bus_slot* closedSlot;
	char* rdSessionPath;
	char* scSessionPath;
	char* streamPath;
	char* mappingId; /* ties the stream to its libei region */
	UINT32 nodeId;
	BOOL haveNode;
	BOOL closed;
	BOOL closedLogged;

	/* the monitors as Mutter lays them out, and their connectors */
	char connectors[MUTTER_SHADOW_MAX_MONITORS][MUTTER_SHADOW_CONNECTOR_SIZE];

	/* PipeWire: the stream of the shared monitor, only in the subsystem thread
	 * once started. width and height are the stream's, which the surface follows */
	struct pw_loop* loop;
	struct pw_context* context;
	struct pw_core* core;
	struct spa_hook coreListener;
	struct pw_stream* stream;
	struct spa_hook streamListener;
	UINT32 width;
	UINT32 height;
	UINT32 format; /* FreeRDP pixel format of the stream */
	BOOL haveFormat;
	BOOL started;
	BOOL fullPaint;

	/* libei, under eiLock: the client threads post input, the subsystem
	 * thread reads its events */
	CRITICAL_SECTION eiLock;
	struct ei* ei;
	struct ei_device* keyboard;
	struct ei_device* pointer;
	BOOL keyboardReady;
	BOOL pointerReady;
	uint32_t sequence;
	uint32_t lockedMods;
	BOOL lockedModsKnown;
	UINT64 pointerInput; /* when a client last moved the pointer */

	/* the cursor the clients draw: its last shape and position */
	UINT32 cursorHash;
	BOOL cursorKnown;
	BOOL cursorResend;
	INT32 cursorX;
	INT32 cursorY;

	/* sound: a capture of the default sink's monitor, running only while
	 * clients are connected; samples go out in messages of 20 ms */
	struct pw_stream* audio;
	struct spa_hook audioListener;
	wStream* audioStream;
	BOOL audioActive;
	UINT64 audioFrames;
	UINT64 audioMessages;
	float audioPeak;
	UINT64 audioLogged;

	/* the clients connected; the event tells the thread the count changed */
	volatile LONG clients;
	HANDLE clientsEvent;

	HANDLE thread;
	HANDLE stopEvent;
};

#endif /* FREERDP_SERVER_SHADOW_MUTTER_SHADOW_H */

/*
   Android JNI Client Layer

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef __ANDROID_FREERDP_H
#define __ANDROID_FREERDP_H

#include <jni.h>

#include <winpr/crt.h>
#include <winpr/clipboard.h>

#include <freerdp/freerdp.h>
#include <freerdp/client/cliprdr.h>

#include "android_event.h"

struct android_context
{
	rdpContext rdpCtx;

	ANDROID_EVENT_QUEUE* event_queue;
	HANDLE thread;

	BOOL is_connected;

	BOOL clipboardSync;
	wClipboard* clipboard;
	UINT32 numServerFormats;
	UINT32 requestedFormatId;
	HANDLE clipboardRequestEvent;
	CLIPRDR_FORMAT* serverFormats;
	CliprdrClientContext* cliprdr;
	UINT32 clipboardCapabilities;
};
typedef struct android_context androidContext;

#endif /* __ANDROID_FREERDP_H */



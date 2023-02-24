/*
 RDP run-loop

 Copyright 2013 Thincast Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson
 Copyright 2023 Iordan Iordanov

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <CoreGraphics/CoreGraphics.h>

#import <freerdp/freerdp.h>
#import <freerdp/channels/channels.h>
#import "TSXTypes.h"
#import <winpr/clipboard.h>
#import <freerdp/client/cliprdr.h>

@class RDPSession, RDPSessionView;

typedef BOOL (*pServerCutText)(rdpContext *context, UINT8 *data, UINT32 size);

// FreeRDP extended structs
typedef struct mf_info mfInfo;

typedef struct mf_context
{
	rdpContext _p;

	mfInfo *mfi;
	rdpSettings *settings;

	BOOL clipboardSync;
	wClipboard *clipboard;
	UINT32 numServerFormats;
	UINT32 requestedFormatId;
	HANDLE clipboardRequestEvent;
	CLIPRDR_FORMAT *serverFormats;
	CliprdrClientContext *cliprdr;
	UINT32 clipboardCapabilities;
	pServerCutText ServerCutText;
} mfContext;

struct mf_info
{
	// RDP
	freerdp *instance;
	mfContext *context;
	rdpContext *_context;

	// UI
	RDPSession *session;

	// Graphics
	CGContextRef bitmap_context;

	// Events
	int event_pipe_producer;
	int event_pipe_consumer;
	HANDLE handle;

	// Tracking connection state
	volatile TSXConnectionState connection_state;
	volatile BOOL
	    unwanted; // set when controlling Session no longer wants the connection to continue
};

#define MFI_FROM_INSTANCE(inst) (((mfContext *)((inst)->context))->mfi)

enum MF_EXIT_CODE
{
	MF_EXIT_SUCCESS = 0,

	MF_EXIT_CONN_FAILED = 128,
	MF_EXIT_CONN_CANCELED = 129,
	MF_EXIT_LOGON_TIMEOUT = 130,

	MF_EXIT_UNKNOWN = 255
};

void ios_init_freerdp(void);
void ios_uninit_freerdp(void);
freerdp *ios_freerdp_new(void);
int ios_run_freerdp(freerdp *instance);
void ios_freerdp_free(freerdp *instance);
void ios_send_clipboard_data(void *context, const void *data, UINT32 size);

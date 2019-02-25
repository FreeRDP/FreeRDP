#ifndef FREERDP_SERVER_PROXY_PFREERDP_H
#define FREERDP_SERVER_PROXY_PFREERDP_H

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/server/audin.h>
#include <freerdp/server/rdpsnd.h>
#include <freerdp/server/encomsp.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/wlog.h>

// Define log tags
#define PROXY_TAG(tag) "proxy." tag

struct test_peer_context
{
	rdpContext _p;

	RFX_CONTEXT* rfx_context;
	NSC_CONTEXT* nsc_context;
	wStream* s;
	BYTE* icon_data;
	BYTE* bg_data;
	int icon_width;
	int icon_height;
	int icon_x;
	int icon_y;
	BOOL activated;
	HANDLE event;
	HANDLE stopEvent;
	HANDLE vcm;
	void* debug_channel;
	HANDLE debug_channel_thread;
	audin_server_context* audin;
	BOOL audin_open;
	UINT32 frame_id;
	RdpsndServerContext* rdpsnd;
	EncomspServerContext* encomsp;
};
typedef struct test_peer_context testPeerContext;

#endif /* FREERDP_SERVER_PROXY_PFREERDP_H */
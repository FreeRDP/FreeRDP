/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
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
#include <winpr/string.h>

#include <winpr/smartcard.h>
#include <winpr/pool.h>

#include <freerdp/server/proxy/proxy_log.h>
#include <freerdp/emulate/scard/smartcard_emulate.h>
#include <freerdp/channels/scard.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/utils/rdpdr_utils.h>

#include <freerdp/utils/smartcard_operations.h>
#include <freerdp/utils/smartcard_call.h>

#include "pf_channel_smartcard.h"
#include "pf_channel_rdpdr.h"

#define TAG PROXY_TAG("channel.scard")

#define SCARD_SVC_CHANNEL_NAME "SCARD"

typedef struct
{
	InterceptContextMapEntry base;
	scard_call_context* callctx;
	PTP_POOL ThreadPool;
	TP_CALLBACK_ENVIRON ThreadPoolEnv;
	wArrayList* workObjects;
} pf_channel_client_context;

typedef struct
{
	SMARTCARD_OPERATION op;
	wStream* out;
	pClientContext* pc;
	wLog* log;
	pf_scard_send_fkt_t send_fkt;
} pf_channel_client_queue_element;

static pf_channel_client_context* scard_get_client_context(pClientContext* pc)
{
	pf_channel_client_context* scard;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->interceptContextMap);

	scard = HashTable_GetItemValue(pc->interceptContextMap, SCARD_SVC_CHANNEL_NAME);
	if (!scard)
		WLog_WARN(TAG, "[%s] missing in pc->interceptContextMap", SCARD_SVC_CHANNEL_NAME);
	return scard;
}

static BOOL pf_channel_client_write_iostatus(wStream* out, const SMARTCARD_OPERATION* op,
                                             UINT32 ioStatus)
{
	UINT16 component, packetid;
	UINT32 dID, cID;
	size_t pos;

	WINPR_ASSERT(op);
	WINPR_ASSERT(out);

	pos = Stream_GetPosition(out);
	Stream_SetPosition(out, 0);
	if (!Stream_CheckAndLogRequiredLength(TAG, out, 16))
		return FALSE;

	Stream_Read_UINT16(out, component);
	Stream_Read_UINT16(out, packetid);

	Stream_Read_UINT32(out, dID);
	Stream_Read_UINT32(out, cID);

	WINPR_ASSERT(component == RDPDR_CTYP_CORE);
	WINPR_ASSERT(packetid == PAKID_CORE_DEVICE_IOCOMPLETION);
	WINPR_ASSERT(dID == op->deviceID);
	WINPR_ASSERT(cID == op->completionID);

	Stream_Write_UINT32(out, ioStatus);
	Stream_SetPosition(out, pos);
	return TRUE;
}

struct thread_arg
{
	pf_channel_client_context* scard;
	pf_channel_client_queue_element* e;
};

static void queue_free(void* obj);
static void* queue_copy(const void* obj);

static VOID irp_thread(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
{
	struct thread_arg* arg = Context;
	pf_channel_client_context* scard = arg->scard;
	{
		UINT32 ioStatus;
		LONG rc = smartcard_irp_device_control_call(arg->scard->callctx, arg->e->out, &ioStatus,
		                                            &arg->e->op);
		if (rc == CHANNEL_RC_OK)
		{
			if (pf_channel_client_write_iostatus(arg->e->out, &arg->e->op, ioStatus))
				arg->e->send_fkt(arg->e->log, arg->e->pc, arg->e->out);
		}
	}
	queue_free(arg->e);
	free(arg);
	ArrayList_Remove(scard->workObjects, Work);
}

static BOOL start_irp_thread(pf_channel_client_context* scard,
                             const pf_channel_client_queue_element* e)
{
	PTP_WORK work;
	struct thread_arg* arg = calloc(1, sizeof(struct thread_arg));
	if (!arg)
		return FALSE;
	arg->scard = scard;
	arg->e = queue_copy(e);
	if (!arg->e)
		goto fail;

	work = CreateThreadpoolWork(irp_thread, arg, &scard->ThreadPoolEnv);
	if (!work)
		goto fail;
	ArrayList_Append(scard->workObjects, work);
	SubmitThreadpoolWork(work);

	return TRUE;

fail:
	if (arg)
		queue_free(arg->e);
	free(arg);
	return FALSE;
}

BOOL pf_channel_smartcard_client_handle(wLog* log, pClientContext* pc, wStream* s, wStream* out,
                                        pf_scard_send_fkt_t send_fkt)
{
	BOOL rc = FALSE;
	LONG status;
	UINT32 FileId;
	UINT32 CompletionId;
	UINT32 ioStatus;
	pf_channel_client_queue_element e = { 0 };
	pf_channel_client_context* scard = scard_get_client_context(pc);

	WINPR_ASSERT(log);
	WINPR_ASSERT(send_fkt);
	WINPR_ASSERT(s);

	if (!scard)
		return FALSE;

	e.log = log;
	e.pc = pc;
	e.out = out;
	e.send_fkt = send_fkt;

	/* Skip IRP header */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return FALSE;
	else
	{
		UINT32 DeviceId;
		UINT32 MajorFunction;
		UINT32 MinorFunction;

		Stream_Read_UINT32(s, DeviceId);      /* DeviceId (4 bytes) */
		Stream_Read_UINT32(s, FileId);        /* FileId (4 bytes) */
		Stream_Read_UINT32(s, CompletionId);  /* CompletionId (4 bytes) */
		Stream_Read_UINT32(s, MajorFunction); /* MajorFunction (4 bytes) */
		Stream_Read_UINT32(s, MinorFunction); /* MinorFunction (4 bytes) */

		if (MajorFunction != IRP_MJ_DEVICE_CONTROL)
		{
			WLog_WARN(TAG, "[%s] Invalid IRP received, expected %s, got %2", SCARD_SVC_CHANNEL_NAME,
			          rdpdr_irp_string(IRP_MJ_DEVICE_CONTROL), rdpdr_irp_string(MajorFunction));
			return FALSE;
		}
		e.op.completionID = CompletionId;
		e.op.deviceID = DeviceId;

		if (!rdpdr_write_iocompletion_header(out, DeviceId, CompletionId, 0))
			return FALSE;
	}

	status = smartcard_irp_device_control_decode(s, CompletionId, FileId, &e.op);
	if (status != 0)
		goto fail;

	switch (e.op.ioControlCode)
	{
		case SCARD_IOCTL_LISTREADERGROUPSA:
		case SCARD_IOCTL_LISTREADERGROUPSW:
		case SCARD_IOCTL_LISTREADERSA:
		case SCARD_IOCTL_LISTREADERSW:
		case SCARD_IOCTL_LOCATECARDSA:
		case SCARD_IOCTL_LOCATECARDSW:
		case SCARD_IOCTL_LOCATECARDSBYATRA:
		case SCARD_IOCTL_LOCATECARDSBYATRW:
		case SCARD_IOCTL_GETSTATUSCHANGEA:
		case SCARD_IOCTL_GETSTATUSCHANGEW:
		case SCARD_IOCTL_CONNECTA:
		case SCARD_IOCTL_CONNECTW:
		case SCARD_IOCTL_RECONNECT:
		case SCARD_IOCTL_DISCONNECT:
		case SCARD_IOCTL_BEGINTRANSACTION:
		case SCARD_IOCTL_ENDTRANSACTION:
		case SCARD_IOCTL_STATE:
		case SCARD_IOCTL_STATUSA:
		case SCARD_IOCTL_STATUSW:
		case SCARD_IOCTL_TRANSMIT:
		case SCARD_IOCTL_CONTROL:
		case SCARD_IOCTL_GETATTRIB:
		case SCARD_IOCTL_SETATTRIB:
			if (!start_irp_thread(scard, &e))
				goto fail;
			return TRUE;

		default:
			status = smartcard_irp_device_control_call(scard->callctx, out, &ioStatus, &e.op);
			if (status != 0)
				goto fail;
			if (!pf_channel_client_write_iostatus(out, &e.op, ioStatus))
				goto fail;
			break;
	}

	rc = send_fkt(log, pc, out) == CHANNEL_RC_OK;

fail:
	smartcard_operation_free(&e.op, FALSE);
	return rc;
}

BOOL pf_channel_smartcard_server_handle(pServerContext* ps, wStream* s)
{
	WLog_ERR(TAG, "TODO: %s unimplemented", __FUNCTION__);
	return TRUE;
}

static void channel_stop_and_wait(pf_channel_client_context* scard, BOOL reset)
{
	WINPR_ASSERT(scard);
	smartcard_call_context_signal_stop(scard->callctx, FALSE);

	while (ArrayList_Count(scard->workObjects) > 0)
	{
		PTP_WORK work = ArrayList_GetItem(scard->workObjects, 0);
		if (!work)
			continue;
		WaitForThreadpoolWorkCallbacks(work, TRUE);
	}

	smartcard_call_context_signal_stop(scard->callctx, reset);
}

static void pf_channel_scard_client_context_free(InterceptContextMapEntry* base)
{
	pf_channel_client_context* entry = (pf_channel_client_context*)base;
	if (!entry)
		return;

	/* Set the stop event.
	 * All threads waiting in blocking operations will abort at the next
	 * available polling slot */
	channel_stop_and_wait(entry, FALSE);
	ArrayList_Free(entry->workObjects);
	CloseThreadpool(entry->ThreadPool);
	DestroyThreadpoolEnvironment(&entry->ThreadPoolEnv);

	smartcard_call_context_free(entry->callctx);
	free(entry);
}

static void queue_free(void* obj)
{
	pf_channel_client_queue_element* element = obj;
	if (!element)
		return;
	smartcard_operation_free(&element->op, FALSE);
	Stream_Free(element->out, TRUE);
	free(element);
}

static void* queue_copy(const void* obj)
{
	const pf_channel_client_queue_element* other = obj;
	pf_channel_client_queue_element* copy;
	if (!other)
		return NULL;
	copy = calloc(1, sizeof(pf_channel_client_queue_element));
	if (!copy)
		return NULL;

	*copy = *other;
	copy->out = Stream_New(NULL, Stream_Capacity(other->out));
	if (!copy->out)
		goto fail;
	Stream_Write(copy->out, Stream_Buffer(other->out), Stream_GetPosition(other->out));
	return copy;
fail:
	queue_free(copy);
	return NULL;
}

static void work_object_free(void* arg)
{
	PTP_WORK work = arg;
	CloseThreadpoolWork(work);
}

BOOL pf_channel_smartcard_client_new(pClientContext* pc)
{
	pf_channel_client_context* scard;
	wObject* obj;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->interceptContextMap);

	scard = calloc(1, sizeof(pf_channel_client_context));
	if (!scard)
		return FALSE;
	scard->base.free = pf_channel_scard_client_context_free;
	scard->callctx = smartcard_call_context_new(pc->context.settings);
	if (!scard->callctx)
		goto fail;

	scard->workObjects = ArrayList_New(TRUE);
	if (!scard->workObjects)
		goto fail;
	obj = ArrayList_Object(scard->workObjects);
	WINPR_ASSERT(obj);
	obj->fnObjectFree = work_object_free;

	scard->ThreadPool = CreateThreadpool(NULL);
	if (!scard->ThreadPool)
		goto fail;
	InitializeThreadpoolEnvironment(&scard->ThreadPoolEnv);
	SetThreadpoolCallbackPool(&scard->ThreadPoolEnv, scard->ThreadPool);

	return HashTable_Insert(pc->interceptContextMap, SCARD_SVC_CHANNEL_NAME, scard);
fail:
	pf_channel_scard_client_context_free(&scard->base);
	return FALSE;
}

void pf_channel_smartcard_client_free(pClientContext* pc)
{
	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->interceptContextMap);
	HashTable_Remove(pc->interceptContextMap, SCARD_SVC_CHANNEL_NAME);
}

BOOL pf_channel_smartcard_client_emulate(pClientContext* pc)
{
	pf_channel_client_context* scard = scard_get_client_context(pc);
	if (!scard)
		return FALSE;
	return smartcard_call_is_configured(scard->callctx);
}

BOOL pf_channel_smartcard_client_reset(pClientContext* pc)
{
	pf_channel_client_context* scard = scard_get_client_context(pc);
	if (!scard)
		return TRUE;

	channel_stop_and_wait(scard, TRUE);
	return TRUE;
}

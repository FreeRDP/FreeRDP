/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
 * Copyright 2011 Anthony Tong <atong@trustedcs.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/smartcard.h>
#include <winpr/environment.h>

#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/scard.h>
#include <freerdp/utils/smartcard_call.h>
#include <freerdp/utils/smartcard_operations.h>
#include <freerdp/utils/rdpdr_utils.h>

#include "smartcard_main.h"

#define CAST_FROM_DEVICE(device) cast_device_from(device, __FUNCTION__, __FILE__, __LINE__)

typedef struct
{
	SMARTCARD_OPERATION operation;
	IRP* irp;
} scard_irp_queue_element;

static SMARTCARD_DEVICE* sSmartcard = NULL;

static void smartcard_context_free(void* pCtx);

static UINT smartcard_complete_irp(SMARTCARD_DEVICE* smartcard, IRP* irp);

static SMARTCARD_DEVICE* cast_device_from(DEVICE* device, const char* fkt, const char* file,
                                          int line)
{
	if (!device)
	{
		WLog_ERR(TAG, "%s [%s:%d] Called smartcard channel with NULL device", fkt, file, line);
		return NULL;
	}

	if (device->type != RDPDR_DTYP_SMARTCARD)
	{
		WLog_ERR(TAG, "%s [%s:%d] Called smartcard channel with invalid device of type %" PRIx32,
		         fkt, file, line, device->type);
		return NULL;
	}

	return (SMARTCARD_DEVICE*)device;
}

static DWORD WINAPI smartcard_context_thread(LPVOID arg)
{
	SMARTCARD_CONTEXT* pContext = (SMARTCARD_CONTEXT*)arg;
	DWORD nCount;
	LONG status = 0;
	DWORD waitStatus;
	HANDLE hEvents[2];
	wMessage message;
	SMARTCARD_DEVICE* smartcard;
	UINT error = CHANNEL_RC_OK;
	smartcard = pContext->smartcard;
	nCount = 0;
	hEvents[nCount++] = MessageQueue_Event(pContext->IrpQueue);

	while (1)
	{
		waitStatus = WaitForMultipleObjects(nCount, hEvents, FALSE, INFINITE);

		if (waitStatus == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "!", error);
			break;
		}

		waitStatus = WaitForSingleObject(MessageQueue_Event(pContext->IrpQueue), 0);

		if (waitStatus == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", error);
			break;
		}

		if (waitStatus == WAIT_OBJECT_0)
		{
			scard_irp_queue_element* element;

			if (!MessageQueue_Peek(pContext->IrpQueue, &message, TRUE))
			{
				WLog_ERR(TAG, "MessageQueue_Peek failed!");
				status = ERROR_INTERNAL_ERROR;
				break;
			}

			if (message.id == WMQ_QUIT)
				break;

			element = (scard_irp_queue_element*)message.wParam;

			if (element)
			{
				WINPR_ASSERT(smartcard);

				if ((status = smartcard_irp_device_control_call(
				         smartcard->callctx, element->irp->output, &element->irp->IoStatus,
				         &element->operation)))
				{
					smartcard_operation_free(&element->operation, TRUE);
					WLog_ERR(TAG, "smartcard_irp_device_control_call failed with error %" PRIu32 "",
					         status);
					break;
				}

				if ((error = smartcard_complete_irp(smartcard, element->irp)))
				{
					smartcard_operation_free(&element->operation, TRUE);
					WLog_ERR(TAG, "Queue_Enqueue failed!");
					break;
				}

				smartcard_operation_free(&element->operation, TRUE);
			}
		}
	}

	if (status && smartcard->rdpcontext)
		setChannelError(smartcard->rdpcontext, error, "smartcard_context_thread reported an error");

	ExitThread(status);
	return error;
}

static void* smartcard_context_new(void* smartcard, SCARDCONTEXT hContext)
{
	SMARTCARD_CONTEXT* pContext;
	pContext = (SMARTCARD_CONTEXT*)calloc(1, sizeof(SMARTCARD_CONTEXT));

	if (!pContext)
	{
		WLog_ERR(TAG, "calloc failed!");
		return pContext;
	}

	pContext->smartcard = smartcard;
	pContext->hContext = hContext;
	pContext->IrpQueue = MessageQueue_New(NULL);

	if (!pContext->IrpQueue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		goto fail;
	}

	pContext->thread = CreateThread(NULL, 0, smartcard_context_thread, pContext, 0, NULL);

	if (!pContext->thread)
	{
		WLog_ERR(TAG, "CreateThread failed!");
		goto fail;
	}

	return pContext;
fail:
	smartcard_context_free(pContext);
	return NULL;
}

void smartcard_context_free(void* pCtx)
{
	SMARTCARD_CONTEXT* pContext = pCtx;

	if (!pContext)
		return;

	/* cancel blocking calls like SCardGetStatusChange */
	WINPR_ASSERT(pContext->smartcard);
	smartcard_call_cancel_context(pContext->smartcard->callctx, pContext->hContext);

	if (pContext->IrpQueue)
	{
		if (MessageQueue_PostQuit(pContext->IrpQueue, 0))
		{
			if (WaitForSingleObject(pContext->thread, INFINITE) == WAIT_FAILED)
				WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", GetLastError());

			CloseHandle(pContext->thread);
		}
		MessageQueue_Free(pContext->IrpQueue);
	}
	smartcard_call_release_context(pContext->smartcard->callctx, pContext->hContext);
	free(pContext);
}

static void smartcard_release_all_contexts(SMARTCARD_DEVICE* smartcard)
{
	smartcard_call_cancel_all_context(smartcard->callctx);
}

static UINT smartcard_free_(SMARTCARD_DEVICE* smartcard)
{
	if (!smartcard)
		return CHANNEL_RC_OK;

	if (smartcard->IrpQueue)
	{
		MessageQueue_Free(smartcard->IrpQueue);
		CloseHandle(smartcard->thread);
	}

	Stream_Free(smartcard->device.data, TRUE);
	ListDictionary_Free(smartcard->rgOutstandingMessages);

	smartcard_call_context_free(smartcard->callctx);

	free(smartcard);
	return CHANNEL_RC_OK;
}
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT smartcard_free(DEVICE* device)
{
	SMARTCARD_DEVICE* smartcard = CAST_FROM_DEVICE(device);

	if (!smartcard)
		return ERROR_INVALID_PARAMETER;

	/**
	 * Calling smartcard_release_all_contexts to unblock all operations waiting for transactions
	 * to unlock.
	 */
	smartcard_release_all_contexts(smartcard);

	/* Stopping all threads and cancelling all IRPs */

	if (smartcard->IrpQueue)
	{
		if (MessageQueue_PostQuit(smartcard->IrpQueue, 0) &&
		    (WaitForSingleObject(smartcard->thread, INFINITE) == WAIT_FAILED))
		{
			DWORD error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", error);
			return error;
		}
	}

	if (sSmartcard == smartcard)
		sSmartcard = NULL;

	return smartcard_free_(smartcard);
}

/**
 * Initialization occurs when the protocol server sends a device announce message.
 * At that time, we need to cancel all outstanding IRPs.
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT smartcard_init(DEVICE* device)
{
	SMARTCARD_DEVICE* smartcard = CAST_FROM_DEVICE(device);

	if (!smartcard)
		return ERROR_INVALID_PARAMETER;

	smartcard_release_all_contexts(smartcard);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT smartcard_complete_irp(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	void* key;

	WINPR_ASSERT(smartcard);
	WINPR_ASSERT(irp);

	key = (void*)(size_t)irp->CompletionId;
	ListDictionary_Remove(smartcard->rgOutstandingMessages, key);

	WINPR_ASSERT(irp->Complete);
	return irp->Complete(irp);
}

/**
 * Multiple threads and SCardGetStatusChange:
 * http://musclecard.996296.n3.nabble.com/Multiple-threads-and-SCardGetStatusChange-td4430.html
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT smartcard_process_irp(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	void* key;
	LONG status;
	BOOL asyncIrp = FALSE;
	SMARTCARD_CONTEXT* pContext = NULL;
	key = (void*)(size_t)irp->CompletionId;

	if (!ListDictionary_Add(smartcard->rgOutstandingMessages, key, irp))
	{
		WLog_ERR(TAG, "ListDictionary_Add failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (irp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
	{
		scard_irp_queue_element* element = calloc(1, sizeof(scard_irp_queue_element));
		if (!element)
			return ERROR_OUTOFMEMORY;

		element->irp = irp;
		element->operation.completionID = irp->CompletionId;

		status = smartcard_irp_device_control_decode(irp->input, irp->CompletionId, irp->FileId,
		                                             &element->operation);

		if (status != SCARD_S_SUCCESS)
		{
			UINT error;

			smartcard_operation_free(&element->operation, TRUE);
			irp->IoStatus = (UINT32)STATUS_UNSUCCESSFUL;

			if ((error = smartcard_complete_irp(smartcard, irp)))
			{
				WLog_ERR(TAG, "Queue_Enqueue failed!");
				return error;
			}

			return CHANNEL_RC_OK;
		}

		asyncIrp = TRUE;

		switch (element->operation.ioControlCode)
		{
			case SCARD_IOCTL_ESTABLISHCONTEXT:
			case SCARD_IOCTL_RELEASECONTEXT:
			case SCARD_IOCTL_ISVALIDCONTEXT:
			case SCARD_IOCTL_CANCEL:
			case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			case SCARD_IOCTL_RELEASETARTEDEVENT:
				asyncIrp = FALSE;
				break;

			case SCARD_IOCTL_LISTREADERGROUPSA:
			case SCARD_IOCTL_LISTREADERGROUPSW:
			case SCARD_IOCTL_LISTREADERSA:
			case SCARD_IOCTL_LISTREADERSW:
			case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			case SCARD_IOCTL_FORGETREADERGROUPA:
			case SCARD_IOCTL_FORGETREADERGROUPW:
			case SCARD_IOCTL_INTRODUCEREADERA:
			case SCARD_IOCTL_INTRODUCEREADERW:
			case SCARD_IOCTL_FORGETREADERA:
			case SCARD_IOCTL_FORGETREADERW:
			case SCARD_IOCTL_ADDREADERTOGROUPA:
			case SCARD_IOCTL_ADDREADERTOGROUPW:
			case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			case SCARD_IOCTL_LOCATECARDSA:
			case SCARD_IOCTL_LOCATECARDSW:
			case SCARD_IOCTL_LOCATECARDSBYATRA:
			case SCARD_IOCTL_LOCATECARDSBYATRW:
			case SCARD_IOCTL_READCACHEA:
			case SCARD_IOCTL_READCACHEW:
			case SCARD_IOCTL_WRITECACHEA:
			case SCARD_IOCTL_WRITECACHEW:
			case SCARD_IOCTL_GETREADERICON:
			case SCARD_IOCTL_GETDEVICETYPEID:
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
			case SCARD_IOCTL_GETTRANSMITCOUNT:
				asyncIrp = TRUE;
				break;
		}

		pContext = smartcard_call_get_context(smartcard->callctx, element->operation.hContext);

		if (!pContext)
			asyncIrp = FALSE;

		if (!asyncIrp)
		{
			UINT error;

			status =
			    smartcard_irp_device_control_call(smartcard->callctx, element->irp->output,
			                                      &element->irp->IoStatus, &element->operation);
			smartcard_operation_free(&element->operation, TRUE);

			if (status)
			{
				WLog_ERR(TAG, "smartcard_irp_device_control_call failed with error %" PRId32 "!",
				         status);
				return (UINT32)status;
			}

			if ((error = smartcard_complete_irp(smartcard, irp)))
			{
				WLog_ERR(TAG, "Queue_Enqueue failed!");
				return error;
			}
		}
		else
		{
			if (pContext)
			{
				if (!MessageQueue_Post(pContext->IrpQueue, NULL, 0, (void*)element, NULL))
				{
					smartcard_operation_free(&element->operation, TRUE);
					WLog_ERR(TAG, "MessageQueue_Post failed!");
					return ERROR_INTERNAL_ERROR;
				}
			}
		}
	}
	else
	{
		UINT ustatus;
		WLog_ERR(TAG, "Unexpected SmartCard IRP: MajorFunction %s, MinorFunction: 0x%08" PRIX32 "",
		         rdpdr_irp_string(irp->MajorFunction), irp->MinorFunction);
		irp->IoStatus = (UINT32)STATUS_NOT_SUPPORTED;

		if ((ustatus = smartcard_complete_irp(smartcard, irp)))
		{
			WLog_ERR(TAG, "Queue_Enqueue failed!");
			return ustatus;
		}
	}

	return CHANNEL_RC_OK;
}

static DWORD WINAPI smartcard_thread_func(LPVOID arg)
{
	IRP* irp;
	DWORD nCount;
	DWORD status;
	HANDLE hEvents[1];
	wMessage message;
	UINT error = CHANNEL_RC_OK;
	SMARTCARD_DEVICE* smartcard = CAST_FROM_DEVICE(arg);

	if (!smartcard)
		return ERROR_INVALID_PARAMETER;

	nCount = 0;
	hEvents[nCount++] = MessageQueue_Event(smartcard->IrpQueue);

	while (1)
	{
		status = WaitForMultipleObjects(nCount, hEvents, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
		{
			if (!MessageQueue_Peek(smartcard->IrpQueue, &message, TRUE))
			{
				WLog_ERR(TAG, "MessageQueue_Peek failed!");
				error = ERROR_INTERNAL_ERROR;
				break;
			}

			if (message.id == WMQ_QUIT)
				break;

			irp = (IRP*)message.wParam;

			if (irp)
			{
				if ((error = smartcard_process_irp(smartcard, irp)))
				{
					WLog_ERR(TAG, "smartcard_process_irp failed with error %" PRIu32 "!", error);
					goto out;
				}
			}
		}
	}

out:

	if (error && smartcard->rdpcontext)
		setChannelError(smartcard->rdpcontext, error, "smartcard_thread_func reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT smartcard_irp_request(DEVICE* device, IRP* irp)
{
	SMARTCARD_DEVICE* smartcard = CAST_FROM_DEVICE(device);

	if (!smartcard)
		return ERROR_INVALID_PARAMETER;

	if (!MessageQueue_Post(smartcard->IrpQueue, NULL, 0, (void*)irp, NULL))
	{
		WLog_ERR(TAG, "MessageQueue_Post failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/* smartcard is always built-in */
#define DeviceServiceEntry smartcard_DeviceServiceEntry

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
extern UINT DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints);
UINT DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	SMARTCARD_DEVICE* smartcard = NULL;
	size_t length;
	UINT error = CHANNEL_RC_NO_MEMORY;

	if (!sSmartcard)
	{
		smartcard = (SMARTCARD_DEVICE*)calloc(1, sizeof(SMARTCARD_DEVICE));

		if (!smartcard)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		smartcard->device.type = RDPDR_DTYP_SMARTCARD;
		smartcard->device.name = "SCARD";
		smartcard->device.IRPRequest = smartcard_irp_request;
		smartcard->device.Init = smartcard_init;
		smartcard->device.Free = smartcard_free;
		smartcard->rdpcontext = pEntryPoints->rdpcontext;
		length = strlen(smartcard->device.name);
		smartcard->device.data = Stream_New(NULL, length + 1);

		if (!smartcard->device.data)
		{
			WLog_ERR(TAG, "Stream_New failed!");
			goto fail;
		}

		Stream_Write(smartcard->device.data, "SCARD", 6);
		smartcard->IrpQueue = MessageQueue_New(NULL);

		if (!smartcard->IrpQueue)
		{
			WLog_ERR(TAG, "MessageQueue_New failed!");
			goto fail;
		}

		smartcard->rgOutstandingMessages = ListDictionary_New(TRUE);

		if (!smartcard->rgOutstandingMessages)
		{
			WLog_ERR(TAG, "ListDictionary_New failed!");
			goto fail;
		}

		smartcard->callctx = smartcard_call_context_new(smartcard->rdpcontext->settings);
		if (!smartcard->callctx)
			goto fail;

		if (!smarcard_call_set_callbacks(smartcard->callctx, smartcard, smartcard_context_new,
		                                 smartcard_context_free))
			goto fail;

		if ((error = pEntryPoints->RegisterDevice(pEntryPoints->devman, &smartcard->device)))
		{
			WLog_ERR(TAG, "RegisterDevice failed!");
			goto fail;
		}

		smartcard->thread =
		    CreateThread(NULL, 0, smartcard_thread_func, smartcard, CREATE_SUSPENDED, NULL);

		if (!smartcard->thread)
		{
			WLog_ERR(TAG, "ListDictionary_New failed!");
			error = ERROR_INTERNAL_ERROR;
			goto fail;
		}

		ResumeThread(smartcard->thread);
	}
	else
		smartcard = sSmartcard;

	if (pEntryPoints->device->Name)
	{
		smartcard_call_context_add(smartcard->callctx, pEntryPoints->device->Name);
	}

	sSmartcard = smartcard;
	return CHANNEL_RC_OK;
fail:
	smartcard_free_(smartcard);
	return error;
}

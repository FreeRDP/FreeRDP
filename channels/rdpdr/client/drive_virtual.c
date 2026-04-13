/**
 * Virtual drive device for Ambient directory sharing.
 *
 * Implements the API declared in freerdp/channels/drive_virtual.h.
 * Compiled as part of the rdpdr channel (static, built into freerdp-client3).
 *
 * Architecture:
 *   1. freerdp_wrapper.c calls virtual_drive_alloc() when the browser shares a
 *      directory, getting back an opaque handle and a pre-assigned device_id.
 *   2. Once rdpdr_is_ready() returns TRUE, virtual_drive_insert() + rdpdr_announce_devices()
 *      register the device with Windows.
 *   3. Windows sends IRPs; FreeRDP calls virtual_drive_irp_request() which stores
 *      the IRP and fires VirtualDriveIrpCallback into Go.
 *   4. Go decodes the IRP, forwards as TDP request to the browser, receives the
 *      TDP response, and calls virtual_drive_complete_irp() from a goroutine.
 *   5. virtual_drive_complete_irp() finds the stored IRP, writes the response
 *      payload, sets IoStatus, and calls irp->Complete() to send the RDP completion.
 */

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/synch.h>
#include <winpr/collections.h>

#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/drive_virtual.h>

#include "rdpdr_main.h"
#include "devman.h"

/* ─── global singleton set by rdpdr_main.c ──────────────────────────── */

rdpdrPlugin* g_rdpdr_plugin = NULL;

/* ─── auto-registered default drive ─────────────────────────────────── */

static VirtualDriveIrpCallback g_auto_irp_cb         = NULL;
static void*                   g_auto_userdata        = NULL;
static UINT32                  g_auto_device_id       = 0;
/* TRUE once the drive has been requested via rdpdr_announce_virtual_drive();
 * persists across re-handshakes so the drive is re-announced automatically. */
static BOOL                    g_drive_should_reannounce = FALSE;
/* Set to TRUE by rdpdr_announce_virtual_drive (Go goroutine); consumed by
 * rdpdr_virtual_check_pending (FreeRDP event loop thread). */
static volatile BOOL           g_drive_pending         = FALSE;

/* Smartcard always gets id=1 (first registered device).  Our virtual drive
 * always gets id=2 (second).  Deterministic because rdpdr_process_connect
 * registers smartcard first, and virtual_drive_new is the only other caller. */
#define RDPDR_VIRTUAL_DRIVE_EXPECTED_ID 2

void rdpdr_virtual_set_plugin(rdpdrPlugin* rdpdr)
{
	g_rdpdr_plugin = rdpdr;
	/* Each SERVER_ANNOUNCE triggers a new devman — invalidate the cached id */
	g_auto_device_id = 0;
}

void rdpdr_virtual_set_auto_drive(VirtualDriveIrpCallback cb, void* userdata)
{
	g_auto_irp_cb  = cb;
	g_auto_userdata = userdata;
	g_auto_device_id = 0;
	g_drive_should_reannounce = FALSE;
	g_drive_pending = FALSE;
	fprintf(stderr, "[vdrive] auto-drive callback registered (cb=%p userdata=%p) — state reset\n",
	        (void*)cb, userdata);
}

UINT32 rdpdr_virtual_get_auto_drive_id(void)
{
	return g_auto_device_id;
}

void rdpdr_virtual_register_auto_drive(void)
{
	if (!g_rdpdr_plugin || !g_rdpdr_plugin->devman || !g_auto_irp_cb)
	{
		fprintf(stderr, "[vdrive] register_auto_drive: skipped (plugin=%p devman=%p cb=%p)\n",
		        (void*)g_rdpdr_plugin,
		        g_rdpdr_plugin ? (void*)g_rdpdr_plugin->devman : NULL,
		        (void*)g_auto_irp_cb);
		return;
	}
	if (g_auto_device_id != 0)
	{
		fprintf(stderr, "[vdrive] register_auto_drive: already registered device_id=%u\n",
		        g_auto_device_id);
		return;
	}
	g_auto_device_id = virtual_drive_new(g_rdpdr_plugin->devman, "Ambient",
	                                      g_auto_irp_cb, g_auto_userdata);
	fprintf(stderr, "[vdrive] register_auto_drive: device_id=%u\n", g_auto_device_id);
}

/* ─── private virtual-drive type ────────────────────────────────────── */

typedef struct
{
	DEVICE device;                 /* must be first */

	VirtualDriveIrpCallback irp_cb;
	void*                   userdata;

	wListDictionary*        pending_irps; /* completion_id (as void*) -> IRP* */
} VIRTUAL_DRIVE;

/* ─── device callbacks ──────────────────────────────────────────────── */

static UINT virtual_drive_irp_request(DEVICE* device, IRP* irp)
{
	VIRTUAL_DRIVE* vd = (VIRTUAL_DRIVE*)device;

	if (!vd || !irp)
	{
		fprintf(stderr, "[vdrive] irp_request: NULL args\n");
		if (irp)
			irp->Discard(irp);
		return ERROR_INVALID_PARAMETER;
	}

	/* Store the IRP so virtual_drive_complete_irp can find it later.
	 * Offset by 1 because completion_id=0 would produce a NULL key,
	 * and ListDictionary cannot store/retrieve NULL keys. */
	void* key = (void*)(size_t)(irp->CompletionId + 1);
	ListDictionary_Add(vd->pending_irps, key, irp);

	/* Grab the remaining input bytes (after the common DR_DEVICE_IOREQUEST header,
	 * which FreeRDP's irp_new has already parsed) */
	size_t         input_len = Stream_GetRemainingLength(irp->input);
	const uint8_t* input     = Stream_Pointer(irp->input);

	fprintf(stderr, "[vdrive] IRP device_id=%u completion_id=%u major=0x%x minor=0x%x "
	        "file_id=%u input_len=%zu\n",
	        device->id, irp->CompletionId, irp->MajorFunction, irp->MinorFunction,
	        irp->FileId, input_len);

	/* Fire the Go callback — must return quickly (called from the FreeRDP event thread) */
	if (vd->irp_cb)
	{
		vd->irp_cb(vd->userdata,
		           device->id,
		           irp->CompletionId,
		           irp->MajorFunction,
		           irp->MinorFunction,
		           irp->FileId,
		           input,
		           (uint32_t)input_len);
	}
	else
	{
		fprintf(stderr, "[vdrive] no IRP callback set, discarding IRP completion_id=%u\n",
		        irp->CompletionId);
		ListDictionary_Take(vd->pending_irps, key);
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		irp->Complete(irp);
	}

	return CHANNEL_RC_OK;
}

static UINT virtual_drive_free(DEVICE* device)
{
	VIRTUAL_DRIVE* vd = (VIRTUAL_DRIVE*)device;

	if (!vd)
		return CHANNEL_RC_OK;

	fprintf(stderr, "[vdrive] freeing virtual drive device_id=%u\n", device->id);

	/* Discard any pending IRPs */
	if (vd->pending_irps)
	{
		ULONG_PTR* keys = NULL;
		ListDictionary_Lock(vd->pending_irps);
		size_t count = ListDictionary_GetKeys(vd->pending_irps, &keys);
		for (size_t i = 0; i < count; i++)
		{
			IRP* irp = ListDictionary_GetItemValue(vd->pending_irps, (void*)keys[i]);
			if (irp)
				irp->Discard(irp);
		}
		free(keys);
		ListDictionary_Unlock(vd->pending_irps);
		ListDictionary_Free(vd->pending_irps);
	}

	if (device->data)
		Stream_Release(device->data);

	free(vd);
	return CHANNEL_RC_OK;
}

/* ─── public API ─────────────────────────────────────────────────────── */

/**
 * Allocate a virtual drive device and assign a device ID, but do NOT yet
 * insert it into devman->devices.  The caller must call virtual_drive_insert()
 * from a context where the devman->devices lock is NOT held.
 */
void* virtual_drive_alloc(DEVMAN* devman, const char* name,
                           VirtualDriveIrpCallback irp_cb, void* userdata)
{
	if (!devman || !name)
	{
		fprintf(stderr, "[vdrive] virtual_drive_alloc: invalid args\n");
		return NULL;
	}

	VIRTUAL_DRIVE* vd = calloc(1, sizeof(VIRTUAL_DRIVE));
	if (!vd)
	{
		fprintf(stderr, "[vdrive] virtual_drive_alloc: OOM\n");
		return NULL;
	}

	/* Build the RDPDR device announce data stream.
	 * Must match FreeRDP's drive_main.c format: sanitised ASCII name + '\0',
	 * with Stream position left at the end so device_announce picks up the
	 * correct data_len via Stream_GetPosition(). */
	size_t name_len = strlen(name);
	if (name_len > 8)
		name_len = 8;

	vd->device.data = Stream_New(NULL, name_len + 1);
	if (!vd->device.data)
	{
		fprintf(stderr, "[vdrive] virtual_drive_alloc: Stream_New failed\n");
		free(vd);
		return NULL;
	}
	for (size_t i = 0; i < name_len; i++)
	{
		BYTE c = (BYTE)name[i];
		if (c <= 0x20 || c == '/' || c == '\\' || c == '|' || c == ':')
			c = '_';
		Stream_Write_UINT8(vd->device.data, c);
	}
	Stream_Write_UINT8(vd->device.data, '\0');
	/* Leave position at end — device_announce uses GetPosition for data_len */

	vd->device.type        = RDPDR_DTYP_FILESYSTEM;
	vd->device.name        = Stream_BufferAs(vd->device.data, char);
	vd->device.IRPRequest  = virtual_drive_irp_request;
	vd->device.Free        = virtual_drive_free;
	vd->irp_cb             = irp_cb;
	vd->userdata           = userdata;

	vd->pending_irps = ListDictionary_New(TRUE);
	if (!vd->pending_irps)
	{
		fprintf(stderr, "[vdrive] virtual_drive_alloc: ListDictionary_New failed\n");
		Stream_Release(vd->device.data);
		free(vd);
		return NULL;
	}

	/* Pre-assign a device ID from the sequence — same as devman_register_device */
	vd->device.id = devman->id_sequence++;

	fprintf(stderr, "[vdrive] allocated virtual drive name='%s' device_id=%u\n",
	        name, vd->device.id);
	return vd;
}

/**
 * Insert a previously alloc'd virtual drive into devman->devices.
 * Must be called from a thread that does NOT hold devman->devices lock.
 */
BOOL virtual_drive_insert(DEVMAN* devman, void* handle)
{
	VIRTUAL_DRIVE* vd = (VIRTUAL_DRIVE*)handle;

	if (!devman || !vd)
	{
		fprintf(stderr, "[vdrive] virtual_drive_insert: invalid args\n");
		return FALSE;
	}

	void* key = (void*)(size_t)vd->device.id;
	if (!ListDictionary_Add(devman->devices, key, &vd->device))
	{
		fprintf(stderr, "[vdrive] virtual_drive_insert: ListDictionary_Add FAILED for id=%u "
		        "(key already exists?)\n", vd->device.id);
		virtual_drive_free(&vd->device);
		return FALSE;
	}

	fprintf(stderr, "[vdrive] inserted virtual drive device_id=%u name='%s' into devman\n",
	        vd->device.id, vd->device.name);
	return TRUE;
}

/**
 * One-shot convenience: alloc + insert.
 * Returns the device ID on success, 0 on failure.
 */
UINT32 virtual_drive_new(DEVMAN* devman, const char* name,
                          VirtualDriveIrpCallback irp_cb, void* userdata)
{
	void* vd = virtual_drive_alloc(devman, name, irp_cb, userdata);
	if (!vd)
		return 0;

	UINT32 id = virtual_drive_get_id(vd);

	if (!virtual_drive_insert(devman, vd))
		return 0;

	return id;
}

UINT32 virtual_drive_get_id(void* handle)
{
	if (!handle)
		return 0;
	return ((VIRTUAL_DRIVE*)handle)->device.id;
}

/**
 * Complete a pending IRP from any thread (typically a Go goroutine).
 *
 * @param devman        DEVMAN pointer from rdpdr_get_devman().
 * @param device_id     Device ID returned by virtual_drive_alloc/new.
 * @param completion_id Completion ID from the IRP callback.
 * @param ntstatus      NTSTATUS result (0 = STATUS_SUCCESS).
 * @param output        Response payload bytes (may be NULL).
 * @param output_len    Byte count of output.
 */
void virtual_drive_complete_irp(DEVMAN* devman, UINT32 device_id, UINT32 completion_id,
                                  UINT32 ntstatus, const uint8_t* output, UINT32 output_len)
{
	if (!devman)
	{
		fprintf(stderr, "[vdrive] complete_irp: NULL devman (completion_id=%u)\n", completion_id);
		return;
	}

	VIRTUAL_DRIVE* vd = (VIRTUAL_DRIVE*)devman_get_device_by_id(devman, device_id);
	if (!vd)
	{
		fprintf(stderr, "[vdrive] complete_irp: device_id=%u not found\n", device_id);
		return;
	}

	void* key = (void*)(size_t)(completion_id + 1);
	IRP* irp  = ListDictionary_Take(vd->pending_irps, key);
	if (!irp)
	{
		fprintf(stderr, "[vdrive] complete_irp: completion_id=%u not found in pending_irps "
		        "(device_id=%u)\n", completion_id, device_id);
		return;
	}

	fprintf(stderr, "[vdrive] completing IRP device_id=%u completion_id=%u "
	        "ntstatus=0x%08x output_len=%u\n",
	        device_id, completion_id, ntstatus, output_len);

	/* Append the response payload after the pre-written IoCompletion header */
	if (output && output_len > 0)
	{
		if (!Stream_EnsureRemainingCapacity(irp->output, output_len))
		{
			fprintf(stderr, "[vdrive] complete_irp: stream resize failed, discarding IRP\n");
			irp->Discard(irp);
			return;
		}
		Stream_Write(irp->output, output, output_len);
	}
	Stream_SealLength(irp->output);

	irp->IoStatus = (NTSTATUS)ntstatus;
	irp->Complete(irp);  /* sends PAKID_CORE_DEVICE_IOCOMPLETION to Windows */
}

/* ─── channel-state query functions ────────────────────────────────── */

/* Helper: runs on the FreeRDP event loop thread — registers the drive in
 * devman and sends a hotplug PAKID_CORE_DEVICELIST_ANNOUNCE.
 * Called from on_ready and check_pending (both are on the event-loop thread),
 * so no cross-thread ListDictionary contention. */
static void do_register_and_announce(void)
{
	if (!g_auto_irp_cb || !g_rdpdr_plugin || !g_rdpdr_plugin->devman)
		return;

	if (g_auto_device_id == 0)
	{
		g_auto_device_id = virtual_drive_new(g_rdpdr_plugin->devman, "Ambient",
		                                      g_auto_irp_cb, g_auto_userdata);
		fprintf(stderr, "[vdrive] do_register_and_announce: registered drive device_id=%u\n",
		        g_auto_device_id);
	}

	if (g_auto_device_id != 0)
	{
		fprintf(stderr, "[vdrive] do_register_and_announce: hotplug-announcing drive device_id=%u\n",
		        g_auto_device_id);
		rdpdr_try_send_device_list_announce_request(g_rdpdr_plugin);
	}
}

void rdpdr_virtual_on_ready(void)
{
	fprintf(stderr, "[vdrive] rdpdr_virtual_on_ready: READY (auto_device_id=%u reannounce=%d pending=%d)\n",
	        g_auto_device_id, (int)g_drive_should_reannounce, (int)g_drive_pending);

	/* Handle pending request from Go goroutine — the goroutine set g_drive_pending
	 * while the channel was mid-handshake and check_pending couldn't run yet. */
	if (g_drive_pending)
	{
		g_drive_pending = FALSE;
		do_register_and_announce();
		return;
	}

	if (!g_drive_should_reannounce)
		return;

	do_register_and_announce();
}

/**
 * Called from the FreeRDP event loop (rdp_iteration) to check if the Go side
 * has requested a drive announcement.  This handles the case where the user
 * shares a directory while the channel is already in the READY state and
 * on_ready has already fired.
 *
 * Thread-safe because it only touches devman from the FreeRDP event loop.
 */
void rdpdr_virtual_check_pending(void)
{
	if (!g_drive_pending)
		return;

	if (!rdpdr_is_ready())
		return;

	g_drive_pending = FALSE;
	fprintf(stderr, "[vdrive] rdpdr_virtual_check_pending: channel READY, registering+announcing\n");
	do_register_and_announce();
}

/**
 * Request that the virtual drive be announced to Windows.
 *
 * THREAD SAFETY: This function is called from the Go TDP goroutine, NOT from
 * the FreeRDP event loop thread.  It MUST NOT touch devman or any locked data
 * structures — it only sets atomic-safe globals (single-word writes) and a
 * flag that the FreeRDP event loop thread will pick up.
 *
 * The actual devman registration and PAKID_CORE_DEVICELIST_ANNOUNCE happen
 * on the FreeRDP event loop thread (via on_ready or check_pending).
 */
UINT32 rdpdr_announce_virtual_drive(VirtualDriveIrpCallback cb, void* userdata)
{
	if (!cb)
	{
		fprintf(stderr, "[vdrive] rdpdr_announce_virtual_drive: NULL callback\n");
		return 0;
	}

	g_auto_irp_cb  = cb;
	g_auto_userdata = userdata;
	g_drive_should_reannounce = TRUE;
	g_drive_pending = TRUE;

	fprintf(stderr, "[vdrive] rdpdr_announce_virtual_drive: drive requested, "
	        "pending for event loop\n");

	/* We cannot know the exact device_id yet (it's assigned by devman on the
	 * event-loop thread), but we know smartcard is always id=1 and our drive
	 * will be id=2 (deterministic sequence).  Return 2 so Go can set up its
	 * mapping immediately and send a success ACK to the browser. */
	return RDPDR_VIRTUAL_DRIVE_EXPECTED_ID;
}

DEVMAN* rdpdr_get_devman(void)
{
	if (!g_rdpdr_plugin)
		return NULL;
	return g_rdpdr_plugin->devman;
}

BOOL rdpdr_is_ready(void)
{
	if (!g_rdpdr_plugin)
		return FALSE;
	enum RDPDR_CHANNEL_STATE st = g_rdpdr_plugin->state;
	return (st == RDPDR_CHANNEL_STATE_READY || st == RDPDR_CHANNEL_STATE_USER_LOGGEDON);
}

UINT rdpdr_announce_devices(void)
{
	if (!g_rdpdr_plugin)
	{
		fprintf(stderr, "[vdrive] rdpdr_announce_devices: no rdpdr plugin\n");
		return ERROR_NOT_READY;
	}
	if (!rdpdr_is_ready())
	{
		fprintf(stderr, "[vdrive] rdpdr_announce_devices: channel not ready (state=%s)\n",
		        rdpdr_debug_channel_state_name());
		return ERROR_NOT_READY;
	}
	fprintf(stderr, "[vdrive] rdpdr_announce_devices: triggering device list announce\n");
	return rdpdr_try_send_device_list_announce_request(g_rdpdr_plugin);
}

const char* rdpdr_debug_channel_state_name(void)
{
	if (!g_rdpdr_plugin)
		return "NO_PLUGIN";
	switch (g_rdpdr_plugin->state)
	{
		case RDPDR_CHANNEL_STATE_INITIAL:        return "INITIAL";
		case RDPDR_CHANNEL_STATE_ANNOUNCE:       return "ANNOUNCE";
		case RDPDR_CHANNEL_STATE_ANNOUNCE_REPLY: return "ANNOUNCE_REPLY";
		case RDPDR_CHANNEL_STATE_NAME_REQUEST:   return "NAME_REQUEST";
		case RDPDR_CHANNEL_STATE_SERVER_CAPS:    return "SERVER_CAPS";
		case RDPDR_CHANNEL_STATE_CLIENT_CAPS:    return "CLIENT_CAPS";
		case RDPDR_CHANNEL_STATE_CLIENTID_CONFIRM: return "CLIENTID_CONFIRM";
		case RDPDR_CHANNEL_STATE_READY:          return "READY";
		case RDPDR_CHANNEL_STATE_USER_LOGGEDON:  return "USER_LOGGEDON";
		default:                                 return "UNKNOWN";
	}
}

/** FreeRDP: A Remote Desktop Protocol Implementation */
#include <freerdp/config.h>

#include <stdlib.h>

#include <winpr/synch.h>

#include <freerdp/client/printer.h>

typedef struct s_printer_registry_entry
{
	const rdpContext* context;
	UINT32 deviceId;
	struct s_printer_registry_entry* next;
} PRINTER_REGISTRY_ENTRY;

static INIT_ONCE printer_registry_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION printer_registry_lock;
static PRINTER_REGISTRY_ENTRY* printer_registry = nullptr;

static BOOL CALLBACK printer_registry_init(WINPR_ATTR_UNUSED PINIT_ONCE once,
	                                          WINPR_ATTR_UNUSED PVOID parameter,
	                                          WINPR_ATTR_UNUSED PVOID* context)
{
	InitializeCriticalSection(&printer_registry_lock);
	return TRUE;
}

static BOOL printer_registry_ensure_initialized(void)
{
	return InitOnceExecuteOnce(&printer_registry_once, printer_registry_init, nullptr, nullptr);
}

BOOL freerdp_printer_device_register(const rdpContext* context, UINT32 deviceId)
{
	PRINTER_REGISTRY_ENTRY* entry = nullptr;

	if (!context || (deviceId == 0) || !printer_registry_ensure_initialized())
		return FALSE;
	entry = calloc(1, sizeof(*entry));
	if (!entry)
		return FALSE;
	entry->context = context;
	entry->deviceId = deviceId;
	EnterCriticalSection(&printer_registry_lock);
	entry->next = printer_registry;
	printer_registry = entry;
	LeaveCriticalSection(&printer_registry_lock);
	return TRUE;
}

void freerdp_printer_device_unregister(const rdpContext* context, UINT32 deviceId)
{
	if (!context || (deviceId == 0) || !printer_registry_ensure_initialized())
		return;
	EnterCriticalSection(&printer_registry_lock);
	PRINTER_REGISTRY_ENTRY** current = &printer_registry;
	while (*current)
	{
		if (((*current)->context == context) && ((*current)->deviceId == deviceId))
		{
			PRINTER_REGISTRY_ENTRY* entry = *current;
			*current = entry->next;
			free(entry);
			break;
		}
		current = &(*current)->next;
	}
	LeaveCriticalSection(&printer_registry_lock);
}

BOOL freerdp_printer_device_exists(const rdpContext* context, UINT32 deviceId)
{
	BOOL found = FALSE;

	if (!context || (deviceId == 0) || !printer_registry_ensure_initialized())
		return FALSE;
	EnterCriticalSection(&printer_registry_lock);
	for (const PRINTER_REGISTRY_ENTRY* entry = printer_registry; entry; entry = entry->next)
	{
		if ((entry->context == context) && (entry->deviceId == deviceId))
		{
			found = TRUE;
			break;
		}
	}
	LeaveCriticalSection(&printer_registry_lock);
	return found;
}

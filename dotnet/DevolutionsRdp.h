#ifndef CS_DEVOLUTIONSRDP_H_
#define CS_DEVOLUTIONSRDP_H_

#include "../../include/freerdp/api.h"
#include <freerdp/freerdp.h>
#include <winpr/clipboard.h>
#include <freerdp/client/cliprdr.h>

typedef void (*fnRegionUpdated)(void* rdp, int x, int y, int width, int height);
typedef void* (*fnDesktopSizeChanged)(void* rdp, int width, int height);
typedef void (*fnOnError)(void* context, int code);
typedef void (*fnOnClipboardUpdate)(void* context, byte* text, int length);
typedef void (*fnOnNewCursor)(void* context, UINT32 id, BYTE* data, UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 hotX, UINT32 hotY);
typedef BYTE* (*fnOnFreeCursor)(void* context, void* pointer);
typedef void (*fnOnSetCursor)(void* context, void* pointer);
typedef void (*fnOnDefaultCursor)(void* context);

typedef struct csharp_context
{
	rdpContext _p;
    
	void* buffer;
	
	fnRegionUpdated regionUpdated;
	fnDesktopSizeChanged desktopSizeChanged;
	fnOnClipboardUpdate onClipboardUpdate;
	fnOnNewCursor onNewCursor;
	fnOnFreeCursor onFreeCursor;
	fnOnSetCursor onSetCursor;
	fnOnDefaultCursor onDefaultCursor;
	fnOnError onError;
	
	BOOL clipboardSync;
	wClipboard* clipboard;
	UINT32 numServerFormats;
	UINT32 requestedFormatId;
	HANDLE clipboardRequestEvent;
	CLIPRDR_FORMAT* serverFormats;
	CliprdrClientContext* cliprdr;
	UINT32 clipboardCapabilities;
} csContext;

FREERDP_API void* csharp_freerdp_new();
FREERDP_API void csharp_freerdp_free(void* instance);
FREERDP_API BOOL csharp_freerdp_connect(void* instance);
FREERDP_API BOOL csharp_freerdp_disconnect(void* instance);
FREERDP_API void csharp_freerdp_set_on_region_updated(void* instance, fnRegionUpdated fn);
FREERDP_API void csharp_freerdp_set_on_desktop_size_changed(void* instance, fnDesktopSizeChanged fn);
FREERDP_API BOOL csharp_freerdp_set_console_mode(void* instance, BOOL useConsoleMode, BOOL useRestrictedAdminMode);
FREERDP_API BOOL csharp_freerdp_set_redirect_clipboard(void* instance, BOOL redirectClipboard);
FREERDP_API BOOL csharp_freerdp_set_connection_info(void* instance, const char* hostname, const char* username, const char* password, const char* domain, UINT32 width, UINT32 height, UINT32 color_depth, UINT32 port, int codecLevel, int security);
FREERDP_API BOOL csharp_freerdp_set_gateway_settings(void* instance, const char* hostname, UINT32 port, const char* username, const char* password, const char* domain, BOOL bypassLocal);
FREERDP_API BOOL csharp_freerdp_set_data_directory(void* instance, const char* directory);
FREERDP_API void csharp_freerdp_set_load_balance_info(void* instance, const char* info);
FREERDP_API BOOL csharp_freerdp_set_scale_factor(void* instance, UINT32 desktopScaleFactor, UINT32 deviceScaleFactor);
FREERDP_API BOOL csharp_freerdp_set_performance_flags(void* instance,
							   BOOL disableWallpaper,
							   BOOL allowFontSmoothing,
							   BOOL allowDesktopComposition,
							   BOOL bitmapCacheEnabled,
							   BOOL disableFullWindowDrag,
							   BOOL disableMenuAnims,
							   BOOL disableThemes);
FREERDP_API BOOL csharp_shall_disconnect(void* instance);
FREERDP_API BOOL csharp_waitforsingleobject(void* instance);
FREERDP_API BOOL csharp_check_event_handles(void* instance, void* buffer);

FREERDP_API void csharp_freerdp_send_clipboard_data(void* instance, BYTE* data, int length);
FREERDP_API void csharp_freerdp_send_cursor_event(void* instance, int x, int y, int flags);
FREERDP_API void csharp_freerdp_send_input(void* instance, int keycode, BOOL down);
FREERDP_API void csharp_freerdp_send_unicode(void* instance, int character);
FREERDP_API DWORD csharp_get_vk_from_keycode(DWORD keycode, DWORD flags);
FREERDP_API DWORD csharp_get_scancode_from_vk(DWORD keycode, DWORD flags);
FREERDP_API void csharp_freerdp_send_vkcode(void* instance, int vkcode, BOOL down);
FREERDP_API void csharp_freerdp_send_scancode(void* instance, int flags, DWORD scancode);
FREERDP_API void csharp_set_log_output(const char* path, const char* name);
FREERDP_API void csharp_freerdp_set_hyperv_info(void* instance, char* pcb);
FREERDP_API void csharp_freerdp_set_keyboard_layout(void* instance, int layoutID);
FREERDP_API void csharp_freerdp_set_smart_sizing(void* instance, bool smartSizing);
FREERDP_API void csharp_freerdp_sync_toggle_keys(void* instance);

FREERDP_API void csharp_set_on_authenticate(void* instance, pAuthenticate fn);
FREERDP_API void csharp_set_on_clipboard_update(void* instance, fnOnClipboardUpdate fn);
FREERDP_API void csharp_set_on_gateway_authenticate(void* instance, pAuthenticate fn);
FREERDP_API void csharp_set_on_verify_certificate(void* instance, pVerifyCertificate fn);
FREERDP_API void csharp_set_on_verify_x509_certificate(void* instance, pVerifyX509Certificate fn);
FREERDP_API void csharp_set_on_error(void* instance, fnOnError fn);
FREERDP_API void csharp_set_on_cursor_notifications(void* instance, fnOnNewCursor newCursor, fnOnFreeCursor freeCursor, fnOnSetCursor setCursor, fnOnDefaultCursor defaultCursor);
FREERDP_API const char* csharp_get_error_info_string(int code);
FREERDP_API int csharp_get_last_error(void* instance);

FREERDP_API void csharp_freerdp_redirect_drive(void* instance, char* name, char* path);
FREERDP_API void csharp_freerdp_set_redirect_all_drives(void* instance, BOOL redirect);
FREERDP_API void csharp_freerdp_set_redirect_home_drive(void* instance, BOOL redirect);
FREERDP_API BOOL csharp_freerdp_set_redirect_audio(void* instance, int redirectSound, BOOL redirectCapture);

#endif /* CS_DEVOLUTIONSRDP_H_ */

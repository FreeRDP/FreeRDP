#include <freerdp/channels/channels.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/event.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>
#include <assert.h>
#include <ctype.h>
#include <freerdp/log.h>
#include <winpr/environment.h>

#include "devolutionsrdp.h"
#include "clipboard.h"
#include "cursor.h"

#define TAG "devolutionsrdp"

// PRIVATE FUNCTIONS DECLARATIONS
/////////////////////////////////
void cs_error_info(void* ctx, ErrorInfoEventArgs* e);
static BOOL cs_pre_connect(freerdp* instance);
static BOOL cs_post_connect(freerdp* instance);
static void cs_post_disconnect(freerdp* instance);
static BOOL cs_authenticate(freerdp* instance, char** username, char** password, char** domain);
static BOOL cs_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint);
static int cs_verify_x509_certificate(freerdp* instance, BYTE* data, int length, const char* hostname, int port, DWORD flags);
/////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////
//// CALLBACKS
////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int cs_get_vk_code(int character)
{
    int _virtual_key_map[256] = {0};
    
    _virtual_key_map['0'] = VK_KEY_0;
    _virtual_key_map['1'] = VK_KEY_1;
    _virtual_key_map['2'] = VK_KEY_2;
    _virtual_key_map['3'] = VK_KEY_3;
    _virtual_key_map['4'] = VK_KEY_4;
    _virtual_key_map['5'] = VK_KEY_5;
    _virtual_key_map['6'] = VK_KEY_6;
    _virtual_key_map['7'] = VK_KEY_7;
    _virtual_key_map['8'] = VK_KEY_8;
    _virtual_key_map['9'] = VK_KEY_9;
    
    _virtual_key_map['a'] = VK_KEY_A;
    _virtual_key_map['b'] = VK_KEY_B;
    _virtual_key_map['c'] = VK_KEY_C;
    _virtual_key_map['d'] = VK_KEY_D;
    _virtual_key_map['e'] = VK_KEY_E;
    _virtual_key_map['f'] = VK_KEY_F;
    _virtual_key_map['g'] = VK_KEY_G;
    _virtual_key_map['h'] = VK_KEY_H;
    _virtual_key_map['i'] = VK_KEY_I;
    _virtual_key_map['j'] = VK_KEY_J;
    _virtual_key_map['k'] = VK_KEY_K;
    _virtual_key_map['l'] = VK_KEY_L;
    _virtual_key_map['m'] = VK_KEY_M;
    _virtual_key_map['n'] = VK_KEY_N;
    _virtual_key_map['o'] = VK_KEY_O;
    _virtual_key_map['p'] = VK_KEY_P;
    _virtual_key_map['q'] = VK_KEY_Q;
    _virtual_key_map['r'] = VK_KEY_R;
    _virtual_key_map['s'] = VK_KEY_S;
    _virtual_key_map['t'] = VK_KEY_T;
    _virtual_key_map['u'] = VK_KEY_U;
    _virtual_key_map['v'] = VK_KEY_V;
    _virtual_key_map['w'] = VK_KEY_W;
    _virtual_key_map['x'] = VK_KEY_X;
    _virtual_key_map['y'] = VK_KEY_Y;
    _virtual_key_map['z'] = VK_KEY_Z;
    
    return _virtual_key_map[character];
}

static int cs_get_unicode(int character)
{
    int _unicode_map[256] = {0};
    
    _unicode_map['-'] = 45;
    _unicode_map['/'] = 47;
    _unicode_map[':'] = 58;
    _unicode_map[';'] = 59;
    _unicode_map['('] = 40;
    _unicode_map[')'] = 41;
    _unicode_map['&'] = 38;
    _unicode_map['@'] = 64;
    _unicode_map['.'] = 46;
    _unicode_map[','] = 44;
    _unicode_map['?'] = 63;
    _unicode_map['!'] = 33;
    _unicode_map['\''] = 39;
    _unicode_map['\"'] = 34;
    
    _unicode_map['['] = 91;
    _unicode_map[']'] = 93;
    _unicode_map['{'] = 123;
    _unicode_map['}'] = 125;
    _unicode_map['#'] = 35;
    _unicode_map['%'] = 37;
    _unicode_map['^'] = 94;
    _unicode_map['*'] = 42;
    _unicode_map['+'] = 43;
    _unicode_map['='] = 61;
    
    _unicode_map['_'] = 95;
    _unicode_map['\\'] = 92;
    _unicode_map['|'] = 124;
    _unicode_map['~'] = 126;
    _unicode_map['<'] = 60;
    _unicode_map['>'] = 62;
    _unicode_map['$'] = 36;
    
    return _unicode_map[character];
}

static void cs_send_virtual_key(freerdp* instance, int vk, BOOL down)
{
    int flags;
    DWORD scancode;
    
    scancode = GetVirtualScanCodeFromVirtualKeyCode(vk, 4);
    flags = (down ? KBD_FLAGS_DOWN : KBD_FLAGS_RELEASE);
    flags |= ((scancode & KBDEXT) ? KBD_FLAGS_EXTENDED : 0);
    freerdp_input_send_keyboard_event(instance->input, flags, scancode);
}

static void cs_send_unicode_key(freerdp* instance, int vk)
{
    freerdp_input_send_unicode_keyboard_event(instance->input, 0, vk);
}

void cs_OnChannelConnectedEventHandler(rdpContext* context, ChannelConnectedEventArgs* e)
{
	csContext* csc = (csContext*)context->instance->context;

    if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
    {
        gdi_graphics_pipeline_init(context->gdi, (RdpgfxClientContext*) e->pInterface);
    }
    else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		cs_cliprdr_init(csc, (CliprdrClientContext*) e->pInterface);
	}
}

void cs_OnChannelDisconnectedEventHandler(rdpContext* context, ChannelDisconnectedEventArgs* e)
{
	csContext* csc = (csContext*)context->instance->context;

    if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
    {
        gdi_graphics_pipeline_uninit(context->gdi, (RdpgfxClientContext*) e->pInterface);
    }
    else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		cs_cliprdr_uninit(csc, (CliprdrClientContext*) e->pInterface);
	}
}

static BOOL cs_context_new(freerdp* instance, rdpContext* context)
{
	return TRUE;
}

static void cs_context_free(freerdp* instance, rdpContext* context)
{
}

static BOOL cs_pre_connect(freerdp* instance)
{
    rdpContext* context = instance->context;
	rdpSettings* settings = instance->settings;
	BOOL bitmap_cache = settings->BitmapCacheEnabled;
    
    ZeroMemory(settings->OrderSupport, 32);
	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

//	settings->FrameAcknowledge = 10;
    PubSub_SubscribeChannelConnected(context->pubSub,
                                     (pChannelConnectedEventHandler) cs_OnChannelConnectedEventHandler);

    PubSub_SubscribeChannelDisconnected(context->pubSub,
                                        (pChannelDisconnectedEventHandler) cs_OnChannelDisconnectedEventHandler);

    if (!freerdp_client_load_addins(context->channels, instance->settings))
            return -1;

    if (!context->cache)
    {
        if (!(context->cache = cache_new(settings)))
            return FALSE;
    }

	return TRUE;
}

BOOL cs_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
    
	gdi->primary->hdc->hwnd->invalid->null = 1;
	return TRUE;
}

BOOL cs_end_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	csContext* csc = (csContext*)context->instance->context;
	INT32 x, y;
	UINT32 w, h;
    
    if (gdi->primary->hdc->hwnd->invalid->null)
        return TRUE;

    x = gdi->primary->hdc->hwnd->invalid->x;
	y = gdi->primary->hdc->hwnd->invalid->y;
	w = gdi->primary->hdc->hwnd->invalid->w;
	h = gdi->primary->hdc->hwnd->invalid->h;

    freerdp_image_copy(csc->buffer, PIXEL_FORMAT_BGRX32, gdi->width * 4, x, y, w, h,
                       gdi->primary_buffer, gdi->dstFormat, gdi->stride, 
                       x, y, NULL, FREERDP_FLIP_NONE);

	if (csc->regionUpdated)
	{
		csc->regionUpdated(context->instance, gdi->primary->hdc->hwnd->invalid->x, gdi->primary->hdc->hwnd->invalid->y, gdi->primary->hdc->hwnd->invalid->w, gdi->primary->hdc->hwnd->invalid->h);
	}

	return TRUE;
}

static BOOL cs_post_connect(freerdp* instance)
{
	UINT32 gdi_format;
	rdpUpdate* update;
	rdpContext* context = instance->context;

	update = instance->context->update;
    
	assert(instance);

	if (instance->settings->ColorDepth > 16)
		gdi_format = PIXEL_FORMAT_BGRX32;
	else
		gdi_format = PIXEL_FORMAT_BGR16;

	if (!gdi_init(instance, gdi_format))
		return FALSE;

	update->BeginPaint = cs_begin_paint;
	update->EndPaint = cs_end_paint;
    
	pointer_cache_register_callbacks(update);
	cs_register_pointer(context);

	return TRUE;
}

static void cs_post_disconnect(freerdp* instance)
{
    rdpContext* context = instance->context;
    
	gdi_free(instance);
    
    if (context->cache)
    {
        cache_free(context->cache);
        context->cache = NULL;
    }
}

static BOOL cs_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	return TRUE;
}

static BOOL cs_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
	return TRUE;
}

static int cs_verify_x509_certificate(freerdp* instance, BYTE* data, int length, const char* hostname, int port, DWORD flags)
{
	return 1;
}

void cs_error_info(void* ctx, ErrorInfoEventArgs* e)
{
    rdpContext* context = (rdpContext*) ctx;
    csContext* csc = (csContext*)context->instance->context;
    
    if (csc->onError)
    {
        csc->onError(context->instance, e->code);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////
//// EXPORTED FUNCTIONS
////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void* csharp_freerdp_new()
{
#ifdef WIN32
	WSADATA wsaData;
#endif
	freerdp* instance;

	// create instance
	if (!(instance = freerdp_new()))
		return NULL;
	
	instance->PreConnect = cs_pre_connect;
	instance->PostConnect = cs_post_connect;
	instance->PostDisconnect = cs_post_disconnect;
	instance->Authenticate = cs_authenticate;
	instance->VerifyCertificate = cs_verify_certificate;
	instance->VerifyX509Certificate = cs_verify_x509_certificate;
	//instance->VerifyChangedCertificate = android_verify_changed_certificate;

	// create context
	instance->ContextSize = sizeof(csContext);
	instance->ContextNew = cs_context_new;
	instance->ContextFree = cs_context_free;
    
	if (!freerdp_context_new(instance))
	{
		freerdp_free(instance);
		instance = NULL;
		goto end;
	}
	else
	{
		PubSub_SubscribeErrorInfo(instance->context->pubSub, cs_error_info);
	}

	if (freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0) != CHANNEL_RC_OK)
	{
		instance = NULL;
		goto end;
	}

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
#ifdef WIN32
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
end:
	return instance;
}

void csharp_freerdp_free(void* instance)
{
	freerdp* inst = (freerdp*)instance;

	freerdp_client_context_free(inst->context);
}

BOOL csharp_freerdp_connect(void* instance)
{
    return freerdp_connect((freerdp*)instance);
}

BOOL csharp_freerdp_disconnect(void* instance)
{
    freerdp* inst = (freerdp*)instance;

	freerdp_disconnect(inst);

    if (freerdp_client_stop(inst->context) != CHANNEL_RC_OK)
        return FALSE;

	return TRUE;
}

void csharp_freerdp_set_on_region_updated(void* instance, fnRegionUpdated fn)
{
    freerdp* inst = (freerdp*)instance;
    csContext* ctxt = (csContext*)inst->context;
	
	ctxt->regionUpdated = fn;
}

BOOL csharp_freerdp_set_gateway_settings(void* instance, const char* hostname, UINT32 port, const char* username, const char* password, const char* domain, BOOL bypassLocal)
{
    freerdp* inst = (freerdp*)instance;
    rdpSettings* settings = inst->settings;
    
    settings->GatewayPort     = port;
    settings->GatewayEnabled = TRUE;
    settings->GatewayUseSameCredentials = FALSE;
    settings->GatewayHostname = strdup(hostname);
    settings->GatewayUsername = strdup(username);
    settings->GatewayPassword = strdup(password);
    settings->GatewayDomain = strdup(domain);
    settings->GatewayBypassLocal = bypassLocal;
    settings->GatewayHttpTransport = TRUE;
    settings->GatewayRpcTransport = TRUE;
    settings->CredentialsFromStdin = FALSE;

    freerdp_update_gateway_usage_method(settings, TRUE, bypassLocal);
    
    if (!settings->GatewayHostname || !settings->GatewayUsername ||
        !settings->GatewayPassword || !settings->GatewayDomain)
    {
        return FALSE;
    }
    
    return TRUE;
}

BOOL csharp_freerdp_set_console_mode(void* instance, BOOL useConsoleMode, BOOL useRestrictedAdminMode)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	settings->ConsoleSession = useConsoleMode;
	settings->RestrictedAdminModeRequired = useRestrictedAdminMode;

	return TRUE;
}

BOOL csharp_freerdp_set_redirect_clipboard(void* instance, BOOL redirectClipboard)
{
    freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

    settings->RedirectClipboard = redirectClipboard;

    return TRUE;
}

BOOL csharp_freerdp_set_redirect_audio(void* instance, int redirectSound, BOOL redirectCapture)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	
	settings->AudioCapture = redirectCapture;
	
	settings->AudioPlayback = redirectSound == 0;
	settings->RemoteConsoleAudio = redirectSound == 2;
	
	
	return TRUE;
}

BOOL csharp_freerdp_set_connection_info(void* instance, const char* hostname, const char* username, const char* password,                       const char* domain, UINT32 width, UINT32 height, UINT32 color_depth, UINT32 port, int codecLevel, int security)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	settings->DesktopWidth = width;
	settings->DesktopHeight = height;
	settings->ColorDepth = color_depth;
	settings->ServerPort = port;
	settings->ExternalCertificateManagement = TRUE;

	// Hack for 16 bit RDVH connections:
	//   In this case we get screen corruptions when we have an odd screen resolution width ... need to investigate what is causing this...
	if (color_depth <= 16)
		settings->DesktopWidth &= (~1);

	if (!(settings->ServerHostname = strdup(hostname)))
		goto out_fail_strdup;

	if (username && strlen(username) > 0)
	{
		if (!(settings->Username = strdup(username)))
			goto out_fail_strdup;
	}

	if (password && strlen(password) > 0)
	{
		if (!(settings->Password = strdup(password)))
			goto out_fail_strdup;
		settings->AutoLogonEnabled = TRUE;
	}

	if (!(settings->Domain = strdup(domain)))
		goto out_fail_strdup;

	settings->SoftwareGdi = TRUE;
    settings->AllowFontSmoothing = TRUE;
    
    if (codecLevel >= 7)
    {
        settings->RemoteFxCodec = TRUE;
        settings->FastPathOutput = TRUE;
        settings->LargePointerFlag = TRUE;
        settings->FrameMarkerCommandEnabled = TRUE;
        settings->ColorDepth = 32;
    }
    
    if(codecLevel >= 8)
    {
        settings->SupportGraphicsPipeline = TRUE;
    }

	switch (security)
	{
		case 1:
			/* Standard RDP */
			settings->RdpSecurity = TRUE;
			settings->TlsSecurity = FALSE;
			settings->NlaSecurity = FALSE;
			settings->ExtSecurity = FALSE;
			settings->UseRdpSecurityLayer = TRUE;
			break;

		case 2:
			/* TLS */
			settings->NlaSecurity = FALSE;
			settings->TlsSecurity = TRUE;
			settings->RdpSecurity = FALSE;
			settings->ExtSecurity = FALSE;
			break;

		case 3:
			/* NLA */
			settings->NlaSecurity = TRUE;
			settings->TlsSecurity = FALSE;
			settings->RdpSecurity = FALSE;
			settings->ExtSecurity = FALSE;
			break;

		default:
			break;
	}

	// set US keyboard layout
	settings->KeyboardLayout = 0x0409;

	return TRUE;

	out_fail_strdup:
	return FALSE;
}

void csharp_freerdp_set_hyperv_info(void* instance, char* pcb)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	
	settings->PreconnectionBlob = _strdup(pcb);
	settings->VmConnectMode = TRUE;
	settings->SendPreconnectionPdu = TRUE;
	settings->NlaSecurity = TRUE;
	settings->NegotiateSecurityLayer = FALSE;
}

void csharp_freerdp_set_keyboard_layout(void* instance, int layoutID)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	
	settings->KeyboardLayout = layoutID;
}

void csharp_freerdp_set_redirect_all_drives(void* instance, BOOL redirect)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	
	settings->RedirectDrives = redirect;
}

void csharp_freerdp_set_redirect_home_drive(void* instance, BOOL redirect)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	
	settings->RedirectHomeDrive = redirect;
}

BOOL csharp_freerdp_set_data_directory(void* instance, const char* directory)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	free(settings->HomePath);
	free(settings->ConfigPath);
	settings->HomePath = settings->ConfigPath = NULL;

	int config_dir_len = strlen(directory) + 10; /* +9 chars for /.freerdp and +1 for \0 */
	char* config_dir_buf = (char*)malloc(config_dir_len);
	if (!config_dir_buf)
		goto out_malloc_fail;

	strcpy(config_dir_buf, directory);
	strcat(config_dir_buf, "/.freerdp");
	settings->HomePath = strdup(directory);
	if (!settings->HomePath)
		goto out_strdup_fail;
	settings->ConfigPath = config_dir_buf;	/* will be freed by freerdp library */

	return TRUE;

	out_strdup_fail:
	free(config_dir_buf);
	out_malloc_fail:
	return FALSE;
}

BOOL csharp_freerdp_set_scale_factor(void* instance, UINT32 desktopScaleFactor, UINT32 deviceScaleFactor)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	
	settings->DesktopScaleFactor = desktopScaleFactor;
	settings->DeviceScaleFactor = deviceScaleFactor;
	
	return TRUE;
}

BOOL csharp_shall_disconnect(void* instance)
{
	return freerdp_shall_disconnect((freerdp*)instance);
}

BOOL csharp_waitforsingleobject(void* instance)
{
	freerdp* inst = (freerdp*)instance;
	HANDLE handles[64];
	DWORD nCount;
	DWORD status;

	nCount = freerdp_get_event_handles(inst->context, &handles[0], 64);

	if (nCount == 0)
		return FALSE;

	status = WaitForMultipleObjects(nCount, handles, FALSE, 100);

	if (status == WAIT_FAILED)
		return FALSE;

	return TRUE;
}

BOOL csharp_check_event_handles(void* instance, void* buffer)
{
    int result;
	freerdp* inst = (freerdp*)instance;
	csContext* ctxt = (csContext*)inst->context;

    ctxt->buffer = buffer;
	
	result = freerdp_check_event_handles(inst->context);
    
	return result;
}

void csharp_freerdp_send_unicode(void* instance, int character)
{
    cs_send_unicode_key((freerdp*)instance, character);
}

void csharp_freerdp_send_vkcode(void* instance, int vkcode, BOOL down)
{
    cs_send_virtual_key((freerdp*)instance, vkcode, down);
}

void csharp_freerdp_send_input(void* instance, int character, BOOL down)
{
    BOOL shift_was_sent = FALSE;
    
    // Send as is.
    if(character >= 256)
    {
        cs_send_virtual_key((freerdp*)instance, character, down);
        return;
    }
    
    int vk = cs_get_unicode(character);
    if(vk != 0)
    {
        cs_send_unicode_key((freerdp*)instance, vk);
    }
    else
    {
        if(isupper(character))
        {
            character = tolower(character);
            if(down)
            {
                cs_send_virtual_key((freerdp*)instance, VK_LSHIFT, TRUE);
            }
            shift_was_sent = TRUE;
        }
        
        vk = cs_get_vk_code(character);
        if(vk == 0)
        {
            // send as is
            vk = character;
        }
        
        // send key pressed
        cs_send_virtual_key((freerdp*)instance, vk, down);
        
        if(shift_was_sent && !down)
            cs_send_virtual_key((freerdp*)instance, VK_LSHIFT, FALSE);
    }
}

void csharp_freerdp_send_cursor_event(void* instance, int x, int y, int flags)
{
    freerdp_input_send_mouse_event(((freerdp*)instance)->input, flags, x, y);
}

void csharp_freerdp_send_clipboard_data(void* instance, BYTE* buffer, int length)
{
    int size;
    BYTE* data;
    UINT32 formatId;

    freerdp* inst = (freerdp*)instance;
    csContext* ctxt = (csContext*)inst->context;

    if(!ctxt->clipboard)
    {
        WLog_ERR(TAG, "Clipboard not initialized yet\n");
        return; /* Clipboard not ready yet.*/
    }

    formatId = ClipboardRegisterFormat(ctxt->clipboard, "UTF8_STRING");

    if (length)
    {
        size = length + 1;
        data = (BYTE*) malloc(size);

        if (!data)
            return;

        CopyMemory(data, buffer, size);
        data[size] = '\0';
        ClipboardSetData(ctxt->clipboard, formatId, (void*) data, size);
    }
    else
    {
        ClipboardEmpty(ctxt->clipboard);
    }

    cs_cliprdr_send_client_format_list(ctxt->cliprdr);
}

void csharp_set_log_output(const char* path, const char* name)
{
    SetEnvironmentVariableA("WLOG_APPENDER", "FILE");
    SetEnvironmentVariableA("WLOG_LEVEL", "DEBUG");
    SetEnvironmentVariableA("WLOG_FILEAPPENDER_OUTPUT_FILE_PATH", path);
    SetEnvironmentVariableA("WLOG_FILEAPPENDER_OUTPUT_FILE_NAME", name);
}

void csharp_set_on_authenticate(void* instance, pAuthenticate fn)
{
	freerdp* inst = (freerdp*)instance;
	
	inst->Authenticate = fn;
}

void csharp_set_on_clipboard_update(void* instance, fnOnClipboardUpdate fn)
{
	freerdp* inst = (freerdp*)instance;
    csContext* ctxt = (csContext*)inst->context;
    
    ctxt->onClipboardUpdate = fn;
}

void csharp_set_on_gateway_authenticate(void* instance, pAuthenticate fn)
{
	freerdp* inst = (freerdp*)instance;
	
	inst->GatewayAuthenticate = fn;
}

void csharp_set_on_verify_certificate(void* instance, pVerifyCertificate fn)
{
	freerdp* inst = (freerdp*)instance;
	
	inst->VerifyCertificate = fn;
}

void csharp_set_on_verify_x509_certificate(void* instance, pVerifyX509Certificate fn)
{
	freerdp* inst = (freerdp*)instance;
	
	inst->VerifyX509Certificate = fn;
}

void csharp_set_on_error(void* instance, fnOnError fn)
{
    freerdp* inst = (freerdp*)instance;
    csContext* ctxt = (csContext*)inst->context;
    
    ctxt->onError = fn;
}

void csharp_set_on_cursor_notifications(void* instance, fnOnNewCursor newCursor, fnOnFreeCursor freeCursor, fnOnSetCursor setCursor, fnOnDefaultCursor defaultCursor)
{
	freerdp* inst = (freerdp*)instance;
	csContext* csc = (csContext*)inst->context;
	
	csc->onNewCursor = newCursor;
	csc->onFreeCursor = freeCursor;
	csc->onSetCursor = setCursor;
	csc->onDefaultCursor = defaultCursor;
}

const char* csharp_get_error_info_string(int code)
{
    return freerdp_get_error_info_string(code);
}

int csharp_get_last_error(void* instance)
{
	freerdp* inst = (freerdp*)instance;
	rdpContext* ctx = (rdpContext*)inst->context;

	return freerdp_get_last_error(ctx);
}

DWORD csharp_get_vk_from_keycode(DWORD keycode, DWORD flags)
{
	return GetVirtualKeyCodeFromKeycode(keycode, flags);
}

DWORD csharp_get_scancode_from_vk(DWORD keycode, DWORD flags)
{
	return GetVirtualScanCodeFromVirtualKeyCode(keycode, flags);
}

void csharp_freerdp_send_scancode(void* instance, int flags, DWORD scancode)
{
	freerdp* inst = (freerdp*)instance;
	
	freerdp_input_send_keyboard_event(inst->input, flags, scancode);
}

void csharp_freerdp_redirect_drive(void* instance, char* name, char* path)
{
	freerdp* inst = (freerdp*)instance;
	char* d[] = { "drive", name, path};
	
	freerdp_client_add_device_channel(inst->settings, 3, d);
}

void csharp_freerdp_set_smart_sizing(void* instance, bool smartSizing)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	
	settings->SmartSizing = smartSizing;
}

void csharp_freerdp_set_load_balance_info(void* instance, const char* info)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	settings->LoadBalanceInfo = (BYTE*)_strdup(info);
	settings->LoadBalanceInfoLength = (UINT32)strlen((char*) settings->LoadBalanceInfo);
}

BOOL csharp_freerdp_set_performance_flags(void* instance, BOOL disableWallpaper, BOOL allowFontSmoothing, BOOL allowDesktopComposition,
					  BOOL bitmapCacheEnabled, BOOL disableFullWindowDrag, BOOL disableMenuAnims, BOOL disableThemes)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	
	settings->DisableWallpaper = disableWallpaper;
	settings->AllowFontSmoothing = allowFontSmoothing;
	settings->AllowDesktopComposition = allowDesktopComposition;
	settings->BitmapCacheEnabled = bitmapCacheEnabled;
	settings->DisableFullWindowDrag = disableFullWindowDrag;
	settings->DisableMenuAnims = disableMenuAnims;
	settings->DisableThemes = disableThemes;
	
	return TRUE;
}

void csharp_freerdp_sync_toggle_keys(void* instance)
{
#ifdef WIN32
	UINT16 syncFlags = 0;
	freerdp* inst = (freerdp*)instance;
	
	if (!inst)
		return;
	
	if (GetKeyState(VK_NUMLOCK))
		syncFlags |= KBD_SYNC_NUM_LOCK;
	
	if (GetKeyState(VK_CAPITAL))
		syncFlags |= KBD_SYNC_CAPS_LOCK;
	
	if (GetKeyState(VK_SCROLL))
		syncFlags |= KBD_SYNC_SCROLL_LOCK;
	
	if (GetKeyState(VK_KANA))
		syncFlags |= KBD_SYNC_KANA_LOCK;
	
	inst->input->FocusInEvent(inst->input, syncFlags);
#endif
}

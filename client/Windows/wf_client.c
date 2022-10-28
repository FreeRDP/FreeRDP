/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Client
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <winpr/windows.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <winpr/assert.h>
#include <sys/types.h>
#include <io.h>

#ifdef WITH_PROGRESS_BAR
#include <shobjidl.h>
#endif

#ifdef WITH_WINDOWS_CERT_STORE
#include <wincrypt.h>
#endif

#include <freerdp/log.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/codec/region.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>

#include "wf_gdi.h"
#include "wf_rail.h"
#include "wf_channels.h"
#include "wf_graphics.h"

#include "resource/resource.h"

#define TAG CLIENT_TAG("windows")

static BOOL wf_has_console(void)
{
#ifdef WITH_WIN_CONSOLE
	int file = _fileno(stdin);
	int tty = _isatty(file);
#else
	int file = -1;
	int tty = 0;
#endif

	WLog_INFO(TAG, "Detected stdin=%d -> %s mode", file, tty ? "console" : "gui");
	return tty;
}

static BOOL wf_end_paint(rdpContext* context)
{
	int i;
	rdpGdi* gdi;
	int ninvalid;
	RECT updateRect;
	HGDI_RGN cinvalid;
	REGION16 invalidRegion;
	RECTANGLE_16 invalidRect;
	const RECTANGLE_16* extents;
	wfContext* wfc = (wfContext*)context;
	gdi = context->gdi;
	ninvalid = gdi->primary->hdc->hwnd->ninvalid;
	cinvalid = gdi->primary->hdc->hwnd->cinvalid;

	if (ninvalid < 1)
		return TRUE;

	region16_init(&invalidRegion);

	for (i = 0; i < ninvalid; i++)
	{
		invalidRect.left = cinvalid[i].x;
		invalidRect.top = cinvalid[i].y;
		invalidRect.right = cinvalid[i].x + cinvalid[i].w;
		invalidRect.bottom = cinvalid[i].y + cinvalid[i].h;
		region16_union_rect(&invalidRegion, &invalidRegion, &invalidRect);
	}

	if (!region16_is_empty(&invalidRegion))
	{
		extents = region16_extents(&invalidRegion);
		updateRect.left = extents->left;
		updateRect.top = extents->top;
		updateRect.right = extents->right;
		updateRect.bottom = extents->bottom;

		wf_scale_rect(wfc, &updateRect);

		InvalidateRect(wfc->hwnd, &updateRect, FALSE);

		if (wfc->rail)
			wf_rail_invalidate_region(wfc, &invalidRegion);
	}

	region16_uninit(&invalidRegion);

	if (!wfc->is_shown)
	{
		wfc->is_shown = TRUE;

#ifdef WITH_PROGRESS_BAR
		if (wfc->taskBarList)
		{
			wfc->taskBarList->lpVtbl->SetProgressState(wfc->taskBarList, wfc->hwnd,
			                                           TBPF_NOPROGRESS);
		}
#endif

		ShowWindow(wfc->hwnd, SW_SHOWNORMAL);
		WLog_INFO(TAG, "Window is shown!");
		fflush(stdout);
	}
	return TRUE;
}

static BOOL wf_begin_paint(rdpContext* context)
{
	HGDI_DC hdc;

	if (!context || !context->gdi || !context->gdi->primary || !context->gdi->primary->hdc)
		return FALSE;

	hdc = context->gdi->primary->hdc;

	if (!hdc || !hdc->hwnd || !hdc->hwnd->invalid)
		return FALSE;

	hdc->hwnd->invalid->null = TRUE;
	hdc->hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL wf_desktop_resize(rdpContext* context)
{
	BOOL same;
	RECT rect;
	rdpSettings* settings;
	wfContext* wfc = (wfContext*)context;

	if (!context || !context->settings)
		return FALSE;

	settings = context->settings;

	if (wfc->primary)
	{
		same = (wfc->primary == wfc->drawing) ? TRUE : FALSE;
		wf_image_free(wfc->primary);
		wfc->primary = wf_image_new(wfc, settings->DesktopWidth, settings->DesktopHeight,
		                            context->gdi->dstFormat, NULL);
	}

	if (!gdi_resize_ex(context->gdi, settings->DesktopWidth, settings->DesktopHeight, 0,
	                   context->gdi->dstFormat, wfc->primary->pdata, NULL))
		return FALSE;

	if (same)
		wfc->drawing = wfc->primary;

	if (wfc->fullscreen != TRUE)
	{
		if (wfc->hwnd && !settings->SmartSizing)
			SetWindowPos(wfc->hwnd, HWND_TOP, -1, -1, settings->DesktopWidth + wfc->diff.x,
			             settings->DesktopHeight + wfc->diff.y, SWP_NOMOVE);
	}
	else
	{
		wf_update_offset(wfc);
		GetWindowRect(wfc->hwnd, &rect);
		InvalidateRect(wfc->hwnd, &rect, TRUE);
	}

	return TRUE;
}

static BOOL wf_pre_connect(freerdp* instance)
{
	UINT32 rc;
	wfContext* wfc;
	UINT32 desktopWidth;
	UINT32 desktopHeight;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	context = instance->context;
	wfc = (wfContext*)instance->context;
	settings = context->settings;
	settings->OsMajorType = OSMAJORTYPE_WINDOWS;
	settings->OsMinorType = OSMINORTYPE_WINDOWS_NT;
	wfc->fullscreen = settings->Fullscreen;
	wfc->fullscreen_toggle = settings->ToggleFullscreen;
	desktopWidth = settings->DesktopWidth;
	desktopHeight = settings->DesktopHeight;

	if (wfc->percentscreen > 0)
	{
		desktopWidth = (GetSystemMetrics(SM_CXSCREEN) * wfc->percentscreen) / 100;
		settings->DesktopWidth = desktopWidth;
		desktopHeight = (GetSystemMetrics(SM_CYSCREEN) * wfc->percentscreen) / 100;
		settings->DesktopHeight = desktopHeight;
	}

	if (wfc->fullscreen)
	{
		if (settings->UseMultimon)
		{
			desktopWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			desktopHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		}
		else
		{
			desktopWidth = GetSystemMetrics(SM_CXSCREEN);
			desktopHeight = GetSystemMetrics(SM_CYSCREEN);
		}
	}

	/* FIXME: desktopWidth has a limitation that it should be divisible by 4,
	 *        otherwise the screen will crash when connecting to an XP desktop.*/
	desktopWidth = (desktopWidth + 3) & (~3);

	if (desktopWidth != settings->DesktopWidth)
	{
		freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, desktopWidth);
	}

	if (desktopHeight != settings->DesktopHeight)
	{
		freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, desktopHeight);
	}

	rc = freerdp_keyboard_init(freerdp_settings_get_uint32(settings, FreeRDP_KeyboardLayout));
	freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, rc);
	PubSub_SubscribeChannelConnected(instance->context->pubSub, wf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    wf_OnChannelDisconnectedEventHandler);
	return TRUE;
}

static void wf_append_item_to_system_menu(HMENU hMenu, UINT fMask, UINT wID, const wchar_t* text,
                                          wfContext* wfc)
{
	MENUITEMINFO item_info = { 0 };
	item_info.fMask = fMask;
	item_info.cbSize = sizeof(MENUITEMINFO);
	item_info.wID = wID;
	item_info.fType = MFT_STRING;
	item_info.dwTypeData = _wcsdup(text);
	item_info.cch = (UINT)_wcslen(text);
	if (wfc)
		item_info.dwItemData = (ULONG_PTR)wfc;
	InsertMenuItem(hMenu, wfc->systemMenuInsertPosition++, TRUE, &item_info);
}

static void wf_add_system_menu(wfContext* wfc)
{
	HMENU hMenu;

	if (wfc->fullscreen && !wfc->fullscreen_toggle)
	{
		return;
	}

	if (wfc->common.context.settings->DynamicResolutionUpdate)
	{
		return;
	}

	hMenu = GetSystemMenu(wfc->hwnd, FALSE);

	wf_append_item_to_system_menu(hMenu,
	                              MIIM_CHECKMARKS | MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_DATA,
	                              SYSCOMMAND_ID_SMARTSIZING, L"Smart sizing", wfc);

	if (wfc->common.context.settings->SmartSizing)
	{
		CheckMenuItem(hMenu, SYSCOMMAND_ID_SMARTSIZING, MF_CHECKED);
	}

	if (wfc->common.context.settings->RemoteAssistanceMode)
		wf_append_item_to_system_menu(hMenu, MIIM_FTYPE | MIIM_ID | MIIM_STRING,
		                              SYSCOMMAND_ID_REQUEST_CONTROL, L"Request control", wfc);
}

static WCHAR* wf_window_get_title(rdpSettings* settings)
{
	BOOL port;
	WCHAR* windowTitle = NULL;
	size_t size;
	char* name;
	WCHAR prefix[] = L"FreeRDP:";

	if (!settings)
		return NULL;

	name = settings->ServerHostname;

	if (settings->WindowTitle)
		return ConvertUtf8ToWCharAlloc(settings->WindowTitle, NULL);

	port = (settings->ServerPort != 3389);
	size = strlen(name) + 16 + wcslen(prefix);
	windowTitle = calloc(size, sizeof(WCHAR));

	if (!windowTitle)
		return NULL;

	if (!port)
		_snwprintf_s(windowTitle, size, _TRUNCATE, L"%s %S", prefix, name);
	else
		_snwprintf_s(windowTitle, size, _TRUNCATE, L"%s %S:%u", prefix, name, settings->ServerPort);

	return windowTitle;
}

static BOOL wf_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	DWORD dwStyle;
	rdpCache* cache;
	wfContext* wfc;
	rdpContext* context;
	rdpSettings* settings;
	EmbedWindowEventArgs e;
	const UINT32 format = PIXEL_FORMAT_BGRX32;

	WINPR_ASSERT(instance);

	context = instance->context;
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	wfc = (wfContext*)instance->context;
	WINPR_ASSERT(wfc);

	wfc->primary = wf_image_new(wfc, settings->DesktopWidth, settings->DesktopHeight, format, NULL);

	if (!gdi_init_ex(instance, format, 0, wfc->primary->pdata, NULL))
		return FALSE;

	cache = instance->context->cache;
	WINPR_ASSERT(cache);

	gdi = instance->context->gdi;

	if (!settings->SoftwareGdi)
	{
		wf_gdi_register_update_callbacks(context->update);
	}

	wfc->window_title = wf_window_get_title(settings);

	if (!wfc->window_title)
		return FALSE;

	if (settings->EmbeddedWindow)
		settings->Decorations = FALSE;

	if (wfc->fullscreen)
		dwStyle = WS_POPUP;
	else if (!settings->Decorations)
		dwStyle = WS_CHILD | WS_BORDER;
	else
		dwStyle =
		    WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX | WS_MAXIMIZEBOX;

	if (!wfc->hwnd)
	{
		wfc->hwnd = CreateWindowEx(0, wfc->wndClassName, wfc->window_title, dwStyle, 0, 0, 0, 0,
		                           wfc->hWndParent, NULL, wfc->hInstance, NULL);
		SetWindowLongPtr(wfc->hwnd, GWLP_USERDATA, (LONG_PTR)wfc);
	}

	wf_resize_window(wfc);
	wf_add_system_menu(wfc);
	BitBlt(wfc->primary->hdc, 0, 0, settings->DesktopWidth, settings->DesktopHeight, NULL, 0, 0,
	       BLACKNESS);
	wfc->drawing = wfc->primary;
	EventArgsInit(&e, "wfreerdp");
	e.embed = FALSE;
	e.handle = (void*)wfc->hwnd;
	PubSub_OnEmbedWindow(context->pubSub, context, &e);
#ifdef WITH_PROGRESS_BAR
	if (wfc->taskBarList)
	{
		ShowWindow(wfc->hwnd, SW_SHOWMINIMIZED);
		wfc->taskBarList->lpVtbl->SetProgressState(wfc->taskBarList, wfc->hwnd, TBPF_INDETERMINATE);
	}
#endif
	UpdateWindow(wfc->hwnd);
	context->update->BeginPaint = wf_begin_paint;
	context->update->DesktopResize = wf_desktop_resize;
	context->update->EndPaint = wf_end_paint;
	wf_register_pointer(context->graphics);

	if (!settings->SoftwareGdi)
	{
		wf_register_graphics(context->graphics);
		wf_gdi_register_update_callbacks(context->update);
		brush_cache_register_callbacks(context->update);
		glyph_cache_register_callbacks(context->update);
		bitmap_cache_register_callbacks(context->update);
		offscreen_cache_register_callbacks(context->update);
		palette_cache_register_callbacks(context->update);
	}

	wfc->floatbar = wf_floatbar_new(wfc, wfc->hInstance, settings->Floatbar);
	return TRUE;
}

static void wf_post_disconnect(freerdp* instance)
{
	wfContext* wfc;

	if (!instance || !instance->context)
		return;

	wfc = (wfContext*)instance->context;
	free(wfc->window_title);
}

static CREDUI_INFOA wfUiInfo = { sizeof(CREDUI_INFOA), NULL, "Enter your credentials",
	                             "Remote Desktop Security", NULL };

static BOOL wf_authenticate_raw(freerdp* instance, const char* title, char** username,
                                char** password, char** domain)
{
	wfContext* wfc;
	BOOL fSave;
	DWORD status;
	DWORD dwFlags;
	char UserName[CREDUI_MAX_USERNAME_LENGTH + 1] = { 0 };
	char Password[CREDUI_MAX_PASSWORD_LENGTH + 1] = { 0 };
	char User[CREDUI_MAX_USERNAME_LENGTH + 1] = { 0 };
	char Domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1] = { 0 };

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	wfc = (wfContext*)instance->context;

	fSave = FALSE;
	dwFlags = CREDUI_FLAGS_DO_NOT_PERSIST | CREDUI_FLAGS_EXCLUDE_CERTIFICATES;

	if (username && *username)
	{
		strncpy(UserName, *username, CREDUI_MAX_USERNAME_LENGTH);
		strncpy(User, UserName, CREDUI_MAX_USERNAME_LENGTH);
	}

	if (password && *password)
	{
		strncpy(Password, *password, CREDUI_MAX_PASSWORD_LENGTH);
	}

	if (domain && *domain)
	{
		strncpy(Domain, *domain, CREDUI_MAX_DOMAIN_TARGET_LENGTH);
	}

	if (!(*UserName && *Password))
	{
		if (!wfc->isConsole && wfc->common.context.settings->CredentialsFromStdin)
			WLog_ERR(TAG, "Flag for stdin read present but stdin is redirected; using GUI");
		if (wfc->isConsole && wfc->common.context.settings->CredentialsFromStdin)
			status = CredUICmdLinePromptForCredentialsA(
			    title, NULL, 0, UserName, CREDUI_MAX_USERNAME_LENGTH + 1, Password,
			    CREDUI_MAX_PASSWORD_LENGTH + 1, &fSave, dwFlags);
		else
			status = CredUIPromptForCredentialsA(&wfUiInfo, title, NULL, 0, UserName,
			                                     CREDUI_MAX_USERNAME_LENGTH + 1, Password,
			                                     CREDUI_MAX_PASSWORD_LENGTH + 1, &fSave, dwFlags);

		if (status != NO_ERROR)
		{
			WLog_ERR(TAG, "CredUIPromptForCredentials unexpected status: 0x%08lX", status);
			return FALSE;
		}

		status = CredUIParseUserNameA(UserName, User, CREDUI_MAX_USERNAME_LENGTH, Domain,
		                              CREDUI_MAX_DOMAIN_TARGET_LENGTH);
		if (status != NO_ERROR)
		{
			WLog_ERR(TAG, "Failed to parse UserName: %s into User: %s Domain: %s", UserName, User,
			         Domain);
			return FALSE;
		}
	}
	*username = _strdup(User);

	if (!(*username))
	{
		WLog_ERR(TAG, "strdup failed", status);
		return FALSE;
	}

	if (strlen(Domain) > 0)
		*domain = _strdup(Domain);
	else
		*domain = _strdup("\0");

	if (!(*domain))
	{
		free(*username);
		WLog_ERR(TAG, "strdup failed", status);
		return FALSE;
	}

	*password = _strdup(Password);

	if (!(*password))
	{
		free(*username);
		free(*domain);
		return FALSE;
	}

	return TRUE;
}

static BOOL wf_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	rdpSettings* settings;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	return wf_authenticate_raw(instance, settings->ServerHostname, username, password, domain);
}

static BOOL wf_gw_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	char tmp[MAX_PATH];
	rdpSettings* settings;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	sprintf_s(tmp, sizeof(tmp), "Gateway %s", settings->GatewayHostname);
	return wf_authenticate_raw(instance, tmp, username, password, domain);
}

static WCHAR* wf_format_text(const WCHAR* fmt, ...)
{
	int rc;
	size_t size = 0;
	WCHAR* buffer = NULL;

	do
	{
		WCHAR* tmp;
		va_list ap;
		va_start(ap, fmt);
		rc = _vsnwprintf(buffer, size, fmt, ap);
		va_end(ap);
		if (rc <= 0)
			goto fail;

		if ((size_t)rc < size)
			return buffer;

		size = (size_t)rc + 1;
		tmp = realloc(buffer, size * sizeof(WCHAR));
		if (!tmp)
			goto fail;

		buffer = tmp;
	} while (TRUE);

fail:
	free(buffer);
	return NULL;
}

#ifdef WITH_WINDOWS_CERT_STORE
/* https://stackoverflow.com/questions/1231178/load-an-pem-encoded-x-509-certificate-into-windows-cryptoapi/3803333#3803333
 */
/* https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/security/cryptoapi/peertrust/cpp/peertrust.cpp
 */
/* https://stackoverflow.com/questions/7340504/whats-the-correct-way-to-verify-an-ssl-certificate-in-win32
 */

static void wf_report_error(char* wszMessage, DWORD dwErrCode)
{
	LPSTR pwszMsgBuf = NULL;

	if (NULL != wszMessage && 0 != *wszMessage)
	{
		WLog_ERR(TAG, "%s", wszMessage);
	}

	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	               NULL,                                      // Location of message
	                                                          //  definition ignored
	               dwErrCode,                                 // Message identifier for
	                                                          //  the requested message
	               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Language identifier for
	                                                          //  the requested message
	               (LPSTR)&pwszMsgBuf,                        // Buffer that receives
	                                                          //  the formatted message
	               0,                                         // Size of output buffer
	                                                          //  not needed as allocate
	                                                          //  buffer flag is set
	               NULL                                       // Array of insert values
	);

	if (NULL != pwszMsgBuf)
	{
		WLog_ERR(TAG, "Error: 0x%08x (%d) %s", dwErrCode, dwErrCode, pwszMsgBuf);
		LocalFree(pwszMsgBuf);
	}
	else
	{
		WLog_ERR(TAG, "Error: 0x%08x (%d)", dwErrCode, dwErrCode);
	}
}

static DWORD wf_is_x509_certificate_trusted(const char* common_name, const char* subject,
                                            const char* issuer, const char* fingerprint)
{
	size_t derPubKeyLen;
	char* derPubKey;

	HRESULT hr = CRYPT_E_NOT_FOUND;

	DWORD dwChainFlags = CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
	PCCERT_CONTEXT pCert = NULL;
	HCERTSTORE hStore = NULL;
	HCERTCHAINENGINE hChainEngine = NULL;
	PCCERT_CHAIN_CONTEXT pChainContext = NULL;

	CERT_ENHKEY_USAGE EnhkeyUsage = { 0 };
	CERT_USAGE_MATCH CertUsage = { 0 };
	CERT_CHAIN_PARA ChainPara = { 0 };
	CERT_CHAIN_POLICY_PARA ChainPolicy = { 0 };
	CERT_CHAIN_POLICY_STATUS PolicyStatus = { 0 };
	CERT_CHAIN_ENGINE_CONFIG EngineConfig = { 0 };

	derPubKeyLen = strlen(fingerprint);
	derPubKey = calloc(derPubKeyLen, sizeof(char));
	if (NULL == derPubKey)
	{
		WLog_ERR(TAG, "Could not allocate derPubKey");
		goto CleanUp;
	}

	/*
	 * Convert from PEM format to DER format - removes header and footer and decodes from base64
	 */
	if (!CryptStringToBinaryA(fingerprint, 0, CRYPT_STRING_BASE64HEADER, derPubKey, &derPubKeyLen,
	                          NULL, NULL))
	{
		WLog_ERR(TAG, "CryptStringToBinary failed. Err: %d", GetLastError());
		goto CleanUp;
	}

	//---------------------------------------------------------
	// Initialize data structures for chain building.

	EnhkeyUsage.cUsageIdentifier = 0;
	EnhkeyUsage.rgpszUsageIdentifier = NULL;

	CertUsage.dwType = USAGE_MATCH_TYPE_AND;
	CertUsage.Usage = EnhkeyUsage;

	ChainPara.cbSize = sizeof(ChainPara);
	ChainPara.RequestedUsage = CertUsage;

	ChainPolicy.cbSize = sizeof(ChainPolicy);

	PolicyStatus.cbSize = sizeof(PolicyStatus);

	EngineConfig.cbSize = sizeof(EngineConfig);
	EngineConfig.dwUrlRetrievalTimeout = 0;

	pCert = CertCreateCertificateContext(X509_ASN_ENCODING, derPubKey, derPubKeyLen);
	if (NULL == pCert)
	{
		WLog_ERR(TAG, "FAILED: Certificate could not be parsed.");
		goto CleanUp;
	}

	dwChainFlags |= CERT_CHAIN_ENABLE_PEER_TRUST;

	// When this flag is set, end entity certificates in the
	// Trusted People store are trusted without doing any chain building
	// This optimizes the chain building process.

	//---------------------------------------------------------
	// Create chain engine.

	if (!CertCreateCertificateChainEngine(&EngineConfig, &hChainEngine))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		goto CleanUp;
	}

	//-------------------------------------------------------------------
	// Build a chain using CertGetCertificateChain

	if (!CertGetCertificateChain(hChainEngine, // use the default chain engine
	                             pCert,        // pointer to the end certificate
	                             NULL,         // use the default time
	                             NULL,         // search no additional stores
	                             &ChainPara,   // use AND logic and enhanced key usage
	                                           //  as indicated in the ChainPara
	                                           //  data structure
	                             dwChainFlags,
	                             NULL,            // currently reserved
	                             &pChainContext)) // return a pointer to the chain created
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		goto CleanUp;
	}

	//---------------------------------------------------------------
	// Verify that the chain complies with policy

	if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_BASE, // use the base policy
	                                      pChainContext,          // pointer to the chain
	                                      &ChainPolicy,
	                                      &PolicyStatus)) // return a pointer to the policy status
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		goto CleanUp;
	}

	if (PolicyStatus.dwError != S_OK)
	{
		wf_report_error("CertVerifyCertificateChainPolicy: Chain Status", PolicyStatus.dwError);
		hr = PolicyStatus.dwError;
		// Instruction: If the PolicyStatus.dwError is CRYPT_E_NO_REVOCATION_CHECK or
		// CRYPT_E_REVOCATION_OFFLINE, it indicates errors in obtaining
		//				revocation information. These can be ignored since the retrieval of
		// revocation information depends on network availability

		if (PolicyStatus.dwError == CRYPT_E_NO_REVOCATION_CHECK ||
		    PolicyStatus.dwError == CRYPT_E_REVOCATION_OFFLINE)
		{
			hr = S_OK;
		}

		goto CleanUp;
	}

	WLog_INFO(TAG, "CertVerifyCertificateChainPolicy succeeded for %s (%s) issued by %s",
	          common_name, subject, issuer);

	hr = S_OK;
CleanUp:

	if (FAILED(hr))
	{
		WLog_INFO(TAG, "CertVerifyCertificateChainPolicy failed for %s (%s) issued by %s",
		          common_name, subject, issuer);
		wf_report_error(NULL, hr);
	}

	free(derPubKey);

	if (NULL != pChainContext)
	{
		CertFreeCertificateChain(pChainContext);
	}

	if (NULL != hChainEngine)
	{
		CertFreeCertificateChainEngine(hChainEngine);
	}

	if (NULL != pCert)
	{
		CertFreeCertificateContext(pCert);
	}

	return (DWORD)hr;
}
#endif

static DWORD wf_cli_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                          const char* common_name, const char* subject,
                                          const char* issuer, const char* fingerprint, DWORD flags)
{
#ifdef WITH_WINDOWS_CERT_STORE
	if (flags & VERIFY_CERT_FLAG_FP_IS_PEM && !(flags & VERIFY_CERT_FLAG_MISMATCH))
	{
		if (wf_is_x509_certificate_trusted(common_name, subject, issuer, fingerprint) == S_OK)
		{
			return 2;
		}
	}
#endif

	return client_cli_verify_certificate_ex(instance, host, port, common_name, subject, issuer,
	                                        fingerprint, flags);
}

static DWORD wf_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                      const char* common_name, const char* subject,
                                      const char* issuer, const char* fingerprint, DWORD flags)
{
	WCHAR* buffer;
	WCHAR* caption;
	int what = IDCANCEL;

#ifdef WITH_WINDOWS_CERT_STORE
	if (flags & VERIFY_CERT_FLAG_FP_IS_PEM && !(flags & VERIFY_CERT_FLAG_MISMATCH))
	{
		if (wf_is_x509_certificate_trusted(common_name, subject, issuer, fingerprint) == S_OK)
		{
			return 2;
		}
	}
#endif

	buffer = wf_format_text(
	    L"Certificate details:\n"
	    L"\tCommonName: %S\n"
	    L"\tSubject: %S\n"
	    L"\tIssuer: %S\n"
	    L"\tThumbprint: %S\n"
	    L"\tHostMismatch: %S\n"
	    L"\n"
	    L"The above X.509 certificate could not be verified, possibly because you do not have "
	    L"the CA certificate in your certificate store, or the certificate has expired. "
	    L"Please look at the OpenSSL documentation on how to add a private CA to the store.\n"
	    L"\n"
	    L"YES\tAccept permanently\n"
	    L"NO\tAccept for this session only\n"
	    L"CANCEL\tAbort connection\n",
	    common_name, subject, issuer, fingerprint,
	    flags & VERIFY_CERT_FLAG_MISMATCH ? "Yes" : "No");
	caption = wf_format_text(L"Verify certificate for %S:%hu", host, port);

	WINPR_UNUSED(instance);

	if (!buffer || !caption)
		goto fail;

	what = MessageBoxW(NULL, buffer, caption, MB_YESNOCANCEL);
fail:
	free(buffer);
	free(caption);

	/* return 1 to accept and store a certificate, 2 to accept
	 * a certificate only for this session, 0 otherwise */
	switch (what)
	{
		case IDYES:
			return 1;
		case IDNO:
			return 2;
		default:
			return 0;
	}
}

static DWORD wf_verify_changed_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                              const char* common_name, const char* subject,
                                              const char* issuer, const char* new_fingerprint,
                                              const char* old_subject, const char* old_issuer,
                                              const char* old_fingerprint, DWORD flags)
{
	WCHAR* buffer;
	WCHAR* caption;
	int what = IDCANCEL;

	buffer = wf_format_text(
	    L"New Certificate details:\n"
	    L"\tCommonName: %S\n"
	    L"\tSubject: %S\n"
	    L"\tIssuer: %S\n"
	    L"\tThumbprint: %S\n"
	    L"\tHostMismatch: %S\n"
	    L"\n"
	    L"Old Certificate details:\n"
	    L"\tSubject: %S\n"
	    L"\tIssuer: %S\n"
	    L"\tThumbprint: %S"
	    L"The above X.509 certificate could not be verified, possibly because you do not have "
	    L"the CA certificate in your certificate store, or the certificate has expired. "
	    L"Please look at the OpenSSL documentation on how to add a private CA to the store.\n"
	    L"\n"
	    L"YES\tAccept permanently\n"
	    L"NO\tAccept for this session only\n"
	    L"CANCEL\tAbort connection\n",
	    common_name, subject, issuer, new_fingerprint,
	    flags & VERIFY_CERT_FLAG_MISMATCH ? "Yes" : "No", old_subject, old_issuer, old_fingerprint);
	caption = wf_format_text(L"Verify certificate change for %S:%hu", host, port);

	WINPR_UNUSED(instance);
	if (!buffer || !caption)
		goto fail;

	what = MessageBoxW(NULL, buffer, caption, MB_YESNOCANCEL);
fail:
	free(buffer);
	free(caption);

	/* return 1 to accept and store a certificate, 2 to accept
	 * a certificate only for this session, 0 otherwise */
	switch (what)
	{
		case IDYES:
			return 1;
		case IDNO:
			return 2;
		default:
			return 0;
	}
}

static BOOL wf_present_gateway_message(freerdp* instance, UINT32 type, BOOL isDisplayMandatory,
                                       BOOL isConsentMandatory, size_t length, const WCHAR* message)
{
	if (!isDisplayMandatory && !isConsentMandatory)
		return TRUE;

	/* special handling for consent messages (show modal dialog) */
	if (type == GATEWAY_MESSAGE_CONSENT && isConsentMandatory)
	{
		int mbRes;
		WCHAR* msg;

		msg = wf_format_text(L"%.*s\n\nI understand and agree to the terms of this policy", length,
		                     message);
		mbRes = MessageBoxW(NULL, msg, L"Consent Message", MB_YESNO);
		free(msg);

		if (mbRes != IDYES)
			return FALSE;
	}
	else
		return client_cli_present_gateway_message(instance, type, isDisplayMandatory,
		                                          isConsentMandatory, length, message);

	return TRUE;
}

static DWORD WINAPI wf_client_thread(LPVOID lpParam)
{
	MSG msg;
	int width;
	int height;
	BOOL msg_ret;
	int quit_msg;
	DWORD error;
	wfContext* wfc;
	freerdp* instance;
	rdpContext* context;
	rdpChannels* channels;
	rdpSettings* settings;

	instance = (freerdp*)lpParam;
	WINPR_ASSERT(instance);

	if (!freerdp_connect(instance))
		goto end;

	context = instance->context;
	WINPR_ASSERT(context);

	wfc = (wfContext*)instance->context;
	WINPR_ASSERT(wfc);

	channels = context->channels;
	WINPR_ASSERT(channels);

	settings = context->settings;
	WINPR_ASSERT(settings);

	while (1)
	{
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
		DWORD nCount = 0;

		if (freerdp_focus_required(instance))
		{
			wf_event_focus_in(wfc);
			wf_event_focus_in(wfc);
		}

		{
			DWORD tmp = freerdp_get_event_handles(context, &handles[nCount], 64 - nCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "freerdp_get_event_handles failed");
				break;
			}

			nCount += tmp;
		}

		if (MsgWaitForMultipleObjects(nCount, handles, FALSE, 1000, QS_ALLINPUT) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "wfreerdp_run: WaitForMultipleObjects failed: 0x%08lX", GetLastError());
			break;
		}

		{
			if (!freerdp_check_event_handles(context))
			{
				if (client_auto_reconnect(instance))
					continue;

				WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
				fflush(stdout);
				break;
			}
		}

		if (freerdp_shall_disconnect_context(instance->context))
			break;

		quit_msg = FALSE;

		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			msg_ret = GetMessage(&msg, NULL, 0, 0);

			if (settings->EmbeddedWindow)
			{
				if ((msg.message == WM_SETFOCUS) && (msg.lParam == 1))
				{
					PostMessage(wfc->hwnd, WM_SETFOCUS, 0, 0);
				}
				else if ((msg.message == WM_KILLFOCUS) && (msg.lParam == 1))
				{
					PostMessage(wfc->hwnd, WM_KILLFOCUS, 0, 0);
				}
			}

			if (msg.message == WM_SIZE)
			{
				width = LOWORD(msg.lParam);
				height = HIWORD(msg.lParam);
				SetWindowPos(wfc->hwnd, HWND_TOP, 0, 0, width, height, SWP_FRAMECHANGED);
			}

			if ((msg_ret == 0) || (msg_ret == -1))
			{
				quit_msg = TRUE;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (quit_msg)
			break;
	}

	/* cleanup */

disconnect:
	freerdp_disconnect(instance);

end:
	error = freerdp_get_last_error(instance->context);
	WLog_DBG(TAG, "Main thread exited with %" PRIu32, error);
	ExitThread(error);
	return error;
}

static DWORD WINAPI wf_keyboard_thread(LPVOID lpParam)
{
	MSG msg;
	BOOL status;
	wfContext* wfc;
	HHOOK hook_handle;
	wfc = (wfContext*)lpParam;
	WINPR_ASSERT(NULL != wfc);
	hook_handle = SetWindowsHookEx(WH_KEYBOARD_LL, wf_ll_kbd_proc, wfc->hInstance, 0);

	if (hook_handle)
	{
		while ((status = GetMessage(&msg, NULL, 0, 0)) != 0)
		{
			if (status == -1)
			{
				WLog_ERR(TAG, "keyboard thread error getting message");
				break;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		UnhookWindowsHookEx(hook_handle);
	}
	else
	{
		WLog_ERR(TAG, "failed to install keyboard hook");
	}

	WLog_DBG(TAG, "Keyboard thread exited.");
	ExitThread(0);
	return 0;
}

int freerdp_client_set_window_size(wfContext* wfc, int width, int height)
{
	WLog_DBG(TAG, "freerdp_client_set_window_size %d, %d", width, height);

	if ((width != wfc->client_width) || (height != wfc->client_height))
	{
		PostThreadMessage(wfc->mainThreadId, WM_SIZE, SIZE_RESTORED,
		                  ((UINT)height << 16) | (UINT)width);
	}

	return 0;
}

void wf_size_scrollbars(wfContext* wfc, UINT32 client_width, UINT32 client_height)
{
	const rdpSettings* settings;
	WINPR_ASSERT(wfc);

	settings = wfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (wfc->disablewindowtracking)
		return;

	// prevent infinite message loop
	wfc->disablewindowtracking = TRUE;

	if (settings->SmartSizing || settings->DynamicResolutionUpdate)
	{
		wfc->xCurrentScroll = 0;
		wfc->yCurrentScroll = 0;

		if (wfc->xScrollVisible || wfc->yScrollVisible)
		{
			if (ShowScrollBar(wfc->hwnd, SB_BOTH, FALSE))
			{
				wfc->xScrollVisible = FALSE;
				wfc->yScrollVisible = FALSE;
			}
		}
	}
	else
	{
		SCROLLINFO si;
		BOOL horiz = wfc->xScrollVisible;
		BOOL vert = wfc->yScrollVisible;

		if (!horiz && client_width < settings->DesktopWidth)
		{
			horiz = TRUE;
		}
		else if (horiz &&
		         client_width >= settings->DesktopWidth /* - GetSystemMetrics(SM_CXVSCROLL)*/)
		{
			horiz = FALSE;
		}

		if (!vert && client_height < settings->DesktopHeight)
		{
			vert = TRUE;
		}
		else if (vert &&
		         client_height >= settings->DesktopHeight /* - GetSystemMetrics(SM_CYHSCROLL)*/)
		{
			vert = FALSE;
		}

		if (horiz == vert && (horiz != wfc->xScrollVisible && vert != wfc->yScrollVisible))
		{
			if (ShowScrollBar(wfc->hwnd, SB_BOTH, horiz))
			{
				wfc->xScrollVisible = horiz;
				wfc->yScrollVisible = vert;
			}
		}

		if (horiz != wfc->xScrollVisible)
		{
			if (ShowScrollBar(wfc->hwnd, SB_HORZ, horiz))
			{
				wfc->xScrollVisible = horiz;
			}
		}

		if (vert != wfc->yScrollVisible)
		{
			if (ShowScrollBar(wfc->hwnd, SB_VERT, vert))
			{
				wfc->yScrollVisible = vert;
			}
		}

		if (horiz)
		{
			// The horizontal scrolling range is defined by
			// (bitmap_width) - (client_width). The current horizontal
			// scroll value remains within the horizontal scrolling range.
			wfc->xMaxScroll = MAX(settings->DesktopWidth - client_width, 0);
			wfc->xCurrentScroll = MIN(wfc->xCurrentScroll, wfc->xMaxScroll);
			si.cbSize = sizeof(si);
			si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
			si.nMin = wfc->xMinScroll;
			si.nMax = settings->DesktopWidth;
			si.nPage = client_width;
			si.nPos = wfc->xCurrentScroll;
			SetScrollInfo(wfc->hwnd, SB_HORZ, &si, TRUE);
		}

		if (vert)
		{
			// The vertical scrolling range is defined by
			// (bitmap_height) - (client_height). The current vertical
			// scroll value remains within the vertical scrolling range.
			wfc->yMaxScroll = MAX(settings->DesktopHeight - client_height, 0);
			wfc->yCurrentScroll = MIN(wfc->yCurrentScroll, wfc->yMaxScroll);
			si.cbSize = sizeof(si);
			si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
			si.nMin = wfc->yMinScroll;
			si.nMax = settings->DesktopHeight;
			si.nPage = client_height;
			si.nPos = wfc->yCurrentScroll;
			SetScrollInfo(wfc->hwnd, SB_VERT, &si, TRUE);
		}
	}

	wfc->disablewindowtracking = FALSE;
	wf_update_canvas_diff(wfc);
}

static BOOL wfreerdp_client_global_init(void)
{
	WSADATA wsaData;

	WSAStartup(0x101, &wsaData);

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);

	return TRUE;
}

static void wfreerdp_client_global_uninit(void)
{
	WSACleanup();
}

static BOOL wfreerdp_client_new(freerdp* instance, rdpContext* context)
{
	wfContext* wfc = (wfContext*)context;
	if (!wfc)
		return FALSE;

	// AttachConsole and stdin do not work well.
	// Use GUI input dialogs instead of command line ones.
	wfc->isConsole = wf_has_console();

	if (!(wfreerdp_client_global_init()))
		return FALSE;

	WINPR_ASSERT(instance);
	instance->PreConnect = wf_pre_connect;
	instance->PostConnect = wf_post_connect;
	instance->PostDisconnect = wf_post_disconnect;
	instance->Authenticate = wf_authenticate;
	instance->GatewayAuthenticate = wf_gw_authenticate;

#ifdef WITH_WINDOWS_CERT_STORE
	freerdp_settings_set_bool(context->settings, FreeRDP_CertificateCallbackPreferPEM, TRUE);
#endif

	if (wfc->isConsole)
	{
		instance->VerifyCertificateEx = wf_cli_verify_certificate_ex;
		instance->VerifyChangedCertificateEx = client_cli_verify_changed_certificate_ex;
		instance->PresentGatewayMessage = client_cli_present_gateway_message;
	}
	else
	{
		instance->VerifyCertificateEx = wf_verify_certificate_ex;
		instance->VerifyChangedCertificateEx = wf_verify_changed_certificate_ex;
		instance->PresentGatewayMessage = wf_present_gateway_message;
	}

#ifdef WITH_PROGRESS_BAR
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_ALL, &IID_ITaskbarList3,
	                 (void**)&wfc->taskBarList);
#endif

	return TRUE;
}

static void wfreerdp_client_free(freerdp* instance, rdpContext* context)
{
	WINPR_UNUSED(instance);
	if (!context)
		return;

#ifdef WITH_PROGRESS_BAR
	CoUninitialize();
#endif
}

static int wfreerdp_client_start(rdpContext* context)
{
	HWND hWndParent;
	HINSTANCE hInstance;
	wfContext* wfc = (wfContext*)context;
	freerdp* instance;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->settings);

	instance = context->instance;
	WINPR_ASSERT(instance);

	hInstance = GetModuleHandle(NULL);
	hWndParent = (HWND)context->settings->ParentWindowId;
	context->settings->EmbeddedWindow = (hWndParent) ? TRUE : FALSE;
	wfc->hWndParent = hWndParent;

	/* initial windows system item position where we will insert new menu item
	 * after default 5 items (restore, move, size, minimize, maximize)
	 * gets incremented each time wf_append_item_to_system_menu is called
	 * or maybe could use GetMenuItemCount() to get initial item count ? */
	wfc->systemMenuInsertPosition = 6;

	wfc->hInstance = hInstance;
	wfc->cursor = LoadCursor(NULL, IDC_ARROW);
	wfc->icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wfc->wndClassName = _tcsdup(_T("FreeRDP"));
	wfc->wndClass.cbSize = sizeof(WNDCLASSEX);
	wfc->wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wfc->wndClass.lpfnWndProc = wf_event_proc;
	wfc->wndClass.cbClsExtra = 0;
	wfc->wndClass.cbWndExtra = 0;
	wfc->wndClass.hCursor = wfc->cursor;
	wfc->wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wfc->wndClass.lpszMenuName = NULL;
	wfc->wndClass.lpszClassName = wfc->wndClassName;
	wfc->wndClass.hInstance = hInstance;
	wfc->wndClass.hIcon = wfc->icon;
	wfc->wndClass.hIconSm = wfc->icon;
	RegisterClassEx(&(wfc->wndClass));
	wfc->keyboardThread =
	    CreateThread(NULL, 0, wf_keyboard_thread, (void*)wfc, 0, &wfc->keyboardThreadId);

	if (!wfc->keyboardThread)
		return -1;

	wfc->common.thread =
	    CreateThread(NULL, 0, wf_client_thread, (void*)instance, 0, &wfc->mainThreadId);

	if (!wfc->common.thread)
		return -1;

	return 0;
}

static int wfreerdp_client_stop(rdpContext* context)
{
	int rc;
	wfContext* wfc = (wfContext*)context;

	WINPR_ASSERT(wfc);
	PostThreadMessage(wfc->mainThreadId, WM_QUIT, 0, 0);
	rc = freerdp_client_common_stop(context);
	wfc->mainThreadId = 0;

	if (wfc->keyboardThread)
	{
		PostThreadMessage(wfc->keyboardThreadId, WM_QUIT, 0, 0);
		WaitForSingleObject(wfc->keyboardThread, INFINITE);
		CloseHandle(wfc->keyboardThread);
		wfc->keyboardThread = NULL;
		wfc->keyboardThreadId = 0;
	}

	return 0;
}

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->Version = 1;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = wfreerdp_client_global_init;
	pEntryPoints->GlobalUninit = wfreerdp_client_global_uninit;
	pEntryPoints->ContextSize = sizeof(wfContext);
	pEntryPoints->ClientNew = wfreerdp_client_new;
	pEntryPoints->ClientFree = wfreerdp_client_free;
	pEntryPoints->ClientStart = wfreerdp_client_start;
	pEntryPoints->ClientStop = wfreerdp_client_stop;
	return 0;
}

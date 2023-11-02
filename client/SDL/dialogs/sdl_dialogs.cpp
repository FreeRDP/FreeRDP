/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#include <vector>
#include <string>
#include <assert.h>

#include <freerdp/log.h>
#include <freerdp/utils/smartcardlogon.h>

#include <SDL.h>

#include "sdl_dialogs.hpp"
#include "sdl_input.hpp"
#include "sdl_input_widgets.hpp"
#include "sdl_select.hpp"
#include "sdl_selectlist.hpp"

#define TAG CLIENT_TAG("SDL.dialogs")

enum
{
	SHOW_DIALOG_ACCEPT_REJECT = 1,
	SHOW_DIALOG_TIMED_ACCEPT = 2
};

static const char* type_str_for_flags(UINT32 flags)
{
	const char* type = "RDP-Server";

	if (flags & VERIFY_CERT_FLAG_GATEWAY)
		type = "RDP-Gateway";

	if (flags & VERIFY_CERT_FLAG_REDIRECT)
		type = "RDP-Redirect";
	return type;
}

static int sdl_show_dialog(rdpContext* context, const char* title, const char* message,
                           Sint32 flags)
{
	if (!sdl_push_user_event(SDL_USEREVENT_SHOW_DIALOG, title, message, flags))
		return 0;

	while (!freerdp_shall_disconnect_context(context))
	{
		SDL_Event event = { 0 };
		const int rc = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_USEREVENT_SHOW_RESULT,
		                              SDL_USEREVENT_SHOW_RESULT);
		if (rc > 0)
			return event.user.code;
		Sleep(1);
	}
	return 0;
}

BOOL sdl_authenticate_ex(freerdp* instance, char** username, char** password, char** domain,
                         rdp_auth_reason reason)
{
	BOOL res = FALSE;

	const char* target = freerdp_settings_get_server_name(instance->context->settings);
	switch (reason)
	{
		case GW_AUTH_HTTP:
		case GW_AUTH_RDG:
		case GW_AUTH_RPC:
			target =
			    freerdp_settings_get_string(instance->context->settings, FreeRDP_GatewayHostname);
			break;
		default:
			break;
	}

	char* title = nullptr;
	size_t titlesize = 0;
	winpr_asprintf(&title, &titlesize, "Credentials required for %s", target);

	char* u = nullptr;
	char* d = nullptr;
	char* p = nullptr;

	assert(username);
	assert(domain);
	assert(password);

	u = *username;
	d = *domain;
	p = *password;

	if (!sdl_push_user_event(SDL_USEREVENT_AUTH_DIALOG, title, u, d, p, reason))
		goto fail;

	while (!freerdp_shall_disconnect_context(instance->context))
	{
		SDL_Event event = { 0 };
		const int rc = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_USEREVENT_AUTH_RESULT,
		                              SDL_USEREVENT_AUTH_RESULT);
		if (rc > 0)
		{
			SDL_UserAuthArg* arg = reinterpret_cast<SDL_UserAuthArg*>(event.padding);

			res = arg->result != 0 ? TRUE : FALSE;

			free(*username);
			free(*domain);
			free(*password);
			*username = arg->user;
			*domain = arg->domain;
			*password = arg->password;
			break;
		}
		Sleep(1);
	}
fail:
	free(title);
	return res;
}

BOOL sdl_choose_smartcard(freerdp* instance, SmartcardCertInfo** cert_list, DWORD count,
                          DWORD* choice, BOOL gateway)
{
	BOOL res = FALSE;

	std::vector<std::string> strlist;
	std::vector<const char*> list;
	for (DWORD i = 0; i < count; i++)
	{
		const SmartcardCertInfo* cert = cert_list[i];
		char* reader = ConvertWCharToUtf8Alloc(cert->reader, nullptr);
		char* container_name = ConvertWCharToUtf8Alloc(cert->containerName, nullptr);

		char* msg = nullptr;
		size_t len = 0;

		winpr_asprintf(&msg, &len,
		               "%s\n\tReader: %s\n\tUser: %s@%s\n\tSubject: %s\n\tIssuer: %s\n\tUPN: %s",
		               container_name, reader, cert->userHint, cert->domainHint, cert->subject,
		               cert->issuer, cert->upn);

		strlist.push_back(msg);
		free(msg);
		free(reader);
		free(container_name);

		auto m = strlist.back();
		list.push_back(m.c_str());
	}

	const char* title = "Select a logon smartcard certificate";
	if (gateway)
		title = "Select a gateway logon smartcard certificate";
	if (!sdl_push_user_event(SDL_USEREVENT_SCARD_DIALOG, title, list.data(), count))
		goto fail;

	while (!freerdp_shall_disconnect_context(instance->context))
	{
		SDL_Event event = { 0 };
		const int rc = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_USEREVENT_SCARD_RESULT,
		                              SDL_USEREVENT_SCARD_RESULT);
		if (rc > 0)
		{
			res = TRUE;
			*choice = static_cast<DWORD>(event.user.code);
			break;
		}
		Sleep(1);
	}

fail:
	return res;
}

BOOL sdl_present_gateway_message(freerdp* instance, UINT32 type, BOOL isDisplayMandatory,
                                 BOOL isConsentMandatory, size_t length, const WCHAR* wmessage)
{
	if (!isDisplayMandatory)
		return TRUE;

	char* title = nullptr;
	size_t len = 0;
	winpr_asprintf(&title, &len, "[gateway]");

	Sint32 flags = 0;
	if (isConsentMandatory)
		flags = SHOW_DIALOG_ACCEPT_REJECT;
	else if (isDisplayMandatory)
		flags = SHOW_DIALOG_TIMED_ACCEPT;
	char* message = ConvertWCharNToUtf8Alloc(wmessage, length, nullptr);

	const int rc = sdl_show_dialog(instance->context, title, message, flags);
	free(title);
	free(message);
	return rc > 0 ? TRUE : FALSE;
}

int sdl_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	int rc = -1;
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);

	if (!instance || !instance->context)
		return -1;

	/* ignore LOGON_MSG_SESSION_CONTINUE message */
	if (type == LOGON_MSG_SESSION_CONTINUE)
		return 0;

	char* title = nullptr;
	size_t tlen = 0;
	winpr_asprintf(&title, &tlen, "[%s] info",
	               freerdp_settings_get_server_name(instance->context->settings));

	char* message = nullptr;
	size_t mlen = 0;
	winpr_asprintf(&message, &mlen, "Logon Error Info %s [%s]", str_data, str_type);

	rc = sdl_show_dialog(instance->context, title, message, SHOW_DIALOG_ACCEPT_REJECT);
	free(title);
	free(message);
	return rc;
}

static DWORD sdl_show_ceritifcate_dialog(rdpContext* context, const char* title,
                                         const char* message)
{
	if (!sdl_push_user_event(SDL_USEREVENT_CERT_DIALOG, title, message))
		return 0;

	while (!freerdp_shall_disconnect_context(context))
	{
		SDL_Event event = { 0 };
		const int rc = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_USEREVENT_CERT_RESULT,
		                              SDL_USEREVENT_CERT_RESULT);
		if (rc > 0)
			return static_cast<DWORD>(event.user.code);
		Sleep(1);
	}
	return 0;
}

DWORD sdl_verify_changed_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                        const char* common_name, const char* subject,
                                        const char* issuer, const char* new_fingerprint,
                                        const char* old_subject, const char* old_issuer,
                                        const char* old_fingerprint, DWORD flags)
{
	const char* type = type_str_for_flags(flags);

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	/* Newer versions of FreeRDP allow exposing the whole PEM by setting
	 * FreeRDP_CertificateCallbackPreferPEM to TRUE
	 */
	char* new_fp_str = nullptr;
	size_t len = 0;
	if (flags & VERIFY_CERT_FLAG_FP_IS_PEM)
	{
		winpr_asprintf(&new_fp_str, &len,
		               "----------- Certificate --------------\n"
		               "%s\n"
		               "--------------------------------------\n",
		               new_fingerprint);
	}
	else
		winpr_asprintf(&new_fp_str, &len, "Thumbprint:  %s\n", new_fingerprint);

	/* Newer versions of FreeRDP allow exposing the whole PEM by setting
	 * FreeRDP_CertificateCallbackPreferPEM to TRUE
	 */
	char* old_fp_str = nullptr;
	size_t olen = 0;
	if (flags & VERIFY_CERT_FLAG_FP_IS_PEM)
	{
		winpr_asprintf(&old_fp_str, &olen,
		               "----------- Certificate --------------\n"
		               "%s\n"
		               "--------------------------------------\n",
		               old_fingerprint);
	}
	else
		winpr_asprintf(&old_fp_str, &olen, "Thumbprint:  %s\n", old_fingerprint);

	const char* collission_str = "";
	if (flags & VERIFY_CERT_FLAG_MATCH_LEGACY_SHA1)
	{
		collission_str =
		    "A matching entry with legacy SHA1 was found in local known_hosts2 store.\n"
		    "If you just upgraded from a FreeRDP version before 2.0 this is expected.\n"
		    "The hashing algorithm has been upgraded from SHA1 to SHA256.\n"
		    "All manually accepted certificates must be reconfirmed!\n"
		    "\n";
	}

	char* title = nullptr;
	size_t tlen = 0;
	winpr_asprintf(&title, &tlen, "Certificate for %s:%" PRIu16 " (%s) has changed", host, port,
	               type);

	char* message = nullptr;
	size_t mlen = 0;
	winpr_asprintf(&message, &mlen,
	               "New Certificate details:\n"
	               "Common Name: %s\n"
	               "Subject:     %s\n"
	               "Issuer:      %s\n"
	               "%s\n"
	               "Old Certificate details:\n"
	               "Subject:     %s\n"
	               "Issuer:      %s\n"
	               "%s\n"
	               "%s\n"
	               "The above X.509 certificate does not match the certificate used for previous "
	               "connections.\n"
	               "This may indicate that the certificate has been tampered with.\n"
	               "Please contact the administrator of the RDP server and clarify.\n",
	               common_name, subject, issuer, new_fp_str, old_subject, old_issuer, old_fp_str,
	               collission_str);

	const DWORD rc = sdl_show_ceritifcate_dialog(instance->context, title, message);
	free(title);
	free(message);
	free(new_fp_str);
	free(old_fp_str);

	return rc;
}

DWORD sdl_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                const char* common_name, const char* subject, const char* issuer,
                                const char* fingerprint, DWORD flags)
{
	const char* type = type_str_for_flags(flags);

	/* Newer versions of FreeRDP allow exposing the whole PEM by setting
	 * FreeRDP_CertificateCallbackPreferPEM to TRUE
	 */
	char* fp_str = nullptr;
	size_t len = 0;
	if (flags & VERIFY_CERT_FLAG_FP_IS_PEM)
	{
		winpr_asprintf(&fp_str, &len,
		               "----------- Certificate --------------\n"
		               "%s\n"
		               "--------------------------------------\n",
		               fingerprint);
	}
	else
		winpr_asprintf(&fp_str, &len, "Thumbprint:  %s\n", fingerprint);

	char* title = nullptr;
	size_t tlen = 0;
	winpr_asprintf(&title, &tlen, "New certificate for %s:%" PRIu16 " (%s)", host, port, type);

	char* message = nullptr;
	size_t mlen = 0;
	winpr_asprintf(
	    &message, &mlen,
	    "Common Name: %s\n"
	    "Subject:     %s\n"
	    "Issuer:      %s\n"
	    "%s\n"
	    "The above X.509 certificate could not be verified, possibly because you do not have\n"
	    "the CA certificate in your certificate store, or the certificate has expired.\n"
	    "Please look at the OpenSSL documentation on how to add a private CA to the store.\n",
	    common_name, subject, issuer, fp_str);

	const DWORD rc = sdl_show_ceritifcate_dialog(instance->context, title, message);
	free(fp_str);
	free(title);
	free(message);
	return rc;
}

BOOL sdl_cert_dialog_show(const char* title, const char* message)
{
	int buttonid = -1;
	enum
	{
		BUTTONID_CERT_ACCEPT_PERMANENT = 23,
		BUTTONID_CERT_ACCEPT_TEMPORARY = 24,
		BUTTONID_CERT_DENY = 25
	};
	const SDL_MessageBoxButtonData buttons[] = {
		{ 0, BUTTONID_CERT_ACCEPT_PERMANENT, "permanent" },
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, BUTTONID_CERT_ACCEPT_TEMPORARY, "temporary" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, BUTTONID_CERT_DENY, "cancel" }
	};

	const SDL_MessageBoxData data = { SDL_MESSAGEBOX_WARNING, nullptr, title,  message,
		                              ARRAYSIZE(buttons),     buttons, nullptr };
	const int rc = SDL_ShowMessageBox(&data, &buttonid);

	Sint32 value = -1;
	if (rc < 0)
		value = 0;
	else
	{
		switch (buttonid)
		{
			case BUTTONID_CERT_ACCEPT_PERMANENT:
				value = 1;
				break;
			case BUTTONID_CERT_ACCEPT_TEMPORARY:
				value = 2;
				break;
			default:
				value = 0;
				break;
		}
	}

	return sdl_push_user_event(SDL_USEREVENT_CERT_RESULT, value);
}

BOOL sdl_message_dialog_show(const char* title, const char* message, Sint32 flags)
{
	int buttonid = -1;
	enum
	{
		BUTTONID_SHOW_ACCEPT = 24,
		BUTTONID_SHOW_DENY = 25
	};
	const SDL_MessageBoxButtonData buttons[] = {
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, BUTTONID_SHOW_ACCEPT, "accept" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, BUTTONID_SHOW_DENY, "cancel" }
	};

	const int button_cnt = (flags & SHOW_DIALOG_ACCEPT_REJECT) ? 2 : 1;
	const SDL_MessageBoxData data = {
		SDL_MESSAGEBOX_WARNING, nullptr, title, message, button_cnt, buttons, nullptr
	};
	const int rc = SDL_ShowMessageBox(&data, &buttonid);

	Sint32 value = -1;
	if (rc < 0)
		value = 0;
	else
	{
		switch (buttonid)
		{
			case BUTTONID_SHOW_ACCEPT:
				value = 1;
				break;
			default:
				value = 0;
				break;
		}
	}

	return sdl_push_user_event(SDL_USEREVENT_SHOW_RESULT, value);
}

BOOL sdl_auth_dialog_show(const SDL_UserAuthArg* args)
{
	const std::vector<std::string> auth = { "Username:        ", "Domain:          ",
		                                    "Password:        " };
	const std::vector<std::string> authPin = { "Device:       ", "PIN:        " };
	const std::vector<std::string> gw = { "GatewayUsername: ", "GatewayDomain:   ",
		                                  "GatewayPassword: " };
	std::vector<std::string> prompt;
	Sint32 rc = -1;

	switch (args->result)
	{
		case AUTH_SMARTCARD_PIN:
			prompt = authPin;
			break;
		case AUTH_TLS:
		case AUTH_RDP:
		case AUTH_NLA:
			prompt = auth;
			break;
		case GW_AUTH_HTTP:
		case GW_AUTH_RDG:
		case GW_AUTH_RPC:
			prompt = gw;
			break;
		default:
			break;
	}

	std::vector<std::string> result;

	if (!prompt.empty())
	{
		std::vector<std::string> initial{ args->user ? args->user : "Smartcard", "" };
		std::vector<Uint32> flags = { SdlInputWidget::SDL_INPUT_READONLY,
			                          SdlInputWidget::SDL_INPUT_MASK };
		if (args->result != AUTH_SMARTCARD_PIN)
		{
			initial = { args->user ? args->user : "", args->domain ? args->domain : "",
				        args->password ? args->password : "" };
			flags = { 0, 0, SdlInputWidget::SDL_INPUT_MASK };
		}
		SdlInputWidgetList ilist(args->title, prompt, initial, flags);
		rc = ilist.run(result);
	}

	if ((rc <= 0) || (result.size() < prompt.size()))
		return FALSE;

	return sdl_push_user_event(SDL_USEREVENT_AUTH_RESULT, _strdup(result[0].c_str()),
	                           _strdup(result[1].c_str()), _strdup(result[2].c_str()), rc);
}

BOOL sdl_scard_dialog_show(const char* title, Sint32 count, const char** list)
{
	std::vector<std::string> vlist;
	for (Sint32 x = 0; x < count; x++)
		vlist.push_back(list[x]);
	SdlSelectList slist(title, vlist);
	Sint32 value = slist.run();
	return sdl_push_user_event(SDL_USEREVENT_SCARD_RESULT, value);
}

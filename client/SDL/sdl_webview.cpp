/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Popup browser for AAD authentication
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <QApplication>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>

#include <string>
#include <stdlib.h>
#include <stdarg.h>
#include <winpr/string.h>
#include <freerdp/log.h>

#include "sdl_webview.hpp"

#define TAG CLIENT_TAG("sdl.webview")

class SchemeHandler : public QWebEngineUrlSchemeHandler
{
  public:
	SchemeHandler(QObject* parent = nullptr) : QWebEngineUrlSchemeHandler(parent)
	{
	}

	void requestStarted(QWebEngineUrlRequestJob* request) override
	{
		QUrl url = request->requestUrl();

		int rc = -1;
		for (auto& param : url.query().split('&'))
		{
			QStringList pair = param.split('=');

			if (pair.size() != 2 || pair[0] != "code")
				continue;

			auto qc = pair[1];
			m_code = qc.toStdString();
			rc = 0;
			break;
		}
		qApp->exit(rc);
	}

	const std::string code() const
	{
		return m_code;
	}

  private:
	std::string m_code;
};

static std::string sdl_webview_get_auth_code(QString url)
{
	int argc = 1;
	std::string name = "FreeRDP WebView";
	char* argv[] = { name.data() };

	QWebEngineUrlScheme::registerScheme(QWebEngineUrlScheme("ms-appx-web"));

	QCoreApplication::setOrganizationName("QtExamples");
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication app(argc, argv);

	SchemeHandler handler;
	QWebEngineProfile::defaultProfile()->installUrlSchemeHandler("ms-appx-web", &handler);

	QWebEngineView webview;
	webview.load(QUrl(url));
	webview.show();

	if (app.exec() != 0)
		return "";

	return handler.code();
}

static BOOL sdl_webview_get_rdsaad_access_token(freerdp* instance, const char* scope,
                                                const char* req_cnf, char** token)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(scope);
	WINPR_ASSERT(req_cnf);
	WINPR_ASSERT(token);

	WINPR_UNUSED(instance);

	std::string client_id = "5177bc73-fd99-4c77-a90c-76844c9b6999";
	std::string redirect_uri =
	    "ms-appx-web%3a%2f%2fMicrosoft.AAD.BrokerPlugin%2f5177bc73-fd99-4c77-a90c-76844c9b6999";

	*token = nullptr;

	auto url = QString::fromStdString(
	    "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?client_id=" + client_id +
	    "&response_type=code&scope=" + scope + "&redirect_uri=" + redirect_uri);

	auto code = sdl_webview_get_auth_code(url);
	if (code.empty())
		return FALSE;

	auto token_request = "grant_type=authorization_code&code=" + code + "&client_id=" + client_id +
	                     "&scope=" + scope + "&redirect_uri=" + redirect_uri +
	                     "&req_cnf=" + req_cnf;
	return client_common_get_access_token(instance, token_request.c_str(), token);
}

static BOOL sdl_webview_get_avd_access_token(freerdp* instance, char** token)
{
	WINPR_ASSERT(token);

	std::string client_id = "a85cf173-4192-42f8-81fa-777a763e6e2c";
	std::string redirect_uri =
	    "ms-appx-web%3a%2f%2fMicrosoft.AAD.BrokerPlugin%2fa85cf173-4192-42f8-81fa-777a763e6e2c";
	std::string scope = "https%3A%2F%2Fwww.wvd.microsoft.com%2F.default";

	*token = nullptr;

	auto url = QString::fromStdString(
	    "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?client_id=" + client_id +
	    "&response_type=code&scope=" + scope + "&redirect_uri=" + redirect_uri);
	auto code = sdl_webview_get_auth_code(url);
	if (code.empty())
		return FALSE;

	auto token_request = "grant_type=authorization_code&code=" + code + "&client_id=" + client_id +
	                     "&scope=" + scope + "&redirect_uri=" + redirect_uri;
	return client_common_get_access_token(instance, token_request.c_str(), token);
}

BOOL sdl_webview_get_access_token(freerdp* instance, AccessTokenType tokenType, char** token,
                                  size_t count, ...)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(token);
	switch (tokenType)
	{
		case ACCESS_TOKEN_TYPE_AAD:
		{
			if (count < 2)
			{
				WLog_ERR(TAG,
				         "ACCESS_TOKEN_TYPE_AAD expected 2 additional arguments, but got %" PRIuz
				         ", aborting",
				         count);
				return FALSE;
			}
			else if (count > 2)
				WLog_WARN(TAG,
				          "ACCESS_TOKEN_TYPE_AAD expected 2 additional arguments, but got %" PRIuz
				          ", ignoring",
				          count);
			va_list ap;
			va_start(ap, count);
			const char* scope = va_arg(ap, const char*);
			const char* req_cnf = va_arg(ap, const char*);
			const BOOL rc = sdl_webview_get_rdsaad_access_token(instance, scope, req_cnf, token);
			va_end(ap);
			return rc;
		}
		case ACCESS_TOKEN_TYPE_AVD:
			if (count != 0)
				WLog_WARN(TAG,
				          "ACCESS_TOKEN_TYPE_AVD expected 0 additional arguments, but got %" PRIuz
				          ", ignoring",
				          count);
			return sdl_webview_get_avd_access_token(instance, token);
		default:
			WLog_ERR(TAG, "Unexpected value for AccessTokenType [%" PRIuz "], aborting", tokenType);
			return FALSE;
	}
}

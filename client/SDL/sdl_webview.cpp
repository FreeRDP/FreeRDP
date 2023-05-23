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
#include <winpr/string.h>

#include "sdl_webview.hpp"

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
		m_code;
	}

  private:
	std::string m_code;
};

BOOL sdl_webview_get_aad_auth_code(freerdp* instance, const char* hostname, char** code,
                                   const char** client_id, const char** redirect_uri)
{
	int argc = 1;
	std::string name = "FreeRDP WebView";

	WINPR_ASSERT(instance);
	WINPR_ASSERT(hostname);
	WINPR_ASSERT(code);
	WINPR_ASSERT(client_id);
	WINPR_ASSERT(redirect_uri);

	WINPR_UNUSED(instance);

	*code = nullptr;
	*client_id = "5177bc73-fd99-4c77-a90c-76844c9b6999";
	*redirect_uri =
	    "ms-appx-web%3a%2f%2fMicrosoft.AAD.BrokerPlugin%2f5177bc73-fd99-4c77-a90c-76844c9b6999";

	auto url = QString("https://login.microsoftonline.com/common/oauth2/v2.0/"
	                   "authorize?client_id=%1&response_type="
	                   "code&scope=ms-device-service%%3A%%2F%%2Ftermsrv.wvd.microsoft.com%%2Fname%%"
	                   "2F%2%%2Fuser_impersonation&redirect_uri=%3")
	               .arg(*client_id)
	               .arg(hostname)
	               .arg(*redirect_uri);

	QWebEngineUrlScheme::registerScheme(QWebEngineUrlScheme("ms-appx-web"));

	char* argv[] = { name.data() };
	QCoreApplication::setOrganizationName("QtExamples");
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication app(argc, argv);

	SchemeHandler handler;
	QWebEngineProfile::defaultProfile()->installUrlSchemeHandler("ms-appx-web", &handler);

	QWebEngineView webview;
	webview.load(QUrl(url));
	webview.show();

	if (app.exec() != 0)
		return FALSE;

	auto val = handler.code();
	if (val.empty())
		return FALSE;
	*code = _strdup(val.c_str());

	return (*code != nullptr) ? TRUE : FALSE;
}

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

#include <stdlib.h>
#include <winpr/string.h>

#include "sdl_webview.hpp"

class SchemeHandler : public QWebEngineUrlSchemeHandler
{
  public:
	SchemeHandler(char** code, QObject* parent = nullptr)
	    : QWebEngineUrlSchemeHandler(parent), codeptr(code){};

	void requestStarted(QWebEngineUrlRequestJob* request)
	{
		QUrl url = request->requestUrl();

		for (auto& param : url.query().split('&'))
		{
			QStringList pair = param.split('=');

			if (pair.size() != 2 || pair[0] != "code")
				continue;

			QByteArray code = pair[1].toUtf8();
			*codeptr = (char*)calloc(1, code.size() + 1);
			strcpy(*codeptr, code.constData());
			break;
		}
		qApp->exit();
	}

  private:
	char** codeptr;
};

BOOL sdl_webview_get_aad_auth_code(freerdp* instance, const char* hostname, char** code,
                                   const char** client_id, const char** redirect_uri)
{
	int argc = 1;
	char* name = "FreeRDP WebView";
	size_t size = 0;
	char* login_url = nullptr;

	*code = nullptr;
	*client_id = "5177bc73-fd99-4c77-a90c-76844c9b6999";
	*redirect_uri =
	    "ms-appx-web%3a%2f%2fMicrosoft.AAD.BrokerPlugin%2f5177bc73-fd99-4c77-a90c-76844c9b6999";

	winpr_asprintf(&login_url, &size,
	               "https://login.microsoftonline.com/common/oauth2/v2.0/"
	               "authorize?client_id=%s&response_type="
	               "code&scope=ms-device-service%%3A%%2F%%2Ftermsrv.wvd.microsoft.com%%2Fname%%"
	               "2F%s%%2Fuser_impersonation&redirect_uri=%s",
	               *client_id, hostname, *redirect_uri);

	QWebEngineUrlScheme::registerScheme(QWebEngineUrlScheme("ms-appx-web"));

	QApplication app(argc, &name);

	SchemeHandler handler(code);
	QWebEngineProfile::defaultProfile()->installUrlSchemeHandler("ms-appx-web", &handler);

	QWebEngineView webview;
	webview.load(QUrl(login_url));
	webview.show();

	app.exec();

	free(login_url);
	return (*code != nullptr);
}

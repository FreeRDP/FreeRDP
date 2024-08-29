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
#include <cstdlib>
#include <cstdarg>
#include <winpr/string.h>
#include <winpr/assert.h>
#include <freerdp/log.h>
#include <freerdp/build-config.h>

#include "../webview_impl.hpp"

#define TAG CLIENT_TAG("sdl.webview")

class SchemeHandler : public QWebEngineUrlSchemeHandler
{
  public:
	explicit SchemeHandler(QObject* parent = nullptr) : QWebEngineUrlSchemeHandler(parent)
	{
	}

	void requestStarted(QWebEngineUrlRequestJob* request) override
	{
		QUrl url = request->requestUrl();

		int rc = -1;
		for (auto& param : url.query().split('&'))
		{
			QStringList pair = param.split('=');

			if (pair.size() != 2 || pair[0] != QLatin1String("code"))
				continue;

			auto qc = pair[1];
			m_code = qc.toStdString();
			rc = 0;
			break;
		}
		qApp->exit(rc);
	}

	[[nodiscard]] std::string code() const
	{
		return m_code;
	}

  private:
	std::string m_code{};
};

bool webview_impl_run(const std::string& title, const std::string& url, std::string& code)
{
	int argc = 1;
	const auto vendor = QLatin1String(FREERDP_VENDOR_STRING);
	const auto product = QLatin1String(FREERDP_PRODUCT_STRING);
	QWebEngineUrlScheme::registerScheme(QWebEngineUrlScheme("ms-appx-web"));

	std::string wtitle = title;
	char* argv[] = { wtitle.data() };
	QCoreApplication::setOrganizationName(vendor);
	QCoreApplication::setApplicationName(product);
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication app(argc, argv);

	SchemeHandler handler;
	QWebEngineProfile::defaultProfile()->installUrlSchemeHandler("ms-appx-web", &handler);

	QWebEngineView webview;
	webview.load(QUrl(QString::fromStdString(url)));
	webview.show();

	if (app.exec() != 0)
		return false;

	auto val = handler.code();
	if (val.empty())
		return false;
	code = val;

	return !code.empty();
}

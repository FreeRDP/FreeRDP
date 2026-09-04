/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Out-of-process Qt-based AAD auth helper - JSON-RPC front end over dedicated pipes
 *
 * Speaks the same protocol as the webview-based helper (see
 * client/common/webview-aad-helper/main.cpp and client/common/aad-auth-helper-protocol.md)
 * and is just as "dumb": it is handed a URL to show and a redirect_uri prefix to watch for, and
 * it hands back whatever URI the browser eventually navigated to. This variant drives a
 * QWebEngineView instead of a native OS webview widget.
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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
#include <QCloseEvent>
#include <QMetaObject>
#include <QString>
#include <QUrl>
#include <QWebEngineNavigationRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <winpr/file.h>
#include <winpr/handle.h>
#include <winpr/json.h>

#include "redirect_watcher.hpp"

constexpr uint32_t kDefaultTimeoutMs = 180000;

/** AAD's native-broker OAuth2 redirect_uri (FreeRDP's own default, see
 * FreeRDP_GatewayAvdAccessTokenFormat in libfreerdp/core/settings.c) uses the non-standard
 * "ms-appx-web" scheme, e.g. ms-appx-web://Microsoft.AAD.BrokerPlugin/<client_id>. Chromium has no
 * built-in navigation support for such schemes: without registering it first, a mid-flow redirect
 * to one never reaches navigationRequested() at all - Chromium instead tries to hand it off to the
 * desktop environment as an "external protocol" (noisy "KApplicationTrader: mimeType ... not
 * found" logs on KDE, xdg-open elsewhere), and the redirect is lost. QWebEngineUrlScheme::
 * registerScheme() (in main(), before QApplication) makes Chromium treat navigations to it like
 * any other scheme, so navigationRequested() below fires and can reject() it before it ever loads
 * anything. installUrlSchemeHandler() (also in main()) additionally installs BrokerSchemeHandler
 * as a second, request-level line of defense in case something still reaches this scheme without
 * going through navigationRequested() first. */
constexpr const char* kBrokerScheme = "ms-appx-web";

struct PendingResult
{
	std::mutex mtx;
	std::condition_variable cv;
	bool done = false;
	bool ok = false;
	std::string redirectUrl;
	std::string errorMessage;
};

class Session;

/** Top-level browser window; forwards navigation/close events to the owning Session. Lives on the
 *  Qt/GUI thread for its whole lifetime - created lazily on the first navigate() call and reused
 *  (together with its QWebEngineProfile, so cookies persist) across the rest of the process'
 *  life, the same way the webview-based helper reuses its single native widget.
 *
 *  Redirect matching is done from navigationRequested(), not urlChanged(): OAuth2 redirect_uris
 *  are frequently non-navigable signal URIs (e.g. AAD's native-client broker uses
 *  ms-appx-web://microsoft.aad.brokerplugin/...), and QtWebEngine will otherwise still try to
 *  resolve/load them - on Linux that means handing the scheme off to the desktop environment
 *  (noisy "KApplicationTrader: mimeType ... not found" logs on KDE, an actual xdg-open elsewhere)
 *  before giving up. Intercepting in navigationRequested() rejects the navigation before
 *  QtWebEngine ever attempts to load or externally resolve it. */
class AuthWindow : public QWebEngineView
{
	Q_OBJECT
  public:
	explicit AuthWindow(Session& owner);

  protected:
	void closeEvent(QCloseEvent* event) override;

  private slots:
	void onNavigationRequested(QWebEngineNavigationRequest& request);

  private:
	Session& session;
};

/** Owns the single AuthWindow for the lifetime of the helper process. All calls other than the
 *  ones made directly by the Qt event loop itself may come from the cmdIn-reader thread and are
 *  marshalled onto the UI thread via QMetaObject::invokeMethod(). */
class Session
{
  public:
	bool navigate(const std::string& title, const std::string& url, const std::string& redirectUri,
	              uint32_t timeoutMs, std::string& redirectUrl, std::string& error)
	{
		PendingResult pending;
		{
			std::lock_guard<std::mutex> lock(stateMtx);
			if (current)
			{
				error = "navigate_already_in_progress";
				return false;
			}
			current = &pending;
			watcher = RedirectWatcher(redirectUri);
		}

		const QString qtitle = QString::fromStdString(title);
		const QUrl qurl(QString::fromStdString(url));
		QMetaObject::invokeMethod(
		    qApp,
		    [this, qtitle, qurl]()
		    {
			    if (!window)
				    window = new AuthWindow(*this);
			    window->setWindowTitle(qtitle);
			    window->resize(800, 600);
			    window->show();
			    window->raise();
			    window->activateWindow();
			    window->setUrl(qurl);
		    },
		    Qt::QueuedConnection);

		const uint32_t timeout = timeoutMs ? timeoutMs : kDefaultTimeoutMs;
		bool signalled = false;
		{
			std::unique_lock<std::mutex> lock(pending.mtx);
			signalled = pending.cv.wait_for(lock, std::chrono::milliseconds(timeout),
			                                [&] { return pending.done; });
		}

		{
			std::lock_guard<std::mutex> lock(stateMtx);
			/* still ours: nobody finished it (timeout) - detach the now-invalid pointer to the
			 * stack-local `pending` before it goes out of scope */
			if (current == &pending)
				current = nullptr;
		}

		/* the popup has served its purpose once this navigate() is done, whatever the outcome -
		 * hide it now rather than leaving it sitting on screen until the next navigate() (if any)
		 * reuses and re-shows it. */
		QMetaObject::invokeMethod(
		    qApp,
		    [this]()
		    {
			    if (window)
				    window->hide();
		    },
		    Qt::QueuedConnection);

		if (!signalled)
		{
			error = "timeout";
			return false;
		}

		if (!pending.ok)
		{
			error = pending.errorMessage.empty() ? "user_cancelled" : pending.errorMessage;
			return false;
		}

		redirectUrl = pending.redirectUrl;
		return true;
	}

	void cancel()
	{
		std::lock_guard<std::mutex> lock(stateMtx);
		finishCurrentLocked(false, "", "user_cancelled");
	}

	void quit()
	{
		{
			std::lock_guard<std::mutex> lock(stateMtx);
			finishCurrentLocked(false, "", "shutting_down");
		}
		/* the window (and the QWebEnginePage/profile reference it holds) must be gone before
		 * QApplication's destructor tears down the default QWebEngineProfile, or QtWebEngine
		 * warns "Release of profile requested but WebEnginePage still not deleted." Deleting it
		 * here, still inside the running event loop, is what QtWebEngine expects. */
		QMetaObject::invokeMethod(
		    qApp,
		    [this]()
		    {
			    delete window;
			    window = nullptr;
			    qApp->quit();
		    },
		    Qt::QueuedConnection);
	}

	/* called on the UI thread by AuthWindow::onNavigationRequested. Returns true if `uri` matched
	 * the pending redirect_uri (in which case the pending navigate() has been resolved and the
	 * caller must reject the navigation rather than let QtWebEngine try to load it). */
	bool onNavigate(const std::string& uri)
	{
		std::lock_guard<std::mutex> lock(stateMtx);
		if (!current || !watcher.matches(uri))
			return false;

		std::string err;
		if (watcher.hasError(uri, err))
			finishCurrentLocked(false, "", err);
		else
			finishCurrentLocked(true, uri, "");
		return true;
	}

	/* called on the UI thread by AuthWindow::closeEvent, e.g. the user closed the popup */
	void onWindowClosed()
	{
		std::lock_guard<std::mutex> lock(stateMtx);
		finishCurrentLocked(false, "", "user_cancelled");
	}

  private:
	/* caller must hold stateMtx */
	void finishCurrentLocked(bool ok, const std::string& redirectUrl, const std::string& err)
	{
		if (!current)
			return;
		{
			std::lock_guard<std::mutex> lock(current->mtx);
			current->done = true;
			current->ok = ok;
			current->redirectUrl = redirectUrl;
			current->errorMessage = err;
		}
		current->cv.notify_all();
		current = nullptr;
	}

	std::mutex stateMtx;
	PendingResult* current = nullptr;
	RedirectWatcher watcher{ std::string() };
	AuthWindow* window = nullptr; /* UI thread only */
};

AuthWindow::AuthWindow(Session& owner) : session(owner)
{
	connect(page(), &QWebEnginePage::navigationRequested, this, &AuthWindow::onNavigationRequested);
}

void AuthWindow::onNavigationRequested(QWebEngineNavigationRequest& request)
{
	if (session.onNavigate(request.url().toString().toStdString()))
		request.reject();
}

void AuthWindow::closeEvent(QCloseEvent* event)
{
	session.onWindowClosed();
	QWebEngineView::closeEvent(event);
}

/** Catches redirects to the registered kBrokerScheme (see its comment above). requestStarted()
 *  runs on the profile's IO thread, not the Qt UI thread - Session::onNavigate() is safe to call
 *  from there since it only ever touches its own mutex-guarded state. No content is ever served
 *  for these requests; failing the job just stops the (already-fake) navigation from getting any
 *  further, whether or not it matched a pending navigate(). */
class BrokerSchemeHandler : public QWebEngineUrlSchemeHandler
{
  public:
	explicit BrokerSchemeHandler(Session& owner) : session(owner)
	{
	}

	void requestStarted(QWebEngineUrlRequestJob* job) override
	{
		session.onNavigate(job->requestUrl().toString().toStdString());
		job->fail(QWebEngineUrlRequestJob::RequestAborted);
	}

  private:
	Session& session;
};

namespace
{

	/* the JSON-RPC channel: handles imported from the --cmdInFd=/--cmdOutFd= command line arguments
	 * (see main()), not stdin/stdout - so this helper's own stdio stays free for its normal
	 * diagnostic output (Qt/Chromium warnings, WLog, etc.) instead of colliding with the protocol.
	 */
	HANDLE g_cmdIn = nullptr;
	HANDLE g_cmdOut = nullptr;

	bool writeLine(const std::string& line)
	{
		std::string data = line;
		data += "\n";

		size_t written = 0;
		while (written < data.size())
		{
			DWORD dwWritten = 0;
			if (!WriteFile(g_cmdOut, data.data() + written,
			               static_cast<DWORD>(data.size() - written), &dwWritten, nullptr) ||
			    (dwWritten == 0))
				return false;
			written += dwWritten;
		}
		return true;
	}

	/* extracts one '\n'-terminated line already buffered in `buf`, if any */
	bool extractLine(std::string& buf, std::string& line)
	{
		const auto pos = buf.find('\n');
		if (pos == std::string::npos)
			return false;
		line = buf.substr(0, pos);
		buf.erase(0, pos + 1);
		return true;
	}

	bool readLine(std::string& buf, std::string& line)
	{
		if (extractLine(buf, line))
			return true;

		while (true)
		{
			char chunk[4096];
			DWORD dwRead = 0;
			if (!ReadFile(g_cmdIn, chunk, sizeof(chunk), &dwRead, nullptr) || (dwRead == 0))
				return false;
			buf.append(chunk, dwRead);
			if (extractLine(buf, line))
				return true;
		}
	}

	void sendLine(const std::string& line)
	{
		(void)writeLine(line);
	}

	void sendJson(WINPR_JSON* obj)
	{
		char* str = WINPR_JSON_PrintUnformatted(obj);
		if (str)
		{
			sendLine(str);
			free(str);
		}
		WINPR_JSON_Delete(obj);
	}

	void sendHelloResult(int64_t id)
	{
		WINPR_JSON* obj = WINPR_JSON_CreateObject();
		bool ok = WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0") &&
		          WINPR_JSON_AddNumberToObject(obj, "id", static_cast<double>(id));
		WINPR_JSON* result = ok ? WINPR_JSON_AddObjectToObject(obj, "result") : nullptr;
		if (result)
		{
			bool unused =
			    WINPR_JSON_AddIntegerToObject(result, "protocol_version", 1) &&
			    WINPR_JSON_AddStringToObject(result, "helper", "freerdp-qt-aad-helper/1.0");
			(void)unused;
		}
		sendJson(obj);
	}

	void sendNavigateResult(int64_t id, const std::string& redirectUrl)
	{
		WINPR_JSON* obj = WINPR_JSON_CreateObject();
		bool ok = WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0") &&
		          WINPR_JSON_AddNumberToObject(obj, "id", static_cast<double>(id));
		WINPR_JSON* result = ok ? WINPR_JSON_AddObjectToObject(obj, "result") : nullptr;
		if (result)
		{
			bool unused = WINPR_JSON_AddStringToObject(result, "status", "ok") &&
			              WINPR_JSON_AddStringToObject(result, "redirect_url", redirectUrl.c_str());
			(void)unused;
		}
		sendJson(obj);
	}

	void sendNullResult(int64_t id)
	{
		WINPR_JSON* obj = WINPR_JSON_CreateObject();
		bool ok = WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0") &&
		          WINPR_JSON_AddNumberToObject(obj, "id", static_cast<double>(id)) &&
		          WINPR_JSON_AddNullToObject(obj, "result");
		(void)ok;
		sendJson(obj);
	}

	void sendError(int64_t id, int code, const std::string& message)
	{
		WINPR_JSON* obj = WINPR_JSON_CreateObject();
		bool ok = WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0") &&
		          WINPR_JSON_AddNumberToObject(obj, "id", static_cast<double>(id));
		WINPR_JSON* error = ok ? WINPR_JSON_AddObjectToObject(obj, "error") : nullptr;
		if (error)
		{
			bool unused = WINPR_JSON_AddIntegerToObject(error, "code", code) &&
			              WINPR_JSON_AddStringToObject(error, "message", message.c_str());
			(void)unused;
		}
		sendJson(obj);
	}

	std::string getStringField(WINPR_JSON* obj, const char* name)
	{
		if (!obj)
			return {};
		WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, name);
		if (!item || !WINPR_JSON_IsString(item))
			return {};
		const char* v = WINPR_JSON_GetStringValue(item);
		return v ? v : "";
	}

	uint32_t getUintField(WINPR_JSON* obj, const char* name, uint32_t def)
	{
		if (!obj)
			return def;
		WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, name);
		if (!item || !WINPR_JSON_IsNumber(item))
			return def;
		return static_cast<uint32_t>(WINPR_JSON_GetNumberValue(item));
	}

	/** reads and dispatches JSON-RPC lines from the cmdIn channel until it closes or an "exit"
	 *  notification is received, then tells the session to shut down the browser window so
	 *  app.exec() on the main thread returns. */
	void readerLoop(Session& session)
	{
		std::string buf;
		std::string line;
		while (readLine(buf, line))
		{
			if (line.empty())
				continue;

			WINPR_JSON* msg = WINPR_JSON_Parse(line.c_str());
			if (!msg)
				continue;

			WINPR_JSON* idItem = WINPR_JSON_GetObjectItemCaseSensitive(msg, "id");
			const bool hasId = idItem && WINPR_JSON_IsNumber(idItem);
			const int64_t id = hasId ? static_cast<int64_t>(WINPR_JSON_GetNumberValue(idItem)) : 0;
			const std::string method = getStringField(msg, "method");
			WINPR_JSON* params = WINPR_JSON_GetObjectItemCaseSensitive(msg, "params");

			if (method == "hello")
			{
				sendHelloResult(id);
			}
			else if (method == "navigate")
			{
				const std::string title = getStringField(params, "title");
				const std::string url = getStringField(params, "url");
				const std::string redirectUri = getStringField(params, "redirect_uri");
				const uint32_t timeoutMs = getUintField(params, "timeout_ms", kDefaultTimeoutMs);

				std::string redirectUrl;
				std::string error;
				if (session.navigate(title, url, redirectUri, timeoutMs, redirectUrl, error))
					sendNavigateResult(id, redirectUrl);
				else
					sendError(id, 1, error);
			}
			else if (method == "$/cancel")
			{
				session.cancel();
			}
			else if (method == "shutdown")
			{
				sendNullResult(id);
			}
			else if (method == "exit")
			{
				WINPR_JSON_Delete(msg);
				break;
			}

			WINPR_JSON_Delete(msg);
		}

		session.quit();
	}

} // namespace

int main(int argc, char* argv[])
{
	std::string cmdInArg;
	std::string cmdOutArg;
	for (int i = 1; i < argc; i++)
	{
		const std::string arg = argv[i];
		if (arg.rfind("--cmdInFd=", 0) == 0)
			cmdInArg = arg;
		else if (arg.rfind("--cmdOutFd=", 0) == 0)
			cmdOutArg = arg;
	}

	if (cmdInArg.empty() || cmdOutArg.empty())
	{
		std::cerr << "usage: " << (argc > 0 ? argv[0] : "freerdp-qt-aad-helper")
		          << " --cmdInFd=<handle> --cmdOutFd=<handle>" << std::endl;
		return 1;
	}

	g_cmdIn = winpr_importHandleFromString(cmdInArg.c_str(), "--cmdInFd={}");
	g_cmdOut = winpr_importHandleFromString(cmdOutArg.c_str(), "--cmdOutFd={}");
	if (!g_cmdIn || (g_cmdIn == INVALID_HANDLE_VALUE) || !g_cmdOut ||
	    (g_cmdOut == INVALID_HANDLE_VALUE))
	{
		std::cerr << "failed to import the cmdIn/cmdOut channel handles" << std::endl;
		return 1;
	}

	/* must be registered before QApplication is constructed. CorsEnabled adds the scheme to
	 * Chromium's CORS-enabled/"web-safe" scheme list, which its network stack also consults to
	 * decide whether an https: page is allowed to redirect into it - without this, Chromium
	 * rejects the AAD redirect with net::ERR_UNSAFE_REDIRECT before we ever see it. LocalScheme
	 * (file:-like privilege) is deliberately NOT set: that made things worse, since Chromium
	 * specifically disallows network content redirecting into a local-privilege scheme. */
	QWebEngineUrlScheme brokerScheme(kBrokerScheme);
	brokerScheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
	brokerScheme.setFlags(QWebEngineUrlScheme::CorsEnabled);
	QWebEngineUrlScheme::registerScheme(brokerScheme);

	QApplication app(argc, argv);
	/* QApplication defaults to quitting as soon as its last top-level window (here, the popup)
	 * is closed - which would call qApp->quit() straight from closeEvent(), bypassing
	 * Session::quit() (and the `delete window` it does before quitting) entirely: the app would
	 * exit with the AuthWindow/QWebEnginePage still alive, triggering QtWebEngine's "Release of
	 * profile requested but WebEnginePage still not deleted" warning at teardown. The window is
	 * deliberately kept alive (just hidden) across navigate() calls anyway - see Session's class
	 * comment - so only our own explicit quit(), driven by the "exit" JSON-RPC notification,
	 * should end the process. */
	app.setQuitOnLastWindowClosed(false);

	Session session;
	BrokerSchemeHandler brokerHandler(session);
	QWebEngineProfile::defaultProfile()->installUrlSchemeHandler(QByteArray(kBrokerScheme),
	                                                             &brokerHandler);

	std::thread reader(readerLoop, std::ref(session));

	const int rc = QApplication::exec();
	reader.join();

	(void)CloseHandle(g_cmdIn);
	(void)CloseHandle(g_cmdOut);
	return rc;
}

#include "main.moc"

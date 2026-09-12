/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Out-of-process AAD auth helper - JSON-RPC front end over dedicated pipes
 *
 * Drives a native webview to complete a client-supplied browser-based auth flow (AAD/OAuth2
 * today, but this binary has no OAuth-specific knowledge at all: it is handed a URL to show and
 * a redirect_uri prefix to watch for, and it hands back whatever URI the browser eventually
 * navigated to). Keeping it this "dumb" is deliberate - see client/common/aad_helper.c
 * for the FreeRDP-side counterpart and the protocol this speaks.
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

#include <webview.h>

#if defined(__linux__)
#include <gtk/gtk.h>
#endif

#include <atomic>
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

namespace
{

	constexpr uint32_t kDefaultTimeoutMs = 180000;

	struct PendingResult
	{
		std::mutex mtx;
		std::condition_variable cv;
		bool done = false;
		bool ok = false;
		std::string redirectUrl;
		std::string errorMessage;
	};

	/** Owns the single native webview instance for the lifetime of the helper process. The webview
	 *  event loop must run on the process' main thread (required by Cocoa on macOS), and is kept
	 *  alive across multiple navigate() calls - rather than recreated per call like the old in-SDL
	 *  implementation - so that cookies/session persist between e.g. an AVD gateway auth and a
	 *  following host auth, avoiding a second login prompt. All calls other than run() itself may
	 *  come from the cmdIn-reader thread and are marshalled onto the UI thread via webview's
	 *  dispatch(). */
	class Session
	{
	  public:
		Session() : w(false, nullptr)
		{
			w.add_navigation_listener(&Session::onNavigateStatic, this);
		}

		void run()
		{
			w.run();
		}

		bool navigate(const std::string& title, const std::string& url,
		              const std::string& redirectUri, uint32_t timeoutMs, std::string& redirectUrl,
		              std::string& error)
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

			w.dispatch(
			    [this, title, url]()
			    {
#if defined(__linux__)
				    /* the window may have been hidden (see below) by a previous navigate() on
				     * this same reused webview - bring it back before showing new content. */
				    auto handle = w.window();
				    if (handle.ok())
					    gtk_widget_show(GTK_WIDGET(handle.value()));
#endif
				    w.set_title(title);
				    w.set_size(800, 600, WEBVIEW_HINT_NONE);
				    w.navigate(url);
			    });

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

			/* the popup has served its purpose once this navigate() is done, whatever the
			 * outcome - hide it now rather than leaving it sitting on screen until the next
			 * navigate() (if any) reuses and re-shows it. webview has no portable hide(), so on
			 * platforms without a native-handle-based fallback below, blanking the content is the
			 * best available approximation. */
			w.dispatch(
			    [this]()
			    {
#if defined(__linux__)
				    auto handle = w.window();
				    if (handle.ok())
					    gtk_widget_hide(GTK_WIDGET(handle.value()));
				    else
					    w.navigate("about:blank");
#else
				    w.navigate("about:blank");
#endif
			    });

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
			w.dispatch([this]() { w.terminate(); });
		}

	  private:
		static void onNavigateStatic(webview_t wv, const char* uri, webview_navigation_event_t type,
		                             void* arg)
		{
			(void)wv;
			if (type != WEBVIEW_LOAD_FINISHED)
				return;
			static_cast<Session*>(arg)->onNavigate(uri);
		}

		void onNavigate(const std::string& uri)
		{
			std::lock_guard<std::mutex> lock(stateMtx);
			if (!current || !watcher.matches(uri))
				return;

			std::string err;
			if (watcher.hasError(uri, err))
				finishCurrentLocked(false, "", err);
			else
				finishCurrentLocked(true, uri, "");
		}

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

		webview::webview w;
		std::mutex stateMtx;
		PendingResult* current = nullptr;
		RedirectWatcher watcher{ std::string() };
	};

	/* the JSON-RPC channel: handles imported from the --cmdInFd=/--cmdOutFd= command line arguments
	 * (see main()), not stdin/stdout - so this helper's own stdio stays free for its normal
	 * diagnostic output instead of colliding with the protocol. */
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
			    WINPR_JSON_AddStringToObject(result, "helper", "freerdp-webview-aad-helper/1.0");
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
	 *  notification is received, then tells the session to shut down the webview so run() on the
	 *  main thread returns. */
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
			else if (method == "cancel")
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
		std::cerr << "usage: " << (argc > 0 ? argv[0] : "freerdp-webview-aad-helper")
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

	Session session;
	std::thread reader(readerLoop, std::ref(session));

	session.run();
	reader.join();

	(void)CloseHandle(g_cmdIn);
	(void)CloseHandle(g_cmdOut);
	return 0;
}

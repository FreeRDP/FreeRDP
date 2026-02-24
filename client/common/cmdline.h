/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Command-Line Interface
 *
 * Copyright 2018 Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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
#ifndef CLIENT_COMMON_CMDLINE_H
#define CLIENT_COMMON_CMDLINE_H

#include <freerdp/config.h>

#include <winpr/cmdline.h>

static const COMMAND_LINE_ARGUMENT_A global_cmd_args[] = {
	{ "a", COMMAND_LINE_VALUE_REQUIRED, "<addin>[,<options>]", nullptr, nullptr, -1, "addin",
	  "Addin" },
	{ "azure", COMMAND_LINE_VALUE_REQUIRED,
	  "[tenantid:<id>],[use-tenantid[:[on|off]],[ad:<url>]"
	  "[avd-access:<format string>],[avd-token:<format string>],[avd-scope:<format string>]",
	  nullptr, nullptr, -1, nullptr, "AzureAD options" },
	{ "action-script", COMMAND_LINE_VALUE_REQUIRED, "<file-name>", "~/.config/freerdp/action.sh",
	  nullptr, -1, nullptr, "Action script" },
	{ "admin", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, "console",
	  "Admin (or console) session" },
	{ "aero", COMMAND_LINE_VALUE_BOOL, nullptr, nullptr, BoolValueFalse, -1, nullptr,
	  "desktop composition" },
	{ "app", COMMAND_LINE_VALUE_REQUIRED,
	  "program:[<path>|<||alias>],cmd:<command>,file:<filename>,guid:<guid>,icon:<filename>,name:<"
	  "name>,workdir:<directory>,hidef:[on|off]",
	  nullptr, nullptr, -1, nullptr, "Remote application program" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "app-cmd", COMMAND_LINE_VALUE_REQUIRED, "<parameters>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /app:cmd:<command>] Remote application command-line parameters" },
	{ "app-file", COMMAND_LINE_VALUE_REQUIRED, "<file-name>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /app:file:<filename>] File to open with remote application" },
	{ "app-guid", COMMAND_LINE_VALUE_REQUIRED, "<app-guid>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /app:guid:<guid>] Remote application GUID" },
	{ "app-icon", COMMAND_LINE_VALUE_REQUIRED, "<icon-path>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /app:icon:<filename>] Remote application icon for user interface" },
	{ "app-name", COMMAND_LINE_VALUE_REQUIRED, "<app-name>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /app:name:<name>] Remote application name for user interface" },
	{ "app-workdir", COMMAND_LINE_VALUE_REQUIRED, "<workspace path>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /app:workdir:<directory>] Remote application workspace path" },
#endif
	{ "assistance", COMMAND_LINE_VALUE_REQUIRED, "<password>", nullptr, nullptr, -1, nullptr,
	  "Remote assistance password" },
	{ "auto-request-control", COMMAND_LINE_VALUE_FLAG, "", nullptr, nullptr, -1, nullptr,
	  "Automatically request remote assistance input control" },
	{ "async-channels", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Asynchronous channels (experimental)" },
	{ "async-update", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Asynchronous update" },
	{ "audio-mode", COMMAND_LINE_VALUE_REQUIRED, "<mode>", nullptr, nullptr, -1, nullptr,
	  "Audio output mode" },
	{ "auth-only", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Authenticate only" },
	{ "auth-pkg-list", COMMAND_LINE_VALUE_REQUIRED, "[[none],]<!ntlm,kerberos,!u2u>", nullptr,
	  nullptr, -1, nullptr,
	  "Authentication package filter (comma-separated list, use '!' to disable). By default "
	  "all methods are enabled. Use explicit 'none' as first argument to disable all methods, "
	  "selectively enabling only the ones following." },
	{ "authentication", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "Authentication (experimental)" },
	{ "auto-reconnect", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Automatic reconnection" },
	{ "auto-reconnect-max-retries", COMMAND_LINE_VALUE_REQUIRED, "<retries>", nullptr, nullptr, -1,
	  nullptr, "Automatic reconnection maximum retries, 0 for unlimited [0,1000]" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "bitmap-cache", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "[DEPRECATED, use /cache:bitmap[:on|off]] bitmap cache" },
	{ "persist-cache", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "[DEPRECATED, use /cache:persist[:on|off]] persistent bitmap cache" },
	{ "persist-cache-file", COMMAND_LINE_VALUE_REQUIRED, "<filename>", nullptr, nullptr, -1,
	  nullptr, "[DEPRECATED, use /cache:persist-file:<filename>] persistent bitmap cache file" },
#endif
	{ "bpp", COMMAND_LINE_VALUE_REQUIRED, "<depth>", "16", nullptr, -1, nullptr,
	  "Session bpp (color depth)" },
	{ "buildconfig", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_BUILDCONFIG, nullptr, nullptr,
	  nullptr, -1, nullptr, "Print the build configuration" },
	{ "cache", COMMAND_LINE_VALUE_REQUIRED,
	  "[bitmap[:on|off],codec[:rfx|nsc],glyph[:on|off],offscreen[:on|off],persist,persist-file:<"
	  "filename>]",
	  nullptr, nullptr, -1, nullptr, "" },
	{ "cert", COMMAND_LINE_VALUE_REQUIRED,
	  "[deny,ignore,name:<name>,tofu,fingerprint:<hash>:<hash as hex>[,fingerprint:<hash>:<another "
	  "hash>]]",
	  nullptr, nullptr, -1, nullptr,
	  "Certificate accept options. Use with care!\n"
	  " * deny         ... Automatically abort connection if the certificate does not match, no "
	  "user interaction.\n"
	  " * ignore       ... Ignore the certificate checks altogether (overrules all other options)\n"
	  " * name         ... Use the alternate <name> instead of the certificate subject to match "
	  "locally stored certificates\n"
	  " * tofu         ... Accept certificate unconditionally on first connect and deny on "
	  "subsequent connections if the certificate does not match\n"
	  " * fingerprints ... A list of certificate hashes that are accepted unconditionally for a "
	  "connection" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "cert-deny", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /cert:deny] Automatically abort connection for any certificate that can "
	  "not be validated." },
	{ "cert-ignore", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /cert:ignore] Ignore certificate" },
	{ "cert-name", COMMAND_LINE_VALUE_REQUIRED, "<name>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /cert:name:<name>] Certificate name" },
	{ "cert-tofu", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /cert:tofu] Automatically accept certificate on first connect" },
#endif
#ifdef _WIN32
	{ "connect-child-session", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, "",
	  "connect to child session (win32)" },
#endif
	{ "client-build-number", COMMAND_LINE_VALUE_REQUIRED, "<number>", nullptr, nullptr, -1, nullptr,
	  "Client Build Number sent to server (influences smartcard behaviour, see [MS-RDPESC])" },
	{ "client-hostname", COMMAND_LINE_VALUE_REQUIRED, "<name>", nullptr, nullptr, -1, nullptr,
	  "Client Hostname to send to server" },
	{ "clipboard", COMMAND_LINE_VALUE_BOOL | COMMAND_LINE_VALUE_OPTIONAL,
	  "[[use-selection:<atom>],[direction-to:[all|local|remote|off]],[files-to[:all|local|remote|"
	  "off]]]",
	  BoolValueTrue, nullptr, -1, nullptr,
	  "Redirect clipboard:\n"
	  " * use-selection:<atom>  ... (X11) Specify which X selection to access. Default is "
	  "CLIPBOARD. PRIMARY is the X-style middle-click selection.\n"
	  " * direction-to:[all|local|remote|off] control enabled clipboard direction\n"
	  " * files-to:[all|local|remote|off] control enabled file clipboard direction" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "codec-cache", COMMAND_LINE_VALUE_REQUIRED, "[rfx|nsc|jpeg]", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /cache:codec:[rfx|nsc|jpeg]] Bitmap codec cache" },
#endif
	{ "compression", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, "z",
	  "compression" },
	{ "compression-level", COMMAND_LINE_VALUE_REQUIRED, "<level>", nullptr, nullptr, -1, nullptr,
	  "Compression level (0,1,2)" },
	{ "credentials-delegation", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1,
	  nullptr, "credentials delegation" },
	{ "d", COMMAND_LINE_VALUE_REQUIRED, "<domain>", nullptr, nullptr, -1, nullptr, "Domain" },
	{ "decorations", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "Window decorations" },
	{ "disp", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr, "Display control" },
	{ "drive", COMMAND_LINE_VALUE_REQUIRED, "<name>,<path>", nullptr, nullptr, -1, nullptr,
	  "Redirect directory <path> as named share <name>. Hotplug support is enabled with "
	  "/drive:hotplug,*. This argument provides the same function as \"Drives that I plug in "
	  "later\" option in MSTSC." },
	{ "drives", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Redirect all mount points as shares" },
	{ "dump", COMMAND_LINE_VALUE_REQUIRED, "<record|replay>,file:<file>[,nodelay]", nullptr,
	  nullptr, -1, nullptr, "record or replay dump" },
	{ "dvc", COMMAND_LINE_VALUE_REQUIRED, "<channel>[,<options>]", nullptr, nullptr, -1, nullptr,
	  "Dynamic virtual channel" },
	{ "dynamic-resolution", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Send resolution updates when the window is resized" },
	{ "echo", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, "echo", "Echo channel" },
	{ "encryption", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "Encryption (experimental)" },
	{ "encryption-methods", COMMAND_LINE_VALUE_REQUIRED, "[40,][56,][128,][FIPS]", nullptr, nullptr,
	  -1, nullptr, "RDP standard security encryption methods" },
	{ "f", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "Fullscreen mode (<Ctrl>+<Alt>+<Enter> toggles fullscreen)" },
	{ "fipsmode", COMMAND_LINE_VALUE_BOOL, nullptr, nullptr, nullptr, -1, nullptr, "FIPS mode" },
	{ "floatbar", COMMAND_LINE_VALUE_OPTIONAL,
	  "sticky:[on|off],default:[visible|hidden],show:[always|fullscreen|window]", nullptr, nullptr,
	  -1, nullptr,
	  "floatbar is disabled by default (when enabled defaults to sticky in fullscreen mode)" },
	{ "fonts", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "smooth fonts (ClearType)" },
	{ "force-console-callbacks", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1,
	  nullptr, "Use default callbacks (console) for certificate/credential/..." },
	{ "frame-ack", COMMAND_LINE_VALUE_REQUIRED, "<number>", nullptr, nullptr, -1, nullptr,
	  "Number of frame acknowledgement" },
	{ "args-from", COMMAND_LINE_VALUE_REQUIRED, "<file>|stdin|fd:<number>|env:<name>", nullptr,
	  nullptr, -1, nullptr,
	  "Read command line from a file, stdin or file descriptor. This argument can not be combined "
	  "with any other. "
	  "Provide one argument per line." },
	{ "from-stdin", COMMAND_LINE_VALUE_OPTIONAL, "force", nullptr, nullptr, -1, nullptr,
	  "Read credentials from stdin. With <force> the prompt is done before connection, otherwise "
	  "on server request." },
	{ "gateway", COMMAND_LINE_VALUE_REQUIRED,
	  "g:<gateway>[:<port>],u:<user>,d:<domain>,p:<password>,usage-method:["
	  "direct|detect],access-token:<"
	  "token>,type:[rpc|http[,no-websockets][,extauth-sspi-ntlm]|auto[,no-websockets][,extauth-"
	  "sspi-ntlm]]|arm,url:<wss://url>,bearer:<oauth2-bearer-token>",
	  nullptr, nullptr, -1, "gw", "Gateway Hostname" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "g", COMMAND_LINE_VALUE_REQUIRED, "<gateway>[:<port>]", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /gateway:g:<url>] Gateway Hostname" },
	{ "gateway-usage-method", COMMAND_LINE_VALUE_REQUIRED, "[direct|detect]", nullptr, nullptr, -1,
	  "gum", "[DEPRECATED, use /gateway:usage-method:<method>] Gateway usage method" },
	{ "gd", COMMAND_LINE_VALUE_REQUIRED, "<domain>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /gateway:d:<domain>] Gateway domain" },
#endif
	{ "gdi", COMMAND_LINE_VALUE_REQUIRED, "sw|hw", nullptr, nullptr, -1, nullptr, "GDI rendering" },
	{ "geometry", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "Geometry tracking channel" },
	{ "gestures", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Consume multitouch input locally" },
#ifdef WITH_GFX_H264
	{ "gfx", COMMAND_LINE_VALUE_OPTIONAL,
	  "[[progressive[:on|off]|RFX[:on|off]|AVC420[:on|off]AVC444[:on|off]],mask:<value>,small-"
	  "cache[:on|off],thin-client[:on|off],progressive[:on|"
	  "off],frame-ack[:on|off]]",
	  nullptr, nullptr, -1, nullptr, "RDP8 graphics pipeline" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "gfx-h264", COMMAND_LINE_VALUE_OPTIONAL, "[[AVC420|AVC444],mask:<value>]", nullptr, nullptr,
	  -1, nullptr, "[DEPRECATED, use /gfx:avc420] RDP8.1 graphics pipeline using H264 codec" },
#endif
#else
	{ "gfx", COMMAND_LINE_VALUE_OPTIONAL,
	  "[progressive[:on|off]|RFX[:on|off]|AVC420[:on|off]AVC444[:on|off]],mask:<value>,small-cache["
	  ":on|off],thin-client[:on|off],progressive[:on|off]]",
	  nullptr, nullptr, -1, nullptr, "RDP8 graphics pipeline" },
#endif
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "gfx-progressive", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "[DEPRECATED, use /gfx:progressive] RDP8 graphics pipeline using progressive codec" },
	{ "gfx-small-cache", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "[DEPRECATED, use /gfx:small-cache] RDP8 graphics pipeline using small cache mode" },
	{ "gfx-thin-client", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "[DEPRECATED, use /gfx:thin-client] RDP8 graphics pipeline using thin client mode" },
	{ "glyph-cache", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "[DEPRECATED, use /cache:glyph[:on|off]] Glyph cache (experimental)" },
#endif
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "gp", COMMAND_LINE_VALUE_REQUIRED, "<password>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /gateway:p:<password>] Gateway password" },
#endif
	{ "grab-keyboard", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "Grab keyboard focus, forward all keys to remote" },
	{ "grab-mouse", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "Grab mouse focus, forward all events to remote" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "gt", COMMAND_LINE_VALUE_REQUIRED,
	  "[rpc|http[,no-websockets][,extauth-sspi-ntlm]|auto[,no-websockets][,extauth-sspi-ntlm]]",
	  nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /gateway:type:<type>] Gateway transport type" },
	{ "gu", COMMAND_LINE_VALUE_REQUIRED, "[[<domain>\\]<user>|<user>[@<domain>]]", nullptr, nullptr,
	  -1, nullptr, "[DEPRECATED, use /gateway:u:<user>] Gateway username" },
	{ "gat", COMMAND_LINE_VALUE_REQUIRED, "<access token>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /gateway:access-token:<token>] Gateway Access Token" },
#endif
	{ "h", COMMAND_LINE_VALUE_REQUIRED, "<height>", "768", nullptr, -1, nullptr, "Height" },
	{ "heartbeat", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "Support heartbeat PDUs" },
	{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, nullptr, nullptr, nullptr, -1, "?",
	  "Print help" },
	{ "home-drive", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Redirect user home as share" },
	{ "ipv4", COMMAND_LINE_VALUE_OPTIONAL, "[:force]", nullptr, nullptr, -1, "4",
	  "Prefer IPv4 A record over IPv6 AAAA record" },
	{ "ipv6", COMMAND_LINE_VALUE_OPTIONAL, "[:force]", nullptr, nullptr, -1, "6",
	  "Prefer IPv6 AAAA record over IPv4 A record" },
#if defined(WITH_JPEG)
	{ "jpeg", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "JPEG codec support" },
	{ "jpeg-quality", COMMAND_LINE_VALUE_REQUIRED, "<percentage>", nullptr, nullptr, -1, nullptr,
	  "JPEG quality" },
#endif
	{ "kbd", COMMAND_LINE_VALUE_REQUIRED,
	  "[layout:[0x<id>|<name>],lang:<0x<id>>,fn-key:<value>,type:<value>,subtype:<value>,unicode[:"
	  "on|off],remap:<key1>=<value1>,remap:<key2>=<value2>,pipe:<filename>]",
	  nullptr, nullptr, -1, nullptr,
	  "Keyboard related options:\n"
	  " * layout: set the keybouard layout announced to the server\n"
	  " * lang: set the keyboard language identifier sent to the server\n"
	  " * fn-key: Function key value\n"
	  " * remap: RDP scancode to another one. Use /list:kbd-scancode to get the mapping. Example: "
	  "To switch "
	  "'a' and 's' on a US keyboard: /kbd:remap:0x1e=0x1f,remap:0x1f=0x1e\n"
	  " * pipe: Name of a named pipe that can be used to type text into the RDP session\n" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "kbd-lang", COMMAND_LINE_VALUE_REQUIRED, "0x<id>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use / kbd:lang:<value>] Keyboard active language identifier" },
	{ "kbd-fn-key", COMMAND_LINE_VALUE_REQUIRED, "<value>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /kbd:fn-key:<value>] Function key value" },
	{ "kbd-list", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT, nullptr, nullptr, nullptr, -1,
	  nullptr, "[DEPRECATED, use /list:kbd] List keyboard layouts" },
	{ "kbd-scancode-list", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT, nullptr, nullptr, nullptr,
	  -1, nullptr, "[DEPRECATED, use list:kbd-scancode] List keyboard RDP scancodes" },
	{ "kbd-lang-list", COMMAND_LINE_VALUE_OPTIONAL | COMMAND_LINE_PRINT, nullptr, nullptr, nullptr,
	  -1, nullptr, "[DEPRECATED, use /list:kbd-lang] List keyboard languages" },
	{ "kbd-remap", COMMAND_LINE_VALUE_REQUIRED,
	  "[DEPRECATED, use /kbd:remap] List of <key>=<value>,... pairs to remap scancodes", nullptr,
	  nullptr, -1, nullptr, "Keyboard scancode remapping" },
	{ "kbd-subtype", COMMAND_LINE_VALUE_REQUIRED, "<id>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /kbd:subtype]Keyboard subtype" },
	{ "kbd-type", COMMAND_LINE_VALUE_REQUIRED, "<id>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /kbd:type] Keyboard type" },
	{ "kbd-unicode", COMMAND_LINE_VALUE_FLAG, "", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /kbd:unicode[:on|off]] Send unicode symbols, e.g. use the local "
	  "keyboard map. ATTENTION: Does not work with every "
	  "RDP server!" },
#endif
	{ "kerberos", COMMAND_LINE_VALUE_REQUIRED,
	  "[kdc-url:<url>,lifetime:<time>,start-time:<time>,renewable-lifetime:<time>,cache:<path>,"
	  "armor:<path>,pkinit-anchors:<path>,pkcs11-module:<name>]",
	  nullptr, nullptr, -1, nullptr, "Kerberos options" },
	{ "load-balance-info", COMMAND_LINE_VALUE_REQUIRED, "<info-string>", nullptr, nullptr, -1,
	  nullptr, "Load balance info" },
	{ "list", COMMAND_LINE_VALUE_REQUIRED | COMMAND_LINE_PRINT,
	  "[kbd|kbd-scancode|kbd-lang[:<value>]|smartcard[:[pkinit-anchors:<path>][,pkcs11-module:<"
	  "name>]]|"
	  "monitor|tune|timezones]",
	  "List available options for subcommand", nullptr, -1, nullptr,
	  "List available options for subcommand" },
	{ "log-filters", COMMAND_LINE_VALUE_REQUIRED, "<tag>:<level>[,<tag>:<level>[,...]]", nullptr,
	  nullptr, -1, nullptr, "Set logger filters, see wLog(7) for details" },
	{ "log-level", COMMAND_LINE_VALUE_REQUIRED, "[OFF|FATAL|ERROR|WARN|INFO|DEBUG|TRACE]", nullptr,
	  nullptr, -1, nullptr, "Set the default log level, see wLog(7) for details" },
	{ "max-fast-path-size", COMMAND_LINE_VALUE_REQUIRED, "<size>", nullptr, nullptr, -1, nullptr,
	  "Specify maximum fast-path update size" },
	{ "max-loop-time", COMMAND_LINE_VALUE_REQUIRED, "<time>", nullptr, nullptr, -1, nullptr,
	  "Specify maximum time in milliseconds spend treating packets" },
	{ "menu-anims", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "menu animations" },
	{ "microphone", COMMAND_LINE_VALUE_OPTIONAL,
	  "[sys:<sys>,][dev:<dev>,][format:<format>,][rate:<rate>,][channel:<channel>]", nullptr,
	  nullptr, -1, "mic", "Audio input (microphone)" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "smartcard-list", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT, nullptr, nullptr, nullptr, -1,
	  nullptr, "[DEPRECATED, use /list:smartcard] List smartcard information" },
	{ "monitor-list", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT, nullptr, nullptr, nullptr, -1,
	  nullptr, "[DEPRECATED, use /list:monitor] List detected monitors" },
#endif
	{ "monitors", COMMAND_LINE_VALUE_REQUIRED, "<id>[,<id>[,...]]", nullptr, nullptr, -1, nullptr,
	  "Select monitors to use (only effective in fullscreen or multimonitor mode)" },
	{ "mouse-motion", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "Send mouse motion events" },
	{ "mouse-relative", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Send mouse motion with relative addressing" },
	{ "mouse", COMMAND_LINE_VALUE_REQUIRED, "[relative:[on|off],grab:[on|off]]", nullptr, nullptr,
	  -1, nullptr,
	  "Mouse related options:\n"
	  " * relative:   send relative mouse movements if supported by server\n"
	  " * grab:       grab the mouse if within the window" },
#if defined(CHANNEL_TSMF_CLIENT)
	{ "multimedia", COMMAND_LINE_VALUE_OPTIONAL, "[sys:<sys>,][dev:<dev>,][decoder:<decoder>]",
	  nullptr, nullptr, -1, "mmr", "[DEPRECATED], use /video] Redirect multimedia (video)" },
#endif
	{ "multimon", COMMAND_LINE_VALUE_OPTIONAL, "force", nullptr, nullptr, -1, nullptr,
	  "Use multiple monitors" },
	{ "multitouch", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Redirect multitouch input" },
	{ "multitransport", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "Support multitransport protocol" },
	{ "nego", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "protocol security negotiation" },
	{ "network", COMMAND_LINE_VALUE_REQUIRED,
	  "[invalid|modem|broadband|broadband-low|broadband-high|wan|lan|auto]", nullptr, nullptr, -1,
	  nullptr, "Network connection type" },
	{ "nsc", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, "nscodec", "NSCodec support" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "offscreen-cache", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "[DEPRECATED, use /cache:offscreen[:on|off]] offscreen bitmap cache" },
#endif
	{ "orientation", COMMAND_LINE_VALUE_REQUIRED, "[0|90|180|270]", nullptr, nullptr, -1, nullptr,
	  "Orientation of display in degrees" },
	{ "old-license", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Use the old license workflow (no CAL and hwId set to 0)" },
	{ "p", COMMAND_LINE_VALUE_REQUIRED, "<password>", nullptr, nullptr, -1, nullptr, "Password" },
#if defined(CHANNEL_PARALLEL_CLIENT)
	{ "parallel", COMMAND_LINE_VALUE_OPTIONAL, "<name>[,<path>]", nullptr, nullptr, -1, nullptr,
	  "Redirect parallel device" },
#endif
	{ "parent-window", COMMAND_LINE_VALUE_REQUIRED, "<window-id>", nullptr, nullptr, -1, nullptr,
	  "Parent window id" },
	{ "pcb", COMMAND_LINE_VALUE_REQUIRED, "<blob>", nullptr, nullptr, -1, nullptr,
	  "Preconnection Blob" },
	{ "pcid", COMMAND_LINE_VALUE_REQUIRED, "<id>", nullptr, nullptr, -1, nullptr,
	  "Preconnection Id" },
	{ "pheight", COMMAND_LINE_VALUE_REQUIRED, "<height>", nullptr, nullptr, -1, nullptr,
	  "Physical height of display (in millimeters)" },
	{ "play-rfx", COMMAND_LINE_VALUE_REQUIRED, "<pcap-file>", nullptr, nullptr, -1, nullptr,
	  "Replay rfx pcap file" },
	{ "port", COMMAND_LINE_VALUE_REQUIRED, "<number>", nullptr, nullptr, -1, nullptr,
	  "Server port" },
	{ "suppress-output", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "suppress output when minimized" },
	{ "print-reconnect-cookie", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1,
	  nullptr, "Print base64 reconnect cookie after connecting" },
	{ "printer", COMMAND_LINE_VALUE_OPTIONAL, "<name>[,<driver>[,default]]", nullptr, nullptr, -1,
	  nullptr, "Redirect printer device" },
	{ "proxy", COMMAND_LINE_VALUE_REQUIRED, "[<proto>://][<user>:<password>@]<host>[:<port>]",
	  nullptr, nullptr, -1, nullptr,
	  "Proxy settings: override env. var (see also environment variable below). Protocol "
	  "\"socks5\" should be given explicitly where \"http\" is default." },
	{ "pth", COMMAND_LINE_VALUE_REQUIRED, "<password-hash>", nullptr, nullptr, -1, "pass-the-hash",
	  "Pass the hash (restricted admin mode)" },
	{ "pwidth", COMMAND_LINE_VALUE_REQUIRED, "<width>", nullptr, nullptr, -1, nullptr,
	  "Physical width of display (in millimeters)" },
	{ "rdp2tcp", COMMAND_LINE_VALUE_REQUIRED, "<executable path[:arg...]>", nullptr, nullptr, -1,
	  nullptr, "TCP redirection" },
	{ "reconnect-cookie", COMMAND_LINE_VALUE_REQUIRED, "<base64-cookie>", nullptr, nullptr, -1,
	  nullptr, "Pass base64 reconnect cookie to the connection" },
	{ "redirect-prefer", COMMAND_LINE_VALUE_REQUIRED, "<FQDN|IP|NETBIOS>,[...]", nullptr, nullptr,
	  -1, nullptr, "Override the preferred redirection order" },
	{ "relax-order-checks", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1,
	  "relax-order-checks",
	  "Do not check if a RDP order was announced during capability exchange, only use when "
	  "connecting to a buggy server" },
	{ "restricted-admin", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, "restrictedAdmin",
	  "Restricted admin mode" },
#ifdef CHANNEL_RDPEAR_CLIENT
	{ "remoteGuard", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, "remoteGuard",
	  "Remote guard credentials" },
#endif
	{ "rfx", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr, "RemoteFX" },
	{ "rfx-mode", COMMAND_LINE_VALUE_REQUIRED, "[image|video]", nullptr, nullptr, -1, nullptr,
	  "RemoteFX mode" },
	{ "scale", COMMAND_LINE_VALUE_REQUIRED, "[100|140|180]", "100", nullptr, -1, nullptr,
	  "Scaling factor of the display" },
	{ "scale-desktop", COMMAND_LINE_VALUE_REQUIRED, "<percentage>", "100", nullptr, -1, nullptr,
	  "Scaling factor for desktop applications (value between 100 and 500)" },
	{ "scale-device", COMMAND_LINE_VALUE_REQUIRED, "100|140|180", "100", nullptr, -1, nullptr,
	  "Scaling factor for app store applications" },
	{ "sec", COMMAND_LINE_VALUE_REQUIRED,
	  "rdp[:[on|off]]|tls[:[on|off]]|nla[:[on|off]]|ext[:[on|off]]|aad[:[on|off]]", nullptr,
	  nullptr, -1, nullptr,
	  "Force specific protocol security. e.g. /sec:nla enables NLA and disables all others, while "
	  "/sec:nla:[on|off] just toggles NLA" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "sec-ext", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "[DEPRECATED, use /sec:ext] NLA extended protocol security" },
	{ "sec-nla", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "[DEPRECATED, use /sec:nla] NLA protocol security" },
	{ "sec-rdp", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "[DEPRECATED, use /sec:rdp] RDP protocol security" },
	{ "sec-tls", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "[DEPRECATED, use /sec:tls] TLS protocol security" },
#endif
#if defined(CHANNEL_SERIAL_CLIENT)
	{ "serial", COMMAND_LINE_VALUE_OPTIONAL, "<name>[,<path>[,<driver>[,permissive]]]", nullptr,
	  nullptr, -1, "tty", "Redirect serial device" },
#endif
	{ "server-name", COMMAND_LINE_VALUE_REQUIRED, "<name>", nullptr, nullptr, -1, nullptr,
	  "User-specified server name to use for validation (TLS, Kerberos)" },
	{ "shell", COMMAND_LINE_VALUE_REQUIRED, "<shell>", nullptr, nullptr, -1, nullptr,
	  "Alternate shell" },
	{ "shell-dir", COMMAND_LINE_VALUE_REQUIRED, "<dir>", nullptr, nullptr, -1, nullptr,
	  "Shell working directory" },
	{ "size", COMMAND_LINE_VALUE_REQUIRED, "<width>x<height> or <percent>%[wh]", "1024x768",
	  nullptr, -1, nullptr, "Screen size" },
	{ "smart-sizing", COMMAND_LINE_VALUE_OPTIONAL, "<width>x<height>", nullptr, nullptr, -1,
	  nullptr, "Scale remote desktop to window size" },
	{ "smartcard", COMMAND_LINE_VALUE_OPTIONAL, "<str>[,<str>...]", nullptr, nullptr, -1, nullptr,
	  "Redirect the smartcard devices containing any of the <str> in their names." },
	{ "smartcard-logon", COMMAND_LINE_VALUE_OPTIONAL,
	  "[cert:<path>,key:<key>,pin:<pin>,csp:<csp name>,reader:<reader>,card:<card>]", nullptr,
	  nullptr, -1, nullptr, "Activates Smartcard (optional certificate) Logon authentication." },
	{ "sound", COMMAND_LINE_VALUE_OPTIONAL,
	  "[sys:<sys>,][dev:<dev>,][format:<format>,][rate:<rate>,][channel:<channel>,][latency:<"
	  "latency>,][quality:<quality>]",
	  nullptr, nullptr, -1, "audio", "Audio output (sound)" },
	{ "span", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "Span screen over multiple monitors" },
	{ "spn-class", COMMAND_LINE_VALUE_REQUIRED, "<service-class>", nullptr, nullptr, -1, nullptr,
	  "SPN authentication service class" },
	{ "ssh-agent", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, "ssh-agent",
	  "SSH Agent forwarding channel" },
	{ "sspi-module", COMMAND_LINE_VALUE_REQUIRED, "<SSPI module path>", nullptr, nullptr, -1,
	  nullptr, "SSPI shared library module file path" },
	{ "winscard-module", COMMAND_LINE_VALUE_REQUIRED, "<WinSCard module path>", nullptr, nullptr,
	  -1, nullptr, "WinSCard shared library module file path" },
	{ "disable-output", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "Deactivate all graphics decoding in the client session. Useful for load tests with many "
	  "simultaneous connections" },
	{ "t", COMMAND_LINE_VALUE_REQUIRED, "<title>", nullptr, nullptr, -1, "title", "Window title" },
	{ "themes", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr, "themes" },
	{ "timeout", COMMAND_LINE_VALUE_REQUIRED, "<time in ms>", "9000", nullptr, -1, "timeout",
	  "Advanced setting for high latency links: Adjust connection timeout, use if you encounter "
	  "timeout failures with your connection" },
	{ "timezone", COMMAND_LINE_VALUE_REQUIRED, "<windows timezone>", nullptr, nullptr, -1, nullptr,
	  "Use supplied windows timezone for connection (requires server support), see /list:timezones "
	  "for allowed values" },
	{ "tls", COMMAND_LINE_VALUE_REQUIRED, "[ciphers|seclevel|secrets-file|enforce]", nullptr,
	  nullptr, -1, nullptr,
	  "TLS configuration options:"
	  " * ciphers:[netmon|ma|<cipher names>]\n"
	  " * seclevel:<level>, default: 1, range: [0-5] Override the default TLS security level, "
	  "might be required for older target servers\n"
	  " * secrets-file:<filename>\n"
	  " * enforce[:[ssl3|1.0|1.1|1.2|1.3]] Force use of SSL/TLS version for a connection. Some "
	  "servers have a buggy TLS "
	  "version negotiation and might fail without this. Defaults to TLS 1.2 if no argument is "
	  "supplied. Use 1.0 for windows 7" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "tls-ciphers", COMMAND_LINE_VALUE_REQUIRED, "[netmon|ma|ciphers]", nullptr, nullptr, -1,
	  nullptr, "[DEPRECATED, use /tls:ciphers] Allowed TLS ciphers" },
	{ "tls-seclevel", COMMAND_LINE_VALUE_REQUIRED, "<level>", "1", nullptr, -1, nullptr,
	  "[DEPRECATED, use /tls:seclevel] TLS security level - defaults to 1" },
	{ "tls-secrets-file", COMMAND_LINE_VALUE_REQUIRED, "<filename>", nullptr, nullptr, -1, nullptr,
	  "[DEPRECATED, use /tls:secrets:file] File were TLS secrets will be stored in the "
	  "SSLKEYLOGFILE format" },
	{ "enforce-tlsv1_2", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "[DEPRECATED, use /tls:enforce:1.2] Force use of TLS1.2 for connection. Some "
	  "servers have a buggy TLS version negotiation and "
	  "might fail without this" },
#endif
	{ "toggle-fullscreen", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "Alt+Ctrl+Enter to toggle fullscreen" },
	{ "tune", COMMAND_LINE_VALUE_REQUIRED, "<setting:value>,<setting:value>", "", nullptr, -1,
	  nullptr, "[experimental] directly manipulate freerdp settings, use with extreme caution!" },
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	{ "tune-list", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT, nullptr, nullptr, nullptr, -1,
	  nullptr, "[DEPRECATED, use /list:tune] Print options allowed for /tune" },
#endif
	{ "u", COMMAND_LINE_VALUE_REQUIRED, "[[<domain>\\]<user>|<user>[@<domain>]]", nullptr, nullptr,
	  -1, nullptr, "Username" },
	{ "unmap-buttons", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Let server see real physical pointer button" },
#ifdef CHANNEL_URBDRC_CLIENT
	{ "usb", COMMAND_LINE_VALUE_REQUIRED,
	  "[dbg,][id:<vid>:<pid>#...,][addr:<bus>:<addr>#...,][auto]", nullptr, nullptr, -1, nullptr,
	  "Redirect USB device" },
#endif
	{ "v", COMMAND_LINE_VALUE_REQUIRED, "<server>[:port]", nullptr, nullptr, -1, nullptr,
	  "Server hostname|URL|IPv4|IPv6 or vsock://<number> or /some/path/to/pipe or |:1234 to pass a "
	  "TCP socket to use" },
	{ "vc", COMMAND_LINE_VALUE_REQUIRED, "<channel>[,<options>]", nullptr, nullptr, -1, nullptr,
	  "Static virtual channel" },
	{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, nullptr, nullptr, nullptr,
	  -1, nullptr, "Print version" },
	{ "video", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "Video optimized remoting channel" },
	{ "prevent-session-lock", COMMAND_LINE_VALUE_OPTIONAL, "<time in sec>", nullptr, nullptr, -1,
	  nullptr,
	  "Prevent session locking by injecting fake mouse motion events to the server "
	  "when the connection is idle (default interval: 180 seconds)" },
	{ "vmconnect", COMMAND_LINE_VALUE_OPTIONAL, "<vmid>", nullptr, nullptr, -1, nullptr,
	  "Hyper-V console (use port 2179, disable negotiation)" },
	{ "w", COMMAND_LINE_VALUE_REQUIRED, "<width>", "1024", nullptr, -1, nullptr, "Width" },
	{ "wallpaper", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
	  "wallpaper" },
	{ "window-drag", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "full window drag" },
	{ "window-position", COMMAND_LINE_VALUE_REQUIRED, "<xpos>x<ypos>", nullptr, nullptr, -1,
	  nullptr, "window position" },
	{ "wm-class", COMMAND_LINE_VALUE_REQUIRED, "<class-name>", nullptr, nullptr, -1, nullptr,
	  "Set the WM_CLASS hint for the window instance" },
	{ "workarea", COMMAND_LINE_VALUE_FLAG, nullptr, nullptr, nullptr, -1, nullptr,
	  "Use available work area" },
	{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
};
#endif /* CLIENT_COMMON_CMDLINE_H */

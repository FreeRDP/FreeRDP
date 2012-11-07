/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Arguments Parsing
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2011 Vic Lee
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#else
#include <unistd.h>
#endif

#include <freerdp/settings.h>
#include <freerdp/constants.h>
#include <freerdp/utils/print.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/passphrase.h>

void freerdp_parse_hostname(rdpSettings* settings, char* hostname)
{
	char* p;

	if (hostname[0] == '[' && (p = strchr(hostname, ']'))
			&& (p[1] == 0 || (p[1] == ':' && !strchr(p + 2, ':'))))
	{
		/* Either "[...]" or "[...]:..." with at most one : after the brackets */
		settings->ServerHostname = _strdup(hostname + 1);

		if ((p = strchr((char*) settings->ServerHostname, ']')))
		{
			*p = 0;

			if (p[1] == ':')
				settings->ServerPort = atoi(p + 2);
		}
	}
	else
	{
		/* Port number is cut off and used if exactly one : in the string */
		settings->ServerHostname = _strdup(hostname);

		if ((p = strchr((char*)settings->ServerHostname, ':')) && !strchr(p + 1, ':'))
		{
			*p = 0;
			settings->ServerPort = atoi(p + 1);
		}
	}
}

/**
 * Parse command-line arguments and update rdpSettings members accordingly.
 * @param settings pointer to rdpSettings struct to be updated.
 * @param argc number of arguments available.
 * @param argv string array of the arguments.
 * @param plugin_callback function to be called when a plugin needs to be loaded.
 * @param plugin_user_data pointer to be passed to the plugin_callback function.
 * @param ui_callback function to be called when a UI-specific argument is being processed.
 * @param ui_user_data pointer to be passed to the ui_callback function.
 * @return number of arguments that were parsed, or FREERDP_ARGS_PARSE_RESULT on failure or --version/--help
 */
int freerdp_parse_args(rdpSettings* settings, int argc, char** argv,
	ProcessPluginArgs plugin_callback, void* plugin_user_data,
	ProcessUIArgs ui_callback, void* ui_user_data)
{
	int t;
	char* p;
	int i, j;
	int index = 1;
	int num_extensions = 0;
	RDP_PLUGIN_DATA* plugin_data;

	while (index < argc)
	{
		if (index == 1)
		{
			p = strstr(argv[index], ".rdp");

			if (!p)
				p = strstr(argv[index], ".RDP");

			if (p)
			{
				settings->ConnectionFile = _strdup(argv[index]);
				index++;
				continue;
			}
		}

		if ((strcmp("-h", argv[index]) == 0 ) || (strcmp("--help", argv[index]) == 0 ))
		{
			printf("\n"
				"FreeRDP - A Free Remote Desktop Protocol Client\n"
				"See http://www.freerdp.com for more information\n"
				"\n"
				"Usage: %s [file] [options] server:port\n"
				"  -0: connect to console session\n"
				"  -a: set color depth in bit, default is 16\n"
				"  -c: shell working directory\n"
				"  -D: hide window decorations\n"
				"  -T: window title\n"
				"  -d: domain\n"
				"  -f: fullscreen mode\n"
				"  -g: set geometry, using format WxH or X%% or 'workarea', default is 1024x768\n"
				"  -h: print this help\n"
				"  -k: set keyboard layout ID\n"
				"  -K: do not interfere with window manager bindings\n"
				"  -n: hostname\n"
				"  -o: console audio\n"
				"  -p: password\n"
				"  -s: set startup-shell\n"
				"  -t: alternative port number, default is 3389\n"
				"  -u: username\n"
				"  -x: performance flags (m[odem], b[roadband] or l[an])\n"
				"  -X: embed into another window with a given XID.\n"
				"  -z: enable compression\n"
				"  --app: RemoteApp connection. This implies -g workarea\n"
				"  --ext: load an extension\n"
				"  --no-auth: disable authentication\n"
				"  --authonly: authentication only, no UI\n"
				"  --from-stdin: unspecified username, password, domain and hostname params are prompted\n"
				"  --no-fastpath: disable fast-path\n"
				"  --no-motion: don't send mouse motion events\n"
				"  --gdi: graphics rendering (hw, sw)\n"
				"  --no-osb: disable offscreen bitmaps\n"
				"  --no-bmp-cache: disable bitmap cache\n"
				"  --bcv3: codec for bitmap cache v3 (rfx, nsc, jpeg)\n"
				"  --plugin: load a virtual channel plugin\n"
				"  --rfx: enable RemoteFX\n"
				"  --rfx-mode: RemoteFX operational flags (v[ideo], i[mage]), default is video\n"
				"  --frame-ack: number of frames pending to be acknowledged, default is 2 (disable with 0)\n"
				"  --nsc: enable NSCodec (experimental)\n"
#ifdef WITH_JPEG
				"  --jpeg: enable jpeg codec, uses 75 quality\n"
				"  --jpegex: enable jpeg and set quality(1..99)\n"
#endif
				"  --disable-wallpaper: disables wallpaper\n"
				"  --composition: enable desktop composition\n"
				"  --disable-full-window-drag: disables full window drag\n"
				"  --disable-menu-animations: disables menu animations\n"
				"  --disable-theming: disables theming\n"
				"  --no-nego: disable negotiation of security layer and enforce highest enabled security protocol\n"
				"  --no-rdp: disable Standard RDP encryption\n"
				"  --no-tls: disable TLS encryption\n"
				"  --no-nla: disable network level authentication\n"
				"  --ntlm: force NTLM authentication protocol version (1 or 2)\n"
				"  --ignore-certificate: ignore verification of logon certificate\n"
				"  --certificate-name: use this name for the logon certificate, instead of the server name\n"
				"  --sec: force protocol security (rdp, tls or nla)\n"
				"  --tsg: Terminal Server Gateway (<username> <password> <hostname>)\n"
				"  --kbd-list: list all keyboard layout ids used by -k\n"
				"  --no-salted-checksum: disable salted checksums with Standard RDP encryption\n"
				"  --pcid: preconnection id\n"
				"  --pcb: preconnection blob\n"
				"  --version: print version information\n"
				"\n", argv[0]);
			return FREERDP_ARGS_PARSE_HELP; /* TODO: What is the correct return? */
		}
		else if (strcmp("-a", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing color depth\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->ColorDepth = atoi(argv[index]);
		}
		else if (strcmp("-u", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing username\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->Username = _strdup(argv[index]);
		}
		else if (strcmp("-p", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing password\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->Password = _strdup(argv[index]);
			settings->AutoLogonEnabled = TRUE;

			/*
			 * Overwrite original password which could be revealed by a simple "ps aux" command.
			 * This approach won't hide the password length, but it is better than nothing.
			 */

			memset(argv[index], '*', strlen(argv[index]));
		}
		else if (strcmp("-d", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing domain\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->Domain = _strdup(argv[index]);
		}
		else if (strcmp("-s", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing shell\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->AlternateShell = _strdup(argv[index]);
		}
		else if (strcmp("-c", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing directory\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->ShellWorkingDirectory = _strdup(argv[index]);
		}
		else if (strcmp("-g", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing dimensions\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}

			if (strncmp("workarea", argv[index], 1) == 0)
			{
				settings->Workarea = TRUE;
			}
			else
			{
				settings->DesktopWidth = (UINT16) strtol(argv[index], &p, 10);

				if (*p == 'x')
				{
					settings->DesktopHeight = (UINT16) strtol(p + 1, &p, 10);
				}
				if (*p == '%')
				{
					settings->PercentScreen = settings->DesktopWidth;

					if (settings->PercentScreen <= 0 || settings->PercentScreen > 100)
					{
						printf("invalid geometry percentage\n");
						return FREERDP_ARGS_PARSE_FAILURE;
					}
				}
				else
				{
					if (ui_callback != NULL)
						ui_callback(settings, "-g", p, ui_user_data);
				}
			}
		}
		else if (strcmp("-f", argv[index]) == 0)
		{
			settings->Fullscreen = TRUE;
		}
		else if (strcmp("-D", argv[index]) == 0)
		{
			settings->Decorations = FALSE;
		}
		else if (strcmp("-T", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing window title\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}

			settings->WindowTitle = _strdup(argv[index]);
		}
		else if (strcmp("-t", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing port number\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->ServerPort = atoi(argv[index]);
		}
		else if (strcmp("-k", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing keyboard layout id\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			sscanf(argv[index], "%X", &(settings->KeyboardLayout));
		}
		else if (strcmp("-K", argv[index]) == 0)
		{
			settings->GrabKeyboard = FALSE;
		}
		else if (strcmp("-n", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing client hostname\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			strncpy(settings->ClientHostname, argv[index], 31);
			settings->ClientHostname[31] = 0;
		}
		else if (strcmp("-o", argv[index]) == 0)
		{
			settings->RemoteConsoleAudio = TRUE;
		}
		else if (strcmp("-0", argv[index]) == 0)
		{
			settings->ConsoleSession = TRUE;
		}
		else if (strcmp("-z", argv[index]) == 0)
		{
			settings->CompressionEnabled = TRUE;
		}
		else if (strcmp("--no-glyph-cache", argv[index]) == 0)
		{
			settings->GlyphSupportLevel = GLYPH_SUPPORT_NONE;
		}
		else if (strcmp("--no-osb", argv[index]) == 0)
		{
			settings->OffscreenSupportLevel = FALSE;
		}
		else if (strcmp("--no-bmp-cache", argv[index]) == 0)
		{
			settings->BitmapCacheEnabled = FALSE;
		}
		else if (strcmp("--no-auth", argv[index]) == 0)
		{
			settings->Authentication = FALSE;
		}
		else if (strcmp("--authonly", argv[index]) == 0)
		{
			settings->AuthenticationOnly = TRUE;
		}
		else if (strcmp("--from-stdin", argv[index]) == 0)
		{
			settings->CredentialsFromStdin = TRUE;
		}
		else if (strcmp("--ignore-certificate", argv[index]) == 0)
		{
			settings->IgnoreCertificate = TRUE;
		}
		else if (strcmp("--certificate-name", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing certificate name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}

			settings->CertificateName = _strdup(argv[index]);
		}
		else if (strcmp("--no-fastpath", argv[index]) == 0)
		{
			settings->FastPathInput = FALSE;
			settings->FastPathOutput = FALSE;
		}
		else if (strcmp("--gdi", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing GDI backend\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (strncmp("sw", argv[index], 1) == 0) /* software */
			{
				settings->SoftwareGdi = TRUE;
			}
			else if (strncmp("hw", argv[index], 1) == 0) /* hardware */
			{
				settings->SoftwareGdi = FALSE;
			}
			else
			{
				printf("unknown GDI backend\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--bcv3", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing codec name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->BitmapCacheV3Enabled = TRUE;
			if (strcmp("rfx", argv[index]) == 0)
			{
				printf("setting rfx\n");
				settings->BitmapCacheV3CodecId = CODEC_ID_REMOTEFX;
				settings->RemoteFxCodec = TRUE;
			}
			else if (strcmp("nsc", argv[index]) == 0)
			{
				printf("setting codec nsc\n");
				settings->BitmapCacheV3CodecId = CODEC_ID_NSCODEC;
				settings->NSCodec = TRUE;
			}
#ifdef WITH_JPEG
			else if (strcmp("jpeg", argv[index]) == 0)
			{
				printf("setting codec jpeg\n");
				settings->BitmapCacheV3CodecId = CODEC_ID_JPEG;
				settings->jpeg_codec = TRUE;
				if (settings->jpeg_quality == 0)
					settings->jpeg_quality = 75;
			}
#endif
			else
			{
				printf("bad codec name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
#ifdef WITH_JPEG
		else if (strcmp("--jpeg", argv[index]) == 0)
		{
			settings->jpeg_codec = TRUE;
			settings->jpeg_quality = 75;
		}
		else if (strcmp("--jpegex", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing codec name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->jpeg_codec = TRUE;
			settings->jpeg_quality = atoi(argv[index]);
		}
#endif
		else if (strcmp("--rfx", argv[index]) == 0)
		{
			settings->RemoteFxCodec = TRUE;
			settings->FastPathOutput = TRUE;
			settings->ColorDepth = 32;
			settings->PerformanceFlags = PERF_FLAG_NONE;
			settings->LargePointerFlag = TRUE;
		}
		else if (strcmp("--rfx-mode", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing RemoteFX mode flag\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (argv[index][0] == 'v') /* video */
			{
				settings->RemoteFxCodecMode = 0x00;
			}
			else if (argv[index][0] == 'i') /* image */
			{
				settings->RemoteFxCodecMode = 0x02;
			}
			else
			{
				printf("unknown RemoteFX mode flag\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--frame-ack", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing frame acknowledge number\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->FrameAcknowledge = atoi(argv[index]);
		}
		else if (strcmp("--nsc", argv[index]) == 0)
		{
			settings->NSCodec = TRUE;
		}
		else if (strcmp("--dump-rfx", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing file name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->DumpRemoteFxFile = _strdup(argv[index]);
			settings->DumpRemoteFx = TRUE;
			settings->RemoteFxOnly = TRUE;
		}
		else if (strcmp("--play-rfx", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing file name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->PlayRemoteFxFile = _strdup(argv[index]);
			settings->PlayRemoteFx = TRUE;
		}
		else if (strcmp("--fonts", argv[index]) == 0)
		{
			settings->AllowFontSmoothing = TRUE;
		}
		else if (strcmp("--disable-wallpaper", argv[index]) == 0)
		{
			settings->DisableWallpaper = TRUE;
		}
		else if (strcmp("--disable-full-window-drag", argv[index]) == 0)
		{
			settings->DisableFullWindowDrag = TRUE;
		}
		else if (strcmp("--disable-menu-animations", argv[index]) == 0)
		{
			settings->DisableMenuAnims = TRUE;
		}
		else if (strcmp("--disable-theming", argv[index]) == 0)
		{
			settings->DisableThemes = TRUE;
		}
		else if (strcmp("--composition", argv[index]) == 0)
		{
			settings->AllowDesktopComposition = TRUE;
		}
		else if (strcmp("--no-motion", argv[index]) == 0)
		{
			settings->MouseMotion = FALSE;
		}
		else if (strcmp("--app", argv[index]) == 0)
		{
			settings->RemoteApplicationMode = TRUE;
			settings->RemoteAppLanguageBarSupported = TRUE;
			settings->Workarea = TRUE;
			settings->PerformanceFlags = PERF_DISABLE_WALLPAPER | PERF_DISABLE_FULLWINDOWDRAG;
		}
		else if (strcmp("-x", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing performance flag\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (argv[index][0] == 'm') /* modem */
			{
				settings->PerformanceFlags = PERF_DISABLE_WALLPAPER |
					PERF_DISABLE_FULLWINDOWDRAG | PERF_DISABLE_MENUANIMATIONS |
					PERF_DISABLE_THEMING;

				settings->ConnectionType = CONNECTION_TYPE_MODEM;
			}
			else if (argv[index][0] == 'b') /* broadband */
			{
				settings->PerformanceFlags = PERF_DISABLE_WALLPAPER;
				settings->ConnectionType = CONNECTION_TYPE_BROADBAND_HIGH;
			}
			else if (argv[index][0] == 'l') /* lan */
			{
				settings->PerformanceFlags = PERF_FLAG_NONE;
				settings->ConnectionType = CONNECTION_TYPE_LAN;
			}
			else
			{
				settings->PerformanceFlags = strtol(argv[index], 0, 16);
			}
		}
		else if (strcmp("-X", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing parent window XID\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}

			settings->ParentWindowId = strtol(argv[index], NULL, 0);

			if (settings->ParentWindowId == 0)
			{
				printf("invalid parent window XID\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--no-rdp", argv[index]) == 0)
		{
			settings->RdpSecurity = FALSE;
		}
		else if (strcmp("--no-tls", argv[index]) == 0)
		{
			settings->TlsSecurity = FALSE;
		}
		else if (strcmp("--no-nla", argv[index]) == 0)
		{
			settings->NlaSecurity = FALSE;
		}
		else if (strcmp("--sec", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing protocol security\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (strncmp("rdp", argv[index], 1) == 0) /* Standard RDP */
			{
				settings->RdpSecurity = TRUE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = FALSE;
				settings->Encryption = TRUE;
				settings->EncryptionMethod = ENCRYPTION_METHOD_40BIT | ENCRYPTION_METHOD_128BIT | ENCRYPTION_METHOD_FIPS;
				settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
			}
			else if (strncmp("tls", argv[index], 1) == 0) /* TLS */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = TRUE;
				settings->NlaSecurity = FALSE;
			}
			else if (strncmp("nla", argv[index], 1) == 0) /* NLA */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = TRUE;
			}
			else
			{
				printf("unknown protocol security\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--no-nego", argv[index]) == 0)
		{
			settings->NegotiateSecurityLayer = FALSE;
		}
		else if (strcmp("--tsg", argv[index]) == 0)
		{
			settings->GatewayUsageMethod = TRUE;
			index++;
			if (index == argc)
			{
				printf("missing TSG username\n");
				return -1;
			}
			settings->GatewayUsername = _strdup(argv[index]);
			index++;
			if (index == argc)
			{
				printf("missing TSG password\n");
				return -1;
			}
			settings->GatewayPassword = _strdup(argv[index]);
			index++;
			if (index == argc)
			{
				printf("missing TSG server\n");
				return -1;
			}
			settings->GatewayHostname = _strdup(argv[index]);
		}
		else if (strcmp("--plugin", argv[index]) == 0)
		{
			index++;
			t = index;
			if (index == argc)
			{
				printf("missing plugin name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			plugin_data = NULL;
			if (strstr(argv[t], "rdpsnd"))
				settings->AudioPlayback = TRUE;
			if (index < argc - 1 && strcmp("--data", argv[index + 1]) == 0)
			{
				index += 2;
				i = 0;
				while (index < argc && strcmp("--", argv[index]) != 0)
				{
					if (plugin_data == NULL)
						plugin_data = (RDP_PLUGIN_DATA*) malloc(sizeof(RDP_PLUGIN_DATA) * (i + 2));
					else
						plugin_data = (RDP_PLUGIN_DATA*) realloc(plugin_data, sizeof(RDP_PLUGIN_DATA) * (i + 2));

					if (strstr(argv[t], "drdynvc") && strstr(argv[index], "audin"))
						settings->AudioCapture = TRUE;

					plugin_data[i].size = sizeof(RDP_PLUGIN_DATA);
					plugin_data[i].data[0] = NULL;
					plugin_data[i].data[1] = NULL;
					plugin_data[i].data[2] = NULL;
					plugin_data[i].data[3] = NULL;
					plugin_data[i + 1].size = 0;

					for (j = 0, p = argv[index]; j < 4 && p != NULL; j++)
					{
						if (*p == '\'')
						{
							plugin_data[i].data[j] = p + 1;
							p = strchr(p + 1, '\'');
							if (p)
								*p++ = 0;
						}
						else
							plugin_data[i].data[j] = p;

						p = strchr(p, ':');
						if (p != NULL)
							*p++ = 0;
					}
					index++;
					i++;
				}
			}

			if (plugin_callback != NULL)
			{
				if (!plugin_callback(settings, argv[t], plugin_data, plugin_user_data))
					return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--ext", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing extension name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (num_extensions >= ARRAY_SIZE(settings->extensions))
			{
				printf("maximum extensions reached\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			snprintf(settings->extensions[num_extensions].name,
				sizeof(settings->extensions[num_extensions].name),
				"%s", argv[index]);
			settings->extensions[num_extensions].data = NULL;
			if (index < argc - 1 && strcmp("--data", argv[index + 1]) == 0)
			{
				index += 2;
				settings->extensions[num_extensions].data = argv[index];
				i = 0;
				while (index < argc && strcmp("--", argv[index]) != 0)
				{
					index++;
					i++;
				}
			}
			num_extensions++;
		}
		else if (strcmp("--no-salted-checksum", argv[index]) == 0)
		{
			settings->SaltedChecksum = FALSE;
		}
		else if (strcmp("--pcid", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing preconnection id value\n");
				return -1;
			}
			settings->SendPreconnectionPdu = TRUE;
			settings->PreconnectionId = atoi(argv[index]);
		}
		else if (strcmp("--pcb", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing preconnection blob value\n");
				return -1;
			}
			settings->SendPreconnectionPdu = TRUE;
			settings->PreconnectionBlob = _strdup(argv[index]);
		}
		else if (strcmp("--version", argv[index]) == 0)
		{
			printf("This is FreeRDP version %s (git %s)\n", FREERDP_VERSION_FULL, GIT_REVISION);
			return FREERDP_ARGS_PARSE_VERSION;
		}
		else if (argv[index][0] != '-')
		{
			freerdp_parse_hostname(settings, argv[index]);

			/* server is the last argument for the current session. arguments
			   followed will be parsed for the next session. */
			index++;

			if (settings->AllowFontSmoothing)
				settings->PerformanceFlags |= PERF_ENABLE_FONT_SMOOTHING;

			if (settings->AllowDesktopComposition)
				settings->PerformanceFlags |= PERF_ENABLE_DESKTOP_COMPOSITION;

			if (settings->DisableWallpaper)
				settings->PerformanceFlags |= PERF_DISABLE_WALLPAPER;

			if (settings->DisableFullWindowDrag)
				settings->PerformanceFlags |= PERF_DISABLE_FULLWINDOWDRAG;

			if (settings->DisableMenuAnims)
				settings->PerformanceFlags |= PERF_DISABLE_MENUANIMATIONS;

			if (settings->DisableThemes)
				settings->PerformanceFlags |= PERF_DISABLE_THEMING;

			break; /* post process missing arguments */

		}
		else
		{
			if (ui_callback != NULL)
			{
				t = ui_callback(settings, argv[index], (index + 1 < argc && argv[index + 1][0] != '-' ?
					argv[index + 1] : NULL), ui_user_data);
				if (t == 0)
				{
					printf("invalid option: %s\n", argv[index]);
					return FREERDP_ARGS_PARSE_FAILURE;
				}
				index += t - 1;
			}
		}
		index++;
	}


	/* --from-stdin will prompt for missing arguments only.
		 You can prompt for username, password, domain and hostname to avoid disclosing
		 these settings to ps. */

	if (settings->CredentialsFromStdin)
	{
		/* username */
		if (NULL == settings->Username)
		{
			char input[512];
			input[0] = '\0';
			printf("username: ");
			if (scanf("%511s%*c", input) > 0)
			{
				settings->Username = _strdup(input);
			}
		}
		/* password */
		if (NULL == settings->Password)
		{
			settings->Password = malloc(512 * sizeof(char));
			if (isatty(STDIN_FILENO))
			{
				freerdp_passphrase_read("password: ", settings->Password, 512, settings->CredentialsFromStdin);
			}
			else
			{
				printf("password: ");
				if (scanf("%511s%*c", settings->Password) <= 0)
				{
					free(settings->Password);
					settings->Password = NULL;
				}
			}
		}
		/* domain */
		if (NULL == settings->Domain)
		{
			char input[512];
			input[0] = '\0';
			printf("domain (control-D to skip): ");
			if (scanf("%511s%*c", input) > 0)
			{
				/* Try to catch the cases where the string is NULL-ish right
				   at the get go */
				if (input[0] != '\0' && !(input[0] == '.' && input[1] == '\0'))
				{
					settings->Domain = _strdup(input);
				}
			}
			if (feof(stdin))
			{
				printf("\n");
				clearerr(stdin);
			}
		}
		/* hostname */
		if (settings->ServerHostname == NULL)
		{
			char input[512];
			input[0] = '\0';
			printf("hostname: ");
			if (scanf("%511s%*c", input) > 0)
			{
				freerdp_parse_hostname(settings, input);
			}
		}
	}

	/* Must have a hostname. Do you? */
	if ((settings->ServerHostname == NULL) && (settings->ConnectionFile == NULL))
	{
		printf("missing server name\n");
		return FREERDP_ARGS_PARSE_FAILURE;
	}
	else
	{
		return index;
	}
}

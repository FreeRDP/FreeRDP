
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>

#include <freerdp/client/channels.h>

int TestClientChannels(int argc, char* argv[])
{
	int index;
	void* entry;
	DWORD dwFlags;
	FREERDP_ADDIN* pAddin;
	FREERDP_ADDIN** ppAddins;

	dwFlags = FREERDP_ADDIN_DYNAMIC;

	printf("Enumerate all\n");
	ppAddins = freerdp_channels_list_client_addins(NULL, NULL, NULL, dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	printf("Enumerate rdpsnd\n");
	ppAddins = freerdp_channels_list_client_addins("rdpsnd", NULL, NULL, dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	printf("Enumerate tsmf video\n");
	ppAddins = freerdp_channels_list_client_addins("tsmf", NULL, "video", dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	ppAddins = freerdp_channels_list_client_addins("unknown", NULL, NULL, dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	printf("Enumerate static addins\n");

	dwFlags = FREERDP_ADDIN_STATIC;
	ppAddins = freerdp_channels_list_client_addins(NULL, NULL, NULL, dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	dwFlags = FREERDP_ADDIN_DYNAMIC;

	entry = freerdp_channels_load_addin_entry("drive", NULL, "DeviceServiceEntry", dwFlags);

	printf("drive entry: %s\n", entry ? "non-null" : "null");

	entry = freerdp_channels_load_addin_entry("rdpsnd", "alsa", NULL, dwFlags);

	printf("rdpsnd-alsa entry: %s\n", entry ? "non-null" : "null");

	return 0;
}

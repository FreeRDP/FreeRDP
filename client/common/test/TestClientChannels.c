
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>

#include <freerdp/client/channels.h>

int TestClientChannels(int argc, char* argv[])
{
	int index;
	DWORD dwFlags;
	FREERDP_ADDIN* pAddin;
	FREERDP_ADDIN** ppAddins;

	dwFlags = FREERDP_ADDIN_DYNAMIC;

	printf("Enumerate all\n");
	ppAddins = freerdp_channels_list_addins(NULL, NULL, NULL, dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	printf("Enumerate rdpsnd\n");
	ppAddins = freerdp_channels_list_addins("rdpsnd", NULL, NULL, dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	printf("Enumerate tsmf video\n");
	ppAddins = freerdp_channels_list_addins("tsmf", NULL, "video", dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	ppAddins = freerdp_channels_list_addins("unknown", NULL, NULL, dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	printf("Enumerate static addins\n");

	dwFlags = FREERDP_ADDIN_STATIC;
	ppAddins = freerdp_channels_list_addins(NULL, NULL, NULL, dwFlags);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	return 0;
}

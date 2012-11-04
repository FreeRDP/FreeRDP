
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>

#include <freerdp/client/channels.h>

int TestClientChannels(int argc, char* argv[])
{
	int index;
	FREERDP_ADDIN* pAddin;
	FREERDP_ADDIN** ppAddins;

	printf("Enumerate all\n");
	ppAddins = freerdp_channels_list_client_addins(NULL, NULL, NULL, 0);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	printf("Enumerate rdpsnd\n");
	ppAddins = freerdp_channels_list_client_addins("rdpsnd", NULL, NULL, 0);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	printf("Enumerate tsmf video\n");
	ppAddins = freerdp_channels_list_client_addins("tsmf", NULL, "video", 0);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	printf("Enumerate unknown\n");
	ppAddins = freerdp_channels_list_client_addins("unknown", NULL, NULL, 0);

	for (index = 0; ppAddins[index] != NULL; index++)
	{
		pAddin = ppAddins[index];

		printf("Addin: Name: %s Subsystem: %s Type: %s\n",
				pAddin->cName, pAddin->cSubsystem, pAddin->cType);
	}

	freerdp_channels_addin_list_free(ppAddins);

	return 0;
}

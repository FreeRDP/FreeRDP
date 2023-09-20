//
//  main.m
//  MacClient2
//
//  Created by Benoît et Kathy on 2013-05-08.
//
//

#import <Cocoa/Cocoa.h>
#include <freerdp/client/cmdline.h>

int main(int argc, char *argv[])
{
	freerdp_client_warn_deprecated(argc, argv);
	for (int i = 0; i < argc; i++)
	{
		char *ctemp = argv[i];
		if (memcmp(ctemp, "/p:", 3) == 0 || memcmp(ctemp, "-p:", 3) == 0)
		{
			memset(ctemp + 3, '*', strlen(ctemp) - 3);
		}
	}

	const char **cargv = (const char **)argv;
	return NSApplicationMain(argc, cargv);
}

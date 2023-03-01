//
//  main.m
//  MacClient2
//
//  Created by Benoît et Kathy on 2013-05-08.
//
//

#import <Cocoa/Cocoa.h>
#include <freerdp/client/cmdline.h>

int main(int argc, const char *argv[])
{
	const char *ctemp;

	freerdp_client_warn_deprecated(argc, argv);
	for (int i = 0; i < argc; i++)
	{
		ctemp = argv[i];
		if (memcmp(ctemp, "/p:", 3) == 0 || memcmp(ctemp, "-p:", 3) == 0)
		{
			memset((char *)ctemp + 3, '*', strlen(ctemp) - 3);
		}
	}

	return NSApplicationMain(argc, argv);
}

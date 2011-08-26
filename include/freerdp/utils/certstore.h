#ifndef __CERTSTORE_UTILS_H
#define __CERTSTORE_UTILS_H

typedef struct rdp_certstore rdpCertstore;
typedef struct rdp_certdata rdpCertdata;

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/utils/memory.h>
struct rdp_certdata
{
    char* thumbprint;
    char* hostname;
}
struct rdp_certstore
{
	FILE* fp;
	char* path;
	char* file;
	char* home;
	boolean available;
	struct rdp_certdata* certdata;
};

#ifndef __CERTSTORE_UTILS_H
#define __CERTSTORE_UTILS_H

typedef struct rdp_certstore rdpCertstore;
typedef struct rdp_certdata rdpCertdata;

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <freerdp/api.h>
#include <freerdp/freerdp.h>
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
	struct rdp_certdata* certdata;
};
void certstore_create(rdpCertstore* certstore);
void certstore_open(rdpCertstore* certstore);
void certstore_load(rdpCertstore* certstore);
void certstore_close(rdpcertstore* certstore);
char* get_local_certloc();
void certstore_init(rdpCertstore* certstore);
rdpCertstore* certstore_new(rdpCertdata* certdata);
void cerstore_free(rdpCertsore* certstore);

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
};

struct rdp_certstore
{
	FILE* fp;
	char* path;
	char* file;
	char* home;
	int match;
	struct rdp_certdata* certdata;
};

FREERDP_API void certstore_create(rdpCertstore* certstore);
FREERDP_API void certstore_open(rdpCertstore* certstore);
FREERDP_API void certstore_load(rdpCertstore* certstore);
FREERDP_API void certstore_close(rdpCertstore* certstore);
FREERDP_API char* get_local_certloc();
FREERDP_API rdpCertdata* certdata_new(char* host_name, char* fingerprint);
FREERDP_API void certdata_free(rdpCertdata* certdata);
FREERDP_API void certstore_init(rdpCertstore* certstore);
FREERDP_API rdpCertstore* certstore_new(rdpCertdata* certdata);
FREERDP_API void certstore_free(rdpCertstore* certstore);
FREERDP_API int match_certdata(rdpCertstore* certstore);
FREERDP_API void print_certdata(rdpCertstore* certstore);

#endif /* __CERTSTORE_UTILS_H */

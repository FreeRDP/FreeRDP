#ifndef __CERTSTORE_UTILS_H
#define __CERTSTORE_UTILS_H

typedef struct rdp_cert_store rdpCertStore;
typedef struct rdp_cert_data rdpCertData;

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/memory.h>

struct rdp_cert_data
{
	char* hostname;
	char* fingerprint;
};

struct rdp_cert_store
{
	FILE* fp;
	int match;
	char* path;
	char* file;
	char* home_path;
	rdpCertData* certdata;
};

FREERDP_API void certstore_create(rdpCertStore* certstore);
FREERDP_API void certstore_open(rdpCertStore* certstore);
FREERDP_API void certstore_load(rdpCertStore* certstore);
FREERDP_API void certstore_close(rdpCertStore* certstore);
FREERDP_API char* get_local_certloc();
FREERDP_API rdpCertData* certdata_new(char* hostname, char* fingerprint);
FREERDP_API void certdata_free(rdpCertData* certdata);
FREERDP_API void certstore_init(rdpCertStore* certstore);
FREERDP_API rdpCertStore* certstore_new(rdpCertData* certdata, char* home_path);
FREERDP_API void certstore_free(rdpCertStore* certstore);
FREERDP_API int cert_data_match(rdpCertStore* certstore);
FREERDP_API void cert_data_print(rdpCertStore* certstore);

#endif /* __CERTSTORE_UTILS_H */

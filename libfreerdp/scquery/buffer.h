#ifndef LIBFREERDP_SCQUERY_BUFFER_H
#define LIBFREERDP_SCQUERY_BUFFER_H
#include <pkcs11-helper-1.0/pkcs11.h>

typedef struct
{
	CK_ULONG flags;
	CK_ULONG size;
	CK_BYTE* data;
} buffer_t, *buffer;

buffer buffer_new_copy(CK_ULONG size, CK_BYTE* data);
buffer buffer_new(CK_ULONG size, CK_BYTE* data);
void buffer_free(buffer buffer);

#endif

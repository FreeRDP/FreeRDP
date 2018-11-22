#ifndef LIBFREERDP_SCQUERY_CERTIFICATE_LIST_H
#define LIBFREERDP_SCQUERY_CERTIFICATE_LIST_H
#include "certificate.h"

typedef struct certificate_list
{
	smartcard_certificate certificate;
	struct certificate_list* rest;
} certificate_list_t, *certificate_list;

smartcard_certificate certificate_list_first(certificate_list list);
certificate_list      certificate_list_rest(certificate_list list);

#define DO_CERTIFICATE_LIST(certificate,current,list)                                       \
	for((current=list,                                                                      \
	     certificate=((current!=NULL)?certificate_list_first(current):CK_INVALID_HANDLE));  \
	    (current!=NULL);                                                                    \
	    (current=certificate_list_rest(current),                                            \
	     certificate=((current!=NULL)?certificate_list_first(current):CK_INVALID_HANDLE)))

/* certificate_list_cons
allocates a new list node containing the certificate and the next list. */
certificate_list certificate_list_cons(smartcard_certificate certificate, certificate_list rest);

/* certificate_list_deepfree
deepfrees the certificates and the list nodes */
void certificate_list_deepfree(certificate_list list);

/* certificate_list_free
frees only the current list nodes (not the next ones). */
void certificate_list_free(certificate_list list);

#endif

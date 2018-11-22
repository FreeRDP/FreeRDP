#include <stdlib.h>
#include "certificate-list.h"
#include "error.h"

certificate_list certificate_list_cons(smartcard_certificate certificate, certificate_list rest)
{
	certificate_list list = checked_malloc(sizeof(*list));

	if (list)
	{
		list->certificate = certificate;
		list->rest = rest;
	}

	return list;
}

void certificate_list_deepfree(certificate_list list)
{
	if (list)
	{
		scquery_certificate_deepfree(list->certificate);
		certificate_list_deepfree(list->rest);
		certificate_list_free(list);
	}
}

smartcard_certificate certificate_list_first(certificate_list list)
{
	return ((list == NULL) ? NULL : list->certificate);
}
certificate_list      certificate_list_rest(certificate_list list)
{
	return ((list == NULL) ? NULL : list->rest);
}
void                  certificate_list_free(certificate_list list)
{
	free(list);
}


/**** THE END ****/

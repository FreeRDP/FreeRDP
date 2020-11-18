#include <stdlib.h>
#include "certificate_list.h"
#include "scquery_error.h"

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
certificate_list certificate_list_rest(certificate_list list)
{
	return ((list == NULL) ? NULL : list->rest);
}
void certificate_list_free(certificate_list list)
{
	free(list);
}

certificate_list certificate_list_delete(smartcard_certificate certificate, certificate_list list)
{
	if (certificate_list_first(list) == certificate)
	{
		certificate_list result = certificate_list_rest(list);
		certificate_list_free(list);
		return result;
	}
	else
	{
		certificate_list previous = list;

		while ((certificate_list_rest(previous) != NULL) &&
		       (certificate_list_first(certificate_list_rest(previous)) != certificate))
		{
			previous = certificate_list_rest(previous);
		}

		if (certificate_list_rest(previous) != NULL)
		{
			certificate_list old = certificate_list_rest(previous);
			previous->rest = certificate_list_rest(old);
			certificate_list_free(old);
		}

		return list;
	}
}

/**** THE END ****/

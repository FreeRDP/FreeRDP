#ifndef LIBFREERDP_SCQUERY_X509_ALT_NAMES_H
#define LIBFREERDP_SCQUERY_X509_ALT_NAMES_H
#include "buffer.h"

typedef struct
{
	char*         type;
	unsigned      count;
	unsigned      allocated;
	char**        components;
} alt_name_t, *alt_name;

alt_name alt_name_new_with_components(char* type, unsigned count, char** components);
alt_name alt_name_new(char* type, unsigned allocated);
void alt_name_add_component(alt_name name, char* component);
void alt_name_free(alt_name name);

typedef struct alt_name_node
{
	alt_name name;
	struct alt_name_node*   rest;
} alt_name_list_t, * alt_name_list;


alt_name_list alt_name_list_cons(alt_name name, alt_name_list rest);
alt_name      alt_name_list_first(alt_name_list list);
alt_name_list alt_name_list_rest(alt_name_list list);
void          alt_name_list_free(alt_name_list list);
void          alt_name_list_deepfree(alt_name_list list);

#define DO_ALT_NAME_LIST(alt_name,current,list)      \
	for((current=list,			     \
	     alt_name=alt_name_list_first(current)); \
	    (current!=NULL);			     \
	    (current=alt_name_list_rest(current),    \
	     alt_name=alt_name_list_first(current)))

alt_name_list certificate_extract_subject_alt_names(buffer certificate_data);

#endif

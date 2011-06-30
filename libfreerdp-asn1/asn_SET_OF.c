/*-
 * Copyright (c) 2003, 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include "asn_internal.h"
#include "asn_SET_OF.h"
#include <errno.h>

/*
 * Add another element into the set.
 */
int
asn_set_add(void *asn_set_of_x, void *ptr) {
	asn_anonymous_set_ *as = _A_SET_FROM_VOID(asn_set_of_x);

	if(as == 0 || ptr == 0) {
		errno = EINVAL;		/* Invalid arguments */
		return -1;
	}

	/*
	 * Make sure there's enough space to insert an element.
	 */
	if(as->count == as->size) {
		int _newsize = as->size ? (as->size << 1) : 4;
		void *_new_arr;
		_new_arr = REALLOC(as->array, _newsize * sizeof(as->array[0]));
		if(_new_arr) {
			as->array = (void **)_new_arr;
			as->size = _newsize;
		} else {
			/* ENOMEM */
			return -1;
		}
	}

	as->array[as->count++] = ptr;

	return 0;
}

void
asn_set_del(void *asn_set_of_x, int number, int _do_free) {
	asn_anonymous_set_ *as = _A_SET_FROM_VOID(asn_set_of_x);

	if(as) {
		void *ptr;
		if(number < 0 || number >= as->count)
			return;

		if(_do_free && as->free) {
			ptr = as->array[number];
		} else {
			ptr = 0;
		}

		as->array[number] = as->array[--as->count];

		/*
		 * Invoke the third-party function only when the state
		 * of the parent structure is consistent.
		 */
		if(ptr) as->free(ptr);
	}
}

/*
 * Free the contents of the set, do not free the set itself.
 */
void
asn_set_empty(void *asn_set_of_x) {
	asn_anonymous_set_ *as = _A_SET_FROM_VOID(asn_set_of_x);

	if(as) {
		if(as->array) {
			if(as->free) {
				while(as->count--)
					as->free(as->array[as->count]);
			}
			FREEMEM(as->array);
			as->array = 0;
		}
		as->count = 0;
		as->size = 0;
	}

}


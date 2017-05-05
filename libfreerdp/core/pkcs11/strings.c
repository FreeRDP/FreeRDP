/*
 * PAM-PKCS11 strings tools
 * Copyright (C) 2005 Juan Antonio Martinez <jonsito@teleline.es>
 * pam-pkcs11 is copyright (C) 2003-2004 of Mario Strasser <mast@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * $Id$
 */

#ifndef __STRINGS_C_
#define __STRINGS_C_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "strings.h"

/*
check for null or blank string
*/
int is_empty_str(const char *str) {
	const char *pt;
	if (!str) return 1;
	for (pt=str; *pt;pt++) if (!isspace(*pt)) return 0;
	/* arriving here means no non-blank char found */
	return 1;
}

/* returns a clone of provided string */
char *clone_str(const char *str) {
	size_t len= strlen(str);
	char *dst= malloc(1+len);
	if (!dst) return NULL;
	strncpy(dst,str,len);
	*(dst+len)='\0';
	return dst;
}

/* returns a uppercased clone of provided string */
char *toupper_str(const char *str) {
	const char *from;
	char *to;
	char *dst= malloc(1+strlen(str));
	if(!dst) return (char *) str; /* should I advise?? */
	for (from=str,to=dst;*from; from++,to++) *to=toupper(*from);
	*to='\0';
	return dst;
}

/* returns a lowercased clone of provided string */
char *tolower_str(const char *str) {
	const char *from;
	char *to;
	char *dst= malloc(1+strlen(str));
	if(!dst) return (char *)str /* should I advise?? */;
	for (from=str,to=dst;*from; from++,to++) *to=tolower(*from);
	*to='\0';
	return dst;
}

/* print a binary array in xx:xx:.... format */
char *bin2hex(const unsigned char *binstr,const int len) {
	int i;
	char *pt;
	char *res= malloc(1+3*len);
	if (!res) return NULL;
	if (len == 0) {
	    *res = 0;
	    return res;
	}
	for(i=0,pt=res;i<len;i++,pt+=3){
	    sprintf(pt,"%02X:",binstr[i]);
	}
	*(--pt)='\0'; /* replace last ':' with '\0' */
	return res;
}

/* convert xx:xx:xx to binary array */
unsigned char *hex2bin(const char *hexstr) {
        char *to;
        char *from      =    (char* )hexstr;
        int nelems      =    (1+strlen(hexstr))/3;
        unsigned char *res = calloc(nelems,sizeof(unsigned char));
        if (!res) return NULL;
        if (*from==':') from++; /* strip first char if equals to ':' */
        for (to=(char *)res; *from;from+=3,to++) {
                unsigned int c;
                if ( sscanf(from,"%02x",&c) == 1)
                        *to=(unsigned char)c;
        }
        return res;
}

/* same as above, but no malloc needed if res is not null */
unsigned char *hex2bin_static(const char *hexstr,unsigned char **res,int *size) {
        char *to;
        char *from      =    (char* )hexstr;
        *size   =    (1+strlen(hexstr))/3;
        if(!*res) *res = calloc(*size,sizeof(unsigned char));
        if (!*res) return NULL;
        if (*from==':') from++; /* strip first char if equals to ':' */
        for (to=(char *)*res; *from;from+=3,to++) {
                unsigned int c;
                if ( sscanf(from,"%02x",&c) == 1)
                        *to=(unsigned char)c;
        }
        return *res;
}

/*
* splits a string into a nelems string array by using sep as char separator
*/
char **split(const char *str,char sep, int nelems){
        int n;
        char *pt;
        char *copy= clone_str(str);
        char **res= calloc(nelems,sizeof(char*));
        if ( (!res) || (!copy) ) {
        	if(copy) free(copy);
        	return NULL;
        }
        for (pt=copy,n=0;n<nelems-1;n++) {
                res[n]=pt;
                pt=strchr(pt,sep);
                if(!pt) {
                	free(copy);
                	return res;
                }
                *pt++='\0';
        }
        res[n]=pt; /* last item stores rest of string */

        free(copy);

        return res;
}

/*
* splits a string into a nelems string array by using sep as char separator,
* but using static structures
* Note that result must be still free()'d
*/
char **split_static(const char *str,char sep, int nelems,char *dst){
        int n;
        char *pt;
        char **res= calloc(nelems,sizeof(char*));
        if ( (!res) || (!dst) ) return NULL;
        strncpy(dst,str,1+strlen(str));
        for (pt=dst,n=0;n<nelems-1;n++) {
                res[n]=pt;
                pt=strchr(pt,sep);
                if(!pt) return res;
                *pt++='\0';
        }
        res[n]=pt; /* last item stores rest of string */
        return res;
}

/* remove redundant spaces from string */
char *trim(const char *str){
        char *from,*to;
        int space=1;
        char *res=malloc(strlen(str));
        if (!res) return NULL;
        for(from=(char *)str,to=res;*from;from++) {
                if (!isspace(*from)) { space=0;*to++=*from; }
                else if(!space) { space=1; *to++=' '; }
        }
        /* remove (if any) spaces at end of string*/
        if(space) *--to='\0';
        else        *to='\0';
        return res;
}

#endif /* __STRINGS_C_ */

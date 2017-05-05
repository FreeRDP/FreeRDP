/*
 * PKCS #11 PAM Login Module
 * Copyright (C) 2003 Mario Strasser <mast@gmx.net>,
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
 * $Id$
 */

/** \file
 This module contains several functions to retrieve data from an URL

 Some examples of valid URL's:
 <ul>
 <li>file:///home/mario/projects/pkcs11_login/tests/ca_crl_0.pem</li>
 <li>ftp://ftp.rediris.es/certs/rediris_cacert.pem</li>
 <li>http://www-t.zhwin.ch/ca/root_ca.crl</li>
 <li>ldap://directory.verisign.com:389/CN=VeriSign IECA, OU=IECA-3, OU=Contractor, OU=PKI, OU=DOD, O=U.S. Government, C=US?certificateRevocationList;binary</li>
 </ul>
*/

#ifndef __URI_H_
#define __URI_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>

#ifndef __URI_C_
#define URI_EXTERN extern
#else
#define URI_EXTERN
#endif

URI_EXTERN int is_uri(const char *path);
URI_EXTERN int is_file(const char *path);
URI_EXTERN int is_dir(const char *path);
URI_EXTERN int is_symlink(const char *path);

/**
*  Downloads data from a given URI
*@param uri_str URL string where to retrieve data
*@param data Pointer to a String buffer where data is retrieved
*@param length Length of retrieved data
*@return -1 on error, 0 on sucess
*/
URI_EXTERN int get_from_uri(const char *uri_str, unsigned char **data, size_t *length);

#undef URI_EXTERN

#endif /* __URI_H_ */

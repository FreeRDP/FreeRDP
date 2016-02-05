/*
   A RDP connection reference. References can use bookmark ids or hostnames to connect to a RDP server.

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.domain;

public class ConnectionReference
{
	public static final String PATH_MANUAL_BOOKMARK_ID = "MBMID/"; 
	public static final String PATH_HOSTNAME = "HOST/";
	public static final String PATH_PLACEHOLDER = "PLCHLD/";
	public static final String PATH_FILE = "FILE/";

	public static String getManualBookmarkReference(long bookmarkId) {
		return (PATH_MANUAL_BOOKMARK_ID + bookmarkId);
	}

	public static String getHostnameReference(String hostname) {
		return (PATH_HOSTNAME + hostname);
	}
	
	public static String getPlaceholderReference(String name) {
		return (PATH_PLACEHOLDER + name);
	}
	
	public static String getFileReference(String uri) {
		return (PATH_FILE + uri);
	}
	
	public static boolean isBookmarkReference(String refStr) {
		return refStr.startsWith(PATH_MANUAL_BOOKMARK_ID);
	}

	public static boolean isManualBookmarkReference(String refStr) {
		return refStr.startsWith(PATH_MANUAL_BOOKMARK_ID);
	}

	public static boolean isHostnameReference(String refStr) {
		return refStr.startsWith(PATH_HOSTNAME);
	}
	
	public static boolean isPlaceholderReference(String refStr) {
		return refStr.startsWith(PATH_PLACEHOLDER);
	}

	public static boolean isFileReference(String refStr) {
		return refStr.startsWith(PATH_FILE);
	}

	public static long getManualBookmarkId(String refStr) {
		return Integer.parseInt(refStr.substring(PATH_MANUAL_BOOKMARK_ID.length()));
	}

	public static String getHostname(String refStr) {
		return refStr.substring(PATH_HOSTNAME.length());
	}

	public static String getPlaceholder(String refStr) {
		return refStr.substring(PATH_PLACEHOLDER.length());
	}

	public static String getFile(String refStr) {
		return refStr.substring(PATH_FILE.length());
	}
}

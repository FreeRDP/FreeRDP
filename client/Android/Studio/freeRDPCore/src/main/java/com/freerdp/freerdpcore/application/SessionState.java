/*
   Session State class

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.application;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.Parcel;
import android.os.Parcelable;

import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.services.LibFreeRDP;

public class SessionState implements Parcelable
{
	private long instance;
	private BookmarkBase bookmark;
	private Uri openUri;
	private BitmapDrawable surface;
	private LibFreeRDP.UIEventListener uiEventListener;
	
	public SessionState(Parcel parcel)
	{
		instance = parcel.readLong();
		bookmark = parcel.readParcelable(null);
		openUri = parcel.readParcelable(null);

		Bitmap bitmap = parcel.readParcelable(null);
		surface = new BitmapDrawable(bitmap);
	}
	
	public SessionState(long instance, BookmarkBase bookmark)
	{
		this.instance = instance;
		this.bookmark = bookmark;
		this.openUri = null;
		this.uiEventListener = null;
	}
	
	public SessionState(long instance, Uri openUri)
	{
		this.instance = instance;
		this.bookmark = null;
		this.openUri = openUri;
		this.uiEventListener = null;
	}
	
	public void connect() {
		if (bookmark != null) {
			LibFreeRDP.setConnectionInfo(instance, bookmark);
		} else {
			LibFreeRDP.setConnectionInfo(instance, openUri);
		}
		LibFreeRDP.connect(instance);
	}
	
	public long getInstance() {
		return instance;
	}
	
	public BookmarkBase getBookmark() {
		return bookmark;
	}
	
	public Uri getOpenUri() {
		return openUri;
	}
	
	public LibFreeRDP.UIEventListener getUIEventListener() {
		return uiEventListener;
	}

	public void setUIEventListener(LibFreeRDP.UIEventListener uiEventListener) {
		this.uiEventListener = uiEventListener;
	}

	public void setSurface(BitmapDrawable surface) {
		this.surface = surface;
	}
	
	public BitmapDrawable getSurface() {
		return surface;
	}

	public static final Parcelable.Creator<SessionState> CREATOR = new Parcelable.Creator<SessionState>()
	{
		public SessionState createFromParcel(Parcel in) {
			return new SessionState(in);
		}

		@Override
		public SessionState[] newArray(int size) {
			return new SessionState[size];
		}
	};
	
	@Override
	public int describeContents() {
		return 0;
	}

	@Override
	public void writeToParcel(Parcel out, int flags) {		
		out.writeLong(instance);
		out.writeParcelable(bookmark, flags);
		out.writeParcelable(openUri, flags);
		out.writeParcelable(surface.getBitmap(), flags);
	}
}

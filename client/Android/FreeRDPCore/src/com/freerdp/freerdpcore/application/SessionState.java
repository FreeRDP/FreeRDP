/*
   Session State class

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.application;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Parcel;
import android.os.Parcelable;

import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.services.LibFreeRDP;

public class SessionState implements Parcelable
{
	private int instance;
	private BookmarkBase bookmark;
	private BitmapDrawable surface;
	private LibFreeRDP.UIEventListener uiEventListener;
	
	public SessionState(Parcel parcel)
	{
		instance = parcel.readInt();
		bookmark = parcel.readParcelable(null);

		Bitmap bitmap = parcel.readParcelable(null);
		surface = new BitmapDrawable(bitmap);
	}
	
	public SessionState(int instance, BookmarkBase bookmark)
	{
		this.instance = instance;
		this.bookmark = bookmark;
		this.uiEventListener = null;
	}
	
	public void connect() {
		LibFreeRDP.setConnectionInfo(instance, bookmark);
		LibFreeRDP.connect(instance);
	}
	
	public int getInstance() {
		return instance;
	}
	
	public BookmarkBase getBookmark() {
		return bookmark;
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
		out.writeInt(instance);
		out.writeParcelable(bookmark, flags);
		out.writeParcelable(surface.getBitmap(), flags);
	}
}

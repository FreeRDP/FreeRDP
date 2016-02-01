/*
   Quick Connect bookmark (used for quick connects using just a hostname)

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.domain;

import android.content.SharedPreferences;
import android.os.Parcel;
import android.os.Parcelable;

public class QuickConnectBookmark extends ManualBookmark {

	public QuickConnectBookmark(Parcel parcel)
	{
		super(parcel);
		type = TYPE_QUICKCONNECT;
	}

	public QuickConnectBookmark() {
		super();
		type = TYPE_QUICKCONNECT;
	}

	public static final Parcelable.Creator<QuickConnectBookmark> CREATOR = new Parcelable.Creator<QuickConnectBookmark>()
	{
		public QuickConnectBookmark createFromParcel(Parcel in) {
			return new QuickConnectBookmark(in);
		}

		@Override
		public QuickConnectBookmark[] newArray(int size) {
			return new QuickConnectBookmark[size];
		}
	};
	
	@Override
	public int describeContents() {
		return 0;
	}

	@Override
	public void writeToParcel(Parcel out, int flags)
	{
		super.writeToParcel(out, flags);
	}

	@Override
	public void writeToSharedPreferences(SharedPreferences sharedPrefs)
	{
		super.writeToSharedPreferences(sharedPrefs);
	}

	@Override
	public void readFromSharedPreferences(SharedPreferences sharedPrefs)
	{
		super.readFromSharedPreferences(sharedPrefs);
	}
	
	// Cloneable
	public Object clone()
	{
		return super.clone();					
	}
	
}

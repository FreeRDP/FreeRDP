/*
   Placeholder for bookmark items with a special purpose (i.e. just displaying some text)

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.domain;

import android.content.SharedPreferences;
import android.os.Parcel;
import android.os.Parcelable;

public class PlaceholderBookmark extends BookmarkBase {

	private String name;
	
	public PlaceholderBookmark(Parcel parcel)
	{
		super(parcel);
		type = TYPE_PLACEHOLDER;
		name = parcel.readString();
	}

	public PlaceholderBookmark() {
		super();
		type = TYPE_PLACEHOLDER;
		name = "";
	}

	public void setName(String name) {
		this.name = name;
	}
	
	public String getName() {
		return name;
	}
	
	public static final Parcelable.Creator<PlaceholderBookmark> CREATOR = new Parcelable.Creator<PlaceholderBookmark>()
	{
		public PlaceholderBookmark createFromParcel(Parcel in) {
			return new PlaceholderBookmark(in);
		}

		@Override
		public PlaceholderBookmark[] newArray(int size) {
			return new PlaceholderBookmark[size];
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
		out.writeString(name);
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
